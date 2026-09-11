/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 *
 * Comprobacion reproducible de los operadores de malla/pintura migrados a C++
 * (Carril C, Flipendo). De momento cubre `paint.vertex_color_dirt`.
 *
 * Mismo patron que `--fl-dump-keymap`, `--fl-dump-tools` y
 * `--fl-selftest-object-ops`: construye escenas deterministas llamando SOLO a
 * operadores por su idname (nunca a la funcion C++ directamente), ejecuta el operador
 * bajo prueba y vuelca el atributo de color resultante elemento a elemento. Como todo
 * se identifica por idname, el mismo volcado sirve para el binario con el Python
 * todavia activo y para el binario ya migrado: si coinciden, la migracion es
 * observacionalmente identica.
 *
 * Uso: Blender -b --fl-selftest-mesh-ops <fichero>
 * Linea base: tests/flipendo/meshops/baseline-python.txt
 */

#pragma once

struct bContext;

namespace flipendo::mesh_ops_selftest {

/** Construye las escenas de prueba, invoca los operadores y escribe el volcado. */
bool dump(bContext *C, const char *filepath);

/** Vuelca en un fichero temporal y lo compara con `baseline_path`; informa por stderr
 * cuantos elementos de color coinciden por caso y en total. Devuelve true si no hay
 * ni una linea distinta. */
bool check(bContext *C, const char *baseline_path);

/** Lo mismo para `mesh.faces_mirror_uv`. Linea base:
 * `tests/flipendo/mirroruv/baseline-python.txt`. */
bool dump_mirror_uv(bContext *C, const char *filepath);
bool check_mirror_uv(bContext *C, const char *baseline_path);

}  // namespace flipendo::mesh_ops_selftest
