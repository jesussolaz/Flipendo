/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 *
 * Ver FL_view3d_menus.hh.
 *
 * Familia 4: los menus de objeto, pose y mapeado UV que el keymap nativo abre
 * con Ctrl-A (aplicar), U (usuario unico y mapeado UV), Ctrl-L (enlazar),
 * Ctrl-H (ganchos), Ctrl-G (grupos de vertices) y Alt-P (propagar pose).
 *
 * Estos ya no son tablas planas: cuatro de ellos deciden que dibujan mirando el
 * objeto activo, sus modificadores, sus grupos de vertices o cuantas escenas hay
 * en el fichero. Cada condicion se reproduce por el mismo camino que usaba el
 * Python — el descriptor de RNA correspondiente —, no por uno parecido.
 */

#include <optional>

#include "BLI_listbase.h"
#include "BLI_vector.hh"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_deform.hh"
#include "BKE_main.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "DNA_camera_types.h"
#include "DNA_light_types.h"
#include "DNA_mesh_types.h"
#include "DNA_view3d_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "ED_geometry.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "FL_ui_registry.hh"

#include "FL_view3d_menus.hh"

namespace blender::ed::view3d {

/**
 * `layout.operator_menu_enum(op, prop)` **sin** `text=`.
 *
 * Trampa: el Python pasa `None`, `rna_translate_ui_text` lo convierte en
 * `std::nullopt`, y es ese `nullopt` — no una cadena vacia — lo que hace que la
 * etiqueta salga del nombre del propio operador. Pasar `""` deja el boton sin
 * texto, y el volcado de dibujo lo canta.
 */
static void menu_enum_o(uiLayout *layout,
                        const bContext *C,
                        const char *opname,
                        const char *propname)
{
  wmOperatorType *ot = WM_operatortype_find(opname, false);
  if (ot == nullptr) {
    return;
  }
  PointerRNA opptr;
  uiItemMenuEnumFullO_ptr(layout, C, ot, propname, std::nullopt, ICON_NONE, &opptr);
}

/* -------------------------------------------------------------------- */
/** \name VIEW3D_MT_object_apply
 * \{ */

static void transform_apply_item(
    uiLayout *layout, const char *text, bool location, bool rotation, bool scale)
{
  PointerRNA props = layout->op("OBJECT_OT_transform_apply", IFACE_(text), ICON_NONE);
  if (props.data == nullptr) {
    return;
  }
  RNA_boolean_set(&props, "location", location);
  RNA_boolean_set(&props, "rotation", rotation);
  RNA_boolean_set(&props, "scale", scale);
}

static void transforms_to_deltas_item(uiLayout *layout, const char *text, const char *mode)
{
  PointerRNA props = layout->op("OBJECT_OT_transforms_to_deltas", IFACE_(text), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "mode", mode);
  }
}

