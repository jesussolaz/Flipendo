/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Mapa de teclado por defecto, en C++.
 *
 * Transliteracion de `scripts/presets/keyconfig/keymap_data/blender_default.py`.
 * Cada funcion `km_*` de aqui corresponde una a una con la del Python y conserva su
 * orden de atajos, que es semantico: gana el primero que casa.
 *
 * Se verifica comparando `--fl-dump-keymap` contra
 * `tests/flipendo/keymap/baseline-python.txt`.
 */

#include "FL_keymap_default.hpp"

#include "FL_keymap_build.hpp"

namespace flipendo::keymap {

/* -------------------------------------------------------------------- */
/** \name Editor de logica (Flipendo)
 *
 * Corresponde a `km_logic()` del Python.
 * \{ */

static void km_logic(wmKeyConfig *kc)
{
  wmKeyMap *km = keymap(kc, "Logic Bricks Editor", "LOGIC_EDITOR", "WINDOW");

  item(km, "logic.properties", ev("N", "PRESS"));
  item(km, "logic.region_flip", ev("F5", "PRESS"));
  item(km, "logic.links_cut", ev("RIGHTMOUSE", "ANY").ctrl());
  item(km, "logic.view_all", ev("HOME", "PRESS"));
  item(km, "logic.view_all", ev("NDOF_BUTTON_FIT", "PRESS"));
  item_menu(km, "LOGIC_MT_logicbricks_add", ev("A", "PRESS").shift());
}

/** \} */


/* -------------------------------------------------------------------- */
/** \name Modales de navegacion de la vista 3D
 *
 * `km_view3d_move_modal`, `km_view3d_zoom_modal`, `km_view3d_dolly_modal` y
 * `km_view3d_rotate_modal` del Python.
 * \{ */

static void km_view3d_move_modal(wmKeyConfig *kc)
{
  wmKeyMap *km = keymap_modal(kc, "View3D Move Modal");

  item_modal(km, "CANCEL", ev("RIGHTMOUSE", "PRESS").any());
}

static void km_view3d_zoom_modal(wmKeyConfig *kc)
{
  wmKeyMap *km = keymap_modal(kc, "View3D Zoom Modal");

  item_modal(km, "CANCEL", ev("RIGHTMOUSE", "PRESS").any());
}

static void km_view3d_dolly_modal(wmKeyConfig *kc)
{
  wmKeyMap *km = keymap_modal(kc, "View3D Dolly Modal");

  item_modal(km, "CANCEL", ev("RIGHTMOUSE", "PRESS").any());
}

static void km_view3d_rotate_modal(wmKeyConfig *kc)
{
  wmKeyMap *km = keymap_modal(kc, "View3D Rotate Modal");

  item_modal(km, "CANCEL", ev("RIGHTMOUSE", "PRESS").any());
  item_modal(km, "AXIS_SNAP_ENABLE", ev("LEFT_ALT", "PRESS").any());
  item_modal(km, "AXIS_SNAP_DISABLE", ev("LEFT_ALT", "RELEASE").any());
  item_modal(km, "AXIS_SNAP_ENABLE", ev("RIGHT_ALT", "PRESS").any());
  item_modal(km, "AXIS_SNAP_DISABLE", ev("RIGHT_ALT", "RELEASE").any());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Gestos y barrido temporal
 * \{ */

static void km_gesture_lasso(wmKeyConfig *kc)
{
  wmKeyMap *km = keymap_modal(kc, "Gesture Lasso");

  item_modal(km, "MOVE", ev("SPACE", "ANY").any());
}

static void km_time_scrub(wmKeyConfig *kc)
{
  wmKeyMap *km = keymap(kc, "Time Scrub", "EMPTY", "WINDOW");

  item(km, "anim.change_frame", ev("LEFTMOUSE", "PRESS"));
}

static void km_time_scrub_clip(wmKeyConfig *kc)
{
  wmKeyMap *km = keymap(kc, "Clip Time Scrub", "CLIP_EDITOR", "PREVIEW");

  item(km, "clip.change_frame", ev("LEFTMOUSE", "PRESS"));
}

static void km_screen_region_context_menu(wmKeyConfig *kc)
{
  wmKeyMap *km = keymap(kc, "Region Context Menu", "EMPTY", "WINDOW");

  item(km, "screen.region_context_menu", ev("RIGHTMOUSE", "PRESS"));
}

/** \} */

void register_default(wmKeyConfig *kc)
{
  km_logic(kc);

  km_view3d_move_modal(kc);
  km_view3d_zoom_modal(kc);
  km_view3d_dolly_modal(kc);
  km_view3d_rotate_modal(kc);

  km_gesture_lasso(kc);
  km_time_scrub(kc);
  km_time_scrub_clip(kc);
  km_screen_region_context_menu(kc);
}

}  // namespace flipendo::keymap
