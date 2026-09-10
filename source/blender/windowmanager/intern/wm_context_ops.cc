/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * La familia `wm.context_*`: operadores que leen y escriben una propiedad indicada
 * por una ruta de datos sobre el contexto.
 *
 * Transliteracion de `scripts/startup/bl_operators/wm.py`. Es la familia mas
 * referenciada del editor -- 131 atajos del mapa de teclado usan su nucleo --. Los
 * 18 tipos de la familia son ya nativos y el keymap no depende de que Python los
 * registre. Ver politicas/KEYMAP-A-CPP.md.
 *
 * Que hacia el Python y como se reproduce
 * ---------------------------------------
 * El original resolvia la ruta con `eval("context." + data_path)`, o sea con el
 * interprete. Aqui se resuelve por RNA, en dos pasos porque el contexto tiene dos
 * clases de miembro:
 *
 *  - los que declara `RNA_Context` (`space_data`, `tool_settings`, `scene`,
 *    `preferences`, `area`...), que cubren la inmensa mayoria y se resuelven de una
 *    sola pasada sobre un puntero al propio contexto;
 *  - los dinamicos, que no estan en RNA y hay que pedir por nombre con
 *    `CTX_data_pointer_get` -- `weight_paint_object` y hermanos.
 *
 * Una ruta que no resuelve NO es un error: el atajo puede estar atado a un evento
 * tan comun como el clic izquierdo, y el Python devolvia PASS_THROUGH para no
 * romper la interaccion. Se conserva ese criterio.
 */

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "DNA_ID.h"
#include "DNA_brush_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_string.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"

#include "RNA_access.hh"
#include "RNA_path.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_screen.hh"

#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include <fmt/format.h>

/* -------------------------------------------------------------------- */
/** \name Resolucion de la ruta de datos
 * \{ */

namespace {

struct ResolvedPath {
  PointerRNA ptr = {};
  PropertyRNA *prop = nullptr;
  int index = -1;

