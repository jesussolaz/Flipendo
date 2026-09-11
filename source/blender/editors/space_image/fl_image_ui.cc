/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spimage
 *
 * Once menus de la cabecera del editor de imagen y UV, en C++ nativo.
 *
 * Sustituyen a `IMAGE_MT_view`, `_view_zoom`, `_select`, `_select_linked`,
 * `_image`, `_image_transform`, `_image_invert`, `_uvs`, `_uvs_showhide`,
 * `_uvs_transform` y `_uvs_mirror` de `scripts/startup/bl_ui/space_image.py`. Los
 * otros once del fichero —los que el keymap abre por nombre— ya estaban en
 * `fl_image_menus.cc`.
 *
 * EL DOCEAVO, `IMAGE_MT_editor_menus`, SE QUEDA EN PYTHON Y NO ES UN OLVIDO
 * -------------------------------------------------------------------------
 * `IMAGE_HT_header` —que sigue siendo Python, porque sus paneles vecinos dependen
 * de `properties_paint_common`— lo invoca como CLASE:
 * `IMAGE_MT_editor_menus.draw_collapsible(context, layout)`, no por nombre.
 * Retirar la clase dejaria la cabecera rota. Se migra cuando se migre la cabecera,
 * que es quien tiene la referencia.
 *
 * POR QUE LOS MENUS SI Y LOS PANELES NO (todavia)
 * -----------------------------------------------
 * Un `MenuType` vive en el registro global (`WM_menutype_add`), no en la lista de
 * una region, asi que migrar menus **no puede** mover ningun panel de sitio: no hay
 * bloque `REGION` que se entere. Los paneles del editor de imagen, en cambio, no se
 * pueden tocar todavia: 14 de ellos llaman a `properties_paint_common` (1.965
 * lineas compartidas con la vista 3D y las pestanas de Propiedades, o sea otros dos
 * carriles) y estan intercalados en la lista de `IMAGE_EDITOR UI`, asi que no forman
 * un prefijo. Ver `politicas/UI-A-CPP.md` y la regla del prefijo de
 * `politicas/MENUS-DEL-KEYMAP-A-CPP.md`.
 *
 * TRAMPAS
 * -------
 * - `IMAGE_MT_image` decide si hay portapapeles de imagen con `sys.platform` y, en
 *   Linux, con `_ghost_backend()`. En macOS —el unico objetivo de Flipendo— siempre
 *   es cierto; queda escrito como condicion de plataforma, no borrado.
 * - `IMAGE_MT_view_zoom` construye el texto con `translate=False`: NO pasa por
 *   `IFACE_()`. Traducirlo cambiaria el volcado.
 * - `context.area.ui_type == 'IMAGE_EDITOR'` es, para este editor,
 *   `sima->mode != SI_MODE_UV` (ver `image_space_subtype_get`).
 *
 * Doctrina: politicas/LENGUAJE-CPP.md.
 */

#include <fmt/format.h>

#include <cmath>
#include <optional>

#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "DNA_image_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_enums.h"
#include "DNA_space_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "FL_ui_registry.hh"

#include "FL_image_ui.hh"

namespace blender::ed::image {

/* -------------------------------------------------------------------- */
/** \name Fuentes de datos
 * \{ */

/** `context.space_data`, ya refinado. */
static PointerRNA space_image_ui_ptr(const bContext *C)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  if (sima == nullptr) {
    return PointerRNA_NULL;
  }
  return RNA_pointer_create_discrete(
      reinterpret_cast<ID *>(CTX_wm_screen(C)), &RNA_SpaceImageEditor, sima);
}

/** `sima.uv_editor`. `SpaceUVEditor` usa el propio `SpaceImage` como dato. */
static PointerRNA uv_editor_ptr(const bContext *C)
{
  SpaceImage *sima = CTX_wm_space_image(C);
  if (sima == nullptr) {
    return PointerRNA_NULL;
  }
  return RNA_pointer_create_discrete(
      reinterpret_cast<ID *>(CTX_wm_screen(C)), &RNA_SpaceUVEditor, sima);
}

/** `context.tool_settings`. */
static PointerRNA tool_settings_ui_ptr(const bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  if (scene == nullptr || ts == nullptr) {
    return PointerRNA_NULL;
  }
  return RNA_pointer_create_id_subdata(scene->id, &RNA_ToolSettings, ts);
}

