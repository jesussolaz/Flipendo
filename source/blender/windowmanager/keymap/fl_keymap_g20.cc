/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Mapa de teclado, grupo 20: mapas modales de gestos (zoom por caja, linea recta,
 * polilinea), el modal estandar, los modales de las herramientas de malla (cuchillo,
 * normales personalizadas, bisel) y los modales de navegacion en primera persona de la
 * vista 3D (volar y caminar).
 * Transliterado de blender_default.py.
 */

#include "FL_keymap_build.hpp"
#include "FL_keymap_params.hpp"

namespace flipendo::keymap {

/* -------------------------------------------------------------------- */
/** \name Gestos modales
 * \{ */

static void km_gesture_zoom_border(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap_modal(kc, "Gesture Zoom Border");

  item_modal(km, "CANCEL", ev("ESC", "PRESS").any());
  item_modal(km, "CANCEL", ev("RIGHTMOUSE", "ANY").any());
  item_modal(km, "BEGIN", ev("LEFTMOUSE", "PRESS"));
  item_modal(km, "IN", ev("LEFTMOUSE", "RELEASE"));
  item_modal(km, "BEGIN", ev("MIDDLEMOUSE", "PRESS"));
  item_modal(km, "OUT", ev("MIDDLEMOUSE", "RELEASE"));
}

static void km_gesture_straight_line(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap_modal(kc, "Gesture Straight Line");

  item_modal(km, "CANCEL", ev("ESC", "PRESS").any());
  item_modal(km, "CANCEL", ev("RIGHTMOUSE", "ANY").any());
  item_modal(km, "BEGIN", ev("LEFTMOUSE", "PRESS"));
  item_modal(km, "SELECT", ev("LEFTMOUSE", "RELEASE").any());
  item_modal(km, "MOVE", ev("SPACE", "ANY").any());
  item_modal(km, "SNAP", ev("LEFT_CTRL", "ANY").any());
  item_modal(km, "FLIP", ev("F", "PRESS").any());
}

static void km_gesture_polyline(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap_modal(kc, "Gesture Polyline");

  item_modal(km, "CONFIRM", ev("RET", "PRESS").any());
  item_modal(km, "CONFIRM", ev("LEFTMOUSE", "DOUBLE_CLICK").any());
  item_modal(km, "CANCEL", ev("ESC", "PRESS").any());
  item_modal(km, "CANCEL", ev("RIGHTMOUSE", "ANY").any());
  item_modal(km, "SELECT", ev("LEFTMOUSE", "PRESS").any());
  item_modal(km, "MOVE", ev("SPACE", "ANY").any());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Modal estandar
 * \{ */

static void km_standard_modal_map(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap_modal(kc, "Standard Modal Map");

  item_modal(km, "CANCEL", ev("ESC", "PRESS").any());
  item_modal(km, "APPLY", ev("LEFTMOUSE", "ANY").any());
  item_modal(km, "APPLY", ev("RET", "PRESS").any());
  item_modal(km, "APPLY", ev("NUMPAD_ENTER", "PRESS").any());
  item_modal(km, "SNAP", ev("LEFT_CTRL", "PRESS").any());
  item_modal(km, "SNAP_OFF", ev("LEFT_CTRL", "RELEASE").any());
  item_modal(km, "SNAP", ev("RIGHT_CTRL", "PRESS").any());
  item_modal(km, "SNAP_OFF", ev("RIGHT_CTRL", "RELEASE").any());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Modales de herramientas de malla
 * \{ */

static void km_knife_tool_modal_map(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap_modal(kc, "Knife Tool Modal Map");

  item_modal(km, "CANCEL", ev("ESC", "PRESS").any());
  item_modal(km, "PANNING", ev("MIDDLEMOUSE", "ANY").any());
  item_modal(km, "ADD_CUT_CLOSED", ev("LEFTMOUSE", "DOUBLE_CLICK").any());
  item_modal(km, "ADD_CUT", ev("LEFTMOUSE", "ANY").any());
  item_modal(km, "UNDO", ev("Z", "PRESS").ctrl());
  item_modal(km, "CONFIRM", ev("RET", "PRESS").any());
  item_modal(km, "CONFIRM", ev("NUMPAD_ENTER", "PRESS").any());
  item_modal(km, "CONFIRM", ev("SPACE", "PRESS").any());
  item_modal(km, "NEW_CUT", ev("RIGHTMOUSE", "PRESS"));
  item_modal(km, "SNAP_MIDPOINTS_ON", ev("LEFT_SHIFT", "PRESS").any());
  item_modal(km, "SNAP_MIDPOINTS_OFF", ev("LEFT_SHIFT", "RELEASE").any());
  item_modal(km, "SNAP_MIDPOINTS_ON", ev("RIGHT_SHIFT", "PRESS").any());
  item_modal(km, "SNAP_MIDPOINTS_OFF", ev("RIGHT_SHIFT", "RELEASE").any());
  item_modal(km, "IGNORE_SNAP_ON", ev("LEFT_CTRL", "PRESS").any());
  item_modal(km, "IGNORE_SNAP_OFF", ev("LEFT_CTRL", "RELEASE").any());
  item_modal(km, "IGNORE_SNAP_ON", ev("RIGHT_CTRL", "PRESS").any());
  item_modal(km, "IGNORE_SNAP_OFF", ev("RIGHT_CTRL", "RELEASE").any());
  item_modal(km, "X_AXIS", ev("X", "PRESS"));
  item_modal(km, "Y_AXIS", ev("Y", "PRESS"));
  item_modal(km, "Z_AXIS", ev("Z", "PRESS"));
  item_modal(km, "ANGLE_SNAP_TOGGLE", ev("A", "PRESS"));
  item_modal(km, "CYCLE_ANGLE_SNAP_EDGE", ev("R", "PRESS"));
  item_modal(km, "CUT_THROUGH_TOGGLE", ev("C", "PRESS"));
  item_modal(km, "SHOW_DISTANCE_ANGLE_TOGGLE", ev("S", "PRESS"));
  item_modal(km, "DEPTH_TEST_TOGGLE", ev("V", "PRESS"));
}

static void km_custom_normals_modal_map(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap_modal(kc, "Custom Normals Modal Map");

  item_modal(km, "CANCEL", ev("ESC", "PRESS").any());
  item_modal(km, "CANCEL", ev("RIGHTMOUSE", "PRESS"));
  item_modal(km, "CONFIRM", ev("RET", "PRESS").any());
  item_modal(km, "CONFIRM", ev("NUMPAD_ENTER", "PRESS").any());
  item_modal(km, "CONFIRM", ev("LEFTMOUSE", "PRESS"));
  item_modal(km, "RESET", ev("R", "PRESS"));
  item_modal(km, "INVERT", ev("I", "PRESS"));
  item_modal(km, "SPHERIZE", ev("S", "PRESS"));
  item_modal(km, "ALIGN", ev("A", "PRESS"));
  item_modal(km, "USE_MOUSE", ev("M", "PRESS"));
  item_modal(km, "USE_PIVOT", ev("L", "PRESS"));
  item_modal(km, "USE_OBJECT", ev("O", "PRESS"));
  item_modal(km, "SET_USE_3DCURSOR", ev("LEFTMOUSE", "CLICK").ctrl());
  item_modal(km, "SET_USE_SELECTED", ev("RIGHTMOUSE", "CLICK").ctrl());
}

static void km_bevel_modal_map(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap_modal(kc, "Bevel Modal Map");

  item_modal(km, "CANCEL", ev("ESC", "PRESS").any());
  item_modal(km, "CANCEL", ev("RIGHTMOUSE", "PRESS").any());
  item_modal(km, "CONFIRM", ev("RET", "PRESS").any());
  item_modal(km, "CONFIRM", ev("NUMPAD_ENTER", "PRESS").any());
  item_modal(km, "CONFIRM", ev("LEFTMOUSE", "PRESS").any());
  item_modal(km, "VALUE_OFFSET", ev("A", "PRESS").any());
  item_modal(km, "VALUE_PROFILE", ev("P", "PRESS").any());
  item_modal(km, "VALUE_SEGMENTS", ev("S", "PRESS").any());
  item_modal(km, "SEGMENTS_UP", ev("WHEELUPMOUSE", "PRESS").any());
  item_modal(km, "SEGMENTS_UP", ev("NUMPAD_PLUS", "PRESS").any());
  item_modal(km, "SEGMENTS_DOWN", ev("WHEELDOWNMOUSE", "PRESS").any());
  item_modal(km, "SEGMENTS_DOWN", ev("NUMPAD_MINUS", "PRESS").any());
  item_modal(km, "OFFSET_MODE_CHANGE", ev("M", "PRESS").any());
  item_modal(km, "CLAMP_OVERLAP_TOGGLE", ev("C", "PRESS").any());
  item_modal(km, "AFFECT_CHANGE", ev("V", "PRESS").any());
  item_modal(km, "HARDEN_NORMALS_TOGGLE", ev("H", "PRESS").any());
  item_modal(km, "MARK_SEAM_TOGGLE", ev("U", "PRESS").any());
  item_modal(km, "MARK_SHARP_TOGGLE", ev("K", "PRESS").any());
  item_modal(km, "OUTER_MITER_CHANGE", ev("O", "PRESS").any());
  item_modal(km, "INNER_MITER_CHANGE", ev("I", "PRESS").any());
  item_modal(km, "PROFILE_TYPE_CHANGE", ev("Z", "PRESS").any());
  item_modal(km, "VERTEX_MESH_CHANGE", ev("N", "PRESS").any());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Navegacion en primera persona de la vista 3D
 * \{ */

static void km_view3d_fly_modal(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap_modal(kc, "View3D Fly Modal");

  item_modal(km, "CANCEL", ev("RIGHTMOUSE", "ANY").any());
  item_modal(km, "CANCEL", ev("ESC", "PRESS").any());
  item_modal(km, "CONFIRM", ev("LEFTMOUSE", "ANY").any());
  item_modal(km, "CONFIRM", ev("RET", "PRESS").any());
  item_modal(km, "CONFIRM", ev("SPACE", "PRESS").any());
  item_modal(km, "CONFIRM", ev("NUMPAD_ENTER", "PRESS").any());
  item_modal(km, "ACCELERATE", ev("NUMPAD_PLUS", "PRESS").any().repeat());
  item_modal(km, "DECELERATE", ev("NUMPAD_MINUS", "PRESS").any().repeat());
  item_modal(km, "ACCELERATE", ev("WHEELUPMOUSE", "PRESS").any());
  item_modal(km, "DECELERATE", ev("WHEELDOWNMOUSE", "PRESS").any());
  item_modal(km, "CONFIRM", ev("TRACKPADPAN", "ANY"));
  item_modal(km, "PAN_ENABLE", ev("MIDDLEMOUSE", "PRESS").any());
  item_modal(km, "PAN_DISABLE", ev("MIDDLEMOUSE", "RELEASE").any());
  item_modal(km, "FORWARD", ev("W", "PRESS").repeat());
  item_modal(km, "BACKWARD", ev("S", "PRESS").repeat());
  item_modal(km, "LEFT", ev("A", "PRESS").repeat());
  item_modal(km, "RIGHT", ev("D", "PRESS").repeat());
  item_modal(km, "UP", ev("E", "PRESS").repeat());
  item_modal(km, "DOWN", ev("Q", "PRESS").repeat());
  item_modal(km, "UP", ev("R", "PRESS").repeat());
  item_modal(km, "DOWN", ev("F", "PRESS").repeat());
  item_modal(km, "FORWARD", ev("UP_ARROW", "PRESS").repeat());
  item_modal(km, "BACKWARD", ev("DOWN_ARROW", "PRESS").repeat());
  item_modal(km, "LEFT", ev("LEFT_ARROW", "PRESS").repeat());
  item_modal(km, "RIGHT", ev("RIGHT_ARROW", "PRESS").repeat());
  item_modal(km, "AXIS_LOCK_X", ev("X", "PRESS"));
  item_modal(km, "AXIS_LOCK_Z", ev("Z", "PRESS"));
  item_modal(km, "PRECISION_ENABLE", ev("LEFT_ALT", "PRESS").any());
  item_modal(km, "PRECISION_DISABLE", ev("LEFT_ALT", "RELEASE").any());
  item_modal(km, "PRECISION_ENABLE", ev("RIGHT_ALT", "PRESS").any());
  item_modal(km, "PRECISION_DISABLE", ev("RIGHT_ALT", "RELEASE").any());
  item_modal(km, "PRECISION_ENABLE", ev("LEFT_SHIFT", "PRESS").any());
  item_modal(km, "PRECISION_DISABLE", ev("LEFT_SHIFT", "RELEASE").any());
  item_modal(km, "PRECISION_ENABLE", ev("RIGHT_SHIFT", "PRESS").any());
  item_modal(km, "PRECISION_DISABLE", ev("RIGHT_SHIFT", "RELEASE").any());
  item_modal(km, "FREELOOK_ENABLE", ev("LEFT_CTRL", "PRESS").any());
  item_modal(km, "FREELOOK_DISABLE", ev("LEFT_CTRL", "RELEASE").any());
  item_modal(km, "FREELOOK_ENABLE", ev("RIGHT_CTRL", "PRESS").any());
  item_modal(km, "FREELOOK_DISABLE", ev("RIGHT_CTRL", "RELEASE").any());
}

static void km_view3d_walk_modal(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap_modal(kc, "View3D Walk Modal");

  item_modal(km, "CANCEL", ev("RIGHTMOUSE", "ANY").any());
  item_modal(km, "CANCEL", ev("ESC", "PRESS").any());
  item_modal(km, "CONFIRM", ev("LEFTMOUSE", "ANY").any());
  item_modal(km, "CONFIRM", ev("RET", "PRESS").any());
  item_modal(km, "CONFIRM", ev("NUMPAD_ENTER", "PRESS").any());
  item_modal(km, "FAST_ENABLE", ev("LEFT_SHIFT", "PRESS").any());
  item_modal(km, "FAST_DISABLE", ev("LEFT_SHIFT", "RELEASE").any());
  item_modal(km, "FAST_ENABLE", ev("RIGHT_SHIFT", "PRESS").any());
  item_modal(km, "FAST_DISABLE", ev("RIGHT_SHIFT", "RELEASE").any());
  item_modal(km, "SLOW_ENABLE", ev("LEFT_ALT", "PRESS").any());
  item_modal(km, "SLOW_DISABLE", ev("LEFT_ALT", "RELEASE").any());
  item_modal(km, "SLOW_ENABLE", ev("RIGHT_ALT", "PRESS").any());
  item_modal(km, "SLOW_DISABLE", ev("RIGHT_ALT", "RELEASE").any());
  item_modal(km, "FORWARD", ev("W", "PRESS").any());
  item_modal(km, "BACKWARD", ev("S", "PRESS").any());
  item_modal(km, "LEFT", ev("A", "PRESS").any());
  item_modal(km, "RIGHT", ev("D", "PRESS").any());
  item_modal(km, "UP", ev("E", "PRESS").any());
  item_modal(km, "DOWN", ev("Q", "PRESS").any());
  item_modal(km, "LOCAL_UP", ev("R", "PRESS").any());
  item_modal(km, "LOCAL_DOWN", ev("F", "PRESS").any());
  item_modal(km, "FORWARD_STOP", ev("W", "RELEASE").any());
  item_modal(km, "BACKWARD_STOP", ev("S", "RELEASE").any());
  item_modal(km, "LEFT_STOP", ev("A", "RELEASE").any());
  item_modal(km, "RIGHT_STOP", ev("D", "RELEASE").any());
  item_modal(km, "UP_STOP", ev("E", "RELEASE").any());
  item_modal(km, "DOWN_STOP", ev("Q", "RELEASE").any());
  item_modal(km, "LOCAL_UP_STOP", ev("R", "RELEASE").any());
  item_modal(km, "LOCAL_DOWN_STOP", ev("F", "RELEASE").any());
  item_modal(km, "FORWARD", ev("UP_ARROW", "PRESS"));
  item_modal(km, "BACKWARD", ev("DOWN_ARROW", "PRESS"));
  item_modal(km, "LEFT", ev("LEFT_ARROW", "PRESS"));
  item_modal(km, "RIGHT", ev("RIGHT_ARROW", "PRESS"));
  item_modal(km, "FORWARD_STOP", ev("UP_ARROW", "RELEASE").any());
  item_modal(km, "BACKWARD_STOP", ev("DOWN_ARROW", "RELEASE").any());
  item_modal(km, "LEFT_STOP", ev("LEFT_ARROW", "RELEASE").any());
  item_modal(km, "RIGHT_STOP", ev("RIGHT_ARROW", "RELEASE").any());
  item_modal(km, "GRAVITY_TOGGLE", ev("TAB", "PRESS"));
  item_modal(km, "GRAVITY_TOGGLE", ev("G", "PRESS"));
  item_modal(km, "JUMP", ev("V", "PRESS").any());
  item_modal(km, "JUMP_STOP", ev("V", "RELEASE").any());
  item_modal(km, "TELEPORT", ev("SPACE", "PRESS").any());
  item_modal(km, "TELEPORT", ev("MIDDLEMOUSE", "ANY").any());
  item_modal(km, "ACCELERATE", ev("NUMPAD_PLUS", "PRESS").any().repeat());
  item_modal(km, "DECELERATE", ev("NUMPAD_MINUS", "PRESS").any().repeat());
  item_modal(km, "ACCELERATE", ev("WHEELUPMOUSE", "PRESS").any());
  item_modal(km, "DECELERATE", ev("WHEELDOWNMOUSE", "PRESS").any());
  item_modal(km, "AXIS_LOCK_Z", ev("Z", "PRESS"));
  item_modal(km, "INCREASE_JUMP", ev("PERIOD", "PRESS").any());
  item_modal(km, "DECREASE_JUMP", ev("COMMA", "PRESS").any());
}

/** \} */

void register_group_20(wmKeyConfig *kc, const Params &params)
{
  km_gesture_zoom_border(kc, params);
  km_gesture_straight_line(kc, params);
  km_gesture_polyline(kc, params);
  km_standard_modal_map(kc, params);
  km_knife_tool_modal_map(kc, params);
  km_custom_normals_modal_map(kc, params);
  km_bevel_modal_map(kc, params);
  km_view3d_fly_modal(kc, params);
  km_view3d_walk_modal(kc, params);
}

}  // namespace flipendo::keymap