static void object_apply_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  /* Hace falta `INVOKE` para el aviso de datos con varios usuarios. */
  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);

  transform_apply_item(layout, N_("Location"), true, false, false);
  transform_apply_item(layout, N_("Rotation"), false, true, false);
  transform_apply_item(layout, N_("Scale"), false, false, true);
  transform_apply_item(layout, N_("All Transforms"), true, true, true);
  transform_apply_item(layout, N_("Rotation & Scale"), false, true, true);

  layout->separator();

  transforms_to_deltas_item(layout, N_("Location to Deltas"), "LOC");
  transforms_to_deltas_item(layout, N_("Rotation to Deltas"), "ROT");
  transforms_to_deltas_item(layout, N_("Scale to Deltas"), "SCALE");
  transforms_to_deltas_item(layout, N_("All Transforms to Deltas"), "ALL");
  layout->op("OBJECT_OT_anim_transforms_to_deltas", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("OBJECT_OT_visual_transform_apply", IFACE_("Visual Transform"), ICON_NONE);
  PointerRNA props = layout->op(
      "OBJECT_OT_convert", IFACE_("Visual Geometry to Mesh"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "target", "MESH");
  }
  layout->op("OBJECT_OT_visual_geometry_to_objects", std::nullopt, ICON_NONE);
  layout->op("OBJECT_OT_duplicates_make_real", std::nullopt, ICON_NONE);
  layout->op("OBJECT_OT_parent_inverse_apply", IFACE_("Parent Inverse"), ICON_NONE);

  geometry::ui_template_node_operator_asset_menu_items(
      *layout, *const_cast<bContext *>(C), "Object/Apply");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_MT_make_single_user
 * \{ */

static void make_single_user_item(uiLayout *layout,
                                  const char *text,
                                  bool object,
                                  bool obdata,
                                  bool material,
                                  bool animation,
                                  bool obdata_animation)
{
  PointerRNA props = layout->op("OBJECT_OT_make_single_user", IFACE_(text), ICON_NONE);
  if (props.data == nullptr) {
    return;
  }
  RNA_boolean_set(&props, "object", object);
  RNA_boolean_set(&props, "obdata", obdata);
  RNA_boolean_set(&props, "material", material);
  RNA_boolean_set(&props, "animation", animation);
  RNA_boolean_set(&props, "obdata_animation", obdata_animation);
}

static void make_single_user_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;
  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_REGION_WIN);

  make_single_user_item(layout, N_("Object"), true, false, false, false, false);
  make_single_user_item(layout, N_("Object & Data"), true, true, false, false, false);
  make_single_user_item(layout, N_("Object & Data & Materials"), true, true, true, false, false);
  make_single_user_item(layout, N_("Materials"), false, false, true, false, false);
  make_single_user_item(layout, N_("Object Animation"), false, false, false, true, false);
  make_single_user_item(layout, N_("Object Data Animation"), false, true, false, false, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_MT_make_links
 * \{ */

static void make_links_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;
  const wmOperatorCallContext opcontext_default = uiLayoutGetOperatorContext(layout);

  /* `len(bpy.data.scenes) > 10`: con muchas escenas el desplegable no cabe y el
   * Python cambia a la version con dialogo. */
  const Main *bmain = CTX_data_main(C);
  const int scenes_num = bmain ? BLI_listbase_count(&bmain->scenes) : 0;

  if (scenes_num > 10) {
    uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);
    layout->op("OBJECT_OT_make_links_scene",
               IFACE_("Link Objects to Scene..."),
               ICON_OUTLINER_OB_EMPTY);
  }
  else {
    uiLayoutSetOperatorContext(layout, WM_OP_EXEC_REGION_WIN);
    uiItemMenuEnumO(
        layout, C, "OBJECT_OT_make_links_scene", "scene", IFACE_("Link Objects to Scene"), ICON_NONE);
  }

  layout->separator();

  uiLayoutSetOperatorContext(layout, opcontext_default);

  uiItemsEnumO(layout, "OBJECT_OT_make_links_data", "type");

  layout->op("OBJECT_OT_join_uvs", IFACE_("Copy UV Maps"), ICON_NONE);

  layout->separator();
  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);
  layout->op("OBJECT_OT_data_transfer", std::nullopt, ICON_NONE);
  layout->op("OBJECT_OT_datalayout_transfer", std::nullopt, ICON_NONE);

  layout->separator();
  menu_enum_o(layout, C, "OBJECT_OT_light_linking_receivers_link", "link_state");
  menu_enum_o(layout, C, "OBJECT_OT_light_linking_blockers_link", "link_state");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_MT_hook
 * \{ */

static void hook_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;
  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_AREA);

  layout->op("OBJECT_OT_hook_add_newob", std::nullopt, ICON_NONE);
  PointerRNA props = layout->op("OBJECT_OT_hook_add_selob", std::nullopt, ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "use_bone", false);
  }
  props = layout->op(
      "OBJECT_OT_hook_add_selob", IFACE_("Hook to Selected Object Bone"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "use_bone", true);
  }

  /* `any([mod.type == 'HOOK' for mod in context.active_object.modifiers])`. */
  const Object *ob = CTX_data_active_object(C);
  bool has_hook = false;
  if (ob) {
    LISTBASE_FOREACH (const ModifierData *, md, &ob->modifiers) {
      if (md->type == eModifierType_Hook) {
        has_hook = true;
        break;
      }
    }
  }

  if (has_hook) {
    layout->separator();

    menu_enum_o(layout, C, "OBJECT_OT_hook_assign", "modifier");
    menu_enum_o(layout, C, "OBJECT_OT_hook_remove", "modifier");

    layout->separator();

    menu_enum_o(layout, C, "OBJECT_OT_hook_select", "modifier");
    menu_enum_o(layout, C, "OBJECT_OT_hook_reset", "modifier");
    menu_enum_o(layout, C, "OBJECT_OT_hook_recenter", "modifier");
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_MT_vertex_group
 * \{ */

/** `ob.vertex_groups.active` (`rna_Object_active_vertex_group_get`). */
static bool object_has_active_vertex_group(const Object *ob)
{
  if (ob == nullptr || !BKE_object_supports_vertex_groups(ob)) {
    return false;
  }
  const ListBase *defbase = BKE_object_defgroup_list(ob);
  return BLI_findlink(defbase, BKE_object_defgroup_active_index_get(ob) - 1) != nullptr;
}

static void vertex_group_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_AREA);
  layout->op("OBJECT_OT_vertex_group_assign_new", std::nullopt, ICON_NONE);

  const Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return;
  }
  const bool paint_mask_vertex = (ob->type == OB_MESH) &&
                                 ((static_cast<const Mesh *>(ob->data)->editflag &
                                   ME_EDIT_PAINT_VERT_SEL) != 0);
  const bool has_active = object_has_active_vertex_group(ob);

  if (ob->mode == OB_MODE_EDIT || (ob->mode == OB_MODE_WEIGHT_PAINT && paint_mask_vertex)) {
    if (has_active) {
      layout->separator();

      layout->op("OBJECT_OT_vertex_group_assign", IFACE_("Assign to Active Group"), ICON_NONE);
      PointerRNA props = layout->op(
          "OBJECT_OT_vertex_group_remove_from", IFACE_("Remove from Active Group"), ICON_NONE);
      if (props.data) {
        RNA_boolean_set(&props, "use_all_groups", false);
      }
      props = layout->op(
          "OBJECT_OT_vertex_group_remove_from", IFACE_("Remove from All"), ICON_NONE);
      if (props.data) {
        RNA_boolean_set(&props, "use_all_groups", true);
      }
    }
  }

  if (has_active) {
    layout->separator();

    uiItemMenuEnumO(layout,
                    C,
                    "OBJECT_OT_vertex_group_set_active",
                    "group",
                    IFACE_("Set Active Group"),
                    ICON_NONE);
    PointerRNA props = layout->op(
        "OBJECT_OT_vertex_group_remove", IFACE_("Remove Active Group"), ICON_NONE);
    if (props.data) {
      RNA_boolean_set(&props, "all", false);
    }
    props = layout->op("OBJECT_OT_vertex_group_remove", IFACE_("Remove All Groups"), ICON_NONE);
    if (props.data) {
      RNA_boolean_set(&props, "all", true);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_MT_pose_apply / VIEW3D_MT_pose_propagate
 * \{ */

static void pose_apply_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;

  PointerRNA props = layout->op("POSE_OT_armature_apply", std::nullopt, ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "selected", false);
  }
  props = layout->op("POSE_OT_armature_apply", IFACE_("Apply Selected as Rest Pose"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "selected", true);
  }
  layout->op("POSE_OT_visual_transform_apply", std::nullopt, ICON_NONE);

  layout->separator();

  props = layout->op("OBJECT_OT_assign_property_defaults", std::nullopt, ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "process_bones", true);
  }
}

