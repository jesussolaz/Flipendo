/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spimage
 *
 * Ver FL_image_menus.hh. Los once menus que el keymap nativo abre por nombre en
 * el editor de imagen y UV: el contextual de UV y el de mascara, los dos
 * radiales, el modo de seleccion, y los de ajustar, alinear, fusionar, separar
 * y desplegar.
 */

#include <optional>

#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "FL_ui_registry.hh"

#include "FL_image_menus.hh"

namespace blender::ed::image {

/** `context.space_data` del editor de imagen, ya refinado. */
static PointerRNA space_image_ptr(const bContext *C)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  if (sima == nullptr) {
    return PointerRNA_NULL;
  }
  return RNA_pointer_create_discrete(
      reinterpret_cast<ID *>(CTX_wm_screen(C)), &RNA_SpaceImageEditor, sima);
}

static void op_enum_item(
    uiLayout *layout, const char *opname, const char *text, const char *prop, const char *value)
{
  PointerRNA props = layout->op(
      opname, text ? IFACE_(text) : std::optional<StringRef>(std::nullopt), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, prop, value);
  }
}

static void op_enum_item_icon(uiLayout *layout,
                              const char *opname,
                              const char *text,
                              int icon,
                              const char *prop,
                              const char *value)
{
  PointerRNA props = layout->op(opname, IFACE_(text), icon);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, prop, value);
  }
}

/** `layout.operator("transform.mirror", text=t).constraint_axis[i] = True`. */
static void mirror_axis_item(uiLayout *layout, const char *text, int axis)
{
  PointerRNA props = layout->op("TRANSFORM_OT_mirror", IFACE_(text), ICON_NONE);
  if (props.data == nullptr) {
    return;
  }
  PropertyRNA *prop = RNA_struct_find_property(&props, "constraint_axis");
  if (prop) {
    RNA_property_boolean_set_index(&props, prop, axis, true);
  }
}

/* -------------------------------------------------------------------- */
/** \name Ajustar, alinear, fusionar, separar, desplegar
 * \{ */

static void uvs_snap_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;

  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_REGION_WIN);

  op_enum_item(layout, "UV_OT_snap_selected", N_("Selected to Pixels"), "target", "PIXELS");
  op_enum_item(layout, "UV_OT_snap_selected", N_("Selected to Cursor"), "target", "CURSOR");
  op_enum_item(
      layout, "UV_OT_snap_selected", N_("Selected to Cursor (Offset)"), "target", "CURSOR_OFFSET");
  op_enum_item(layout,
               "UV_OT_snap_selected",
               N_("Selected to Adjacent Unselected"),
               "target",
               "ADJACENT_UNSELECTED");

  layout->separator();

  op_enum_item(layout, "UV_OT_snap_cursor", N_("Cursor to Pixels"), "target", "PIXELS");
  op_enum_item(layout, "UV_OT_snap_cursor", N_("Cursor to Selected"), "target", "SELECTED");
  op_enum_item(layout, "UV_OT_snap_cursor", N_("Cursor to Origin"), "target", "ORIGIN");
}

static void uvs_align_draw(const bContext * /*C*/, Menu *menu)
{
  uiItemsEnumO(menu->layout, "UV_OT_align", "axis");
}

static void uvs_merge_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;

  layout->op("UV_OT_weld", IFACE_("At Center"), ICON_NONE);
  /* Sobre todo para que case con el menu de la malla. */
  op_enum_item(layout, "UV_OT_snap_selected", N_("At Cursor"), "target", "CURSOR");

  layout->separator();

  layout->op("UV_OT_remove_doubles", IFACE_("By Distance"), ICON_NONE);
}

static void uvs_split_draw(const bContext * /*C*/, Menu *menu)
{
  menu->layout->op("UV_OT_select_split", IFACE_("Selection"), ICON_NONE);
}

