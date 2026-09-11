/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 *
 * Puerto nativo en C++ de `mesh.faces_mirror_uv`, que vivia en
 * `scripts/startup/bl_operators/mesh.py` (clase `MeshMirrorUV`, Flipendo carril C).
 *
 * Copia las coordenadas UV de un lado de la malla al otro, emparejando vertices por su
 * posicion espejada en X y caras por el conjunto de vertices emparejados.
 */

#include <algorithm>
#include <map>
#include <tuple>
#include <vector>

#include "BLI_math_base.h"
#include "BLI_math_vector.h"
#include "BLI_vector.hh"

#include "DNA_ID.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLT_translation.hh"

#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_mesh.hh"
#include "BKE_report.hh"

#include "DEG_depsgraph.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_mesh.hh"
#include "ED_object.hh"

#include "mesh_intern.hh" /* own include */

using namespace blender;

/* La clave del emparejamiento: las tres coordenadas redondeadas a `precision` decimales,
 * que es lo que hace `v.co.to_tuple(precision)` de mathutils (`double_round()` sobre el
 * float promovido a double).
 *
 * TRAMPA: en Python `(-0.0,) == (0.0,)` y ademas tienen el mismo hash, asi que el
 * diccionario los trata como la MISMA clave. Un `std::map<double,...>` tambien los ve
 * iguales al comparar con `<`, pero el `-co[0]` de la busqueda espejada puede generar un
 * cero negativo donde el original tenia cero positivo; se normaliza para no depender de
 * eso. */
using MirrorKey = std::tuple<double, double, double>;

static double normalize_zero(const double value)
{
  return (value == 0.0) ? 0.0 : value;
}

static MirrorKey mirror_key(const float co[3], const int precision)
{
  return MirrorKey(normalize_zero(double_round(double(co[0]), precision)),
                   normalize_zero(double_round(double(co[1]), precision)),
                   normalize_zero(double_round(double(co[2]), precision)));
}

static MirrorKey mirror_key_flip_x(const MirrorKey &key)
{
  return MirrorKey(normalize_zero(-std::get<0>(key)), std::get<1>(key), std::get<2>(key));
}

