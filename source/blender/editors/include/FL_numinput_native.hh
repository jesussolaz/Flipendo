/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 *
 * Evaluacion NATIVA de los campos numericos de la interfaz, sin interprete Python.
 *
 * Por que existe
 * --------------
 * Los campos numericos de Blender no son campos numericos: son campos de expresion.
 * Escribir `2*3` da 6 y escribir `5cm` da 0.05 m porque el texto se pasa por
 * `BPY_run_string_as_number()`. Al compilar con `WITH_PYTHON=OFF` esas dos llamadas
 * —`interface.cc` y `numinput.cc`— caian a una rama `#else` que hacia `atof(str)`:
 * `2*3` daba **2** y `5cm` daba **5**. Sin error, sin aviso y con un numero que
 * parece bueno. Era la peor clase de perdida: silenciosa.
 *
 * El arbol ya trae con que arreglarlo sin interprete:
 * - `BLI_expr_pylike_eval` evalua en C++ el subconjunto de expresiones Python que
 *   se puede calcular en doble precision (es el mismo evaluador que ya usan los
 *   drivers simples en `fcurve_driver.cc`).
 * - `BKE_unit_replace_string` traduce `5cm` a `(5*0.01)*1` antes de evaluar.
 *
 * Este modulo es el pegamento entre los dos, con la misma semantica observable que
 * `PyC_RunString_AsNumber()`: cadena vacia -> 0.0 y exito; resultado no finito ->
 * 0.0 y exito; error -> false sin tocar `r_value`, con el mensaje en `r_string` y
 * en `reports` si los hay.
 *
 * Que se cubre y que no
 * ---------------------
 * Cubierto (verificado con `--fl-selftest-numinput` contra el binario con Python):
 * aritmetica `+ - * /`, parentesis, unario `+ -`, literales decimales y notacion
 * cientifica, constantes `pi` `True` `False`, comparaciones y `and` `or` `not` y el
 * ternario `a if c else b`, y las funciones `min max radians degrees abs fabs floor
 * ceil trunc int sin cos tan asin acos atan atan2 exp log sqrt pow fmod` (incluido
 * `log(a,b)` de dos argumentos). Unidades simples y compuestas de cualquier tipo,
 * porque la conversion la hace `BKE_unit_replace_string` antes de evaluar.
 *
 * NO cubierto, y por eso **falla ruidosamente** en vez de dar un numero malo:
 * - `**` (potencia), `%` (modulo) y `//` (division entera): `BLI_expr_pylike` no los
 *   tokeniza. Se escriben `pow(a,b)` y `fmod(a,b)`. Ampliar el evaluador tocaria
 *   tambien el camino rapido de los drivers en el build CON Python, asi que queda
 *   fuera de este cambio.
 * - Literales hexadecimales/octales/binarios (`0x10`) y separadores `1_000`.
 * - Listas separadas por coma (`10km, 2m`), que Python suma por ser una tupla.
 * - `round()` de un `.5` exacto: aqui es `round()` de C (medio hacia afuera, 2.5->3)
 *   y en Python es redondeo bancario (2.5->2). El resto de valores coincide.
 * - `lerp`, `clamp` y `smoothstep`: existen en `BLI_expr_pylike` pero NO en el
 *   `math` de Python, asi que aqui salen y con Python no. Divergencia al reves.
 */

#pragma once

struct ReportList;

namespace flipendo::numinput {

/**
 * Espejo de `BPy_RunErrInfo`: donde dejar el error si la expresion no evalua.
 * Todos los campos son opcionales.
 */
struct EvalErrInfo {
  /** Si no es nullptr, el error se anade al informe (editor de Info). */
  ReportList *reports = nullptr;
  /** Prefijo del mensaje del informe. */
  const char *report_prefix = nullptr;
  /** Mensaje corto de una linea en vez del detallado. */
  bool use_single_line_error = false;
  /** Si no es nullptr, recibe una copia del mensaje (liberar con `MEM_freeN`). */
  char **r_string = nullptr;
};

/**
 * Evalua `str` como numero sin interprete de Python.
 *
 * Devuelve true si el valor de `r_value` es utilizable. En caso de error devuelve
 * false y **no toca** `r_value` (igual que la version de Python, de la que depende
 * `user_string_to_number` para no reescribir el valor previo del campo).
 */
bool eval_expression(const char *str, EvalErrInfo *err_info, double *r_value);

}  // namespace flipendo::numinput

struct bContext;

namespace flipendo::numinput::selftest {

/**
 * Implementacion de `--fl-selftest-numinput <fichero>`.
 *
 * Evalua una bateria de cadenas de entrada —la tabla vive en C++, no en un fichero
 * de datos— pasando por el punto de entrada de verdad (`user_string_to_number`), y
 * vuelca el resultado numerico con 17 cifras significativas.
 *
 * El mismo binario con Python y sin Python tiene que escribir el MISMO fichero.
 * La linea base congelada esta en `tests/flipendo/numinput/baseline-python.txt`.
 *
 * Los casos declarados NO cubiertos se listan en el fichero por su cadena de
 * entrada pero sin resultado (ahi los dos builds difieren a proposito); su valor
 * real en este binario se imprime por salida estandar, para poder documentarlo.
 */
bool dump(bContext *C, const char *filepath);

}  // namespace flipendo::numinput::selftest
