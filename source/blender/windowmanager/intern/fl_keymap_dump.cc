/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Volcado del mapa de teclado. Ver FL_keymap_dump.hpp.
 */

#include "FL_keymap_dump.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "DNA_windowmanager_types.h"

#include "BLI_listbase.h"

#include "WM_api.hh"
#include "WM_keymap.hh"

#include "keymap/FL_keymap_default.hpp"

#include "DNA_ID.h"


namespace {

struct KeyConfigRef {
  const char *label;
  const wmKeyConfig *conf;
};

/* Serializador propio de IDProperty.
 *
 * No se usa IDP_reprN por dos razones. La primera es que revienta: con las
 * propiedades reales de los atajos, idp_repr_fn_recursive peta (verificado con
 * backtrace). La segunda es que aqui hace falta una salida DETERMINISTA — las
 * claves de un grupo se ordenan alfabeticamente — porque este texto existe para
 * compararse entre dos builds, y un cambio de orden de insercion no debe
 * confundirse con un cambio de comportamiento. */
void property_to_text(const IDProperty *prop, std::string &out);

void property_value_to_text(const IDProperty *prop, std::string &out)
{
  char buf[64];

  switch (prop->type) {
    case IDP_STRING: {
      const char *s = static_cast<const char *>(prop->data.pointer);
      out += '"';
      out += (s ? s : "");
      out += '"';
      break;
    }
    case IDP_INT:
      std::snprintf(buf, sizeof(buf), "%d", prop->data.val);
      out += buf;
      break;
    case IDP_BOOLEAN:
      out += (prop->data.val ? "true" : "false");
      break;
    case IDP_FLOAT: {
      float f;
      std::memcpy(&f, &prop->data.val, sizeof(float));
      std::snprintf(buf, sizeof(buf), "%.9g", double(f));
      out += buf;
      break;
    }
    case IDP_DOUBLE: {
      double d;
      std::memcpy(&d, &prop->data.val, sizeof(double));
      std::snprintf(buf, sizeof(buf), "%.17g", d);
      out += buf;
      break;
    }
    case IDP_ARRAY: {
      out += '[';
      for (int i = 0; i < prop->len; i++) {
        if (i) {
          out += ',';
        }
        switch (prop->subtype) {
          case IDP_INT:
            std::snprintf(buf, sizeof(buf), "%d", static_cast<const int *>(prop->data.pointer)[i]);
            break;
          case IDP_BOOLEAN:
            std::snprintf(buf, sizeof(buf), "%d",
                          int(static_cast<const int8_t *>(prop->data.pointer)[i]));
            break;
          case IDP_FLOAT:
            std::snprintf(buf, sizeof(buf), "%.9g",
                          double(static_cast<const float *>(prop->data.pointer)[i]));
            break;
          case IDP_DOUBLE:
            std::snprintf(buf, sizeof(buf), "%.17g",
                          static_cast<const double *>(prop->data.pointer)[i]);
            break;
          default:
            std::snprintf(buf, sizeof(buf), "?");
            break;
        }
        out += buf;
      }
      out += ']';
      break;
    }
    case IDP_GROUP: {
      std::vector<const IDProperty *> members;
      LISTBASE_FOREACH (const IDProperty *, sub, &prop->data.group) {
        members.push_back(sub);
      }
      std::sort(members.begin(), members.end(), [](const IDProperty *a, const IDProperty *b) {
        return std::strcmp(a->name, b->name) < 0;
      });
      out += '{';
      for (size_t i = 0; i < members.size(); i++) {
        if (i) {
          out += ',';
        }
        property_to_text(members[i], out);
      }
      out += '}';
      break;
    }
    case IDP_IDPARRAY: {
      const IDProperty *items = static_cast<const IDProperty *>(prop->data.pointer);
      out += '<';
      for (int i = 0; i < prop->len; i++) {
        if (i) {
          out += ',';
        }
        property_value_to_text(&items[i], out);
      }
      out += '>';
      break;
    }
    case IDP_ID:
      /* Un puntero a ID no se puede comparar entre ejecuciones; basta con saber que
       * la propiedad es de ese tipo. */
      out += "<ID>";
      break;
    default:
      std::snprintf(buf, sizeof(buf), "<tipo %d>", int(prop->type));
      out += buf;
      break;
  }
}

void property_to_text(const IDProperty *prop, std::string &out)
{
  out += prop->name;
  out += '=';
  property_value_to_text(prop, out);
}

std::string properties_to_text(const IDProperty *properties)
{
  if (properties == nullptr) {
    return "";
  }
  std::string out;
  property_value_to_text(properties, out);
  return out;
}

void write_keymap(FILE *fp, const wmKeyMap *km);
void write_keymap(FILE *fp, const wmKeyMap *km)
{
  std::fprintf(fp,
               "KEYMAP %s space=%d region=%d flag=%d\n",
               km->idname,
               int(km->spaceid),
               int(km->regionid),
               int(km->flag));

  LISTBASE_FOREACH (const wmKeyMapItem *, kmi, &km->items) {
    /* Un elemento de keymap modal no lleva operador sino un valor de enumeracion. */
    const bool is_modal = (km->flag & KEYMAP_MODAL) != 0;

    std::fprintf(fp,
                 "  ITEM %s%s type=%d val=%d dir=%d "
                 "shift=%d ctrl=%d alt=%d oskey=%d hyper=%d keymod=%d flag=%d maptype=%d",
                 is_modal ? "propvalue=" : "",
                 is_modal ? std::to_string(int(kmi->propvalue)).c_str() : kmi->idname,
                 int(kmi->type),
                 int(kmi->val),
                 int(kmi->direction),
                 int(kmi->shift),
                 int(kmi->ctrl),
                 int(kmi->alt),
                 int(kmi->oskey),
                 int(kmi->hyper),
                 int(kmi->keymodifier),
                 int(kmi->flag),
                 int(kmi->maptype));

    const std::string props = properties_to_text(kmi->properties);
    if (!props.empty()) {
      std::fprintf(fp, " props=%s", props.c_str());
    }
    std::fputc('\n', fp);
  }
}

}  // namespace

