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
#include "FL_keymap_params.hpp"

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

/* Cada grupo vive en su propio fichero (fl_keymap_gNN.cc) y registra las funciones
 * km_* que le tocan. El reparto es solo para que el fichero no sea inmanejable: el
 * orden de registro no importa entre keymaps distintos. */
void register_group_01(wmKeyConfig *kc, const Params &params);
void register_group_02(wmKeyConfig *kc, const Params &params);
void register_group_03(wmKeyConfig *kc, const Params &params);
void register_group_04(wmKeyConfig *kc, const Params &params);
void register_group_05(wmKeyConfig *kc, const Params &params);
void register_group_06(wmKeyConfig *kc, const Params &params);
void register_group_07(wmKeyConfig *kc, const Params &params);
void register_group_08(wmKeyConfig *kc, const Params &params);
void register_group_09(wmKeyConfig *kc, const Params &params);
void register_group_10(wmKeyConfig *kc, const Params &params);
void register_group_11(wmKeyConfig *kc, const Params &params);
void register_group_12(wmKeyConfig *kc, const Params &params);

/* Cada grupo vive en su propio fichero (fl_keymap_gNN.cc) y registra las funciones
 * km_* que le tocan. El reparto es solo para que el fichero no sea inmanejable: el
 * orden de registro no importa entre keymaps distintos. */
void register_group_01(wmKeyConfig *kc, const Params &params);
void register_group_02(wmKeyConfig *kc, const Params &params);
void register_group_03(wmKeyConfig *kc, const Params &params);
void register_group_04(wmKeyConfig *kc, const Params &params);
void register_group_05(wmKeyConfig *kc, const Params &params);
void register_group_06(wmKeyConfig *kc, const Params &params);
void register_group_07(wmKeyConfig *kc, const Params &params);
void register_group_08(wmKeyConfig *kc, const Params &params);
void register_group_09(wmKeyConfig *kc, const Params &params);
void register_group_10(wmKeyConfig *kc, const Params &params);
void register_group_11(wmKeyConfig *kc, const Params &params);
void register_group_12(wmKeyConfig *kc, const Params &params);
void register_group_13(wmKeyConfig *kc, const Params &params);
void register_group_14(wmKeyConfig *kc, const Params &params);
void register_group_15(wmKeyConfig *kc, const Params &params);
void register_group_16(wmKeyConfig *kc, const Params &params);
void register_group_17(wmKeyConfig *kc, const Params &params);
void register_group_18(wmKeyConfig *kc, const Params &params);
void register_group_19(wmKeyConfig *kc, const Params &params);
void register_group_20(wmKeyConfig *kc, const Params &params);
void register_group_21(wmKeyConfig *kc, const Params &params);
void register_group_22(wmKeyConfig *kc, const Params &params);
void register_group_23(wmKeyConfig *kc, const Params &params);
void register_group_24(wmKeyConfig *kc, const Params &params);
void register_group_25(wmKeyConfig *kc, const Params &params);
void register_group_26(wmKeyConfig *kc, const Params &params);
void register_group_27(wmKeyConfig *kc, const Params &params);
void register_group_28(wmKeyConfig *kc, const Params &params);

void register_default(wmKeyConfig *kc)
{
  /* Se leen las preferencias del usuario en cada reconstruccion; el keymap se
   * rehace entero cuando cambian. */
  register_default_with_params(kc, params_from_preferences());
}

void register_default_with_params(wmKeyConfig *kc, const Params &params)
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

  register_group_01(kc, params);
  register_group_02(kc, params);
  register_group_03(kc, params);
  register_group_04(kc, params);
  register_group_05(kc, params);
  register_group_06(kc, params);
  register_group_07(kc, params);
  register_group_08(kc, params);
  register_group_09(kc, params);
  register_group_10(kc, params);
  register_group_11(kc, params);
  register_group_12(kc, params);
  register_group_13(kc, params);
  register_group_14(kc, params);
  register_group_15(kc, params);
  register_group_16(kc, params);
  register_group_17(kc, params);
  register_group_18(kc, params);
  register_group_19(kc, params);
  register_group_20(kc, params);
  register_group_21(kc, params);
  register_group_22(kc, params);
  register_group_23(kc, params);
  register_group_24(kc, params);
  register_group_25(kc, params);
  register_group_26(kc, params);
  register_group_27(kc, params);
  register_group_28(kc, params);
}

}  // namespace flipendo::keymap
