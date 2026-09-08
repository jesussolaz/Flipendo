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

#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BKE_idprop.hh"
#include "BKE_keyconfig.h"

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

/* Las preferencias viven en un grupo de IDProperty; una que el usuario no ha tocado
 * simplemente no esta, y entonces vale su valor de fabrica. */
static bool pref_bool(const IDProperty *group, const char *name, const bool fallback)
{
  if (group == nullptr) {
    return fallback;
  }
  const IDProperty *prop = IDP_GetPropertyFromGroup(const_cast<IDProperty *>(group), name);
  if (prop == nullptr) {
    return fallback;
  }
  if (prop->type == IDP_BOOLEAN || prop->type == IDP_INT) {
    return prop->data.val != 0;
  }
  return fallback;
}

static int pref_enum(const IDProperty *group, const char *name, const int fallback)
{
  if (group == nullptr) {
    return fallback;
  }
  const IDProperty *prop = IDP_GetPropertyFromGroup(const_cast<IDProperty *>(group), name);
  if (prop == nullptr || (prop->type != IDP_INT && prop->type != IDP_BOOLEAN)) {
    return fallback;
  }
  return prop->data.val;
}

Params params_from_preferences()
{
  Params p;

  wmKeyConfigPref *kpt = BKE_keyconfig_pref_ensure(&U, WM_KEYCONFIG_STR_DEFAULT);
  const IDProperty *g = kpt ? kpt->prop : nullptr;

  /* `select_mouse`: 0 = izquierdo (fabrica), 1 = derecho. */
  p.select_mouse_right = pref_enum(g, "select_mouse", 0) == 1;

  const int spacebar = pref_enum(g, "spacebar_action", 0);
  p.spacebar_action = (spacebar == 1) ? SpacebarAction::Tool :
                      (spacebar == 2) ? SpacebarAction::Search :
                                        SpacebarAction::Play;

  /* `tool_key_mode`: 0 = inmediato (fabrica), 1 = activa la herramienta. */
  p.use_key_activate_tools = pref_enum(g, "tool_key_mode", 0) == 1;

  p.use_select_all_toggle = pref_bool(g, "use_select_all_toggle", false);
  p.use_v3d_tab_menu = pref_bool(g, "use_v3d_tab_menu", false);
  p.use_v3d_shade_ex_pie = pref_bool(g, "use_v3d_shade_ex_pie", false);
  p.use_alt_click_leader = pref_bool(g, "use_alt_click_leader", false);
  p.use_pie_click_drag = pref_bool(g, "use_pie_click_drag", false);
  p.use_file_single_click = pref_bool(g, "use_file_single_click", false);
  p.use_alt_navigation = pref_bool(g, "use_alt_navigation", true);
  p.use_region_toggle_pie = pref_bool(g, "use_region_toggle_pie", false);

  p.v3d_tilde_action = (pref_enum(g, "v3d_tilde_action", 0) == 1) ? TildeAction::Gizmo :
                                                                    TildeAction::View;
  p.use_v3d_mmb_pan = pref_enum(g, "v3d_mmb_action", 0) == 1;
  p.v3d_alt_mmb_drag_action = (pref_enum(g, "v3d_alt_mmb_drag_action", 0) == 1) ?
                                  AltMmbDragAction::Absolute :
                                  AltMmbDragAction::Relative;

  /* Estos tres no son preferencias sueltas: `Blender.py:343-372` los derivaba de
   * otras. Se reproduce ahi mismo el calculo. */
  const bool gizmo_drag = pref_enum(g, "gizmo_action", 1) == 1;
  p.use_gizmo_drag = !p.select_mouse_right && gizmo_drag;
  /* use_fallback_tool va fijo a true, no es preferencia. */
  p.use_fallback_tool = true;
  p.use_fallback_tool_select_handled = p.select_mouse_right ?
                                           (pref_enum(g, "rmb_action", 0) != 1) :
                                           false;
  p.use_alt_tool_or_cursor = p.select_mouse_right ?
                                 pref_bool(g, "use_alt_cursor", false) :
                                 pref_bool(g, "use_alt_tool", false);

  p.finalize();
  return p;
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
