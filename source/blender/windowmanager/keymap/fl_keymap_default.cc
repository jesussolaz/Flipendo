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

void register_default(wmKeyConfig *kc)
{
  km_logic(kc);
}

}  // namespace flipendo::keymap
