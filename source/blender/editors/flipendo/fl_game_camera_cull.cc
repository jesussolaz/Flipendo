/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 *
 * Descarte de vertices por la camara, en C++ nativo.
 *
 * Migracion de `scripts/addons_core/game_engine_object_camera_vertex_cull.py`
 * (287 lineas, UPBGE; autor Mitko Nikov).
 *
 * QUE HACE, IGUAL QUE EL ORIGINAL
 *
 * Para cada vertice de la malla se calcula su posicion en la vista de la camara
 * de la escena. Los que caen fuera del rectangulo `[-margin, 1+margin]` en las
 * dos direcciones —o, si se activa el descarte por distancia, los que estan mas
 * lejos que `distance`— se marcan con peso 1 en el grupo de vertices
 * `Hide_Group`; los demas quedan a 0. Un modificador Mask con
 * `invert_vertex_group` los esconde. Es exactamente la receta del addon
 * (`update_object()`), incluido el nombre del grupo, que es dato de usuario.
 *
 * DONDE VIVEN LOS AJUSTES
 *
 * El addon los colgaba de `Object.camera_cull_props`, un `PropertyGroup` de
 * Python. Un `PropertyGroup` nativo equivalente pide DNA nueva, es decir tocar
 * el formato del `.blend`, y eso no es de este carril. Se usan propiedades de
 * ID sobre el objeto (`IDProperty`), que son nativas, viajan en el `.blend`, se
 * ven y se editan desde el panel, y no cambian el formato. Los nombres llevan
 * prefijo `fl_camera_cull_` para no chocar con nada del usuario.
 *
 * Doctrina: `politicas/LENGUAJE-CPP.md`, `politicas/ADDONS-MOTOR-A-CPP.md`.
 */

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"

#include "BLT_translation.hh"

#include "DNA_camera_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_camera.h"
#include "BKE_context.hh"
#include "BKE_deform.hh"
#include "BKE_idprop.hh"
#include "BKE_main.hh"
#include "BKE_mesh.hh"
#include "BKE_modifier.hh"
#include "BKE_object_deform.h"
#include "BKE_report.hh"
#include "BKE_screen.hh"

#include "DEG_depsgraph.hh"

#include "ED_object.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "FL_game_runtime.hh"
#include "FL_ui_registry.hh"

