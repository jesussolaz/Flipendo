/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Mapa de teclado, grupo 02: lista de botones View2D, interfaz de usuario, edicion de
 * mascaras, marcadores y editor de propiedades.
 * Transliterado de blender_default.py.
 */

#include "FL_keymap_build.hpp"
#include "FL_keymap_params.hpp"

namespace flipendo::keymap {

/* -------------------------------------------------------------------- */
/** \name Plantillas compartidas dentro del grupo
 * \{ */

/**
 * `_template_items_select_actions()` del Python.
 *
 * Se usa en dos keymaps de este grupo ("Mask Editing" y "Markers") y tiene tres ramas
 * segun preferencias; se centraliza aqui para no copiar las tres dos veces.
 */
static void template_items_select_actions(wmKeyMap *km, const Params &params, const char *op)
{
  if (!params.use_select_all_toggle) {
    item(km, op, ev("A", "PRESS")).enum_("action", "SELECT");
    item(km, op, ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, op, ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
    item(km, op, ev("A", "DOUBLE_CLICK")).enum_("action", "DESELECT");
  }
  else if (params.legacy) {
    /* En el keymap legacy Alt-A es reproducir, asi que ahi no deselecciona. */
    item(km, op, ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, op, ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }
  else {
    item(km, op, ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, op, ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, op, ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lista de botones View2D
 * \{ */

static void km_view2d_buttons_list(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap(kc, "View2D Buttons List", "EMPTY", "WINDOW");

  /* Barras de desplazamiento. */
  item(km, "view2d.scroller_activate", ev("LEFTMOUSE", "PRESS"));
  item(km, "view2d.scroller_activate", ev("MIDDLEMOUSE", "PRESS"));
  /* Desplazamiento. */
  item(km, "view2d.pan", ev("MIDDLEMOUSE", "PRESS"));
  item(km, "view2d.pan", ev("TRACKPADPAN", "ANY"));
  item(km, "view2d.scroll_down", ev("WHEELDOWNMOUSE", "PRESS"));
  item(km, "view2d.scroll_up", ev("WHEELUPMOUSE", "PRESS"));
  item(km, "view2d.scroll_down", ev("PAGE_DOWN", "PRESS").repeat()).boolean("page", true);
  item(km, "view2d.scroll_up", ev("PAGE_UP", "PRESS").repeat()).boolean("page", true);
  /* Zoom. */
  item(km, "view2d.zoom", ev("MIDDLEMOUSE", "PRESS").ctrl());
  item(km, "view2d.zoom", ev("TRACKPADZOOM", "ANY"));
  item(km, "view2d.zoom", ev("TRACKPADPAN", "ANY").ctrl());
  item(km, "view2d.zoom_out", ev("NUMPAD_MINUS", "PRESS").repeat());
  item(km, "view2d.zoom_in", ev("NUMPAD_PLUS", "PRESS").repeat());
  item(km, "view2d.reset", ev("HOME", "PRESS"));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Interfaz de usuario
 * \{ */

static void km_user_interface(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap(kc, "User Interface", "EMPTY", "WINDOW");

  /* Todos los cuentagotas comparten el mismo evento y lo dejan pasar hasta que uno de
   * ellos lo puede atender. */
  item(km, "ui.eyedropper_color", ev("E", "PRESS"));
  item(km, "ui.eyedropper_colorramp", ev("E", "PRESS"));
  item(km, "ui.eyedropper_colorramp_point", ev("E", "PRESS").alt());
  item(km, "ui.eyedropper_id", ev("E", "PRESS"));
  item(km, "ui.eyedropper_depth", ev("E", "PRESS"));
  item(km, "ui.eyedropper_bone", ev("E", "PRESS"));
  /* Copiar la ruta del dato. */
  item(km, "ui.copy_data_path_button", ev("C", "PRESS").shift().ctrl());
  item(km, "ui.copy_data_path_button", ev("C", "PRESS").shift().ctrl().alt())
      .boolean("full_path", true);
  /* Fotogramas clave y controladores. */
  item(km, "anim.keyframe_insert_button", ev("I", "PRESS")).boolean("all", true);
  item(km, "anim.keyframe_delete_button", ev("I", "PRESS").alt()).boolean("all", true);
  item(km, "anim.keyframe_clear_button", ev("I", "PRESS").shift().alt()).boolean("all", true);
  item(km, "anim.driver_button_add", ev("D", "PRESS").ctrl());
  item(km, "anim.driver_button_remove", ev("D", "PRESS").ctrl().alt());
  item(km, "anim.keyingset_button_add", ev("K", "PRESS"));
  item(km, "anim.keyingset_button_remove", ev("K", "PRESS").alt());
  item(km, "ui.reset_default_button", ev("BACK_SPACE", "PRESS")).boolean("all", true);
  /* Listas de UI (el poll comprueba que haya una bajo el cursor). */
  item(km, "ui.list_start_filter", ev("F", "PRESS").ctrl());
  /* Vistas de UI (el poll comprueba que haya una bajo el cursor). */
  item(km, "ui.view_start_filter", ev("F", "PRESS").ctrl());
  item(km, "ui.view_scroll", ev("WHEELUPMOUSE", "ANY"));
  item(km, "ui.view_scroll", ev("WHEELDOWNMOUSE", "ANY"));
  item(km, "ui.view_scroll", ev("TRACKPADPAN", "ANY"));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Compartido entre editores (mascaras, linea de tiempo)
 * \{ */

static void km_mask_editing(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Mask Editing", "EMPTY", "WINDOW");

  if (params.select_mouse_right) {
    /* `mask.slide_point` hace practicamente lo mismo, asi que en el keymap de
     * seleccion con boton izquierdo se prefiere dejar ahi el menu contextual. */
    item(km, "mask.select", ev("RIGHTMOUSE", "PRESS")).boolean("deselect_all", !params.legacy);
  }

  item(km, "mask.new", ev("N", "PRESS").alt());
  item_menu(km, "MASK_MT_add", ev("A", "PRESS").shift());

  /* `_template_items_proportional_editing(connected=False,
   * toggle_data_path="tool_settings.use_proportional_edit_mask")`. */
  if (!params.legacy) {
    item_menu_pie(km, "VIEW3D_MT_proportional_editing_falloff_pie", ev("O", "PRESS").shift());
  }
  else {
    item(km, "wm.context_cycle_enum", ev("O", "PRESS").shift())
        .string("data_path", "tool_settings.proportional_edit_falloff")
        .boolean("wrap", true);
  }
  item(km, "wm.context_toggle", ev("O", "PRESS"))
      .string("data_path", "tool_settings.use_proportional_edit_mask");
  /* `connected=False`: no se anade el conmutador de "solo conectados". */

  item(km, "mask.add_vertex_slide", ev("LEFTMOUSE", "PRESS").ctrl());
  item(km, "mask.add_feather_vertex_slide", ev("LEFTMOUSE", "PRESS").shift().ctrl());
  item(km, "mask.delete", ev("X", "PRESS"));
  item(km, "mask.delete", ev("DEL", "PRESS"));
  item(km, "mask.select", ev(params.select_mouse, "PRESS").shift()).boolean("toggle", true);

  template_items_select_actions(km, params, "mask.select_all");

  item(km, "mask.select_linked", ev("L", "PRESS").ctrl());
  item(km, "mask.select_linked_pick", ev("L", "PRESS")).boolean("deselect", false);
  item(km, "mask.select_linked_pick", ev("L", "PRESS").shift()).boolean("deselect", true);
  item(km, "mask.select_box", ev("B", "PRESS"));
  item(km, "mask.select_circle", ev("C", "PRESS"));
  item(km, "mask.select_lasso", ev(params.action_mouse, "CLICK_DRAG").ctrl().alt())
      .enum_("mode", "ADD");
  item(km, "mask.select_lasso", ev(params.action_mouse, "CLICK_DRAG").shift().ctrl().alt())
      .enum_("mode", "SUB");
  item(km, "mask.select_more", ev("NUMPAD_PLUS", "PRESS").ctrl().repeat());
  item(km, "mask.select_less", ev("NUMPAD_MINUS", "PRESS").ctrl().repeat());

  /* `_template_items_hide_reveal_actions("mask.hide_view_set", "mask.hide_view_clear")`. */
  item(km, "mask.hide_view_clear", ev("H", "PRESS").alt());
  item(km, "mask.hide_view_set", ev("H", "PRESS")).boolean("unselected", false);
  item(km, "mask.hide_view_set", ev("H", "PRESS").shift()).boolean("unselected", true);

  item(km, "clip.select", ev(params.select_mouse, "PRESS").ctrl());
  item(km, "mask.cyclic_toggle", ev("C", "PRESS").alt());
  item(km, "mask.slide_point", ev("LEFTMOUSE", "PRESS"));
  item(km, "mask.slide_spline_curvature", ev("LEFTMOUSE", "PRESS"));
  item(km, "mask.handle_type_set", ev("V", "PRESS"));
  /* El modificador cambia segun el keymap: Ctrl en el legacy, Shift en el moderno. */
  if (params.legacy) {
    item(km, "mask.normals_make_consistent", ev("N", "PRESS").ctrl());
  }
  else {
    item(km, "mask.normals_make_consistent", ev("N", "PRESS").shift());
  }
  item(km, "mask.parent_set", ev("P", "PRESS").ctrl());
  item(km, "mask.parent_clear", ev("P", "PRESS").alt());
  item(km, "mask.shape_key_insert", ev("I", "PRESS"));
  item(km, "mask.shape_key_clear", ev("I", "PRESS").alt());
  item(km, "mask.duplicate_move", ev("D", "PRESS").shift());
  item(km, "mask.copy_splines", ev("C", "PRESS").ctrl());
  item(km, "mask.paste_splines", ev("V", "PRESS").ctrl());
  item(km, "transform.translate", ev("G", "PRESS"));
  item(km, "transform.translate", ev(params.select_mouse, "CLICK_DRAG"));
  item(km, "transform.rotate", ev("R", "PRESS"));
  item(km, "transform.resize", ev("S", "PRESS"));
  item(km, "transform.tosphere", ev("S", "PRESS").shift().alt());
  item(km, "transform.shear", ev("S", "PRESS").shift().ctrl().alt());
  item(km, "transform.transform", ev("S", "PRESS").alt()).enum_("mode", "MASK_SHRINKFATTEN");

  /* Cursor 2D. */
  item(km, "uv.cursor_set", params.cursor_set_event);
  if (params.has_cursor_tweak_event) {
    item(km, "transform.translate", params.cursor_tweak_event)
        .boolean("release_confirm", true)
        .boolean("cursor_transform", true);
  }
}

static void km_markers(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Markers", "EMPTY", "WINDOW");

  item(km, "marker.add", ev("M", "PRESS"));
  item(km, "marker.move", ev(params.select_mouse, "CLICK_DRAG")).boolean("tweak", true);
  item(km, "marker.duplicate", ev("D", "PRESS").shift());
  item(km, "marker.select", ev(params.select_mouse, "PRESS"));
  item(km, "marker.select", ev(params.select_mouse, "PRESS").shift()).boolean("extend", true);
  item(km, "marker.select", ev(params.select_mouse, "PRESS").ctrl()).boolean("camera", true);
  item(km, "marker.select", ev(params.select_mouse, "PRESS").shift().ctrl())
      .boolean("extend", true)
      .boolean("camera", true);
  item(km, "marker.select_box", ev(params.select_mouse, "CLICK_DRAG")).boolean("tweak", true);
  item(km, "marker.select_box", ev(params.select_mouse, "CLICK_DRAG").shift())
      .boolean("tweak", true)
      .enum_("mode", "ADD");
  item(km, "marker.select_box", ev(params.select_mouse, "CLICK_DRAG").ctrl())
      .boolean("tweak", true)
      .enum_("mode", "SUB");
  item(km, "marker.select_box", ev("B", "PRESS"));

  template_items_select_actions(km, params, "marker.select_all");

  item(km, "marker.delete", ev("X", "PRESS"));
  item(km, "marker.delete", ev("DEL", "PRESS")).boolean("confirm", false);
  item_panel(km, "TOPBAR_PT_name_marker", ev("F2", "PRESS")).boolean("keep_open", false);
  item_panel(km, "TOPBAR_PT_name_marker", ev("LEFTMOUSE", "DOUBLE_CLICK"))
      .boolean("keep_open", false);
  item(km, "marker.move", ev("G", "PRESS"));
  item(km, "marker.camera_bind", ev("B", "PRESS").ctrl());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Editor de propiedades
 * \{ */

static void km_property_editor(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap(kc, "Property Editor", "PROPERTIES", "WINDOW");

  item(km, "buttons.context_menu", ev("RIGHTMOUSE", "PRESS"));
  item(km, "screen.space_context_cycle", ev("WHEELUPMOUSE", "PRESS").ctrl())
      .enum_("direction", "PREV");
  item(km, "screen.space_context_cycle", ev("WHEELDOWNMOUSE", "PRESS").ctrl())
      .enum_("direction", "NEXT");
  item(km, "buttons.start_filter", ev("F", "PRESS").ctrl());
  item(km, "buttons.clear_filter", ev("F", "PRESS").alt());
  /* Paneles de modificadores. */
  item(km, "object.modifier_set_active", ev("LEFTMOUSE", "PRESS"));
  item(km, "object.modifier_remove", ev("X", "PRESS")).boolean("report", true);
  item(km, "object.modifier_remove", ev("DEL", "PRESS")).boolean("report", true);
  item(km, "object.modifier_copy", ev("D", "PRESS").shift());
  item(km, "object.add_modifier_menu", ev("A", "PRESS").shift());
  item(km, "object.modifier_apply", ev("A", "PRESS").ctrl()).boolean("report", true);
  /* Paneles de efectos de sombreado. */
  item(km, "object.shaderfx_remove", ev("X", "PRESS")).boolean("report", true);
  item(km, "object.shaderfx_remove", ev("DEL", "PRESS")).boolean("report", true);
  item(km, "object.shaderfx_copy", ev("D", "PRESS").shift());
  /* Paneles de restricciones. */
  item(km, "constraint.delete", ev("X", "PRESS")).boolean("report", true);
  item(km, "constraint.delete", ev("DEL", "PRESS")).boolean("report", true);
  item(km, "constraint.copy", ev("D", "PRESS").shift());
  item(km, "constraint.apply", ev("A", "PRESS").ctrl()).boolean("report", true);
}

/** \} */

void register_group_02(wmKeyConfig *kc, const Params &params)
{
  km_view2d_buttons_list(kc, params);
  km_user_interface(kc, params);
  km_mask_editing(kc, params);
  km_markers(kc, params);
  km_property_editor(kc, params);
}

}  // namespace flipendo::keymap
