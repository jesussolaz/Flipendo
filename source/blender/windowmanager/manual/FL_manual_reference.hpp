/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * La tabla del manual en linea, como DATOS.
 *
 * `scripts/modules/rna_manual_reference.py` eran 4.272 lineas de Python que no
 * ejecutaban nada: una tupla autogenerada de 4.253 pares
 * (patron de ruta RNA -> sufijo de URL del manual), ordenada por longitud de
 * patron descendente, que el editor cargaba con `bpy.utils.execfile()` cada vez
 * que el usuario pulsaba «Ver manual en linea» en el menu contextual de un
 * boton (o F1 / Alt-F1, que estan en el keymap nativo).
 *
 * Aqui esa tabla es un fichero de datos instalado en
 * `datafiles/manual/rna_manual_reference.txt` y este modulo es su lector.
 *
 * Ver politicas/MODULES-Y-EL-INTERPRETE.md.
 */

#pragma once

#include <optional>
#include <string>
#include <vector>

struct bContext;

namespace flipendo::manual {

/** Version del formato de la tabla que escribe y entiende esta build. */
constexpr int FORMAT_VERSION = 1;

struct Entry {
  /** Patron estilo `fnmatch` sobre la ruta RNA en minusculas. */
  std::string pattern;
  /** Sufijo de URL relativo al prefijo del manual. */
  std::string url_suffix;
  /**
   * Longitud del prefijo literal del patron (hasta el primer `*`, `?` o `[`).
   * Es la optimizacion que el Python hacia con una expresion regular
   * (`^[^?\*\[]+`), y NO es solo velocidad: cuando vale cero el Python
   * *descarta la entrada*, asi que se replica tal cual. Ver la trampa en el
   * comentario de `find_url_suffix()`.
   */
  int literal_prefix_len = 0;
};

struct Table {
  bool loaded = false;
  /** Plantilla del prefijo, con `{lang}` y `{version}` por sustituir. */
  std::string prefix_template;
  std::vector<Entry> entries;
  /** Ruta de la que se cargo, para los mensajes de error. */
  std::string source_path;
};

/**
 * Devuelve la tabla, cargandola de `datafiles/manual/` la primera vez.
 * Si no se encuentra el fichero devuelve una tabla vacia con `loaded = false`
 * (y avisa una sola vez por el log): sin tabla no hay manual, pero el editor
 * sigue funcionando.
 */
const Table &table_ensure();

/** Carga una tabla de un fichero concreto. Para las herramientas de verificacion. */
bool table_read(const char *filepath, Table &r_table);

/**
 * `fnmatchcase()` de Python, sin traducir a expresion regular.
 * Soporta `*`, `?` y `[...]` / `[!...]`; la barra invertida NO escapa (igual
 * que `fnmatch.translate`, que hace `re.escape` sobre ella).
 */
bool glob_match(const char *pattern, const char *str);

/**
 * El `WM_OT_doc_view_manual._find_reference` de Python: recorre la tabla en
 * orden y devuelve el sufijo de la primera entrada cuyo patron case.
 * `rna_id` se pasa a minusculas aqui dentro, como hacia el Python.
 */
std::optional<std::string> find_url_suffix(const std::string &rna_id);

/** Equivalente de `bpy.utils.manual_language_code()`. */
std::string language_code(bContext *C);

/** Equivalente de `rna_manual_reference.url_manual_prefix`. */
std::string url_prefix(bContext *C);

/** Prefijo + sufijo, o nada si la ruta RNA no esta en la tabla. */
std::optional<std::string> url_from_rna_id(bContext *C, const std::string &rna_id);

/**
 * Registra la tabla de datos como proveedor de URLs del manual
 * (`WM_manual.hpp`). Se llama una vez, desde `WM_init()`.
 *
 * Hace falta porque el proveedor integrado de `wm_system_ops.cc` solo tiene
 * cinco entradas y manda el resto al buscador del manual: sin esto, «Ver manual
 * en linea» deja de llevar al parrafo concreto para practicamente toda ruta RNA,
 * que es justo la capacidad que tenian las 4.253 entradas del Python.
 */
void provider_register_builtin_table();

/* -------------------------------------------------------------------------- */
/** \name Herramientas de conversion y verificacion
 * \{ */

/**
 * Convierte `scripts/modules/rna_manual_reference.py` en el fichero de datos.
 * Es la herramienta que genero `release/datafiles/manual/rna_manual_reference.txt`;
 * se conserva para poder regenerarlo si alguna vez se reimporta la tabla de
 * upstream. `--fl-convert-manual-reference <entrada.py> <salida.txt>`.
 */
bool convert_py_table(const char *py_path, const char *out_path);

/**
 * Lee una lista de rutas RNA (una por linea) y vuelca `ruta<TAB>sufijo`, con el
 * prefijo en una sola linea `PREFIX`. Se vuelca el sufijo y no la URL entera a
 * proposito: el prefijo lleva la version de Blender y el idioma, y repetirlos en
 * cada fila haria roja la linea base al subir de version sin ninguna regresion.
 * `--fl-dump-manual <lista> <salida>`.
 */
bool dump_urls(bContext *C, const char *ids_path, const char *out_path);

/**
 * Compara contra una linea base congelada con el Python vivo.
 * `--fl-check-manual <linea-base>`.
 */
bool check_urls(bContext *C, const char *baseline_path);

/** \} */

}  // namespace flipendo::manual
