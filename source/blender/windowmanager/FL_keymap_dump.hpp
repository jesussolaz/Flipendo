/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Volcado del mapa de teclado a texto, para comparar configuraciones.
 *
 * Existe por la migracion del keymap por defecto de Python a C++. Hoy el keymap
 * sale de `scripts/presets/keyconfig/keymap_data/blender_default.py` (8.669 lineas)
 * ejecutado desde `WM_keyconfig_reload`; el objetivo es que salga de C++. La unica
 * forma barata de garantizar que no se pierde ni un atajo por el camino es volcar el
 * resultado ANTES y DESPUES y comparar los dos ficheros.
 *
 * El formato es estable y ordenado a proposito: una linea por elemento, sin punteros
 * ni direcciones, para que `diff` sea concluyente. El orden de los elementos DENTRO
 * de un keymap se conserva tal cual porque es semantico — gana el primero que casa.
 *
 * Uso:  Blender --background --fl-dump-keymap <fichero>
 */

#ifndef __FL_KEYMAP_DUMP_HPP__
#define __FL_KEYMAP_DUMP_HPP__

struct wmWindowManager;

/**
 * Escribe en `filepath` el volcado de las configuraciones de teclado de `wm`.
 * Devuelve false si no se pudo escribir.
 */
bool FL_keyconfig_dump(const wmWindowManager *wm, const char *filepath);

#endif /* __FL_KEYMAP_DUMP_HPP__ */
