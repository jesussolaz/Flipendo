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

#include "FL_keymap_params.hpp"

struct wmKeyConfig;

namespace flipendo::keymap {

/** Registra en `kc` todos los keymaps por defecto ya transliterados. */
void register_default(wmKeyConfig *kc);

/**
 * Igual, pero con unos `Params` dados en vez de los de las preferencias del usuario.
 *
 * Existe porque el keymap **depende de 17 preferencias**, y algunas cambian el NOMBRE
 * del menu que abre una tecla (`VIEW3D_MT_snap` frente a `VIEW3D_MT_snap_pie`,
 * `VIEW3D_MT_shading_pie` frente a `_ex_pie`...). Comprobar solo la configuracion de
 * fabrica dejaria sin mirar justo los menus que solo ve quien cambia una preferencia
 * — y esos fallan igual de callados. Lo usa `--fl-check-keymap-menus`.
 */
void register_default_with_params(wmKeyConfig *kc, const Params &params);

}  // namespace flipendo::keymap

#endif /* __FL_KEYMAP_DEFAULT_HPP__ */
