/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Mapa de teclado, grupo 05: editor de curvas (Graph Editor, general y ventana) y
 * editor de imagen (Image Editor, general y ventana).
 * Transliterado de blender_default.py.
 */

#include "FL_keymap_build.hpp"
#include "FL_keymap_params.hpp"

namespace flipendo::keymap {

/* `NUMBERS_1` del Python: las teclas de numero en su orden FISICO del teclado, o sea
 * con el cero al final. No confundir con `NUMBERS_0`, que va en orden numerico. */
static const char *NUMBERS_1[10] = {
    "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE", "ZERO"};

/* -------------------------------------------------------------------- */
/** \name Editor de curvas (Graph Editor)
 * \{ */

static void km_graph_editor_generic(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Graph Editor Generic", "GRAPH_EDITOR", "WINDOW");

  /* `_template_space_region_type_toggle(params, sidebar_key={'N', 'PRESS'})`:
   * sin toolbar_key ni channels_key, la tecla del menu radial es la de la barra
   * lateral. */
  if (params.use_region_toggle_pie) {
    item_menu_pie(km, "WM_MT_region_toggle_pie", ev("N", "PRESS"));
  }
  else {
    item(km, "wm.context_toggle", ev("N", "PRESS")).string("data_path", "space_data.show_region_ui");
  }

  item(km, "graph.extrapolation_type", ev("E", "PRESS").shift());
  item(km, "graph.fmodifier_add", ev("M", "PRESS").shift().ctrl()).boolean("only_active", false);
  item(km, "anim.channels_select_filter", ev("F", "PRESS").ctrl());

  /* `_template_items_hide_reveal_actions("graph.hide", "graph.reveal")`. */
  item(km, "graph.reveal", ev("H", "PRESS").alt());
  item(km, "graph.hide", ev("H", "PRESS")).boolean("unselected", false);
  item(km, "graph.hide", ev("H", "PRESS").shift()).boolean("unselected", true);

  /* `value` de `wm.context_set_enum` es una cadena en RNA (el operador la resuelve al
   * ejecutarse), no una enumeracion: por eso `.string()` y no `.enum_()`. */
  item(km, "wm.context_set_enum", ev("TAB", "PRESS").ctrl())
      .string("data_path", "area.type")
      .string("value", "DOPESHEET_EDITOR");
}

static void km_graph_editor(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Graph Editor", "GRAPH_EDITOR", "WINDOW");

  item(km, "wm.context_toggle", ev("H", "PRESS").ctrl())
      .string("data_path", "space_data.show_handles");
  item(km, "graph.clickselect", ev(params.select_mouse, "PRESS"))
      .boolean("deselect_all", !params.legacy);
  item(km, "graph.clickselect", ev(params.select_mouse, "PRESS").alt()).boolean("column", true);
  item(km, "graph.clickselect", ev(params.select_mouse, "PRESS").shift()).boolean("extend", true);
  item(km, "graph.clickselect", ev(params.select_mouse, "PRESS").shift().alt())
      .boolean("extend", true)
      .boolean("column", true);
  item(km, "graph.clickselect", ev(params.select_mouse, "PRESS").ctrl().alt())
      .boolean("curves", true);
  item(km, "graph.clickselect", ev(params.select_mouse, "PRESS").shift().ctrl().alt())
      .boolean("extend", true)
      .boolean("curves", true);

  /* En el keymap antiguo el barrido de izquierda/derecha iba con PRESS. */
  const char *leftright_value = params.legacy ? "PRESS" : "CLICK";
  item(km, "graph.select_leftright", ev(params.select_mouse, leftright_value).ctrl())
      .enum_("mode", "CHECK");
  item(km, "graph.select_leftright", ev(params.select_mouse, leftright_value).ctrl().shift())
      .enum_("mode", "CHECK")
      .boolean("extend", true);
  item(km, "graph.select_leftright", ev("LEFT_BRACKET", "PRESS")).enum_("mode", "LEFT");
  item(km, "graph.select_leftright", ev("RIGHT_BRACKET", "PRESS")).enum_("mode", "RIGHT");

  /* `_template_items_select_actions(params, "graph.select_all")`. */
  if (!params.use_select_all_toggle) {
    item(km, "graph.select_all", ev("A", "PRESS")).enum_("action", "SELECT");
    item(km, "graph.select_all", ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, "graph.select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
    item(km, "graph.select_all", ev("A", "DOUBLE_CLICK")).enum_("action", "DESELECT");
  }
  else if (params.legacy) {
    /* En el keymap antiguo Alt-A es la reproduccion, asi que no hay deseleccion. */
    item(km, "graph.select_all", ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, "graph.select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }
  else {
    item(km, "graph.select_all", ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, "graph.select_all", ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, "graph.select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }

  item(km, "graph.select_box", ev("B", "PRESS"));
  item(km, "graph.select_box", ev("B", "PRESS").alt()).boolean("axis_range", true);
  item(km, "graph.select_box", ev(params.select_mouse, "CLICK_DRAG"))
      .boolean("tweak", true)
      .enum_("mode", "SET");
  item(km, "graph.select_box", ev(params.select_mouse, "CLICK_DRAG").shift())
      .boolean("tweak", true)
      .enum_("mode", "ADD");
  item(km, "graph.select_box", ev(params.select_mouse, "CLICK_DRAG").ctrl())
      .boolean("tweak", true)
      .enum_("mode", "SUB");
  item(km, "graph.select_lasso", ev(params.action_mouse, "CLICK_DRAG").ctrl())
      .enum_("mode", "ADD");
  item(km, "graph.select_lasso", ev(params.action_mouse, "CLICK_DRAG").shift().ctrl())
      .enum_("mode", "SUB");
  item(km, "graph.select_circle", ev("C", "PRESS"));
  item(km, "graph.select_column", ev("K", "PRESS")).enum_("mode", "KEYS");
  item(km, "graph.select_column", ev("K", "PRESS").ctrl()).enum_("mode", "CFRA");
  item(km, "graph.select_column", ev("K", "PRESS").shift()).enum_("mode", "MARKERS_COLUMN");
  item(km, "graph.select_column", ev("K", "PRESS").alt()).enum_("mode", "MARKERS_BETWEEN");
  item(km, "graph.select_more", ev("NUMPAD_PLUS", "PRESS").ctrl().repeat());
  item(km, "graph.select_less", ev("NUMPAD_MINUS", "PRESS").ctrl().repeat());
  item(km, "graph.select_linked", ev("L", "PRESS"));
  item(km, "graph.frame_jump", ev("G", "PRESS").ctrl());

  if (!params.legacy) {
    item_menu_pie(km, "GRAPH_MT_snap_pie", ev("S", "PRESS").shift());
  }
  else {
    item(km, "graph.snap", ev("S", "PRESS").shift());
  }

  item(km, "graph.mirror", ev("M", "PRESS").ctrl());
  item(km, "graph.handle_type", ev("V", "PRESS"));
  item(km, "graph.interpolation_type", ev("T", "PRESS"));
  item(km, "graph.easing_type", ev("E", "PRESS").ctrl());
  item(km, "graph.smooth", ev("O", "PRESS").alt());
  item(km, "graph.bake_keys", ev("O", "PRESS").shift().alt());
  item(km, "graph.keys_to_samples", ev("C", "PRESS").alt());
  item_menu(km, "GRAPH_MT_delete", ev("X", "PRESS"));
  item(km, "graph.delete", ev("DEL", "PRESS")).boolean("confirm", false);
  item(km, "graph.duplicate_move", ev("D", "PRESS").shift());
  item(km, "graph.keyframe_insert", ev("I", "PRESS"));
  item(km, "graph.click_insert", ev(params.action_mouse, "CLICK").ctrl());
  item(km, "graph.click_insert", ev(params.action_mouse, "CLICK").shift().ctrl())
      .boolean("extend", true);
  item(km, "graph.copy", ev("C", "PRESS").ctrl());
  item(km, "graph.paste", ev("V", "PRESS").ctrl());
  item(km, "graph.paste", ev("V", "PRESS").shift().ctrl()).boolean("flipped", true);
  item_menu(km, "GRAPH_MT_key_smoothing", ev("S", "PRESS").alt());
  item_menu(km, "GRAPH_MT_key_blending", ev("D", "PRESS").alt());
  item(km, "graph.previewrange_set", ev("P", "PRESS").ctrl().alt());
  item(km, "graph.view_all", ev("HOME", "PRESS"));
  item(km, "graph.view_all", ev("NDOF_BUTTON_FIT", "PRESS"));
  item(km, "graph.view_selected", ev("NUMPAD_PERIOD", "PRESS"));
  item(km, "graph.view_frame", ev("NUMPAD_0", "PRESS"));
  item_menu_pie(km, "GRAPH_MT_view_pie", ev("ACCENT_GRAVE", "PRESS"));
  item(km, "anim.channels_editable_toggle", ev("TAB", "PRESS"));
  item(km, "transform.translate", ev("G", "PRESS"));
  item(km, "transform.translate", ev(params.select_mouse, "CLICK_DRAG"));
  item(km, "transform.transform", ev("E", "PRESS")).enum_("mode", "TIME_EXTEND");
  item(km, "transform.rotate", ev("R", "PRESS"));
  item(km, "transform.resize", ev("S", "PRESS"));

  /* `_template_items_proportional_editing(params, connected=False,
   * toggle_data_path="tool_settings.use_proportional_fcurve")`. Sin `connected` no se
   * anade el conmutador de Alt-O. */
  if (!params.legacy) {
    item_menu_pie(km, "VIEW3D_MT_proportional_editing_falloff_pie", ev("O", "PRESS").shift());
  }
  else {
    item(km, "wm.context_cycle_enum", ev("O", "PRESS").shift())
        .string("data_path", "tool_settings.proportional_edit_falloff")
        .boolean("wrap", true);
  }
  item(km, "wm.context_toggle", ev("O", "PRESS"))
      .string("data_path", "tool_settings.use_proportional_fcurve");

  item(km, "marker.add", ev("M", "PRESS"));

  /* `_template_items_context_menu("GRAPH_MT_context_menu", params.context_menu_event)`:
   * el evento configurable y ademas la tecla de menu contextual del sistema. */
  item_menu(km, "GRAPH_MT_context_menu", params.context_menu_event);
  item_menu(km, "GRAPH_MT_context_menu", ev("APP", "PRESS"));

  if (!params.legacy) {
    item_menu_pie(km, "GRAPH_MT_pivot_pie", ev("PERIOD", "PRESS"));
  }
  else {
    /* Punto de pivote antiguo. */
    item(km, "wm.context_set_enum", ev("COMMA", "PRESS"))
        .string("data_path", "space_data.pivot_point")
        .string("value", "BOUNDING_BOX_CENTER");
    item(km, "wm.context_set_enum", ev("PERIOD", "PRESS"))
        .string("data_path", "space_data.pivot_point")
        .string("value", "CURSOR");
    item(km, "wm.context_set_enum", ev("PERIOD", "PRESS").ctrl())
        .string("data_path", "space_data.pivot_point")
        .string("value", "INDIVIDUAL_ORIGINS");
  }

  if (!params.select_mouse_right && !params.legacy) {
    /* Con seleccion por el izquierdo el boton de accion ya esta ocupado. */
    item(km, "graph.cursor_set", ev("RIGHTMOUSE", "PRESS").shift());
  }
  else {
    item(km, "graph.cursor_set", ev(params.action_mouse, "PRESS"));
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Editor de imagen (Image)
 * \{ */

static void km_image_generic(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Image Generic", "IMAGE_EDITOR", "WINDOW");

  /* `_template_space_region_type_toggle(params, toolbar_key={'T', 'PRESS'},
   * sidebar_key={'N', 'PRESS'})`: el menu radial se abre con la tecla de la barra
   * lateral, que es la que tiene prioridad en la plantilla. */
  if (params.use_region_toggle_pie) {
    item_menu_pie(km, "WM_MT_region_toggle_pie", ev("N", "PRESS"));
  }
  else {
    item(km, "wm.context_toggle", ev("T", "PRESS"))
        .string("data_path", "space_data.show_region_toolbar");
    item(km, "wm.context_toggle", ev("N", "PRESS")).string("data_path", "space_data.show_region_ui");
  }

  item(km, "image.new", ev("N", "PRESS").alt());
  item(km, "image.open", ev("O", "PRESS").alt());
  item(km, "image.reload", ev("R", "PRESS").alt());
  item(km, "image.read_viewlayers", ev("R", "PRESS").ctrl());
  item(km, "image.save", ev("S", "PRESS").alt());
  item(km, "image.cycle_render_slot", ev("J", "PRESS").repeat());
  item(km, "image.cycle_render_slot", ev("J", "PRESS").alt().repeat()).boolean("reverse", true);
  item_menu_pie(km, "IMAGE_MT_view_pie", ev("ACCENT_GRAVE", "PRESS"));

  if (!params.legacy) {
    item(km, "image.save_as", ev("S", "PRESS").shift().alt());
  }
  else {
    item(km, "image.save_as", ev("F3", "PRESS"));
  }
}

static void km_image(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Image", "IMAGE_EDITOR", "WINDOW");

  item(km, "image.view_all", ev("HOME", "PRESS"));
  item(km, "image.view_all", ev("HOME", "PRESS").shift()).boolean("fit_view", true);
  item(km, "image.view_selected", ev("NUMPAD_PERIOD", "PRESS"));
  item(km, "image.view_cursor_center", ev("C", "PRESS").shift());
  item(km, "image.view_pan", ev("MIDDLEMOUSE", "PRESS"));
  item(km, "image.view_pan", ev("MIDDLEMOUSE", "PRESS").shift());
  item(km, "image.view_pan", ev("TRACKPADPAN", "ANY"));
  item(km, "image.view_all", ev("NDOF_BUTTON_FIT", "PRESS"));
  item(km, "image.view_ndof", ev("NDOF_MOTION", "ANY"));
  item(km, "image.view_zoom_in", ev("WHEELINMOUSE", "PRESS"));
  item(km, "image.view_zoom_out", ev("WHEELOUTMOUSE", "PRESS"));
  item(km, "image.view_zoom_in", ev("NUMPAD_PLUS", "PRESS").repeat());
  item(km, "image.view_zoom_out", ev("NUMPAD_MINUS", "PRESS").repeat());
  item(km, "image.view_zoom", ev("MIDDLEMOUSE", "PRESS").ctrl());
  item(km, "image.view_zoom", ev("TRACKPADZOOM", "ANY"));
  item(km, "image.view_zoom", ev("TRACKPADPAN", "ANY").ctrl());
  item(km, "image.view_zoom_border", ev("B", "PRESS").shift());
  item(km, "image.view_zoom_ratio", ev("NUMPAD_8", "PRESS").ctrl()).number("ratio", 8.0f);
  item(km, "image.view_zoom_ratio", ev("NUMPAD_4", "PRESS").ctrl()).number("ratio", 4.0f);
  item(km, "image.view_zoom_ratio", ev("NUMPAD_2", "PRESS").ctrl()).number("ratio", 2.0f);
  item(km, "image.view_zoom_ratio", ev("NUMPAD_8", "PRESS").shift()).number("ratio", 8.0f);
  item(km, "image.view_zoom_ratio", ev("NUMPAD_4", "PRESS").shift()).number("ratio", 4.0f);
  item(km, "image.view_zoom_ratio", ev("NUMPAD_2", "PRESS").shift()).number("ratio", 2.0f);
  item(km, "image.view_zoom_ratio", ev("NUMPAD_1", "PRESS")).number("ratio", 1.0f);
  item(km, "image.view_zoom_ratio", ev("NUMPAD_2", "PRESS")).number("ratio", 0.5f);
  item(km, "image.view_zoom_ratio", ev("NUMPAD_4", "PRESS")).number("ratio", 0.25f);
  item(km, "image.view_zoom_ratio", ev("NUMPAD_8", "PRESS")).number("ratio", 0.125f);
  item(km, "image.change_frame", ev("LEFTMOUSE", "PRESS"));
  item(km, "image.sample", ev(params.action_mouse, "PRESS"));
  item(km, "image.curves_point_set", ev(params.action_mouse, "PRESS").ctrl())
      .enum_("point", "BLACK_POINT");
  item(km, "image.curves_point_set", ev(params.action_mouse, "PRESS").shift())
      .enum_("point", "WHITE_POINT");
  item(km, "object.mode_set", ev("TAB", "PRESS")).enum_("mode", "EDIT").boolean("toggle", true);

  /* Las nueve primeras teclas de numero eligen la ranura de render. El indice es el
   * valor, asi que va de 0 a 8 aunque las teclas sean del 1 al 9. */
  for (int i = 0; i < 9; i++) {
    item(km, "wm.context_set_int", ev(NUMBERS_1[i], "PRESS"))
        .string("data_path", "space_data.image.render_slots.active_index")
        .integer("value", i);
  }

  item(km, "image.render_border", ev("B", "PRESS").ctrl());
  item(km, "image.clear_render_border", ev("B", "PRESS").ctrl().alt());
  item(km, "wm.context_toggle", ev("ACCENT_GRAVE", "PRESS").ctrl())
      .string("data_path", "space_data.show_gizmo");
  item(km, "wm.context_toggle", ev("Z", "PRESS").alt().shift())
      .string("data_path", "space_data.overlay.show_overlays");

  /* `_template_items_context_menu("IMAGE_MT_mask_context_menu",
   * params.context_menu_event)`. */
  item_menu(km, "IMAGE_MT_mask_context_menu", params.context_menu_event);
  item_menu(km, "IMAGE_MT_mask_context_menu", ev("APP", "PRESS"));

  if (!params.legacy) {
    item_menu_pie(km, "IMAGE_MT_pivot_pie", ev("PERIOD", "PRESS"));
  }
  else {
    /* Punto de pivote antiguo. */
    item(km, "wm.context_set_enum", ev("COMMA", "PRESS"))
        .string("data_path", "space_data.pivot_point")
        .string("value", "CENTER");
    item(km, "wm.context_set_enum", ev("COMMA", "PRESS").ctrl())
        .string("data_path", "space_data.pivot_point")
        .string("value", "MEDIAN");
    item(km, "wm.context_set_enum", ev("PERIOD", "PRESS"))
        .string("data_path", "space_data.pivot_point")
        .string("value", "CURSOR");

    item(km, "image.view_center_cursor", ev("HOME", "PRESS").alt());
  }
}

/** \} */

void register_group_05(wmKeyConfig *kc, const Params &params)
{
  km_graph_editor_generic(kc, params);
  km_graph_editor(kc, params);

  km_image_generic(kc, params);
  km_image(kc, params);
}

}  // namespace flipendo::keymap
