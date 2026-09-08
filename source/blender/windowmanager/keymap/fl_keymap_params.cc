/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Calculo de los parametros derivados del keymap. Ver FL_keymap_params.hpp.
 * Transliteracion de `Params.__init__` (blender_default.py:104-224).
 */

#include "FL_keymap_params.hpp"

namespace flipendo::keymap {

void Params::finalize()
{
  if (select_mouse_right) {
    /* Seleccion con el boton derecho. */
    select_mouse = "RIGHTMOUSE";
    select_mouse_value = "PRESS";
    action_mouse = "LEFTMOUSE";
    tool_mouse = "LEFTMOUSE";
    tool_maybe_tweak_value = use_alt_tool_or_cursor ? "PRESS" : "CLICK_DRAG";

    context_menu_event = Event("W", "PRESS");

    if (use_alt_tool_or_cursor) {
      cursor_set_event = Event("LEFTMOUSE", "PRESS").alt();
      cursor_tweak_event = Event("LEFTMOUSE", "CLICK_DRAG").alt();
      has_cursor_tweak_event = true;
    }
    else {
      cursor_set_event = Event("LEFTMOUSE", "CLICK");
      has_cursor_tweak_event = false;
    }

    tool_modifier_alt_any = false;
  }
  else {
    /* Seleccion con el izquierdo: usa CLICK para poder distinguir click de arrastre
     * sobre el mismo boton. */
    select_mouse = "LEFTMOUSE";
    select_mouse_value = "CLICK";
    action_mouse = "RIGHTMOUSE";
    tool_mouse = "LEFTMOUSE";
    tool_maybe_tweak_value = "CLICK_DRAG";

    context_menu_event = legacy ? Event("W", "PRESS") : Event("RIGHTMOUSE", "PRESS");

    cursor_set_event = Event("RIGHTMOUSE", "PRESS").shift();
    cursor_tweak_event = Event("RIGHTMOUSE", "CLICK_DRAG").shift();
    has_cursor_tweak_event = true;

    tool_modifier_alt_any = use_alt_tool_or_cursor;
  }

  use_tweak_select_passthrough = !legacy;

  /* Con seleccion por el izquierdo, la herramienta de reserva siempre se considera
   * ya cubierta por las acciones de seleccion. */
  if (!select_mouse_right) {
    use_fallback_tool_select_handled = true;
    use_fallback_tool_select_mouse = true;
  }
  else {
    use_fallback_tool_select_mouse = !use_fallback_tool_select_handled;
  }

  select_mouse_value_fallback = use_fallback_tool_select_handled ? select_mouse_value : "CLICK";
  pie_value = use_pie_click_drag ? "CLICK_DRAG" : "PRESS";

  select_tweak_event = Event(select_mouse, "CLICK_DRAG");
  tool_tweak_event = Event(tool_mouse, "CLICK_DRAG");
  tool_maybe_tweak_event = Event(tool_mouse, tool_maybe_tweak_value);
}

Event with_tool_modifier(const Params &params, Event event)
{
  if (params.tool_modifier_alt_any) {
    event.alt_any();
  }
  return event;
}

const Params &default_params()
{
  static Params params = [] {
    Params p;
    p.finalize();
    return p;
  }();
  return params;
}

}  // namespace flipendo::keymap