/* Devuelve <tenia capa UV activa, numero de duplicados detectados>. */
static void mesh_mirror_uv(Mesh *mesh,
                           const bool dir_negative,
                           const int precision,
                           bool *r_has_uv_layer,
                           int *r_double_warn)
{
  *r_has_uv_layer = false;
  *r_double_warn = 0;

  const int uv_index = CustomData_get_active_layer(&mesh->corner_data, CD_PROP_FLOAT2);
  if (uv_index == -1) {
    return;
  }
  *r_has_uv_layer = true;

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  const StringRef uv_name = CustomData_get_active_layer_name(&mesh->corner_data,
                                                             CD_PROP_FLOAT2);
  bke::SpanAttributeWriter<float2> uv_attr = attributes.lookup_for_write_span<float2>(uv_name);
  if (!uv_attr) {
    return;
  }
  const bool *uv_select = ED_mesh_uv_map_vert_select_layer_get(mesh, uv_index);

  const Span<float3> positions = mesh->vert_positions();
  const OffsetIndices faces = mesh->faces();
  const Span<int> corner_verts = mesh->corner_verts();

  /* --- Emparejamiento de vertices por posicion espejada en X. --- */
  std::map<MirrorKey, int> mirror_gt, mirror_lt;
  for (const int i : positions.index_range()) {
    const MirrorKey key = mirror_key(positions[i], precision);
    const double x = std::get<0>(key);
    if (x >= 0.0) {
      /* `double_warn += co in mirror_gt` antes de sobrescribir: cuenta cuantos vertices
       * caen en la misma posicion redondeada. */
      if (mirror_gt.find(key) != mirror_gt.end()) {
        (*r_double_warn)++;
      }
      mirror_gt[key] = i;
    }
    if (x <= 0.0) {
      if (mirror_lt.find(key) != mirror_lt.end()) {
        (*r_double_warn)++;
      }
      mirror_lt[key] = i;
    }
  }

  Array<int> vmap(positions.size(), -1);
  for (int pass = 0; pass < 2; pass++) {
    const std::map<MirrorKey, int> &from = (pass == 0) ? mirror_gt : mirror_lt;
    const std::map<MirrorKey, int> &to = (pass == 0) ? mirror_lt : mirror_gt;
    for (const auto &item : from) {
      const auto found = to.find(mirror_key_flip_x(item.first));
      if (found != to.end()) {
        vmap[item.second] = found->second;
      }
    }
  }

  /* --- Datos por cara. Las UV se copian ANTES de tocar nada: una cara y su espejo
   * pueden emparejarse la una con la otra, y cada una tiene que leer las UV originales
   * de la otra, no las ya reescritas. --- */
  const int faces_num = faces.size();
  Array<bool> face_uv_selected(faces_num);
  Array<float3> face_centers(faces_num);
  Vector<Vector<int>> face_verts(faces_num);
  Vector<Vector<float2>> face_uvs_copy(faces_num);
  std::map<std::vector<int>, int> mirror_pm;

  for (const int i : IndexRange(faces_num)) {
    const IndexRange face = faces[i];
    bool all_selected = true;
    float3 center(0.0f, 0.0f, 0.0f);
    for (const int corner : face) {
      /* TRAMPA: `uv.select` de RNA devuelve FALSE cuando la malla no tiene capa de
       * seleccion de UV (`rna_MeshUVLoop_select_get`: `return select ? select[i] : false`).
       * O sea, en una malla recien creada `puvsel` es todo falso y el operador NO COPIA
       * NADA. Tratar la ausencia de capa como "todo seleccionado" parece lo razonable y
       * es justo lo contrario de lo que hace el original. */
      if (uv_select == nullptr || !uv_select[corner]) {
        all_selected = false;
      }
      face_verts[i].append(corner_verts[corner]);
      face_uvs_copy[i].append(uv_attr.span[corner]);
      center += positions[corner_verts[corner]];
    }
    face_uv_selected[i] = all_selected;
    face_centers[i] = center / float(face.size());

    std::vector<int> sorted_verts(face_verts[i].begin(), face_verts[i].end());
    std::sort(sorted_verts.begin(), sorted_verts.end());
    mirror_pm[sorted_verts] = i;
  }

  /* --- Emparejamiento de caras: una cara casa con la que tiene exactamente el conjunto
   * de vertices espejados. --- */
  Vector<std::pair<int, int>> pmap;
  for (const int i : IndexRange(faces_num)) {
    std::vector<int> tvidxs;
    bool complete = true;
    for (const int v : face_verts[i]) {
      if (vmap[v] == -1) {
        complete = false;
        break;
      }
      tvidxs.push_back(vmap[v]);
    }
    if (!complete) {
      continue;
    }
    std::sort(tvidxs.begin(), tvidxs.end());
    const auto found = mirror_pm.find(tvidxs);
    if (found != mirror_pm.end()) {
      pmap.append({i, found->second});
    }
  }

  /* --- Copia de las UV. --- */
  for (const std::pair<int, int> &pair : pmap) {
    const int i = pair.first;
    const int j = pair.second;
    if (!face_uv_selected[i] || !face_uv_selected[j]) {
      continue;
    }
    /* `DIR == 0` copia del lado positivo al negativo y `DIR == 1` al reves. */
    if (!dir_negative && face_centers[i][0] < 0.0f) {
      continue;
    }
    if (dir_negative && face_centers[i][0] > 0.0f) {
      continue;
    }

    const Vector<int> &v1 = face_verts[j];
    Vector<int> v2;
    for (const int v : face_verts[i]) {
      v2.append(vmap[v]);
    }
    if (v1.size() != v2.size()) {
      continue;
    }

    const IndexRange face_i = faces[i];
    for (const int k : v1.index_range()) {
      /* `v1.index(v2[k])` de Python: el PRIMER indice que coincide. */
      const int k_map = int(v1.first_index_of_try(v2[k]));
      if (k_map < 0) {
        continue;
      }
      const float2 &src = face_uvs_copy[j][k_map];
      uv_attr.span[face_i[k]] = float2(-(src.x - 0.5f) + 0.5f, src.y);
    }
  }

  uv_attr.finish();
}

