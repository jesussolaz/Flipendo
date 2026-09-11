/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Resolucion de rutas RNA y asignacion de valores. Ver FL_preset.hpp.
 *
 * La raiz de toda ruta de un preset es `context.`, que es literalmente lo que
 * escribia el Python (`bpy.context.`). Se resuelve en el MISMO orden que usaba
 * el interprete (`pyrna_struct_getattro`): primero como propiedad RNA del tipo
 * `Context`, y solo si no existe, por los callbacks de contexto
 * (`CTX_data_pointer_get`). El orden importa: `scene` es propiedad de `Context`
 * y `camera` no, y si se invirtiera habria casos en que la ruta se resolveria a
 * otra cosa sin avisar.
 */

#include "FL_preset.hpp"

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "MEM_guardedalloc.h"

#include "BKE_context.hh"

#include "RNA_access.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"
#include "RNA_types.hh"

namespace flipendo::preset {

namespace {

std::string quote(const std::string &s)
{
  return "'" + s + "'";
}

/**
 * Parte `context.<miembro>.<resto>` y devuelve el puntero del miembro.
 * Sin `r_rest` vacio se llama a `RNA_path_resolve_property` sobre el.
 */
bool resolve_root(bContext *C,
                  const std::string &path,
                  PointerRNA &r_root,
                  std::string &r_rest,
                  std::string &r_error)
{
  if (path.compare(0, 8, "context.") != 0) {
    r_error = "la ruta no empieza por 'context.': " + quote(path);
    return false;
  }
  size_t end = 8;
  while (end < path.size() && path[end] != '.' && path[end] != '[') {
    end++;
  }
  const std::string member = path.substr(8, end - 8);
  if (member.empty()) {
    r_error = "falta el miembro de contexto en " + quote(path);
    return false;
  }
  /* Un `[` tras el miembro pertenece al resto de la ruta. */
  r_rest = (end < path.size() && path[end] == '.') ? path.substr(end + 1) : path.substr(end);

  PointerRNA ctx_ptr = RNA_pointer_create_discrete(nullptr, &RNA_Context, C);
  if (PropertyRNA *prop = RNA_struct_find_property(&ctx_ptr, member.c_str())) {
    if (RNA_property_type(prop) != PROP_POINTER) {
      r_error = "el miembro de contexto " + quote(member) + " no es un puntero";
      return false;
    }
    r_root = RNA_property_pointer_get(&ctx_ptr, prop);
  }
  else {
    r_root = CTX_data_pointer_get(C, member.c_str());
  }
  if (r_root.data == nullptr) {
    r_error = "el contexto no tiene " + quote(member) + " (ruta " + quote(path) + ")";
    return false;
  }
  return true;
}

bool value_as_double(const Value &v, double &r_out)
{
  switch (v.kind) {
    case ValueKind::Int:
      r_out = double(v.i);
      return true;
    case ValueKind::Float:
      r_out = v.f;
      return true;
    case ValueKind::Bool:
      r_out = v.b ? 1.0 : 0.0;
      return true;
    default:
      return false;
  }
}

bool value_as_long(const Value &v, long long &r_out)
{
  switch (v.kind) {
    case ValueKind::Int:
      r_out = v.i;
      return true;
    case ValueKind::Bool:
      r_out = v.b ? 1 : 0;
      return true;
    case ValueKind::Float:
      /* Python rechazaba float -> int; aqui tambien, para no redondear en silencio. */
      return false;
    default:
      return false;
  }
}

const char *kind_name(ValueKind k)
{
  switch (k) {
    case ValueKind::None:
      return "none";
    case ValueKind::Bool:
      return "booleano";
    case ValueKind::Int:
      return "entero";
    case ValueKind::Float:
      return "flotante";
    case ValueKind::String:
      return "cadena";
    case ValueKind::Array:
      return "lista";
    case ValueKind::EnumSet:
      return "conjunto";
  }
  return "?";
}

bool set_property(bContext *C,
                  PointerRNA &ptr,
                  PropertyRNA *prop,
                  const std::string &path,
                  const Value &value,
                  std::string &r_error)
{
  auto type_error = [&](const char *want) {
    char buf[512];
    std::snprintf(buf,
                  sizeof(buf),
                  "%s: se esperaba %s y el preset trae un %s",
                  path.c_str(),
                  want,
                  kind_name(value.kind));
    r_error = buf;
    return false;
  };

  if (!RNA_property_editable_flag(&ptr, prop)) {
    r_error = path + ": la propiedad no es editable";
    return false;
  }

  const PropertyType type = RNA_property_type(prop);
  const bool is_array = RNA_property_array_check(prop);

  if (is_array && type != PROP_COLLECTION) {
    if (value.kind != ValueKind::Array) {
      return type_error("una lista");
    }
    const int len = RNA_property_array_length(&ptr, prop);
    if (int(value.items.size()) != len) {
      char buf[256];
      std::snprintf(buf,
                    sizeof(buf),
                    "%s: la propiedad tiene %d componentes y el preset trae %d",
                    path.c_str(),
                    len,
                    int(value.items.size()));
      r_error = buf;
      return false;
    }
    switch (type) {
      case PROP_BOOLEAN: {
        std::unique_ptr<bool[]> raw(new bool[static_cast<size_t>(len)]);
        for (int i = 0; i < len; i++) {
          long long n;
          if (!value_as_long(value.items[size_t(i)], n)) {
            return type_error("booleanos");
          }
          raw[size_t(i)] = n != 0;
        }
        RNA_property_boolean_set_array(&ptr, prop, raw.get());
        break;
      }
      case PROP_INT: {
        std::vector<int> raw(static_cast<size_t>(len));
        for (int i = 0; i < len; i++) {
          long long n;
          if (!value_as_long(value.items[size_t(i)], n)) {
            return type_error("enteros");
          }
          raw[size_t(i)] = int(n);
        }
        RNA_property_int_set_array(&ptr, prop, raw.data());
        break;
      }
      case PROP_FLOAT: {
        std::vector<float> raw(static_cast<size_t>(len));
        for (int i = 0; i < len; i++) {
          double d;
          if (!value_as_double(value.items[size_t(i)], d)) {
            return type_error("numeros");
          }
          raw[size_t(i)] = float(d);
        }
        RNA_property_float_set_array(&ptr, prop, raw.data());
        break;
      }
      default:
        r_error = path + ": tipo de vector RNA no soportado";
        return false;
    }
  }
  else {
    switch (type) {
      case PROP_BOOLEAN: {
        long long n;
        if (!value_as_long(value, n)) {
          return type_error("un booleano");
        }
        RNA_property_boolean_set(&ptr, prop, n != 0);
        break;
      }
      case PROP_INT: {
        long long n;
        if (!value_as_long(value, n)) {
          return type_error("un entero");
        }
        RNA_property_int_set(&ptr, prop, int(n));
        break;
      }
      case PROP_FLOAT: {
        double d;
        if (!value_as_double(value, d)) {
          return type_error("un numero");
        }
        RNA_property_float_set(&ptr, prop, float(d));
        break;
      }
      case PROP_STRING: {
        if (value.kind != ValueKind::String) {
          return type_error("una cadena");
        }
        RNA_property_string_set(&ptr, prop, value.s.c_str());
        break;
      }
      case PROP_ENUM: {
        const bool is_flag = (RNA_property_flag(prop) & PROP_ENUM_FLAG) != 0;
        if (is_flag) {
          if (value.kind != ValueKind::EnumSet && value.kind != ValueKind::Array) {
            return type_error("un conjunto de identificadores");
          }
          int bits = 0;
          for (const Value &item : value.items) {
            if (item.kind != ValueKind::String) {
              return type_error("identificadores de enumeracion");
            }
            int v = 0;
            if (!RNA_property_enum_value(C, &ptr, prop, item.s.c_str(), &v)) {
              r_error = path + ": " + quote(item.s) + " no es un valor de la enumeracion";
              return false;
            }
            bits |= v;
          }
          RNA_property_enum_set(&ptr, prop, bits);
        }
        else if (value.kind == ValueKind::String) {
          int v = 0;
          if (!RNA_property_enum_value(C, &ptr, prop, value.s.c_str(), &v)) {
            r_error = path + ": " + quote(value.s) + " no es un valor de la enumeracion";
            return false;
          }
          RNA_property_enum_set(&ptr, prop, v);
        }
        else {
          long long n;
          if (!value_as_long(value, n)) {
            return type_error("un identificador de enumeracion");
          }
          RNA_property_enum_set(&ptr, prop, int(n));
        }
        break;
      }
      case PROP_POINTER: {
        if (value.kind != ValueKind::None) {
          r_error = path + ": solo se puede asignar 'none' a un puntero desde un preset";
          return false;
        }
        RNA_property_pointer_set(&ptr, prop, PointerRNA_NULL, nullptr);
        break;
      }
      default:
        r_error = path + ": tipo de propiedad RNA no soportado en un preset";
        return false;
    }
  }

  if (RNA_property_update_check(prop)) {
    RNA_property_update(C, &ptr, prop);
  }
  return true;
}

}  // namespace

/**
 * Evalua el guardian de un `when`: lee la ruta RNA y la compara con el literal.
 *
 * Solo `==` y `!=`, y solo contra un literal. Si la ruta no se puede resolver se
 * informa y se toma la rama falsa: mejor una rama definida que un estado a
 * medias.
 */
static bool when_condition_eval(bContext *C, const Op &op, ApplyReport &r_report)
{
  PointerRNA base;
  std::string rest;
  std::string error;
  if (!resolve_root(C, op.path, base, rest, error)) {
    r_report.errors.push_back("when: " + error);
    return false;
  }
  PointerRNA ptr;
  PropertyRNA *prop = nullptr;
  if (rest.empty() || !RNA_path_resolve_property(&base, rest.c_str(), &ptr, &prop)) {
    r_report.errors.push_back("when: " + op.path + ": esta ruta RNA ya no existe");
    return false;
  }

  bool equal = false;
  switch (RNA_property_type(prop)) {
    case PROP_BOOLEAN:
      equal = (RNA_property_boolean_get(&ptr, prop) ? 1 : 0) ==
              (op.value.kind == ValueKind::Bool ? (op.value.b ? 1 : 0) : int(op.value.i));
      break;
    case PROP_INT:
      equal = RNA_property_int_get(&ptr, prop) ==
              int(op.value.kind == ValueKind::Float ? (long long)(op.value.f) : op.value.i);
      break;
    case PROP_FLOAT:
      equal = double(RNA_property_float_get(&ptr, prop)) ==
              (op.value.kind == ValueKind::Int ? double(op.value.i) : op.value.f);
      break;
    case PROP_ENUM: {
      const char *id = nullptr;
      if (RNA_property_enum_identifier(
              C, &ptr, prop, RNA_property_enum_get(&ptr, prop), &id) &&
          id != nullptr)
      {
        equal = (op.value.kind == ValueKind::String) && (op.value.s == id);
      }
      break;
    }
    case PROP_STRING: {
      char *value = RNA_property_string_get_alloc(&ptr, prop, nullptr, 0, nullptr);
      equal = (op.value.kind == ValueKind::String) && (value != nullptr) &&
              (op.value.s == value);
      if (value != nullptr) {
        MEM_freeN(value);
      }
      break;
    }
    default:
      r_report.errors.push_back("when: " + op.path + ": tipo de propiedad no comparable");
      return false;
  }
  return (op.compare == CompareOp::Equal) ? equal : !equal;
}

bool apply(bContext *C, const Preset &preset, ApplyReport &r_report)
{
  std::vector<PointerRNA> stack;

  /* Pila de guardianes: `taken` dice si la rama actual se ejecuta. */
  std::vector<bool> when_stack;
  const auto skipping = [&when_stack]() {
    for (const bool taken : when_stack) {
      if (!taken) {
        return true;
      }
    }
    return false;
  };

  for (const Op &op : preset.ops) {
    if (op.kind == OpKind::When) {
      /* Si ya estamos saltando, no se evalua: no se toca el estado por error. */
      when_stack.push_back(skipping() ? false : when_condition_eval(C, op, r_report));
      continue;
    }
    if (op.kind == OpKind::Otherwise) {
      if (!when_stack.empty()) {
        when_stack.back() = !when_stack.back();
      }
      continue;
    }
    if (op.kind == OpKind::WhenEnd) {
      if (!when_stack.empty()) {
        when_stack.pop_back();
      }
      continue;
    }
    if (skipping()) {
      continue;
    }
    if (op.kind == OpKind::CollectionEnd) {
      if (!stack.empty()) {
        stack.pop_back();
      }
      continue;
    }

    PointerRNA base;
    std::string rest;
    std::string error;
    if (!op.path.empty() && op.path[0] == '.') {
      if (stack.empty()) {
        r_report.errors.push_back("ruta relativa sin bloque abierto: " + op.path);
        continue;
      }
      base = stack.back();
      rest = op.path.substr(1);
    }
    else if (!resolve_root(C, op.path, base, rest, error)) {
      r_report.errors.push_back(error);
      if (op.kind == OpKind::CollectionAdd) {
        stack.push_back(PointerRNA_NULL);
      }
      continue;
    }

    if (rest.empty()) {
      r_report.errors.push_back(op.path + ": la ruta no llega a ninguna propiedad");
      if (op.kind == OpKind::CollectionAdd) {
        stack.push_back(PointerRNA_NULL);
      }
      continue;
    }

    PointerRNA ptr;
    PropertyRNA *prop = nullptr;
    if (!RNA_path_resolve_property(&base, rest.c_str(), &ptr, &prop)) {
      r_report.errors.push_back(op.path + ": esta ruta RNA ya no existe");
      if (op.kind == OpKind::CollectionAdd) {
        stack.push_back(PointerRNA_NULL);
      }
      continue;
    }

    switch (op.kind) {
      case OpKind::Set:
        if (!set_property(C, ptr, prop, op.path, op.value, error)) {
          r_report.errors.push_back(error);
        }
        else {
          r_report.applied++;
        }
        break;
      case OpKind::Clear:
        if (RNA_property_type(prop) != PROP_COLLECTION) {
          r_report.errors.push_back(op.path + ": 'clear' solo vale sobre una coleccion");
        }
        else {
          RNA_property_collection_clear(&ptr, prop);
          r_report.applied++;
        }
        break;
      case OpKind::CollectionAdd: {
        PointerRNA item = PointerRNA_NULL;
        if (RNA_property_type(prop) != PROP_COLLECTION) {
          r_report.errors.push_back(op.path + ": 'add' solo vale sobre una coleccion");
        }
        else {
          RNA_property_collection_add(&ptr, prop, &item);
          r_report.applied++;
        }
        stack.push_back(item);
        break;
      }
      case OpKind::CollectionEnd:
        break;
    }
  }

  return r_report.errors.empty();
}

bool apply_file(bContext *C, const std::string &filepath, std::string &r_error)
{
  Preset preset;
  if (!read_file(filepath, preset, r_error)) {
    return false;
  }
  ApplyReport report;
  if (apply(C, preset, report)) {
    return true;
  }
  r_error = filepath + ": " + report.errors.front();
  if (report.errors.size() > 1) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), " (y %d error(es) mas)", int(report.errors.size()) - 1);
    r_error += buf;
  }
  return false;
}

