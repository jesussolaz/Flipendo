/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Presets como DATOS, no como codigo.
 *
 * Un preset de Blender era un script de Python que el editor ejecutaba
 * (`bpy.utils.execfile`), y su inmensa mayoria no hacia mas que asignar
 * propiedades RNA. Eso es dato disfrazado de codigo: aqui se lee como dato.
 *
 * Ver politicas/PRESETS-A-DATOS.md para el formato y la deuda declarada.
 */

#pragma once

#include <string>
#include <vector>

struct bContext;

namespace flipendo::preset {

/** Version del formato `.fpreset` que escribe y entiende esta build. */
constexpr int FORMAT_VERSION = 1;

enum class ValueKind {
  None,   /* `none` -> puntero nulo. */
  Bool,   /* `true` / `false`. */
  Int,    /* Entero con signo. */
  Float,  /* Coma flotante. */
  String, /* `"texto"`; tambien el identificador de un enum simple. */
  Array,  /* `[a, b, c]`. */
  EnumSet, /* `{"A", "B"}` -> enum de banderas. */
};

struct Value {
  ValueKind kind = ValueKind::None;
  bool b = false;
  long long i = 0;
  double f = 0.0;
  /* Verdadero si el valor vino de una propiedad RNA de precision simple. Solo
   * afecta a como se IMPRIME: se busca la representacion mas corta que vuelve a
   * dar el mismo `float`, no el mismo `double`. */
  bool f_single = false;
  std::string s;
  std::vector<Value> items;

  static Value make_bool(bool v);
  static Value make_int(long long v);
  static Value make_float(double v, bool single);
  static Value make_string(std::string v);
  static Value make_none();
};

enum class OpKind {
  Set,           /* `set <ruta> <valor>` */
  Clear,         /* `clear <ruta>` sobre una coleccion de IDProperty. */
  CollectionAdd, /* `add <ruta>` abre un elemento nuevo. */
  CollectionEnd, /* `end` lo cierra. */
  /**
   * `when <ruta> == <valor>` / `when <ruta> != <valor>`: guardian.
   *
   * Existe por los cinco presets de FFmpeg, que decidian `gopsize` (y, en el
   * del DVD, `resolution_y`) segun `scene.render.fps != 25`. No es un
   * mini-lenguaje y no va a crecer: UNA comparacion de una ruta RNA contra un
   * literal, con `==` o `!=`, y las operaciones que protege. Sin bucles, sin
   * expresiones, sin anidamiento. Hornear una de las dos ramas habria cambiado
   * el comportamiento para la mitad de los usuarios; ver PRESETS-A-DATOS.md.
   */
  When,
  Otherwise, /* `otherwise`: la otra rama del `when`. */
  WhenEnd,   /* `endwhen`: lo cierra. */
};

/** Comparacion de un `when`. */
enum class CompareOp {
  Equal,
  NotEqual,
};

struct Op {
  OpKind kind = OpKind::Set;
  /** Solo para `When`. */
  CompareOp compare = CompareOp::Equal;
  /** Ruta RNA con raiz `context.`, o relativa (`.campo`) dentro de un `add`. */
  std::string path;
  Value value;
  int line = 0;
};

struct Preset {
  int version = FORMAT_VERSION;
  /** Familia del preset (`render`, `camera`, `cycles/integrator`...). */
  std::string subdir;
  std::vector<Op> ops;
};

/* -------------------------------------------------------------------------- */
/** \name Lectura
 * \{ */

/**
 * Lee un preset del disco. Acepta `.fpreset` (formato nativo) y `.py`
 * (compatibilidad con los presets que el usuario ya tenga guardados: se parsean
 * de forma NATIVA, sin interprete, aceptando solo el subconjunto que el propio
 * escritor de Blender generaba).
 */
bool read_file(const std::string &filepath, Preset &r_preset, std::string &r_error);

bool read_fpreset_text(const std::string &text,
                       const char *origin,
                       Preset &r_preset,
                       std::string &r_error);

/**
 * Parser nativo del subconjunto de Python que generaba `AddPresetBase`:
 * `import bpy`, alias (`scene = bpy.context.scene`) y asignaciones de literales.
 * Cualquier otra cosa (if, for, import de otro modulo, llamada) se rechaza con
 * fichero y linea: en silencio no se pierde nada.
 */
bool read_legacy_python_text(const std::string &text,
                             const char *origin,
                             Preset &r_preset,
                             std::string &r_error);

/** \} */

/* -------------------------------------------------------------------------- */
/** \name Escritura
 * \{ */

bool write_file(const std::string &filepath, const Preset &preset, std::string &r_error);

/** Serializa un valor con la gramatica del formato. Publico porque lo usa el volcado. */
std::string format_value(const Value &value);

/** \} */

/* -------------------------------------------------------------------------- */
/** \name Aplicacion
 * \{ */

struct ApplyReport {
  int applied = 0;
  std::vector<std::string> errors;
};

/**
 * Resuelve cada ruta contra el contexto y asigna el valor con la API de RNA.
 * Reproduce lo que hacia `setattr` desde Python: comprueba que la propiedad sea
 * editable y dispara `RNA_property_update` cuando la propiedad lo pide.
 */
bool apply(bContext *C, const Preset &preset, ApplyReport &r_report);

bool apply_file(bContext *C, const std::string &filepath, std::string &r_error);

/**
 * Lee el valor actual de cada ruta. Es lo que usa el escritor de presets (para
 * guardar el estado del usuario) y el volcado de verificacion.
 */
bool capture(bContext *C,
             const std::vector<std::string> &paths,
             Preset &r_preset,
             std::string &r_error);

/** \} */

/* -------------------------------------------------------------------------- */
/** \name Herramientas de linea de ordenes (verificacion y conversion)
 * \{ */

/**
 * Rutas que guarda un preset de la familia `subdir`, en el mismo orden en que las
 * listaba `AddPreset*.preset_values`. Para `operator/<idname>` se sacan de las
 * propiedades del operador activo.
 */
bool spec_paths(bContext *C,
                const std::string &subdir,
                bool use_focal_length,
                std::vector<std::string> &r_paths,
                std::string &r_error);

/** Captura el estado de esa familia y lo escribe como `.fpreset`. */
bool write_preset(bContext *C,
                  const std::string &subdir,
                  const std::string &filepath,
                  bool use_focal_length,
                  std::string &r_error);

/** `--fl-convert-presets <dir>`: convierte `.py` -> `.fpreset` en el mismo sitio. */
bool convert_tree(const char *dir);

/** `--fl-check-presets <dir>`: aplica cada preset por los dos caminos y compara. */
bool check_tree(bContext *C, const char *dir, const char *report_path);

/** \} */

}  // namespace flipendo::preset
