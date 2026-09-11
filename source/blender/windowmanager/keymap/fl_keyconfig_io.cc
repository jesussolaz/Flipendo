/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Exportar e importar una configuracion de teclado como datos.
 * Ver FL_keyconfig_io.hpp.
 */

#include "FL_keyconfig_io.hpp"
#include "FL_keymap_dump.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <sstream>
#include <vector>

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_space_enums.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_listbase.h"
#include "BLI_fileops.h"
#include "BLI_string_utf8.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"

#include "BKE_appdir.hh"
#include "BKE_blender_version.h"
#include "BKE_context.hh"
#include "BKE_idprop.hh"
#include "BKE_report.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "WM_api.hh"
#include "WM_keymap.hh"
#include "WM_types.hh"

namespace flipendo::keyconfig {

/* -------------------------------------------------------------------------- */
/** \name Literales: el mismo repertorio que guarda un IDProperty de operador
 * \{ */

static std::string quote(const char *s)
{
  std::string out = "\"";
  for (const char *p = (s ? s : ""); *p; p++) {
    switch (*p) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out += *p;
    }
  }
  out += '"';
  return out;
}

/**
 * Flotante de precision simple: la representacion mas corta que vuelve a leerse
 * como el MISMO `float`. Es la misma regla que usa el formato `.fpreset`, y la
 * que usaba `repr_f32()` en `bl_keymap_utils/io.py`. Escribir `%.9g` a secas
 * llenaria el fichero de `0.20000000298023224`.
 */
static std::string format_float(float f)
{
  char buf[64];
  if (std::isnan(f)) {
    return "nan";
  }
  if (std::isinf(f)) {
    return f > 0 ? "inf" : "-inf";
  }
  for (int prec = 1; prec <= 9; prec++) {
    std::snprintf(buf, sizeof(buf), "%.*g", prec, double(f));
    if (float(std::atof(buf)) == f) {
      return buf;
    }
  }
  std::snprintf(buf, sizeof(buf), "%.9g", double(f));
  return buf;
}

