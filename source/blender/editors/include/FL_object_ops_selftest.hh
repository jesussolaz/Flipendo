/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 *
 * Comprobacion reproducible de los operadores de objeto/malla migrados a C++
 * (Carril C, Flipendo: `object.align`, `object.randomize_transform`,
 * `mesh.primitive_torus_add`).
 *
 * Mismo patron que `--fl-dump-keymap`/`--fl-dump-tools`: construye una escena
 * determinista, invoca cada operador POR SU IDNAME (con `WM_operator_name_call`,
 * igual que lo haria un `.blend` o un atajo de teclado — no llama a la funcion en
 * C++ directamente), y vuelca el estado numerico resultante. Como identifica los
 * operadores por idname, el mismo volcado sirve para el binario con el Python
 * todavia activo y para el binario ya migrado: si ambos coinciden, la migracion
 * es observacionalmente identica.
 *
 * Uso: Blender -b --fl-selftest-object-ops <fichero>
 * Linea base: tests/flipendo/objectops/baseline-python.txt
 */

#pragma once

struct bContext;

namespace flipendo::object_ops_selftest {

/** Construye la escena de prueba, invoca los operadores y escribe el volcado. */
bool dump(bContext *C, const char *filepath);

}  // namespace flipendo::object_ops_selftest
