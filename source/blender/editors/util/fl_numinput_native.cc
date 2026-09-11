/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edutil
 *
 * Evaluacion nativa de campos numericos y su autocomprobacion.
 * Ver `FL_numinput_native.hh` para el porque y para la tabla de lo cubierto.
 */

#include <cmath>
#include <cstdio>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "BLI_expr_pylike_eval.h"
#include "BLI_fileops.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_report.hh"
#include "BKE_unit.hh"

#include "DNA_scene_types.h"

#include "ED_numinput.hh"

#include "FL_numinput_native.hh"

namespace flipendo::numinput {

/* -------------------------------------------------------------------- */
/** \name Evaluador
 * \{ */

/* Espejo de `run_string_handle_error()` de `bpy_interface_run.cc`: el error va al
 * informe si lo hay, se imprime por stderr si nadie lo ha impreso, y se copia a
 * `r_string` si lo piden. Manteniendo esto igual, el comportamiento observable del
 * build sin Python (mensaje en el editor de Info incluido) es el mismo. */
static void handle_error(EvalErrInfo *err_info, const char *err_str)
{
  if (err_info == nullptr) {
    fprintf(stderr, "%s\n", err_str);
    return;
  }
  if (!(err_info->reports || err_info->r_string)) {
    return;
  }

  if (err_info->reports != nullptr) {
    if (err_info->report_prefix) {
      BKE_reportf(err_info->reports, RPT_ERROR, "%s: %s", err_info->report_prefix, err_str);
    }
    else {
      BKE_report(err_info->reports, RPT_ERROR, err_str);
    }
  }

  if ((err_info->reports == nullptr) || !BKE_reports_print_test(err_info->reports, RPT_ERROR)) {
    if (err_info->report_prefix) {
      fprintf(stderr, "%s: ", err_info->report_prefix);
    }
    fprintf(stderr, "%s\n", err_str);
  }

  if (err_info->r_string != nullptr) {
    *err_info->r_string = BLI_strdup(err_str);
  }
}

bool eval_expression(const char *str, EvalErrInfo *err_info, double *r_value)
{
  /* `BPY_run_string_as_number()` trata la cadena vacia como 0.0 sin error. */
  if (str == nullptr || str[0] == '\0') {
    *r_value = 0.0;
    return true;
  }

  /* CPython rechaza una expresion que empieza por espacio o tabulador
   * (`IndentationError: unexpected indent`), y `BLI_expr_pylike` se lo come sin
   * rechistar. Medido: con Python, teclear `  42  ` en un campo NO lo acepta. Se
   * replica el rechazo para no cambiar lo que ve el usuario. El espacio final si
   * vale en los dos. */
  if (ELEM(str[0], ' ', '\t')) {
    handle_error(err_info, "IndentationError: unexpected indent");
    return false;
  }

  ExprPyLike_Parsed *expr = BLI_expr_pylike_parse(str, nullptr, 0);

  if (!BLI_expr_pylike_is_valid(expr)) {
    BLI_expr_pylike_free(expr);
    /* Ruidoso a proposito: es exactamente lo que el `atof()` de antes callaba. */
    handle_error(err_info,
                 (err_info && err_info->use_single_line_error) ?
                     "SyntaxError: expresion no valida sin Python" :
                     "SyntaxError: expresion no valida en un build sin Python.\n"
                     "Se admite aritmetica, parentesis, unidades, pi/True/False y las "
                     "funciones min max radians degrees abs fabs floor ceil trunc int sin "
                     "cos tan asin acos atan atan2 exp log sqrt pow fmod.\n"
                     "No se admiten ** ni % ni // (usa pow() y fmod()), ni 0x..., ni listas "
                     "separadas por comas.");
    return false;
  }

  double result = 0.0;
  const eExprPyLike_EvalStatus status = BLI_expr_pylike_eval(expr, nullptr, 0, &result);
  BLI_expr_pylike_free(expr);

  switch (status) {
    case EXPR_PYLIKE_SUCCESS:
      break;
    case EXPR_PYLIKE_DIV_BY_ZERO:
      handle_error(err_info, "ZeroDivisionError: division by zero");
      return false;
    case EXPR_PYLIKE_MATH_ERROR:
      handle_error(err_info, "ValueError: math domain error");
      return false;
    default:
      handle_error(err_info, "RuntimeError: expression evaluation failed");
      return false;
  }

  /* `PyC_RunString_AsNumber()` convierte los no finitos en 0.0 y da exito. */
  *r_value = std::isfinite(result) ? result : 0.0;
  return true;
}

/** \} */

}  // namespace flipendo::numinput

