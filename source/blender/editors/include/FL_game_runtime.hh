/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 *
 * FL_game_runtime — publicacion del juego, en C++.
 *
 * Sustituye a los addons de UPBGE `game_engine_save_as_runtime_eevee.py` y
 * `game_engine_publishing.py`, que hacian en Python el flujo que el motor vende
 * como producto: copiar el Player, meterle el `.blend` dentro y dejar un
 * ejecutable autonomo. Escribirlo en Python era una contradiccion: el Player de
 * distribucion se compila SIN CPython (`politicas/PLAYER-SIN-CPYTHON.md`), de
 * modo que el motor exportaba juegos sin interprete con una herramienta que
 * necesitaba interprete.
 *
 * Doctrina: `politicas/LENGUAJE-CPP.md`, `politicas/ADDONS-MOTOR-A-CPP.md`.
 */

#pragma once

struct wmOperatorType;

namespace flipendo::game {

/** Registra los operadores nativos de publicacion del juego. */
void operatortypes_register();

/**
 * Vuelca el estado observable de un bundle exportado y lo escribe en
 * `filepath_out` (o por `stdout` si es `nullptr`).
 *
 * Formato (una linea por dato, orden estable):
 *
 * \code
 * FL-RUNTIME-DUMP 1
 * BUNDLE <nombre del .app>
 * FILES <numero de ficheros regulares>
 * LINKS <numero de enlaces simbolicos>
 * DIRS <numero de directorios>
 * BYTES <suma de tamanos de ficheros regulares>
 * PAYLOAD <ruta relativa del .blend> <bytes>
 * COMPONENT <objeto> <valor de fl_component>
 * COMPONENTS <n>
 * ENTRY <ruta relativa> <bytes>
 * \endcode
 *
 * Es el verificador del carril: se corre sobre el bundle que produce el camino
 * Python y sobre el que produce este C++, y las dos salidas tienen que ser
 * identicas salvo el nombre del bundle.
 *
 * \return true si el bundle se pudo leer entero.
 */
bool runtime_dump(const char *bundle_path, const char *filepath_out);

}  // namespace flipendo::game