static void uvs_unwrap_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;

  /* Estaria bien poder poner `uiItemsEnumO(layout, "UV_OT_unwrap", "method")`,
   * pero los items de la enumeracion no llevan el prefijo «Unwrap», asi que el
   * Python los pone a mano y aqui tambien. */
  op_enum_item(layout, "UV_OT_unwrap", N_("Unwrap Angle Based"), "method", "ANGLE_BASED");
  op_enum_item(layout, "UV_OT_unwrap", N_("Unwrap Conformal"), "method", "CONFORMAL");
  op_enum_item(layout, "UV_OT_unwrap", N_("Unwrap Minimum Stretch"), "method", "MINIMUM_STRETCH");

  layout->separator();

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);
  layout->op("UV_OT_smart_project", std::nullopt, ICON_NONE);
  layout->op("UV_OT_lightmap_pack", std::nullopt, ICON_NONE);
  layout->op("UV_OT_follow_active_quads", std::nullopt, ICON_NONE);

  layout->separator();

  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_REGION_WIN);
  layout->op("UV_OT_cube_project", std::nullopt, ICON_NONE);
  layout->op("UV_OT_cylinder_project", std::nullopt, ICON_NONE);
  layout->op("UV_OT_sphere_project", std::nullopt, ICON_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Modo de seleccion
 * \{ */

static void context_set_item(uiLayout *layout,
                             const char *opname,
                             const char *text,
                             int icon,
                             const char *value,
                             const char *data_path)
{
  PointerRNA props = layout->op(opname, IFACE_(text), icon);
  if (props.data == nullptr) {
    return;
  }
  RNA_string_set(&props, "value", value);
  RNA_string_set(&props, "data_path", data_path);
}

static void uvs_select_mode_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);
  const ToolSettings *ts = CTX_data_tool_settings(C);
  if (ts == nullptr) {
    return;
  }

  /* Segun este o no la seleccion sincronizada, se toca `mesh_select_mode` o
   * `uv_select_mode`; son dos operadores distintos y dos juegos de iconos. */
  if (ts->uv_flag & UV_SYNC_SELECTION) {
    context_set_item(layout,
                     "WM_OT_context_set_value",
                     N_("Vertex"),
                     ICON_VERTEXSEL,
                     "(True, False, False)",
                     "tool_settings.mesh_select_mode");
    context_set_item(layout,
                     "WM_OT_context_set_value",
                     N_("Edge"),
                     ICON_EDGESEL,
                     "(False, True, False)",
                     "tool_settings.mesh_select_mode");
    context_set_item(layout,
                     "WM_OT_context_set_value",
                     N_("Face"),
                     ICON_FACESEL,
                     "(False, False, True)",
                     "tool_settings.mesh_select_mode");
  }
  else {
    context_set_item(layout,
                     "WM_OT_context_set_string",
                     N_("Vertex"),
                     ICON_UV_VERTEXSEL,
                     "VERTEX",
                     "tool_settings.uv_select_mode");
    context_set_item(layout,
                     "WM_OT_context_set_string",
                     N_("Edge"),
                     ICON_UV_EDGESEL,
                     "EDGE",
                     "tool_settings.uv_select_mode");
    context_set_item(layout,
                     "WM_OT_context_set_string",
                     N_("Face"),
                     ICON_UV_FACESEL,
                     "FACE",
                     "tool_settings.uv_select_mode");
    context_set_item(layout,
                     "WM_OT_context_set_string",
                     N_("Island"),
                     ICON_UV_ISLANDSEL,
                     "ISLAND",
                     "tool_settings.uv_select_mode");
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Contextuales
 * \{ */

static void uvs_context_menu_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  PointerRNA sima = space_image_ptr(C);
  if (sima.data == nullptr) {
    return;
  }
  /* `sima.show_uvedit` tiene getter propio (`rna_SpaceImageEditor_show_uvedit_get`),
   * asi que se lee por RNA y no a mano. */
  if (!RNA_boolean_get(&sima, "show_uvedit")) {
    return;
  }

  const ToolSettings *ts = CTX_data_tool_settings(C);
  if (ts == nullptr) {
    return;
  }
  bool is_vert_mode, is_edge_mode;
  if (ts->uv_flag & UV_SYNC_SELECTION) {
    is_vert_mode = (ts->selectmode & SCE_SELECT_VERTEX) != 0;
    is_edge_mode = (ts->selectmode & SCE_SELECT_EDGE) != 0;
  }
  else {
    is_vert_mode = ts->uv_selectmode == UV_SELECT_VERTEX;
    is_edge_mode = ts->uv_selectmode == UV_SELECT_EDGE;
  }

  /* Anadir. */
  layout->op("UV_OT_unwrap", std::nullopt, ICON_NONE);
  layout->op("UV_OT_follow_active_quads", std::nullopt, ICON_NONE);

  layout->separator();

  /* Modificar. */
  PointerRNA props = layout->op("UV_OT_pin", std::nullopt, ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "clear", false);
  }
  props = layout->op("UV_OT_pin", IFACE_("Unpin"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "clear", true);
  }

  layout->separator();

  layout->menu("IMAGE_MT_uvs_snap", std::nullopt, ICON_NONE);

  mirror_axis_item(layout, N_("Mirror X"), 0);
  mirror_axis_item(layout, N_("Mirror Y"), 1);

  layout->separator();

  uiItemsEnumO(layout, "UV_OT_align", "axis");

  layout->separator();

  if (is_vert_mode || is_edge_mode) {
    uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);

    if (is_vert_mode) {
      layout->op("TRANSFORM_OT_vert_slide", std::nullopt, ICON_NONE);
    }
    if (is_edge_mode) {
      layout->op("TRANSFORM_OT_edge_slide", std::nullopt, ICON_NONE);
    }

    uiLayoutSetOperatorContext(layout, WM_OP_EXEC_REGION_WIN);
    layout->separator();
  }

  /* Quitar. */
  layout->menu("IMAGE_MT_uvs_merge", std::nullopt, ICON_NONE);
  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);
  layout->op("UV_OT_stitch", std::nullopt, ICON_NONE);
  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_REGION_WIN);
  layout->menu("IMAGE_MT_uvs_split", std::nullopt, ICON_NONE);
}

