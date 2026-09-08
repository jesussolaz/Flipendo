/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Mapa de teclado, grupo 07: navegador de ficheros (ventana principal y botones),
 * editor de hoja de exposicion (Dope Sheet) y parte generica del editor NLA.
 * Transliterado de blender_default.py.
 */

#include "FL_keymap_build.hpp"
#include "FL_keymap_params.hpp"

namespace flipendo::keymap {

/* `_template_items_select_actions()` del Python. Se repite dentro de este grupo
 * (file.select_all y action.select_all), asi que se comparte en vez de duplicar los
 * tres condicionales. */
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

/* -------------------------------------------------------------------- */
/** \name Navegador de ficheros
 * \{ */

static void km_file_browser_main(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "File Browser Main", "FILE_BROWSER", "WINDOW");

  if (!params.use_file_single_click) {
    item(km, "file.select", ev("LEFTMOUSE", "DOUBLE_CLICK"))
        .boolean("open", true)
        .boolean("deselect_all", !params.legacy);
  }

  item(km, "file.mouse_execute", ev("LEFTMOUSE", "DOUBLE_CLICK"));
  /* Hacen falta .execute y .select a la vez: el primero solo funciona cuando hay un
   * operador de fichero (o sea, fuera del modo editor) pero es el que carga ficheros;
   * el segundo hace que la seleccion funcione cuando no hay operador. */
  item(km, "file.select", ev("LEFTMOUSE", "PRESS"))
      .boolean("open", params.use_file_single_click)
      .boolean("deselect_all", !params.legacy);
  item(km, "file.select", ev("LEFTMOUSE", "CLICK").ctrl())
      .boolean("extend", true)
      .boolean("open", false);
  item(km, "file.select", ev("LEFTMOUSE", "CLICK").shift())
      .boolean("extend", true)
      .boolean("fill", true)
      .boolean("open", false);
  item(km, "file.select_walk", ev("UP_ARROW", "PRESS").repeat()).enum_("direction", "UP");
  /* Ojo: este es el unico de los doce que NO lleva `repeat`. Es asi en el original y se
   * conserva tal cual para no cambiar el resultado. */
  item(km, "file.select_walk", ev("UP_ARROW", "PRESS").shift())
      .enum_("direction", "UP")
      .boolean("extend", true);
  item(km, "file.select_walk", ev("UP_ARROW", "PRESS").shift().ctrl().repeat())
      .enum_("direction", "UP")
      .boolean("extend", true)
      .boolean("fill", true);
  item(km, "file.select_walk", ev("DOWN_ARROW", "PRESS").repeat()).enum_("direction", "DOWN");
  item(km, "file.select_walk", ev("DOWN_ARROW", "PRESS").shift().repeat())
      .enum_("direction", "DOWN")
      .boolean("extend", true);
  item(km, "file.select_walk", ev("DOWN_ARROW", "PRESS").shift().ctrl().repeat())
      .enum_("direction", "DOWN")
      .boolean("extend", true)
      .boolean("fill", true);
  item(km, "file.select_walk", ev("LEFT_ARROW", "PRESS").repeat()).enum_("direction", "LEFT");
  item(km, "file.select_walk", ev("LEFT_ARROW", "PRESS").shift().repeat())
      .enum_("direction", "LEFT")
      .boolean("extend", true);
  item(km, "file.select_walk", ev("LEFT_ARROW", "PRESS").shift().ctrl().repeat())
      .enum_("direction", "LEFT")
      .boolean("extend", true)
      .boolean("fill", true);
  item(km, "file.select_walk", ev("RIGHT_ARROW", "PRESS").repeat()).enum_("direction", "RIGHT");
  item(km, "file.select_walk", ev("RIGHT_ARROW", "PRESS").shift().repeat())
      .enum_("direction", "RIGHT")
      .boolean("extend", true);
  item(km, "file.select_walk", ev("RIGHT_ARROW", "PRESS").shift().ctrl().repeat())
      .enum_("direction", "RIGHT")
      .boolean("extend", true)
      .boolean("fill", true);
  item(km, "file.previous", ev("BUTTON4MOUSE", "CLICK"));
  item(km, "file.next", ev("BUTTON5MOUSE", "CLICK"));
  template_items_select_actions(km, params, "file.select_all");
  item(km, "file.select_box", ev("B", "PRESS"));
  item(km, "file.select_box", ev("LEFTMOUSE", "CLICK_DRAG"));
  item(km, "file.select_box", ev("LEFTMOUSE", "CLICK_DRAG").shift()).enum_("mode", "ADD");
  item(km, "file.select_box", ev("LEFTMOUSE", "CLICK_DRAG").ctrl()).enum_("mode", "SUB");
  item(km, "file.highlight", ev("MOUSEMOVE", "ANY").any());
  item(km, "file.sort_column_ui_context", ev("LEFTMOUSE", "PRESS").any());
  item(km, "file.view_selected", ev("NUMPAD_PERIOD", "PRESS"));
  /* `_template_items_context_menu()`: la tecla del usuario mas la tecla de menu APP. */
  item_menu(km, "ASSETBROWSER_MT_context_menu", params.context_menu_event);
  item_menu(km, "ASSETBROWSER_MT_context_menu", ev("APP", "PRESS"));
}

