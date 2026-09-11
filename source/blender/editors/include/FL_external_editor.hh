/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 *
 * Abrir un fichero en el editor de texto externo, en C++.
 *
 * Sustituye a `scripts/modules/bl_text_utils/external_editor.py` (54 lineas),
 * que era **uno de los cinco puentes C++ -> Python vivos** del arbol y el unico
 * de ellos que implementaba capacidad de usuario: `TEXT_OT_jump_to_file_at_point`
 * (`text_ops.cc`) construia una expresion Python, escapaba la ruta byte a byte
 * en hexadecimal y llamaba a `open_external_editor(filepath, line, column)`.
 *
 * Reproduce cuatro cosas del Python, y ninguna es «llamar a `system()`»:
 * `shlex.split()` en modo POSIX, `string.Template.substitute()` con cinco
 * variables, `subprocess.run(check=True)`, y los tres mensajes de error.
 *
 * Ver politicas/MODULES-Y-EL-INTERPRETE.md.
 */

#pragma once

#include <string>
#include <vector>

struct bContext;

namespace flipendo::text {

/**
 * `shlex.split(s)` de Python en modo POSIX (que es el que usaba el Python).
 *
 * - Los espacios separan; las comillas simples toman todo literal hasta la
 *   siguiente comilla simple.
 * - Dentro de comillas dobles la barra invertida solo escapa `"` y `\`; ante
 *   cualquier otro caracter se queda tal cual (esto es `escapedquotes` de
 *   `shlex`, y es facil de equivocar).
 * - Fuera de comillas la barra invertida escapa el caracter siguiente, sea cual
 *   sea.
 * - Comilla sin cerrar o barra invertida final -> error, como el `ValueError`
 *   de Python.
 *
 * Devuelve false y deja el motivo en `r_error` si el texto esta mal formado.
 */
bool shlex_split(const std::string &text, std::vector<std::string> &r_args, std::string &r_error);

/** Una variable de la plantilla. */
struct TemplateVar {
  const char *name;
  std::string value;
};

/**
 * `string.Template(s).substitute(vars)` de Python.
 *
 * - `$$` es un `$` literal.
 * - `$nombre` y `${nombre}`, con `nombre` = `[_A-Za-z][_A-Za-z0-9]*` (ASCII).
 * - Un `$` seguido de cualquier otra cosa es un marcador invalido -> error,
 *   como el `ValueError` de Python.
 * - Un nombre que no esta en `vars` -> error, como el `KeyError` de Python.
 */
bool template_substitute(const std::string &text,
                         const std::vector<TemplateVar> &vars,
                         std::string &r_out,
                         std::string &r_error);

/**
 * El `open_external_editor()` del Python entero.
 *
 * `line` y `column` llegan en base 0, como los pasaba el C++.
 * Devuelve la cadena vacia si todo fue bien, y el mensaje de error (ya
 * traducido) si no. Misma convencion que tenia la funcion de Python.
 */
std::string open_external_editor(const char *filepath, int line, int column);

/**
 * Vuelca, para cada caso de `<casos>` (lineas
 * `argumentos<TAB>ruta<TAB>linea<TAB>columna`), el `argv` que sale de
 * `shlex_split` + `template_substitute`, o el error. Sirve para comparar contra
 * el Python vivo sin lanzar ningun proceso.
 * `--fl-dump-external-editor <casos> <salida>`.
 */
bool external_editor_dump(const char *cases_path, const char *out_path);

/** Compara contra una linea base congelada con el Python vivo. */
bool external_editor_check(const char *baseline_path);

}  // namespace flipendo::text
