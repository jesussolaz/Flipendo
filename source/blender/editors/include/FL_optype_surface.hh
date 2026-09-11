/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 *
 * Volcado de la "superficie de registro" de los operadores que el carril C migra de
 * Python a C++: idname, nombre visible, descripcion, contexto de traduccion y, por cada
 * propiedad, identificador, tipo, subtipo, longitud de array, nombre y descripcion
 * visibles, valor por defecto, rangos duros y blandos, y los items de los enums.
 *
 * Es la comprobacion que exige la doctrina ("mismos identificadores ... mismas
 * propiedades (nombre, tipo, subtipo, defecto, rangos, enums, flags)") y, a diferencia
 * de ejecutar el operador, se puede hacer en `--background` para TODOS ellos, incluidos
 * los que solo tienen sentido con una region de vista 3D debajo (los
 * `view3d.edit_mesh_extrude_*`, que arrancan un transform modal).
 *
 * Uso: Blender -b --fl-dump-optypes <fichero>
 *      Blender -b --fl-check-optypes <linea-base>
 * Linea base: tests/flipendo/optypes/baseline-python.txt
 */

#pragma once

struct bContext;

namespace flipendo::optype_surface {

bool dump(bContext *C, const char *filepath);
bool check(bContext *C, const char *baseline_path);

/**
 * Lo mismo para los operadores de `scripts/startup/bl_operators/presets.py`, con su
 * propia lista y su propia linea base (`tests/flipendo/presetops/baseline-python.txt`).
 *
 * Va aparte, y no anadiendo idnames a la lista de arriba, porque aquella linea base
 * esta congelada contra un binario que ya no se puede reconstruir: sus `.py` se
 * retiraron. Este volcado ademas anade las BANDERAS -- las del tipo de operador
 * (`REGISTER`, `INTERNAL`) y las de cada propiedad (`HIDDEN`, `SKIP_SAVE`,
 * `ANIMATABLE`...) --, que la doctrina exige y el volcado v1 no llevaba. La trampa que
 * cazan: una propiedad declarada desde Python NO es animable y una declarada con
 * `RNA_def_boolean()` en C++ SI lo es, asi que sin `RNA_def_property_clear_flag()` el
 * contrato cambia sin que se note.
 *
 * Uso: Blender -b --fl-dump-preset-optypes <fichero>
 *      Blender -b --fl-check-preset-optypes <linea-base>
 */
bool dump_presets(bContext *C, const char *filepath);
bool check_presets(bContext *C, const char *baseline_path);

/**
 * Y lo mismo para los tres operadores de niveles de detalle de `bl_operators/object.py`.
 * Linea base: `tests/flipendo/lod/optypes-python.txt`.
 */
bool dump_lod(bContext *C, const char *filepath);
bool check_lod(bContext *C, const char *baseline_path);

}  // namespace flipendo::optype_surface
