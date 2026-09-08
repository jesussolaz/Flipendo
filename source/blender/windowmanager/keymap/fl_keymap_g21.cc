/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Mapa de teclado, grupo 21: modales de pintura y escultura (trazo, Expand, filtro de
 * malla), modales del lapiz de curvas, de los enlaces y el redimensionado de nodos y
 * del deslizamiento en el secuenciador; los keymaps genericos de gizmo (incluido su
 * modal) y el menu emergente de la barra de herramientas.
 * Transliterado de blender_default.py.
 */

#include "FL_keymap_build.hpp"
#include "FL_keymap_params.hpp"

/* Solo para el arreglo de `any_except("alt")` en los gizmos: hay que tocar los
 * modificadores del atajo uno a uno, y eso pide la struct y las constantes KM_*. */
#include "DNA_windowmanager_types.h"
#include "WM_types.hh"

namespace flipendo::keymap {

/* -------------------------------------------------------------------- */
/** \name Modales de pintura y escultura
 * \{ */

static void km_paint_stroke_modal(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap_modal(kc, "Paint Stroke Modal");

  item_modal(km, "CANCEL", ev("ESC", "PRESS").any());
}

static void km_sculpt_expand_modal(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap_modal(kc, "Sculpt Expand Modal");

  item_modal(km, "CANCEL", ev("ESC", "PRESS").any());
  item_modal(km, "CANCEL", ev("RIGHTMOUSE", "PRESS").any());
  item_modal(km, "CONFIRM", ev("LEFTMOUSE", "PRESS").any());
  item_modal(km, "CONFIRM", ev("LEFTMOUSE", "RELEASE").any());
  item_modal(km, "INVERT", ev("F", "PRESS").any());
  item_modal(km, "PRESERVE", ev("E", "PRESS").any());
  item_modal(km, "GRADIENT", ev("G", "PRESS").any());
  item_modal(km, "RECURSION_STEP_GEODESIC", ev("R", "PRESS"));
  item_modal(km, "RECURSION_STEP_TOPOLOGY", ev("R", "PRESS").alt());
  item_modal(km, "MOVE_TOGGLE", ev("SPACE", "ANY").any());

  /* El Python genera aqui la misma tanda de cuatro caidas dos veces, primero con la
   * fila de numeros y luego con el teclado numerico (NUMBERS_1 y NUMPAD_1); el orden
   * importa, asi que se conserva el bucle exterior. */
  {
    const char *const falloff[4] = {
        "FALLOFF_GEODESICS",
        "FALLOFF_TOPOLOGY",
        "FALLOFF_TOPOLOGY_DIAGONALS",
        "FALLOFF_SPHERICAL",
    };
    const char *const numbers_1[4] = {"ONE", "TWO", "THREE", "FOUR"};
    const char *const numpad_1[4] = {"NUMPAD_1", "NUMPAD_2", "NUMPAD_3", "NUMPAD_4"};

    for (const char *const *numseq_1 : {numbers_1, numpad_1}) {
      for (int i = 0; i < 4; i++) {
        item_modal(km, falloff[i], ev(numseq_1[i], "PRESS").any());
      }
    }
  }

  item_modal(km, "SNAP_TOGGLE", ev("LEFT_CTRL", "ANY"));
  item_modal(km, "SNAP_TOGGLE", ev("RIGHT_CTRL", "ANY"));
  item_modal(km, "LOOP_COUNT_INCREASE", ev("W", "PRESS").any().repeat());
  item_modal(km, "LOOP_COUNT_DECREASE", ev("Q", "PRESS").any().repeat());
  item_modal(km, "BRUSH_GRADIENT_TOGGLE", ev("B", "PRESS").any());
  item_modal(km, "TEXTURE_DISTORTION_INCREASE", ev("Y", "PRESS"));
  item_modal(km, "TEXTURE_DISTORTION_DECREASE", ev("T", "PRESS"));
}

static void km_sculpt_mesh_filter_modal_map(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap_modal(kc, "Mesh Filter Modal Map");

  item_modal(km, "CONFIRM", ev("LEFTMOUSE", "PRESS").any());
  item_modal(km, "CONFIRM", ev("LEFTMOUSE", "RELEASE").any());
  item_modal(km, "CONFIRM", ev("RET", "RELEASE").any());
  item_modal(km, "CONFIRM", ev("NUMPAD_ENTER", "RELEASE").any());

  item_modal(km, "CANCEL", ev("ESC", "PRESS").any());
  item_modal(km, "CANCEL", ev("RIGHTMOUSE", "PRESS").any());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Modales de curvas, nodos y secuenciador
 * \{ */

static void km_curve_pen_modal_map(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap_modal(kc, "Curve Pen Modal Map");

  item_modal(km, "FREE_ALIGN_TOGGLE", ev("LEFT_SHIFT", "ANY").any());
  item_modal(km, "MOVE_ADJACENT", ev("LEFT_CTRL", "ANY").any());
  item_modal(km, "MOVE_ENTIRE", ev("SPACE", "ANY").any());
  item_modal(km, "LOCK_ANGLE", ev("LEFT_ALT", "ANY").any());
  item_modal(km, "LINK_HANDLES", ev("RIGHT_CTRL", "PRESS").any());
}

static void km_node_link_modal_map(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap_modal(kc, "Node Link Modal Map");

  item_modal(km, "BEGIN", ev("LEFTMOUSE", "PRESS").any());
  item_modal(km, "CONFIRM", ev("LEFTMOUSE", "RELEASE").any());
  item_modal(km, "CANCEL", ev("RIGHTMOUSE", "PRESS").any());
  item_modal(km, "CANCEL", ev("ESC", "PRESS").any());
  item_modal(km, "SWAP", ev("LEFT_ALT", "ANY").any());
  item_modal(km, "SWAP", ev("RIGHT_ALT", "ANY").any());
}

static void km_node_resize_modal_map(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap_modal(kc, "Node Resize Modal Map");

  item_modal(km, "BEGIN", ev("LEFTMOUSE", "PRESS").any());
  item_modal(km, "CANCEL", ev("RIGHTMOUSE", "PRESS").any());
  item_modal(km, "CANCEL", ev("ESC", "PRESS").any());
  item_modal(km, "SNAP_INVERT_ON", ev("RIGHT_CTRL", "PRESS").any());
  item_modal(km, "SNAP_INVERT_OFF", ev("RIGHT_CTRL", "RELEASE").any());
  item_modal(km, "SNAP_INVERT_ON", ev("LEFT_CTRL", "PRESS").any());
  item_modal(km, "SNAP_INVERT_OFF", ev("LEFT_CTRL", "RELEASE").any());
}

static void km_sequencer_slip_modal_map(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap_modal(kc, "Slip Modal");

  item_modal(km, "CONFIRM", ev("LEFTMOUSE", "PRESS").any());
  item_modal(km, "CONFIRM", ev("LEFTMOUSE", "RELEASE").any());
  item_modal(km, "CONFIRM", ev("RET", "RELEASE").any());
  item_modal(km, "CONFIRM", ev("NUMPAD_ENTER", "RELEASE").any());
  item_modal(km, "CONFIRM", ev("SPACE", "RELEASE").any());
  item_modal(km, "CANCEL", ev("RIGHTMOUSE", "ANY").any());
  item_modal(km, "CANCEL", ev("ESC", "PRESS").any());
  item_modal(km, "PRECISION_ENABLE", ev("LEFT_SHIFT", "PRESS").any());
  item_modal(km, "PRECISION_DISABLE", ev("LEFT_SHIFT", "RELEASE").any());
  item_modal(km, "PRECISION_ENABLE", ev("RIGHT_SHIFT", "PRESS").any());
  item_modal(km, "PRECISION_DISABLE", ev("RIGHT_SHIFT", "RELEASE").any());
  item_modal(km, "CLAMP_TOGGLE", ev("C", "RELEASE").any());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Gizmos sin keymap propio
 * \{ */

/*
 * TODO(keymap): `Event` no sabe expresar `any_except(...)` del Python, que pone
 * "cualquier estado" en unos modificadores y "sin pulsar" en otros; solo tiene
 * `.any()`, que los pone todos. Falta algo como `.ctrl_any()` / `.shift_any()` en
 * FL_keymap_build.hpp (el DNA ya lo admite: cada modificador guarda KM_ANY,
 * KM_NOTHING o KM_MOD_HELD por separado). Mientras tanto, esta plantilla ajusta el
 * atajo recien creado a mano, que es lo unico que reproduce el baseline.
 */

/**
 * `_template_items_gizmo_tweak_value*()` del Python, para un valor de evento.
 *
 * El evento original es
 * `{"type": 'LEFTMOUSE', "value": <value>, **any_except("alt")}`: ctrl, shift, oskey e
 * hyper en cualquier estado y alt exigido SIN pulsar.
 *
 * Como el Python declara "ctrl" en el evento, el pase de macOS
 * (`keyconfig_data_oskey_from_ctrl_for_macos`) le antepone un gemelo que mueve ese
 * valor de ctrl a oskey. El andamiaje no puede generarlo aqui, porque
 * `oskey_twin_wanted()` mira el booleano `ctrl` del Event y esta a false; por eso se
 * escriben los dos atajos a mano, primero el gemelo y despues el original, que es el
 * orden que produce el Python.
 */
static void gizmo_tweak_value(wmKeyMap *km, const char *value)
{
  /* Gemelo de macOS: Cmd en cualquier estado, Ctrl sin pulsar. */
  wmKeyMapItem *kmi_oskey = item(km, "gizmogroup.gizmo_tweak", ev("LEFTMOUSE", value)).raw();
  kmi_oskey->shift = KM_ANY;
  kmi_oskey->ctrl = KM_NOTHING;
  kmi_oskey->alt = KM_NOTHING;
  kmi_oskey->oskey = KM_ANY;
  kmi_oskey->hyper = KM_ANY;

  wmKeyMapItem *kmi = item(km, "gizmogroup.gizmo_tweak", ev("LEFTMOUSE", value)).raw();
  kmi->shift = KM_ANY;
  kmi->ctrl = KM_ANY;
  kmi->alt = KM_NOTHING;
  kmi->oskey = KM_ANY;
  kmi->hyper = KM_ANY;
}

static void km_generic_gizmo(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap(kc, "Generic Gizmo", "EMPTY", "WINDOW");

  gizmo_tweak_value(km, "PRESS");
}

static void km_generic_gizmo_drag(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap(kc, "Generic Gizmo Drag", "EMPTY", "WINDOW");

  gizmo_tweak_value(km, "CLICK_DRAG");
}

static void km_generic_gizmo_click_drag(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap(kc, "Generic Gizmo Click Drag", "EMPTY", "WINDOW");

  gizmo_tweak_value(km, "CLICK");
  gizmo_tweak_value(km, "CLICK_DRAG");
}

static void km_generic_gizmo_maybe_drag(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Generic Gizmo Maybe Drag", "EMPTY", "WINDOW");

  if (params.use_gizmo_drag) {
    gizmo_tweak_value(km, "CLICK_DRAG");
  }
  else {
    gizmo_tweak_value(km, "PRESS");
  }
}

static void km_generic_gizmo_select(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap(kc, "Generic Gizmo Select", "EMPTY", "WINDOW");

  /* El Python deja aqui un "TODO, currently in C code". */
  gizmo_tweak_value(km, "PRESS");
}

static void km_generic_gizmo_tweak_modal_map(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap_modal(kc, "Generic Gizmo Tweak Modal Map");

  item_modal(km, "CANCEL", ev("ESC", "PRESS").any());
  item_modal(km, "CANCEL", ev("RIGHTMOUSE", "PRESS").any());
  item_modal(km, "CONFIRM", ev("RET", "PRESS").any());
  item_modal(km, "CONFIRM", ev("NUMPAD_ENTER", "PRESS").any());
  item_modal(km, "PRECISION_ON", ev("RIGHT_SHIFT", "PRESS").any());
  item_modal(km, "PRECISION_OFF", ev("RIGHT_SHIFT", "RELEASE").any());
  item_modal(km, "PRECISION_ON", ev("LEFT_SHIFT", "PRESS").any());
  item_modal(km, "PRECISION_OFF", ev("LEFT_SHIFT", "RELEASE").any());
  item_modal(km, "SNAP_ON", ev("RIGHT_CTRL", "PRESS").any());
  item_modal(km, "SNAP_OFF", ev("RIGHT_CTRL", "RELEASE").any());
  item_modal(km, "SNAP_ON", ev("LEFT_CTRL", "PRESS").any());
  item_modal(km, "SNAP_OFF", ev("LEFT_CTRL", "RELEASE").any());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Keymaps emergentes
 * \{ */

static void km_popup_toolbar(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap(kc, "Toolbar Popup", "EMPTY", "TEMPORARY");

  item_tool(km, "builtin.cursor", ev("SPACE", "PRESS"));
  item_tool(km, "builtin.select", ev("W", "PRESS"));
  item_tool(km, "builtin.select_lasso", ev("L", "PRESS"));
  item_tool(km, "builtin.transform", ev("T", "PRESS"));
  item_tool(km, "builtin.measure", ev("M", "PRESS"));
}

/** \} */

void register_group_21(wmKeyConfig *kc, const Params &params)
{
  km_paint_stroke_modal(kc, params);
  km_sculpt_expand_modal(kc, params);
  km_sculpt_mesh_filter_modal_map(kc, params);
  km_curve_pen_modal_map(kc, params);
  km_node_link_modal_map(kc, params);
  km_node_resize_modal_map(kc, params);
  km_sequencer_slip_modal_map(kc, params);
  km_generic_gizmo(kc, params);
  km_generic_gizmo_drag(kc, params);
  km_generic_gizmo_click_drag(kc, params);
  km_generic_gizmo_maybe_drag(kc, params);
  km_generic_gizmo_select(kc, params);
  km_generic_gizmo_tweak_modal_map(kc, params);
  km_popup_toolbar(kc, params);
}

}  // namespace flipendo::keymap