bool FL_keyconfig_dump(const wmWindowManager *wm, const char *filepath)
{
  if (wm == nullptr) {
    std::fprintf(stderr, "FL_keyconfig_dump: no hay gestor de ventanas\n");
    return false;
  }

  FILE *fp = std::fopen(filepath, "w");
  if (fp == nullptr) {
    std::fprintf(stderr, "FL_keyconfig_dump: no se pudo escribir %s\n", filepath);
    return false;
  }

  const KeyConfigRef configs[] = {
      {"default", wm->defaultconf},
      {"addon", wm->addonconf},
      {"user", wm->userconf},
  };

  for (const KeyConfigRef &ref : configs) {
    if (ref.conf == nullptr) {
      continue;
    }

    /* Los keymaps se ordenan por (idname, space, region) para que el volcado no
     * dependa del orden en que se registraron. El orden de los ELEMENTOS dentro de
     * cada keymap si se respeta: gana el primero que casa, o sea que es semantico. */
    std::vector<const wmKeyMap *> keymaps;
    LISTBASE_FOREACH (const wmKeyMap *, km, &ref.conf->keymaps) {
      keymaps.push_back(km);
    }
    std::sort(keymaps.begin(), keymaps.end(), [](const wmKeyMap *a, const wmKeyMap *b) {
      const int c = std::strcmp(a->idname, b->idname);
      if (c != 0) {
        return c < 0;
      }
      if (a->spaceid != b->spaceid) {
        return a->spaceid < b->spaceid;
      }
      return a->regionid < b->regionid;
    });

    size_t items = 0;
    for (const wmKeyMap *km : keymaps) {
      items += size_t(BLI_listbase_count(&km->items));
    }
    std::fprintf(fp,
                 "CONFIG %s keymaps=%zu items=%zu\n",
                 ref.label,
                 keymaps.size(),
                 items);

    for (const wmKeyMap *km : keymaps) {
      write_keymap(fp, km);
    }
  }

  std::fclose(fp);
  return true;
}

