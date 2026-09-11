/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 *
 * Puerto nativo en C++ de `object.make_dupli_face`, que vivia en
 * `scripts/startup/bl_operators/object.py` (clase `MakeDupliFace`, Flipendo carril C).
 *
 * Convierte los objetos seleccionados en instancias sobre caras: agrupa por dato
 * compartido (la malla, o la coleccion si es un empty de instancia), construye una malla
 * nueva con una cara cuadrada diminuta por cada objeto —colocada y orientada por su
 * matriz de mundo— y cuelga de ella un unico objeto instanciado.
 */

#include <map>

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "DNA_collection_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_attribute.hh"
#include "BKE_collection.hh"
#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_mesh.hh"
#include "BKE_object.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_mesh.hh"
#include "ED_object.hh"

#include "object_intern.hh"

namespace blender::ed::object {

/* El cuadrado base tiene medio centimetro de lado y luego se compensa con
 * `instance_faces_scale = 1 / SCALE_FAC`. No se toca: cambiar la constante cambia el
 * tamano de todas las instancias generadas. */
static const float SCALE_FAC = 0.01f;

static wmOperatorStatus make_dupli_face_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Collection *collection = CTX_data_collection(C);

  const float offset = 0.5f * SCALE_FAC;
  const float base_quad[4][3] = {
      {-offset, -offset, 0.0f},
      {+offset, -offset, 0.0f},
      {+offset, +offset, 0.0f},
      {-offset, +offset, 0.0f},
  };

  /* `linked` es un `defaultdict(list)`: agrupa por dato compartido conservando el orden
   * en que aparece cada clave por primera vez. */
  Vector<ID *> linked_keys;
  Vector<Vector<Object *>> linked_values;
  std::map<ID *, int> linked_index;

  CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
    ID *key = nullptr;
    if (ob->type == OB_MESH) {
      key = static_cast<ID *>(ob->data);
    }
    else if (ob->type == OB_EMPTY && (ob->transflag & OB_DUPLICOLLECTION) &&
             ob->instance_collection != nullptr)
    {
      key = &ob->instance_collection->id;
    }
    if (key == nullptr) {
      continue;
    }
    const auto found = linked_index.find(key);
    int index;
    if (found == linked_index.end()) {
      index = linked_keys.size();
      linked_index[key] = index;
      linked_keys.append(key);
      linked_values.append({});
    }
    else {
      index = found->second;
    }
    linked_values[index].append(ob);
  }
  CTX_DATA_END;

  for (const int group : linked_keys.index_range()) {
    ID *data = linked_keys[group];
    const Vector<Object *> &objects = linked_values[group];

    const int nbr_faces = objects.size();
    const int nbr_verts = nbr_faces * 4;

    char mesh_name[MAX_ID_NAME - 2];
    BLI_snprintf(mesh_name, sizeof(mesh_name), "%s_dupli", data->name + 2);
    Mesh *mesh = BKE_mesh_add(bmain, mesh_name);
    id_us_min(&mesh->id);

    ED_mesh_verts_add(mesh, nullptr, nbr_verts);
    ED_mesh_loops_add(mesh, nullptr, nbr_faces * 4);
    ED_mesh_faces_add(mesh, nullptr, nbr_faces);

    MutableSpan<float3> positions = mesh->vert_positions_for_write();
    MutableSpan<int> corner_verts = mesh->corner_verts_for_write();
    MutableSpan<int> face_offsets = mesh->face_offsets_for_write();

    int vert_index = 0;
    for (Object *ob : objects) {
      /* `matrix_to_quad()`: la parte 3x3 de la matriz de mundo (que ya lleva la escala)
       * aplicada al cuadrado base, mas la traslacion. */
      float mat3[3][3];
      copy_m3_m4(mat3, ob->object_to_world().ptr());
      const float *trans = ob->object_to_world().location();
      for (const float(&corner)[3] : base_quad) {
        float co[3];
        mul_v3_m3v3(co, mat3, corner);
        add_v3_v3(co, trans);
        positions[vert_index] = float3(co);
        vert_index++;
      }
    }
    for (const int i : IndexRange(nbr_faces * 4)) {
      corner_verts[i] = i;
    }
    for (const int i : IndexRange(nbr_faces)) {
      face_offsets[i] = i * 4;
    }
    face_offsets[nbr_faces] = nbr_faces * 4;

    /* `mesh.update()`: genera los datos de arista. */
    bke::mesh_calc_edges(*mesh, true, false);
    mesh->tag_topology_changed();

    Object *ob_new = BKE_object_add_only_object(bmain, OB_MESH, mesh->id.name + 2);
    ob_new->data = mesh;
    id_us_plus(&mesh->id);
    BKE_collection_object_add(bmain, collection, ob_new);

    Object *ob_inst = nullptr;
    if (GS(data->name) == ID_GR) {
      ob_inst = BKE_object_add_only_object(bmain, OB_EMPTY, data->name + 2);
      ob_inst->data = nullptr;
      ob_inst->transflag |= OB_DUPLICOLLECTION;
      ob_inst->instance_collection = reinterpret_cast<Collection *>(data);
      id_us_plus(data);
    }
    else {
      ob_inst = BKE_object_add_only_object(bmain, OB_MESH, data->name + 2);
      ob_inst->data = data;
      id_us_plus(data);
    }
    BKE_collection_object_add(bmain, collection, ob_inst);

    ob_new->transflag |= OB_DUPLIFACES;
    parent_set(ob_inst, ob_new, ob_inst->partype, ob_inst->parsubstr);
    ob_new->transflag |= OB_DUPLIFACES_SCALE;
    ob_new->instance_faces_scale = 1.0f / SCALE_FAC;

    BKE_view_layer_synced_ensure(scene, view_layer);
    for (Object *ob_sel : {ob_inst, ob_new}) {
      if (Base *base = BKE_view_layer_base_find(view_layer, ob_sel)) {
        base_select(base, BA_SELECT);
      }
    }

    /* Los objetos originales salen de todas sus colecciones. */
    for (Object *ob : objects) {
      LISTBASE_FOREACH (Collection *, coll, &bmain->collections) {
        if (BKE_collection_has_object(coll, ob)) {
          BKE_collection_object_remove(bmain, coll, ob, false);
        }
      }
      if (BKE_collection_has_object(scene->master_collection, ob)) {
        BKE_collection_object_remove(bmain, scene->master_collection, ob, false);
      }
    }
  }

  DEG_relations_tag_update(bmain);
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, nullptr);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
  return OPERATOR_FINISHED;
}

void OBJECT_OT_make_dupli_face(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Make Instance Face";
  ot->idname = "OBJECT_OT_make_dupli_face";
  ot->description = "Convert objects into instanced faces";

  /* API callbacks. */
  ot->exec = make_dupli_face_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

}  // namespace blender::ed::object
