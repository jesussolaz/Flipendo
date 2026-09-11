/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spclip
 *
 * Ver FL_clip_menus.hh. Los ocho menus que el keymap nativo abre por nombre en
 * el editor de clips: el contextual de seguimiento, los cinco radiales
 * (marcador, seguimiento, resolucion, reconstruccion, vista), el del punto de
 * pivote y el de seleccionar agrupados.
 *
 * Cinco de ellos tienen `poll`, y casi todos miran `space.mode` y `space.clip`.
 * Los datos que cuelgan del clip (`clip.tracking.tracks.active`,
 * `clip.tracking.settings`) se resuelven con `RNA_path_resolve()` sobre el
 * puntero del espacio: es el mismo camino que recorre el Python, y asi no hay
 * que adivinar como se llama cada campo en el DNA.
 */

#include <optional>

#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "RNA_access.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "FL_ui_registry.hh"

#include "FL_clip_menus.hh"

namespace blender::ed::clip {

/** `context.space_data` del editor de clips, ya refinado. */
static PointerRNA space_clip_ptr(const bContext *C)
{
  SpaceClip *sc = CTX_wm_space_clip(C);
  if (sc == nullptr) {
    return PointerRNA_NULL;
  }
  return RNA_pointer_create_discrete(
      reinterpret_cast<ID *>(CTX_wm_screen(C)), &RNA_SpaceClipEditor, sc);
}

/** El `SpaceClip` con clip cargado, o `nullptr`. */
static const SpaceClip *space_clip_with_clip(const bContext *C)
{
  const SpaceClip *sc = CTX_wm_space_clip(C);
  return (sc != nullptr && sc->clip != nullptr) ? sc : nullptr;
}

static bool tracking_with_clip_poll(const bContext *C, MenuType * /*mt*/)
{
  const SpaceClip *sc = space_clip_with_clip(C);
  return sc != nullptr && sc->mode == SC_MODE_TRACKING;
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
  PointerRNA props = layout->op(
      opname, text ? IFACE_(text) : std::optional<StringRef>(std::nullopt), icon);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, prop, value);
  }
}

/** `wm.context_set_enum` con su `data_path` y su `value`. */
static void context_set_enum_item(
    uiLayout *layout, const char *text, int icon, const char *data_path, const char *value)
{
  PointerRNA props = layout->op("WM_OT_context_set_enum", IFACE_(text), icon);
  if (props.data == nullptr) {
    return;
  }
  RNA_string_set(&props, "data_path", data_path);
  RNA_string_set(&props, "value", value);
}

/* -------------------------------------------------------------------- */
/** \name Seleccionar agrupados y punto de pivote
 * \{ */

static void select_grouped_draw(const bContext * /*C*/, Menu *menu)
{
  uiItemsEnumO(menu->layout, "CLIP_OT_select_grouped", "group");
}

