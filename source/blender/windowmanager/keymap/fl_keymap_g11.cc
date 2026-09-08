/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Mapa de teclado, grupo 11: graficas y hoja de exposicion del editor de clips
 * (Clip Graph Editor, Clip Dopesheet Editor), hoja de calculo (Spreadsheet) y los
 * mapas genericos de animacion (Frames, Animation, Animation Channels).
 * Transliterado de blender_default.py.
 */

#include "FL_keymap_build.hpp"
#include "FL_keymap_params.hpp"

namespace flipendo::keymap {

/* -------------------------------------------------------------------- */
/** \name Editor (Clip: graficas y hoja de exposicion)
 * \{ */

static void km_clip_graph_editor(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Clip Graph Editor", "CLIP_EDITOR", "WINDOW");

  item(km, "clip.graph_select", ev(params.select_mouse, "PRESS"));
  item(km, "clip.graph_select", ev(params.select_mouse, "PRESS").shift())
      .boolean("extend", true);

  /* _template_items_select_actions(params, "clip.graph_select_all_markers"). */
  if (!params.use_select_all_toggle) {
    item(km, "clip.graph_select_all_markers", ev("A", "PRESS")).enum_("action", "SELECT");
    item(km, "clip.graph_select_all_markers", ev("A", "PRESS").alt())
        .enum_("action", "DESELECT");
    item(km, "clip.graph_select_all_markers", ev("I", "PRESS").ctrl())
        .enum_("action", "INVERT");
    item(km, "clip.graph_select_all_markers", ev("A", "DOUBLE_CLICK"))
        .enum_("action", "DESELECT");
  }
  else if (params.legacy) {
    /* En el keymap heredado Alt+A es la reproduccion, por eso ahi no hay "deseleccionar". */
    item(km, "clip.graph_select_all_markers", ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, "clip.graph_select_all_markers", ev("I", "PRESS").ctrl())
        .enum_("action", "INVERT");
  }
  else {
    item(km, "clip.graph_select_all_markers", ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, "clip.graph_select_all_markers", ev("A", "PRESS").alt())
        .enum_("action", "DESELECT");
    item(km, "clip.graph_select_all_markers", ev("I", "PRESS").ctrl())
        .enum_("action", "INVERT");
  }

  item(km, "clip.graph_select_box", ev("B", "PRESS"));
  item(km, "clip.graph_delete_curve", ev("X", "PRESS"));
  item(km, "clip.graph_delete_curve", ev("DEL", "PRESS"));
  item(km, "clip.graph_delete_knot", ev("X", "PRESS").shift());
  item(km, "clip.graph_delete_knot", ev("DEL", "PRESS").shift());
  item(km, "clip.graph_view_all", ev("HOME", "PRESS"));
  item(km, "clip.graph_view_all", ev("NDOF_BUTTON_FIT", "PRESS"));
  item(km, "clip.graph_center_current_frame", ev("NUMPAD_0", "PRESS"));
  item(km, "wm.context_toggle", ev("L", "PRESS"))
      .string("data_path", "space_data.lock_time_cursor");
  item(km, "clip.clear_track_path", ev("T", "PRESS").alt())
      .enum_("action", "REMAINED")
      .boolean("clear_active", true);
  item(km, "clip.clear_track_path", ev("T", "PRESS").shift())
      .enum_("action", "UPTO")
      .boolean("clear_active", true);
  item(km, "clip.clear_track_path", ev("T", "PRESS").shift().alt())
      .enum_("action", "ALL")
      .boolean("clear_active", true);
  item(km, "clip.graph_disable_markers", ev("D", "PRESS").shift())
      .enum_("action", "TOGGLE");
  item(km, "transform.translate", ev("G", "PRESS"));
  item(km, "transform.translate", ev(params.select_mouse, "CLICK_DRAG"));
  item(km, "transform.resize", ev("S", "PRESS"));
  item(km, "transform.rotate", ev("R", "PRESS"));

  /* `params.select_mouse == 'LEFTMOUSE'` se comprueba con `!params.select_mouse_right`. */
  if (!params.select_mouse_right && !params.legacy) {
    item(km, "clip.change_frame", ev("RIGHTMOUSE", "PRESS").shift());
  }
  else {
    item(km, "clip.change_frame", ev(params.action_mouse, "PRESS"));
  }
}

static void km_clip_dopesheet_editor(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap(kc, "Clip Dopesheet Editor", "CLIP_EDITOR", "WINDOW");

  item(km, "clip.dopesheet_select_channel", ev("LEFTMOUSE", "PRESS")).boolean("extend", true);
  item(km, "clip.dopesheet_view_all", ev("HOME", "PRESS"));
  item(km, "clip.dopesheet_view_all", ev("NDOF_BUTTON_FIT", "PRESS"));
  item(km, "clip.delete_track", ev("X", "PRESS"));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Editor (Spreadsheet)
 * \{ */

static void km_spreadsheet_generic(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Spreadsheet Generic", "SPREADSHEET", "WINDOW");

  /* _template_space_region_type_toggle(params, sidebar_key=N, channels_key=T).
   * Con el menu radial de regiones activo la plantilla emite solo el radial, con la
   * tecla de la barra lateral (`pie_key = sidebar_key or ... or channels_key`). */
  if (params.use_region_toggle_pie) {
    item_menu_pie(km, "WM_MT_region_toggle_pie", ev("N", "PRESS"));
  }
  else {
    item(km, "wm.context_toggle", ev("N", "PRESS"))
        .string("data_path", "space_data.show_region_ui");
    item(km, "wm.context_toggle", ev("T", "PRESS"))
        .string("data_path", "space_data.show_region_channels");
  }

  item(km, "spreadsheet.resize_column", ev("LEFTMOUSE", "PRESS"));
  item(km, "spreadsheet.fit_column", ev("LEFTMOUSE", "DOUBLE_CLICK"));
  item(km, "spreadsheet.reorder_columns", ev("LEFTMOUSE", "CLICK_DRAG"));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Animacion
 * \{ */

static void km_frames(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Frames", "EMPTY", "WINDOW");

  /* Saltos de fotograma. */
  item(km, "screen.frame_offset", ev("LEFT_ARROW", "PRESS").repeat()).integer("delta", -1);
  item(km, "screen.frame_offset", ev("RIGHT_ARROW", "PRESS").repeat()).integer("delta", 1);
  item(km, "screen.frame_jump", ev("RIGHT_ARROW", "PRESS").shift().repeat())
      .boolean("end", true);
  item(km, "screen.frame_jump", ev("LEFT_ARROW", "PRESS").shift().repeat())
      .boolean("end", false);
  item(km, "screen.keyframe_jump", ev("UP_ARROW", "PRESS").repeat()).boolean("next", true);
  item(km, "screen.keyframe_jump", ev("DOWN_ARROW", "PRESS").repeat()).boolean("next", false);
  item(km, "screen.keyframe_jump", ev("MEDIA_LAST", "PRESS")).boolean("next", true);
  item(km, "screen.keyframe_jump", ev("MEDIA_FIRST", "PRESS")).boolean("next", false);
  item(km, "screen.frame_offset", ev("WHEELDOWNMOUSE", "PRESS").alt()).integer("delta", 1);
  item(km, "screen.frame_offset", ev("WHEELUPMOUSE", "PRESS").alt()).integer("delta", -1);

  if (!params.legacy) {
    /* Reproduccion nueva. */
    if (params.spacebar_action == SpacebarAction::Tool ||
        params.spacebar_action == SpacebarAction::Search)
    {
      item(km, "screen.animation_play", ev("SPACE", "PRESS").shift());
    }
    else if (params.spacebar_action == SpacebarAction::Play) {
      item(km, "screen.animation_play", ev("SPACE", "PRESS"));
    }

    item(km, "screen.animation_play", ev("SPACE", "PRESS").shift().ctrl())
        .boolean("reverse", true);
  }
  else {
    /* Reproduccion a la antigua. */
    item(km, "screen.frame_offset", ev("UP_ARROW", "PRESS").shift().repeat())
        .integer("delta", 10);
    item(km, "screen.frame_offset", ev("DOWN_ARROW", "PRESS").shift().repeat())
        .integer("delta", -10);
    item(km, "screen.frame_jump", ev("UP_ARROW", "PRESS").shift().ctrl().repeat())
        .boolean("end", true);
    item(km, "screen.frame_jump", ev("DOWN_ARROW", "PRESS").shift().ctrl().repeat())
        .boolean("end", false);
    item(km, "screen.animation_play", ev("A", "PRESS").alt());
    item(km, "screen.animation_play", ev("A", "PRESS").shift().alt()).boolean("reverse", true);
  }

  item(km, "screen.animation_cancel", ev("ESC", "PRESS"));
  item(km, "screen.animation_play", ev("MEDIA_PLAY", "PRESS"));
  item(km, "screen.animation_cancel", ev("MEDIA_STOP", "PRESS"));
}

static void km_animation(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap(kc, "Animation", "EMPTY", "WINDOW");

  /* Gestion de fotogramas. */
  item(km, "wm.context_toggle", ev("T", "PRESS").ctrl())
      .string("data_path", "space_data.show_seconds");
  /* Rango de previsualizacion. */
  item(km, "anim.previewrange_set", ev("P", "PRESS"));
  item(km, "anim.previewrange_clear", ev("P", "PRESS").alt());
  item(km, "anim.start_frame_set", ev("HOME", "PRESS").ctrl());
  item(km, "anim.end_frame_set", ev("END", "PRESS").ctrl());
}

static void km_animation_channels(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Animation Channels", "EMPTY", "WINDOW");

  /* Seleccion con clic. */
  item(km, "anim.channels_click", ev("LEFTMOUSE", "PRESS"));
  item(km, "anim.channels_click", ev("LEFTMOUSE", "CLICK").shift())
      .boolean("extend_range", true);
  item(km, "anim.channels_click", ev("LEFTMOUSE", "CLICK").ctrl()).boolean("extend", true);
  item(km, "anim.channels_click", ev("LEFTMOUSE", "PRESS").shift().ctrl())
      .boolean("children_only", true);
  /* Renombrar. */
  item(km, "anim.channels_rename", ev("LEFTMOUSE", "DOUBLE_CLICK"));
  /* Seleccionar claves. */
  item(km, "anim.channel_select_keys", ev("LEFTMOUSE", "DOUBLE_CLICK"));
  item(km, "anim.channel_select_keys", ev("LEFTMOUSE", "DOUBLE_CLICK").shift())
      .boolean("extend", true);
  /* Buscar (filtro por nombre). */
  item(km, "anim.channels_select_filter", ev("F", "PRESS").ctrl());
  /* Seleccion. */
  /* _template_items_select_actions(params, "anim.channels_select_all"). */
  if (!params.use_select_all_toggle) {
    item(km, "anim.channels_select_all", ev("A", "PRESS")).enum_("action", "SELECT");
    item(km, "anim.channels_select_all", ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, "anim.channels_select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
    item(km, "anim.channels_select_all", ev("A", "DOUBLE_CLICK")).enum_("action", "DESELECT");
  }
  else if (params.legacy) {
    /* En el keymap heredado Alt+A es la reproduccion, por eso ahi no hay "deseleccionar". */
    item(km, "anim.channels_select_all", ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, "anim.channels_select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }
  else {
    item(km, "anim.channels_select_all", ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, "anim.channels_select_all", ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, "anim.channels_select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }

  item(km, "anim.channels_select_box", ev("B", "PRESS"));
  item(km, "anim.channels_select_box", ev("LEFTMOUSE", "CLICK_DRAG"))
      .boolean("extend", false);
  item(km, "anim.channels_select_box", ev("LEFTMOUSE", "CLICK_DRAG").shift())
      .boolean("extend", true);
  item(km, "anim.channels_select_box", ev("LEFTMOUSE", "CLICK_DRAG").ctrl())
      .boolean("deselect", true);
  /* Borrar. */
  item(km, "anim.channels_delete", ev("X", "PRESS"));
  item(km, "anim.channels_delete", ev("DEL", "PRESS"));
  /* Ajustes. */
  item(km, "anim.channels_setting_toggle", ev("W", "PRESS").shift());
  item(km, "anim.channels_setting_enable", ev("W", "PRESS").shift().ctrl());
  item(km, "anim.channels_setting_disable", ev("W", "PRESS").alt());
  item(km, "anim.channels_editable_toggle", ev("TAB", "PRESS"));
  /* Desplegar/plegar. */
  item(km, "anim.channels_expand", ev("NUMPAD_PLUS", "PRESS"));
  item(km, "anim.channels_collapse", ev("NUMPAD_MINUS", "PRESS"));
  item(km, "anim.channels_expand", ev("NUMPAD_PLUS", "PRESS").ctrl()).boolean("all", false);
  item(km, "anim.channels_collapse", ev("NUMPAD_MINUS", "PRESS").ctrl()).boolean("all", false);
  /* Mover. */
  item(km, "anim.channels_move", ev("PAGE_UP", "PRESS").repeat()).enum_("direction", "UP");
  item(km, "anim.channels_move", ev("PAGE_DOWN", "PRESS").repeat()).enum_("direction", "DOWN");
  item(km, "anim.channels_move", ev("PAGE_UP", "PRESS").shift()).enum_("direction", "TOP");
  item(km, "anim.channels_move", ev("PAGE_DOWN", "PRESS").shift()).enum_("direction", "BOTTOM");
  /* Grupos. */
  item(km, "anim.channels_group", ev("G", "PRESS").ctrl());
  item(km, "anim.channels_ungroup", ev("G", "PRESS").ctrl().alt());
  /* Menus. */
  /* _template_items_context_menu("DOPESHEET_MT_channel_context_menu", params.context_menu_event). */
  item_menu(km, "DOPESHEET_MT_channel_context_menu", params.context_menu_event);
  item_menu(km, "DOPESHEET_MT_channel_context_menu", ev("APP", "PRESS"));
  /* Vista. */
  item(km, "anim.channel_view_pick", ev("MIDDLEMOUSE", "PRESS").alt());
  item(km, "anim.channels_view_selected", ev("NUMPAD_PERIOD", "PRESS"));
}

/** \} */

void register_group_11(wmKeyConfig *kc, const Params &params)
{
  km_clip_graph_editor(kc, params);
  km_clip_dopesheet_editor(kc, params);
  km_spreadsheet_generic(kc, params);
  km_frames(kc, params);
  km_animation(kc, params);
  km_animation_channels(kc, params);
}

}  // namespace flipendo::keymap