static void km_file_browser_buttons(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap(kc, "File Browser Buttons", "FILE_BROWSER", "WINDOW");

  item(km, "file.filenum", ev("NUMPAD_PLUS", "PRESS").repeat()).integer("increment", 1);
  item(km, "file.filenum", ev("NUMPAD_PLUS", "PRESS").shift().repeat()).integer("increment", 10);
  item(km, "file.filenum", ev("NUMPAD_PLUS", "PRESS").ctrl().repeat()).integer("increment", 100);
  item(km, "file.filenum", ev("NUMPAD_MINUS", "PRESS").repeat()).integer("increment", -1);
  item(km, "file.filenum", ev("NUMPAD_MINUS", "PRESS").shift().repeat()).integer("increment", -10);
  item(km, "file.filenum", ev("NUMPAD_MINUS", "PRESS").ctrl().repeat()).integer("increment", -100);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Editor de hoja de exposicion (Dope Sheet)
 * \{ */

static void km_dopesheet_generic(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Dopesheet Generic", "DOPESHEET_EDITOR", "WINDOW");

  /* `_template_space_region_type_toggle(sidebar_key=N)`: con el menu radial activo la
   * misma tecla abre el radial en vez de conmutar solo la barra lateral. */
  if (params.use_region_toggle_pie) {
    item_menu_pie(km, "WM_MT_region_toggle_pie", ev("N", "PRESS"));
  }
  else {
    item(km, "wm.context_toggle", ev("N", "PRESS")).string("data_path", "space_data.show_region_ui");
  }

  item(km, "wm.context_set_enum", ev("TAB", "PRESS").ctrl())
      .string("data_path", "area.type")
      .string("value", "GRAPH_EDITOR");
  item(km, "action.extrapolation_type", ev("E", "PRESS").shift());
}

static void km_dopesheet(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Dopesheet", "DOPESHEET_EDITOR", "WINDOW");

  item(km, "action.clickselect", ev(params.select_mouse, "PRESS"))
      .boolean("deselect_all", !params.legacy);
  item(km, "action.clickselect", ev(params.select_mouse, "PRESS").alt()).boolean("column", true);
  item(km, "action.clickselect", ev(params.select_mouse, "PRESS").shift()).boolean("extend", true);
  item(km, "action.clickselect", ev(params.select_mouse, "PRESS").shift().alt())
      .boolean("extend", true)
      .boolean("column", true);
  item(km, "action.clickselect", ev(params.select_mouse, "PRESS").ctrl().alt())
      .boolean("channel", true);
  item(km, "action.clickselect", ev(params.select_mouse, "PRESS").shift().ctrl().alt())
      .boolean("extend", true)
      .boolean("channel", true);
  item(km,
       "action.select_leftright",
       ev(params.select_mouse, params.legacy ? "PRESS" : "CLICK").ctrl())
      .enum_("mode", "CHECK");
  item(km,
       "action.select_leftright",
       ev(params.select_mouse, params.legacy ? "PRESS" : "CLICK").ctrl().shift())
      .enum_("mode", "CHECK")
      .boolean("extend", true);
  item(km, "action.select_leftright", ev("LEFT_BRACKET", "PRESS")).enum_("mode", "LEFT");
  item(km, "action.select_leftright", ev("RIGHT_BRACKET", "PRESS")).enum_("mode", "RIGHT");
  template_items_select_actions(km, params, "action.select_all");
  item(km, "action.select_box", ev("B", "PRESS")).boolean("axis_range", false);
  item(km, "action.select_box", ev("B", "PRESS").alt()).boolean("axis_range", true);
  item(km, "action.select_box", ev(params.select_mouse, "CLICK_DRAG"))
      .boolean("tweak", true)
      .enum_("mode", "SET");
  item(km, "action.select_box", ev(params.select_mouse, "CLICK_DRAG").shift())
      .boolean("tweak", true)
      .enum_("mode", "ADD");
  item(km, "action.select_box", ev(params.select_mouse, "CLICK_DRAG").ctrl())
      .boolean("tweak", true)
      .enum_("mode", "SUB");
  item(km, "action.select_lasso", ev(params.action_mouse, "CLICK_DRAG").ctrl())
      .enum_("mode", "ADD");
  item(km, "action.select_lasso", ev(params.action_mouse, "CLICK_DRAG").shift().ctrl())
      .enum_("mode", "SUB");
  item(km, "action.select_circle", ev("C", "PRESS"));
  item(km, "action.select_column", ev("K", "PRESS")).enum_("mode", "KEYS");
  item(km, "action.select_column", ev("K", "PRESS").ctrl()).enum_("mode", "CFRA");
  item(km, "action.select_column", ev("K", "PRESS").shift()).enum_("mode", "MARKERS_COLUMN");
  item(km, "action.select_column", ev("K", "PRESS").alt()).enum_("mode", "MARKERS_BETWEEN");
  item(km, "action.select_more", ev("NUMPAD_PLUS", "PRESS").ctrl().repeat());
  item(km, "action.select_less", ev("NUMPAD_MINUS", "PRESS").ctrl().repeat());
  item(km, "action.select_linked", ev("L", "PRESS"));
  item(km, "action.frame_jump", ev("G", "PRESS").ctrl());
  if (!params.legacy) {
    item_menu_pie(km, "DOPESHEET_MT_snap_pie", ev("S", "PRESS").shift());
  }
  else {
    item(km, "action.snap", ev("S", "PRESS").shift());
  }
  item(km, "action.mirror", ev("M", "PRESS").ctrl());
  item(km, "action.handle_type", ev("V", "PRESS"));
  item(km, "action.interpolation_type", ev("T", "PRESS"));
  item(km, "action.extrapolation_type", ev("E", "PRESS").shift());
  item(km, "action.easing_type", ev("E", "PRESS").ctrl());
  item(km, "action.keyframe_type", ev("R", "PRESS"));
  item(km, "action.bake_keys", ev("O", "PRESS").shift().alt());
  item(km, "grease_pencil.layer_isolate", ev("NUMPAD_ASTERIX", "PRESS"));
  item_menu(km, "DOPESHEET_MT_delete", ev("X", "PRESS"));
  item(km, "action.delete", ev("DEL", "PRESS")).boolean("confirm", false);
  item(km, "action.duplicate_move", ev("D", "PRESS").shift());
  item(km, "action.keyframe_insert", ev("I", "PRESS"));
  item(km, "action.copy", ev("C", "PRESS").ctrl());
  item(km, "action.paste", ev("V", "PRESS").ctrl());
  item(km, "action.paste", ev("V", "PRESS").shift().ctrl()).boolean("flipped", true);
  item(km, "action.previewrange_set", ev("P", "PRESS").ctrl().alt());
  item(km, "action.view_all", ev("HOME", "PRESS"));
  item(km, "action.view_all", ev("NDOF_BUTTON_FIT", "PRESS"));
  item(km, "action.view_selected", ev("NUMPAD_PERIOD", "PRESS"));
  item(km, "action.view_frame", ev("NUMPAD_0", "PRESS"));
  item_menu_pie(km, "DOPESHEET_MT_view_pie", ev("ACCENT_GRAVE", "PRESS"));
  item(km, "anim.channels_editable_toggle", ev("TAB", "PRESS"));
  item(km, "anim.channels_select_filter", ev("F", "PRESS").ctrl());
  item(km, "transform.transform", ev("G", "PRESS")).enum_("mode", "TIME_TRANSLATE");
  item(km, "transform.transform", ev(params.select_mouse, "CLICK_DRAG"))
      .enum_("mode", "TIME_TRANSLATE");
  item(km, "transform.transform", ev("E", "PRESS")).enum_("mode", "TIME_EXTEND");
  item(km, "transform.transform", ev("S", "PRESS")).enum_("mode", "TIME_SCALE");
  item(km, "transform.transform", ev("T", "PRESS").shift()).enum_("mode", "TIME_SLIDE");
  /* `_template_items_proportional_editing(connected=False,
   * toggle_data_path="tool_settings.use_proportional_action")`. Al ser `connected`
   * falso no hay el Alt+O de "solo conectados". */
  if (!params.legacy) {
    item_menu_pie(km, "VIEW3D_MT_proportional_editing_falloff_pie", ev("O", "PRESS").shift());
  }
  else {
    item(km, "wm.context_cycle_enum", ev("O", "PRESS").shift())
        .string("data_path", "tool_settings.proportional_edit_falloff")
        .boolean("wrap", true);
  }
  item(km, "wm.context_toggle", ev("O", "PRESS"))
      .string("data_path", "tool_settings.use_proportional_action");
  item(km, "marker.add", ev("M", "PRESS"));
  item(km, "marker.camera_bind", ev("B", "PRESS").ctrl());
  /* `_template_items_context_menu()`. */
  item_menu(km, "DOPESHEET_MT_context_menu", params.context_menu_event);
  item_menu(km, "DOPESHEET_MT_context_menu", ev("APP", "PRESS"));
  /* `_template_items_change_frame()`: con seleccion por el izquierdo el barrido pasa a
   * Shift+derecho para no pisar la seleccion. */
  if (!params.select_mouse_right && !params.legacy) {
    item(km, "anim.change_frame", ev("RIGHTMOUSE", "PRESS").shift());
  }
  else {
    item(km, "anim.change_frame", ev(params.action_mouse, "PRESS"));
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Editor NLA
 * \{ */

static void km_nla_generic(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "NLA Generic", "NLA_EDITOR", "WINDOW");

  /* `_template_space_region_type_toggle(sidebar_key=N)`. */
  if (params.use_region_toggle_pie) {
    item_menu_pie(km, "WM_MT_region_toggle_pie", ev("N", "PRESS"));
  }
  else {
    item(km, "wm.context_toggle", ev("N", "PRESS")).string("data_path", "space_data.show_region_ui");
  }

  item(km, "nla.tweakmode_enter", ev("TAB", "PRESS")).boolean("use_upper_stack_evaluation", true);
  item(km, "nla.tweakmode_exit", ev("TAB", "PRESS"));
  item(km, "nla.tweakmode_enter", ev("TAB", "PRESS").shift()).boolean("isolate_action", true);
  item(km, "nla.tweakmode_exit", ev("TAB", "PRESS").shift()).boolean("isolate_action", true);
  item(km, "anim.channels_select_filter", ev("F", "PRESS").ctrl());
}

/** \} */

void register_group_07(wmKeyConfig *kc, const Params &params)
{
  km_file_browser_main(kc, params);
  km_file_browser_buttons(kc, params);
  km_dopesheet_generic(kc, params);
  km_dopesheet(kc, params);
  km_nla_generic(kc, params);
}

}  // namespace flipendo::keymap
