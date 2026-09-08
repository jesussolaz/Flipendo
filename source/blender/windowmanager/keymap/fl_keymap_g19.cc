/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Mapa de teclado, grupo 19: modos de edicion de curvas ("Curves") y de nube de
 * puntos ("Point Cloud"), y los primeros mapas modales: cuentagotas (color y rampa
 * de color), transformacion, colocacion interactiva en la vista 3D y los gestos de
 * circulo y de caja.
 * Transliterado de blender_default.py.
 */

#include "FL_keymap_build.hpp"
#include "FL_keymap_params.hpp"

namespace flipendo::keymap {

/* -------------------------------------------------------------------- */
/** \name Modo edicion (curvas y nube de puntos)
 * \{ */

static void km_edit_curves(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Curves", "EMPTY", "WINDOW");

  /* _template_items_transform_actions(params, use_bend=True, use_mirror=True).
   * Los tres primeros son `op_tool_optional`: con `use_key_activate_tools` la tecla
   * cambia de herramienta (`op_tool_cycle`) en vez de lanzar el operador. */
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
  item(km, "transform.bend", ev("W", "PRESS").shift());
  item(km, "transform.mirror", ev("M", "PRESS").ctrl());

  item(km, "curves.set_selection_domain", ev("ONE", "PRESS")).enum_("domain", "POINT");
  item(km, "curves.set_selection_domain", ev("TWO", "PRESS")).enum_("domain", "CURVE");
  item(km, "curves.duplicate_move", ev("D", "PRESS").shift());

  /* _template_items_select_actions(params, "curves.select_all") */
  if (!params.use_select_all_toggle) {
    item(km, "curves.select_all", ev("A", "PRESS")).enum_("action", "SELECT");
    item(km, "curves.select_all", ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, "curves.select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
    item(km, "curves.select_all", ev("A", "DOUBLE_CLICK")).enum_("action", "DESELECT");
  }
  else if (params.legacy) {
    /* En el keymap heredado Alt+A es la reproduccion, por eso ahi no hay "deseleccionar". */
    item(km, "curves.select_all", ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, "curves.select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }
  else {
    item(km, "curves.select_all", ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, "curves.select_all", ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, "curves.select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }

  item(km, "curves.extrude_move", ev("E", "PRESS"));
  item(km, "curves.select_linked", ev("L", "PRESS").ctrl());
  item(km, "curves.select_linked_pick", ev("L", "PRESS")).boolean("deselect", false);
  item(km, "curves.select_linked_pick", ev("L", "PRESS").shift()).boolean("deselect", true);
  item(km, "curves.delete", ev("X", "PRESS"));
  item(km, "curves.delete", ev("DEL", "PRESS"));
  item(km, "curves.separate", ev("P", "PRESS"));
  item(km, "curves.select_more", ev("NUMPAD_PLUS", "PRESS").ctrl().repeat());
  item(km, "curves.select_less", ev("NUMPAD_MINUS", "PRESS").ctrl().repeat());
  item(km, "curves.split", ev("Y", "PRESS"));

  /* _template_items_proportional_editing(
   *     params, connected=True, toggle_data_path="tool_settings.use_proportional_edit") */
  if (!params.legacy) {
    item_menu_pie(km, "VIEW3D_MT_proportional_editing_falloff_pie", ev("O", "PRESS").shift());
  }
  else {
    item(km, "wm.context_cycle_enum", ev("O", "PRESS").shift())
        .string("data_path", "tool_settings.proportional_edit_falloff")
        .boolean("wrap", true);
  }
  item(km, "wm.context_toggle", ev("O", "PRESS"))
      .string("data_path", "tool_settings.use_proportional_edit");
  item(km, "wm.context_toggle", ev("O", "PRESS").alt())
      .string("data_path", "tool_settings.use_proportional_connected");

  item(km, "curves.tilt_clear", ev("T", "PRESS").alt());
  /* op_tool_optional(transform.tilt, (op_tool_cycle, "builtin.tilt"), params) */
  if (params.use_key_activate_tools) {
    item_tool(km, "builtin.tilt", ev("T", "PRESS").ctrl()).boolean("cycle", true);
  }
  else {
    item(km, "transform.tilt", ev("T", "PRESS").ctrl());
  }
  item(km, "transform.transform", ev("S", "PRESS").alt()).enum_("mode", "CURVE_SHRINKFATTEN");
  item(km, "curves.cyclic_toggle", ev("C", "PRESS").alt());
  item(km, "curves.handle_type_set", ev("V", "PRESS"));
  item_menu(km, "VIEW3D_MT_edit_curves_add", ev("A", "PRESS").shift());

  /* _template_items_context_menu("VIEW3D_MT_edit_curves_context_menu",
   *                              params.context_menu_event) */
  item_menu(km, "VIEW3D_MT_edit_curves_context_menu", params.context_menu_event);
  item_menu(km, "VIEW3D_MT_edit_curves_context_menu", ev("APP", "PRESS"));
}

static void km_edit_pointcloud(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Point Cloud", "EMPTY", "WINDOW");

  /* _template_items_transform_actions(params, use_bend=True, use_mirror=True) */
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
  item(km, "transform.bend", ev("W", "PRESS").shift());
  item(km, "transform.mirror", ev("M", "PRESS").ctrl());

  item(km, "pointcloud.duplicate_move", ev("D", "PRESS").shift());

  /* _template_items_select_actions(params, "pointcloud.select_all") */
  if (!params.use_select_all_toggle) {
    item(km, "pointcloud.select_all", ev("A", "PRESS")).enum_("action", "SELECT");
    item(km, "pointcloud.select_all", ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, "pointcloud.select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
    item(km, "pointcloud.select_all", ev("A", "DOUBLE_CLICK")).enum_("action", "DESELECT");
  }
  else if (params.legacy) {
    item(km, "pointcloud.select_all", ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, "pointcloud.select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }
  else {
    item(km, "pointcloud.select_all", ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, "pointcloud.select_all", ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, "pointcloud.select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }

  item(km, "pointcloud.delete", ev("X", "PRESS"));
  item(km, "pointcloud.delete", ev("DEL", "PRESS"));
  item(km, "pointcloud.separate", ev("P", "PRESS"));
  item(km, "transform.transform", ev("S", "PRESS").alt()).enum_("mode", "CURVE_SHRINKFATTEN");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mapas modales y gizmos
 * \{ */

static void km_eyedropper_modal_map(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap_modal(kc, "Eyedropper Modal Map");

  item_modal(km, "CANCEL", ev("ESC", "PRESS").any());
  item_modal(km, "CANCEL", ev("RIGHTMOUSE", "PRESS").any());
  item_modal(km, "SAMPLE_CONFIRM", ev("RET", "RELEASE").any());
  item_modal(km, "SAMPLE_CONFIRM", ev("NUMPAD_ENTER", "RELEASE").any());
  item_modal(km, "SAMPLE_CONFIRM", ev("LEFTMOUSE", "RELEASE").any());
  item_modal(km, "SAMPLE_BEGIN", ev("LEFTMOUSE", "PRESS").any());
  item_modal(km, "SAMPLE_RESET", ev("SPACE", "RELEASE").any());
}

static void km_eyedropper_colorramp_pointsampling_map(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap_modal(kc, "Eyedropper ColorRamp PointSampling Map");

  item_modal(km, "CANCEL", ev("ESC", "PRESS").any());
  item_modal(km, "CANCEL", ev("BACK_SPACE", "PRESS").any());
  item_modal(km, "SAMPLE_CONFIRM", ev("RIGHTMOUSE", "PRESS").any());
  item_modal(km, "SAMPLE_CONFIRM", ev("RET", "RELEASE").any());
  item_modal(km, "SAMPLE_CONFIRM", ev("NUMPAD_ENTER", "RELEASE").any());
  item_modal(km, "SAMPLE_SAMPLE", ev("LEFTMOUSE", "PRESS").any());
  item_modal(km, "SAMPLE_RESET", ev("SPACE", "RELEASE").any());
}

/**
 * `alt_without_navigaton` del Python: cuando Alt esta reservado para navegar, estos
 * atajos NO llevan Alt; si no lo esta, lo llevan. Se resuelve aqui porque en el
 * Python es un diccionario que se expande con `**` en varios eventos seguidos.
 */
static Event ev_alt_without_navigation(const char *type, const char *value, const Params &params)
{
  Event event = ev(type, value);
  if (!params.use_alt_navigation) {
    event.alt();
  }
  return event;
}

static void km_transform_modal_map(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_modal(kc, "Transform Modal Map");

  item_modal(km, "CONFIRM", ev("LEFTMOUSE", "PRESS").any());
  item_modal(km, "CONFIRM", ev("RET", "PRESS").any());
  item_modal(km, "CONFIRM", ev("NUMPAD_ENTER", "PRESS").any());
  item_modal(km, "CONFIRM", ev("SPACE", "PRESS").any());
  item_modal(km, "CANCEL", ev("RIGHTMOUSE", "PRESS").any());
  item_modal(km, "CANCEL", ev("ESC", "PRESS").any());
  item_modal(km, "AXIS_X", ev("X", "PRESS"));
  item_modal(km, "AXIS_Y", ev("Y", "PRESS"));
  item_modal(km, "AXIS_Z", ev("Z", "PRESS"));
  item_modal(km, "PLANE_X", ev("X", "PRESS").shift());
  item_modal(km, "PLANE_Y", ev("Y", "PRESS").shift());
  item_modal(km, "PLANE_Z", ev("Z", "PRESS").shift());
  item_modal(km, "CONS_OFF", ev("C", "PRESS"));
  item_modal(km, "TRANSLATE", ev("G", "PRESS"));
  item_modal(km, "VERT_EDGE_SLIDE", ev("G", "PRESS"));
  item_modal(km, "ROTATE", ev("R", "PRESS"));
  item_modal(km, "TRACKBALL", ev("R", "PRESS"));
  item_modal(km, "RESIZE", ev("S", "PRESS"));
  item_modal(km, "ROTATE_NORMALS", ev("N", "PRESS"));
  item_modal(km, "EDIT_SNAP_SOURCE_ON", ev("B", "PRESS"));
  item_modal(km, "EDIT_SNAP_SOURCE_OFF", ev("B", "PRESS"));
  item_modal(km, "SNAP_TOGGLE", ev("TAB", "PRESS").shift());
  item_modal(km, "SNAP_INV_ON", ev("LEFT_CTRL", "PRESS").any());
  item_modal(km, "SNAP_INV_OFF", ev("LEFT_CTRL", "RELEASE").any());
  item_modal(km, "SNAP_INV_ON", ev("RIGHT_CTRL", "PRESS").any());
  item_modal(km, "SNAP_INV_OFF", ev("RIGHT_CTRL", "RELEASE").any());
  item_modal(km, "ADD_SNAP", ev("A", "PRESS"));
  item_modal(km, "ADD_SNAP", ev("A", "PRESS").ctrl());
  item_modal(km, "REMOVE_SNAP", ev("A", "PRESS").alt());
  item_modal(km, "PROPORTIONAL_SIZE_UP", ev("PAGE_UP", "PRESS").repeat());
  item_modal(km, "PROPORTIONAL_SIZE_DOWN", ev("PAGE_DOWN", "PRESS").repeat());
  item_modal(km, "PROPORTIONAL_SIZE_UP", ev("PAGE_UP", "PRESS").shift().repeat());
  item_modal(km, "PROPORTIONAL_SIZE_DOWN", ev("PAGE_DOWN", "PRESS").shift().repeat());
  item_modal(km, "PROPORTIONAL_SIZE_UP", ev_alt_without_navigation("WHEELDOWNMOUSE", "PRESS", params));
  item_modal(km, "PROPORTIONAL_SIZE_DOWN", ev_alt_without_navigation("WHEELUPMOUSE", "PRESS", params));
  item_modal(km, "PROPORTIONAL_SIZE_UP", ev("WHEELDOWNMOUSE", "PRESS").shift());
  item_modal(km, "PROPORTIONAL_SIZE_DOWN", ev("WHEELUPMOUSE", "PRESS").shift());
  item_modal(km, "PROPORTIONAL_SIZE", ev_alt_without_navigation("TRACKPADPAN", "ANY", params));
  item_modal(km, "AUTOIK_CHAIN_LEN_UP", ev("PAGE_UP", "PRESS").repeat());
  item_modal(km, "AUTOIK_CHAIN_LEN_DOWN", ev("PAGE_DOWN", "PRESS").repeat());
  item_modal(km, "AUTOIK_CHAIN_LEN_UP", ev("PAGE_UP", "PRESS").shift().repeat());
  item_modal(km, "AUTOIK_CHAIN_LEN_DOWN", ev("PAGE_DOWN", "PRESS").shift().repeat());
  item_modal(km, "AUTOIK_CHAIN_LEN_UP", ev_alt_without_navigation("WHEELDOWNMOUSE", "PRESS", params));
  item_modal(km, "AUTOIK_CHAIN_LEN_DOWN", ev_alt_without_navigation("WHEELUPMOUSE", "PRESS", params));
  item_modal(km, "AUTOIK_CHAIN_LEN_UP", ev("WHEELDOWNMOUSE", "PRESS").shift());
  item_modal(km, "AUTOIK_CHAIN_LEN_DOWN", ev("WHEELUPMOUSE", "PRESS").shift());
  item_modal(km, "INSERTOFS_TOGGLE_DIR", ev("T", "PRESS"));
  item_modal(km, "NODE_ATTACH_ON", ev("LEFT_ALT", "RELEASE").any());
  item_modal(km, "NODE_ATTACH_OFF", ev("LEFT_ALT", "PRESS").any());
  item_modal(km, "NODE_FRAME", ev("F", "PRESS"));
  item_modal(km, "AUTOCONSTRAIN", ev_alt_without_navigation("MIDDLEMOUSE", "ANY", params));
  item_modal(km,
             "AUTOCONSTRAINPLANE",
             ev_alt_without_navigation("MIDDLEMOUSE", "ANY", params).shift());
  item_modal(km, "PRECISION", ev("LEFT_SHIFT", "ANY").any());
  item_modal(km, "PRECISION", ev("RIGHT_SHIFT", "ANY").any());

  if (params.use_alt_navigation) {
    item_modal(km, "PASSTHROUGH_NAVIGATE", ev("LEFT_ALT", "ANY").any());
  }
}

static void km_view3d_interactive_add_tool_modal(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap_modal(kc, "View3D Placement Modal");

  item_modal(km, "PIVOT_CENTER_ON", ev("LEFT_ALT", "PRESS").any());
  item_modal(km, "PIVOT_CENTER_OFF", ev("LEFT_ALT", "RELEASE").any());
  item_modal(km, "PIVOT_CENTER_ON", ev("RIGHT_ALT", "PRESS").any());
  item_modal(km, "PIVOT_CENTER_OFF", ev("RIGHT_ALT", "RELEASE").any());
  item_modal(km, "FIXED_ASPECT_ON", ev("LEFT_SHIFT", "PRESS").any());
  item_modal(km, "FIXED_ASPECT_OFF", ev("LEFT_SHIFT", "RELEASE").any());
  item_modal(km, "FIXED_ASPECT_ON", ev("RIGHT_SHIFT", "PRESS").any());
  item_modal(km, "FIXED_ASPECT_OFF", ev("RIGHT_SHIFT", "RELEASE").any());
  item_modal(km, "SNAP_ON", ev("LEFT_CTRL", "PRESS").any());
  item_modal(km, "SNAP_OFF", ev("LEFT_CTRL", "RELEASE").any());
  item_modal(km, "SNAP_ON", ev("RIGHT_CTRL", "PRESS").any());
  item_modal(km, "SNAP_OFF", ev("RIGHT_CTRL", "RELEASE").any());
}

static void km_view3d_gesture_circle(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap_modal(kc, "View3D Gesture Circle");

  /* El "soltar" usa valor cualquiera para que el circulo salga con cualquier boton;
   * hace falta cuando la seleccion circular se activa como herramienta. */
  item_modal(km, "CANCEL", ev("ESC", "PRESS").any());
  item_modal(km, "CANCEL", ev("RIGHTMOUSE", "ANY").any());
  item_modal(km, "CONFIRM", ev("RET", "PRESS").any());
  item_modal(km, "CONFIRM", ev("NUMPAD_ENTER", "PRESS"));
  item_modal(km, "SELECT", ev("LEFTMOUSE", "PRESS"));
  item_modal(km, "DESELECT", ev("LEFTMOUSE", "PRESS").shift());
  item_modal(km, "NOP", ev("LEFTMOUSE", "RELEASE").any());
  item_modal(km, "DESELECT", ev("MIDDLEMOUSE", "PRESS"));
  item_modal(km, "NOP", ev("MIDDLEMOUSE", "RELEASE").any());
  item_modal(km, "SUBTRACT", ev("WHEELUPMOUSE", "PRESS"));
  item_modal(km, "SUBTRACT", ev("NUMPAD_MINUS", "PRESS").repeat());
  item_modal(km, "ADD", ev("WHEELDOWNMOUSE", "PRESS"));
  item_modal(km, "ADD", ev("NUMPAD_PLUS", "PRESS").repeat());
  item_modal(km, "SIZE", ev("TRACKPADPAN", "ANY"));
}

static void km_gesture_border(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap_modal(kc, "Gesture Box");

  item_modal(km, "CANCEL", ev("ESC", "PRESS").any());
  item_modal(km, "CANCEL", ev("RIGHTMOUSE", "PRESS").any());
  item_modal(km, "SELECT", ev("RIGHTMOUSE", "RELEASE").any());
  item_modal(km, "BEGIN", ev("LEFTMOUSE", "PRESS").shift());
  item_modal(km, "DESELECT", ev("LEFTMOUSE", "RELEASE").shift());
  item_modal(km, "BEGIN", ev("LEFTMOUSE", "PRESS"));
  item_modal(km, "SELECT", ev("LEFTMOUSE", "RELEASE").any());
  item_modal(km, "BEGIN", ev("MIDDLEMOUSE", "PRESS"));
  item_modal(km, "DESELECT", ev("MIDDLEMOUSE", "RELEASE"));
  item_modal(km, "MOVE", ev("SPACE", "ANY").any());
}

/** \} */

void register_group_19(wmKeyConfig *kc, const Params &params)
{
  km_edit_curves(kc, params);
  km_edit_pointcloud(kc, params);

  km_eyedropper_modal_map(kc, params);
  km_eyedropper_colorramp_pointsampling_map(kc, params);
  km_transform_modal_map(kc, params);
  km_view3d_interactive_add_tool_modal(kc, params);
  km_view3d_gesture_circle(kc, params);
  km_gesture_border(kc, params);
}

}  // namespace flipendo::keymap
