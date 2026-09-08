/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Mapa de teclado, grupo 09: editor de video (Video Sequence Editor, Sequencer)
 * y su vista previa (Preview), incluida la edicion de texto de las tiras.
 * Transliterado de blender_default.py.
 */

#include "FL_keymap_build.hpp"
#include "FL_keymap_params.hpp"

namespace flipendo::keymap {

/* `NUMBERS_1` del Python: la fila de numeros en orden fisico, o sea el 0 al final. */
static const char *const NUMBERS_1[10] = {
    "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE", "ZERO"};

/* `_template_items_select_actions()` del Python. Se repite dentro de este grupo
 * (Sequencer y Preview, las dos con "sequencer.select_all"), asi que se comparte en
 * vez de duplicar los tres condicionales. */
static void template_items_select_actions(wmKeyMap *km, const Params &params, const char *op)
{
  if (!params.use_select_all_toggle) {
    item(km, op, ev("A", "PRESS")).enum_("action", "SELECT");
    item(km, op, ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, op, ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
    item(km, op, ev("A", "DOUBLE_CLICK")).enum_("action", "DESELECT");
  }
  else if (params.legacy) {
    /* En el mapa antiguo Alt+A es la reproduccion, asi que no hay deseleccion aqui. */
    item(km, op, ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, op, ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }
  else {
    item(km, op, ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, op, ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, op, ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }
}

/* `_template_sequencer_generic_select()` del Python. La usan el Sequencer y, a
 * traves de `_template_sequencer_preview_select()`, la vista previa. */
static void template_sequencer_generic_select(wmKeyMap *km,
                                              const char *type,
                                              const char *value,
                                              const bool legacy)
{
  Item base = item(km, "sequencer.select", ev(type, value));
  if (!legacy) {
    base.boolean("deselect_all", true);
  }
  item(km, "sequencer.select", ev(type, value).shift()).boolean("toggle", true);
}

/* -------------------------------------------------------------------- */
/** \name Editor de video: atajos comunes a la linea de tiempo y la vista previa
 * \{ */

static void km_sequencer_generic(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Video Sequence Editor", "SEQUENCE_EDITOR", "WINDOW");

  /* `_template_space_region_type_toggle(params, toolbar_key=T, sidebar_key=N)`. */
  if (params.use_region_toggle_pie) {
    /* Con el radial de regiones activo, la tecla de la barra lateral (la primera no
     * nula del Python) es la que lo abre, y la de la barra de herramientas se pierde. */
    item_menu_pie(km, "WM_MT_region_toggle_pie", ev("N", "PRESS"));
  }
  else {
    item(km, "wm.context_toggle", ev("T", "PRESS"))
        .string("data_path", "space_data.show_region_toolbar");
    item(km, "wm.context_toggle", ev("N", "PRESS"))
        .string("data_path", "space_data.show_region_ui");
  }

  item(km, "wm.context_toggle", ev("O", "PRESS").shift())
      .string("data_path", "scene.sequence_editor.show_overlay_frame");
  /* value_1 y value_2 son cadenas en `wm.context_toggle_enum`, no enumeraciones. */
  item(km, "wm.context_toggle_enum", ev("TAB", "PRESS").ctrl())
      .string("data_path", "space_data.view_type")
      .string("value_1", "SEQUENCER")
      .string("value_2", "PREVIEW");
  item(km, "wm.context_toggle", ev("TAB", "PRESS").shift())
      .string("data_path", "tool_settings.use_snap_sequencer");
  item(km, "sequencer.refresh_all", ev("E", "PRESS").ctrl());

  if (!params.select_mouse_right && !params.legacy) {
    /* Cambio rapido a la herramienta de seleccion: con seleccion por el izquierdo no
     * se puede seleccionar comodamente con cualquier otra herramienta activa. */
    item_tool(km, "builtin.select_box", ev("W", "PRESS")).boolean("cycle", true);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Editor de video: linea de tiempo
 * \{ */

static void km_sequencer(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Sequencer", "SEQUENCE_EDITOR", "WINDOW");

  template_sequencer_generic_select(
      km, params.select_mouse, params.select_mouse_value_fallback, params.legacy);

  item(km, "sequencer.select", ev(params.select_mouse, "PRESS").ctrl())
      .boolean("linked_time", true);
  item(km, "sequencer.select", ev(params.select_mouse, "PRESS").ctrl().shift())
      .boolean("linked_time", true)
      .boolean("extend", true);
  item(km, "sequencer.select", ev(params.select_mouse, "CLICK").ctrl())
      .boolean("side_of_frame", true);
  item(km, "sequencer.select", ev(params.select_mouse, "PRESS").alt())
      .boolean("deselect_all", true)
      .boolean("ignore_connections", true);
  item(km, "sequencer.select", ev(params.select_mouse, "PRESS").alt().shift())
      .boolean("toggle", true)
      .boolean("ignore_connections", true);
  item(km, "sequencer.select_more", ev("NUMPAD_PLUS", "PRESS").ctrl().repeat());
  item(km, "sequencer.select_less", ev("NUMPAD_MINUS", "PRESS").ctrl().repeat());
  item(km, "sequencer.select_linked_pick", ev("L", "PRESS"));
  item(km, "sequencer.select_linked_pick", ev("L", "PRESS").shift()).boolean("extend", true);
  item(km, "sequencer.select_linked", ev("L", "PRESS").ctrl());
  item(km, "sequencer.select_box", ev(params.select_mouse, "CLICK_DRAG"))
      .boolean("tweak", true)
      .enum_("mode", "SET");
  item(km, "sequencer.select_box", ev(params.select_mouse, "CLICK_DRAG").shift())
      .boolean("tweak", true)
      .enum_("mode", "ADD");
  item(km, "sequencer.select_box", ev(params.select_mouse, "CLICK_DRAG").ctrl())
      .boolean("tweak", true)
      .enum_("mode", "SUB");
  item(km, "sequencer.select_box", ev(params.select_mouse, "CLICK_DRAG").alt())
      .boolean("tweak", true)
      .boolean("ignore_connections", true)
      .enum_("mode", "SET");
  item(km, "sequencer.select_box", ev("B", "PRESS"));
  item(km, "sequencer.select_box", ev("B", "PRESS").ctrl()).boolean("include_handles", true);
  item(km, "sequencer.select_grouped", ev("G", "PRESS").shift());

  template_items_select_actions(km, params, "sequencer.select_all");

  item(km, "sequencer.split", ev("K", "PRESS")).enum_("type", "SOFT");
  item(km, "sequencer.split", ev("K", "PRESS").shift()).enum_("type", "HARD");
  item(km, "sequencer.mute", ev("H", "PRESS")).boolean("unselected", false);
  item(km, "sequencer.mute", ev("H", "PRESS").shift()).boolean("unselected", true);
  item(km, "sequencer.unmute", ev("H", "PRESS").alt()).boolean("unselected", false);
  item(km, "sequencer.unmute", ev("H", "PRESS").shift().alt()).boolean("unselected", true);
  item(km, "sequencer.lock", ev("H", "PRESS").ctrl());
  item(km, "sequencer.unlock", ev("H", "PRESS").ctrl().alt());
  item(km, "sequencer.connect", ev("C", "PRESS").ctrl().alt()).boolean("toggle", true);
  item(km, "sequencer.reassign_inputs", ev("R", "PRESS"));
  item(km, "sequencer.reload", ev("R", "PRESS").alt());
  item(km, "sequencer.reload", ev("R", "PRESS").shift().alt()).boolean("adjust_length", true);
  item(km, "sequencer.offset_clear", ev("O", "PRESS").alt());
  item(km, "sequencer.duplicate_move", ev("D", "PRESS").shift());
  item(km, "sequencer.retiming_key_delete", ev("X", "PRESS"));
  item(km, "sequencer.retiming_key_delete", ev("DEL", "PRESS"));
  item(km, "sequencer.delete", ev("X", "PRESS"));
  item(km, "sequencer.delete", ev("DEL", "PRESS"));
  item(km, "sequencer.copy", ev("C", "PRESS").ctrl());
  item(km, "sequencer.paste", ev("V", "PRESS").ctrl());
  item(km, "sequencer.paste", ev("V", "PRESS").ctrl().shift()).boolean("keep_offset", true);
  item(km, "sequencer.images_separate", ev("Y", "PRESS"));
  item(km, "sequencer.meta_toggle", ev("TAB", "PRESS"));
  item(km, "sequencer.meta_make", ev("G", "PRESS").ctrl());
  item(km, "sequencer.meta_separate", ev("G", "PRESS").ctrl().alt());
  item(km, "sequencer.view_all", ev("HOME", "PRESS"));
  item(km, "sequencer.view_all", ev("NDOF_BUTTON_FIT", "PRESS"));
  item(km, "sequencer.view_selected", ev("NUMPAD_PERIOD", "PRESS"));
  item(km, "sequencer.view_frame", ev("NUMPAD_0", "PRESS"));
  item(km, "sequencer.strip_jump", ev("PAGE_UP", "PRESS").repeat())
      .boolean("next", true)
      .boolean("center", false);
  item(km, "sequencer.strip_jump", ev("PAGE_DOWN", "PRESS").repeat())
      .boolean("next", false)
      .boolean("center", false);
  item(km, "sequencer.strip_jump", ev("PAGE_UP", "PRESS").alt().repeat())
      .boolean("next", true)
      .boolean("center", true);
  item(km, "sequencer.strip_jump", ev("PAGE_DOWN", "PRESS").alt().repeat())
      .boolean("next", false)
      .boolean("center", true);
  item(km, "sequencer.swap", ev("LEFT_ARROW", "PRESS").alt().repeat()).enum_("side", "LEFT");
  item(km, "sequencer.swap", ev("RIGHT_ARROW", "PRESS").alt().repeat()).enum_("side", "RIGHT");
  item(km, "sequencer.gap_remove", ev("BACK_SPACE", "PRESS")).boolean("all", false);
  item(km, "sequencer.gap_remove", ev("BACK_SPACE", "PRESS").shift()).boolean("all", true);
  item(km, "sequencer.gap_insert", ev("EQUAL", "PRESS").shift());
  item(km, "sequencer.snap", ev("S", "PRESS").shift());
  item(km, "sequencer.swap_inputs", ev("S", "PRESS").alt());

  /* Una camara por tecla de la fila de numeros; la camara empieza en 1. */
  for (int i = 0; i < 10; i++) {
    item(km, "sequencer.split_multicam", ev(NUMBERS_1[i], "PRESS")).integer("camera", i + 1);
  }

  item_menu(km, "SEQUENCER_MT_add", ev("A", "PRESS").shift());
  item_menu(km, "SEQUENCER_MT_change", ev("C", "PRESS"));
  item_menu_pie(km, "SEQUENCER_MT_view_pie", ev("ACCENT_GRAVE", "PRESS"));
  item(km, "sequencer.slip", ev("S", "PRESS"));
  item(km, "wm.context_set_int", ev("O", "PRESS"))
      .string("data_path", "scene.sequence_editor.overlay_frame")
      .integer("value", 0);
  item(km, "transform.seq_slide", ev("G", "PRESS")).boolean("view2d_edge_pan", true);
  item(km, "transform.seq_slide", ev(params.select_mouse, "CLICK_DRAG"))
      .boolean("view2d_edge_pan", true)
      .boolean("use_restore_handle_selection", true);
  item(km, "transform.seq_slide", ev(params.select_mouse, "CLICK_DRAG").alt())
      .boolean("view2d_edge_pan", true)
      .boolean("use_restore_handle_selection", true);
  item(km, "transform.seq_slide", ev(params.select_mouse, "CLICK_DRAG").ctrl())
      .boolean("view2d_edge_pan", true)
      .boolean("use_restore_handle_selection", true);
  item(km, "transform.transform", ev("E", "PRESS")).enum_("mode", "TIME_EXTEND");
  item(km, "marker.add", ev("M", "PRESS"));
  item(km, "sequencer.select_side_of_frame", ev("LEFT_BRACKET", "PRESS")).enum_("side", "LEFT");
  item(km, "sequencer.select_side_of_frame", ev("RIGHT_BRACKET", "PRESS")).enum_("side", "RIGHT");
  item(km, "wm.context_toggle", ev("Z", "PRESS").alt().shift())
      .string("data_path", "space_data.show_overlays");

  /* `_template_items_context_menu("SEQUENCER_MT_context_menu", params.context_menu_event)`. */
  item_menu(km, "SEQUENCER_MT_context_menu", params.context_menu_event);
  item_menu(km, "SEQUENCER_MT_context_menu", ev("APP", "PRESS"));

  item_menu(km, "SEQUENCER_MT_retiming", ev("I", "PRESS"));
  item(km, "sequencer.retiming_segment_speed_set", ev("R", "PRESS"));
  item(km, "sequencer.retiming_show", ev("R", "PRESS").ctrl());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Editor de video: vista previa
 * \{ */

/* `_seq_preview_text_edit_cursor_move()` del Python: por cada tecla, el movimiento
 * del cursor de texto y su gemelo con Shift, que ademas extiende la seleccion. */
static void seq_preview_text_edit_cursor_move(wmKeyMap *km)
{
  struct Move {
    const char *type;
    bool ctrl;
    const char *value;
  };
  static const Move moves[] = {
      {"LEFT_ARROW", false, "PREVIOUS_CHARACTER"},
      {"RIGHT_ARROW", false, "NEXT_CHARACTER"},
      {"UP_ARROW", false, "PREVIOUS_LINE"},
      {"DOWN_ARROW", false, "NEXT_LINE"},
      {"HOME", false, "LINE_BEGIN"},
      {"END", false, "LINE_END"},
      {"LEFT_ARROW", true, "PREVIOUS_WORD"},
      {"RIGHT_ARROW", true, "NEXT_WORD"},
      {"PAGE_UP", false, "TEXT_BEGIN"},
      {"PAGE_DOWN", false, "TEXT_END"},
  };

  for (const Move &move : moves) {
    Event e = ev(move.type, "PRESS").repeat();
    if (move.ctrl) {
      e.ctrl();
    }
    item(km, "sequencer.text_cursor_move", e).enum_("type", move.value);

    Event e_select = ev(move.type, "PRESS").shift().repeat();
    if (move.ctrl) {
      e_select.ctrl();
    }
    item(km, "sequencer.text_cursor_move", e_select)
        .enum_("type", move.value)
        .boolean("select_text", true);
  }
}

static void km_sequencer_preview(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Preview", "SEQUENCE_EDITOR", "WINDOW");

  /* Edicion de texto. */
  seq_preview_text_edit_cursor_move(km);
  item(km, "sequencer.text_delete", ev("DEL", "PRESS").repeat())
      .enum_("type", "NEXT_OR_SELECTION");
  item(km, "sequencer.text_delete", ev("BACK_SPACE", "PRESS").repeat())
      .enum_("type", "PREVIOUS_OR_SELECTION");
  item(km, "sequencer.text_line_break", ev("RET", "PRESS").repeat());
  item(km, "sequencer.text_line_break", ev("NUMPAD_ENTER", "PRESS").repeat());
  item(km, "sequencer.text_select_all", ev("A", "PRESS").ctrl());
  item(km, "sequencer.text_deselect_all", ev("ESC", "PRESS"));
  item(km, "sequencer.text_edit_mode_toggle", ev("TAB", "PRESS"));
  item(km, "sequencer.text_edit_copy", ev("C", "PRESS").ctrl());
  item(km, "sequencer.text_edit_paste", ev("V", "PRESS").ctrl());
  item(km, "sequencer.text_edit_cut", ev("X", "PRESS").ctrl());
  item(km, "sequencer.text_insert", ev("TEXTINPUT", "ANY").any().repeat());

  /* Seleccion. `_template_sequencer_preview_select()`: la generica y, encima, las
   * combinaciones propias de la vista previa. */
  template_sequencer_generic_select(
      km, params.select_mouse, params.select_mouse_value_fallback, params.legacy);
  item(km, "sequencer.select", ev(params.select_mouse, params.select_mouse_value_fallback).ctrl())
      .boolean("center", true);
  item(km, "sequencer.select", ev(params.select_mouse, params.select_mouse_value_fallback).alt())
      .boolean("ignore_connections", true);
  item(km,
       "sequencer.select",
       ev(params.select_mouse, params.select_mouse_value_fallback).shift().ctrl())
      .boolean("toggle", true)
      .boolean("center", true);
  item(km,
       "sequencer.select",
       ev(params.select_mouse, params.select_mouse_value_fallback).shift().alt())
      .boolean("toggle", true)
      .boolean("ignore_connections", true);

  template_items_select_actions(km, params, "sequencer.select_all");
  item(km, "sequencer.select_box", ev("B", "PRESS"));

  /* Vista. */
  item(km, "sequencer.view_selected", ev("NUMPAD_PERIOD", "PRESS"));
  item(km, "sequencer.view_all_preview", ev("HOME", "PRESS"));
  item(km, "sequencer.view_all_preview", ev("NDOF_BUTTON_FIT", "PRESS"));
  item(km, "sequencer.view_ghost_border", ev("O", "PRESS"));
  item(km, "sequencer.view_zoom_ratio", ev("NUMPAD_8", "PRESS").ctrl()).number("ratio", 8.0f);
  item(km, "sequencer.view_zoom_ratio", ev("NUMPAD_4", "PRESS").ctrl()).number("ratio", 4.0f);
  item(km, "sequencer.view_zoom_ratio", ev("NUMPAD_2", "PRESS").ctrl()).number("ratio", 2.0f);
  item(km, "sequencer.view_zoom_ratio", ev("NUMPAD_1", "PRESS")).number("ratio", 1.0f);
  item(km, "sequencer.view_zoom_ratio", ev("NUMPAD_2", "PRESS")).number("ratio", 0.5f);
  item(km, "sequencer.view_zoom_ratio", ev("NUMPAD_4", "PRESS")).number("ratio", 0.25f);
  item(km, "sequencer.view_zoom_ratio", ev("NUMPAD_8", "PRESS")).number("ratio", 0.125f);
  item_menu_pie(km, "SEQUENCER_MT_preview_view_pie", ev("ACCENT_GRAVE", "PRESS"));

  /* Transformaciones. `_template_items_transform_actions(params, use_mirror=True)`,
   * con `op_tool_optional()`: si el usuario activa herramientas con la tecla, la
   * misma tecla cambia de herramienta en vez de lanzar la transformacion. */
  if (params.use_key_activate_tools) {
    item_tool(km, "builtin.move", ev("G", "PRESS")).boolean("cycle", true);
  }
  else {
    item(km, "transform.translate", ev("G", "PRESS"));
  }
  if (params.use_key_activate_tools) {
    item_tool(km, "builtin.rotate", ev("R", "PRESS")).boolean("cycle", true);
  }
  else {
    item(km, "transform.rotate", ev("R", "PRESS"));
  }
  if (params.use_key_activate_tools) {
    item_tool(km, "builtin.scale", ev("S", "PRESS")).boolean("cycle", true);
  }
  else {
    item(km, "transform.resize", ev("S", "PRESS"));
  }
  item(km, "transform.translate", ev(params.select_mouse, "CLICK_DRAG"));
  item(km, "transform.mirror", ev("M", "PRESS").ctrl());

  item(km, "transform.translate", ev("PERIOD", "PRESS").ctrl()).boolean("translate_origin", true);

  /* Edicion. */
  item(km, "sequencer.strip_transform_clear", ev("G", "PRESS").alt()).enum_("property", "POSITION");
  item(km, "sequencer.strip_transform_clear", ev("S", "PRESS").alt()).enum_("property", "SCALE");
  item(km, "sequencer.strip_transform_clear", ev("R", "PRESS").alt()).enum_("property", "ROTATION");

  item(km, "sequencer.preview_duplicate_move", ev("D", "PRESS").shift());
  item(km, "sequencer.mute", ev("H", "PRESS")).boolean("unselected", false);
  item(km, "sequencer.mute", ev("H", "PRESS").shift()).boolean("unselected", true);
  item(km, "sequencer.unmute", ev("H", "PRESS").alt()).boolean("unselected", false);
  item(km, "sequencer.delete", ev("X", "PRESS"));
  item(km, "sequencer.delete", ev("DEL", "PRESS"));

  /* `_template_items_context_menu("SEQUENCER_MT_preview_context_menu", ...)`. */
  item_menu(km, "SEQUENCER_MT_preview_context_menu", params.context_menu_event);
  item_menu(km, "SEQUENCER_MT_preview_context_menu", ev("APP", "PRESS"));

  if (!params.legacy) {
    /* Menus radiales nuevos. */
    item(km, "wm.context_toggle", ev("ACCENT_GRAVE", "PRESS").ctrl())
        .string("data_path", "space_data.show_gizmo");
    item_menu_pie(km, "SEQUENCER_MT_pivot_pie", ev("PERIOD", "PRESS"));
    item(km, "wm.context_toggle", ev("Z", "PRESS").alt().shift())
        .string("data_path", "space_data.show_overlays");
  }

  /* Cursor 2D. El Python comprueba si `cursor_tweak_event` existe; aqui lo dice
   * `has_cursor_tweak_event`. */
  if (params.has_cursor_tweak_event) {
    item(km, "sequencer.cursor_set", params.cursor_set_event);
    item(km, "transform.translate", params.cursor_tweak_event)
        .boolean("release_confirm", true)
        .boolean("cursor_transform", true);
  }
  else {
    item(km, "sequencer.cursor_set", params.cursor_set_event);
  }
}

/** \} */

void register_group_09(wmKeyConfig *kc, const Params &params)
{
  km_sequencer_generic(kc, params);
  km_sequencer(kc, params);
  km_sequencer_preview(kc, params);
}

}  // namespace flipendo::keymap
