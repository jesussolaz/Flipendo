/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Mapa de teclado, grupo 08: editor de NLA (canales y area principal) y editor de
 * texto (atajos generales del espacio y edicion del texto).
 * Transliterado de blender_default.py.
 */

#include "FL_keymap_build.hpp"
#include "FL_keymap_params.hpp"

namespace flipendo::keymap {

/* -------------------------------------------------------------------- */
/** \name Editor (NLA)
 * \{ */

static void km_nla_tracks(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "NLA Tracks", "NLA_EDITOR", "WINDOW");

  item(km, "nla.channels_click", ev("LEFTMOUSE", "PRESS"));
  item(km, "nla.channels_click", ev("LEFTMOUSE", "PRESS").shift()).boolean("extend", true);
  item(km, "nla.tracks_add", ev("A", "PRESS").shift()).boolean("above_selected", false);
  item(km, "nla.tracks_add", ev("A", "PRESS").shift().ctrl()).boolean("above_selected", true);
  item(km, "nla.tracks_delete", ev("X", "PRESS"));
  item(km, "nla.tracks_delete", ev("DEL", "PRESS"));

  /* _template_items_context_menu("NLA_MT_channel_context_menu", params.context_menu_event) */
  item_menu(km, "NLA_MT_channel_context_menu", params.context_menu_event);
  item_menu(km, "NLA_MT_channel_context_menu", ev("APP", "PRESS"));
}

