/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Volcado y verificacion del catalogo nativo de herramientas.
 *
 * Igual que con el mapa de teclado, la migracion no se revisa leyendo: se congela lo
 * que produce el Python en `tests/flipendo/toolsystem/baseline-python.txt` y se exige
 * que el catalogo nativo escriba exactamente lo mismo.
 *
 * Uso:  Blender --fl-dump-tools <fichero>
 *       Blender --fl-check-tools tests/flipendo/toolsystem/baseline-python.txt
 *
 * La comprobacion es INCREMENTAL a proposito: solo compara las secciones de espacio y
 * modo que el catalogo nativo ya declara, y lista aparte las que faltan. Asi vale
 * desde la primera herramienta trasladada, en vez de dar rojo hasta el final.
 */

#ifndef __FL_TOOLSYSTEM_DUMP_HPP__
#define __FL_TOOLSYSTEM_DUMP_HPP__

struct bContext;

namespace flipendo::toolsystem {

/** Escribe el catalogo nativo en el formato de la linea base. */
bool dump_native(const bContext *C, const char *filepath);

/** Compara el catalogo nativo contra la linea base. Informa por stdout. */
bool check_native(const bContext *C, const char *baseline_filepath);

}  // namespace flipendo::toolsystem

#endif /* __FL_TOOLSYSTEM_DUMP_HPP__ */