/* -------------------------------------------------------------------- */
/** \name Autocomprobacion `--fl-selftest-numinput`
 * \{ */

namespace flipendo::numinput::selftest {

namespace {

struct Case {
  /** Cadena tal cual la teclearia el usuario en el campo. */
  const char *input;
  /** `B_UNIT_*`. */
  int unit_type;
  /** true si los dos builds (con y sin Python) tienen que dar lo mismo. */
  bool covered;
  /** Motivo, solo para los no cubiertos. */
  const char *note;
};

#define CASE_OK(str, type) {str, type, true, nullptr}
#define CASE_NO(str, type, why) {str, type, false, why}

const Case cases[] = {
    /* --- Aritmetica: el caso que motivo todo esto es el primero. --- */
    CASE_OK("2*3", B_UNIT_NONE),
    CASE_OK("1+1", B_UNIT_NONE),
    CASE_OK("10-4", B_UNIT_NONE),
    CASE_OK("7/2", B_UNIT_NONE),
    CASE_OK("1/3", B_UNIT_NONE),
    CASE_OK("2*3+1", B_UNIT_NONE),
    CASE_OK("1+2*3", B_UNIT_NONE),
    CASE_OK("-5", B_UNIT_NONE),
    CASE_OK("- 5", B_UNIT_NONE),
    CASE_OK("+5", B_UNIT_NONE),
    CASE_OK("--5", B_UNIT_NONE),
    CASE_OK("1.5*2", B_UNIT_NONE),
    CASE_OK(".5*4", B_UNIT_NONE),
    CASE_OK("1e3", B_UNIT_NONE),
    CASE_OK("1E-3", B_UNIT_NONE),
    CASE_OK("2.5e+2", B_UNIT_NONE),
    CASE_OK("  42  ", B_UNIT_NONE), /* CPython no admite sangria: error en los dos. */
    CASE_OK("42  ", B_UNIT_NONE),   /* El espacio final si vale en los dos. */
    CASE_OK("   ", B_UNIT_NONE),
    CASE_OK("", B_UNIT_NONE),
    CASE_OK("1e300*1e300", B_UNIT_NONE), /* Infinito -> 0.0 y exito, en los dos. */

    /* --- Parentesis y precedencia --- */
    CASE_OK("(1+2)*(3+4)", B_UNIT_NONE),
    CASE_OK("((2))", B_UNIT_NONE),
    CASE_OK("2*(3*(4+1))", B_UNIT_NONE),
    CASE_OK("-(2+3)", B_UNIT_NONE),
    CASE_OK("2*(-3)", B_UNIT_NONE),

    /* --- Constantes --- */
    CASE_OK("pi", B_UNIT_NONE),
    CASE_OK("2*pi", B_UNIT_NONE),
    CASE_OK("pi/2", B_UNIT_NONE),
    CASE_OK("True", B_UNIT_NONE),
    CASE_OK("False", B_UNIT_NONE),

    /* --- Funciones matematicas --- */
    CASE_OK("sqrt(2)", B_UNIT_NONE),
    CASE_OK("abs(-3)", B_UNIT_NONE),
    CASE_OK("fabs(-3.5)", B_UNIT_NONE),
    CASE_OK("floor(2.7)", B_UNIT_NONE),
    CASE_OK("ceil(2.1)", B_UNIT_NONE),
    CASE_OK("trunc(-2.9)", B_UNIT_NONE),
    CASE_OK("int(2.9)", B_UNIT_NONE),
    CASE_OK("int(-2.9)", B_UNIT_NONE),
    CASE_OK("round(2.4)", B_UNIT_NONE),
    CASE_OK("min(3,5)", B_UNIT_NONE),
    CASE_OK("max(3,5)", B_UNIT_NONE),
    CASE_OK("min(3,5,1)", B_UNIT_NONE),
    CASE_OK("max(3,5,9,2)", B_UNIT_NONE),
    CASE_OK("pow(2,10)", B_UNIT_NONE),
    CASE_OK("fmod(7,3)", B_UNIT_NONE),
    CASE_OK("radians(90)", B_UNIT_NONE),
    CASE_OK("degrees(pi)", B_UNIT_NONE),
    CASE_OK("sin(0)", B_UNIT_NONE),
    CASE_OK("cos(0)", B_UNIT_NONE),
    CASE_OK("tan(0)", B_UNIT_NONE),
    CASE_OK("asin(1)", B_UNIT_NONE),
    CASE_OK("acos(0)", B_UNIT_NONE),
    CASE_OK("atan(1)", B_UNIT_NONE),
    CASE_OK("atan2(1,1)", B_UNIT_NONE),
    CASE_OK("exp(1)", B_UNIT_NONE),
    CASE_OK("log(10)", B_UNIT_NONE),
    CASE_OK("log(8,2)", B_UNIT_NONE),
    CASE_OK("sqrt(abs(-16))", B_UNIT_NONE),
    CASE_OK("max(1,min(5,3))", B_UNIT_NONE),

    /* --- Logica y ternario (los admite el evaluador y Python) --- */
    CASE_OK("1 if 2>1 else 3", B_UNIT_NONE),
    CASE_OK("1 if 2<1 else 3", B_UNIT_NONE),
    CASE_OK("1<2<3", B_UNIT_NONE),
    CASE_OK("2>3", B_UNIT_NONE),
    CASE_OK("not 5", B_UNIT_NONE),
    CASE_OK("not 0", B_UNIT_NONE),
    CASE_OK("0 or 3", B_UNIT_NONE),
    CASE_OK("2 and 7", B_UNIT_NONE),

    /* --- Errores: los dos builds tienen que RECHAZAR, no inventarse un numero --- */
    CASE_OK("1/0", B_UNIT_NONE),
    CASE_OK("sqrt(-1)", B_UNIT_NONE),
    CASE_OK("log(0)", B_UNIT_NONE),
    CASE_OK("2+", B_UNIT_NONE),
    CASE_OK("(", B_UNIT_NONE),
    CASE_OK("foo", B_UNIT_NONE),
    CASE_OK("2 3", B_UNIT_NONE),

    /* --- Unidades simples (longitud, sistema metrico) --- */
    CASE_OK("5cm", B_UNIT_LENGTH),
    CASE_OK("1m", B_UNIT_LENGTH),
    CASE_OK("2km", B_UNIT_LENGTH),
    CASE_OK("0.5m", B_UNIT_LENGTH),
    CASE_OK("3mm", B_UNIT_LENGTH),
    CASE_OK("250um", B_UNIT_LENGTH),
    CASE_OK("2ft", B_UNIT_LENGTH),
    CASE_OK("3in", B_UNIT_LENGTH),
    CASE_OK("1mi", B_UNIT_LENGTH),
    CASE_OK("5yd", B_UNIT_LENGTH),
    CASE_OK("7", B_UNIT_LENGTH), /* Sin unidad: manda la unidad preferida de la escena. */
    CASE_OK("2*3", B_UNIT_LENGTH),

    /* --- Unidades compuestas y mezcladas con aritmetica --- */
    CASE_OK("1m20cm", B_UNIT_LENGTH),
    CASE_OK("1m 20cm", B_UNIT_LENGTH),
    CASE_OK("-1m50cm", B_UNIT_LENGTH),
    CASE_OK("5'2\"", B_UNIT_LENGTH),
    CASE_OK("2*3cm", B_UNIT_LENGTH),
    CASE_OK("2+2in", B_UNIT_LENGTH),
    CASE_OK("(1+1)m", B_UNIT_LENGTH),
    CASE_OK("3km+50m", B_UNIT_LENGTH),

    /* --- Otros tipos de unidad --- */
    CASE_OK("90", B_UNIT_ROTATION),
    CASE_OK("90d", B_UNIT_ROTATION),
    CASE_OK("45+45", B_UNIT_ROTATION),
    CASE_OK("2*45", B_UNIT_ROTATION),
    CASE_OK("1s", B_UNIT_TIME),
    CASE_OK("2min", B_UNIT_TIME),
    CASE_OK("1h", B_UNIT_TIME),
    CASE_OK("1h30min", B_UNIT_TIME),
    CASE_OK("5kg", B_UNIT_MASS),
    CASE_OK("500g", B_UNIT_MASS),
    CASE_OK("2m2", B_UNIT_AREA),
    CASE_OK("3m3", B_UNIT_VOLUME),
    CASE_OK("50mm", B_UNIT_CAMERA),

    /* --- Potencia, modulo y division entera (BLI_expr_pylike los aprendio el
     *     2026-09-11; la precedencia rara de `**` esta medida aqui) --- */
    CASE_OK("2**3", B_UNIT_NONE),
    CASE_OK("2**10", B_UNIT_NONE),
    CASE_OK("2**0.5", B_UNIT_NONE),
    CASE_OK("2**-1", B_UNIT_NONE),   /* El unario liga MAS que ** por la derecha. */
    CASE_OK("-2**2", B_UNIT_NONE),   /* ...y MENOS por la izquierda: -4, no 4. */
    CASE_OK("2**3**2", B_UNIT_NONE), /* Asociativo por la derecha: 512, no 64. */
    CASE_OK("(2**3)**2", B_UNIT_NONE),
    CASE_OK("2*3**2", B_UNIT_NONE),  /* ** liga mas que *: 18, no 36. */
    CASE_OK("2**3+1", B_UNIT_NONE),
    CASE_OK("0**-1", B_UNIT_NONE),   /* Error en los dos. */
    CASE_OK("(-8)**(1/3)", B_UNIT_NONE), /* Error en los dos: complejo / NaN. */
    CASE_OK("7%3", B_UNIT_NONE),
    CASE_OK("-7%3", B_UNIT_NONE), /* Modulo con suelo: 2, no -1. */
    CASE_OK("7%-3", B_UNIT_NONE), /* -2, el signo es el del divisor. */
    CASE_OK("-7%-3", B_UNIT_NONE),
    CASE_OK("7.5%2", B_UNIT_NONE),
    CASE_OK("7%0", B_UNIT_NONE), /* Error en los dos. */
    CASE_OK("2+7%3", B_UNIT_NONE), /* % liga mas que +: 3, no 0. */
    CASE_OK("7//2", B_UNIT_NONE),
    CASE_OK("-7//2", B_UNIT_NONE), /* Division con suelo: -4, no -3. */
    CASE_OK("7//-2", B_UNIT_NONE),
    CASE_OK("-7//-2", B_UNIT_NONE),
    CASE_OK("7.5//2", B_UNIT_NONE),
    CASE_OK("7//0", B_UNIT_NONE), /* Error en los dos. */
    CASE_OK("1+2*3**2%5//2", B_UNIT_NONE), /* Todas juntas, por la precedencia. */

    /* --- Declarados NO cubiertos: aqui los dos builds difieren a proposito --- */
    CASE_NO("0x10", B_UNIT_NONE, "literales hexadecimales: solo Python"),
    CASE_NO("1_000", B_UNIT_NONE, "separador de miles del literal: solo Python"),
    CASE_NO("round(2.5)", B_UNIT_NONE, "round() de C redondea 2.5 a 3; Python redondea a 2 (bancario)"),
    CASE_NO("lerp(0,10,0.25)", B_UNIT_NONE, "lerp existe en BLI_expr_pylike y no en math de Python"),
    CASE_NO("clamp(5,0,1)", B_UNIT_NONE, "clamp existe en BLI_expr_pylike y no en math de Python"),
    CASE_NO("smoothstep(0,1,0.5)",
       B_UNIT_NONE,
       "smoothstep existe en BLI_expr_pylike y no en math de Python"),
    CASE_NO("2,5", B_UNIT_NONE, "Python lo lee como tupla y suma 2+5=7; aqui es error de sintaxis"),
    CASE_NO("10km, 2m", B_UNIT_LENGTH, "lista separada por comas: Python la suma, aqui es error"),
    CASE_NO("3**500",
            B_UNIT_NONE,
            "entero**entero grande: Python lo calcula exacto con enteros de precision "
            "arbitraria y luego redondea; aqui es pow() en doble desde el principio"),
};

#undef CASE_OK
#undef CASE_NO

const char *unit_type_name(int type)
{
  switch (type) {
    case B_UNIT_NONE:
      return "NONE";
    case B_UNIT_LENGTH:
      return "LENGTH";
    case B_UNIT_AREA:
      return "AREA";
    case B_UNIT_VOLUME:
      return "VOLUME";
    case B_UNIT_MASS:
      return "MASS";
    case B_UNIT_ROTATION:
      return "ROTATION";
    case B_UNIT_TIME:
      return "TIME";
    case B_UNIT_TIME_ABSOLUTE:
      return "TIME_ABSOLUTE";
    case B_UNIT_VELOCITY:
      return "VELOCITY";
    case B_UNIT_ACCELERATION:
      return "ACCELERATION";
    case B_UNIT_CAMERA:
      return "CAMERA";
    case B_UNIT_POWER:
      return "POWER";
    case B_UNIT_TEMPERATURE:
      return "TEMPERATURE";
    case B_UNIT_WAVELENGTH:
      return "WAVELENGTH";
    case B_UNIT_COLOR_TEMPERATURE:
      return "COLOR_TEMPERATURE";
    case B_UNIT_FREQUENCY:
      return "FREQUENCY";
    default:
      return "?";
  }
}

/* Ajustes de unidades fijos, no los de la escena: la comprobacion tiene que dar lo
 * mismo aunque cambie el fichero de arranque. Metrico, escala 1, sin division en
 * varias unidades, rotacion en grados. */
UnitSettings fixed_unit_settings()
{
  /* Mismos valores que pone `BKE_scene_init_data()` (`scene.cc:193-199`) en una
   * escena nueva, pero escritos aqui a mano para no depender del fichero de
   * arranque. Ojo: `length_unit` y companeros son `char` con signo, asi que no
   * valen las constantes tipo `USER_UNIT_ADAPTIVE` (0xFF). */
  UnitSettings unit = {};
  unit.scale_length = 1.0f;
  unit.system = USER_UNIT_METRIC;
  unit.system_rotation = 0;
  unit.flag = 0;
  unit.length_unit = uchar(BKE_unit_base_of_type_get(USER_UNIT_METRIC, B_UNIT_LENGTH));
  unit.mass_unit = uchar(BKE_unit_base_of_type_get(USER_UNIT_METRIC, B_UNIT_MASS));
  unit.time_unit = uchar(BKE_unit_base_of_type_get(USER_UNIT_METRIC, B_UNIT_TIME));
  unit.temperature_unit = uchar(BKE_unit_base_of_type_get(USER_UNIT_METRIC, B_UNIT_TEMPERATURE));
  return unit;
}

}  // namespace

bool dump(bContext *C, const char *filepath)
{
  FILE *f = BLI_fopen(filepath, "w");
  if (f == nullptr) {
    fprintf(stderr, "fl-selftest-numinput: no se pudo escribir '%s'\n", filepath);
    return false;
  }

  const UnitSettings unit = fixed_unit_settings();

  fprintf(f, "# volcado de campos numericos de Flipendo\n");
  fprintf(f,
          "# Cada linea: <tipo de unidad> | <cadena tecleada> | ok=<0|1> | <valor con "
          "%%.17g>\n");
  fprintf(f, "# Unidades fijas: metrico, scale_length=1, sin split, rotacion en grados.\n");
  fprintf(f,
          "# El binario CON Python y el binario SIN Python tienen que escribir esto igual.\n");

  int total_covered = 0, total_uncovered = 0;
  for (const Case &c : cases) {
    (c.covered ? total_covered : total_uncovered)++;
  }

  fprintf(f, "\n[cubiertos] %d\n", total_covered);
  for (const Case &c : cases) {
    if (!c.covered) {
      continue;
    }
    double value = 0.0;
    char *error = nullptr;
    const bool ok = user_string_to_number(C, c.input, unit, c.unit_type, &value, true, &error);
    if (error) {
      MEM_freeN(error);
    }
    fprintf(f, "%-14s | %-22s | ok=%d | %.17g\n", unit_type_name(c.unit_type), c.input, int(ok), value);
  }

  /* Los no cubiertos van al fichero SIN su valor —ahi los dos builds difieren a
   * proposito— y con su valor por salida estandar, para poder documentar en que
   * consiste exactamente la diferencia. */
  fprintf(f, "\n[no-cubiertos] %d\n", total_uncovered);
  printf("fl-selftest-numinput: casos NO cubiertos, valor real de ESTE binario:\n");
  for (const Case &c : cases) {
    if (c.covered) {
      continue;
    }
    fprintf(f, "%-14s | %-22s | %s\n", unit_type_name(c.unit_type), c.input, c.note);

    double value = 0.0;
    char *error = nullptr;
    const bool ok = user_string_to_number(C, c.input, unit, c.unit_type, &value, true, &error);
    if (error) {
      MEM_freeN(error);
    }
    printf("  %-14s %-22s ok=%d %.17g\n", unit_type_name(c.unit_type), c.input, int(ok), value);
  }

  fclose(f);

  printf("fl-selftest-numinput: %d casos cubiertos + %d no cubiertos escritos en '%s'\n",
         total_covered,
         total_uncovered,
         filepath);
  return true;
}

}  // namespace flipendo::numinput::selftest

/** \} */
