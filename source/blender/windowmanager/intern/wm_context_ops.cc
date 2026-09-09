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
 * referenciada del editor -- 131 atajos del mapa de teclado la usan -- y una de las
 * piezas por las que el keymap nativo todavia tiene que esperar a que arranque
 * Python. Ver politicas/KEYMAP-A-CPP.md.
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

#include <cstring>

#include "DNA_ID.h"
#include "DNA_brush_types.h"
#include "DNA_screen_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_string.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"

#include "RNA_access.hh"
#include "RNA_path.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_types.hh"

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