static void pose_propagate_item(uiLayout *layout, const char *text, const char *mode)
{
  PointerRNA props = layout->op("POSE_OT_propagate", IFACE_(text), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "mode", mode);
  }
}

static void pose_propagate_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;

  pose_propagate_item(layout, N_("To Next Keyframe"), "NEXT_KEY");
  pose_propagate_item(layout, N_("To Last Keyframe (Make Cyclic)"), "LAST_KEY");

  layout->separator();

  pose_propagate_item(layout, N_("On Selected Keyframes"), "SELECTED_KEYS");

  layout->separator();

  pose_propagate_item(layout, N_("On Selected Markers"), "SELECTED_MARKERS");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_MT_uv_map
 * \{ */

static void uv_map_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  /* `layout.menu_contents("IMAGE_MT_uvs_unwrap")`: mete las entradas del otro
   * menu aqui dentro, sin boton propio. Sigue en Python; se busca por nombre. */
  uiItemMContents(layout, "IMAGE_MT_uvs_unwrap");

  layout->separator();

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);
  PointerRNA props = layout->op("UV_OT_project_from_view", std::nullopt, ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "scale_to_bounds", false);
  }
  props = layout->op("UV_OT_project_from_view", IFACE_("Project from View (Bounds)"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "scale_to_bounds", true);
  }

  layout->separator();

  props = layout->op("MESH_OT_mark_seam", std::nullopt, ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "clear", false);
  }
  props = layout->op("MESH_OT_mark_seam", IFACE_("Clear Seam"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "clear", true);
  }

  layout->separator();

  layout->op("UV_OT_reset", std::nullopt, ICON_NONE);

  geometry::ui_template_node_operator_asset_menu_items(
      *layout, *const_cast<bContext *>(C), "UV");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registro
 * \{ */


/* -------------------------------------------------------------------- */
/** \name VIEW3D_MT_object_context_menu
 *
 * El boton derecho en modo objeto: el menu mas usado de Blender y el que mas
 * ramas por tipo de objeto tiene. Casi todas ellas montan un
 * `wm.context_modal_mouse`, que es el operador de «arrastra el raton para
 * ajustar esto»; su rotulo de cabecera va por `RPT_()`, no por `IFACE_()`,
 * porque es un mensaje de informe con formato (`%.3f`), no una etiqueta.
 * \{ */

/** `wm.context_modal_mouse` con sus cuatro propiedades. */
static void modal_mouse_item(uiLayout *layout,
                             const char *text,
                             const char *data_path_item,
                             const char *header_text,
                             const float input_scale = 1.0f,
                             const bool set_input_scale = false)
{
  PointerRNA props = layout->op("WM_OT_context_modal_mouse", IFACE_(text), ICON_NONE);
  if (props.data == nullptr) {
    return;
  }
  RNA_string_set(&props, "data_path_iter", "selected_editable_objects");
  RNA_string_set(&props, "data_path_item", data_path_item);
  if (set_input_scale) {
    RNA_float_set(&props, "input_scale", input_scale);
  }
  RNA_string_set(&props, "header_text", RPT_(header_text));
}

static void object_context_menu_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  const View3D *view = CTX_wm_view3d(C);
  const Object *obj = CTX_data_active_object(C);
  const int selected_objects_len = CTX_data_collection_get(C, "selected_objects").size();

  /* La rama de «nada seleccionado» esta comentada en el Python; no se escribe. */

  if (obj == nullptr) {
    /* `pass` del Python. */
  }
  else if (obj->type == OB_CAMERA) {
    const Camera *cam = static_cast<const Camera *>(obj->data);
    uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);

    layout->op("VIEW3D_OT_object_as_camera", IFACE_("Set Active Camera"), ICON_NONE);

    if (cam->type == CAM_PERSP) {
      modal_mouse_item(layout,
                       N_("Adjust Focal Length"),
                       "data.lens",
                       /* `lens_unit` es un bitflag sobre `flag`: `MILLIMETERS` es 0 y `FOV` es
                        * `CAM_ANGLETOGGLE` (`rna_camera.cc:659-665`). */
                       ((cam->flag & CAM_ANGLETOGGLE) == 0) ?
                           N_("Camera Focal Length: %.1fmm") :
                           N_("Camera Focal Length: %.1f°"),
                       0.1f,
                       true);
    }
    else {
      modal_mouse_item(layout,
                       N_("Camera Lens Scale"),
                       "data.ortho_scale",
                       N_("Camera Lens Scale: %.3f"),
                       0.01f,
                       true);
    }

    if (cam->dof.focus_object == nullptr) {
      const RegionView3D *rv3d = CTX_wm_region_view3d(C);
      if (view != nullptr && view->camera == obj && rv3d != nullptr && rv3d->persp == RV3D_CAMOB) {
        layout->op("UI_OT_eyedropper_depth", IFACE_("DOF Distance (Pick)"), ICON_NONE);
      }
      else {
        modal_mouse_item(layout,
                         N_("Adjust Focus Distance"),
                         "data.dof.focus_distance",
                         N_("Focus Distance: %.3f"),
                         0.02f,
                         true);
      }
    }

    layout->separator();
  }
  else if (ELEM(obj->type, OB_CURVES_LEGACY, OB_FONT)) {
    uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);

    modal_mouse_item(
        layout, N_("Adjust Extrusion"), "data.extrude", N_("Extrude: %.3f"), 0.01f, true);
    modal_mouse_item(layout, N_("Adjust Offset"), "data.offset", N_("Offset: %.3f"), 0.01f, true);

    layout->separator();
  }
  else if (obj->type == OB_EMPTY) {
    uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);

    modal_mouse_item(layout,
                     N_("Adjust Empty Display Size"),
                     "empty_display_size",
                     N_("Empty Display Size: %.3f"),
                     0.01f,
                     true);

    layout->separator();

    if (obj->empty_drawtype == OB_EMPTY_IMAGE) {
      layout->op("IMAGE_OT_convert_to_mesh_plane", IFACE_("Convert to Mesh Plane"), ICON_NONE);
      layout->op("GREASE_PENCIL_OT_trace_image", std::nullopt, ICON_NONE);

      layout->separator();
    }
  }
  else if (obj->type == OB_LAMP) {
    const Light *light = static_cast<const Light *>(obj->data);

    uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);

    modal_mouse_item(
        layout, N_("Adjust Light Power"), "data.energy", N_("Light Power: %.3f"), 1.0f, true);

    if (light->type == LA_AREA) {
      if (ELEM(light->area_shape, LA_AREA_RECT, LA_AREA_ELLIPSE)) {
        modal_mouse_item(
            layout, N_("Adjust Area Light X Size"), "data.size", N_("Light Size X: %.3f"));
        modal_mouse_item(
            layout, N_("Adjust Area Light Y Size"), "data.size_y", N_("Light Size Y: %.3f"));
      }
      else {
        modal_mouse_item(
            layout, N_("Adjust Area Light Size"), "data.size", N_("Light Size: %.3f"));
      }
    }
    else if (ELEM(light->type, LA_SPOT, LA_LOCAL)) {
      modal_mouse_item(
          layout, N_("Adjust Light Radius"), "data.shadow_soft_size", N_("Light Radius: %.3f"));
    }
    else if (light->type == LA_SUN) {
      modal_mouse_item(
          layout, N_("Adjust Sun Light Angle"), "data.angle", N_("Light Angle: %.3f"));
    }

    if (light->type == LA_SPOT) {
      layout->separator();

      modal_mouse_item(layout,
                       N_("Adjust Spot Light Size"),
                       "data.spot_size",
                       N_("Spot Size: %.2f"),
                       0.01f,
                       true);
      modal_mouse_item(layout,
                       N_("Adjust Spot Light Blend"),
                       "data.spot_blend",
                       N_("Spot Blend: %.2f"),
                       -0.01f,
                       true);
    }

    layout->separator();
  }

  /* Comun a varios tipos. */
  if (obj != nullptr) {
    if (ELEM(obj->type, OB_MESH, OB_CURVES_LEGACY, OB_SURF)) {
      layout->op("OBJECT_OT_shade_smooth", std::nullopt, ICON_NONE);
      if (obj->type == OB_MESH) {
        layout->op("OBJECT_OT_shade_auto_smooth", std::nullopt, ICON_NONE);
      }
      layout->op("OBJECT_OT_shade_flat", std::nullopt, ICON_NONE);

      layout->separator();
    }

    if (ELEM(obj->type, OB_MESH, OB_CURVES_LEGACY, OB_SURF, OB_ARMATURE, OB_GREASE_PENCIL)) {
      if (selected_objects_len > 1) {
        layout->op("OBJECT_OT_join", std::nullopt, ICON_NONE);
      }
    }

    if (ELEM(obj->type,
             OB_MESH,
             OB_CURVES_LEGACY,
             OB_CURVES,
             OB_SURF,
             OB_POINTCLOUD,
             OB_MBALL,
             OB_FONT,
             OB_GREASE_PENCIL))
    {
      menu_enum_o(layout, C, "OBJECT_OT_convert", "target");
    }

    if (ELEM(obj->type,
             OB_MESH,
             OB_CURVES_LEGACY,
             OB_CURVES,
             OB_SURF,
             OB_GREASE_PENCIL,
             OB_LATTICE,
             OB_ARMATURE,
             OB_MBALL,
             OB_FONT,
             OB_POINTCLOUD) ||
        (obj->type == OB_EMPTY && obj->instance_collection != nullptr))
    {
      uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);
      uiItemMenuEnumO(layout, C, "OBJECT_OT_origin_set", "type", IFACE_("Set Origin"), ICON_NONE);
      uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);

      layout->separator();
    }
  }

  /* Comun a todos. */
  layout->op("VIEW3D_OT_copybuffer", IFACE_("Copy Objects"), ICON_COPYDOWN);
  layout->op("VIEW3D_OT_pastebuffer", IFACE_("Paste Objects"), ICON_PASTEDOWN);

  layout->separator();

  layout->op("OBJECT_OT_duplicate_move", std::nullopt, ICON_DUPLICATE);
  layout->op("OBJECT_OT_duplicate_move_linked", std::nullopt, ICON_NONE);

  layout->separator();

  PointerRNA props = layout->op("WM_OT_call_panel", IFACE_("Rename Active Object..."), ICON_NONE);
  if (props.data) {
    RNA_string_set(&props, "name", "TOPBAR_PT_name");
    RNA_boolean_set(&props, "keep_open", false);
  }

  layout->separator();

  layout->menu("VIEW3D_MT_mirror", std::nullopt, ICON_NONE);
  layout->menu("VIEW3D_MT_snap", std::nullopt, ICON_NONE);
  layout->menu("VIEW3D_MT_object_parent", std::nullopt, ICON_NONE);
  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);

  if (view != nullptr && view->localvd != nullptr) {
    layout->op("VIEW3D_OT_localview_remove_from", std::nullopt, ICON_NONE);
  }
  else {
    layout->op("OBJECT_OT_move_to_collection", std::nullopt, ICON_NONE);
  }

  layout->separator();

  layout->op("ANIM_OT_keyframe_insert", IFACE_("Insert Keyframe"), ICON_NONE);
  props = layout->op(
      "ANIM_OT_keyframe_insert_menu", IFACE_("Insert Keyframe with Keying Set"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "always_prompt", true);
  }

  layout->separator();

  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_REGION_WIN);
  props = layout->op("OBJECT_OT_delete", IFACE_("Delete"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "use_global", false);
  }

  geometry::ui_template_node_operator_asset_menu_items(
      *layout, *const_cast<bContext *>(C), "Object");
}

/** \} */

static const flipendo::MenuDecl view3d_object_menus[] = {
    {
        /*idname*/ "VIEW3D_MT_object_context_menu",
        /*label*/ N_("Object"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ object_context_menu_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_object_apply",
        /*label*/ N_("Apply"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ object_apply_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_make_single_user",
        /*label*/ N_("Make Single User"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ make_single_user_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_make_links",
        /*label*/ N_("Link/Transfer Data"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ make_links_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_hook",
        /*label*/ N_("Hooks"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ hook_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_vertex_group",
        /*label*/ N_("Vertex Groups"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ vertex_group_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_pose_apply",
        /*label*/ N_("Apply"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ pose_apply_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_pose_propagate",
        /*label*/ N_("Propagate"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ pose_propagate_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_uv_map",
        /*label*/ N_("UV Mapping"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ uv_map_draw,
    },
};

void view3d_object_menus_register()
{
  flipendo::menus_register({view3d_object_menus, ARRAY_SIZE(view3d_object_menus)});
}

/** \} */

}  // namespace blender::ed::view3d
