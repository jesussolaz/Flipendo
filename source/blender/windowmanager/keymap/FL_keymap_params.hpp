/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Parametros del mapa de teclado por defecto.
 *
 * Transliteracion de la clase `Params` de
 * `scripts/presets/keyconfig/keymap_data/blender_default.py:25-224`.
 *
 * Esto NO se colapsa a una configuracion fija a proposito. El keymap depende de 17
 * preferencias del usuario (raton de seleccion, accion de la barra espaciadora,
 * menus radiales, herramientas de reserva...), y hornear una sola permutacion
 * borraria opciones que hoy existen. Los `km_*` conservan sus condicionales sobre
 * esta estructura, igual que el Python.
 *
 * OJO con los valores por defecto: NO son los del `__init__` de `Params`. El keymap
 * real no lo construye `blender_default.py` con sus propios defectos, sino
 * `presets/keyconfig/Blender.py:343-372`, que le pasa los valores derivados de las
 * preferencias de fabrica del usuario -- y algunos no coinciden. Los que importan:
 *
 *   - seleccion con el boton IZQUIERDO (`select_mouse` por defecto es 'LEFT'),
 *     no el derecho como dice el `__init__`;
 *   - `use_fallback_tool` va fijo a true, no es preferencia;
 *   - `use_gizmo_drag` = (seleccion izquierda Y gizmo_action == 'DRAG');
 *   - `spacebar_action` por defecto es 'PLAY', no 'TOOL'.
 *
 * Estos son los valores que produjeron `tests/flipendo/keymap/baseline-python.txt`.
 */

#ifndef __FL_KEYMAP_PARAMS_HPP__
#define __FL_KEYMAP_PARAMS_HPP__

#include "FL_keymap_build.hpp"

namespace flipendo::keymap {

/** `Params.spacebar_action` */
enum class SpacebarAction { Play, Tool, Search };
/** `Params.v3d_tilde_action` */
enum class TildeAction { View, Gizmo };
/** `Params.v3d_alt_mmb_drag_action` */
enum class AltMmbDragAction { Relative, Absolute };

struct Params {
  /* --- Opciones de entrada --- */
  bool legacy = false;
  /** true = raton derecho selecciona (por defecto); false = izquierdo. */
  bool select_mouse_right = false;
  bool use_mouse_emulate_3_button = false;
  bool use_alt_tool_or_cursor = false;

  /* --- Preferencias del usuario --- */
  SpacebarAction spacebar_action = SpacebarAction::Play;
  bool use_key_activate_tools = false;
  bool use_region_toggle_pie = false;
  bool use_select_all_toggle = false;
  bool use_gizmo_drag = true;
  bool use_fallback_tool = true;
  bool use_fallback_tool_select_handled = true;
  bool use_v3d_tab_menu = false;
  bool use_v3d_shade_ex_pie = false;
  bool use_v3d_mmb_pan = false;
  bool use_alt_click_leader = false;
  bool use_pie_click_drag = false;
  bool use_alt_navigation = true;
  bool use_file_single_click = false;
  TildeAction v3d_tilde_action = TildeAction::View;
  AltMmbDragAction v3d_alt_mmb_drag_action = AltMmbDragAction::Relative;

  /* --- Derivados (los calcula `finalize()`, como el `__init__` del Python) --- */
  const char *select_mouse = "RIGHTMOUSE";
  const char *select_mouse_value = "PRESS";
  const char *action_mouse = "LEFTMOUSE";
  const char *tool_mouse = "LEFTMOUSE";
  const char *tool_maybe_tweak_value = "CLICK_DRAG";
  const char *select_mouse_value_fallback = "PRESS";
  const char *pie_value = "PRESS";
  bool use_tweak_select_passthrough = true;
  bool use_fallback_tool_select_mouse = false;
  /** `tool_modifier` del Python: hoy solo puede pedir "Alt pulsado o no". */
  bool tool_modifier_alt_any = false;

  Event context_menu_event{"W", "PRESS"};
  Event cursor_set_event{"LEFTMOUSE", "CLICK"};
  Event cursor_tweak_event{"NONE", "PRESS"};
  bool has_cursor_tweak_event = false;
  Event select_tweak_event{"RIGHTMOUSE", "CLICK_DRAG"};
  Event tool_tweak_event{"LEFTMOUSE", "CLICK_DRAG"};
  Event tool_maybe_tweak_event{"LEFTMOUSE", "CLICK_DRAG"};

  /** Recalcula los derivados. Hay que llamarlo tras tocar cualquier opcion. */
  void finalize();
};

/** Los parametros con los que se construye el keymap por defecto. */
const Params &default_params();

}  // namespace flipendo::keymap

#endif /* __FL_KEYMAP_PARAMS_HPP__ */