bool FL_keyconfig_dump_native(wmWindowManager *wm, const char *filepath)
{
  if (wm == nullptr) {
    std::fprintf(stderr, "FL_keyconfig_dump_native: no hay gestor de ventanas\n");
    return false;
  }

  /* Configuracion temporal, para no tocar la que usa el editor. */
  wmKeyConfig *kc = WM_keyconfig_new(wm, "Flipendo Native (temporal)", false);
  flipendo::keymap::register_default(kc);

  FILE *fp = std::fopen(filepath, "w");
  if (fp == nullptr) {
    std::fprintf(stderr, "FL_keyconfig_dump_native: no se pudo escribir %s\n", filepath);
    WM_keyconfig_remove(wm, kc);
    return false;
  }

  std::vector<const wmKeyMap *> keymaps;
  LISTBASE_FOREACH (const wmKeyMap *, km, &kc->keymaps) {
    keymaps.push_back(km);
  }
  std::sort(keymaps.begin(), keymaps.end(), [](const wmKeyMap *a, const wmKeyMap *b) {
    const int c = std::strcmp(a->idname, b->idname);
    if (c != 0) {
      return c < 0;
    }
    if (a->spaceid != b->spaceid) {
      return a->spaceid < b->spaceid;
    }
    return a->regionid < b->regionid;
  });

  size_t items = 0;
  for (const wmKeyMap *km : keymaps) {
    items += size_t(BLI_listbase_count(&km->items));
  }
  std::fprintf(fp, "CONFIG default keymaps=%zu items=%zu\n", keymaps.size(), items);
  for (const wmKeyMap *km : keymaps) {
    write_keymap(fp, km);
  }

  std::fclose(fp);
  WM_keyconfig_remove(wm, kc);
  return true;
}

/* -------------------------------------------------------------------- */
/** \name Comprobacion contra la linea base
 * \{ */

namespace {

/* Trocea un volcado en {nombre de keymap -> sus lineas}, quedandose solo con la
 * seccion "CONFIG default". */
std::map<std::string, std::string> parse_dump(std::istream &in)
{
  std::map<std::string, std::string> out;
  std::string line;
  std::string current;
  bool in_default = false;

  while (std::getline(in, line)) {
    if (line.rfind("CONFIG ", 0) == 0) {
      in_default = (line.rfind("CONFIG default", 0) == 0);
      current.clear();
      continue;
    }
    if (!in_default) {
      continue;
    }
    if (line.rfind("KEYMAP ", 0) == 0) {
      /* El nombre llega hasta " space=". */
      const size_t start = 7;
      const size_t end = line.find(" space=", start);
      current = (end == std::string::npos) ? line.substr(start) : line.substr(start, end - start);
      out[current] = line + "\n";
      continue;
    }
    if (!current.empty()) {
      out[current] += line + "\n";
    }
  }
  return out;
}

}  // namespace

bool FL_keyconfig_check_native(wmWindowManager *wm, const char *baseline_filepath)
{
  std::ifstream baseline_file(baseline_filepath);
  if (!baseline_file) {
    std::fprintf(stderr, "FL_keyconfig_check_native: no se pudo leer %s\n", baseline_filepath);
    return false;
  }
  const std::map<std::string, std::string> baseline = parse_dump(baseline_file);

  /* El keymap nativo se construye en memoria y se serializa al mismo formato. */
  wmKeyConfig *kc = WM_keyconfig_new(wm, "Flipendo Native (comprobacion)", false);
  flipendo::keymap::register_default(kc);

  std::ostringstream native_text;
  {
    /* Se reutiliza el mismo escritor, volcando a un fichero temporal en memoria no
     * es posible con FILE*, asi que se recorre directamente. */
    std::vector<const wmKeyMap *> keymaps;
    LISTBASE_FOREACH (const wmKeyMap *, km, &kc->keymaps) {
      keymaps.push_back(km);
    }
    native_text << "CONFIG default keymaps=" << keymaps.size() << " items=0\n";
    for (const wmKeyMap *km : keymaps) {
      char *buf = nullptr;
      size_t size = 0;
      FILE *mem = open_memstream(&buf, &size);
      if (mem) {
        write_keymap(mem, km);
        std::fclose(mem);
        native_text << buf;
        free(buf);
      }
    }
  }
  std::istringstream native_stream(native_text.str());
  const std::map<std::string, std::string> native = parse_dump(native_stream);

  size_t ok = 0, bad = 0;
  for (const auto &kv : native) {
    const auto it = baseline.find(kv.first);
    if (it == baseline.end()) {
      std::printf("  SOBRA    %s (no existe en la linea base)\n", kv.first.c_str());
      bad++;
      continue;
    }
    if (it->second == kv.second) {
      ok++;
    }
    else {
      std::printf("  DIFIERE  %s\n", kv.first.c_str());
      bad++;
    }
  }

  std::printf("\nkeymaps transliterados: %zu  |  identicos: %zu  |  con diferencias: %zu\n",
              native.size(), ok, bad);
  std::printf("linea base: %zu keymaps  |  quedan por transliterar: %zu\n",
              baseline.size(),
              baseline.size() > native.size() ? baseline.size() - native.size() : 0);

  WM_keyconfig_remove(wm, kc);
  return bad == 0;
}

/** \} */
