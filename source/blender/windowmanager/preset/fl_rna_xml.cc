/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Implementación de `FL_rna_xml.hpp`. Transliteración de `scripts/modules/rna_xml.py`.
 *
 * Se ha hecho línea a línea, incluidas las rarezas del formato, porque la verificación
 * compara el fichero escrito **byte a byte** con el que escribía el Python.
 */

#include "FL_rna_xml.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>

#include "DNA_windowmanager_types.h"

#include "BLI_array.hh"
#include "BLI_fileops.h"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "BKE_context.hh"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"
#include "RNA_types.hh"

#ifdef WITH_PUGIXML
#  include <pugixml.hpp>
#endif

namespace flipendo::rna_xml {

using blender::Span;
using blender::Vector;

namespace {

/* -------------------------------------------------------------------------- */
/** \name Texto
 * \{ */

/** `"{:.6g}".format(v)` de Python, que es exactamente `%.6g` de C para valores finitos. */
std::string fmt_float(const float value)
{
  char buf[64];
  BLI_snprintf(buf, sizeof(buf), "%.6g", double(value));
  return buf;
}

std::string fmt_int(const int value)
{
  char buf[32];
  BLI_snprintf(buf, sizeof(buf), "%d", value);
  return buf;
}

/** `number_to_str` para booleanos. */
const char *fmt_bool(const bool value)
{
  return value ? "TRUE" : "FALSE";
}

void replace_all(std::string &s, const char *from, const char *to)
{
  const size_t from_len = strlen(from);
  size_t pos = 0;
  while ((pos = s.find(from, pos)) != std::string::npos) {
    s.replace(pos, from_len, to);
    pos += strlen(to);
  }
}

/**
 * `xml.sax.saxutils.quoteattr`, con su orden exacto: primero `&`, luego `<` y `>`, luego
 * los tres blancos, y al final elige la comilla. El `&` va el primero a propósito: si
 * fuera después se escaparían los `&` que acaban de introducir los demás.
 */
std::string quoteattr(const std::string &data_in)
{
  std::string data = data_in;
  replace_all(data, "&", "&amp;");
  replace_all(data, "<", "&lt;");
  replace_all(data, ">", "&gt;");
  replace_all(data, "\n", "&#10;");
  replace_all(data, "\r", "&#13;");
  replace_all(data, "\t", "&#9;");

  if (data.find('"') != std::string::npos) {
    if (data.find('\'') != std::string::npos) {
      replace_all(data, "\"", "&quot;");
      return "\"" + data + "\"";
    }
    return "'" + data + "'";
  }
  return "\"" + data + "\"";
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name Las dos listas de tipos del original
 * \{ */

/**
 * `skip_classes`: no se serializan ni se recorren. El original las usaba para dos cosas
 * a la vez —excluirlas del mapa de propiedades y cortar la recursión—, y aquí basta con
 * lo segundo.
 */
bool struct_is_skipped(const StructRNA *srna)
{
  StructRNA *s = const_cast<StructRNA *>(srna);
  return RNA_struct_is_a(s, &RNA_Operator) || RNA_struct_is_a(s, &RNA_Panel) ||
         RNA_struct_is_a(s, &RNA_KeyingSet) || RNA_struct_is_a(s, &RNA_Header) ||
         RNA_struct_is_a(s, &RNA_PropertyGroup);
}

/**
 * `referenced_classes`: no se siguen, se referencian por nombre (`Tipo::nombre`). Tienen
 * que tener una propiedad `name` única. `ID` cubre casi todo.
 */
bool struct_is_referenced(const StructRNA *srna)
{
  StructRNA *s = const_cast<StructRNA *>(srna);
  return RNA_struct_is_a(s, &RNA_ID) || RNA_struct_is_a(s, &RNA_Bone) ||
         RNA_struct_is_a(s, &RNA_ActionGroup) || RNA_struct_is_a(s, &RNA_PoseBone) ||
         RNA_struct_is_a(s, &RNA_Node) || RNA_struct_is_a(s, &RNA_Strip);
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name El escritor
 * \{ */

struct Writer {
  std::string out;
  std::string ident_val = "  ";

  void fw(const std::string &text)
  {
    out += text;
  }

  /**
   * El caso especial de color del original: una propiedad de subtipo `COLOR_GAMMA`,
   * acotada a 0..1 y de tres o cuatro componentes se escribe en hexadecimal.
   *
   * Ojo con `int(v * 255)`: Python TRUNCA hacia cero, no redondea. 0.5 -> 127, no 128.
   */
  static bool prop_is_hex_color(PointerRNA *ptr, PropertyRNA *prop, const int array_len)
  {
    if (RNA_property_type(prop) != PROP_FLOAT) {
      return false;
    }
    if (!ELEM(array_len, 3, 4)) {
      return false;
    }
    if (RNA_property_subtype(prop) != PROP_COLOR_GAMMA) {
      return false;
    }
    float hardmin, hardmax;
    RNA_property_float_range(ptr, prop, &hardmin, &hardmax);
    return hardmin == 0.0f && hardmax == 1.0f;
  }

  static std::string array_to_string(PointerRNA *ptr, PropertyRNA *prop, const int array_len)
  {
    if (prop_is_hex_color(ptr, prop, array_len)) {
      blender::Array<float> values(array_len);
      RNA_property_float_get_array(ptr, prop, values.data());
      std::string out = "#";
      for (const int i : values.index_range()) {
        char buf[8];
        BLI_snprintf(buf, sizeof(buf), "%02x", int(values[i] * 255.0f));
        out += buf;
      }
      return out;
    }

    std::string out;
    for (int i = 0; i < array_len; i++) {
      if (i != 0) {
        out += " ";
      }
      switch (RNA_property_type(prop)) {
        case PROP_BOOLEAN:
          out += fmt_bool(RNA_property_boolean_get_index(ptr, prop, i));
          break;
        case PROP_INT:
          out += fmt_int(RNA_property_int_get_index(ptr, prop, i));
          break;
        case PROP_FLOAT:
          out += fmt_float(RNA_property_float_get_index(ptr, prop, i));
          break;
        default:
          break;
      }
    }
    return out;
  }

  /** `rna2xml_node`. */
  void node(const std::string &ident, PointerRNA *value, const PointerRNA *parent)
  {
    const std::string ident_next = ident + ident_val;

    if (struct_is_skipped(value->type)) {
      return;
    }
    /* «XXX, FIXME, point-cache has eternal nested pointer to itself». El original corta
     * la recursión comparando el valor con su padre; aquí es comparar el puntero y el
     * tipo, que es lo que hace la igualdad de `bpy_struct`. */
    if (parent != nullptr && parent->data == value->data && parent->type == value->type) {
      return;
    }

    const char *type_name = RNA_struct_identifier(value->type);

    Vector<std::string> node_attrs;
    Vector<PropertyRNA *> nodes_items;
    Vector<PropertyRNA *> nodes_lists;

    RNA_STRUCT_BEGIN (value, prop) {
      const char *id = RNA_property_identifier(prop);
      if (STREQ(id, "rna_type")) {
        continue;
      }
      /* `if not prop.is_skip_save`. */
      if (RNA_property_flag(prop) & PROP_SKIP_SAVE) {
        continue;
      }

      const PropertyType type = RNA_property_type(prop);
      const int array_len = RNA_property_array_length(value, prop);

      if (array_len > 0) {
        node_attrs.append(std::string(id) + "=\"" + array_to_string(value, prop, array_len) +
                          "\"");
        continue;
      }

      switch (type) {
        case PROP_BOOLEAN:
          node_attrs.append(std::string(id) + "=\"" +
                            fmt_bool(RNA_property_boolean_get(value, prop)) + "\"");
          break;
        case PROP_INT:
          node_attrs.append(std::string(id) + "=\"" + fmt_int(RNA_property_int_get(value, prop)) +
                            "\"");
          break;
        case PROP_FLOAT:
          node_attrs.append(std::string(id) + "=\"" +
                            fmt_float(RNA_property_float_get(value, prop)) + "\"");
          break;
        case PROP_STRING: {
          char *text = RNA_property_string_get_alloc(value, prop, nullptr, 0, nullptr);
          node_attrs.append(std::string(id) + "=" + quoteattr(text ? text : ""));
          if (text) {
            MEM_freeN(text);
          }
          break;
        }
        case PROP_ENUM: {
          if (RNA_property_flag(prop) & PROP_ENUM_FLAG) {
            /* En Python esto era un `set`, y `",".join(list(conjunto))` sale en el orden
             * de iteración del conjunto, que depende del hash de las cadenas y por tanto
             * NO es estable entre ejecuciones. Aquí se emite en el orden de la tabla de
             * RNA, que sí lo es. Es una diferencia deliberada y no afecta a nada del
             * árbol: ningún tema usa una enumeración de banderas (medido: cero `="{`
             * en los dos temas distribuidos). */
            const int flag_value = RNA_property_enum_get(value, prop);
            const EnumPropertyItem *items = nullptr;
            bool free_items = false;
            RNA_property_enum_items(nullptr, value, prop, &items, nullptr, &free_items);
            std::string joined;
            for (const EnumPropertyItem *item = items; item && item->identifier; item++) {
              if (item->value != 0 && (flag_value & item->value) == item->value) {
                if (!joined.empty()) {
                  joined += ",";
                }
                joined += item->identifier;
              }
            }
            if (free_items) {
              MEM_freeN(const_cast<EnumPropertyItem *>(items));
            }
            node_attrs.append(std::string(id) + "=" + quoteattr("{" + joined + "}"));
          }
          else {
            /* Una enumeración simple llega a Python como `str`. */
            const int enum_value = RNA_property_enum_get(value, prop);
            const EnumPropertyItem *items = nullptr;
            bool free_items = false;
            RNA_property_enum_items(nullptr, value, prop, &items, nullptr, &free_items);
            const char *ident = "";
            RNA_enum_id_from_value(items, enum_value, &ident);
            if (free_items) {
              MEM_freeN(const_cast<EnumPropertyItem *>(items));
            }
            node_attrs.append(std::string(id) + "=" + quoteattr(ident));
          }
          break;
        }
        case PROP_POINTER: {
          PointerRNA sub = RNA_property_pointer_get(value, prop);
          if (sub.data == nullptr) {
            /* `elif subvalue is None`. */
            node_attrs.append(std::string(id) + "=\"NONE\"");
          }
          else if (struct_is_referenced(sub.type)) {
            char *name = RNA_struct_name_get_alloc(&sub, nullptr, 0, nullptr);
            node_attrs.append(std::string(id) + "=" +
                              quoteattr(std::string(RNA_struct_identifier(sub.type)) +
                                        "::" + (name ? name : "")));
            if (name) {
              MEM_freeN(name);
            }
          }
          else {
            nodes_items.append(prop);
          }
          break;
        }
        case PROP_COLLECTION:
          nodes_lists.append(prop);
          break;
        default:
          break;
      }
    }
    RNA_STRUCT_END;

    /* Apertura y atributos. `pretty_format` siempre está activo en el único camino que
     * usaba el árbol. Ojo con el `>`: va en la sangría de los ATRIBUTOS. */
    if (!node_attrs.is_empty()) {
      fw(ident + "<" + type_name + "\n");
      for (const std::string &attr : node_attrs) {
        fw(ident_next + attr + "\n");
      }
      fw(ident_next + ">\n");
    }
    else {
      fw(ident + "<" + type_name + ">\n");
    }

    /* Miembros únicos. El envoltorio lleva el nombre de la PROPIEDAD, no el del tipo. */
    for (PropertyRNA *prop : nodes_items) {
      const char *id = RNA_property_identifier(prop);
      fw(ident_next + "<" + id + ">\n");
      PointerRNA sub = RNA_property_pointer_get(value, prop);
      node(ident_next + ident_val, &sub, value);
      fw(ident_next + "</" + id + ">\n");
    }

    /* Listas. */
    for (PropertyRNA *prop : nodes_lists) {
      const char *id = RNA_property_identifier(prop);
      fw(ident_next + "<" + id + ">\n");
      RNA_PROP_BEGIN (value, item, prop) {
        if (item.data != nullptr) {
          PointerRNA item_copy = item;
          node(ident_next + ident_val, &item_copy, value);
        }
      }
      RNA_PROP_END;
      fw(ident_next + "</" + id + ">\n");
    }

    fw(ident + "</" + type_name + ">\n");
  }
};

/** `_get_context_val`: `context.path_resolve(path)`. */
bool context_value_get(bContext *C, const char *path, PointerRNA &r_ptr)
{
  PointerRNA ctx_ptr = RNA_pointer_create_discrete(nullptr, &RNA_Context, C);
  PropertyRNA *prop = nullptr;
  if (!RNA_path_resolve(&ctx_ptr, path, &r_ptr, &prop)) {
    return false;
  }
  return r_ptr.data != nullptr;
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name El lector
 * \{ */

#ifdef WITH_PUGIXML

bool name_is_secure(const Span<const char *> secure_types, const char *name)
{
  if (secure_types.is_empty()) {
    return true;
  }
  for (const char *allowed : secure_types) {
    if (STREQ(allowed, name)) {
      return true;
    }
  }
  return false;
}

/** Trocea por espacios, como `str.split()` de Python. */
Vector<std::string> split_ws(const std::string &text)
{
  Vector<std::string> out;
  size_t i = 0;
  while (i < text.size()) {
    while (i < text.size() && isspace(static_cast<unsigned char>(text[i]))) {
      i++;
    }
    const size_t start = i;
    while (i < text.size() && !isspace(static_cast<unsigned char>(text[i]))) {
      i++;
    }
    if (i > start) {
      out.append(text.substr(start, i - start));
    }
  }
  return out;
}

/**
 * Asigna un atributo XML a una propiedad RNA, reproduciendo la coerción del original.
 *
 * El `bContext` NO es decorativo: `setattr` de Python llama a `RNA_property_update` con
 * el contexto de verdad, y esa función acaba en `CTX_data_main(C)`. Pasarle `nullptr`
 * —que es lo primero que uno escribe— tumba el proceso al leer el primer tema. Lo cazó
 * el arnés con un volcado truncado a mitad del caso 1.
 */
void attribute_apply(bContext *C, PointerRNA *value, PropertyRNA *prop, const std::string &text)
{
  const PropertyType type = RNA_property_type(prop);
  const int array_len = RNA_property_array_length(value, prop);

  if (array_len > 0) {
    Vector<float> numbers;
    if (!text.empty() && text[0] == '#') {
      /* Hexadecimal: cada par de dígitos es un byte que se divide por 255. */
      for (size_t i = 1; i + 1 < text.size() + 1; i += 2) {
        if (i + 1 >= text.size() + 1) {
          break;
        }
        const std::string pair = text.substr(i, 2);
        if (pair.size() < 2) {
          break;
        }
        numbers.append(float(strtol(pair.c_str(), nullptr, 16)) / 255.0f);
      }
    }
    else {
      for (const std::string &piece : split_ws(text)) {
        if (piece == "TRUE") {
          numbers.append(1.0f);
        }
        else if (piece == "FALSE") {
          numbers.append(0.0f);
        }
        else {
          numbers.append(float(atof(piece.c_str())));
        }
      }
    }
    /* El `except ValueError` del original: si el tamaño no encaja, se recorta o se
     * completa con lo que ya había, NUNCA se deja el array a medias. */
    const int n = std::min<int>(array_len, int(numbers.size()));
    for (int i = 0; i < n; i++) {
      switch (type) {
        case PROP_BOOLEAN:
          RNA_property_boolean_set_index(value, prop, i, numbers[i] != 0.0f);
          break;
        case PROP_INT:
          RNA_property_int_set_index(value, prop, i, int(numbers[i]));
          break;
        case PROP_FLOAT:
          RNA_property_float_set_index(value, prop, i, numbers[i]);
          break;
        default:
          break;
      }
    }
    RNA_property_update(C, value, prop);
    return;
  }

  switch (type) {
    case PROP_BOOLEAN:
      RNA_property_boolean_set(value, prop, text == "TRUE");
      break;
    case PROP_INT:
      RNA_property_int_set(value, prop, int(atol(text.c_str())));
      break;
    case PROP_FLOAT:
      RNA_property_float_set(value, prop, float(atof(text.c_str())));
      break;
    case PROP_STRING:
      RNA_property_string_set(value, prop, text.c_str());
      break;
    case PROP_ENUM: {
      const EnumPropertyItem *items = nullptr;
      bool free_items = false;
      RNA_property_enum_items(nullptr, value, prop, &items, nullptr, &free_items);
      int enum_value = 0;
      if (RNA_enum_value_from_id(items, text.c_str(), &enum_value)) {
        RNA_property_enum_set(value, prop, enum_value);
      }
      if (free_items) {
        MEM_freeN(const_cast<EnumPropertyItem *>(items));
      }
      break;
    }
    default:
      break;
  }
  RNA_property_update(C, value, prop);
}

void node_read(bContext *C,
               const pugi::xml_node &xml_node,
               PointerRNA *value,
               const Span<const char *> secure_types)
{
  if (!name_is_secure(secure_types, xml_node.name())) {
    printf("Loading the XML with type restrictions, skipping \"%s\"\n", xml_node.name());
    return;
  }

  /* Atributos simples. */
  for (const pugi::xml_attribute &attr : xml_node.attributes()) {
    PropertyRNA *prop = RNA_struct_find_property(value, attr.name());
    if (prop == nullptr) {
      printf("%s.%s not found\n", RNA_struct_identifier(value->type), attr.name());
      continue;
    }
    attribute_apply(C, value, prop, attr.value());
  }

  /* Atributos compuestos: un elemento hijo con el nombre de la PROPIEDAD, y dentro uno
   * o varios elementos con el nombre del TIPO. */
  for (const pugi::xml_node &child : xml_node.children()) {
    if (child.type() != pugi::node_element) {
      continue;
    }
    PropertyRNA *prop = RNA_struct_find_property(value, child.name());
    if (prop == nullptr) {
      continue;
    }

    Vector<pugi::xml_node> elems;
    for (const pugi::xml_node &inner : child.children()) {
      if (inner.type() == pugi::node_element) {
        elems.append(inner);
      }
    }

    if (RNA_property_type(prop) == PROP_COLLECTION) {
      const int length = RNA_property_collection_length(value, prop);
      if (int(elems.size()) != length) {
        printf("Size Mismatch! collection: %s\n", child.name());
        continue;
      }
      int index = 0;
      RNA_PROP_BEGIN (value, item, prop) {
        PointerRNA item_copy = item;
        node_read(C, elems[index], &item_copy, secure_types);
        index++;
      }
      RNA_PROP_END;
    }
    else if (RNA_property_type(prop) == PROP_POINTER) {
      /* `len(elems) == 1`: un sub-nodo nombrado por su tipo. Vacío también vale. */
      if (elems.size() == 1) {
        PointerRNA sub = RNA_property_pointer_get(value, prop);
        if (sub.data != nullptr) {
          node_read(C, elems[0], &sub, secure_types);
        }
      }
    }
  }
}

#endif /* WITH_PUGIXML */

/** \} */

}  // namespace

/* -------------------------------------------------------------------------- */
/** \name API pública
 * \{ */

bool write_string(bContext *C,
                  const Span<MapEntry> rna_map,
                  std::string &r_text,
                  std::string &r_error)
{
  Writer writer;
  writer.fw("<bpy>\n");
  for (const MapEntry &entry : rna_map) {
    PointerRNA ptr;
    if (!context_value_get(C, entry.rna_path, ptr)) {
      r_error = std::string("ruta no encontrada: ") + entry.rna_path;
      return false;
    }
    writer.node("  ", &ptr, nullptr);
  }
  writer.fw("</bpy>\n");
  r_text = writer.out;
  return true;
}

bool write_file(bContext *C,
                const std::string &filepath,
                const Span<MapEntry> rna_map,
                std::string &r_error)
{
  std::string text;
  if (!write_string(C, rna_map, text, r_error)) {
    return false;
  }
  FILE *f = BLI_fopen(filepath.c_str(), "w");
  if (f == nullptr) {
    r_error = "no se pudo escribir '" + filepath + "'";
    return false;
  }
  fwrite(text.data(), 1, text.size(), f);
  fclose(f);
  return true;
}

bool run_file(bContext *C,
              const std::string &filepath,
              const Span<MapEntry> rna_map,
              const Span<const char *> secure_types,
              std::string &r_error)
{
#ifdef WITH_PUGIXML
  pugi::xml_document doc;
  const pugi::xml_parse_result result = doc.load_file(filepath.c_str());
  if (!result) {
    r_error = "XML ilegible en '" + filepath + "': " + result.description();
    return false;
  }
  /* `getElementsByTagName("bpy")[0]`: el primero en orden de documento, a cualquier
   * profundidad. */
  const pugi::xml_node bpy_node = doc.find_node(
      [](const pugi::xml_node &n) { return STREQ(n.name(), "bpy"); });
  if (!bpy_node) {
    r_error = "el fichero no tiene un elemento <bpy>";
    return false;
  }

  for (const MapEntry &entry : rna_map) {
    const char *tag = entry.xml_tag;
    const pugi::xml_node xml_node = bpy_node.find_node(
        [tag](const pugi::xml_node &n) { return STREQ(n.name(), tag); });
    if (!xml_node) {
      /* El original indexaba `[0]` y habría levantado IndexError; aquí se dice y se
       * sigue con la siguiente entrada, que es menos destructivo. */
      printf("rna_xml: no hay elemento <%s> en '%s'\n", entry.xml_tag, filepath.c_str());
      continue;
    }
    PointerRNA ptr;
    if (!context_value_get(C, entry.rna_path, ptr)) {
      printf("Error: path '%s' not found\n", entry.rna_path);
      continue;
    }
    node_read(C, xml_node, &ptr, secure_types);
  }
  return true;
#else
  (void)C;
  (void)filepath;
  (void)rna_map;
  (void)secure_types;
  r_error = "esta build no lleva pugixml: no se pueden leer temas XML";
  return false;
#endif
}

/** \} */

}  // namespace flipendo::rna_xml