  bool ok() const { return prop != nullptr; }
};

/* Devuelve la parte de `path` anterior al primer '.' o '[', que es el miembro de
 * contexto por el que empieza la ruta. */
std::string first_component(const char *path)
{
  const char *p = path;
  while (*p && *p != '.' && *p != '[') {
    p++;
  }
  return std::string(path, size_t(p - path));
}

ResolvedPath resolve(bContext *C, const char *data_path)
{
  ResolvedPath out;
  if (data_path == nullptr || data_path[0] == '\0') {
    return out;
  }

  /* Via 1: el contexto como struct RNA. Cubre space_data, tool_settings, scene,
   * preferences, area y el resto de miembros declarados en rna_context.cc. */
  PointerRNA ctx_ptr = RNA_pointer_create_discrete(nullptr, &RNA_Context, C);
  if (RNA_path_resolve_property_full(&ctx_ptr, data_path, &out.ptr, &out.prop, &out.index)) {
    return out;
  }

  /* Via 2: miembros dinamicos del contexto, que no estan en RNA_Context. */
  const std::string member = first_component(data_path);
  if (member.empty()) {
    return out;
  }
  PointerRNA member_ptr = CTX_data_pointer_get(C, member.c_str());
  if (member_ptr.data == nullptr) {
    return out;
  }

  const char *rest = data_path + member.size();
  if (*rest == '.') {
    rest++;
  }
  if (*rest == '\0') {
    /* La ruta era solo el miembro; no hay propiedad que tocar. */
    return out;
  }
  RNA_path_resolve_property_full(&member_ptr, rest, &out.ptr, &out.prop, &out.index);
  return out;
}

/* El Python decidia si la accion entra en el historial mirando de quien es el dato:
 * los cambios sobre la ventana, la pantalla o un pincel no se deshacen
 * (`operator_value_is_undo`, wm.py:222-245). Aqui se mira el mismo dueno. */
wmOperatorStatus undo_return(const ResolvedPath &r)
{
  const ID *owner = r.ptr.owner_id;
  if (owner == nullptr) {
    return OPERATOR_CANCELLED;
  }
  const short type = GS(owner->name);
  if (type == ID_WM || type == ID_SCR || type == ID_BR) {
    return OPERATOR_CANCELLED;
  }
  return OPERATOR_FINISHED;
}

/* Las tres propiedades que comparten casi todos estos operadores. */
void def_data_path(wmOperatorType *ot)
{
  PropertyRNA *prop = RNA_def_string(
      ot->srna, "data_path", nullptr, 0, "Context Attributes", "Context data-path (expanded)");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

}  // namespace

/** \} */

/* -------------------------------------------------------------------- */
/** \name wm.context_toggle
 * \{ */

static wmOperatorStatus context_toggle_exec(bContext *C, wmOperator *op)
{
  char data_path[1024];
  RNA_string_get(op->ptr, "data_path", data_path);

  const ResolvedPath r = resolve(C, data_path);
  if (!r.ok()) {
    /* Ruta invalida: se deja pasar el evento en vez de romper la interaccion. */
    return OPERATOR_PASS_THROUGH;
  }
  if (RNA_property_type(r.prop) != PROP_BOOLEAN) {
    return OPERATOR_PASS_THROUGH;
  }

  if (r.index != -1) {
    const bool value = RNA_property_boolean_get_index(
        const_cast<PointerRNA *>(&r.ptr), r.prop, r.index);
    RNA_property_boolean_set_index(const_cast<PointerRNA *>(&r.ptr), r.prop, r.index, !value);
  }
  else {
    const bool value = RNA_property_boolean_get(const_cast<PointerRNA *>(&r.ptr), r.prop);
    RNA_property_boolean_set(const_cast<PointerRNA *>(&r.ptr), r.prop, !value);
  }
  RNA_property_update(C, const_cast<PointerRNA *>(&r.ptr), r.prop);

  return undo_return(r);
}

void WM_OT_context_toggle(wmOperatorType *ot)
{
  ot->name = "Context Toggle";
  ot->idname = "WM_OT_context_toggle";
  ot->description = "Toggle a context value";

  ot->exec = context_toggle_exec;
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  def_data_path(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Asignacion directa
 * \{ */

namespace {

/* Cuerpo comun de los `context_set_*`: resolver, asignar y decidir el undo. */
template<typename AssignFn>
wmOperatorStatus context_assign(bContext *C, wmOperator *op, PropertyType expected, AssignFn assign)
{
  char data_path[1024];
  RNA_string_get(op->ptr, "data_path", data_path);

  const ResolvedPath r = resolve(C, data_path);
  if (!r.ok() || RNA_property_type(r.prop) != expected) {
    return OPERATOR_PASS_THROUGH;
  }
  assign(const_cast<PointerRNA *>(&r.ptr), r.prop, r.index);
  RNA_property_update(C, const_cast<PointerRNA *>(&r.ptr), r.prop);
  return undo_return(r);
}

}  // namespace

static wmOperatorStatus context_set_boolean_exec(bContext *C, wmOperator *op)
{
  const bool v = RNA_boolean_get(op->ptr, "value");
  return context_assign(C, op, PROP_BOOLEAN, [&](PointerRNA *p, PropertyRNA *prop, int index) {
    if (index != -1) {
      RNA_property_boolean_set_index(p, prop, index, v);
    }
    else {
      RNA_property_boolean_set(p, prop, v);
    }
  });
}

void WM_OT_context_set_boolean(wmOperatorType *ot)
{
  ot->name = "Context Set Boolean";
  ot->idname = "WM_OT_context_set_boolean";
  ot->description = "Set a context value";
  ot->exec = context_set_boolean_exec;
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  def_data_path(ot);
  RNA_def_boolean(ot->srna, "value", true, "Value", "Assignment value");
}

static wmOperatorStatus context_set_int_exec(bContext *C, wmOperator *op)
{
  const int v = RNA_int_get(op->ptr, "value");
  const bool relative = RNA_boolean_get(op->ptr, "relative");
  return context_assign(C, op, PROP_INT, [&](PointerRNA *p, PropertyRNA *prop, int index) {
    const int base = (index != -1) ? RNA_property_int_get_index(p, prop, index) :
                                     RNA_property_int_get(p, prop);
    const int result = relative ? base + v : v;
    if (index != -1) {
      RNA_property_int_set_index(p, prop, index, result);
    }
    else {
      RNA_property_int_set(p, prop, result);
    }
  });
}

void WM_OT_context_set_int(wmOperatorType *ot)
{
  ot->name = "Context Set";
  ot->idname = "WM_OT_context_set_int";
  ot->description = "Set a context value";
  ot->exec = context_set_int_exec;
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  def_data_path(ot);
  RNA_def_int(ot->srna, "value", 0, INT_MIN, INT_MAX, "Value", "Assign value", INT_MIN, INT_MAX);
  RNA_def_boolean(ot->srna, "relative", false, "Relative", "Apply relative to the current value");
}

static wmOperatorStatus context_set_float_exec(bContext *C, wmOperator *op)
{
  const float v = RNA_float_get(op->ptr, "value");
  const bool relative = RNA_boolean_get(op->ptr, "relative");
  return context_assign(C, op, PROP_FLOAT, [&](PointerRNA *p, PropertyRNA *prop, int index) {
    const float base = (index != -1) ? RNA_property_float_get_index(p, prop, index) :
                                       RNA_property_float_get(p, prop);
    const float result = relative ? base + v : v;
    if (index != -1) {
      RNA_property_float_set_index(p, prop, index, result);
    }
    else {
      RNA_property_float_set(p, prop, result);
    }
  });
}

void WM_OT_context_set_float(wmOperatorType *ot)
{
  ot->name = "Context Set Float";
  ot->idname = "WM_OT_context_set_float";
  ot->description = "Set a context value";
  ot->exec = context_set_float_exec;
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  def_data_path(ot);
  RNA_def_float(
      ot->srna, "value", 0.0f, -FLT_MAX, FLT_MAX, "Value", "Assignment value", -FLT_MAX, FLT_MAX);
  RNA_def_boolean(ot->srna, "relative", false, "Relative", "Apply relative to the current value");
}

static wmOperatorStatus context_set_string_exec(bContext *C, wmOperator *op)
{
  char value[1024];
  RNA_string_get(op->ptr, "value", value);
  return context_assign(C, op, PROP_STRING, [&](PointerRNA *p, PropertyRNA *prop, int /*index*/) {
    RNA_property_string_set(p, prop, value);
  });
}

void WM_OT_context_set_string(wmOperatorType *ot)
{
  ot->name = "Context Set String";
  ot->idname = "WM_OT_context_set_string";
  ot->description = "Set a context value";
  ot->exec = context_set_string_exec;
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  def_data_path(ot);
  RNA_def_string(ot->srna, "value", nullptr, 1024, "Value", "Assign value");
}

static wmOperatorStatus context_set_enum_exec(bContext *C, wmOperator *op)
{
  char value[1024];
  RNA_string_get(op->ptr, "value", value);
  return context_assign(C, op, PROP_ENUM, [&](PointerRNA *p, PropertyRNA *prop, int /*index*/) {
    /* El valor llega como identificador de texto, igual que en el Python. */
    int enum_value = 0;
    if (RNA_property_enum_value(C, p, prop, value, &enum_value)) {
      RNA_property_enum_set(p, prop, enum_value);
    }
  });
}

void WM_OT_context_set_enum(wmOperatorType *ot)
{
  ot->name = "Context Set Enum";
  ot->idname = "WM_OT_context_set_enum";
  ot->description = "Set a context value";
  ot->exec = context_set_enum_exec;
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  def_data_path(ot);
  RNA_def_string(ot->srna, "value", nullptr, 1024, "Value", "Assignment value (as a string)");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Alternar entre dos valores
 * \{ */

static wmOperatorStatus context_toggle_enum_exec(bContext *C, wmOperator *op)
{
  char v1[1024], v2[1024];
  RNA_string_get(op->ptr, "value_1", v1);
  RNA_string_get(op->ptr, "value_2", v2);

  return context_assign(C, op, PROP_ENUM, [&](PointerRNA *p, PropertyRNA *prop, int /*index*/) {
    const int current = RNA_property_enum_get(p, prop);
    int e1 = 0, e2 = 0;
    if (!RNA_property_enum_value(C, p, prop, v1, &e1) ||
        !RNA_property_enum_value(C, p, prop, v2, &e2))
    {
      return;
    }
    RNA_property_enum_set(p, prop, (current == e1) ? e2 : e1);
  });
}

void WM_OT_context_toggle_enum(wmOperatorType *ot)
{
  ot->name = "Context Toggle Values";
  ot->idname = "WM_OT_context_toggle_enum";
  ot->description = "Toggle a context value";
  ot->exec = context_toggle_enum_exec;
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  def_data_path(ot);
  RNA_def_string(ot->srna, "value_1", nullptr, 1024, "Value", "Toggle enum");
  RNA_def_string(ot->srna, "value_2", nullptr, 1024, "Value", "Toggle enum");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Recorrer valores
 * \{ */

static wmOperatorStatus context_cycle_int_exec(bContext *C, wmOperator *op)
{
  const bool reverse = RNA_boolean_get(op->ptr, "reverse");
  const bool wrap = RNA_boolean_get(op->ptr, "wrap");

  char data_path[1024];
  RNA_string_get(op->ptr, "data_path", data_path);
  const ResolvedPath r = resolve(C, data_path);
  if (!r.ok()) {
    return OPERATOR_PASS_THROUGH;
  }
  PointerRNA *p = const_cast<PointerRNA *>(&r.ptr);

  const PropertyType type = RNA_property_type(r.prop);
  if (type != PROP_INT && type != PROP_ENUM) {
    return OPERATOR_PASS_THROUGH;
  }

  int value = (type == PROP_INT) ? RNA_property_int_get(p, r.prop) :
                                   RNA_property_enum_get(p, r.prop);
  value += reverse ? -1 : 1;

  if (type == PROP_INT) {
    RNA_property_int_set(p, r.prop, value);
    /* Si el valor se salio del rango, RNA lo recorta; con `wrap` se va al extremo
     * contrario, que es lo que hacia el Python comparando antes y despues. */
    const int clamped = RNA_property_int_get(p, r.prop);
    if (wrap && clamped != value) {
      int min = 0, max = 0;
      RNA_property_int_range(p, r.prop, &min, &max);
      RNA_property_int_set(p, r.prop, reverse ? max : min);
    }
  }
  else {
    RNA_property_enum_set(p, r.prop, value);
    const int clamped = RNA_property_enum_get(p, r.prop);
    if (wrap && clamped != value) {
      RNA_property_enum_set(p, r.prop, reverse ? INT_MAX : 0);
    }
  }
  RNA_property_update(C, p, r.prop);
  return undo_return(r);
}

void WM_OT_context_cycle_int(wmOperatorType *ot)
{
  ot->name = "Context Int Cycle";
  ot->idname = "WM_OT_context_cycle_int";
  ot->description = "Set a context value (increment or decrement)";
  ot->exec = context_cycle_int_exec;
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  def_data_path(ot);
  RNA_def_boolean(ot->srna, "reverse", false, "Reverse", "Cycle backwards");
  RNA_def_boolean(ot->srna, "wrap", false, "Wrap", "Wrap back to the first/last values");
}

static wmOperatorStatus context_cycle_enum_exec(bContext *C, wmOperator *op)
{
  const bool reverse = RNA_boolean_get(op->ptr, "reverse");
  const bool wrap = RNA_boolean_get(op->ptr, "wrap");

  char data_path[1024];
  RNA_string_get(op->ptr, "data_path", data_path);
  const ResolvedPath r = resolve(C, data_path);
  if (!r.ok() || RNA_property_type(r.prop) != PROP_ENUM) {
    return OPERATOR_PASS_THROUGH;
  }
  PointerRNA *p = const_cast<PointerRNA *>(&r.ptr);

  /* Se recorre la lista de identificadores, que puede ser dinamica. */
  const EnumPropertyItem *items = nullptr;
  bool free_items = false;
  int totitem = 0;
  RNA_property_enum_items(C, p, r.prop, &items, &totitem, &free_items);
  if (items == nullptr || totitem == 0) {
    if (free_items && items) {
      MEM_freeN(items);
    }
    return OPERATOR_PASS_THROUGH;
  }

  const int current = RNA_property_enum_get(p, r.prop);
  int index = -1;
  for (int i = 0; i < totitem; i++) {
    if (items[i].identifier && items[i].value == current) {
      index = i;
      break;
    }
  }

  if (index != -1) {
    int next = index + (reverse ? -1 : 1);
    /* Los separadores no tienen identificador y no cuentan como valor. */
    while (next >= 0 && next < totitem && items[next].identifier == nullptr) {
      next += reverse ? -1 : 1;
    }
    if (next < 0 || next >= totitem) {
      if (wrap) {
        next = reverse ? totitem - 1 : 0;
        while (next >= 0 && next < totitem && items[next].identifier == nullptr) {
          next += reverse ? -1 : 1;
        }
      }
      else {
        next = index;
      }
    }
    if (next >= 0 && next < totitem && items[next].identifier) {
      RNA_property_enum_set(p, r.prop, items[next].value);
      RNA_property_update(C, p, r.prop);
    }
  }

  if (free_items) {
    MEM_freeN(items);
  }
  return undo_return(r);
}

void WM_OT_context_cycle_enum(wmOperatorType *ot)
{
  ot->name = "Context Enum Cycle";
  ot->idname = "WM_OT_context_cycle_enum";
  ot->description = "Toggle a context value";
  ot->exec = context_cycle_enum_exec;
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  def_data_path(ot);
  RNA_def_boolean(ot->srna, "reverse", false, "Reverse", "Cycle backwards");
  RNA_def_boolean(ot->srna, "wrap", false, "Wrap", "Wrap back to the first/last values");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Escalar un valor
 * \{ */

static wmOperatorStatus context_scale_float_exec(bContext *C, wmOperator *op)
{
  const float factor = RNA_float_get(op->ptr, "value");
  if (factor == 1.0f) {
    return OPERATOR_CANCELLED;
  }
  return context_assign(C, op, PROP_FLOAT, [&](PointerRNA *p, PropertyRNA *prop, int /*index*/) {
    RNA_property_float_set(p, prop, RNA_property_float_get(p, prop) * factor);
  });
}

void WM_OT_context_scale_float(wmOperatorType *ot)
{
  ot->name = "Context Scale Float";
  ot->idname = "WM_OT_context_scale_float";
  ot->description = "Scale a float context value";
  ot->exec = context_scale_float_exec;
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  def_data_path(ot);
  RNA_def_float(
      ot->srna, "value", 1.0f, -FLT_MAX, FLT_MAX, "Value", "Assign value", -FLT_MAX, FLT_MAX);
}

static wmOperatorStatus context_scale_int_exec(bContext *C, wmOperator *op)
{
  const float factor = RNA_float_get(op->ptr, "value");
  const bool always_step = RNA_boolean_get(op->ptr, "always_step");
  if (factor == 1.0f) {
    return OPERATOR_CANCELLED;
  }
  return context_assign(C, op, PROP_INT, [&](PointerRNA *p, PropertyRNA *prop, int /*index*/) {
    const int base = RNA_property_int_get(p, prop);
    int result = int(float(base) * factor);
    /* Con factores cercanos a 1 el redondeo no llegaria a mover el valor; el Python
     * forzaba un paso de uno para que el atajo siempre haga algo (wm.py:353-400). */
    if (always_step && result == base) {
      result += (factor > 1.0f) ? 1 : -1;
    }
    RNA_property_int_set(p, prop, result);
  });
}

void WM_OT_context_scale_int(wmOperatorType *ot)
{
  ot->name = "Context Scale Int";
  ot->idname = "WM_OT_context_scale_int";
  ot->description = "Scale an int context value";
  ot->exec = context_scale_int_exec;
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  def_data_path(ot);
  RNA_def_float(
      ot->srna, "value", 1.0f, -FLT_MAX, FLT_MAX, "Value", "Assign value", -FLT_MAX, FLT_MAX);
  RNA_def_boolean(ot->srna,
                  "always_step",
                  true,
                  "Always Step",
                  "Always adjust the value by a minimum of 1 when 'value' is not 1.0");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ayudantes de las siete operaciones contextuales restantes
 * \{ */

namespace {

void def_data_path_python(wmOperatorType *ot)
{
  /* `maxlen=1024` de bpy.props incluye 1024 caracteres mas el terminador;
   * RNA_def_string recibe el tamaño del almacenamiento, de ahi 1025. */
  RNA_def_string(ot->srna,
                 "data_path",
                 nullptr,
                 1025,
                 "Context Attributes",
                 "Context data-path (expanded using visible windows in the current .blend file)");
}

void def_collection_paths(wmOperatorType *ot)
{
  RNA_def_string(ot->srna,
                 "data_path_iter",
                 nullptr,
                 0,
                 "data_path_iter",
                 "The data path relative to the context, must point to an iterable");
  RNA_def_string(ot->srna,
                 "data_path_item",
                 nullptr,
                 0,
                 "data_path_item",
                 "The data path from each iterable to the value (int or float)");
}

ResolvedPath resolve_from_pointer(PointerRNA root, const char *data_path)
{
  ResolvedPath out;
  if (data_path != nullptr && data_path[0] != '\0') {
    RNA_path_resolve_property_full(&root, data_path, &out.ptr, &out.prop, &out.index);
  }
  return out;
}

wmOperatorStatus undo_return_owner(const PointerRNA &ptr)
{
  const ID *owner = ptr.owner_id;
  if (owner == nullptr) {
    return OPERATOR_CANCELLED;
  }
  const short type = GS(owner->name);
  return ELEM(type, ID_WM, ID_SCR, ID_BR) ? OPERATOR_CANCELLED : OPERATOR_FINISHED;
}

std::string context_path_description(bContext *C,
                                     PointerRNA *props,
                                     const char *prefix,
                                     const bool include_value)
{
  char data_path[1025];
  RNA_string_get(props, "data_path", data_path);
  const ResolvedPath r = resolve(C, data_path);
  if (!r.ok()) {
    return {};
  }

  const char *description = RNA_property_ui_description(r.prop);
  if (description == nullptr || description[0] == '\0') {
    return {};
  }

  std::string result = fmt::format(fmt::runtime(TIP_("{:s}: {:s}")), prefix, description);
  if (include_value) {
    char value[1025];
    RNA_string_get(props, "value", value);
    result += '\n';
    result += fmt::format(fmt::runtime(TIP_("{:s}: {:s}")), TIP_("Value"), value);
  }
  return result;
}

struct Literal {
  enum class Type {
    None,
    Boolean,
    Integer,
    Float,
    String,
    Sequence,
  };

  Type type = Type::None;
  bool boolean = false;
  long long integer = 0;
  double floating = 0.0;
  std::string string;
  std::vector<Literal> sequence;
};

class LiteralParser {
 public:
  explicit LiteralParser(const char *text) : begin_(text ? text : ""), cursor_(begin_) {}

  bool parse(Literal &r_value)
  {
    skip_space();
    if (!parse_value(r_value)) {
      return false;
    }
    skip_space();
    return *cursor_ == '\0';
  }

 private:
  const char *begin_;
  const char *cursor_;

  void skip_space()
  {
    while (std::isspace(static_cast<unsigned char>(*cursor_))) {
      cursor_++;
    }
  }

  bool consume_word(const char *word)
  {
    const size_t length = std::strlen(word);
    if (std::strncmp(cursor_, word, length) != 0) {
      return false;
    }
    const unsigned char next = static_cast<unsigned char>(cursor_[length]);
    if (std::isalnum(next) || next == '_') {
      return false;
    }
    cursor_ += length;
    return true;
  }

  bool parse_value(Literal &r_value)
  {
    skip_space();
    if (*cursor_ == '(' || *cursor_ == '[') {
      return parse_sequence(r_value);
    }
    if (*cursor_ == '\'' || *cursor_ == '"') {
      return parse_string(r_value);
    }
    if (consume_word("True")) {
      r_value.type = Literal::Type::Boolean;
      r_value.boolean = true;
      return true;
    }
    if (consume_word("False")) {
      r_value.type = Literal::Type::Boolean;
      r_value.boolean = false;
      return true;
    }
    if (consume_word("None")) {
      r_value.type = Literal::Type::None;
      return true;
    }
    return parse_number(r_value);
  }

  bool parse_sequence(Literal &r_value)
  {
    const char close = (*cursor_ == '(') ? ')' : ']';
    cursor_++;
    r_value.type = Literal::Type::Sequence;
    skip_space();
    if (*cursor_ == close) {
      cursor_++;
      return true;
    }

    while (*cursor_) {
      Literal item;
      if (!parse_value(item)) {
        return false;
      }
      r_value.sequence.push_back(std::move(item));
      skip_space();
      if (*cursor_ == close) {
        cursor_++;
        return true;
      }
      if (*cursor_ != ',') {
        return false;
      }
      cursor_++;
      skip_space();
      if (*cursor_ == close) {
        cursor_++;
        return true;
      }
    }
    return false;
  }

  static int hex_value(const char c)
  {
    if (c >= '0' && c <= '9') {
      return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
      return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
      return c - 'A' + 10;
    }
    return -1;
  }

  bool parse_string(Literal &r_value)
  {
    const char quote_char = *cursor_++;
    r_value.type = Literal::Type::String;
    while (*cursor_ && *cursor_ != quote_char) {
      char value = *cursor_++;
      if (value != '\\') {
        r_value.string += value;
        continue;
      }
      value = *cursor_++;
      if (value == '\0') {
        return false;
      }
      switch (value) {
        case 'n':
          r_value.string += '\n';
          break;
        case 'r':
          r_value.string += '\r';
          break;
        case 't':
          r_value.string += '\t';
          break;
        case 'b':
          r_value.string += '\b';
          break;
        case 'f':
          r_value.string += '\f';
          break;
        case 'x': {
          const int high = hex_value(cursor_[0]);
          const int low = hex_value(cursor_[1]);
          if (high < 0 || low < 0) {
            return false;
          }
          r_value.string += char((high << 4) | low);
          cursor_ += 2;
          break;
        }
        default:
          r_value.string += value;
          break;
      }
    }
    if (*cursor_ != quote_char) {
      return false;
    }
    cursor_++;
    return true;
  }

  bool parse_number(Literal &r_value)
  {
    char *end = nullptr;
    const double value = std::strtod(cursor_, &end);
    if (end == cursor_) {
      return false;
    }
    bool is_float = false;
    for (const char *p = cursor_; p < end; p++) {
      if (*p == '.' || *p == 'e' || *p == 'E') {
        is_float = true;
        break;
      }
    }
    if (is_float) {
      r_value.type = Literal::Type::Float;
      r_value.floating = value;
    }
    else {
      char *integer_end = nullptr;
      r_value.integer = std::strtoll(cursor_, &integer_end, 0);
      if (integer_end != end) {
        return false;
      }
      r_value.type = Literal::Type::Integer;
    }
    cursor_ = end;
    return true;
  }
};

void flatten_sequence(const Literal &literal, std::vector<const Literal *> &r_values)
{
  if (literal.type == Literal::Type::Sequence) {
    for (const Literal &item : literal.sequence) {
      flatten_sequence(item, r_values);
    }
  }
  else {
    r_values.push_back(&literal);
  }
}

bool literal_as_int(const Literal &literal, int &r_value)
{
  if (literal.type == Literal::Type::Integer) {
    r_value = int(literal.integer);
    return literal.integer >= INT_MIN && literal.integer <= INT_MAX;
  }
  return false;
}

bool literal_as_float(const Literal &literal, float &r_value)
{
  if (literal.type == Literal::Type::Float) {
    r_value = float(literal.floating);
    return true;
  }
  if (literal.type == Literal::Type::Integer) {
    r_value = float(literal.integer);
    return true;
  }
  return false;
}

bool assign_literal(bContext *C, const ResolvedPath &r, const Literal &literal)
{
  PointerRNA *ptr = const_cast<PointerRNA *>(&r.ptr);
  const PropertyType type = RNA_property_type(r.prop);
  const int array_length = RNA_property_array_length(ptr, r.prop);

  if (array_length != 0 && r.index == -1) {
    if (literal.type != Literal::Type::Sequence) {
      return false;
    }
    std::vector<const Literal *> values;
    flatten_sequence(literal, values);
    if (int(values.size()) != array_length) {
      return false;
    }

    if (type == PROP_BOOLEAN) {
      std::unique_ptr<bool[]> array(new bool[size_t(array_length)]);
      for (int i = 0; i < array_length; i++) {
        if (values[size_t(i)]->type != Literal::Type::Boolean) {
          return false;
        }
        array[size_t(i)] = values[size_t(i)]->boolean;
      }
      RNA_property_boolean_set_array(ptr, r.prop, array.get());
    }
    else if (type == PROP_INT) {
      std::vector<int> array(size_t(array_length), 0);
      for (int i = 0; i < array_length; i++) {
        if (!literal_as_int(*values[size_t(i)], array[size_t(i)])) {
          return false;
        }
      }
      RNA_property_int_set_array(ptr, r.prop, array.data());
    }
    else if (type == PROP_FLOAT) {
      std::vector<float> array(size_t(array_length), 0.0f);
      for (int i = 0; i < array_length; i++) {
        if (!literal_as_float(*values[size_t(i)], array[size_t(i)])) {
          return false;
        }
      }
      RNA_property_float_set_array(ptr, r.prop, array.data());
    }
    else {
      return false;
    }
  }
  else {
    switch (type) {
      case PROP_BOOLEAN:
        if (literal.type != Literal::Type::Boolean) {
          return false;
        }
        if (r.index == -1) {
          RNA_property_boolean_set(ptr, r.prop, literal.boolean);
        }
        else {
          RNA_property_boolean_set_index(ptr, r.prop, r.index, literal.boolean);
        }
        break;
      case PROP_INT: {
        int value;
        if (!literal_as_int(literal, value)) {
          return false;
        }
        if (r.index == -1) {
          RNA_property_int_set(ptr, r.prop, value);
        }
        else {
          RNA_property_int_set_index(ptr, r.prop, r.index, value);
        }
        break;
      }
      case PROP_FLOAT: {
        float value;
        if (!literal_as_float(literal, value)) {
          return false;
        }
        if (r.index == -1) {
          RNA_property_float_set(ptr, r.prop, value);
        }
        else {
          RNA_property_float_set_index(ptr, r.prop, r.index, value);
        }
        break;
      }
      case PROP_STRING:
        if (literal.type != Literal::Type::String || r.index != -1) {
          return false;
        }
        RNA_property_string_set(ptr, r.prop, literal.string.c_str());
        break;
      case PROP_ENUM:
        if (literal.type == Literal::Type::String) {
          int value;
          if (!RNA_property_enum_value(C, ptr, r.prop, literal.string.c_str(), &value)) {
            return false;
          }
          RNA_property_enum_set(ptr, r.prop, value);
        }
        else {
          int value;
          if (!literal_as_int(literal, value)) {
            return false;
          }
          RNA_property_enum_set(ptr, r.prop, value);
        }
        break;
      case PROP_POINTER:
        if (literal.type != Literal::Type::None || r.index != -1) {
          return false;
        }
        RNA_property_pointer_set(ptr, r.prop, PointerRNA_NULL, nullptr);
        break;
      case PROP_COLLECTION:
        return false;
    }
  }

  RNA_property_update(C, ptr, r.prop);
  return true;
}

template<typename T> void rotate_values(std::vector<T> &values, const bool reverse)
{
  if (values.size() < 2) {
    return;
  }
  if (reverse) {
    std::rotate(values.begin(), values.end() - 1, values.end());
  }
  else {
    std::rotate(values.begin(), values.begin() + 1, values.end());
  }
}

}  // namespace

/** \} */

/* -------------------------------------------------------------------- */
/** \name wm.context_set_value
 * \{ */

static std::string context_set_value_description(bContext *C,
                                                 wmOperatorType * /*ot*/,
                                                 PointerRNA *props)
{
  return context_path_description(C, props, TIP_("Assign"), true);
}

static wmOperatorStatus context_set_value_exec(bContext *C, wmOperator *op)
{
  char data_path[1025], value_text[1025];
  RNA_string_get(op->ptr, "data_path", data_path);
  RNA_string_get(op->ptr, "value", value_text);
  const ResolvedPath r = resolve(C, data_path);
  if (!r.ok()) {
    return OPERATOR_PASS_THROUGH;
  }

  Literal value;
  if (!LiteralParser(value_text).parse(value) || !assign_literal(C, r, value)) {
    BKE_reportf(op->reports,
                RPT_ERROR,
                "Cannot assign literal '%s' to context.%s",
                value_text,
                data_path);
    return OPERATOR_CANCELLED;
  }
  return undo_return(r);
}

void WM_OT_context_set_value(wmOperatorType *ot)
{
  ot->name = "Context Set Value";
  ot->idname = "WM_OT_context_set_value";
  ot->description = "Set a context value";
  ot->exec = context_set_value_exec;
  ot->get_description = context_set_value_description;
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  def_data_path_python(ot);
  RNA_def_string(ot->srna,
                 "value",
                 nullptr,
                 1025,
                 "Value",
                 "Assignment value (as a string)");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name wm.context_cycle_array
 * \{ */

static std::string context_cycle_array_description(bContext *C,
                                                   wmOperatorType * /*ot*/,
                                                   PointerRNA *props)
{
  return context_path_description(C, props, TIP_("Cycle"), false);
}

static wmOperatorStatus context_cycle_array_exec(bContext *C, wmOperator *op)
{
  char data_path[1025];
  RNA_string_get(op->ptr, "data_path", data_path);
  const bool reverse = RNA_boolean_get(op->ptr, "reverse");
  const ResolvedPath r = resolve(C, data_path);
  if (!r.ok() || r.index != -1) {
    return OPERATOR_PASS_THROUGH;
  }

  PointerRNA *ptr = const_cast<PointerRNA *>(&r.ptr);
  const int length = RNA_property_array_length(ptr, r.prop);
  if (length == 0) {
    return OPERATOR_PASS_THROUGH;
  }

  switch (RNA_property_type(r.prop)) {
    case PROP_BOOLEAN: {
      std::unique_ptr<bool[]> values(new bool[size_t(length)]);
      RNA_property_boolean_get_array(ptr, r.prop, values.get());
      if (reverse) {
        const bool last = values[size_t(length - 1)];
        for (int i = length - 1; i > 0; i--) {
          values[size_t(i)] = values[size_t(i - 1)];
        }
        values[0] = last;
      }
      else {
        const bool first = values[0];
        for (int i = 0; i < length - 1; i++) {
          values[size_t(i)] = values[size_t(i + 1)];
        }
        values[size_t(length - 1)] = first;
      }
      RNA_property_boolean_set_array(ptr, r.prop, values.get());
      break;
    }
    case PROP_INT: {
      std::vector<int> values(size_t(length), 0);
      RNA_property_int_get_array(ptr, r.prop, values.data());
      rotate_values(values, reverse);
      RNA_property_int_set_array(ptr, r.prop, values.data());
      break;
    }
    case PROP_FLOAT: {
      std::vector<float> values(size_t(length), 0.0f);
      RNA_property_float_get_array(ptr, r.prop, values.data());
      rotate_values(values, reverse);
      RNA_property_float_set_array(ptr, r.prop, values.data());
      break;
    }
    default:
      return OPERATOR_PASS_THROUGH;
  }

  RNA_property_update(C, ptr, r.prop);
  return undo_return(r);
}

void WM_OT_context_cycle_array(wmOperatorType *ot)
{
  ot->name = "Context Array Cycle";
  ot->idname = "WM_OT_context_cycle_array";
  ot->description = "Set a context array value (useful for cycling the active mesh edit mode)";
  ot->exec = context_cycle_array_exec;
  ot->get_description = context_cycle_array_description;
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  def_data_path_python(ot);
  PropertyRNA *prop = RNA_def_boolean(
      ot->srna, "reverse", false, "Reverse", "Cycle backwards");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name wm.context_menu_enum / wm.context_pie_enum
 * \{ */

static std::string context_menu_enum_description(bContext *C,
                                                 wmOperatorType * /*ot*/,
                                                 PointerRNA *props)
{
  return context_path_description(C, props, TIP_("Menu"), false);
}

static std::string context_pie_enum_description(bContext *C,
                                                wmOperatorType * /*ot*/,
                                                PointerRNA *props)
{
  return context_path_description(C, props, TIP_("Pie Menu"), false);
}

static wmOperatorStatus context_menu_enum_exec(bContext *C, wmOperator *op)
{
  char data_path[1025];
  RNA_string_get(op->ptr, "data_path", data_path);
  const ResolvedPath r = resolve(C, data_path);
  if (!r.ok()) {
    return OPERATOR_PASS_THROUGH;
  }
  if (RNA_property_type(r.prop) != PROP_ENUM) {
    return OPERATOR_CANCELLED;
  }

  uiPopupMenu *popup = UI_popup_menu_begin(
      C, RNA_property_ui_name(r.prop), RNA_property_ui_icon(r.prop));
  uiLayout *layout = UI_popup_menu_layout(popup);
  layout->prop(const_cast<PointerRNA *>(&r.ptr),
               r.prop,
               r.index,
               0,
               UI_ITEM_R_EXPAND,
               std::nullopt,
               ICON_NONE);
  UI_popup_menu_end(C, popup);
  return OPERATOR_FINISHED;
}

void WM_OT_context_menu_enum(wmOperatorType *ot)
{
  ot->name = "Context Enum Menu";
  ot->idname = "WM_OT_context_menu_enum";
  ot->description = nullptr;
  ot->exec = context_menu_enum_exec;
  ot->get_description = context_menu_enum_description;
  ot->flag = OPTYPE_INTERNAL;
  def_data_path_python(ot);
}

static wmOperatorStatus context_pie_enum_invoke(bContext *C,
                                                wmOperator *op,
                                                const wmEvent *event)
{
  char data_path[1025];
  RNA_string_get(op->ptr, "data_path", data_path);
  const ResolvedPath r = resolve(C, data_path);
  if (!r.ok()) {
    return OPERATOR_PASS_THROUGH;
  }
  if (RNA_property_type(r.prop) != PROP_ENUM) {
    return OPERATOR_CANCELLED;
  }

  uiPieMenu *pie = UI_pie_menu_begin(
      C, RNA_property_ui_name(r.prop), RNA_property_ui_icon(r.prop), event);
  uiLayout *layout = &UI_pie_menu_layout(pie)->menu_pie();
  layout->prop(const_cast<PointerRNA *>(&r.ptr),
               r.prop,
               r.index,
               0,
               UI_ITEM_R_EXPAND,
               std::nullopt,
               ICON_NONE);
  UI_pie_menu_end(C, pie);
  return OPERATOR_FINISHED;
}

void WM_OT_context_pie_enum(wmOperatorType *ot)
{
  ot->name = "Context Enum Pie";
  ot->idname = "WM_OT_context_pie_enum";
  ot->description = nullptr;
  ot->invoke = context_pie_enum_invoke;
  ot->get_description = context_pie_enum_description;
  ot->flag = OPTYPE_INTERNAL;
  def_data_path_python(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name wm.context_set_id
 * \{ */

static wmOperatorStatus context_set_id_exec(bContext *C, wmOperator *op)
{
  char data_path[1025], value[1025];
  RNA_string_get(op->ptr, "data_path", data_path);
  RNA_string_get(op->ptr, "value", value);
  const ResolvedPath r = resolve(C, data_path);
  if (!r.ok() || r.index != -1 || RNA_property_type(r.prop) != PROP_POINTER) {
    return OPERATOR_PASS_THROUGH;
  }

  PointerRNA *ptr = const_cast<PointerRNA *>(&r.ptr);
  StructRNA *fixed_type = RNA_property_pointer_type(ptr, r.prop);
  const short id_code = fixed_type ? RNA_type_to_ID_code(fixed_type) : 0;
  if (id_code == 0) {
    return undo_return(r);
  }

  ID *id = BKE_libblock_find_name(CTX_data_main(C), id_code, value);
  const PointerRNA value_ptr = id ? RNA_id_pointer_create(id) : PointerRNA_NULL;
  RNA_property_pointer_set(ptr, r.prop, value_ptr, op->reports);
  RNA_property_update(C, ptr, r.prop);
  return undo_return(r);
}

void WM_OT_context_set_id(wmOperatorType *ot)
{
  ot->name = "Set Library ID";
  ot->idname = "WM_OT_context_set_id";
  ot->description = "Set a context value to an ID data-block";
  ot->exec = context_set_id_exec;
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  def_data_path_python(ot);
  RNA_def_string(ot->srna, "value", nullptr, 1025, "Value", "Assign value");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name wm.context_collection_boolean_set
 * \{ */

static const EnumPropertyItem context_collection_boolean_set_items[] = {
    {0, "TOGGLE", 0, "Toggle", ""},
    {1, "ENABLE", 0, "Enable", ""},
    {2, "DISABLE", 0, "Disable", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static wmOperatorStatus context_collection_boolean_set_exec(bContext *C, wmOperator *op)
{
  char data_path_iter[1024], data_path_item[1024];
  RNA_string_get(op->ptr, "data_path_iter", data_path_iter);
  RNA_string_get(op->ptr, "data_path_item", data_path_item);

  const blender::Vector<PointerRNA> items = CTX_data_collection_get(C, data_path_iter);
  std::vector<ResolvedPath> valid;
  bool is_set = false;
  for (const PointerRNA &item : items) {
    ResolvedPath r = resolve_from_pointer(item, data_path_item);
    if (!r.ok()) {
      continue;
    }
    if (RNA_property_type(r.prop) != PROP_BOOLEAN || r.index != -1) {
      BKE_reportf(op->reports,
                  RPT_WARNING,
                  "Non boolean value found: %s[ ].%s",
                  data_path_iter,
                  data_path_item);
      return OPERATOR_CANCELLED;
    }
    PointerRNA *ptr = &r.ptr;
    is_set |= RNA_property_boolean_get(ptr, r.prop);
    valid.push_back(r);
  }

  if (valid.empty()) {
    return OPERATOR_CANCELLED;
  }

  switch (RNA_enum_get(op->ptr, "type")) {
    case 1:
      is_set = true;
      break;
    case 2:
      is_set = false;
      break;
    default:
      is_set = !is_set;
      break;
  }

  for (ResolvedPath &r : valid) {
    RNA_property_boolean_set(&r.ptr, r.prop, is_set);
    RNA_property_update(C, &r.ptr, r.prop);
  }
  return undo_return_owner(valid.back().ptr);
}

void WM_OT_context_collection_boolean_set(wmOperatorType *ot)
{
  ot->name = "Context Collection Boolean Set";
  ot->idname = "WM_OT_context_collection_boolean_set";
  ot->description = "Set boolean values for a collection of items";
  ot->exec = context_collection_boolean_set_exec;
  ot->flag = OPTYPE_UNDO | OPTYPE_REGISTER | OPTYPE_INTERNAL;

  def_collection_paths(ot);
  RNA_def_enum(
      ot->srna, "type", context_collection_boolean_set_items, 0, "Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name wm.context_modal_mouse
 * \{ */

namespace {

struct ContextModalValue {
  ResolvedPath resolved;
  double original;
  bool is_int;
};

struct ContextModalData {
  std::vector<ContextModalValue> values;
};

void context_modal_apply(bContext *C, wmOperator *op, ContextModalData &data, double delta)
{
  delta *= RNA_float_get(op->ptr, "input_scale");
  if (RNA_boolean_get(op->ptr, "invert")) {
    delta = -delta;
  }

  for (ContextModalValue &value : data.values) {
    PointerRNA *ptr = &value.resolved.ptr;
    if (value.is_int) {
      /* Python round usa empate al par; nearbyint reproduce ese criterio con
       * el modo de redondeo normal del proceso. */
      RNA_property_int_set(ptr, value.resolved.prop, int(std::nearbyint(value.original + delta)));
    }
    else {
      /* El Python materializaba el nuevo valor con `{:f}` antes del `exec`, por
       * lo que redondeaba a seis decimales en cada movimiento. */
      char value_text[64];
      BLI_snprintf(value_text, sizeof(value_text), "%.6f", value.original + delta);
      RNA_property_float_set(ptr, value.resolved.prop, std::strtof(value_text, nullptr));
    }
    RNA_property_update(C, ptr, value.resolved.prop);
  }
}

void context_modal_restore(bContext *C, ContextModalData &data)
{
  for (ContextModalValue &value : data.values) {
    PointerRNA *ptr = &value.resolved.ptr;
    if (value.is_int) {
      RNA_property_int_set(ptr, value.resolved.prop, int(value.original));
    }
    else {
      RNA_property_float_set(ptr, value.resolved.prop, float(value.original));
    }
    RNA_property_update(C, ptr, value.resolved.prop);
  }
}

}  // namespace

static wmOperatorStatus context_modal_mouse_modal(bContext *C,
                                                  wmOperator *op,
                                                  const wmEvent *event)
{
  ContextModalData *data = static_cast<ContextModalData *>(op->customdata);
  if (data == nullptr || data->values.empty()) {
    return OPERATOR_CANCELLED;
  }

  if (event->type == MOUSEMOVE) {
    const int delta = event->xy[0] - RNA_int_get(op->ptr, "initial_x");
    context_modal_apply(C, op, *data, delta);

    char header_text[1024];
    RNA_string_get(op->ptr, "header_text", header_text);
    if (header_text[0] != '\0') {
      char formatted[2048];
      if (data->values.size() == 1) {
        const ContextModalValue &value = data->values.front();
        if (value.is_int) {
          const int current = RNA_property_int_get(
              const_cast<PointerRNA *>(&value.resolved.ptr), value.resolved.prop);
          BLI_snprintf(formatted, sizeof(formatted), header_text, current);
        }
        else {
          const float current = RNA_property_float_get(
              const_cast<PointerRNA *>(&value.resolved.ptr), value.resolved.prop);
          BLI_snprintf(formatted, sizeof(formatted), header_text, double(current));
        }
      }
      else {
        BLI_snprintf(formatted, sizeof(formatted), header_text, double(delta));
        BLI_strncat(formatted, RPT_(" (delta)"), sizeof(formatted));
      }
      ED_area_status_text(CTX_wm_area(C), formatted);
    }
  }
  else if (event->type == LEFTMOUSE) {
    const PointerRNA owner = data->values.front().resolved.ptr;
    MEM_delete(data);
    op->customdata = nullptr;
    ED_area_status_text(CTX_wm_area(C), nullptr);
    return undo_return_owner(owner);
  }
  else if (ELEM(event->type, RIGHTMOUSE, EVT_ESCKEY)) {
    context_modal_restore(C, *data);
    MEM_delete(data);
    op->customdata = nullptr;
    ED_area_status_text(CTX_wm_area(C), nullptr);
    return OPERATOR_CANCELLED;
  }

  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus context_modal_mouse_invoke(bContext *C,
                                                   wmOperator *op,
                                                   const wmEvent *event)
{
  char data_path_iter[1024], data_path_item[1024];
  RNA_string_get(op->ptr, "data_path_iter", data_path_iter);
  RNA_string_get(op->ptr, "data_path_item", data_path_item);

  ContextModalData *data = MEM_new<ContextModalData>(__func__);
  const blender::Vector<PointerRNA> items = CTX_data_collection_get(C, data_path_iter);
  for (const PointerRNA &item : items) {
    ResolvedPath r = resolve_from_pointer(item, data_path_item);
    if (!r.ok() || r.index != -1 || !RNA_property_editable(&r.ptr, r.prop)) {
      continue;
    }
    const PropertyType type = RNA_property_type(r.prop);
    if (type == PROP_INT) {
      data->values.push_back({r, double(RNA_property_int_get(&r.ptr, r.prop)), true});
    }
    else if (type == PROP_FLOAT) {
      data->values.push_back({r, double(RNA_property_float_get(&r.ptr, r.prop)), false});
    }
  }

  if (data->values.empty()) {
    MEM_delete(data);
    BKE_reportf(op->reports,
                RPT_WARNING,
                "Nothing to operate on: %s[ ].%s",
                data_path_iter,
                data_path_item);
    return OPERATOR_CANCELLED;
  }

  op->customdata = data;
  RNA_int_set(op->ptr, "initial_x", event->xy[0]);
  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

void WM_OT_context_modal_mouse(wmOperatorType *ot)
{
  ot->name = "Context Modal Mouse";
  ot->idname = "WM_OT_context_modal_mouse";
  ot->description = "Adjust arbitrary values with mouse input";
  ot->invoke = context_modal_mouse_invoke;
  ot->modal = context_modal_mouse_modal;
  ot->flag = OPTYPE_GRAB_CURSOR_XY | OPTYPE_BLOCKING | OPTYPE_UNDO | OPTYPE_INTERNAL;

  def_collection_paths(ot);
  RNA_def_string(ot->srna,
                 "header_text",
                 nullptr,
                 0,
                 "Header Text",
                 "Text to display in header during scale");

  PropertyRNA *prop = RNA_def_float(ot->srna,
                                    "input_scale",
                                    0.01f,
                                    -FLT_MAX,
                                    FLT_MAX,
                                    "input_scale",
                                    "Scale the mouse movement by this value before applying the delta",
                                    -FLT_MAX,
                                    FLT_MAX);
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 3, 2);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_boolean(ot->srna, "invert", false, "invert", "Invert the mouse input");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_int(
      ot->srna, "initial_x", 0, INT_MIN, INT_MAX, "initial_x", nullptr, INT_MIN, INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN);
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
}

/** \} */
