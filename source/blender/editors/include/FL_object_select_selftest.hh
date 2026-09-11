/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 *
 * Comprobacion reproducible de los operadores de seleccion migrados desde
 * `bl_operators/object.py` (Carril C, Flipendo): `object.select_pattern`,
 * `object.select_camera` y `object.select_hierarchy`.
 *
 * Mismo patron que `--fl-selftest-mesh-ops` y `--fl-selftest-rigidbody-ops`: escenas
 * deterministas construidas llamando SOLO a operadores por su idname, y volcado del
 * estado observable (para cada objeto: nombre, seleccion, si es el activo, si esta
 * oculto y quien es su padre).
 *
 * Uso: Blender -b --fl-selftest-object-select <fichero>
 *      Blender -b --fl-check-object-select <linea-base>
 * Linea base: tests/flipendo/objectselect/baseline-python.txt
 */

#pragma once

struct bContext;

namespace flipendo::object_select_selftest {

bool dump(bContext *C, const char *filepath);
bool check(bContext *C, const char *baseline_path);

/** Lo mismo para `object.make_dupli_face`. Linea base:
 * `tests/flipendo/dupliface/baseline-python.txt`. */
bool dump_dupli_face(bContext *C, const char *filepath);
bool check_dupli_face(bContext *C, const char *baseline_path);

}  // namespace flipendo::object_select_selftest