static wmOperatorStatus mesh_faces_mirror_uv_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  const bool dir_negative = (RNA_enum_get(op->ptr, "direction") == 1);
  const int precision = RNA_int_get(op->ptr, "precision");

  Object *ob = CTX_data_active_object(C);
  const bool is_editmode = (ob != nullptr && ob->mode == OB_MODE_EDIT);
  if (is_editmode) {
    PointerRNA ptr;
    WM_operator_properties_create(&ptr, "object.mode_set");
    RNA_enum_set_identifier(C, &ptr, "mode", "OBJECT");
    RNA_boolean_set(&ptr, "toggle", false);
    WM_operator_name_call(C, "object.mode_set", WM_OP_EXEC_DEFAULT, &ptr, nullptr);
    WM_operator_properties_free(&ptr);
  }

  Vector<Mesh *> meshes;
  CTX_DATA_BEGIN (C, Object *, ob_iter, selected_objects) {
    if (ob_iter->type != OB_MESH) {
      continue;
    }
    Mesh *mesh = static_cast<Mesh *>(ob_iter->data);
    if (!ID_IS_EDITABLE(&mesh->id)) {
      continue;
    }
    meshes.append(mesh);
  }
  CTX_DATA_END;

  /* `mesh.tag` del Python: la misma malla puede estar en varios objetos seleccionados y
   * solo se procesa una vez. */
  for (Mesh *mesh : meshes) {
    mesh->id.tag &= ~ID_TAG_DOIT;
  }

  int total_no_active_uv = 0;
  int total_duplicates = 0;
  int meshes_with_duplicates = 0;

  for (Mesh *mesh : meshes) {
    if (mesh->id.tag & ID_TAG_DOIT) {
      continue;
    }
    mesh->id.tag |= ID_TAG_DOIT;

    bool has_uv_layer;
    int double_warn;
    mesh_mirror_uv(mesh, dir_negative, precision, &has_uv_layer, &double_warn);

    if (!has_uv_layer) {
      total_no_active_uv++;
    }
    else if (double_warn != 0) {
      total_duplicates += double_warn;
      meshes_with_duplicates++;
    }
    DEG_id_tag_update(&mesh->id, ID_RECALC_GEOMETRY);
  }

  if (is_editmode) {
    PointerRNA ptr;
    WM_operator_properties_create(&ptr, "object.mode_set");
    RNA_enum_set_identifier(C, &ptr, "mode", "EDIT");
    RNA_boolean_set(&ptr, "toggle", false);
    WM_operator_name_call(C, "object.mode_set", WM_OP_EXEC_DEFAULT, &ptr, nullptr);
    WM_operator_properties_free(&ptr);
  }

  /* Los tres avisos, con los mismos textos y el mismo orden de comprobacion. */
  if (total_duplicates != 0 && total_no_active_uv != 0) {
    BKE_reportf(op->reports,
                RPT_WARNING,
                RPT_("%d mesh(es) with no active UV layer, %d duplicates found in %d mesh(es), "
                     "mirror may be incomplete"),
                total_no_active_uv,
                total_duplicates,
                meshes_with_duplicates);
  }
  else if (total_no_active_uv != 0) {
    BKE_reportf(op->reports,
                RPT_WARNING,
                RPT_("%d mesh(es) with no active UV layer"),
                total_no_active_uv);
  }
  else if (total_duplicates != 0) {
    BKE_reportf(op->reports,
                RPT_WARNING,
                RPT_("%d duplicates found in %d mesh(es), mirror may be incomplete"),
                total_duplicates,
                meshes_with_duplicates);
  }

  WM_event_add_notifier(C, NC_GEOM | ND_DATA, nullptr);
  UNUSED_VARS(bmain);
  return OPERATOR_FINISHED;
}

static bool mesh_faces_mirror_uv_poll(bContext *C)
{
  /* `obj = context.view_layer.objects.active; return (obj and obj.type == 'MESH')`. */
  const Object *ob = CTX_data_active_object(C);
  return ob != nullptr && ob->type == OB_MESH;
}

void MESH_OT_faces_mirror_uv(wmOperatorType *ot)
{
  static const EnumPropertyItem direction_items[] = {
      {0, "POSITIVE", 0, "Positive", ""},
      {1, "NEGATIVE", 0, "Negative", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Copy Mirrored UV Coords";
  ot->idname = "MESH_OT_faces_mirror_uv";
  ot->description = "Copy mirror UV coordinates on the X axis based on a mirrored mesh";

  /* API callbacks. */
  ot->exec = mesh_faces_mirror_uv_exec;
  ot->poll = mesh_faces_mirror_uv_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_enum(ot->srna, "direction", direction_items, 0, "Axis Direction", "");
  RNA_def_int(ot->srna,
              "precision",
              3,
              1,
              16,
              "Precision",
              "Tolerance for finding vertex duplicates",
              1,
              16);
}
