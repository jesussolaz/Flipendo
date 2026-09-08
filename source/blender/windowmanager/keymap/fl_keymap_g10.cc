/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Mapa de teclado, grupo 10: canales del editor de video (Sequencer Channels),
 * consola de Python (Console) y editor de clips de seguimiento (Clip y Clip Editor).
 * Transliterado de blender_default.py.
 */

#include "FL_keymap_build.hpp"
#include "FL_keymap_params.hpp"

namespace flipendo::keymap {

/* -------------------------------------------------------------------- */
/** \name Canales del secuenciador
 * \{ */

static void km_sequencer_channels(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap(kc, "Sequencer Channels", "SEQUENCE_EDITOR", "WINDOW");

  /* Renombrar. */
  item(km, "sequencer.rename_channel", ev("LEFTMOUSE", "PRESS").ctrl());
  item(km, "sequencer.rename_channel", ev("LEFTMOUSE", "DOUBLE_CLICK"));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Editor (Consola)
 * \{ */

static void km_console(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap(kc, "Console", "CONSOLE", "WINDOW");

  item(km, "console.move", ev("LEFT_ARROW", "PRESS").ctrl().repeat())
      .enum_("type", "PREVIOUS_WORD");
  item(km, "console.move", ev("LEFT_ARROW", "PRESS").ctrl().shift().repeat())
      .enum_("type", "PREVIOUS_WORD")
      .boolean("select", true);
  item(km, "console.move", ev("RIGHT_ARROW", "PRESS").ctrl().repeat())
      .enum_("type", "NEXT_WORD");
  item(km, "console.move", ev("RIGHT_ARROW", "PRESS").ctrl().shift().repeat())
      .enum_("type", "NEXT_WORD")
      .boolean("select", true);
  item(km, "console.move", ev("HOME", "PRESS")).enum_("type", "LINE_BEGIN");
  item(km, "console.move", ev("HOME", "PRESS").shift())
      .enum_("type", "LINE_BEGIN")
      .boolean("select", true);
  item(km, "console.move", ev("END", "PRESS")).enum_("type", "LINE_END");
  item(km, "console.move", ev("END", "PRESS").shift())
      .enum_("type", "LINE_END")
      .boolean("select", true);
  item(km, "wm.context_cycle_int", ev("WHEELUPMOUSE", "PRESS").ctrl())
      .string("data_path", "space_data.font_size")
      .boolean("reverse", false);
  item(km, "wm.context_cycle_int", ev("WHEELDOWNMOUSE", "PRESS").ctrl())
      .string("data_path", "space_data.font_size")
      .boolean("reverse", true);
  item(km, "wm.context_cycle_int", ev("NUMPAD_PLUS", "PRESS").ctrl().repeat())
      .string("data_path", "space_data.font_size")
      .boolean("reverse", false);
  item(km, "wm.context_cycle_int", ev("NUMPAD_MINUS", "PRESS").ctrl().repeat())
      .string("data_path", "space_data.font_size")
      .boolean("reverse", true);
  item(km, "console.move", ev("LEFT_ARROW", "PRESS").repeat())
      .enum_("type", "PREVIOUS_CHARACTER");
  item(km, "console.move", ev("LEFT_ARROW", "PRESS").repeat().shift())
      .enum_("type", "PREVIOUS_CHARACTER")
      .boolean("select", true);
  item(km, "console.move", ev("RIGHT_ARROW", "PRESS").repeat())
      .enum_("type", "NEXT_CHARACTER");
  item(km, "console.move", ev("RIGHT_ARROW", "PRESS").repeat().shift())
      .enum_("type", "NEXT_CHARACTER")
      .boolean("select", true);
  item(km, "console.history_cycle", ev("UP_ARROW", "PRESS").repeat()).boolean("reverse", true);
  item(km, "console.history_cycle", ev("DOWN_ARROW", "PRESS").repeat()).boolean("reverse", false);
  item(km, "console.delete", ev("DEL", "PRESS").repeat()).enum_("type", "NEXT_CHARACTER");
  item(km, "console.delete", ev("BACK_SPACE", "PRESS").repeat())
      .enum_("type", "PREVIOUS_CHARACTER");
  item(km, "console.delete", ev("BACK_SPACE", "PRESS").shift().repeat())
      .enum_("type", "PREVIOUS_CHARACTER");
  item(km, "console.delete", ev("DEL", "PRESS").ctrl().repeat()).enum_("type", "NEXT_WORD");
  item(km, "console.delete", ev("BACK_SPACE", "PRESS").ctrl().repeat())
      .enum_("type", "PREVIOUS_WORD");
  item(km, "console.clear_line", ev("RET", "PRESS").shift());
  item(km, "console.clear_line", ev("NUMPAD_ENTER", "PRESS").shift());
  item(km, "console.execute", ev("RET", "PRESS")).boolean("interactive", true);
  item(km, "console.execute", ev("NUMPAD_ENTER", "PRESS")).boolean("interactive", true);
  item(km, "console.copy_as_script", ev("C", "PRESS").shift().ctrl());
  item(km, "console.copy", ev("C", "PRESS").ctrl());
  item(km, "console.copy", ev("X", "PRESS").ctrl()).boolean("delete", true);
  item(km, "console.paste", ev("V", "PRESS").ctrl().repeat());
  item(km, "console.select_set", ev("LEFTMOUSE", "PRESS"));
  item(km, "console.select_all", ev("A", "PRESS").ctrl());
  item(km, "console.select_word", ev("LEFTMOUSE", "DOUBLE_CLICK"));
  item(km, "console.insert", ev("TAB", "PRESS").ctrl().repeat()).string("text", "\t");
  item(km, "console.indent_or_autocomplete", ev("TAB", "PRESS").repeat());
  item(km, "console.unindent", ev("TAB", "PRESS").shift().repeat());
  /* _template_items_context_menu("CONSOLE_MT_context_menu", RIGHTMOUSE/PRESS). */
  item_menu(km, "CONSOLE_MT_context_menu", ev("RIGHTMOUSE", "PRESS"));
  item_menu(km, "CONSOLE_MT_context_menu", ev("APP", "PRESS"));
  item(km, "console.insert", ev("TEXTINPUT", "ANY").any().repeat());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Editor (Clip)
 * \{ */

static void km_clip(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Clip", "CLIP_EDITOR", "WINDOW");

  /* _template_space_region_type_toggle(params, toolbar_key=T, sidebar_key=N).
   * Con el menu radial de regiones activo la plantilla emite solo el radial, con la
   * tecla de la barra lateral, y no las dos alternancias. */
  if (params.use_region_toggle_pie) {
    item_menu_pie(km, "WM_MT_region_toggle_pie", ev("N", "PRESS"));
  }
  else {
    item(km, "wm.context_toggle", ev("T", "PRESS"))
        .string("data_path", "space_data.show_region_toolbar");
    item(km, "wm.context_toggle", ev("N", "PRESS"))
        .string("data_path", "space_data.show_region_ui");
  }

  item(km, "clip.open", ev("O", "PRESS").alt());
  item(km, "clip.track_markers", ev("LEFT_ARROW", "PRESS").alt().repeat())
      .boolean("backwards", true)
      .boolean("sequence", false);
  item(km, "clip.track_markers", ev("RIGHT_ARROW", "PRESS").alt().repeat())
      .boolean("backwards", false)
      .boolean("sequence", false);
  item(km, "clip.track_markers", ev("T", "PRESS").ctrl())
      .boolean("backwards", false)
      .boolean("sequence", true);
  item(km, "clip.track_markers", ev("T", "PRESS").shift().ctrl())
      .boolean("backwards", true)
      .boolean("sequence", true);
  item(km, "wm.context_toggle_enum", ev("TAB", "PRESS"))
      .string("data_path", "space_data.mode")
      .string("value_1", "TRACKING")
      .string("value_2", "MASK");
  item(km, "clip.prefetch", ev("P", "PRESS"));
  item_menu_pie(km, "CLIP_MT_tracking_pie", ev("E", "PRESS"));
  item_menu_pie(km, "CLIP_MT_solving_pie", ev("S", "PRESS").shift());
  item_menu_pie(km, "CLIP_MT_marker_pie", ev("E", "PRESS").shift());
  item_menu_pie(km, "CLIP_MT_reconstruction_pie", ev("W", "PRESS").shift());
  item_menu_pie(km, "CLIP_MT_view_pie", ev("ACCENT_GRAVE", "PRESS"));
}

static void km_clip_editor(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Clip Editor", "CLIP_EDITOR", "WINDOW");

  item(km, "clip.view_pan", ev("MIDDLEMOUSE", "PRESS"));
  item(km, "clip.view_pan", ev("MIDDLEMOUSE", "PRESS").shift());
  item(km, "clip.view_pan", ev("TRACKPADPAN", "ANY"));
  item(km, "clip.view_zoom", ev("MIDDLEMOUSE", "PRESS").ctrl());
  item(km, "clip.view_zoom", ev("TRACKPADZOOM", "ANY"));
  item(km, "clip.view_zoom", ev("TRACKPADPAN", "ANY").ctrl());
  item(km, "clip.view_zoom_in", ev("WHEELINMOUSE", "PRESS"));
  item(km, "clip.view_zoom_out", ev("WHEELOUTMOUSE", "PRESS"));
  item(km, "clip.view_zoom_in", ev("NUMPAD_PLUS", "PRESS").repeat());
  item(km, "clip.view_zoom_out", ev("NUMPAD_MINUS", "PRESS").repeat());
  item(km, "clip.view_zoom_ratio", ev("NUMPAD_8", "PRESS").ctrl()).number("ratio", 8.0f);
  item(km, "clip.view_zoom_ratio", ev("NUMPAD_4", "PRESS").ctrl()).number("ratio", 4.0f);
  item(km, "clip.view_zoom_ratio", ev("NUMPAD_2", "PRESS").ctrl()).number("ratio", 2.0f);
  item(km, "clip.view_zoom_ratio", ev("NUMPAD_8", "PRESS").shift()).number("ratio", 8.0f);
  item(km, "clip.view_zoom_ratio", ev("NUMPAD_4", "PRESS").shift()).number("ratio", 4.0f);
  item(km, "clip.view_zoom_ratio", ev("NUMPAD_2", "PRESS").shift()).number("ratio", 2.0f);
  item(km, "clip.view_zoom_ratio", ev("NUMPAD_1", "PRESS")).number("ratio", 1.0f);
  item(km, "clip.view_zoom_ratio", ev("NUMPAD_2", "PRESS")).number("ratio", 0.5f);
  item(km, "clip.view_zoom_ratio", ev("NUMPAD_4", "PRESS")).number("ratio", 0.25f);
  item(km, "clip.view_zoom_ratio", ev("NUMPAD_8", "PRESS")).number("ratio", 0.125f);
  item(km, "clip.view_all", ev("HOME", "PRESS"));
  item(km, "clip.view_all", ev("F", "PRESS")).boolean("fit_view", true);
  item(km, "clip.view_selected", ev("NUMPAD_PERIOD", "PRESS"));
  item(km, "clip.view_all", ev("NDOF_BUTTON_FIT", "PRESS"));
  item(km, "clip.view_ndof", ev("NDOF_MOTION", "ANY"));
  item(km, "clip.frame_jump", ev("LEFT_ARROW", "PRESS").shift().ctrl().repeat())
      .enum_("position", "PATHSTART");
  item(km, "clip.frame_jump", ev("RIGHT_ARROW", "PRESS").shift().ctrl().repeat())
      .enum_("position", "PATHEND");
  item(km, "clip.frame_jump", ev("LEFT_ARROW", "PRESS").shift().alt().repeat())
      .enum_("position", "FAILEDPREV");
  item(km, "clip.frame_jump", ev("RIGHT_ARROW", "PRESS").shift().alt().repeat())
      .enum_("position", "PATHSTART");
  item(km, "clip.change_frame", ev("LEFTMOUSE", "PRESS"));
  item(km, "clip.select", ev(params.select_mouse, "PRESS"))
      .boolean("deselect_all", !params.legacy);
  item(km, "clip.select", ev(params.select_mouse, "PRESS").shift()).boolean("extend", true);

  /* _template_items_select_actions(params, "clip.select_all"). */
  if (!params.use_select_all_toggle) {
    item(km, "clip.select_all", ev("A", "PRESS")).enum_("action", "SELECT");
    item(km, "clip.select_all", ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, "clip.select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
    item(km, "clip.select_all", ev("A", "DOUBLE_CLICK")).enum_("action", "DESELECT");
  }
  else if (params.legacy) {
    /* En el keymap heredado Alt+A es la reproduccion. */
    item(km, "clip.select_all", ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, "clip.select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }
  else {
    item(km, "clip.select_all", ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, "clip.select_all", ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, "clip.select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }

  item(km, "clip.select_box", ev("B", "PRESS"));
  item(km, "clip.select_circle", ev("C", "PRESS"));
  item_menu(km, "CLIP_MT_select_grouped", ev("G", "PRESS").shift());
  item(km, "clip.select_lasso", ev(params.action_mouse, "CLICK_DRAG").ctrl().alt())
      .enum_("mode", "ADD");
  item(km, "clip.select_lasso", ev(params.action_mouse, "CLICK_DRAG").shift().ctrl().alt())
      .enum_("mode", "SUB");
  item(km, "clip.add_marker_slide", ev("LEFTMOUSE", "PRESS").ctrl());
  item(km, "clip.delete_marker", ev("X", "PRESS").shift());
  item(km, "clip.delete_marker", ev("DEL", "PRESS").shift());
  item(km, "clip.slide_marker", ev("LEFTMOUSE", "PRESS"));
  item(km, "clip.disable_markers", ev("D", "PRESS").shift()).enum_("action", "TOGGLE");
  item(km, "clip.delete_track", ev("X", "PRESS"));
  item(km, "clip.delete_track", ev("DEL", "PRESS"));
  item(km, "clip.lock_tracks", ev("L", "PRESS").ctrl()).enum_("action", "LOCK");
  item(km, "clip.lock_tracks", ev("L", "PRESS").alt()).enum_("action", "UNLOCK");

  /* _template_items_hide_reveal_actions("clip.hide_tracks", "clip.hide_tracks_clear"). */
  item(km, "clip.hide_tracks_clear", ev("H", "PRESS").alt());
  item(km, "clip.hide_tracks", ev("H", "PRESS")).boolean("unselected", false);
  item(km, "clip.hide_tracks", ev("H", "PRESS").shift()).boolean("unselected", true);

  item(km, "clip.slide_plane_marker", ev("LEFTMOUSE", "CLICK_DRAG"));
  item(km, "clip.keyframe_insert", ev("I", "PRESS"));
  item(km, "clip.keyframe_delete", ev("I", "PRESS").alt());
  item(km, "clip.join_tracks", ev("J", "PRESS").ctrl());
  item(km, "clip.lock_selection_toggle", ev("L", "PRESS"));
  item(km, "wm.context_toggle", ev("D", "PRESS").alt())
      .string("data_path", "space_data.show_disabled");
  item(km, "wm.context_toggle", ev("S", "PRESS").alt())
      .string("data_path", "space_data.show_marker_search");
  item(km, "wm.context_toggle", ev("M", "PRESS"))
      .string("data_path", "space_data.use_mute_footage");
  item(km, "transform.translate", ev("G", "PRESS"));
  item(km, "transform.translate", ev(params.select_mouse, "CLICK_DRAG"));
  item(km, "transform.resize", ev("S", "PRESS"));
  item(km, "transform.rotate", ev("R", "PRESS"));
  item(km, "clip.clear_track_path", ev("T", "PRESS").alt())
      .enum_("action", "REMAINED")
      .boolean("clear_active", false);
  item(km, "clip.clear_track_path", ev("T", "PRESS").shift())
      .enum_("action", "UPTO")
      .boolean("clear_active", false);
  item(km, "clip.clear_track_path", ev("T", "PRESS").shift().alt())
      .enum_("action", "ALL")
      .boolean("clear_active", false);
  item(km, "clip.cursor_set", params.cursor_set_event);
  item(km, "clip.copy_tracks", ev("C", "PRESS").ctrl());
  item(km, "clip.paste_tracks", ev("V", "PRESS").ctrl());
  /* _template_items_context_menu("CLIP_MT_tracking_context_menu", params.context_menu_event). */
  item_menu(km, "CLIP_MT_tracking_context_menu", params.context_menu_event);
  item_menu(km, "CLIP_MT_tracking_context_menu", ev("APP", "PRESS"));

  if (!params.legacy) {
    item_menu_pie(km, "CLIP_MT_pivot_pie", ev("PERIOD", "PRESS"));
  }
  else {
    /* Punto de pivote a la antigua. */
    item(km, "wm.context_set_enum", ev("COMMA", "PRESS"))
        .string("data_path", "space_data.pivot_point")
        .enum_("value", "BOUNDING_BOX_CENTER");
    item(km, "wm.context_set_enum", ev("COMMA", "PRESS").ctrl())
        .string("data_path", "space_data.pivot_point")
        .enum_("value", "MEDIAN_POINT");
    item(km, "wm.context_set_enum", ev("PERIOD", "PRESS"))
        .string("data_path", "space_data.pivot_point")
        .enum_("value", "CURSOR");
    item(km, "wm.context_set_enum", ev("PERIOD", "PRESS").ctrl())
        .string("data_path", "space_data.pivot_point")
        .enum_("value", "INDIVIDUAL_ORIGINS");

    item(km, "clip.view_center_cursor", ev("HOME", "PRESS").alt());
  }
}

/** \} */

void register_group_10(wmKeyConfig *kc, const Params &params)
{
  km_sequencer_channels(kc, params);
  km_console(kc, params);
  km_clip(kc, params);
  km_clip_editor(kc, params);
}

}  // namespace flipendo::keymap
