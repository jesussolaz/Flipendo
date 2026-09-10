/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 *
 * Ver FL_view3d_menus.hh.
 *
 * Familia 1: los menus radiales de la vista 3D y su gemelo no radial
 * `VIEW3D_MT_snap`. Son los que cuelgan de las teclas de uso diario
 * (Ctrl-Tab, `, Z, ., coma, Shift-S, Shift-O, A, K...) y los que menos
 * dependen de nada: tablas planas de operadores y de propiedades RNA.
 *
 * Cada `draw` reproduce linea a linea el `draw()` de la clase de Python que
 * sustituye, en el mismo orden. Las equivalencias, de una vez por todas:
 *
 * | Python                                   | C++                                        |
 * |------------------------------------------|--------------------------------------------|
 * | `layout.menu_pie()`                      | `layout->menu_pie()`                        |
 * | `pie.operator_enum(op, prop)`            | `uiItemsEnumO(pie, OP, prop)`               |
 * | `pie.operator(op, text=, icon=)`         | `pie->op(OP, IFACE_(text), ICON_x)`         |
 * | `pie.prop(ptr, prop, text=, icon=)`      | `pie->prop(&ptr, prop, flag, texto, icono)` |
 * | `pie.prop(ptr, prop, expand=True)`       | `... UI_ITEM_R_EXPAND ...`                  |
 * | `pie.prop_enum(ptr, prop, value=)`       | `uiItemEnumR_string(pie, &ptr, prop, val)`  |
 *
 * Trampa de la traduccion: `rna_uiItemR` traduce el texto con el contexto por
 * defecto (el bloque que usaria el de la propiedad esta en un `#if 0` desde
 * hace anos, `rna_ui_api.cc:74`), y `rna_uiItemO` con el de la `srna` del
 * operador, que es `BLT_I18NCONTEXT_DEFAULT_BPYRNA` ("*") y que
 * `BLT_is_default_context()` normaliza al contexto por defecto. Es decir:
 * `IFACE_()` es el equivalente exacto en los dos casos.
 */

#include <initializer_list>

#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "FL_ui_registry.hh"

#include "FL_view3d_menus.hh"

