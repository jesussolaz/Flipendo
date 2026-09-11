/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/** \file
 * \ingroup creator
 *
 * Guardas del arnes `--fl-check-*` / `--fl-selftest-*` / `--fl-dump-*`.
 *
 * POR QUE EXISTE ESTE FICHERO
 * ---------------------------
 * El 2026-09-11 se midio que los dieciseis `--fl-selftest-*` imprimian
 * «Error: falta el fichero de salida» y **salian con codigo 0**. Dieciseis pruebas
 * que no podian fallar. Lo mismo hacian los `--fl-check-*` sin linea base, entre
 * ellos `--fl-check-tools`. Un arnes que no falla nunca no es un arnes: es un sello
 * de goma, y envenena todo lo que se haya dado por verificado con el.
 *
 * La regla que imponen estas guardas, y que no admite excepciones:
 *
 *   **Si no pudo comprobar, es fallo, y lo dice con el motivo.**
 *
 * Las cuatro formas de «no pudo comprobar» que se cierran aqui:
 *
 *  1. `arg_missing()`     — falta el argumento obligatorio (salida o linea base).
 *  2. `baseline_required()` — la linea base no existe, no se puede leer o esta vacia.
 *  3. `dump_required()`   — el volcado no se escribio o quedo vacio.
 *  4. `gui_required()`    — el comprobador necesita un editor real y se pidio en
 *                           `--background`, donde no escribe nada.
 *
 * Todas salen con `WM_exit(C, EXIT_FAILURE)` —nunca `exit()`, para que se limpien los
 * temporales, como pide la nota de `ARG_PASS_FINAL` en `creator_args.cc`— y todas
 * imprimen por `stderr` una linea que empieza por `ARNES:` con el motivo exacto, para
 * que un rojo se pueda leer sin abrir el codigo.
 */

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#include "BLI_fileops.h"

#include "BKE_global.hh"

#include "WM_api.hh"

struct bContext;

namespace flipendo::harness {

/** Cabecera comun de todo mensaje del arnes, para poder buscarlos con un grep. */
inline void say(const char *flag, const char *fmt, ...)
{
  fprintf(stderr, "\nARNES: %s: ", flag);
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  fprintf(stderr, "\n");
}

/**
 * Falta el argumento obligatorio del comprobador.
 *
 * Antes esto imprimia el error y devolvia 0, con lo que Blender seguia arrancando y
 * acababa saliendo con exito: la prueba «pasaba» sin haber ejecutado nada.
 */
inline int arg_missing(bContext *C, const char *flag, const char *que_falta)
{
  say(flag, "falta %s.", que_falta);
  say(flag, "no se ha comprobado NADA, asi que esto es un fallo: se sale con codigo 1.");
  WM_exit(C, EXIT_FAILURE);
  return 1;
}

/** Devuelve el tamano del fichero, o -1 si no existe o no se puede leer. */
inline int64_t readable_size(const char *path)
{
  if (path == nullptr || path[0] == '\0') {
    return -1;
  }
  FILE *fp = fopen(path, "rb");
  if (fp == nullptr) {
    return -1;
  }
  fclose(fp);
  const size_t size = BLI_file_size(path);
  /* `BLI_file_size` devuelve `size_t`; un fallo de `stat` vuelve como (size_t)-1. */
  if (size == size_t(-1)) {
    return -1;
  }
  return int64_t(size);
}

/**
 * La linea base tiene que existir, poder leerse y no estar vacia.
 *
 * Comparar contra un fichero vacio da cero diferencias y por tanto verde: es el
 * verde mas peligroso de todos, porque parece que se comprobo algo.
 */
inline void baseline_required(bContext *C, const char *flag, const char *path)
{
  const int64_t size = readable_size(path);
  if (size < 0) {
    say(flag, "no existe o no se puede leer la linea base '%s'.", path);
    say(flag, "sin linea base no hay con que comparar: se sale con codigo 1.");
    WM_exit(C, EXIT_FAILURE);
  }
  if (size == 0) {
    say(flag, "la linea base '%s' esta vacia (0 bytes).", path);
    say(flag, "comparar contra un fichero vacio da cero diferencias y un verde falso: codigo 1.");
    WM_exit(C, EXIT_FAILURE);
  }
}

/**
 * El directorio de entrada tiene que existir y ser un directorio.
 *
 * Un comprobador que recorre un arbol inexistente encuentra cero ficheros, cuenta cero
 * diferencias y sale con exito: otro verde sin nada detras.
 */
inline void dir_required(bContext *C, const char *flag, const char *path)
{
  if (path == nullptr || path[0] == '\0' || !BLI_is_dir(path)) {
    say(flag, "el directorio '%s' no existe o no es un directorio.", path ? path : "(vacio)");
    say(flag, "recorrer un arbol que no esta da cero diferencias y un verde falso: codigo 1.");
    WM_exit(C, EXIT_FAILURE);
  }
}

/**
 * El volcado tiene que haberse escrito y no estar vacio.
 *
 * `ok` es lo que devolvio el volcador. Se combinan las dos cosas porque un volcador
 * puede devolver true y no haber escrito ni un byte (paso con los dos selftests que
 * necesitan editor: programaban el trabajo y devolvian true en el acto).
 */
inline bool dump_written(const char *flag, const char *path, const bool ok)
{
  if (!ok) {
    say(flag, "el volcador fallo; no se da por bueno nada.");
    return false;
  }
  const int64_t size = readable_size(path);
  if (size < 0) {
    say(flag, "el volcado '%s' no llego a escribirse.", path);
    return false;
  }
  if (size == 0) {
    say(flag, "el volcado '%s' quedo vacio (0 bytes): no se ha observado nada.", path);
    return false;
  }
  return true;
}

/**
 * Hay comprobadores que en `--background` no escriben nada porque necesitan un
 * editor real (una ventana, un area, una region). Antes se quedaban callados y
 * salian con 0: doblemente falso, porque ademas el fichero quedaba vacio.
 */
inline void gui_required(bContext *C, const char *flag)
{
  if (G.background) {
    say(flag, "necesita un editor real y se ha pedido en --background.");
    say(flag,
        "en --background no hay ventana ni area, el volcado quedaria vacio y el verde "
        "seria falso. Ejecutalo en modo grafico (sin --background): codigo 1.");
    WM_exit(C, EXIT_FAILURE);
  }
}

}  // namespace flipendo::harness