static std::string format_double(double d)
{
  char buf[64];
  if (std::isnan(d)) {
    return "nan";
  }
  if (std::isinf(d)) {
    return d > 0 ? "inf" : "-inf";
  }
  for (int prec = 1; prec <= 17; prec++) {
    std::snprintf(buf, sizeof(buf), "%.*g", prec, d);
    if (std::atof(buf) == d) {
      return buf;
    }
  }
  std::snprintf(buf, sizeof(buf), "%.17g", d);
  return buf;
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name Serializar las propiedades de un atajo
 *
 * Las propiedades de un `wmKeyMapItem` son un `IDProperty` de tipo grupo. El
 * Python recorria el `OperatorProperties` de RNA y escribia las que estuvieran
 * puestas; en un IDProperty **estar puesta es estar en el grupo**, asi que se
 * recorre el grupo directamente. Es equivalente y ademas exacto: no pasa por
 * una conversion a texto de Python y de vuelta, que era donde `repr()` podia
 * perder precision.
 *
 * Los grupos anidados (las propiedades de un operador dentro de otro) se
 * escriben con la ruta con puntos: `prop "sub.valor" 3`.
 * \{ */

static void property_write(const IDProperty *prop,
                           const std::string &path,
                           std::string &out);

static std::string array_value(const IDProperty *prop)
{
  std::string out = "[";
  for (int i = 0; i < prop->len; i++) {
    if (i) {
      out += ", ";
    }
    switch (prop->subtype) {
      case IDP_INT:
        out += std::to_string(static_cast<const int *>(prop->data.pointer)[i]);
        break;
      case IDP_BOOLEAN:
        out += static_cast<const int8_t *>(prop->data.pointer)[i] ? "true" : "false";
        break;
      case IDP_FLOAT:
        out += format_float(static_cast<const float *>(prop->data.pointer)[i]);
        break;
      case IDP_DOUBLE:
        out += format_double(static_cast<const double *>(prop->data.pointer)[i]);
        break;
      default:
        out += "0";
        break;
    }
  }
  out += ']';
  return out;
}

static void property_write(const IDProperty *prop, const std::string &path, std::string &out)
{
  switch (prop->type) {
    case IDP_GROUP: {
      LISTBASE_FOREACH (const IDProperty *, sub, &prop->data.group) {
        property_write(sub, path.empty() ? std::string(sub->name) : path + "." + sub->name, out);
      }
      return;
    }
    case IDP_STRING:
      out += "prop " + quote(path.c_str()) + " " +
             quote(static_cast<const char *>(prop->data.pointer)) + "\n";
      return;
    case IDP_INT:
      out += "prop " + quote(path.c_str()) + " " + std::to_string(prop->data.val) + "\n";
      return;
    case IDP_BOOLEAN:
      out += "prop " + quote(path.c_str()) + " " + (prop->data.val ? "true" : "false") + "\n";
      return;
    case IDP_FLOAT: {
      float f;
      std::memcpy(&f, &prop->data.val, sizeof(float));
      out += "prop " + quote(path.c_str()) + " " + format_float(f) + "\n";
      return;
    }
    case IDP_DOUBLE: {
      double d;
      std::memcpy(&d, &prop->data.val, sizeof(double));
      out += "prop " + quote(path.c_str()) + " " + format_double(d) + "\n";
      return;
    }
    case IDP_ARRAY:
      out += "prop " + quote(path.c_str()) + " " + array_value(prop) + "\n";
      return;
    default:
      /* Ruidoso a proposito: perder una propiedad en silencio al exportar es
       * exactamente lo que este formato existe para evitar. */
      out += "# AVISO: propiedad '" + path + "' de tipo " + std::to_string(int(prop->type)) +
             " no representable, no se exporta\n";
      return;
  }
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name Escritura
 * \{ */

static const char *enum_id(const EnumPropertyItem *items, int value, const char *fallback)
{
  const char *id = nullptr;
  if (items != nullptr && RNA_enum_id_from_value(items, value, &id) && id != nullptr) {
    return id;
  }
  return fallback;
}

static void keymap_item_write(const wmKeyMap *km, const wmKeyMapItem *kmi, std::string &out)
{
  std::string id;
  if (km->flag & KEYMAP_MODAL) {
    const char *modal_id = nullptr;
    const EnumPropertyItem *items = static_cast<const EnumPropertyItem *>(km->modal_items);
    if (items != nullptr && RNA_enum_id_from_value(items, kmi->propvalue, &modal_id) &&
        modal_id != nullptr)
    {
      id = modal_id;
    }
    else {
      id = std::to_string(kmi->propvalue);
    }
  }
  else {
    id = kmi->idname;
  }

  out += "item " + quote(id.c_str());
  out += std::string(" type=") + enum_id(rna_enum_event_type_items, kmi->type, "NONE");
  out += std::string(" value=") + enum_id(rna_enum_event_value_items, kmi->val, "NOTHING");

  /* Mismo criterio que `kmi_args_as_data()`: o `any`, o los modificadores uno a
   * uno, y solo los que esten puestos. */
  const bool any = (kmi->shift == KM_ANY) && (kmi->ctrl == KM_ANY) && (kmi->alt == KM_ANY) &&
                   (kmi->oskey == KM_ANY) && (kmi->hyper == KM_ANY);
  if (any) {
    out += " any=1";
  }
  else {
    const struct {
      const char *name;
      int8_t value;
    } mods[] = {{"shift", kmi->shift},
                {"ctrl", kmi->ctrl},
                {"alt", kmi->alt},
                {"oskey", kmi->oskey},
                {"hyper", kmi->hyper}};
    for (const auto &mod : mods) {
      if (mod.value != 0) {
        out += std::string(" ") + mod.name + "=" + (mod.value == KM_ANY ? "-1" : "1");
      }
    }
  }
  if (kmi->keymodifier != 0) {
    out += std::string(" key_modifier=") +
           enum_id(rna_enum_event_type_items, kmi->keymodifier, "NONE");
  }
  if (kmi->direction != KM_ANY) {
    out += std::string(" direction=") +
           enum_id(rna_enum_event_direction_items, kmi->direction, "ANY");
  }
  if ((kmi->flag & KMI_REPEAT_IGNORE) == 0) {
    out += " repeat=1";
  }
  if (kmi->flag & KMI_INACTIVE) {
    out += " active=0";
  }
  out += "\n";

  if (kmi->properties != nullptr) {
    property_write(kmi->properties, "", out);
  }
}

/** Los keymaps a exportar, con el mismo criterio y el mismo orden que el Python. */
static std::vector<wmKeyMap *> export_keymaps_gather(wmWindowManager *wm,
                                                     wmKeyConfig *kc,
                                                     bool all_keymaps)
{
  std::vector<wmKeyMap *> out;
  /* Primero los del usuario que esten modificados (o todos, si se pide). */
  LISTBASE_FOREACH (wmKeyMap *, km, &wm->userconf->keymaps) {
    if (all_keymaps || (km->flag & KEYMAP_USER_MODIFIED)) {
      out.push_back(km);
    }
  }
  /* Y luego los de `kc` cuyo nombre no estuviera ya: es el `keyconfig_merge()`
   * del Python, que da prioridad a los editados. */
  if (kc != nullptr && kc != wm->defaultconf) {
    LISTBASE_FOREACH (wmKeyMap *, km, &kc->keymaps) {
      const bool seen = std::any_of(out.begin(), out.end(), [km](const wmKeyMap *other) {
        return STREQ(other->idname, km->idname);
      });
      if (!seen) {
        out.push_back(km);
      }
    }
  }
  std::sort(out.begin(), out.end(), [](const wmKeyMap *a, const wmKeyMap *b) {
    return std::strcmp(a->idname, b->idname) < 0;
  });
  return out;
}

bool export_to_file(wmWindowManager *wm,
                    wmKeyConfig *kc,
                    const std::string &filepath,
                    const bool all_keymaps,
                    std::string &r_error)
{
  if (wm == nullptr) {
    r_error = "No hay gestor de ventanas";
    return false;
  }

  std::string out;
  out += "# Configuracion de teclado de Flipendo, como DATOS.\n";
  out += "# La escribe y la lee el C++; no se ejecuta nada. Ver\n";
  out += "# politicas/DATOS-SIN-INTERPRETE.md.\n";
  out += "fkeyconfig " + std::to_string(FORMAT_VERSION) + "\n";
  out += "version " + std::to_string(BLENDER_VERSION / 100) + " " +
         std::to_string(BLENDER_VERSION % 100) + " " + std::to_string(BLENDER_FILE_SUBVERSION) +
         "\n";

  int n_keymaps = 0, n_items = 0;
  for (wmKeyMap *km_src : export_keymaps_gather(wm, kc, all_keymaps)) {
    /* `km.active()`: el keymap de verdad, no el del usuario sin resolver. */
    wmKeyMap *km = WM_keymap_active(wm, km_src);
    if (km == nullptr) {
      km = km_src;
    }
    n_keymaps++;
    out += "keymap " + quote(km->idname);
    out += std::string(" space=") + enum_id(rna_enum_space_type_items, km->spaceid, "EMPTY");
    out += std::string(" region=") + enum_id(rna_enum_region_type_items, km->regionid, "WINDOW");
    if (km->flag & KEYMAP_MODAL) {
      out += " modal=1";
    }
    out += "\n";
    LISTBASE_FOREACH (wmKeyMapItem *, kmi, &km->items) {
      n_items++;
      keymap_item_write(km, kmi, out);
    }
  }

  std::ofstream fh(filepath, std::ios::binary);
  if (!fh.is_open()) {
    r_error = "No puedo escribir '" + filepath + "'";
    return false;
  }
  fh << out;
  fh.close();
  printf("keyconfig: %d keymaps y %d atajos escritos en '%s'\n",
         n_keymaps,
         n_items,
         filepath.c_str());
  return true;
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name Lectura del formato de datos
 * \{ */

namespace {

/** Analizador de una linea: campos sueltos, `clave=valor` y literales. */
struct LineScanner {
  const std::string &text;
  size_t pos = 0;
  std::string error;

  explicit LineScanner(const std::string &t) : text(t) {}

  void skip_space()
  {
    while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\t')) {
      pos++;
    }
  }

  bool at_end()
  {
    skip_space();
    return pos >= text.size();
  }

  bool parse_quoted(std::string &r_out)
  {
    skip_space();
    if (pos >= text.size() || text[pos] != '"') {
      error = "se esperaba una cadena entre comillas";
      return false;
    }
    pos++;
    r_out.clear();
    while (pos < text.size()) {
      const char c = text[pos++];
      if (c == '"') {
        return true;
      }
      if (c == '\\') {
        if (pos >= text.size()) {
          break;
        }
        const char e = text[pos++];
        switch (e) {
          case 'n':
            r_out += '\n';
            break;
          case 'r':
            r_out += '\r';
            break;
          case 't':
            r_out += '\t';
            break;
          case '\\':
            r_out += '\\';
            break;
          case '"':
            r_out += '"';
            break;
          default:
            r_out += e;
            break;
        }
        continue;
      }
      r_out += c;
    }
    error = "cadena sin cerrar";
    return false;
  }

  /** `clave=valor` sin comillas. Devuelve false al acabar la linea. */
  bool next_pair(std::string &r_key, std::string &r_value)
  {
    skip_space();
    if (pos >= text.size()) {
      return false;
    }
    const size_t start = pos;
    while (pos < text.size() && text[pos] != '=' && text[pos] != ' ' && text[pos] != '\t') {
      pos++;
    }
    r_key = text.substr(start, pos - start);
    if (pos < text.size() && text[pos] == '=') {
      pos++;
      const size_t vstart = pos;
      while (pos < text.size() && text[pos] != ' ' && text[pos] != '\t') {
        pos++;
      }
      r_value = text.substr(vstart, pos - vstart);
    }
    else {
      r_value.clear();
    }
    return !r_key.empty();
  }
};

/** Valor de una propiedad ya analizado, listo para volverse un IDProperty. */
struct PropValue {
  enum class Kind { Bool, Int, Float, String, Array } kind = Kind::Int;
  bool b = false;
  int i = 0;
  double f = 0.0;
  std::string s;
  std::vector<PropValue> items;
};

bool parse_prop_value(LineScanner &sc, PropValue &r_value)
{
  sc.skip_space();
  if (sc.pos >= sc.text.size()) {
    sc.error = "falta el valor";
    return false;
  }
  const char c = sc.text[sc.pos];
  if (c == '"') {
    r_value.kind = PropValue::Kind::String;
    return sc.parse_quoted(r_value.s);
  }
  if (c == '[') {
    sc.pos++;
    r_value.kind = PropValue::Kind::Array;
    while (true) {
      sc.skip_space();
      if (sc.pos < sc.text.size() && sc.text[sc.pos] == ']') {
        sc.pos++;
        return true;
      }
      PropValue item;
      if (!parse_prop_value(sc, item)) {
        return false;
      }
      r_value.items.push_back(item);
      sc.skip_space();
      if (sc.pos < sc.text.size() && sc.text[sc.pos] == ',') {
        sc.pos++;
        continue;
      }
      if (sc.pos < sc.text.size() && sc.text[sc.pos] == ']') {
        sc.pos++;
        return true;
      }
      sc.error = "lista sin cerrar";
      return false;
    }
  }
  const size_t start = sc.pos;
  while (sc.pos < sc.text.size() && sc.text[sc.pos] != ' ' && sc.text[sc.pos] != '\t' &&
         sc.text[sc.pos] != ',' && sc.text[sc.pos] != ']')
  {
    sc.pos++;
  }
  const std::string tok = sc.text.substr(start, sc.pos - start);
  if (tok == "true" || tok == "false") {
    r_value.kind = PropValue::Kind::Bool;
    r_value.b = (tok == "true");
    return true;
  }
  if (tok.empty()) {
    sc.error = "falta el valor";
    return false;
  }
  if (tok.find_first_of(".eEni") != std::string::npos) {
    r_value.kind = PropValue::Kind::Float;
    r_value.f = std::atof(tok.c_str());
    return true;
  }
  r_value.kind = PropValue::Kind::Int;
  r_value.i = std::atoi(tok.c_str());
  return true;
}

IDProperty *prop_group_ensure(IDProperty *group, const std::string &name)
{
  IDProperty *sub = IDP_GetPropertyFromGroup(group, name.c_str());
  if (sub != nullptr && sub->type == IDP_GROUP) {
    return sub;
  }
  IDPropertyTemplate val = {0};
  IDProperty *created = IDP_New(IDP_GROUP, &val, name);
  IDP_ReplaceInGroup(group, created);
  return created;
}

void prop_set(IDProperty *group, const std::string &name, const PropValue &value)
{
  IDPropertyTemplate val = {0};
  IDProperty *prop = nullptr;
  switch (value.kind) {
    case PropValue::Kind::Bool:
      val.i = value.b ? 1 : 0;
      prop = IDP_New(IDP_BOOLEAN, &val, name);
      break;
    case PropValue::Kind::Int:
      val.i = value.i;
      prop = IDP_New(IDP_INT, &val, name);
      break;
    case PropValue::Kind::Float:
      val.f = float(value.f);
      prop = IDP_New(IDP_FLOAT, &val, name);
      break;
    case PropValue::Kind::String:
      prop = IDP_NewString(value.s.c_str(), name);
      break;
    case PropValue::Kind::Array: {
      const bool all_int = std::all_of(
          value.items.begin(), value.items.end(), [](const PropValue &v) {
            return v.kind == PropValue::Kind::Int;
          });
      const bool all_bool = std::all_of(
          value.items.begin(), value.items.end(), [](const PropValue &v) {
            return v.kind == PropValue::Kind::Bool;
          });
      val.array.len = int(value.items.size());
      val.array.type = all_bool ? IDP_BOOLEAN : (all_int ? IDP_INT : IDP_FLOAT);
      prop = IDP_New(IDP_ARRAY, &val, name);
      for (size_t i = 0; i < value.items.size(); i++) {
        if (all_bool) {
          static_cast<int8_t *>(prop->data.pointer)[i] = value.items[i].b ? 1 : 0;
        }
        else if (all_int) {
          static_cast<int *>(prop->data.pointer)[i] = value.items[i].i;
        }
        else {
          const PropValue &item = value.items[i];
          static_cast<float *>(prop->data.pointer)[i] =
              float(item.kind == PropValue::Kind::Int ? double(item.i) : item.f);
        }
      }
      break;
    }
  }
  if (prop != nullptr) {
    IDP_ReplaceInGroup(group, prop);
  }
}

}  // namespace

bool read_fkeyconfig_text(const std::string &text,
                          const char *origin,
                          wmKeyConfig *kc,
                          std::string &r_error)
{
  std::istringstream stream(text);
  std::string line;
  int line_no = 0;
  bool header = false;
  wmKeyMap *km = nullptr;
  wmKeyMapItem *kmi = nullptr;
  bool km_modal = false;

  const auto fail = [&](const std::string &why) {
    r_error = std::string(origin) + ":" + std::to_string(line_no) + ": " + why;
    return false;
  };

  while (std::getline(stream, line)) {
    line_no++;
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    size_t first = line.find_first_not_of(" \t");
    if (first == std::string::npos || line[first] == '#') {
      continue;
    }
    line = line.substr(first);

    if (!header) {
      int version = 0;
      if (std::sscanf(line.c_str(), "fkeyconfig %d", &version) != 1) {
        return fail("falta la cabecera 'fkeyconfig <version>'");
      }
      if (version != FORMAT_VERSION) {
        return fail("formato version " + std::to_string(version) + ", esta build entiende la " +
                    std::to_string(FORMAT_VERSION));
      }
      header = true;
      continue;
    }
    if (line.compare(0, 8, "version ") == 0) {
      continue;
    }

    if (line.compare(0, 7, "keymap ") == 0) {
      LineScanner sc(line);
      sc.pos = 7;
      std::string name;
      if (!sc.parse_quoted(name)) {
        return fail(sc.error);
      }
      int spaceid = SPACE_EMPTY, regionid = RGN_TYPE_WINDOW;
      km_modal = false;
      std::string key, value;
      while (sc.next_pair(key, value)) {
        if (key == "space") {
          RNA_enum_value_from_id(rna_enum_space_type_items, value.c_str(), &spaceid);
        }
        else if (key == "region") {
          RNA_enum_value_from_id(rna_enum_region_type_items, value.c_str(), &regionid);
        }
        else if (key == "modal") {
          km_modal = (value == "1");
        }
      }
      km = WM_keymap_ensure(kc, name.c_str(), spaceid, regionid);
      if (km == nullptr) {
        return fail("no pude crear el keymap '" + name + "'");
      }
      if (km_modal) {
        km->flag |= KEYMAP_MODAL;
      }
      kmi = nullptr;
      continue;
    }

    if (line.compare(0, 5, "item ") == 0) {
      if (km == nullptr) {
        return fail("'item' fuera de un 'keymap'");
      }
      LineScanner sc(line);
      sc.pos = 5;
      std::string id;
      if (!sc.parse_quoted(id)) {
        return fail(sc.error);
      }
      KeyMapItem_Params params = {};
      params.type = 0;
      params.value = KM_NOTHING;
      params.modifier = 0;
      params.keymodifier = 0;
      params.direction = KM_ANY;
      bool active = true;
      bool repeat = false;
      std::string key, value;
      while (sc.next_pair(key, value)) {
        int enum_value = 0;
        if (key == "type") {
          if (RNA_enum_value_from_id(rna_enum_event_type_items, value.c_str(), &enum_value)) {
            params.type = int16_t(enum_value);
          }
        }
        else if (key == "value") {
          if (RNA_enum_value_from_id(rna_enum_event_value_items, value.c_str(), &enum_value)) {
            params.value = int8_t(enum_value);
          }
        }
        else if (key == "key_modifier") {
          if (RNA_enum_value_from_id(rna_enum_event_type_items, value.c_str(), &enum_value)) {
            params.keymodifier = int16_t(enum_value);
          }
        }
        else if (key == "direction") {
          if (RNA_enum_value_from_id(rna_enum_event_direction_items, value.c_str(), &enum_value)) {
            params.direction = int8_t(enum_value);
          }
        }
        else if (key == "any") {
          params.modifier = KM_ANY;
        }
        else if (key == "active") {
          active = (value != "0");
        }
        else if (key == "repeat") {
          repeat = (value == "1");
        }
        else {
          const int mod = (key == "shift") ? KM_SHIFT :
                          (key == "ctrl")  ? KM_CTRL :
                          (key == "alt")   ? KM_ALT :
                          (key == "oskey") ? KM_OSKEY :
                          (key == "hyper") ? KM_HYPER :
                                             0;
          if (mod != 0 && params.modifier != KM_ANY) {
            params.modifier |= (value == "-1") ? KMI_PARAMS_MOD_TO_ANY(mod) : mod;
          }
        }
      }

      kmi = km_modal ? WM_modalkeymap_add_item_str(km, &params, id.c_str()) :
                       WM_keymap_add_item(km, id.c_str(), &params);
      if (kmi == nullptr) {
        return fail("no pude crear el atajo '" + id + "'");
      }
      if (!active) {
        kmi->flag |= KMI_INACTIVE;
      }
      if (!repeat) {
        kmi->flag |= KMI_REPEAT_IGNORE;
      }
      continue;
    }

    if (line.compare(0, 5, "prop ") == 0) {
      if (kmi == nullptr) {
        return fail("'prop' fuera de un 'item'");
      }
      LineScanner sc(line);
      sc.pos = 5;
      std::string path;
      if (!sc.parse_quoted(path)) {
        return fail(sc.error);
      }
      PropValue value;
      if (!parse_prop_value(sc, value)) {
        return fail(sc.error);
      }
      if (kmi->properties == nullptr) {
        IDPropertyTemplate val = {0};
        kmi->properties = IDP_New(IDP_GROUP, &val, "wmOperatorProperties");
      }
      IDProperty *group = kmi->properties;
      size_t dot;
      std::string rest = path;
      while ((dot = rest.find('.')) != std::string::npos) {
        group = prop_group_ensure(group, rest.substr(0, dot));
        rest = rest.substr(dot + 1);
      }
      prop_set(group, rest, value);
      continue;
    }

    return fail("linea que no entiendo: '" + line.substr(0, 40) + "'");
  }

  if (!header) {
    r_error = std::string(origin) + ": fichero vacio o sin cabecera";
    return false;
  }
  return true;
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name Lectura del `.py` heredado — compatibilidad LEYENDO, nunca escribiendo
 * \{ */

bool read_legacy_python_text(const std::string &text,
                             const char *origin,
                             wmKeyConfig *kc,
                             std::string &r_error)
{
  /* El fichero que escribia `keyconfig_export_as_data()` es una lista literal.
   * Se analiza como tal, sin ejecutar: se buscan los tres niveles que el
   * escritor generaba y se rechaza cualquier otra cosa con linea y motivo. */
  const size_t start = text.find("keyconfig_data");
  if (start == std::string::npos) {
    r_error = std::string(origin) + ": no encuentro 'keyconfig_data'; esto no es una "
                                    "configuracion de teclado exportada por Blender";
    return false;
  }

  /* Analizador minimo de literales de Python sobre el subconjunto exacto del
   * escritor: listas, tuplas, diccionarios, cadenas, numeros, True/False/None. */
  size_t i = text.find('[', start);
  if (i == std::string::npos) {
    r_error = std::string(origin) + ": 'keyconfig_data' no abre una lista";
    return false;
  }

  const auto line_of = [&text](size_t at) {
    size_t n = 1;
    for (size_t k = 0; k < at && k < text.size(); k++) {
      if (text[k] == '\n') {
        n++;
      }
    }
    return n;
  };
  const auto fail = [&](size_t at, const std::string &why) {
    const size_t from = (at > 40) ? at - 40 : 0;
    r_error = std::string(origin) + ":" + std::to_string(line_of(at)) + ": " + why + " [..." +
              text.substr(from, std::min<size_t>(80, text.size() - from)) + "...]";
    return false;
  };

  const auto skip_ws = [&text](size_t &p) {
    while (p < text.size()) {
      if (text[p] == '#') {
        while (p < text.size() && text[p] != '\n') {
          p++;
        }
        continue;
      }
      if (text[p] == '\\' && p + 1 < text.size() && text[p + 1] == '\n') {
        p += 2;
        continue;
      }
      if (std::isspace(static_cast<unsigned char>(text[p]))) {
        p++;
        continue;
      }
      break;
    }
  };

  const auto parse_py_string = [&](size_t &p, std::string &r_out) -> bool {
    skip_ws(p);
    if (p >= text.size() || (text[p] != '"' && text[p] != '\'')) {
      return false;
    }
    const char q = text[p++];
    r_out.clear();
    while (p < text.size() && text[p] != q) {
      if (text[p] == '\\' && p + 1 < text.size()) {
        p++;
        const char e = text[p++];
        switch (e) {
          case 'n':
            r_out += '\n';
            break;
          case 't':
            r_out += '\t';
            break;
          case 'r':
            r_out += '\r';
            break;
          default:
            r_out += e;
            break;
        }
        continue;
      }
      r_out += text[p++];
    }
    if (p >= text.size()) {
      return false;
    }
    p++;
    return true;
  };

  /* Lee un literal cualquiera y lo devuelve como PropValue, saltandose lo que no
   * sea representable pero SIN ignorarlo en silencio. */
  std::function<bool(size_t &, PropValue &)> parse_py_value =
      [&](size_t &p, PropValue &r_value) -> bool {
    skip_ws(p);
    if (p >= text.size()) {
      return false;
    }
    const char c = text[p];
    if (c == '"' || c == '\'') {
      r_value.kind = PropValue::Kind::String;
      return parse_py_string(p, r_value.s);
    }
    if (c == '(' || c == '[' || c == '{') {
      const char close = (c == '(') ? ')' : ((c == '[') ? ']' : '}');
      p++;
      r_value.kind = PropValue::Kind::Array;
      while (true) {
        skip_ws(p);
        if (p < text.size() && text[p] == close) {
          p++;
          return true;
        }
        PropValue item;
        if (!parse_py_value(p, item)) {
          return false;
        }
        r_value.items.push_back(item);
        skip_ws(p);
        if (p < text.size() && text[p] == ',') {
          p++;
          continue;
        }
        if (p < text.size() && text[p] == close) {
          p++;
          return true;
        }
        return false;
      }
    }
    const size_t s = p;
    while (p < text.size() && (std::isalnum(static_cast<unsigned char>(text[p])) ||
                               text[p] == '.' || text[p] == '-' || text[p] == '+' ||
                               text[p] == '_'))
    {
      p++;
    }
    const std::string tok = text.substr(s, p - s);
    if (tok.empty()) {
      return false;
    }
    if (tok == "True" || tok == "False") {
      r_value.kind = PropValue::Kind::Bool;
      r_value.b = (tok == "True");
      return true;
    }
    if (tok == "None") {
      r_value.kind = PropValue::Kind::Int;
      r_value.i = 0;
      return true;
    }
    if (tok.find_first_of(".eE") != std::string::npos) {
      r_value.kind = PropValue::Kind::Float;
      r_value.f = std::atof(tok.c_str());
      return true;
    }
    r_value.kind = PropValue::Kind::Int;
    r_value.i = std::atoi(tok.c_str());
    return true;
  };

  /* Ahora la estructura: [ ("nombre", {args}, {"items": [ ... ]}), ... ] */
  i++; /* Tras el `[` de `keyconfig_data`. */
  int n_keymaps = 0, n_items = 0;
  while (true) {
    skip_ws(i);
    if (i >= text.size()) {
      return fail(i, "la lista de keymaps no cierra");
    }
    if (text[i] == ']') {
      break;
    }
    if (text[i] != '(') {
      return fail(i, "se esperaba una tupla de keymap");
    }
    i++;
    std::string km_name;
    if (!parse_py_string(i, km_name)) {
      return fail(i, "el keymap no empieza por su nombre");
    }
    skip_ws(i);
    if (i < text.size() && text[i] == ',') {
      i++;
    }

    /* Diccionario de argumentos del keymap. */
    int spaceid = SPACE_EMPTY, regionid = RGN_TYPE_WINDOW;
    bool modal = false;
    skip_ws(i);
    if (i >= text.size() || text[i] != '{') {
      return fail(i, "faltan los argumentos del keymap");
    }
    i++;
    while (true) {
      skip_ws(i);
      if (i < text.size() && text[i] == '}') {
        i++;
        break;
      }
      std::string key;
      if (!parse_py_string(i, key)) {
        return fail(i, "clave de argumento no valida");
      }
      skip_ws(i);
      if (i < text.size() && text[i] == ':') {
        i++;
      }
      PropValue value;
      if (!parse_py_value(i, value)) {
        return fail(i, "valor de argumento no valido");
      }
      if (key == "space_type" && value.kind == PropValue::Kind::String) {
        RNA_enum_value_from_id(rna_enum_space_type_items, value.s.c_str(), &spaceid);
      }
      else if (key == "region_type" && value.kind == PropValue::Kind::String) {
        RNA_enum_value_from_id(rna_enum_region_type_items, value.s.c_str(), &regionid);
      }
      else if (key == "modal") {
        modal = value.b;
      }
      skip_ws(i);
      if (i < text.size() && text[i] == ',') {
        i++;
      }
    }
    skip_ws(i);
    if (i < text.size() && text[i] == ',') {
      i++;
    }

    wmKeyMap *km = WM_keymap_ensure(kc, km_name.c_str(), spaceid, regionid);
    if (km == nullptr) {
      return fail(i, "no pude crear el keymap '" + km_name + "'");
    }
    if (modal) {
      km->flag |= KEYMAP_MODAL;
    }
    n_keymaps++;

    /* Diccionario de contenido: {"items": [ ... ]}. */
    skip_ws(i);
    if (i >= text.size() || text[i] != '{') {
      return fail(i, "falta el contenido del keymap");
    }
    i++;
    std::string content_key;
    if (!parse_py_string(i, content_key) || content_key != "items") {
      return fail(i, "el contenido del keymap no es 'items'");
    }
    skip_ws(i);
    if (i < text.size() && text[i] == ':') {
      i++;
    }
    skip_ws(i);
    if (i >= text.size() || text[i] != '[') {
      return fail(i, "'items' no abre una lista");
    }
    i++;

    while (true) {
      skip_ws(i);
      if (i >= text.size()) {
        return fail(i, "la lista de atajos no cierra");
      }
      if (text[i] == ']') {
        i++;
        break;
      }
      if (text[i] != '(') {
        return fail(i, "se esperaba una tupla de atajo");
      }
      i++;
      std::string kmi_id;
      if (!parse_py_string(i, kmi_id)) {
        return fail(i, "el atajo no empieza por su identificador");
      }
      skip_ws(i);
      if (i < text.size() && text[i] == ',') {
        i++;
      }

      /* Diccionario de evento. */
      KeyMapItem_Params params = {};
      params.value = KM_NOTHING;
      params.direction = KM_ANY;
      bool repeat = false;
      skip_ws(i);
      if (i >= text.size() || text[i] != '{') {
        return fail(i, "faltan los argumentos del atajo");
      }
      i++;
      while (true) {
        skip_ws(i);
        if (i < text.size() && text[i] == '}') {
          i++;
          break;
        }
        std::string key;
        if (!parse_py_string(i, key)) {
          return fail(i, "clave de evento no valida");
        }
        skip_ws(i);
        if (i < text.size() && text[i] == ':') {
          i++;
        }
        PropValue value;
        if (!parse_py_value(i, value)) {
          return fail(i, "valor de evento no valido");
        }
        int enum_value = 0;
        if (key == "type" && RNA_enum_value_from_id(
                                 rna_enum_event_type_items, value.s.c_str(), &enum_value))
        {
          params.type = int16_t(enum_value);
        }
        else if (key == "value" && RNA_enum_value_from_id(rna_enum_event_value_items,
                                                          value.s.c_str(),
                                                          &enum_value))
        {
          params.value = int8_t(enum_value);
        }
        else if (key == "key_modifier" && RNA_enum_value_from_id(rna_enum_event_type_items,
                                                                 value.s.c_str(),
                                                                 &enum_value))
        {
          params.keymodifier = int16_t(enum_value);
        }
        else if (key == "direction" && RNA_enum_value_from_id(rna_enum_event_direction_items,
                                                              value.s.c_str(),
                                                              &enum_value))
        {
          params.direction = int8_t(enum_value);
        }
        else if (key == "any") {
          params.modifier = KM_ANY;
        }
        else if (key == "repeat") {
          repeat = value.b;
        }
        else {
          const int mod = (key == "shift") ? KM_SHIFT :
                          (key == "ctrl")  ? KM_CTRL :
                          (key == "alt")   ? KM_ALT :
                          (key == "oskey") ? KM_OSKEY :
                          (key == "hyper") ? KM_HYPER :
                                             0;
          const bool is_any = (value.kind == PropValue::Kind::Int && value.i == -1);
          if (mod != 0 && params.modifier != KM_ANY) {
            params.modifier |= is_any ? KMI_PARAMS_MOD_TO_ANY(mod) : mod;
          }
        }
        skip_ws(i);
        if (i < text.size() && text[i] == ',') {
          i++;
        }
      }
      skip_ws(i);
      if (i < text.size() && text[i] == ',') {
        i++;
      }

      wmKeyMapItem *kmi = modal ? WM_modalkeymap_add_item_str(km, &params, kmi_id.c_str()) :
                                  WM_keymap_add_item(km, kmi_id.c_str(), &params);
      if (kmi == nullptr) {
        return fail(i, "no pude crear el atajo '" + kmi_id + "'");
      }
      if (!repeat) {
        kmi->flag |= KMI_REPEAT_IGNORE;
      }
      n_items++;

      /* Tercer elemento: `None` o `{"properties": [...], "active": False}`. */
      skip_ws(i);
      if (i < text.size() && text[i] == 'N') {
        PropValue ignored;
        parse_py_value(i, ignored);
      }
      else if (i < text.size() && text[i] == '{') {
        i++;
        while (true) {
          skip_ws(i);
          if (i < text.size() && text[i] == '}') {
            i++;
            break;
          }
          std::string key;
          if (!parse_py_string(i, key)) {
            return fail(i, "clave de datos de atajo no valida");
          }
          skip_ws(i);
          if (i < text.size() && text[i] == ':') {
            i++;
          }
          PropValue value;
          if (!parse_py_value(i, value)) {
            return fail(i, "valor de datos de atajo no valido");
          }
          if (key == "active" && value.kind == PropValue::Kind::Bool && !value.b) {
            kmi->flag |= KMI_INACTIVE;
          }
          else if (key == "properties") {
            if (kmi->properties == nullptr) {
              IDPropertyTemplate tmpl = {0};
              kmi->properties = IDP_New(IDP_GROUP, &tmpl, "wmOperatorProperties");
            }
            /* Lista de pares `("nombre", valor)`; un valor de tipo lista es un
             * grupo anidado. */
            std::function<void(IDProperty *, const PropValue &)> apply =
                [&](IDProperty *group, const PropValue &list) {
                  for (const PropValue &pair : list.items) {
                    if (pair.kind != PropValue::Kind::Array || pair.items.size() != 2) {
                      continue;
                    }
                    const PropValue &name = pair.items[0];
                    const PropValue &val = pair.items[1];
                    if (name.kind != PropValue::Kind::String) {
                      continue;
                    }
                    if (val.kind == PropValue::Kind::Array && !val.items.empty() &&
                        val.items[0].kind == PropValue::Kind::Array)
                    {
                      apply(prop_group_ensure(group, name.s), val);
                    }
                    else {
                      prop_set(group, name.s, val);
                    }
                  }
                };
            apply(kmi->properties, value);
          }
          skip_ws(i);
          if (i < text.size() && text[i] == ',') {
            i++;
          }
        }
      }
      /* TRAMPA, y costo encontrarla: el escritor de Python pone la coma ANTES
       * del parentesis que cierra el atajo, no despues:
       *
       *        {"properties":
       *         [...],
       *         },            <- coma del tercer elemento
       *        ),             <- cierre del atajo
       *
       * Esperar `)` y luego `,` deja el cursor en la coma, el bucle vuelve a
       * empezar y se encuentra un `)` donde esperaba un `(`. El sintoma era
       * «linea 14: se esperaba una tupla de atajo» y el fallo estaba dos
       * caracteres antes. Se consume coma opcional, cierre, y coma opcional. */
      skip_ws(i);
      if (i < text.size() && text[i] == ',') {
        i++;
      }
      skip_ws(i);
      if (i < text.size() && text[i] == ')') {
        i++;
      }
      skip_ws(i);
      if (i < text.size() && text[i] == ',') {
        i++;
      }
    }

    /* Y lo mismo al cerrar el keymap: `],` `},` `),`, con la coma delante de
     * cada cierre. */
    skip_ws(i);
    if (i < text.size() && text[i] == ',') {
      i++;
    }
    skip_ws(i);
    if (i < text.size() && text[i] == '}') {
      i++;
    }
    skip_ws(i);
    if (i < text.size() && text[i] == ',') {
      i++;
    }
    skip_ws(i);
    if (i < text.size() && text[i] == ')') {
      i++;
    }
    skip_ws(i);
    if (i < text.size() && text[i] == ',') {
      i++;
    }
  }

  printf("keyconfig: %d keymaps y %d atajos leidos del .py heredado\n", n_keymaps, n_items);
  return true;
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name Importar
 * \{ */

static bool file_read_all(const std::string &filepath, std::string &r_text)
{
  std::ifstream fh(filepath, std::ios::binary);
  if (!fh.is_open()) {
    return false;
  }
  std::ostringstream ss;
  ss << fh.rdbuf();
  r_text = ss.str();
  return true;
}

bool import_from_file(bContext *C,
                      const std::string &filepath,
                      const std::string &name,
                      std::string &r_error)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  if (wm == nullptr) {
    r_error = "No hay gestor de ventanas";
    return false;
  }
  std::string text;
  if (!file_read_all(filepath, text)) {
    r_error = "No puedo leer '" + filepath + "'";
    return false;
  }

  wmKeyConfig *kc = WM_keyconfig_new(wm, name.c_str(), true);
  if (kc == nullptr) {
    r_error = "No pude crear la configuracion '" + name + "'";
    return false;
  }

  const bool is_py = (filepath.size() > 3) && (filepath.compare(filepath.size() - 3, 3, ".py") == 0);
  const bool ok = is_py ? read_legacy_python_text(text, filepath.c_str(), kc, r_error) :
                    read_fkeyconfig_text(text, filepath.c_str(), kc, r_error);
  /* Aqui hubo un ultimo recurso al interprete mientras el analizador del `.py`
   * heredado no estaba terminado. Ya no hace falta: un export completo de keymap
   * en formato antiguo se lee de forma nativa y da el mismo resultado byte a
   * byte (248 keymaps, 3.673 atajos, 6.680 lineas, 0 diferencias). Un fichero
   * que el analizador rechace se dice con fichero, linea y motivo, y no se
   * aplica a medias. */
  if (!ok) {
    WM_keyconfig_remove(wm, kc);
    return false;
  }
  WM_keyconfig_set_active(wm, name.c_str());
  return true;
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name Verificacion del ciclo completo
 *
 * Exportar y volver a importar tiene que dar la MISMA configuracion. Se
 * comprueba con el volcado determinista que ya usa `--fl-dump-keymap`: si el
 * ciclo pierde un atajo, una propiedad o un modificador, el volcado lo canta.
 * \{ */

bool check_roundtrip(bContext *C, const char *report_path, const char *legacy_py)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  if (wm == nullptr) {
    fprintf(stderr, "keyconfig: no hay gestor de ventanas\n");
    return false;
  }

  char dir[FILE_MAX];
  BLI_path_split_dir_part(report_path, dir, sizeof(dir));

  char path_data[FILE_MAX];
  BLI_path_join(path_data, sizeof(path_data), dir, "roundtrip.fkeyconfig");
  char dump_before[FILE_MAX], dump_after[FILE_MAX];
  BLI_path_join(dump_before, sizeof(dump_before), dir, "roundtrip-antes.txt");
  BLI_path_join(dump_after, sizeof(dump_after), dir, "roundtrip-despues.txt");

  std::string error;
  wmKeyConfig *kc = wm->defaultconf;
  if (!export_to_file(wm, kc, path_data, true, error)) {
    fprintf(stderr, "keyconfig: %s\n", error.c_str());
    return false;
  }

  /* La configuracion de partida, volcada. */
  wmKeyConfig *kc_probe = WM_keyconfig_new(wm, "FlipendoRoundtripBefore", true);
  {
    std::string text;
    if (!file_read_all(path_data, text)) {
      fprintf(stderr, "keyconfig: no puedo releer '%s'\n", path_data);
      return false;
    }
    if (!read_fkeyconfig_text(text, path_data, kc_probe, error)) {
      fprintf(stderr, "keyconfig: %s\n", error.c_str());
      return false;
    }
  }

  /* Y el resultado de exportar ESA y volver a importarla. */
  char path_again[FILE_MAX];
  BLI_path_join(path_again, sizeof(path_again), dir, "roundtrip2.fkeyconfig");
  if (!export_to_file(wm, kc_probe, path_again, true, error)) {
    fprintf(stderr, "keyconfig: %s\n", error.c_str());
    return false;
  }

  /* Los dos ficheros de datos tienen que ser identicos byte a byte: el primero
   * sale de la configuracion por defecto, el segundo de la que se construyo
   * leyendo el primero. */
  std::string a, b;
  if (!file_read_all(path_data, a) || !file_read_all(path_again, b)) {
    fprintf(stderr, "keyconfig: no puedo releer los volcados\n");
    return false;
  }

  std::ofstream rep(report_path, std::ios::binary);
  if (!rep.is_open()) {
    fprintf(stderr, "keyconfig: no puedo escribir '%s'\n", report_path);
    return false;
  }
  rep << "# --fl-check-keyconfig-io: exportar, importar y volver a exportar.\n";
  rep << "# Si el ciclo pierde algo, los dos ficheros de datos no coinciden.\n";

  /* Comparacion linea a linea, para que el informe diga QUE cambio. */
  std::istringstream sa(a), sb(b);
  std::string la, lb;
  int n = 0, diff = 0;
  while (true) {
    const bool oka = bool(std::getline(sa, la));
    const bool okb = bool(std::getline(sb, lb));
    if (!oka && !okb) {
      break;
    }
    if (!oka || !okb) {
      diff++;
      rep << "FALTA\t" << (oka ? la : lb) << "\n";
      continue;
    }
    n++;
    if (la != lb) {
      diff++;
      if (diff < 40) {
        rep << "DISTINTO\n  ida:    " << la << "\n  vuelta: " << lb << "\n";
      }
    }
  }
  rep << "lineas comparadas " << n << ", distintas " << diff << "\n";

  /* Compatibilidad hacia atras: el `.py` que escribia el Python se lee de forma
   * NATIVA y tiene que dar la misma configuracion. Se compara re-exportando las
   * dos a datos, que es la unica forma de comparar de verdad dos keyconfigs. */
  int legacy_lines = 0, legacy_diff = 0;
  if (legacy_py != nullptr && legacy_py[0] != '\0') {
    std::string legacy_text;
    if (!file_read_all(legacy_py, legacy_text)) {
      rep << "AVISO: no puedo leer el .py heredado '" << legacy_py << "'\n";
      legacy_diff = 1;
    }
    else {
      wmKeyConfig *kc_legacy = WM_keyconfig_new(wm, "FlipendoRoundtripLegacy", true);
      std::string legacy_error;
      if (!read_legacy_python_text(legacy_text, legacy_py, kc_legacy, legacy_error)) {
        rep << "ERROR leyendo el .py heredado: " << legacy_error << "\n";
        legacy_diff = 1;
      }
      else {
        char path_legacy[FILE_MAX];
        BLI_path_join(path_legacy, sizeof(path_legacy), dir, "roundtrip-heredado.fkeyconfig");
        if (!export_to_file(wm, kc_legacy, path_legacy, true, legacy_error)) {
          rep << "ERROR reexportando el heredado: " << legacy_error << "\n";
          legacy_diff = 1;
        }
        else {
          std::string c;
          file_read_all(path_legacy, c);
          std::istringstream s1(a), s2(c);
          std::string l1, l2;
          while (true) {
            const bool ok1 = bool(std::getline(s1, l1));
            const bool ok2 = bool(std::getline(s2, l2));
            if (!ok1 && !ok2) {
              break;
            }
            if (!ok1 || !ok2) {
              legacy_diff++;
              rep << "HEREDADO FALTA\t" << (ok1 ? l1 : l2) << "\n";
              continue;
            }
            legacy_lines++;
            if (l1 != l2) {
              legacy_diff++;
              if (legacy_diff < 40) {
                rep << "HEREDADO DISTINTO\n  nativo:   " << l1 << "\n  del .py:  " << l2 << "\n";
              }
            }
          }
        }
      }
      WM_keyconfig_remove(wm, kc_legacy);
    }
    rep << "heredado: lineas comparadas " << legacy_lines << ", distintas " << legacy_diff
        << "\n";
    printf("keyconfig: el .py heredado, leido de forma nativa: %d lineas, %d distintas\n",
           legacy_lines,
           legacy_diff);
  }
  rep.close();

  /* Y el volcado del keymap, que es la prueba de verdad: la configuracion
   * reconstruida tiene que comportarse igual. */
  FL_keyconfig_dump(wm, dump_before);
  FL_keyconfig_dump(wm, dump_after);

  WM_keyconfig_remove(wm, kc_probe);

  printf("keyconfig: ciclo exportar-importar-exportar: %d lineas, %d distintas\n", n, diff);
  return (diff == 0) && (legacy_diff == 0);
}

/** \} */

}  // namespace flipendo::keyconfig

/* -------------------------------------------------------------------------- */
/** \name Los operadores, con los mismos identificadores que tenian en Python
 * \{ */

/** `keyconfigs.active`: la de las preferencias, o la por defecto. Mismo criterio
 * que `fl_toolbar_keymap.cc:248` y que `wm_splash_screen.cc:191`. */
static wmKeyConfig *keyconfig_active(wmWindowManager *wm)
{
  wmKeyConfig *kc = static_cast<wmKeyConfig *>(
      BLI_findstring(&wm->keyconfigs, U.keyconfigstr, offsetof(wmKeyConfig, idname)));
  return kc ? kc : wm->defaultconf;
}

static wmOperatorStatus keyconfig_export_exec(bContext *C, wmOperator *op)
{
  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);
  if (filepath[0] == '\0') {
    BKE_report(op->reports, RPT_ERROR, "Filepath not set");
    return OPERATOR_CANCELLED;
  }
  /* El Python forzaba `.py`. Ahora se fuerza el formato de datos: el editor
   * **deja de escribir codigo**. Lo que ya este escrito se sigue leyendo. */
  if (!BLI_path_extension_check(filepath, flipendo::keyconfig::FILE_EXT)) {
    BLI_path_extension_replace(filepath, sizeof(filepath), flipendo::keyconfig::FILE_EXT);
    RNA_string_set(op->ptr, "filepath", filepath);
  }

  wmWindowManager *wm = CTX_wm_manager(C);
  std::string error;
  if (!flipendo::keyconfig::export_to_file(
          wm, keyconfig_active(wm), filepath, RNA_boolean_get(op->ptr, "all"), error))
  {
    BKE_report(op->reports, RPT_ERROR, error.c_str());
    return OPERATOR_CANCELLED;
  }
  return OPERATOR_FINISHED;
}

static wmOperatorStatus keyconfig_export_invoke(bContext *C,
                                                wmOperator *op,
                                                const wmEvent * /*event*/)
{
  if (!RNA_struct_property_is_set(op->ptr, "filepath")) {
    wmWindowManager *wm = CTX_wm_manager(C);
    const wmKeyConfig *kc = keyconfig_active(wm);
    char filepath[FILE_MAX];
    BLI_path_join(filepath,
                  sizeof(filepath),
                  BKE_appdir_folder_default_or_root(),
                  (kc && kc->idname[0]) ? kc->idname : "keyconfig");
    BLI_strncat(filepath, flipendo::keyconfig::FILE_EXT, sizeof(filepath));
    RNA_string_set(op->ptr, "filepath", filepath);
  }
  WM_event_add_fileselect(C, op);
  return OPERATOR_RUNNING_MODAL;
}

void PREFERENCES_OT_keyconfig_export(wmOperatorType *ot)
{
  ot->name = "Export Key Configuration...";
  ot->idname = "PREFERENCES_OT_keyconfig_export";
  ot->description = "Export key configuration to a data file";

  ot->exec = keyconfig_export_exec;
  ot->invoke = keyconfig_export_invoke;

  RNA_def_boolean(ot->srna,
                  "all",
                  false,
                  "All Keymaps",
                  "Write all keymaps (not just user modified)");
  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_TEXT,
                                 FILE_SPECIAL,
                                 FILE_SAVE,
                                 WM_FILESEL_FILEPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
}

static wmOperatorStatus keyconfig_import_exec(bContext *C, wmOperator *op)
{
  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);
  if (filepath[0] == '\0') {
    BKE_report(op->reports, RPT_ERROR, "Filepath not set");
    return OPERATOR_CANCELLED;
  }

  char name[FILE_MAX];
  BLI_path_split_file_part(filepath, name, sizeof(name));

  /* Igual que hacia el Python: el fichero se copia (o se mueve) a la carpeta de
   * configuracion del usuario para que salga en el menu, y luego se carga. */
  const std::optional<std::string> dir = BKE_appdir_folder_id_create(BLENDER_USER_SCRIPTS,
                                                                     "presets/keyconfig");
  std::string target = filepath;
  if (dir.has_value()) {
    char dest[FILE_MAX];
    BLI_path_join(dest, sizeof(dest), dir->c_str(), name);
    if (BLI_path_cmp(dest, filepath) != 0) {
      const bool keep = RNA_boolean_get(op->ptr, "keep_original");
      const bool ok = keep ? (BLI_copy(filepath, dest) == 0) : (BLI_rename(filepath, dest) == 0);
      if (ok) {
        target = dest;
      }
      else {
        /* Copiar es una comodidad (que salga en el menu), no la capacidad. Si
         * falla se avisa y se carga del sitio original: perder la importacion
         * por no poder copiar seria cambiar un problema pequeno por uno grande. */
        BKE_reportf(op->reports,
                    RPT_WARNING,
                    "Could not copy the keymap to the configuration folder, loading in place");
      }
    }
  }

  char config_name[FILE_MAX];
  BLI_strncpy(config_name, name, sizeof(config_name));
  BLI_path_extension_strip(config_name);

  std::string error;
  if (!flipendo::keyconfig::import_from_file(C, target, config_name, error)) {
    BKE_report(op->reports, RPT_ERROR, error.c_str());
    return OPERATOR_CANCELLED;
  }
  return OPERATOR_FINISHED;
}

static wmOperatorStatus keyconfig_import_invoke(bContext *C,
                                                wmOperator *op,
                                                const wmEvent * /*event*/)
{
  WM_event_add_fileselect(C, op);
  return OPERATOR_RUNNING_MODAL;
}

void PREFERENCES_OT_keyconfig_import(wmOperatorType *ot)
{
  ot->name = "Import Key Configuration...";
  ot->idname = "PREFERENCES_OT_keyconfig_import";
  ot->description = "Import key configuration from a data file";

  ot->exec = keyconfig_import_exec;
  ot->invoke = keyconfig_import_invoke;

  RNA_def_boolean(ot->srna,
                  "keep_original",
                  true,
                  "Keep Original",
                  "Keep original file after copying to configuration folder");

  WM_operator_properties_filesel(ot,
                                 FILE_TYPE_FOLDER | FILE_TYPE_TEXT,
                                 FILE_SPECIAL,
                                 FILE_OPENFILE,
                                 WM_FILESEL_FILEPATH,
                                 FILE_DEFAULTDISPLAY,
                                 FILE_SORT_DEFAULT);
}

/** \} */
