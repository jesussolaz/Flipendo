/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Mapa de teclado, grupo 06: editor de nodos (generico y principal), ventana de
 * Informacion y navegador de ficheros.
 * Transliterado de blender_default.py.
 */

#include <cstring>

#include "FL_keymap_build.hpp"
#include "FL_keymap_params.hpp"

namespace flipendo::keymap {

/* `NUMBERS_0` del Python: teclas de la fila superior en orden numerico. */
static const char *NUMBERS_0[10] = {
    "ZERO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE"};

/* -------------------------------------------------------------------- */
/** \name Plantillas usadas por este grupo
 *
 * Se declaran aqui porque en este lote se repiten (`_template_node_select` tres
 * veces, las otras dos veces cada una); expandirlas a mano en cada sitio habria
 * multiplicado las ocasiones de equivocarse en el orden.
 * \{ */

/** `_template_node_select(type=, value=, select_passthrough=)`. */
static void template_node_select(wmKeyMap *km,
                                 const char *type,
                                 const char *value,
                                 const bool select_passthrough)
{
  item(km, "node.select", ev(type, value)).boolean("select_passthrough", select_passthrough);
  item(km, "node.select", ev(type, value).ctrl());
  item(km, "node.select", ev(type, value).alt());
  item(km, "node.select", ev(type, value).ctrl().alt());
  item(km, "node.select", ev(type, value).shift()).boolean("toggle", true);
  item(km, "node.select", ev(type, value).shift().ctrl()).boolean("toggle", true);
  item(km, "node.select", ev(type, value).shift().alt()).boolean("toggle", true);
  item(km, "node.select", ev(type, value).shift().ctrl().alt()).boolean("toggle", true);

  if (select_passthrough && std::strcmp(value, "PRESS") == 0) {
    /* Con paso a traves hace falta un CLICK aparte que si deseleccione lo demas. */
    item(km, "node.select", ev(type, "CLICK")).boolean("deselect_all", true);
  }
}

/** `_template_items_select_actions(params, operator)`. */
static void template_items_select_actions(wmKeyMap *km, const Params &params, const char *op)
{
  if (!params.use_select_all_toggle) {
    item(km, op, ev("A", "PRESS")).enum_("action", "SELECT");
    item(km, op, ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, op, ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
    item(km, op, ev("A", "DOUBLE_CLICK")).enum_("action", "DESELECT");
  }
  else if (params.legacy) {
    /* En el keymap antiguo Alt-A es reproducir, asi que no se usa para deseleccionar. */
    item(km, op, ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, op, ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }
  else {
    item(km, op, ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, op, ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, op, ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }
}

/** `_template_items_context_menu(menu, key_args_primary)`. */
static void template_items_context_menu(wmKeyMap *km, const char *menu, const Event &primary)
{
  item_menu(km, menu, primary);
  item_menu(km, menu, ev("APP", "PRESS"));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Editor de nodos
 * \{ */

static void km_node_generic(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Node Generic", "NODE_EDITOR", "WINDOW");

  /* `_template_space_region_type_toggle(toolbar_key=T, sidebar_key=N)`. */
  if (params.use_region_toggle_pie) {
    /* La plantilla elige la tecla de la barra lateral como tecla del menu radial. */
    item_menu_pie(km, "WM_MT_region_toggle_pie", ev("N", "PRESS"));
  }
  else {
    item(km, "wm.context_toggle", ev("T", "PRESS"))
        .string("data_path", "space_data.show_region_toolbar");
    item(km, "wm.context_toggle", ev("N", "PRESS"))
        .string("data_path", "space_data.show_region_ui");
  }
}

static void km_node_editor(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Node Editor", "NODE_EDITOR", "WINDOW");

  if (!params.legacy) {
    template_node_select(km, params.select_mouse, params.select_mouse_value, true);
    /* Con seleccion por boton derecho tambien se puede seleccionar con el izquierdo. */
    if (params.select_mouse_right) {
      template_node_select(km, "LEFTMOUSE", "PRESS", true);
    }
    else {
      item_tool(km, "builtin.select_box", ev("W", "PRESS")).boolean("cycle", true);
    }
  }
  else {
    template_node_select(km, "RIGHTMOUSE", params.select_mouse_value, true);
    template_node_select(km, "LEFTMOUSE", "PRESS", true);
  }

  item(km, "node.select_box", ev(params.select_mouse, "CLICK_DRAG")).boolean("tweak", true);
  item(km, "node.select_lasso", ev("LEFTMOUSE", "CLICK_DRAG").ctrl().alt())
      .enum_("mode", "ADD");
  item(km, "node.select_lasso", ev("LEFTMOUSE", "CLICK_DRAG").shift().ctrl().alt())
      .enum_("mode", "SUB");

  /* `op_tool_optional`: con "activar herramientas con tecla" la tecla activa la
   * herramienta en vez de lanzar el operador. */
  if (params.use_key_activate_tools) {
    item_tool(km, "builtin.select_box", ev("B", "PRESS"));
  }
  else {
    item(km, "node.select_box", ev("B", "PRESS")).boolean("tweak", false);
  }
  if (params.use_key_activate_tools) {
    item_tool(km, "builtin.select_circle", ev("C", "PRESS"));
  }
  else {
    item(km, "node.select_circle", ev("C", "PRESS"));
  }

  item(km, "node.link", ev("LEFTMOUSE", "CLICK_DRAG")).boolean("detach", false);
  item(km, "node.link", ev("LEFTMOUSE", "CLICK_DRAG").ctrl()).boolean("detach", true);
  item(km, "node.resize", ev("LEFTMOUSE", "CLICK_DRAG"));
  item(km,
       "node.add_reroute",
       ev(params.legacy ? "LEFTMOUSE" : "RIGHTMOUSE", "CLICK_DRAG").shift());
  item(km,
       "node.links_cut",
       ev(params.legacy ? "LEFTMOUSE" : "RIGHTMOUSE", "CLICK_DRAG").ctrl());
  item(km, "node.links_mute", ev("RIGHTMOUSE", "CLICK_DRAG").ctrl().alt());
  item(km, "node.select_link_viewer", ev("LEFTMOUSE", "PRESS").shift().ctrl());
  /* El mismo atajo tres veces: uno para el editor de geometria y dos para el de
   * sombreado, donde hace de sustituto del nodo visor mientras no exista. */
  item(km, "node.connect_to_output", ev("LEFTMOUSE", "PRESS").shift().alt())
      .boolean("run_in_geometry_nodes", true);
  item(km, "node.connect_to_output", ev("LEFTMOUSE", "PRESS").shift().ctrl())
      .boolean("run_in_geometry_nodes", false);
  item(km, "node.connect_to_output", ev("LEFTMOUSE", "PRESS").shift().alt())
      .boolean("run_in_geometry_nodes", false);
  item(km, "node.backimage_move", ev("MIDDLEMOUSE", "PRESS").alt());
  /* El Python calcula 1.0 / 1.2 en doble y luego lo estrecha a float; se hace igual
   * para que el valor guardado sea bit a bit el mismo. */
  item(km, "node.backimage_zoom", ev("V", "PRESS").repeat())
      .number("factor", float(1.0 / 1.2));
  item(km, "node.backimage_zoom", ev("V", "PRESS").alt().repeat()).number("factor", 1.2f);
  item(km, "node.backimage_fit", ev("HOME", "PRESS").alt());
  item(km, "node.backimage_sample", ev(params.action_mouse, "PRESS").alt());
  item(km, "node.link_make", ev("J", "PRESS")).boolean("replace", false);
  item(km, "node.link_make", ev("J", "PRESS").shift()).boolean("replace", true);
  item_menu(km, "NODE_MT_add", ev("A", "PRESS").shift());
  item(km, "node.duplicate_move", ev("D", "PRESS").shift())
      .sub("NODE_OT_translate_attach")
      .sub("TRANSFORM_OT_translate")
      .boolean("view2d_edge_pan", true);
  item(km, "node.duplicate_move_linked", ev("D", "PRESS").alt())
      .sub("NODE_OT_translate_attach")
      .sub("TRANSFORM_OT_translate")
      .boolean("view2d_edge_pan", true);
  item(km, "node.duplicate_move_keep_inputs", ev("D", "PRESS").shift().ctrl())
      .sub("NODE_OT_translate_attach")
      .sub("TRANSFORM_OT_translate")
      .boolean("view2d_edge_pan", true);
  item(km, "node.parent_set", ev("P", "PRESS").ctrl());
  item(km, "node.detach", ev("P", "PRESS").alt());
  item(km, "node.join_named", ev("F", "PRESS"));
  item(km, "node.hide_toggle", ev("H", "PRESS"));
  item(km, "node.mute_toggle", ev("M", "PRESS"));
  item(km, "node.preview_toggle", ev("H", "PRESS").shift());
  item(km, "node.hide_socket_toggle", ev("H", "PRESS").ctrl());
  item(km, "node.view_all", ev("HOME", "PRESS"));
  item(km, "node.view_all", ev("NDOF_BUTTON_FIT", "PRESS"));
  item(km, "node.view_selected", ev("NUMPAD_PERIOD", "PRESS"));
  item_menu_pie(km, "NODE_MT_view_pie", ev("ACCENT_GRAVE", "PRESS"));
  item(km, "node.delete", ev("X", "PRESS"));
  item(km, "node.delete", ev("DEL", "PRESS"));
  item(km, "node.delete_reconnect", ev("X", "PRESS").ctrl());
  item(km, "node.delete_reconnect", ev("DEL", "PRESS").ctrl());
  template_items_select_actions(km, params, "node.select_all");
  item(km, "node.select_linked_to", ev("L", "PRESS").shift());
  item(km, "node.select_linked_from", ev("L", "PRESS"));
  item(km, "node.select_grouped", ev("G", "PRESS").shift());
  item(km, "node.select_grouped", ev("G", "PRESS").shift().ctrl()).boolean("extend", true);
  item(km, "node.select_same_type_step", ev("RIGHT_BRACKET", "PRESS").shift())
      .boolean("prev", false);
  item(km, "node.select_same_type_step", ev("LEFT_BRACKET", "PRESS").shift())
      .boolean("prev", true);
  item(km, "node.find_node", ev("F", "PRESS").ctrl());
  item(km, "node.group_make", ev("G", "PRESS").ctrl());
  item(km, "node.group_ungroup", ev("G", "PRESS").ctrl().alt());
  item(km, "node.group_separate", ev("P", "PRESS"));
  item(km, "node.group_edit", ev("TAB", "PRESS")).boolean("exit", false);
  item(km, "node.group_edit", ev("TAB", "PRESS").ctrl()).boolean("exit", true);
  item(km, "node.read_viewlayers", ev("R", "PRESS").ctrl());
  item(km, "node.render_changed", ev("Z", "PRESS"));
  item(km, "node.clipboard_copy", ev("C", "PRESS").ctrl());
  item(km, "node.clipboard_paste", ev("V", "PRESS").ctrl());
  item(km, "node.viewer_border", ev("B", "PRESS").ctrl());
  item(km, "node.clear_viewer_border", ev("B", "PRESS").ctrl().alt());
  item(km, "node.translate_attach", ev("G", "PRESS"))
      .sub("TRANSFORM_OT_translate")
      .boolean("view2d_edge_pan", true);
  item(km, "node.translate_attach", ev("LEFTMOUSE", "CLICK_DRAG"))
      .sub("TRANSFORM_OT_translate")
      .boolean("view2d_edge_pan", true);
  /* Con seleccion por boton izquierdo este atajo seria el mismo que el anterior. */
  if (params.select_mouse_right) {
    item(km, "node.translate_attach", ev(params.select_mouse, "CLICK_DRAG"))
        .sub("TRANSFORM_OT_translate")
        .boolean("view2d_edge_pan", true);
  }
  item(km, "transform.translate", ev("G", "PRESS")).boolean("view2d_edge_pan", true);
  item(km, "transform.translate", ev("LEFTMOUSE", "CLICK_DRAG"))
      .boolean("release_confirm", true)
      .boolean("view2d_edge_pan", true);
  /* Igual que arriba: no duplicar el atajo anterior. */
  if (params.select_mouse_right) {
    item(km, "transform.translate", ev(params.select_mouse, "CLICK_DRAG"))
        .boolean("release_confirm", true)
        .boolean("view2d_edge_pan", true);
  }
  item(km, "transform.rotate", ev("R", "PRESS"));
  item(km, "transform.resize", ev("S", "PRESS"));
  item(km, "node.move_detach_links_release", ev(params.action_mouse, "CLICK_DRAG").alt())
      .sub("NODE_OT_translate_attach")
      .sub("TRANSFORM_OT_translate")
      .boolean("view2d_edge_pan", true);
  item(km, "node.move_detach_links", ev(params.select_mouse, "CLICK_DRAG").alt())
      .sub("TRANSFORM_OT_translate")
      .boolean("view2d_edge_pan", true);
  item(km, "wm.context_toggle", ev("TAB", "PRESS").shift())
      .string("data_path", "tool_settings.use_snap_node");
  item(km, "wm.context_toggle", ev("Z", "PRESS").alt().shift())
      .string("data_path", "space_data.overlay.show_overlays");
  template_items_context_menu(km, "NODE_MT_context_menu", params.context_menu_event);

  /* Atajos del visor: el bucle `for i in range(1, 10)` del Python. */
  for (int i = 1; i < 10; i++) {
    item(km, "node.viewer_shortcut_get", ev(NUMBERS_0[i], "PRESS")).integer("viewer_index", i);
  }
  for (int i = 1; i < 10; i++) {
    item(km, "node.viewer_shortcut_set", ev(NUMBERS_0[i], "PRESS").ctrl())
        .integer("viewer_index", i);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ventana de Informacion
 * \{ */

static void km_info(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Info", "INFO", "WINDOW");

  item(km, "info.select_pick", ev("LEFTMOUSE", "CLICK"));
  item(km, "info.select_pick", ev("LEFTMOUSE", "CLICK").shift()).boolean("extend", true);
  item(km, "info.select_box", ev("LEFTMOUSE", "CLICK_DRAG")).boolean("wait_for_input", false);
  template_items_select_actions(km, params, "info.select_all");
  item(km, "info.select_box", ev("B", "PRESS"));
  item(km, "info.report_replay", ev("R", "PRESS"));
  item(km, "info.report_delete", ev("X", "PRESS"));
  item(km, "info.report_delete", ev("DEL", "PRESS"));
  item(km, "info.report_copy", ev("C", "PRESS").ctrl());
  template_items_context_menu(km, "INFO_MT_context_menu", params.context_menu_event);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Navegador de ficheros
 * \{ */

static void km_file_browser(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "File Browser", "FILE_BROWSER", "WINDOW");

  /* `_template_space_region_type_toggle(toolbar_key=T)`: sin tecla de barra lateral,
   * asi que con el menu radial activo la plantilla no anade nada. */
  if (!params.use_region_toggle_pie) {
    item(km, "wm.context_toggle", ev("T", "PRESS"))
        .string("data_path", "space_data.show_region_toolbar");
  }

  item(km, "wm.context_toggle", ev("N", "PRESS"))
      .string("data_path", "space_data.show_region_tool_props");
  item(km, "file.parent", ev("UP_ARROW", "PRESS").alt());
  item(km, "file.previous", ev("LEFT_ARROW", "PRESS").alt());
  item(km, "file.previous", ev("BUTTON4MOUSE", "PRESS"));
  item(km, "file.next", ev("RIGHT_ARROW", "PRESS").alt());
  item(km, "file.next", ev("BUTTON5MOUSE", "PRESS"));
  /* Los dos operadores de refresco se excluyen por `poll`, asi que solo uno esta
   * disponible segun el contexto. */
  item(km, "file.refresh", ev("R", "PRESS"));
  item(km, "asset.library_refresh", ev("R", "PRESS"));
  item(km, "file.parent", ev("P", "PRESS"));
  item(km, "file.previous", ev("BACK_SPACE", "PRESS"));
  item(km, "file.next", ev("BACK_SPACE", "PRESS").shift());
  item(km, "wm.context_toggle", ev("H", "PRESS"))
      .string("data_path", "space_data.params.show_hidden");
  item(km, "file.directory_new", ev("I", "PRESS")).boolean("confirm", false);
  item(km, "file.rename", ev("F2", "PRESS"));
  item(km, "file.delete", ev("X", "PRESS"));
  item(km, "file.delete", ev("DEL", "PRESS"));
  item(km, "file.smoothscroll", ev("TIMER1", "ANY").any());
  item(km, "file.bookmark_add", ev("B", "PRESS").ctrl());
  item(km, "file.start_filter", ev("F", "PRESS").ctrl());
  item(km, "file.edit_directory_path", ev("L", "PRESS").ctrl());
  item(km, "file.filenum", ev("NUMPAD_PLUS", "PRESS").repeat()).integer("increment", 1);
  item(km, "file.filenum", ev("NUMPAD_PLUS", "PRESS").shift().repeat()).integer("increment", 10);
  item(km, "file.filenum", ev("NUMPAD_PLUS", "PRESS").ctrl().repeat()).integer("increment", 100);
  item(km, "file.filenum", ev("NUMPAD_MINUS", "PRESS").repeat()).integer("increment", -1);
  item(km, "file.filenum", ev("NUMPAD_MINUS", "PRESS").shift().repeat()).integer("increment", -10);
  item(km, "file.filenum", ev("NUMPAD_MINUS", "PRESS").ctrl().repeat()).integer("increment", -100);
  item_menu_pie(km, "FILEBROWSER_MT_view_pie", ev("ACCENT_GRAVE", "PRESS"));

  /* Selecciona el fichero bajo el cursor antes de abrir el menu contextual. */
  item(km, "file.select", ev("RIGHTMOUSE", "PRESS"))
      .boolean("open", false)
      .boolean("only_activate_if_selected", !params.select_mouse_right)
      .boolean("pass_through", true);
  template_items_context_menu(km, "FILEBROWSER_MT_context_menu", params.context_menu_event);
}

/** \} */

void register_group_06(wmKeyConfig *kc, const Params &params)
{
  km_node_generic(kc, params);
  km_node_editor(kc, params);
  km_info(kc, params);
  km_file_browser(kc, params);
}

}  // namespace flipendo::keymap
