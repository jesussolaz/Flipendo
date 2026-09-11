/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editor_physics
 *
 * Comprobacion reproducible de los operadores de cuerpo rigido migrados desde
 * `bl_operators/rigidbody.py` (Carril C, Flipendo): `rigidbody.object_settings_copy` y
 * `rigidbody.connect`.
 *
 * Mismo patron que `--fl-selftest-mesh-ops`: escenas deterministas construidas llamando
 * SOLO a operadores por su idname, y volcado del estado observable (seleccion, objeto
 * activo, ajustes de cuerpo rigido leidos POR RNA con los mismos identificadores que
 * usaba el Python, y los constraints generados).
 *
 * `rigidbody.bake_to_keyframes` NO esta aqui, y no por pereza: depende de
 * `anim.keyframe_insert_by_name`, cuyo `poll` (`modify_key_op_poll`) exige un `ScrArea`
 * activo. Ni `--background` ni `-P` en modo grafico lo proporcionan, asi que NI SIQUIERA
 * EL ORIGINAL EN PYTHON se puede invocar desde un guion. Queda como deuda escrita en
 * `politicas/RIGIDBODY-A-CPP.md`.
 *
 * Uso: Blender -b --fl-selftest-rigidbody-ops <fichero>
 *      Blender -b --fl-check-rigidbody-ops <linea-base>
 * Linea base: tests/flipendo/rigidbody/baseline-python.txt
 */

#pragma once

struct bContext;

namespace flipendo::rigidbody_ops_selftest {

/** Construye las escenas de prueba, invoca los operadores y escribe el volcado. */
bool dump(bContext *C, const char *filepath);

/** Vuelca y compara con `baseline_path`; informa por stderr con las cifras por caso. */
bool check(bContext *C, const char *baseline_path);

}  // namespace flipendo::rigidbody_ops_selftest