namespace blender::ed::view3d {

/* -------------------------------------------------------------------- */
/** \name Fuentes de datos
 *
 * Cada una reproduce el descriptor de `bpy.context` que usaba el Python, con
 * el mismo `owner_id` — el volcado de diseno serializa `Struct.prop[indice]`,
 * asi que el tipo RNA tiene que ser el mismo, no solo el dato.
 * \{ */

/** `context.space_data` ya refinado (`rna_Context_space_data_get`). */
static PointerRNA space_view3d_ptr(const bContext *C)
{
  View3D *v3d = CTX_wm_view3d(C);
  if (v3d == nullptr) {
    return PointerRNA_NULL;
  }
  return RNA_pointer_create_discrete(
      reinterpret_cast<ID *>(CTX_wm_screen(C)), &RNA_SpaceView3D, v3d);
}

/** `context.tool_settings` (`rna_Context_tool_settings_get`). */
static PointerRNA tool_settings_ptr(const bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  if (scene == nullptr || ts == nullptr) {
    return PointerRNA_NULL;
  }
  return RNA_pointer_create_id_subdata(scene->id, &RNA_ToolSettings, ts);
}

/** `context.scene`. */
static PointerRNA scene_ptr(const bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  return scene ? RNA_id_pointer_create(&scene->id) : PointerRNA_NULL;
}

/** `context.pose_object` (`screen_ctx_pose_object`), por el mismo camino. */
static Object *pose_object(const bContext *C)
{
  PointerRNA ptr = CTX_data_pointer_get_type(C, "pose_object", &RNA_Object);
  return static_cast<Object *>(ptr.data);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_MT_object_mode_pie
 * \{ */

static void object_mode_pie_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();
  uiItemsEnumO(&pie, "OBJECT_OT_mode_set", "mode");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_MT_view_pie
 * \{ */

static void view_pie_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  uiItemsEnumO(&pie, "VIEW3D_OT_view_axis", "type");
  pie.op("VIEW3D_OT_view_camera", IFACE_("View Camera"), ICON_CAMERA_DATA);
  pie.op("VIEW3D_OT_view_selected", IFACE_("View Selected"), ICON_ZOOM_SELECTED);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_MT_transform_gizmo_pie
 * \{ */

/**
 * `pie.operator("view3d.transform_gizmo_set", text=...).type = {...}`.
 *
 * `type` es un `EnumProperty` con `ENUM_FLAG`, asi que el valor es la union de
 * bits de los identificadores. Se resuelven por RNA en vez de escribir 1/2/4 a
 * mano: el operador todavia es Python (`bl_operators/view3d.py`) y el dia que
 * lo migre otro carril los valores no tienen por que coincidir con el orden.
 */
static void gizmo_set_item(uiLayout &pie,
                           const char *text,
                           std::initializer_list<const char *> type_ids)
{
  PointerRNA op_ptr = pie.op("VIEW3D_OT_transform_gizmo_set", IFACE_(text), ICON_NONE);
  if (op_ptr.data == nullptr) {
    return;
  }
  PropertyRNA *prop = RNA_struct_find_property(&op_ptr, "type");
  if (prop == nullptr) {
    return;
  }
  int value = 0;
  for (const char *id : type_ids) {
    int item_value = 0;
    if (RNA_property_enum_value(nullptr, &op_ptr, prop, id, &item_value)) {
      value |= item_value;
    }
  }
  RNA_property_enum_set(&op_ptr, prop, value);
}

static void transform_gizmo_pie_draw(const bContext *C, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  /* 1: izquierda. */
  gizmo_set_item(pie, N_("Move"), {"TRANSLATE"});
  /* 2: derecha. */
  gizmo_set_item(pie, N_("Rotate"), {"ROTATE"});
  /* 3: abajo. */
  gizmo_set_item(pie, N_("Scale"), {"SCALE"});
  /* 4: arriba. */
  PointerRNA space_ptr = space_view3d_ptr(C);
  if (space_ptr.data) {
    pie.prop(&space_ptr, "show_gizmo", UI_ITEM_NONE, IFACE_("Show Gizmos"), ICON_GIZMO);
  }
  /* 5: arriba/izquierda. */
  gizmo_set_item(pie, N_("All"), {"TRANSLATE", "ROTATE", "SCALE"});
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_MT_shading_pie / VIEW3D_MT_shading_ex_pie
 * \{ */

static void shading_pie_draw(const bContext *C, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  PointerRNA view = space_view3d_ptr(C);
  if (view.data == nullptr) {
    return;
  }
  PointerRNA shading = RNA_pointer_get(&view, "shading");

  pie.prop(&shading, "type", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
}

static void shading_ex_pie_draw(const bContext *C, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  PointerRNA view = space_view3d_ptr(C);
  if (view.data == nullptr) {
    return;
  }
  PointerRNA shading = RNA_pointer_get(&view, "shading");
  PointerRNA overlay = RNA_pointer_get(&view, "overlay");

  uiItemEnumR_string(&pie, &shading, "type", "WIREFRAME", std::nullopt, ICON_NONE);
  uiItemEnumR_string(&pie, &shading, "type", "SOLID", std::nullopt, ICON_NONE);

  /* Duplica a proposito la logica de `view3d.toggle_xray` para poder marcar el
   * elemento activo; ver el comentario del Python y #58661. */
  if (pose_object(C) != nullptr) {
    pie.prop(&overlay, "show_xray_bone", UI_ITEM_NONE, std::nullopt, ICON_XRAY);
  }
  else {
    const View3D *v3d = static_cast<const View3D *>(view.data);
    const bool xray_active = (CTX_data_mode_enum(C) == CTX_MODE_EDIT_MESH) ||
                             ELEM(v3d->shading.type, OB_SOLID, OB_WIRE);
    uiLayout *sub = &pie;
    if (!xray_active) {
      sub = &pie.row(false);
      uiLayoutSetActive(sub, false);
    }
    sub->prop(&shading,
              (v3d->shading.type == OB_WIRE) ? "show_xray_wireframe" : "show_xray",
              UI_ITEM_NONE,
              IFACE_("Toggle X-Ray"),
              ICON_XRAY);
  }

  pie.prop(&overlay, "show_overlays", UI_ITEM_NONE, IFACE_("Toggle Overlays"), ICON_OVERLAY);

  uiItemEnumR_string(&pie, &shading, "type", "MATERIAL", std::nullopt, ICON_NONE);
  uiItemEnumR_string(&pie, &shading, "type", "RENDERED", std::nullopt, ICON_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_MT_pivot_pie
 * \{ */

static void pivot_pie_draw(const bContext *C, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  PointerRNA ts = tool_settings_ptr(C);
  if (ts.data == nullptr) {
    return;
  }
  const Object *obj = CTX_data_active_object(C);
  const eContextObjectMode mode = CTX_data_mode_enum(C);

  uiItemEnumR_string(&pie, &ts, "transform_pivot_point", "BOUNDING_BOX_CENTER", std::nullopt, ICON_NONE);
  uiItemEnumR_string(&pie, &ts, "transform_pivot_point", "CURSOR", std::nullopt, ICON_NONE);
  uiItemEnumR_string(&pie, &ts, "transform_pivot_point", "INDIVIDUAL_ORIGINS", std::nullopt, ICON_NONE);
  uiItemEnumR_string(&pie, &ts, "transform_pivot_point", "MEDIAN_POINT", std::nullopt, ICON_NONE);
  uiItemEnumR_string(&pie, &ts, "transform_pivot_point", "ACTIVE_ELEMENT", std::nullopt, ICON_NONE);

  if ((obj == nullptr) || ELEM(mode, CTX_MODE_OBJECT, CTX_MODE_POSE, CTX_MODE_PAINT_WEIGHT)) {
    pie.prop(&ts, "use_transform_pivot_point_align", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  if (ELEM(mode, CTX_MODE_EDIT_GPENCIL_LEGACY, CTX_MODE_EDIT_GREASE_PENCIL)) {
    PointerRNA gpencil_sculpt = RNA_pointer_get(&ts, "gpencil_sculpt");
    pie.prop(&gpencil_sculpt, "use_scale_thickness", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_MT_orientations_pie
 * \{ */

static void orientations_pie_draw(const bContext *C, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  Scene *scene = CTX_data_scene(C);
  if (scene == nullptr) {
    return;
  }
  /* `scene.transform_orientation_slots[0]`. */
  PointerRNA slot = RNA_pointer_create_discrete(
      &scene->id, &RNA_TransformOrientationSlot, &scene->orientation_slots[0]);

  pie.prop(&slot, "type", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_MT_snap_pie / VIEW3D_MT_snap
 * \{ */

static void snap_pie_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  pie.op("VIEW3D_OT_snap_cursor_to_grid", IFACE_("Cursor to Grid"), ICON_CURSOR);
  pie.op("VIEW3D_OT_snap_selected_to_grid",
         IFACE_("Selection to Grid"),
         ICON_RESTRICT_SELECT_OFF);
  pie.op("VIEW3D_OT_snap_cursor_to_selected", IFACE_("Cursor to Selected"), ICON_CURSOR);
  {
    PointerRNA op_ptr = pie.op(
        "VIEW3D_OT_snap_selected_to_cursor", IFACE_("Selection to Cursor"), ICON_RESTRICT_SELECT_OFF);
    if (op_ptr.data) {
      RNA_boolean_set(&op_ptr, "use_offset", false);
    }
  }
  {
    PointerRNA op_ptr = pie.op("VIEW3D_OT_snap_selected_to_cursor",
                               IFACE_("Selection to Cursor (Keep Offset)"),
                               ICON_RESTRICT_SELECT_OFF);
    if (op_ptr.data) {
      RNA_boolean_set(&op_ptr, "use_offset", true);
    }
  }
  pie.op("VIEW3D_OT_snap_selected_to_active",
         IFACE_("Selection to Active"),
         ICON_RESTRICT_SELECT_OFF);
  pie.op("VIEW3D_OT_snap_cursor_to_center", IFACE_("Cursor to World Origin"), ICON_CURSOR);
  pie.op("VIEW3D_OT_snap_cursor_to_active", IFACE_("Cursor to Active"), ICON_CURSOR);
}

static void snap_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;

  layout->op("VIEW3D_OT_snap_selected_to_grid", IFACE_("Selection to Grid"), ICON_NONE);
  {
    PointerRNA op_ptr = layout->op(
        "VIEW3D_OT_snap_selected_to_cursor", IFACE_("Selection to Cursor"), ICON_NONE);
    if (op_ptr.data) {
      RNA_boolean_set(&op_ptr, "use_offset", false);
    }
  }
  {
    PointerRNA op_ptr = layout->op(
        "VIEW3D_OT_snap_selected_to_cursor", IFACE_("Selection to Cursor (Keep Offset)"), ICON_NONE);
    if (op_ptr.data) {
      RNA_boolean_set(&op_ptr, "use_offset", true);
    }
  }
  layout->op("VIEW3D_OT_snap_selected_to_active", IFACE_("Selection to Active"), ICON_NONE);

  layout->separator();

  layout->op("VIEW3D_OT_snap_cursor_to_selected", IFACE_("Cursor to Selected"), ICON_NONE);
  layout->op("VIEW3D_OT_snap_cursor_to_center", IFACE_("Cursor to World Origin"), ICON_NONE);
  layout->op("VIEW3D_OT_snap_cursor_to_grid", IFACE_("Cursor to Grid"), ICON_NONE);
  layout->op("VIEW3D_OT_snap_cursor_to_active", IFACE_("Cursor to Active"), ICON_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_MT_proportional_editing_falloff_pie
 * \{ */

static void proportional_editing_falloff_pie_draw(const bContext *C, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  /* El Python lo saca de `context.scene.tool_settings`, que es el mismo dato y
   * el mismo `owner_id` que `context.tool_settings`. */
  PointerRNA ts = tool_settings_ptr(C);
  if (ts.data == nullptr) {
    return;
  }
  pie.prop(&ts, "proportional_edit_falloff", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_MT_sculpt_mask_edit_pie
 * \{ */

static void sculpt_mask_edit_pie_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();
  PointerRNA props;

  props = pie.op("PAINT_OT_mask_flood_fill", IFACE_("Invert Mask"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "mode", "INVERT");
  }
  props = pie.op("PAINT_OT_mask_flood_fill", IFACE_("Clear Mask"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "mode", "VALUE");
    RNA_float_set(&props, "value", 0.0f);
  }
  props = pie.op("SCULPT_OT_mask_filter", IFACE_("Smooth Mask"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "filter_type", "SMOOTH");
  }
  props = pie.op("SCULPT_OT_mask_filter", IFACE_("Sharpen Mask"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "filter_type", "SHARPEN");
  }
  props = pie.op("SCULPT_OT_mask_filter", IFACE_("Grow Mask"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "filter_type", "GROW");
  }
  props = pie.op("SCULPT_OT_mask_filter", IFACE_("Shrink Mask"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "filter_type", "SHRINK");
  }
  props = pie.op("SCULPT_OT_mask_filter", IFACE_("Increase Contrast"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "filter_type", "CONTRAST_INCREASE");
    RNA_boolean_set(&props, "auto_iteration_count", false);
  }
  props = pie.op("SCULPT_OT_mask_filter", IFACE_("Decrease Contrast"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "filter_type", "CONTRAST_DECREASE");
    RNA_boolean_set(&props, "auto_iteration_count", false);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_MT_sculpt_automasking_pie
 * \{ */

static void sculpt_automasking_pie_draw(const bContext *C, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  PointerRNA ts = tool_settings_ptr(C);
  if (ts.data == nullptr) {
    return;
  }
  PointerRNA sculpt = RNA_pointer_get(&ts, "sculpt");

  pie.prop(&sculpt, "use_automasking_topology", UI_ITEM_NONE, IFACE_("Topology"), ICON_NONE);
  pie.prop(&sculpt, "use_automasking_face_sets", UI_ITEM_NONE, IFACE_("Face Sets"), ICON_NONE);
  pie.prop(
      &sculpt, "use_automasking_boundary_edges", UI_ITEM_NONE, IFACE_("Mesh Boundary"), ICON_NONE);
  pie.prop(&sculpt,
           "use_automasking_boundary_face_sets",
           UI_ITEM_NONE,
           IFACE_("Face Sets Boundary"),
           ICON_NONE);
  pie.prop(&sculpt, "use_automasking_cavity", UI_ITEM_NONE, IFACE_("Cavity"), ICON_NONE);
  pie.prop(&sculpt,
           "use_automasking_cavity_inverted",
           UI_ITEM_NONE,
           IFACE_("Cavity (Inverted)"),
           ICON_NONE);
  pie.prop(&sculpt, "use_automasking_start_normal", UI_ITEM_NONE, IFACE_("Area Normal"), ICON_NONE);
  pie.prop(&sculpt, "use_automasking_view_normal", UI_ITEM_NONE, IFACE_("View Normal"), ICON_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_MT_grease_pencil_sculpt_automasking_pie
 * \{ */

static void grease_pencil_sculpt_automasking_pie_draw(const bContext *C, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  PointerRNA ts = tool_settings_ptr(C);
  if (ts.data == nullptr) {
    return;
  }
  PointerRNA sculpt = RNA_pointer_get(&ts, "gpencil_sculpt");

  pie.prop(&sculpt, "use_automasking_stroke", UI_ITEM_NONE, IFACE_("Stroke"), ICON_NONE);
  pie.prop(&sculpt, "use_automasking_layer_stroke", UI_ITEM_NONE, IFACE_("Layer"), ICON_NONE);
  pie.prop(&sculpt, "use_automasking_material_stroke", UI_ITEM_NONE, IFACE_("Material"), ICON_NONE);
  pie.prop(&sculpt, "use_automasking_layer_active", UI_ITEM_NONE, IFACE_("Active Layer"), ICON_NONE);
  pie.prop(
      &sculpt, "use_automasking_material_active", UI_ITEM_NONE, IFACE_("Active Material"), ICON_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_MT_sculpt_face_sets_edit_pie
 * \{ */

static void sculpt_face_sets_edit_pie_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();
  PointerRNA props;

  props = pie.op("SCULPT_OT_face_sets_create", IFACE_("Face Set from Masked"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "mode", "MASKED");
  }

  props = pie.op("SCULPT_OT_face_sets_create", IFACE_("Face Set from Visible"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "mode", "VISIBLE");
  }

  pie.op("PAINT_OT_visibility_invert", IFACE_("Invert Visible"), ICON_NONE);

  props = pie.op("PAINT_OT_hide_show_all", IFACE_("Show All"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "action", "SHOW");
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_MT_wpaint_vgroup_lock_pie
 * \{ */

static void vgroup_lock_item(
    uiLayout &pie, const char *text, int icon, const char *action, const char *mask)
{
  PointerRNA props = pie.op("OBJECT_OT_vertex_group_lock", IFACE_(text), icon);
  if (props.data == nullptr) {
    return;
  }
  RNA_enum_set_identifier(nullptr, &props, "action", action);
  RNA_enum_set_identifier(nullptr, &props, "mask", mask);
}

static void wpaint_vgroup_lock_pie_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  /* 1: izquierda. */
  vgroup_lock_item(pie, N_("Lock All"), ICON_LOCKED, "LOCK", "ALL");
  /* 2: derecha. */
  vgroup_lock_item(pie, N_("Unlock All"), ICON_UNLOCKED, "UNLOCK", "ALL");
  /* 3: abajo. */
  vgroup_lock_item(pie, N_("Unlock Selected"), ICON_UNLOCKED, "UNLOCK", "SELECTED");
  /* 4: arriba. */
  vgroup_lock_item(pie, N_("Lock Selected"), ICON_LOCKED, "LOCK", "SELECTED");
  /* 5: arriba/izquierda. */
  vgroup_lock_item(pie, N_("Lock Unselected"), ICON_LOCKED, "LOCK", "UNSELECTED");
  /* 6: arriba/derecha. */
  vgroup_lock_item(pie, N_("Lock Only Selected"), ICON_NONE, "LOCK", "INVERT_UNSELECTED");
  /* 7: abajo/izquierda. */
  vgroup_lock_item(pie, N_("Lock Only Unselected"), ICON_NONE, "UNLOCK", "INVERT_UNSELECTED");
  /* 8: abajo/derecha. */
  vgroup_lock_item(pie, N_("Invert Locks"), ICON_NONE, "INVERT", "ALL");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registro
 * \{ */

static const flipendo::MenuDecl view3d_menus[] = {
    {
        /*idname*/ "VIEW3D_MT_object_mode_pie",
        /*label*/ N_("Mode"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ object_mode_pie_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_view_pie",
        /*label*/ N_("View"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ view_pie_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_transform_gizmo_pie",
        /*label*/ N_("View"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ transform_gizmo_pie_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_shading_pie",
        /*label*/ N_("Shading"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ shading_pie_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_shading_ex_pie",
        /*label*/ N_("Shading"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ shading_ex_pie_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_pivot_pie",
        /*label*/ N_("Pivot Point"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ pivot_pie_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_orientations_pie",
        /*label*/ N_("Orientation"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ orientations_pie_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_snap_pie",
        /*label*/ N_("Snap"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ snap_pie_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_snap",
        /*label*/ N_("Snap"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ snap_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_proportional_editing_falloff_pie",
        /*label*/ N_("Proportional Editing Falloff"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ proportional_editing_falloff_pie_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_sculpt_mask_edit_pie",
        /*label*/ N_("Mask Edit"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ sculpt_mask_edit_pie_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_sculpt_automasking_pie",
        /*label*/ N_("Automasking"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ sculpt_automasking_pie_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_grease_pencil_sculpt_automasking_pie",
        /*label*/ N_("Automasking"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ grease_pencil_sculpt_automasking_pie_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_sculpt_face_sets_edit_pie",
        /*label*/ N_("Face Sets Edit"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ sculpt_face_sets_edit_pie_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_wpaint_vgroup_lock_pie",
        /*label*/ N_("Vertex Group Locks"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ wpaint_vgroup_lock_pie_draw,
    },
};

void view3d_menus_register()
{
  flipendo::menus_register({view3d_menus, ARRAY_SIZE(view3d_menus)});
}

/** \} */

}  // namespace blender::ed::view3d
