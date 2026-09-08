/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Mapa de teclado por defecto en C++, sustituto de
 * `scripts/presets/keyconfig/keymap_data/blender_default.py`.
 *
 * Mientras la transliteracion esta a medias esto NO se engancha al arranque: se
 * construye aparte con `--fl-dump-keymap-native` y se compara contra la linea base
 * `tests/flipendo/keymap/baseline-python.txt`. Cuando el volcado coincida, se
 * sustituye la llamada a Python de `WM_keyconfig_reload`.
 */

#ifndef __FL_KEYMAP_DEFAULT_HPP__
#define __FL_KEYMAP_DEFAULT_HPP__

struct wmKeyConfig;

namespace flipendo::keymap {

/** Registra en `kc` todos los keymaps por defecto ya transliterados. */
void register_default(wmKeyConfig *kc);

}  // namespace flipendo::keymap

#endif /* __FL_KEYMAP_DEFAULT_HPP__ */