static void pivot_pie_draw(const bContext *C, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  PointerRNA sc = space_clip_ptr(C);
  if (sc.data == nullptr) {
    return;
  }
  uiItemEnumR_string(&pie, &sc, "pivot_point", "BOUNDING_BOX_CENTER", std::nullopt, ICON_NONE);
  uiItemEnumR_string(&pie, &sc, "pivot_point", "CURSOR", std::nullopt, ICON_NONE);
  uiItemEnumR_string(&pie, &sc, "pivot_point", "INDIVIDUAL_ORIGINS", std::nullopt, ICON_NONE);
  uiItemEnumR_string(&pie, &sc, "pivot_point", "MEDIAN_POINT", std::nullopt, ICON_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name CLIP_MT_tracking_context_menu
 * \{ */

/** `draw_mask_context_menu()` de `bl_ui/properties_mask_common.py`. */
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

static bool tracking_context_menu_poll(const bContext *C, MenuType * /*mt*/)
{
  return space_clip_with_clip(C) != nullptr;
}

static void tracking_context_menu_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  const SpaceClip *sc = CTX_wm_space_clip(C);
  if (sc == nullptr) {
    return;
  }

  if (sc->mode == SC_MODE_TRACKING) {
    layout->op("CLIP_OT_track_settings_to_track", std::nullopt, ICON_NONE);
    layout->op("CLIP_OT_track_settings_as_default", std::nullopt, ICON_NONE);

    layout->separator();

    layout->op("CLIP_OT_track_copy_color", std::nullopt, ICON_NONE);

    layout->separator();

    layout->op("CLIP_OT_copy_tracks", std::nullopt, ICON_COPYDOWN);
    layout->op("CLIP_OT_paste_tracks", std::nullopt, ICON_PASTEDOWN);

    layout->separator();

    op_enum_item(layout, "CLIP_OT_disable_markers", N_("Disable Markers"), "action", "DISABLE");
    op_enum_item(layout, "CLIP_OT_disable_markers", N_("Enable Markers"), "action", "ENABLE");

    layout->separator();

    layout->op("CLIP_OT_hide_tracks", std::nullopt, ICON_NONE);
    layout->op("CLIP_OT_hide_tracks_clear", IFACE_("Show Tracks"), ICON_NONE);

    layout->separator();

    op_enum_item(layout, "CLIP_OT_lock_tracks", N_("Lock Tracks"), "action", "LOCK");
    op_enum_item(layout, "CLIP_OT_lock_tracks", N_("Unlock Tracks"), "action", "UNLOCK");

    layout->separator();

    layout->op("CLIP_OT_join_tracks", std::nullopt, ICON_NONE);
    layout->op("CLIP_OT_average_tracks", std::nullopt, ICON_NONE);

    layout->separator();

    layout->op("CLIP_OT_delete_track", std::nullopt, ICON_NONE);
  }
  else if (sc->mode == SC_MODE_MASKEDIT) {
    mask_context_menu_items(layout, C);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Radiales de seguimiento
 * \{ */

static void marker_pie_draw(const bContext *C, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  PointerRNA sc = space_clip_ptr(C);
  if (sc.data == nullptr) {
    return;
  }
  /* `clip.tracking.tracks.active`, por el mismo camino que el Python. */
  PointerRNA track_active = PointerRNA_NULL;
  RNA_path_resolve(&sc, "clip.tracking.tracks.active", &track_active, nullptr);

  context_set_enum_item(&pie,
                        N_("Location"),
                        ICON_NONE,
                        "space_data.clip.tracking.tracks.active.motion_model",
                        "Loc");
  context_set_enum_item(&pie,
                        N_("Affine"),
                        ICON_NONE,
                        "space_data.clip.tracking.tracks.active.motion_model",
                        "Affine");
  pie.op("CLIP_OT_track_settings_to_track", std::nullopt, ICON_COPYDOWN);
  pie.op("CLIP_OT_track_settings_as_default", std::nullopt, ICON_SETTINGS);

  if (track_active.data) {
    pie.prop(&track_active, "use_normalization", UI_ITEM_NONE, IFACE_("Normalization"), ICON_NONE);
    pie.prop(&track_active, "use_brute", UI_ITEM_NONE, IFACE_("Use Brute Force"), ICON_NONE);
    context_set_enum_item(&pie,
                          N_("Match Previous"),
                          ICON_KEYFRAME_HLT,
                          "space_data.clip.tracking.tracks.active.pattern_match",
                          "PREV_FRAME");
    context_set_enum_item(&pie,
                          N_("Match Keyframe"),
                          ICON_KEYFRAME,
                          "space_data.clip.tracking.tracks.active.pattern_match",
                          "KEYFRAME");
  }
}

static void tracking_pie_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  PointerRNA props = pie.op("CLIP_OT_track_markers", std::nullopt, ICON_TRACKING_BACKWARDS);
  if (props.data) {
    RNA_boolean_set(&props, "backwards", true);
    RNA_boolean_set(&props, "sequence", true);
  }
  props = pie.op("CLIP_OT_track_markers", std::nullopt, ICON_TRACKING_FORWARDS);
  if (props.data) {
    RNA_boolean_set(&props, "backwards", false);
    RNA_boolean_set(&props, "sequence", true);
  }
  op_enum_item_icon(&pie, "CLIP_OT_disable_markers", nullptr, ICON_HIDE_OFF, "action", "TOGGLE");
  pie.op("CLIP_OT_detect_features", std::nullopt, ICON_ZOOM_SELECTED);
  op_enum_item_icon(
      &pie, "CLIP_OT_clear_track_path", nullptr, ICON_TRACKING_CLEAR_BACKWARDS, "action", "UPTO");
  op_enum_item_icon(&pie,
                    "CLIP_OT_clear_track_path",
                    nullptr,
                    ICON_TRACKING_CLEAR_FORWARDS,
                    "action",
                    "REMAINED");
  props = pie.op("CLIP_OT_refine_markers", std::nullopt, ICON_TRACKING_REFINE_BACKWARDS);
  if (props.data) {
    RNA_boolean_set(&props, "backwards", true);
  }
  props = pie.op("CLIP_OT_refine_markers", std::nullopt, ICON_TRACKING_REFINE_FORWARDS);
  if (props.data) {
    RNA_boolean_set(&props, "backwards", false);
  }
}

static void solving_pie_draw(const bContext *C, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  PointerRNA sc = space_clip_ptr(C);
  if (sc.data == nullptr) {
    return;
  }
  PointerRNA settings = PointerRNA_NULL;
  RNA_path_resolve(&sc, "clip.tracking.settings", &settings, nullptr);

  pie.op("CLIP_OT_clear_solution", std::nullopt, ICON_FILE_REFRESH);
  pie.op("CLIP_OT_solve_camera", IFACE_("Solve Camera"), ICON_OUTLINER_OB_CAMERA);
  if (settings.data) {
    pie.prop(&settings, "use_tripod_solver", UI_ITEM_NONE, IFACE_("Tripod Solver"), ICON_NONE);
  }
  pie.op("CLIP_OT_create_plane_track", std::nullopt, ICON_MATPLANE);
  op_enum_item_icon(
      &pie, "CLIP_OT_set_solver_keyframe", N_("Set Keyframe A"), ICON_KEYFRAME, "keyframe", "KEYFRAME_A");
  op_enum_item_icon(
      &pie, "CLIP_OT_set_solver_keyframe", N_("Set Keyframe B"), ICON_KEYFRAME, "keyframe", "KEYFRAME_B");
  PointerRNA props = pie.op("CLIP_OT_clean_tracks", std::nullopt, ICON_X);
  if (props.data) {
    RNA_int_set(&props, "frames", 15);
    RNA_float_set(&props, "error", 2.0f);
  }
  pie.op("CLIP_OT_filter_tracks", std::nullopt, ICON_FILTER);
}

static void reconstruction_pie_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  pie.op("CLIP_OT_set_viewport_background", IFACE_("Set Viewport Background"), ICON_FILE_IMAGE);
  pie.op("CLIP_OT_setup_tracking_scene", IFACE_("Setup Tracking Scene"), ICON_SCENE_DATA);
  pie.op("CLIP_OT_set_plane", IFACE_("Set Floor"), ICON_AXIS_TOP);
  pie.op("CLIP_OT_set_origin", IFACE_("Set Origin"), ICON_OBJECT_ORIGIN);
  op_enum_item_icon(&pie, "CLIP_OT_set_axis", N_("Set X Axis"), ICON_AXIS_FRONT, "axis", "X");
  op_enum_item_icon(&pie, "CLIP_OT_set_axis", N_("Set Y Axis"), ICON_AXIS_SIDE, "axis", "Y");
  pie.op("CLIP_OT_set_scale", IFACE_("Set Scale"), ICON_ARROW_LEFTRIGHT);
  pie.op("CLIP_OT_apply_solution_scale", std::nullopt, ICON_ARROW_LEFTRIGHT);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name CLIP_MT_view_pie
 * \{ */

static bool view_pie_poll(const bContext *C, MenuType * /*mt*/)
{
  /* Los operadores de vista aun no estan hechos en el modo hoja de exposicion. */
  const SpaceClip *sc = CTX_wm_space_clip(C);
  return sc != nullptr && sc->view != SC_VIEW_DOPESHEET;
}

static void view_pie_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  const SpaceClip *sc = CTX_wm_space_clip(C);
  if (sc == nullptr) {
    return;
  }
  uiLayout &pie = layout->menu_pie();

  if (sc->view == SC_VIEW_CLIP) {
    pie.op("CLIP_OT_view_all", std::nullopt, ICON_NONE);
    pie.op("CLIP_OT_view_selected", std::nullopt, ICON_ZOOM_SELECTED);

    if (sc->mode == SC_MODE_MASKEDIT) {
      pie.op("CLIP_OT_view_center_cursor", std::nullopt, ICON_NONE);
      pie.separator();
    }
    else {
      /* Huecos para que los items no se muevan de sitio entre modos. */
      pie.separator();
      pie.separator();
    }

    PointerRNA props = pie.op("CLIP_OT_view_all", IFACE_("Frame All Fit"), ICON_NONE);
    if (props.data) {
      RNA_boolean_set(&props, "fit_view", true);
    }
  }

  if (sc->view == SC_VIEW_GRAPH) {
    uiLayoutSetOperatorContext(&pie, WM_OP_INVOKE_REGION_PREVIEW);
    pie.op("CLIP_OT_graph_view_all", std::nullopt, ICON_NONE);
    pie.separator();
    pie.op("CLIP_OT_graph_center_current_frame", std::nullopt, ICON_NONE);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registro
 * \{ */

static const flipendo::MenuDecl clip_menus[] = {
    {
        /*idname*/ "CLIP_MT_select_grouped",
        /*label*/ N_("Select Grouped"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ select_grouped_draw,
    },
    {
        /*idname*/ "CLIP_MT_tracking_context_menu",
        /*label*/ N_("Context Menu"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ tracking_context_menu_draw,
        /*poll*/ tracking_context_menu_poll,
    },
    {
        /*idname*/ "CLIP_MT_pivot_pie",
        /*label*/ N_("Pivot Point"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ pivot_pie_draw,
    },
    {
        /*idname*/ "CLIP_MT_marker_pie",
        /*label*/ N_("Marker Settings"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ marker_pie_draw,
        /*poll*/ tracking_with_clip_poll,
    },
    {
        /*idname*/ "CLIP_MT_tracking_pie",
        /*label*/ N_("Tracking"),
        /*description*/ nullptr,
        /*translation_context*/ BLT_I18NCONTEXT_ID_MOVIECLIP,
        /*draw*/ tracking_pie_draw,
        /*poll*/ tracking_with_clip_poll,
    },
    {
        /*idname*/ "CLIP_MT_solving_pie",
        /*label*/ N_("Solving"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ solving_pie_draw,
        /*poll*/ tracking_with_clip_poll,
    },
    {
        /*idname*/ "CLIP_MT_reconstruction_pie",
        /*label*/ N_("Reconstruction"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ reconstruction_pie_draw,
        /*poll*/ tracking_with_clip_poll,
    },
    {
        /*idname*/ "CLIP_MT_view_pie",
        /*label*/ N_("View"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ view_pie_draw,
        /*poll*/ view_pie_poll,
    },
};

void clip_menus_register()
{
  flipendo::menus_register({clip_menus, ARRAY_SIZE(clip_menus)});
}

/** \} */

}  // namespace blender::ed::clip