/**
 * `draw_mask_context_menu()` de `bl_ui/properties_mask_common.py`: lo comparten
 * el editor de imagen, el de clips y el secuenciador. Aqui va el unico lector
 * que se migra en esta tanda; cuando le toquen los otros dos, esto se comparte.
 */
static void mask_context_menu_items(uiLayout *layout, const bContext *C)
{
  if (wmOperatorType *ot = WM_operatortype_find("MASK_OT_handle_type_set", false)) {
    PointerRNA props;
    uiItemMenuEnumFullO_ptr(layout, C, ot, "type", std::nullopt, ICON_NONE, &props);
  }
  layout->op("MASK_OT_switch_direction", std::nullopt, ICON_NONE);
  layout->op("MASK_OT_cyclic_toggle", std::nullopt, ICON_NONE);

  layout->separator();
  layout->op("MASK_OT_copy_splines", std::nullopt, ICON_COPYDOWN);
  layout->op("MASK_OT_paste_splines", std::nullopt, ICON_PASTEDOWN);

  layout->separator();

  layout->op("MASK_OT_shape_key_rekey", IFACE_("Re-Key Shape Points"), ICON_NONE);
  layout->op("MASK_OT_feather_weight_clear", std::nullopt, ICON_NONE);
  layout->op("MASK_OT_shape_key_feather_reset", IFACE_("Reset Feather Animation"), ICON_NONE);

  layout->separator();

  layout->op("MASK_OT_parent_set", std::nullopt, ICON_NONE);
  layout->op("MASK_OT_parent_clear", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("MASK_OT_delete", std::nullopt, ICON_NONE);
}

static bool mask_context_menu_poll(const bContext *C, MenuType * /*mt*/)
{
  PointerRNA sima = space_image_ptr(C);
  return sima.data != nullptr && RNA_boolean_get(&sima, "show_maskedit");
}

static void mask_context_menu_draw(const bContext *C, Menu *menu)
{
  mask_context_menu_items(menu->layout, C);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Radiales
 * \{ */

static void pivot_pie_draw(const bContext *C, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  PointerRNA sima = space_image_ptr(C);
  if (sima.data == nullptr) {
    return;
  }
  uiItemEnumR_string(&pie, &sima, "pivot_point", "CENTER", std::nullopt, ICON_NONE);
  uiItemEnumR_string(&pie, &sima, "pivot_point", "CURSOR", std::nullopt, ICON_NONE);
  uiItemEnumR_string(&pie, &sima, "pivot_point", "INDIVIDUAL_ORIGINS", std::nullopt, ICON_NONE);
  uiItemEnumR_string(&pie, &sima, "pivot_point", "MEDIAN", std::nullopt, ICON_NONE);
}

static void uvs_snap_pie_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;
  uiLayout &pie = layout->menu_pie();

  /* El Python pone el contexto sobre `layout`, no sobre `pie`; se copia igual. */
  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_REGION_WIN);

  op_enum_item_icon(&pie,
                    "UV_OT_snap_selected",
                    N_("Selected to Pixels"),
                    ICON_RESTRICT_SELECT_OFF,
                    "target",
                    "PIXELS");
  op_enum_item_icon(
      &pie, "UV_OT_snap_cursor", N_("Cursor to Pixels"), ICON_PIVOT_CURSOR, "target", "PIXELS");
  op_enum_item_icon(
      &pie, "UV_OT_snap_cursor", N_("Cursor to Selected"), ICON_PIVOT_CURSOR, "target", "SELECTED");
  op_enum_item_icon(&pie,
                    "UV_OT_snap_selected",
                    N_("Selected to Cursor"),
                    ICON_RESTRICT_SELECT_OFF,
                    "target",
                    "CURSOR");
  op_enum_item_icon(&pie,
                    "UV_OT_snap_selected",
                    N_("Selected to Cursor (Offset)"),
                    ICON_RESTRICT_SELECT_OFF,
                    "target",
                    "CURSOR_OFFSET");
  op_enum_item_icon(&pie,
                    "UV_OT_snap_selected",
                    N_("Selected to Adjacent Unselected"),
                    ICON_RESTRICT_SELECT_OFF,
                    "target",
                    "ADJACENT_UNSELECTED");
  op_enum_item_icon(
      &pie, "UV_OT_snap_cursor", N_("Cursor to Origin"), ICON_PIVOT_CURSOR, "target", "ORIGIN");
}

static void view_pie_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  PointerRNA sima = space_image_ptr(C);
  if (sima.data == nullptr) {
    return;
  }
  const bool show_uvedit = RNA_boolean_get(&sima, "show_uvedit");
  const bool show_maskedit = RNA_boolean_get(&sima, "show_maskedit");

  uiLayout &pie = layout->menu_pie();
  pie.op("IMAGE_OT_view_all", std::nullopt, ICON_NONE);

  if (show_uvedit || show_maskedit) {
    pie.op("IMAGE_OT_view_selected", IFACE_("Frame Selected"), ICON_ZOOM_SELECTED);
    pie.op("IMAGE_OT_view_center_cursor", IFACE_("Center View to Cursor"), ICON_NONE);
  }
  else {
    /* Huecos para que los items no se muevan de sitio entre modos. */
    pie.separator();
    pie.separator();
  }

  PointerRNA props = pie.op("IMAGE_OT_view_zoom_ratio", IFACE_("Zoom 1:1"), ICON_NONE);
  if (props.data) {
    RNA_float_set(&props, "ratio", 1.0f);
  }
  props = pie.op("IMAGE_OT_view_all", IFACE_("Frame All Fit"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "fit_view", true);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registro
 * \{ */

static const flipendo::MenuDecl image_menus[] = {
    {
        /*idname*/ "IMAGE_MT_uvs_snap",
        /*label*/ N_("Snap"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ uvs_snap_draw,
    },
    {
        /*idname*/ "IMAGE_MT_uvs_align",
        /*label*/ N_("Align"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ uvs_align_draw,
    },
    {
        /*idname*/ "IMAGE_MT_uvs_merge",
        /*label*/ N_("Merge"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ uvs_merge_draw,
    },
    {
        /*idname*/ "IMAGE_MT_uvs_split",
        /*label*/ N_("Split"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ uvs_split_draw,
    },
    {
        /*idname*/ "IMAGE_MT_uvs_unwrap",
        /*label*/ N_("Unwrap"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ uvs_unwrap_draw,
    },
    {
        /*idname*/ "IMAGE_MT_uvs_select_mode",
        /*label*/ N_("UV Select Mode"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ uvs_select_mode_draw,
    },
    {
        /*idname*/ "IMAGE_MT_uvs_context_menu",
        /*label*/ N_("UV"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ uvs_context_menu_draw,
    },
    {
        /*idname*/ "IMAGE_MT_mask_context_menu",
        /*label*/ N_("Mask"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ mask_context_menu_draw,
        /*poll*/ mask_context_menu_poll,
    },
    {
        /*idname*/ "IMAGE_MT_pivot_pie",
        /*label*/ N_("Pivot Point"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ pivot_pie_draw,
    },
    {
        /*idname*/ "IMAGE_MT_uvs_snap_pie",
        /*label*/ N_("Snap"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ uvs_snap_pie_draw,
    },
    {
        /*idname*/ "IMAGE_MT_view_pie",
        /*label*/ N_("View"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ view_pie_draw,
    },
};

void image_menus_register()
{
  flipendo::menus_register({image_menus, ARRAY_SIZE(image_menus)});
}

/** \} */

}  // namespace blender::ed::image