namespace flipendo::game {

/* Nombre del grupo de vertices. Es dato de usuario: el addon lo llamaba asi y
 * los `.blend` que ya lo usan lo tienen escrito. No se toca. */
static const char *FL_HIDE_GROUP = "Hide_Group";

static const char *FL_PROP_ENABLED = "fl_camera_cull_enabled";
static const char *FL_PROP_DIST_ENABLED = "fl_camera_cull_distance_enabled";
static const char *FL_PROP_MARGIN = "fl_camera_cull_margin";
static const char *FL_PROP_DISTANCE = "fl_camera_cull_distance";

/* -------------------------------------------------------------------- */
/** \name Ajustes en propiedades de ID
 * \{ */

static float cull_prop_get_float(const Object *ob, const char *name, const float fallback)
{
  IDProperty *group = IDP_GetProperties(const_cast<ID *>(&ob->id));
  if (group == nullptr) {
    return fallback;
  }
  IDProperty *prop = IDP_GetPropertyFromGroup(group, name);
  if (prop == nullptr) {
    return fallback;
  }
  if (prop->type == IDP_FLOAT) {
    return IDP_Float(prop);
  }
  if (prop->type == IDP_DOUBLE) {
    return float(IDP_Double(prop));
  }
  if (prop->type == IDP_INT) {
    return float(IDP_Int(prop));
  }
  return fallback;
}

static bool cull_prop_get_bool(const Object *ob, const char *name, const bool fallback)
{
  return cull_prop_get_float(ob, name, fallback ? 1.0f : 0.0f) != 0.0f;
}

static void cull_prop_set_float(Object *ob, const char *name, const float value)
{
  IDPropertyTemplate val = {0};
  val.f = value;
  IDProperty *prop = IDP_New(IDP_FLOAT, &val, name);
  IDP_ReplaceInGroup(IDP_EnsureProperties(&ob->id), prop);
}

static void cull_prop_set_bool(Object *ob, const char *name, const bool value)
{
  IDPropertyTemplate val = {0};
  val.i = value ? 1 : 0;
  IDProperty *prop = IDP_New(IDP_INT, &val, name);
  IDP_ReplaceInGroup(IDP_EnsureProperties(&ob->id), prop);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name La proyeccion, la misma que `bpy_extras.object_utils`
 * \{ */

/**
 * Traduccion literal de `world_to_camera_view()`
 * (`scripts/modules/bpy_extras/object_utils.py:229-262`): devuelve la posicion
 * normalizada del punto en el encuadre de la camara, con `z` = distancia al
 * plano de la lente.
 */
static void world_to_camera_view(const Scene *scene,
                                 const Object *camera_ob,
                                 const float co_world[3],
                                 float r_out[3])
{
  const Camera *camera = static_cast<const Camera *>(camera_ob->data);

  float mat[4][4];
  normalize_m4_m4(mat, camera_ob->object_to_world().ptr());
  float inv[4][4];
  invert_m4_m4(inv, mat);

  float co_local[3];
  mul_v3_m4v3(co_local, inv, co_world);
  const float z = -co_local[2];

  float frame[4][3];
  BKE_camera_view_frame(scene, camera, frame);
  /* El Python hace `[-v for v in ...[:3]]`: niega los tres primeros vertices. */
  for (int i = 0; i < 3; i++) {
    negate_v3(frame[i]);
  }

  if (camera->type != CAM_ORTHO) {
    if (z == 0.0f) {
      r_out[0] = 0.5f;
      r_out[1] = 0.5f;
      r_out[2] = 0.0f;
      return;
    }
    for (int i = 0; i < 3; i++) {
      const float fz = frame[i][2];
      if (fz != 0.0f) {
        const float s = z / fz;
        mul_v3_fl(frame[i], s);
      }
    }
  }

  const float min_x = frame[2][0], max_x = frame[1][0];
  const float min_y = frame[1][1], max_y = frame[0][1];

  r_out[0] = (max_x != min_x) ? (co_local[0] - min_x) / (max_x - min_x) : 0.5f;
  r_out[1] = (max_y != min_y) ? (co_local[1] - min_y) / (max_y - min_y) : 0.5f;
  r_out[2] = z;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name El calculo
 * \{ */

/**
 * Recalcula `Hide_Group` para un objeto de malla y se asegura de que existe el
 * modificador Mask. Devuelve el numero de vertices marcados, o -1 si el objeto
 * no se puede procesar.
 */
static int camera_cull_update_object(
    Main *bmain, Scene *scene, Object *ob, Object *camera, ReportList *reports)
{
  if (ob == nullptr || ob->type != OB_MESH || camera == nullptr || camera->type != OB_CAMERA) {
    return -1;
  }
  Mesh *mesh = static_cast<Mesh *>(ob->data);
  if (mesh == nullptr) {
    return -1;
  }

  const bool enabled = cull_prop_get_bool(ob, FL_PROP_ENABLED, false);
  const bool dist_enabled = cull_prop_get_bool(ob, FL_PROP_DIST_ENABLED, false);
  const float margin = cull_prop_get_float(ob, FL_PROP_MARGIN, 0.3f);
  const float distance = cull_prop_get_float(ob, FL_PROP_DISTANCE, 30.0f);

  bDeformGroup *dg = BKE_object_defgroup_find_name(ob, FL_HIDE_GROUP);
  if (dg == nullptr) {
    if (!enabled) {
      /* Igual que el addon: si no esta activado y no habia grupo, no se crea. */
      return 0;
    }
    dg = BKE_object_defgroup_add_name(ob, FL_HIDE_GROUP);
    if (dg == nullptr) {
      return -1;
    }
  }
  const int def_nr = BLI_findindex(&mesh->vertex_group_names, dg);

  MDeformVert *dverts = mesh->deform_verts_for_write().data();
  if (dverts == nullptr) {
    dverts = BKE_object_defgroup_data_create(&mesh->id);
    if (dverts == nullptr) {
      return -1;
    }
  }

  const blender::Span<blender::float3> positions = mesh->vert_positions();
  const float(*obmat)[4] = ob->object_to_world().ptr();

  int hidden = 0;
  for (int i = 0; i < mesh->verts_num; i++) {
    /* Todos a 0 primero, que es lo que hacia el addon con "SUBTRACT". */
    MDeformWeight *dw = BKE_defvert_ensure_index(&dverts[i], def_nr);
    dw->weight = 0.0f;

    if (!enabled) {
      continue;
    }

    float co_world[3];
    mul_v3_m4v3(co_world, obmat, positions[i]);

    float view[3];
    world_to_camera_view(scene, camera, co_world, view);

    const bool inside_frame = (view[0] >= -margin) && (view[0] <= 1.0f + margin) &&
                              (view[1] >= -margin) && (view[1] <= 1.0f + margin);
    if (inside_frame) {
      if (!dist_enabled || view[2] <= distance) {
        continue;
      }
    }
    dw->weight = 1.0f;
    hidden++;
  }

  /* El modificador Mask, con el grupo invertido. */
  if (enabled) {
    MaskModifierData *mmd = nullptr;
    LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
      if (md->type == eModifierType_Mask) {
        mmd = reinterpret_cast<MaskModifierData *>(md);
        break;
      }
    }
    if (mmd == nullptr) {
      ModifierData *md = blender::ed::object::modifier_add(
          reports, bmain, scene, ob, "Mask", eModifierType_Mask);
      mmd = reinterpret_cast<MaskModifierData *>(md);
    }
    if (mmd != nullptr) {
      mmd->mode = MOD_MASK_MODE_VGROUP;
      STRNCPY(mmd->vgroup, FL_HIDE_GROUP);
      mmd->flag |= MOD_MASK_INV;
    }
  }

  DEG_id_tag_update(&ob->id, ID_RECALC_GEOMETRY);
  return hidden;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name `object.camera_vertex_cull`
 * \{ */

static wmOperatorStatus camera_vertex_cull_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *ob = CTX_data_active_object(C);

  if (ob == nullptr || ob->type != OB_MESH) {
    BKE_report(op->reports, RPT_ERROR, "Select a mesh object");
    return OPERATOR_CANCELLED;
  }
  if (scene == nullptr || scene->camera == nullptr) {
    /* Mismo mensaje que imprimia `getCamera()`. */
    BKE_report(op->reports, RPT_ERROR, "No scene camera");
    return OPERATOR_CANCELLED;
  }

  cull_prop_set_bool(ob, FL_PROP_ENABLED, RNA_boolean_get(op->ptr, "camera_cull_enabled"));
  cull_prop_set_bool(
      ob, FL_PROP_DIST_ENABLED, RNA_boolean_get(op->ptr, "distance_cull_enabled"));
  cull_prop_set_float(ob, FL_PROP_MARGIN, RNA_float_get(op->ptr, "margin"));
  cull_prop_set_float(ob, FL_PROP_DISTANCE, RNA_float_get(op->ptr, "distance"));

  const int hidden = camera_cull_update_object(bmain, scene, ob, scene->camera, op->reports);
  if (hidden < 0) {
    BKE_report(op->reports, RPT_ERROR, "No se pudo calcular el descarte");
    return OPERATOR_CANCELLED;
  }

  printf("FL-CULL object=%s verts=%d hidden=%d\n", ob->id.name + 2, ((Mesh *)ob->data)->verts_num, hidden);
  BKE_reportf(op->reports, RPT_INFO, "Camera Vertex Cull: %d vertices ocultos", hidden);
  WM_event_add_notifier(C, NC_OBJECT | ND_MODIFIER, ob);
  return OPERATOR_FINISHED;
}

static void OBJECT_OT_camera_vertex_cull(wmOperatorType *ot)
{
  ot->name = "Camera Vertex Cull";
  ot->idname = "OBJECT_OT_camera_vertex_cull";
  ot->description = "Hide vertices, edges and polys based on Camera Frustum";

  ot->exec = camera_vertex_cull_exec;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Mismos nombres, descripciones, defectos y rangos que `CameraCullProperties`. */
  RNA_def_boolean(ot->srna,
                  "camera_cull_enabled",
                  false,
                  "Enable",
                  "Hide vertexes based on the Camera frustum");
  RNA_def_boolean(ot->srna,
                  "distance_cull_enabled",
                  false,
                  "Enable Distance Cull",
                  "Hide vertexes based on the Camera Distance");
  RNA_def_float(ot->srna,
                "margin",
                0.3f,
                0.0f,
                100.0f,
                "Margin",
                "Threshold outside the Camera frustum",
                0.0f,
                100.0f);
  RNA_def_float(
      ot->srna, "distance", 30.0f, 0.0f, 1000.0f, "Distance", "Culling Distance", 0.0f, 1000.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Panel `PLS_PT_camera_vertex_cull`
 * \{ */

static void camera_cull_panel_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  Object *ob = CTX_data_active_object(const_cast<bContext *>(C));

  if (ob == nullptr || ob->type != OB_MESH) {
    /* Mismo texto que el addon. */
    layout->label(IFACE_("Select a mesh object"), ICON_NONE);
    return;
  }

  PointerRNA ptr = RNA_id_pointer_create(&ob->id);
  char path[128];

  uiLayout *row = &layout->row(true);
  SNPRINTF(path, "[\"%s\"]", FL_PROP_ENABLED);
  row->prop(&ptr, path, UI_ITEM_NONE, IFACE_("Enable"), ICON_NONE);
  SNPRINTF(path, "[\"%s\"]", FL_PROP_DIST_ENABLED);
  row->prop(&ptr, path, UI_ITEM_NONE, IFACE_("Enable Distance Cull"), ICON_NONE);

  SNPRINTF(path, "[\"%s\"]", FL_PROP_MARGIN);
  layout->prop(&ptr, path, UI_ITEM_NONE, IFACE_("Margin"), ICON_NONE);
  SNPRINTF(path, "[\"%s\"]", FL_PROP_DISTANCE);
  layout->prop(&ptr, path, UI_ITEM_NONE, IFACE_("Distance"), ICON_NONE);

  layout->op("OBJECT_OT_camera_vertex_cull", IFACE_("Camera Vertex Cull"), ICON_NONE);
}

static void camera_cull_panel_register()
{
  SpaceType *st = BKE_spacetype_from_id(SPACE_PROPERTIES);
  if (st == nullptr) {
    return;
  }
  ARegionType *art = BKE_regiontype_from_id(st, RGN_TYPE_WINDOW);
  if (art == nullptr) {
    return;
  }

  /* Mismos `bl_idname`, `bl_label` y `bl_context` que la clase de Python. */
  PanelDecl panel{};
  panel.idname = "PLS_PT_camera_vertex_cull";
  panel.label = N_("Camera Vertex Cull");
  panel.context = "object";
  panel.flag = PANEL_TYPE_DEFAULT_CLOSED;
  panel.draw = camera_cull_panel_draw;
  panels_register(art, SPACE_PROPERTIES, blender::Span<const PanelDecl>(&panel, 1));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registro
 * \{ */

void camera_cull_operatortypes_register()
{
  WM_operatortype_append(OBJECT_OT_camera_vertex_cull);
  camera_cull_panel_register();
}

/** \} */

}  // namespace flipendo::game