static void km_nla_editor(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "NLA Editor", "NLA_EDITOR", "WINDOW");

  item(km, "nla.click_select", ev(params.select_mouse, "PRESS"))
      .boolean("deselect_all", !params.legacy);
  item(km, "nla.click_select", ev(params.select_mouse, "PRESS").shift()).boolean("extend", true);
  item(km,
       "nla.select_leftright",
       ev(params.select_mouse, params.legacy ? "PRESS" : "CLICK").ctrl())
      .enum_("mode", "CHECK");
  item(km,
       "nla.select_leftright",
       ev(params.select_mouse, params.legacy ? "PRESS" : "CLICK").ctrl().shift())
      .enum_("mode", "CHECK")
      .boolean("extend", true);
  item(km, "nla.select_leftright", ev("LEFT_BRACKET", "PRESS")).enum_("mode", "LEFT");
  item(km, "nla.select_leftright", ev("RIGHT_BRACKET", "PRESS")).enum_("mode", "RIGHT");

  /* _template_items_select_actions(params, "nla.select_all") */
  if (!params.use_select_all_toggle) {
    item(km, "nla.select_all", ev("A", "PRESS")).enum_("action", "SELECT");
    item(km, "nla.select_all", ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, "nla.select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
    item(km, "nla.select_all", ev("A", "DOUBLE_CLICK")).enum_("action", "DESELECT");
  }
  else if (params.legacy) {
    /* En el keymap legacy Alt+A es reproducir, por eso ahi no hay "deseleccionar". */
    item(km, "nla.select_all", ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, "nla.select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }
  else {
    item(km, "nla.select_all", ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, "nla.select_all", ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, "nla.select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }

  item(km, "nla.select_box", ev("B", "PRESS")).boolean("axis_range", false);
  item(km, "nla.select_box", ev("B", "PRESS").alt()).boolean("axis_range", true);
  item(km, "nla.select_box", ev(params.select_mouse, "CLICK_DRAG"))
      .boolean("tweak", true)
      .enum_("mode", "SET");
  item(km, "nla.select_box", ev(params.select_mouse, "CLICK_DRAG").shift())
      .boolean("tweak", true)
      .enum_("mode", "ADD");
  item(km, "nla.select_box", ev(params.select_mouse, "CLICK_DRAG").ctrl())
      .boolean("tweak", true)
      .enum_("mode", "SUB");
  item(km, "nla.previewrange_set", ev("P", "PRESS").ctrl().alt());
  item(km, "nla.view_all", ev("HOME", "PRESS"));
  item(km, "nla.view_all", ev("NDOF_BUTTON_FIT", "PRESS"));
  item(km, "nla.view_selected", ev("NUMPAD_PERIOD", "PRESS"));
  item(km, "nla.view_frame", ev("NUMPAD_0", "PRESS"));
  item_menu_pie(km, "NLA_MT_view_pie", ev("ACCENT_GRAVE", "PRESS"));
  item(km, "nla.actionclip_add", ev("A", "PRESS").shift());
  item(km, "nla.transition_add", ev("T", "PRESS").shift());
  item(km, "nla.soundclip_add", ev("K", "PRESS").shift());
  item(km, "nla.meta_add", ev("G", "PRESS").ctrl());
  item(km, "nla.meta_remove", ev("G", "PRESS").ctrl().alt());
  item(km, "nla.duplicate_linked_move", ev("D", "PRESS").shift());
  item(km, "nla.duplicate_move", ev("D", "PRESS").alt());
  item(km, "nla.make_single_user", ev("U", "PRESS"));
  item(km, "nla.delete", ev("X", "PRESS"));
  item(km, "nla.delete", ev("DEL", "PRESS"));
  item(km, "nla.split", ev("Y", "PRESS"));
  item(km, "nla.mute_toggle", ev("H", "PRESS"));
  item(km, "nla.swap", ev("F", "PRESS").alt());
  item(km, "nla.move_up", ev("PAGE_UP", "PRESS").repeat());
  item(km, "nla.move_down", ev("PAGE_DOWN", "PRESS").repeat());
  item(km, "nla.apply_scale", ev("A", "PRESS").ctrl());
  item(km, "nla.clear_scale", ev("S", "PRESS").alt());

  if (!params.legacy) {
    item_menu_pie(km, "NLA_MT_snap_pie", ev("S", "PRESS").shift());
  }
  else {
    item(km, "nla.snap", ev("S", "PRESS").shift());
  }

  item(km, "nla.fmodifier_add", ev("M", "PRESS").shift().ctrl());
  item(km, "transform.transform", ev("G", "PRESS")).enum_("mode", "TRANSLATION");
  item(km, "transform.transform", ev(params.select_mouse, "CLICK_DRAG"))
      .enum_("mode", "TRANSLATION");
  item(km, "transform.transform", ev("E", "PRESS")).enum_("mode", "TIME_EXTEND");
  item(km, "transform.transform", ev("S", "PRESS")).enum_("mode", "TIME_SCALE");
  item(km, "marker.add", ev("M", "PRESS"));

  /* _template_items_context_menu("NLA_MT_context_menu", params.context_menu_event) */
  item_menu(km, "NLA_MT_context_menu", params.context_menu_event);
  item_menu(km, "NLA_MT_context_menu", ev("APP", "PRESS"));

  /* `params.select_mouse == 'LEFTMOUSE'` se comprueba con `!params.select_mouse_right`. */
  if (!params.select_mouse_right && !params.legacy) {
    item(km, "anim.change_frame", ev("RIGHTMOUSE", "PRESS").shift())
        .boolean("seq_solo_preview", true);
  }
  else {
    item(km, "anim.change_frame", ev(params.action_mouse, "PRESS"))
        .boolean("seq_solo_preview", true);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Editor (Text)
 * \{ */

static void km_text_generic(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Text Generic", "TEXT_EDITOR", "WINDOW");

  /* _template_space_region_type_toggle(params, sidebar_key={'T', 'PRESS', ctrl}).
   * Solo se pasa `sidebar_key`, asi que la variante de menu radial usa esa misma tecla
   * y la variante normal genera un unico `wm.context_toggle`. */
  if (params.use_region_toggle_pie) {
    item_menu_pie(km, "WM_MT_region_toggle_pie", ev("T", "PRESS").ctrl());
  }
  else {
    item(km, "wm.context_toggle", ev("T", "PRESS").ctrl())
        .string("data_path", "space_data.show_region_ui");
  }

  item(km, "text.start_find", ev("F", "PRESS").ctrl());
  item(km, "text.jump", ev("J", "PRESS").ctrl());
  item(km, "text.find_set_selected", ev("G", "PRESS").ctrl());
  item(km, "text.replace", ev("H", "PRESS").ctrl());
}

static void km_text(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Text", "TEXT_EDITOR", "WINDOW");

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

  if (!params.legacy) {
    item(km, "text.new", ev("N", "PRESS").alt());
  }
  else {
    item(km, "text.new", ev("N", "PRESS").ctrl());

    item(km, "text.move", ev("LEFT_ARROW", "PRESS").alt().repeat())
        .enum_("type", "PREVIOUS_WORD");
    item(km, "text.move", ev("RIGHT_ARROW", "PRESS").alt().repeat()).enum_("type", "NEXT_WORD");
  }

  item(km, "text.open", ev("O", "PRESS").alt());
  item(km, "text.reload", ev("R", "PRESS").alt());
  item(km, "text.save", ev("S", "PRESS").alt());
  item(km, "text.save_as", ev("S", "PRESS").shift().ctrl().alt());
  item(km, "text.run_script", ev("P", "PRESS").alt());
  item(km, "text.cut", ev("X", "PRESS").ctrl());
  item(km, "text.copy", ev("C", "PRESS").ctrl());
  item(km, "text.paste", ev("V", "PRESS").ctrl().repeat());
  item(km, "text.cut", ev("DEL", "PRESS").shift());
  item(km, "text.copy", ev("INSERT", "PRESS").ctrl());
  item(km, "text.paste", ev("INSERT", "PRESS").shift().repeat());
  item(km, "text.duplicate_line", ev("D", "PRESS").ctrl().repeat());
  item(km, "text.select_all", ev("A", "PRESS").ctrl());
  item(km, "text.select_line", ev("A", "PRESS").shift().ctrl());
  item(km, "text.select_word", ev("LEFTMOUSE", "DOUBLE_CLICK"));
  item(km, "text.move_lines", ev("UP_ARROW", "PRESS").shift().ctrl().repeat())
      .enum_("direction", "UP");
  item(km, "text.move_lines", ev("DOWN_ARROW", "PRESS").shift().ctrl().repeat())
      .enum_("direction", "DOWN");
  item(km, "text.indent_or_autocomplete", ev("TAB", "PRESS").repeat());
  item(km, "text.unindent", ev("TAB", "PRESS").shift().repeat());
  item(km, "text.comment_toggle", ev("SLASH", "PRESS").ctrl());
  item(km, "text.move", ev("HOME", "PRESS")).enum_("type", "LINE_BEGIN");
  item(km, "text.move", ev("END", "PRESS")).enum_("type", "LINE_END");
  item(km, "text.move", ev("E", "PRESS").ctrl()).enum_("type", "LINE_END");
  item(km, "text.move", ev("E", "PRESS").shift().ctrl()).enum_("type", "LINE_END");
  item(km, "text.move", ev("LEFT_ARROW", "PRESS").repeat()).enum_("type", "PREVIOUS_CHARACTER");
  item(km, "text.move", ev("RIGHT_ARROW", "PRESS").repeat()).enum_("type", "NEXT_CHARACTER");
  item(km, "text.move", ev("LEFT_ARROW", "PRESS").ctrl().repeat()).enum_("type", "PREVIOUS_WORD");
  item(km, "text.move", ev("RIGHT_ARROW", "PRESS").ctrl().repeat()).enum_("type", "NEXT_WORD");
  item(km, "text.move", ev("UP_ARROW", "PRESS").repeat()).enum_("type", "PREVIOUS_LINE");
  item(km, "text.move", ev("DOWN_ARROW", "PRESS").repeat()).enum_("type", "NEXT_LINE");
  item(km, "text.move", ev("PAGE_UP", "PRESS").repeat()).enum_("type", "PREVIOUS_PAGE");
  item(km, "text.move", ev("PAGE_DOWN", "PRESS").repeat()).enum_("type", "NEXT_PAGE");
  item(km, "text.move", ev("HOME", "PRESS").ctrl()).enum_("type", "FILE_TOP");
  item(km, "text.move", ev("END", "PRESS").ctrl()).enum_("type", "FILE_BOTTOM");
  item(km, "text.move_select", ev("HOME", "PRESS").shift()).enum_("type", "LINE_BEGIN");
  item(km, "text.move_select", ev("END", "PRESS").shift()).enum_("type", "LINE_END");
  item(km, "text.move_select", ev("LEFT_ARROW", "PRESS").shift().repeat())
      .enum_("type", "PREVIOUS_CHARACTER");
  item(km, "text.move_select", ev("RIGHT_ARROW", "PRESS").shift().repeat())
      .enum_("type", "NEXT_CHARACTER");
  item(km, "text.move_select", ev("LEFT_ARROW", "PRESS").shift().ctrl().repeat())
      .enum_("type", "PREVIOUS_WORD");
  item(km, "text.move_select", ev("RIGHT_ARROW", "PRESS").shift().ctrl().repeat())
      .enum_("type", "NEXT_WORD");
  item(km, "text.move_select", ev("UP_ARROW", "PRESS").shift().repeat())
      .enum_("type", "PREVIOUS_LINE");
  item(km, "text.move_select", ev("DOWN_ARROW", "PRESS").shift().repeat())
      .enum_("type", "NEXT_LINE");
  item(km, "text.move_select", ev("PAGE_UP", "PRESS").shift().repeat())
      .enum_("type", "PREVIOUS_PAGE");
  item(km, "text.move_select", ev("PAGE_DOWN", "PRESS").shift().repeat())
      .enum_("type", "NEXT_PAGE");
  item(km, "text.move_select", ev("HOME", "PRESS").shift().ctrl()).enum_("type", "FILE_TOP");
  item(km, "text.move_select", ev("END", "PRESS").shift().ctrl()).enum_("type", "FILE_BOTTOM");
  item(km, "text.delete", ev("DEL", "PRESS").repeat()).enum_("type", "NEXT_CHARACTER");
  item(km, "text.delete", ev("BACK_SPACE", "PRESS").repeat())
      .enum_("type", "PREVIOUS_CHARACTER");
  item(km, "text.delete", ev("BACK_SPACE", "PRESS").shift().repeat())
      .enum_("type", "PREVIOUS_CHARACTER");
  item(km, "text.delete", ev("DEL", "PRESS").ctrl().repeat()).enum_("type", "NEXT_WORD");
  item(km, "text.delete", ev("BACK_SPACE", "PRESS").ctrl().repeat())
      .enum_("type", "PREVIOUS_WORD");
  item(km, "text.overwrite_toggle", ev("INSERT", "PRESS"));
  item(km, "text.scroll_bar", ev("LEFTMOUSE", "PRESS"));
  item(km, "text.scroll_bar", ev("MIDDLEMOUSE", "PRESS"));
  item(km, "text.scroll", ev("MIDDLEMOUSE", "PRESS"));
  item(km, "text.scroll", ev("TRACKPADPAN", "ANY"));
  item(km, "text.selection_set", ev("LEFTMOUSE", "CLICK_DRAG"));
  item(km, "text.cursor_set", ev("LEFTMOUSE", "PRESS"));
  item(km, "text.selection_set", ev("LEFTMOUSE", "PRESS").shift());
  item(km, "text.scroll", ev("WHEELUPMOUSE", "PRESS")).integer("lines", -1);
  item(km, "text.scroll", ev("WHEELDOWNMOUSE", "PRESS")).integer("lines", 1);
  item(km, "text.line_break", ev("RET", "PRESS").repeat());
  item(km, "text.line_break", ev("NUMPAD_ENTER", "PRESS").repeat());
  item(km, "text.line_number", ev("TEXTINPUT", "ANY").any().repeat());
  item_menu(km, "TEXT_MT_context_menu", ev("RIGHTMOUSE", "PRESS"));
  item(km, "text.insert", ev("TEXTINPUT", "ANY").any().repeat());
}

/** \} */

void register_group_08(wmKeyConfig *kc, const Params &params)
{
  km_nla_tracks(kc, params);
  km_nla_editor(kc, params);
  km_text_generic(kc, params);
  km_text(kc, params);
}

}  // namespace flipendo::keymap