/** `layout.operator(op, text=t).<prop> = <bool>`. */
static void op_bool_item(
    uiLayout *layout, const char *opname, const char *text, int icon, const char *prop, bool value)
{
  PointerRNA props = layout->op(
      opname, text ? std::optional<StringRef>(IFACE_(text)) : std::nullopt, icon);
  if (props.data) {
    RNA_boolean_set(&props, prop, value);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name IMAGE_MT_view y IMAGE_MT_view_zoom
 * \{ */

static void view_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  SpaceImage *sima = CTX_wm_space_image(C);
  if (sima == nullptr) {
    return;
  }
  PointerRNA sima_ptr = space_image_ui_ptr(C);
  PointerRNA uv_ptr = uv_editor_ptr(C);
  PointerRNA ts_ptr = tool_settings_ui_ptr(C);
  PointerRNA paint_ptr = RNA_pointer_get(&ts_ptr, "image_paint");

  const bool show_uvedit = RNA_boolean_get(&sima_ptr, "show_uvedit");
  const bool show_render = RNA_boolean_get(&sima_ptr, "show_render");
  const bool show_maskedit = RNA_boolean_get(&sima_ptr, "show_maskedit");

  layout->prop(&sima_ptr, "show_region_toolbar", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(&sima_ptr, "show_region_ui", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(&sima_ptr, "show_region_tool_header", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(&sima_ptr, "show_region_asset_shelf", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(&sima_ptr, "show_region_hud", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  layout->separator();

  layout->prop(&sima_ptr, "use_realtime_update", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(&uv_ptr, "show_metadata", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  layout->separator();

  if (show_uvedit || show_maskedit) {
    layout->op("IMAGE_OT_view_selected", IFACE_("Frame Selected"), ICON_NONE);
  }

  layout->op("IMAGE_OT_view_all", std::nullopt, ICON_NONE);
  layout->op("IMAGE_OT_view_center_cursor", IFACE_("Center View to Cursor"), ICON_NONE);

  layout->menu("IMAGE_MT_view_zoom", std::nullopt, ICON_NONE);

  layout->separator();

  if (show_render) {
    layout->op("IMAGE_OT_render_border", std::nullopt, ICON_NONE);
    layout->op("IMAGE_OT_clear_render_border", std::nullopt, ICON_NONE);

    layout->separator();

    op_bool_item(layout,
                 "IMAGE_OT_cycle_render_slot",
                 "Render Slot Cycle Next",
                 ICON_NONE,
                 "reverse",
                 false);
    op_bool_item(layout,
                 "IMAGE_OT_cycle_render_slot",
                 "Render Slot Cycle Previous",
                 ICON_NONE,
                 "reverse",
                 true);
    layout->separator();
  }

  /* `paint.brush and (context.image_paint_object or sima.mode == 'PAINT')`. */
  const bool has_brush = paint_ptr.data != nullptr &&
                         RNA_pointer_get(&paint_ptr, "brush").data != nullptr;
  const bool painting = CTX_data_pointer_get_type(C, "image_paint_object", &RNA_Object).data !=
                            nullptr ||
                        sima->mode == SI_MODE_PAINT;
  if (has_brush && painting) {
    layout->prop(&ts_ptr, "show_uv_local_view", UI_ITEM_NONE, IFACE_("Show Same Material"), ICON_NONE);
  }

  layout->menu("INFO_MT_area", std::nullopt, ICON_NONE);
}

static void view_zoom_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  /* `(1, 8), (1, 4), (1, 2), (1, 1), (2, 1), (4, 1), (8, 1)`. */
  static const int ratios[][2] = {{1, 8}, {1, 4}, {1, 2}, {1, 1}, {2, 1}, {4, 1}, {8, 1}};

  PointerRNA sima_ptr = space_image_ui_ptr(C);
  const float current_zoom = sima_ptr.data ? RNA_float_get(&sima_ptr, "zoom_percentage") : 0.0f;

  for (const int(&r)[2] : ratios) {
    const float ratio = float(r[0]) / float(r[1]);
    const float percent = ratio * 100.0f;
    /* `isclose(percent, current_zoom, abs_tol=0.5)`. */
    const bool is_current = std::fabs(percent - current_zoom) <= 0.5f;
    /* `translate=False`: el texto NO pasa por `IFACE_()`. `{:g}` es el formato de
     * Python, que fmt escribe igual. */
    const std::string text = fmt::format("{:g}% ({:d}:{:d})", percent, r[0], r[1]);
    PointerRNA props = layout->op(
        "IMAGE_OT_view_zoom_ratio", text, is_current ? ICON_LAYER_ACTIVE : ICON_NONE);
    if (props.data) {
      RNA_float_set(&props, "ratio", ratio);
    }
  }

  layout->separator();
  layout->op("IMAGE_OT_view_zoom_in", std::nullopt, ICON_NONE);
  layout->op("IMAGE_OT_view_zoom_out", std::nullopt, ICON_NONE);
  op_bool_item(layout, "IMAGE_OT_view_all", "Zoom to Fit", ICON_NONE, "fit_view", true);
  layout->op("IMAGE_OT_view_zoom_border", IFACE_("Zoom Region..."), ICON_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Seleccion
 * \{ */

static void select_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  PointerRNA props = layout->op("UV_OT_select_all", IFACE_("All"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "action", "SELECT");
  }
  props = layout->op("UV_OT_select_all", IFACE_("None"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "action", "DESELECT");
  }
  props = layout->op("UV_OT_select_all", IFACE_("Invert"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "action", "INVERT");
  }

  layout->separator();

  op_bool_item(layout, "UV_OT_select_box", nullptr, ICON_NONE, "pinned", false);
  op_bool_item(layout, "UV_OT_select_box", "Box Select Pinned", ICON_NONE, "pinned", true);
  layout->op("UV_OT_select_circle", std::nullopt, ICON_NONE);
  uiItemMenuEnumO(layout, C, "UV_OT_select_lasso", "mode", IFACE_("Lasso Select"), ICON_NONE);

  layout->separator();

  layout->op("UV_OT_select_more", IFACE_("More"), ICON_NONE);
  layout->op("UV_OT_select_less", IFACE_("Less"), ICON_NONE);

  layout->separator();

  uiItemMenuEnumO(layout, C, "UV_OT_select_similar", "type", IFACE_("Select Similar"), ICON_NONE);
  layout->menu("IMAGE_MT_select_linked", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("UV_OT_select_pinned", IFACE_("Select Pinned"), ICON_NONE);
  layout->op("UV_OT_select_split", std::nullopt, ICON_NONE);
  layout->op("UV_OT_select_overlap", std::nullopt, ICON_NONE);
}

static void select_linked_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;
  layout->op("UV_OT_select_linked", IFACE_("Linked"), ICON_NONE);
  layout->op("UV_OT_shortest_path_select", IFACE_("Shortest Path"), ICON_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Imagen
 * \{ */

/**
 * El `has_image_clipboard` del Python: cierto en Windows y macOS, y en Linux solo
 * con el backend Wayland de GHOST. Flipendo es solo macOS, asi que aqui es cierto;
 * la condicion queda escrita para que se vea que no se ha perdido nada.
 */
static bool has_image_clipboard()
{
#if defined(__APPLE__) || defined(_WIN32)
  return true;
#else
  /* En Linux dependeria del backend de GHOST (`_ghost_backend() == 'WAYLAND'`).
   * Flipendo no construye para Linux. */
  return false;
#endif
}

static void image_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  SpaceImage *sima = CTX_wm_space_image(C);
  if (sima == nullptr) {
    return;
  }
  PointerRNA sima_ptr = space_image_ui_ptr(C);
  Image *ima = sima->image;
  const bool show_render = RNA_boolean_get(&sima_ptr, "show_render");

  PointerRNA new_props = layout->op(
      "IMAGE_OT_new", CTX_IFACE_(BLT_I18NCONTEXT_ID_IMAGE, "New..."), ICON_FILE_NEW);
  UNUSED_VARS(new_props);
  layout->op("IMAGE_OT_open", IFACE_("Open..."), ICON_FILE_FOLDER);

  layout->op("IMAGE_OT_read_viewlayers", std::nullopt, ICON_NONE);

  if (ima != nullptr) {
    layout->separator();

    if (!show_render) {
      layout->op("IMAGE_OT_replace", IFACE_("Replace..."), ICON_NONE);
      layout->op("IMAGE_OT_reload", IFACE_("Reload"), ICON_NONE);
    }

    layout->op("IMAGE_OT_external_edit", IFACE_("Edit Externally"), ICON_NONE);
  }

  layout->separator();

  if (has_image_clipboard()) {
    layout->op("IMAGE_OT_clipboard_copy", IFACE_("Copy"), ICON_NONE);
    layout->op("IMAGE_OT_clipboard_paste", IFACE_("Paste"), ICON_NONE);
    layout->separator();
  }

  PointerRNA ima_ptr = ima ? RNA_id_pointer_create(&ima->id) : PointerRNA_NULL;

  if (ima != nullptr) {
    layout->op("IMAGE_OT_save", IFACE_("Save"), ICON_FILE_TICK);
    layout->op("IMAGE_OT_save_as", IFACE_("Save As..."), ICON_NONE);
    op_bool_item(layout, "IMAGE_OT_save_as", "Save a Copy...", ICON_NONE, "copy", true);
  }

  if (ima != nullptr && ima->source == IMA_SRC_SEQUENCE) {
    layout->op("IMAGE_OT_save_sequence", std::nullopt, ICON_NONE);
  }

  layout->op("IMAGE_OT_save_all_modified", IFACE_("Save All Images"), ICON_NONE);

  if (ima != nullptr) {
    layout->separator();

    layout->menu("IMAGE_MT_image_invert", std::nullopt, ICON_NONE);
    layout->op("IMAGE_OT_resize", IFACE_("Resize"), ICON_NONE);
    layout->menu("IMAGE_MT_image_transform", std::nullopt, ICON_NONE);
  }

  if (ima != nullptr && !show_render) {
    if (RNA_pointer_get(&ima_ptr, "packed_file").data != nullptr) {
      if (ima->filepath[0] != '\0') {
        layout->separator();
        layout->op("IMAGE_OT_unpack", IFACE_("Unpack"), ICON_NONE);
      }
    }
    else {
      layout->separator();
      layout->op("IMAGE_OT_pack", IFACE_("Pack"), ICON_NONE);
    }
  }

  /* `context.area.ui_type == 'IMAGE_EDITOR'`: para este editor el subtipo es
   * `SI_MODE_UV` o `SI_MODE_VIEW` (`image_space_subtype_get`). */
  if (ima != nullptr && sima->mode != SI_MODE_UV) {
    layout->separator();
    layout->op("PALETTE_OT_extract_from_image", IFACE_("Extract Palette"), ICON_NONE);
  }
}

static void image_transform_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;
  op_bool_item(layout, "IMAGE_OT_flip", "Flip Horizontally", ICON_NONE, "use_flip_x", true);
  op_bool_item(layout, "IMAGE_OT_flip", "Flip Vertically", ICON_NONE, "use_flip_y", true);
  layout->separator();

  PointerRNA props = layout->op(
      "IMAGE_OT_rotate_orthogonal", IFACE_("Rotate 90° Clockwise"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "degrees", "90");
  }
  props = layout->op(
      "IMAGE_OT_rotate_orthogonal", IFACE_("Rotate 90° Counter-Clockwise"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "degrees", "270");
  }
  props = layout->op("IMAGE_OT_rotate_orthogonal", IFACE_("Rotate 180°"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "degrees", "180");
  }
}

static void image_invert_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;

  PointerRNA props = layout->op(
      "IMAGE_OT_invert", IFACE_("Invert Image Colors"), ICON_IMAGE_RGB);
  if (props.data) {
    RNA_boolean_set(&props, "invert_r", true);
    RNA_boolean_set(&props, "invert_g", true);
    RNA_boolean_set(&props, "invert_b", true);
  }

  layout->separator();

  op_bool_item(layout, "IMAGE_OT_invert", "Invert Red Channel", ICON_RGB_RED, "invert_r", true);
  op_bool_item(layout, "IMAGE_OT_invert", "Invert Green Channel", ICON_RGB_GREEN, "invert_g", true);
  op_bool_item(layout, "IMAGE_OT_invert", "Invert Blue Channel", ICON_RGB_BLUE, "invert_b", true);
  op_bool_item(
      layout, "IMAGE_OT_invert", "Invert Alpha Channel", ICON_IMAGE_ALPHA, "invert_a", true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV
 * \{ */

static void uvs_showhide_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;
  layout->op("UV_OT_reveal", std::nullopt, ICON_NONE);
  op_bool_item(layout, "UV_OT_hide", "Hide Selected", ICON_NONE, "unselected", false);
  op_bool_item(layout, "UV_OT_hide", "Hide Unselected", ICON_NONE, "unselected", true);
}

static void uvs_transform_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;

  layout->op("TRANSFORM_OT_translate", std::nullopt, ICON_NONE);
  layout->op("TRANSFORM_OT_rotate", std::nullopt, ICON_NONE);
  layout->op("TRANSFORM_OT_resize", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("TRANSFORM_OT_shear", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("TRANSFORM_OT_vert_slide", std::nullopt, ICON_NONE);
  layout->op("TRANSFORM_OT_edge_slide", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("UV_OT_randomize_uv_transform", std::nullopt, ICON_NONE);
}

/** `layout.operator("transform.mirror", text=t).constraint_axis[i] = True`. */
static void mirror_axis_op(uiLayout *layout, const char *text, int axis)
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

static void uvs_mirror_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;

  layout->op("MESH_OT_faces_mirror_uv", std::nullopt, ICON_NONE);

  layout->separator();

  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_REGION_WIN);

  mirror_axis_op(layout, "X Axis", 0);
  mirror_axis_op(layout, "Y Axis", 1);
}

static void uvs_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  PointerRNA uv_ptr = uv_editor_ptr(C);
  if (uv_ptr.data == nullptr) {
    return;
  }

  layout->menu("IMAGE_MT_uvs_transform", std::nullopt, ICON_NONE);
  layout->menu("IMAGE_MT_uvs_mirror", std::nullopt, ICON_NONE);
  layout->menu("IMAGE_MT_uvs_snap", std::nullopt, ICON_NONE);

  if (PropertyRNA *prop = RNA_struct_find_property(&uv_ptr, "pixel_round_mode")) {
    uiItemMenuEnumR_prop(layout, &uv_ptr, prop, std::nullopt, ICON_NONE);
  }
  layout->prop(&uv_ptr, "lock_bounds", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  layout->separator();

  layout->menu("IMAGE_MT_uvs_merge", std::nullopt, ICON_NONE);
  layout->menu("IMAGE_MT_uvs_split", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("UV_OT_rip_move", std::nullopt, ICON_NONE);

  layout->separator();

  layout->prop(&uv_ptr, "use_live_unwrap", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->menu("IMAGE_MT_uvs_unwrap", std::nullopt, ICON_NONE);

  layout->separator();

  op_bool_item(layout, "UV_OT_pin", nullptr, ICON_NONE, "clear", false);
  op_bool_item(layout, "UV_OT_pin", "Unpin", ICON_NONE, "clear", true);
  op_bool_item(layout, "UV_OT_pin", "Invert Pins", ICON_NONE, "invert", true);

  layout->separator();

  op_bool_item(layout, "UV_OT_mark_seam", nullptr, ICON_NONE, "clear", false);
  op_bool_item(layout, "UV_OT_mark_seam", "Clear Seam", ICON_NONE, "clear", true);
  layout->op("UV_OT_seams_from_islands", std::nullopt, ICON_NONE);

  layout->separator();

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);
  layout->op("UV_OT_pack_islands", std::nullopt, ICON_NONE);
  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_REGION_WIN);
  layout->op("UV_OT_average_islands_scale", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("UV_OT_minimize_stretch", std::nullopt, ICON_NONE);
  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);
  layout->op("UV_OT_stitch", std::nullopt, ICON_NONE);
  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_REGION_WIN);
  layout->menu("IMAGE_MT_uvs_align", std::nullopt, ICON_NONE);
  layout->op("UV_OT_align_rotation", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("UV_OT_copy", std::nullopt, ICON_NONE);
  layout->op("UV_OT_paste", std::nullopt, ICON_NONE);

  layout->separator();

  layout->menu("IMAGE_MT_uvs_showhide", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("UV_OT_reset", std::nullopt, ICON_NONE);

  layout->separator();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registro
 * \{ */

static const flipendo::MenuDecl image_ui_menus[] = {
    {
        /*idname*/ "IMAGE_MT_view",
        /*label*/ N_("View"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ view_draw,
    },
    {
        /*idname*/ "IMAGE_MT_view_zoom",
        /*label*/ N_("Zoom"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ view_zoom_draw,
    },
    {
        /*idname*/ "IMAGE_MT_select",
        /*label*/ N_("Select"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ select_draw,
    },
    {
        /*idname*/ "IMAGE_MT_select_linked",
        /*label*/ N_("Select Linked"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ select_linked_draw,
    },
    {
        /*idname*/ "IMAGE_MT_image",
        /*label*/ N_("Image"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ image_draw,
    },
    {
        /*idname*/ "IMAGE_MT_image_transform",
        /*label*/ N_("Transform"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ image_transform_draw,
    },
    {
        /*idname*/ "IMAGE_MT_image_invert",
        /*label*/ N_("Invert"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ image_invert_draw,
    },
    {
        /*idname*/ "IMAGE_MT_uvs_showhide",
        /*label*/ N_("Show/Hide Faces"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ uvs_showhide_draw,
    },
    {
        /*idname*/ "IMAGE_MT_uvs_transform",
        /*label*/ N_("Transform"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ uvs_transform_draw,
    },
    {
        /*idname*/ "IMAGE_MT_uvs_mirror",
        /*label*/ N_("Mirror"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ uvs_mirror_draw,
    },
    {
        /*idname*/ "IMAGE_MT_uvs",
        /*label*/ N_("UV"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ uvs_draw,
    },
};

void image_ui_menus_register()
{
  flipendo::menus_register({image_ui_menus, ARRAY_SIZE(image_ui_menus)});
}

/** \} */

}  // namespace blender::ed::image