/* -------------------------------------------------------------------------- */
/** \name Captura del estado
 *
 * Lo inverso de `apply`. Lo usan el escritor de presets del usuario y el volcado
 * de verificacion; los dos necesitan exactamente lo mismo: el valor de una lista
 * de rutas, formateado de forma determinista.
 * \{ */

static bool capture_one(bContext *C, const std::string &path, Value &r_value, std::string &r_error)
{
  PointerRNA base;
  std::string rest;
  if (!resolve_root(C, path, base, rest, r_error)) {
    return false;
  }
  PointerRNA ptr;
  PropertyRNA *prop = nullptr;
  if (rest.empty() || !RNA_path_resolve_property(&base, rest.c_str(), &ptr, &prop)) {
    r_error = path + ": esta ruta RNA ya no existe";
    return false;
  }

  const PropertyType type = RNA_property_type(prop);
  const bool is_array = RNA_property_array_check(prop);

  if (is_array && type != PROP_COLLECTION) {
    const int len = RNA_property_array_length(&ptr, prop);
    r_value = Value();
    r_value.kind = ValueKind::Array;
    switch (type) {
      case PROP_BOOLEAN: {
        std::unique_ptr<bool[]> raw(new bool[static_cast<size_t>(len)]);
        RNA_property_boolean_get_array(&ptr, prop, raw.get());
        for (int i = 0; i < len; i++) {
          r_value.items.push_back(Value::make_bool(raw[size_t(i)]));
        }
        break;
      }
      case PROP_INT: {
        std::vector<int> raw(static_cast<size_t>(len));
        RNA_property_int_get_array(&ptr, prop, raw.data());
        for (int i = 0; i < len; i++) {
          r_value.items.push_back(Value::make_int(raw[size_t(i)]));
        }
        break;
      }
      case PROP_FLOAT: {
        std::vector<float> raw(static_cast<size_t>(len));
        RNA_property_float_get_array(&ptr, prop, raw.data());
        for (int i = 0; i < len; i++) {
          r_value.items.push_back(Value::make_float(double(raw[size_t(i)]), true));
        }
        break;
      }
      default:
        r_error = path + ": tipo de vector RNA no soportado";
        return false;
    }
    return true;
  }

  switch (type) {
    case PROP_BOOLEAN:
      r_value = Value::make_bool(RNA_property_boolean_get(&ptr, prop));
      return true;
    case PROP_INT:
      r_value = Value::make_int(RNA_property_int_get(&ptr, prop));
      return true;
    case PROP_FLOAT:
      r_value = Value::make_float(double(RNA_property_float_get(&ptr, prop)), true);
      return true;
    case PROP_STRING: {
      char *s = RNA_property_string_get_alloc(&ptr, prop, nullptr, 0, nullptr);
      r_value = Value::make_string(s ? s : "");
      if (s) {
        MEM_freeN(s);
      }
      return true;
    }
    case PROP_ENUM: {
      const int raw = RNA_property_enum_get(&ptr, prop);
      const bool is_flag = (RNA_property_flag(prop) & PROP_ENUM_FLAG) != 0;
      if (is_flag) {
        r_value = Value();
        r_value.kind = ValueKind::EnumSet;
        const EnumPropertyItem *items = nullptr;
        int totitem = 0;
        bool free_items = false;
        RNA_property_enum_items(C, &ptr, prop, &items, &totitem, &free_items);
        for (int i = 0; i < totitem; i++) {
          if (items[i].identifier && items[i].identifier[0] && (raw & items[i].value)) {
            r_value.items.push_back(Value::make_string(items[i].identifier));
          }
        }
        if (free_items && items) {
          MEM_freeN(const_cast<EnumPropertyItem *>(items));
        }
        return true;
      }
      const char *ident = nullptr;
      if (RNA_property_enum_identifier(C, &ptr, prop, raw, &ident) && ident) {
        r_value = Value::make_string(ident);
      }
      else {
        r_value = Value::make_int(raw);
      }
      return true;
    }
    case PROP_POINTER: {
      PointerRNA sub = RNA_property_pointer_get(&ptr, prop);
      if (sub.data == nullptr) {
        r_value = Value::make_none();
        return true;
      }
      /* Un puntero con datos no se guarda en un preset: el Python tampoco lo
       * hacia bien (escribia el `repr()` de un objeto, que no se puede releer). */
      r_error = path + ": apunta a un dato, no a un valor guardable";
      return false;
    }
    default:
      r_error = path + ": tipo de propiedad RNA no capturable";
      return false;
  }
}

bool capture(bContext *C,
             const std::vector<std::string> &paths,
             Preset &r_preset,
             std::string &r_error)
{
  for (const std::string &path : paths) {
    Op op;
    op.kind = OpKind::Set;
    op.path = path;
    if (!capture_one(C, path, op.value, r_error)) {
      return false;
    }
    r_preset.ops.push_back(op);
  }
  return true;
}

/** \} */

}  // namespace flipendo::preset
