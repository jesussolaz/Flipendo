/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Volcado del catalogo de herramientas. Ver FL_toolsystem_dump.hpp.
 */

#include <algorithm>
#include <utility>
#include <cstdio>
#include <cstring>
#include <string>

#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_index_range.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"
#include "BLI_map.hh"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "DNA_space_types.h"

#include "FL_toolsystem.hpp"
#include "FL_toolsystem_dump.hpp"

namespace flipendo::toolsystem {

/* -------------------------------------------------------------------- */
/** \name Formato
 *
 * La linea base la genero un script de Python con `%r` y `%s`, asi que el formato hay
 * que reproducirlo tal cual, comillas incluidas. No es capricho: cualquier diferencia
 * de formato se confunde con una diferencia de contenido.
 * \{ */

/** `repr()` de una cadena de Python. */
static std::string py_repr(const char *s)
{
  if (s == nullptr) {
    return "None";
  }
  const blender::StringRefNull str(s);
  /* Python prefiere la comilla simple, y cambia a la doble solo si la cadena lleva una
   * simple y no lleva ninguna doble. */
  const bool has_single = str.find('\'') != blender::StringRef::not_found;
  const bool has_double = str.find('"') != blender::StringRef::not_found;
  const char quote = (has_single && !has_double) ? '"' : '\'';

  std::string out;
  out += quote;
  for (const char c : str) {
    switch (c) {
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
        if (c == quote) {
          out += '\\';
          out += c;
        }
        else if (static_cast<unsigned char>(c) < 0x20 || static_cast<unsigned char>(c) == 0x7f) {
          char buf[8];
          SNPRINTF(buf, "\\x%02x", static_cast<unsigned char>(c));
          out += buf;
        }
        else {
          out += c;
        }
        break;
    }
  }
  out += quote;
  return out;
}

/** Un campo opcional: la cadena, o `None` sin comillas, como lo escribe `%s`. */
static std::string py_str_or_none(const char *s)
{
  return s != nullptr ? std::string(s) : std::string("None");
}

/** `sorted(item.options)` del Python, o `None` si no hay ninguna. */
static std::string options_repr(const int options)
{
  blender::Vector<const char *> names;
  /* En orden alfabetico, que es lo que da `sorted()` sobre el conjunto. */
  if (options & TOOL_OPTION_KEYMAP_FALLBACK) {
    names.append("KEYMAP_FALLBACK");
  }
  if (options & TOOL_OPTION_USE_BRUSHES) {
    names.append("USE_BRUSHES");
  }
  if (names.is_empty()) {
    return "None";
  }
  std::string out = "[";
  for (const int i : names.index_range()) {
    if (i != 0) {
      out += ", ";
    }
    out += py_repr(names[i]);
  }
  out += "]";
  return out;
}

static std::string tool_line(const ToolDecl &tool)
{
  std::string out = "  TOOL ";
  out += tool.idname;
  out += " label=" + py_repr(tool.label);
  out += " icon=" + py_str_or_none(tool.icon);
  out += " cursor=" + py_str_or_none(tool.cursor);
  out += " widget=" + py_str_or_none(tool.gizmo_group);
  out += " keymap=" + py_str_or_none(tool.keymap_name);
  out += " brush_type=" + py_str_or_none(tool.brush_type);
  out += " data_block=" + py_str_or_none(tool.data_block);
  out += " op=" + py_str_or_none(tool.op);
  out += " options=" + options_repr(tool.options);
  return out;
}

/** El identificador RNA del espacio: 'NODE_EDITOR', 'VIEW_3D'... */
static const char *space_type_identifier(const int space_type)
{
  const char *identifier = nullptr;
  if (!RNA_enum_identifier(rna_enum_space_type_items, space_type, &identifier)) {
    return "UNKNOWN";
  }
  return identifier;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Recorrido del catalogo
 * \{ */

struct SpaceMode {
  const ToolbarDecl *toolbar;
  /** `nullptr` en un espacio sin modos. */
  const char *mode;
};

/**
 * Espacios por nombre y, dentro, modos por nombre: el orden de las lineas base. Un
 * espacio sin modos da una sola pareja con `mode == nullptr`.
 */
static blender::Vector<SpaceMode> space_modes_ordered()
{
  blender::Vector<const ToolbarDecl *> toolbars;
  for (const ToolbarDecl *decl : toolbars_all()) {
    toolbars.append(decl);
  }
  std::sort(toolbars.begin(), toolbars.end(), [](const ToolbarDecl *a, const ToolbarDecl *b) {
    return strcmp(space_type_identifier(a->space_type), space_type_identifier(b->space_type)) < 0;
  });

  blender::Vector<SpaceMode> out;
  for (const ToolbarDecl *toolbar : toolbars) {
    blender::Vector<const char *> modes;
    for (const ModeTools &mode_tools : toolbar->modes) {
      if (mode_tools.mode != nullptr) {
        modes.append(mode_tools.mode);
      }
    }
    std::sort(modes.begin(), modes.end(), [](const char *a, const char *b) {
      return strcmp(a, b) < 0;
    });
    if (modes.is_empty()) {
      modes.append(nullptr);
    }
    for (const char *mode : modes) {
      out.append({toolbar, mode});
    }
  }
  return out;
}

struct Section {
  std::string header;
  blender::Vector<std::string> lines;
};

/**
 * Todas las secciones que el catalogo nativo sabe producir, ordenadas igual que la
 * linea base: por nombre de espacio y, dentro, por nombre de modo.
 */
static blender::Vector<Section> sections_build(const bContext *C)
{
  blender::Vector<const ToolbarDecl *> toolbars;
  for (const ToolbarDecl *decl : toolbars_all()) {
    toolbars.append(decl);
  }
  std::sort(toolbars.begin(), toolbars.end(), [](const ToolbarDecl *a, const ToolbarDecl *b) {
    return strcmp(space_type_identifier(a->space_type), space_type_identifier(b->space_type)) < 0;
  });

  blender::Vector<Section> out;
  for (const ToolbarDecl *toolbar : toolbars) {
    /* Los modos con nombre, en orden alfabetico. Un espacio sin ninguno produce una
     * sola seccion etiquetada None: es lo que declara, y no hay forma de que la lista
     * comun se recorra dos veces. */
    blender::Vector<const char *> modes;
    for (const ModeTools &mode_tools : toolbar->modes) {
      if (mode_tools.mode != nullptr) {
        modes.append(mode_tools.mode);
      }
    }
    std::sort(modes.begin(), modes.end(), [](const char *a, const char *b) {
      return strcmp(a, b) < 0;
    });
    if (modes.is_empty()) {
      modes.append(nullptr);
    }

    for (const char *mode : modes) {
      const blender::Vector<const ToolDecl *> tools = tools_for_space_mode(C, *toolbar, mode);
      Section section;
      char header[256];
      SNPRINTF(header,
               "SPACE %s MODE %s tools=%d",
               space_type_identifier(toolbar->space_type),
               mode != nullptr ? mode : "None",
               int(tools.size()));
      section.header = header;
      for (const ToolDecl *tool : tools) {
        section.lines.append(tool_line(*tool));
      }
      out.append(std::move(section));
    }
  }
  return out;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volcado de la activacion
 *
 * Mismo formato que `tests/flipendo/toolsystem/dump_activation_gui.py`, que lo saco
 * interceptando `tool.setup()` en el Python real. Las dos reglas que hacen el
 * resultado determinista son las mismas aqui: la memoria de grupos se vacia antes de
 * cada activacion, y para activar como reserva la herramienta "activa" es la primera
 * del modo.
 * \{ */

static const char *or_dash(const char *s)
{
  return (s != nullptr && s[0] != '\0') ? s : "-";
}

static std::string activation_line(const ActivationArgs &a)
{
  std::string options;
  if (a.options & TOOL_OPTION_KEYMAP_FALLBACK) {
    options += "KEYMAP_FALLBACK";
  }
  if (a.options & TOOL_OPTION_USE_BRUSHES) {
    options += options.empty() ? "USE_BRUSHES" : ",USE_BRUSHES";
  }
  char buf[1024];
  SNPRINTF(buf,
           "index=%d keymap=%s cursor=%s options=%s gizmo=%s brush_type=%s data_block=%s op=%s "
           "idname_fallback=%s keymap_fallback=%s",
           a.index,
           a.keymap,
           a.cursor,
           options.empty() ? "-" : options.c_str(),
           or_dash(a.gizmo_group),
           a.brush_type,
           or_dash(a.data_block),
           or_dash(a.op),
           or_dash(a.idname_fallback),
           or_dash(a.keymap_fallback.c_str()));
  return buf;
}

static std::string gizmo_props_repr(const ToolDecl &tool)
{
  if (tool.gizmo_properties.is_empty()) {
    return "-";
  }
  std::string out;
  for (const GizmoProp &prop : tool.gizmo_properties) {
    char buf[128];
    /* `%g`, como el Python: 75.0 sale "75". */
    SNPRINTF(buf, "%s=%g", prop.prop, double(prop.number));
    out += out.empty() ? "" : ",";
    out += buf;
  }
  return out;
}

bool dump_activation_native(const bContext *C, const char *filepath)
{
  FILE *fp = BLI_fopen(filepath, "w");
  if (fp == nullptr) {
    fprintf(stderr, "No se pudo abrir '%s' para escribir.\n", filepath);
    return false;
  }
  int lines = 0;
  for (const SpaceMode &sm : space_modes_ordered()) {
    const ToolbarDecl &toolbar = *sm.toolbar;
    const char *label = sm.mode != nullptr ? sm.mode : "None";
    const char *space = space_type_identifier(toolbar.space_type);

    blender::Vector<std::pair<const ToolDecl *, int>> flat;
    for (const ToolGroupView &group : tools_unexpanded_for_space_mode(C, toolbar, sm.mode)) {
      for (const int i : group.tools.index_range()) {
        flat.append({group.tools[i], i});
      }
    }
    if (flat.is_empty()) {
      continue;
    }
    const char *active = flat[0].first->idname;

    for (const auto &[tool, index] : flat) {
      group_active_clear(toolbar.space_type);
      ActivationArgs args;
      if (!activation_compute(C, toolbar, sm.mode, *tool, index, false, "", "", args)) {
        fprintf(fp, "ACT %s %s %s ERROR\n", space, label, tool->idname);
        lines++;
        continue;
      }
      const bool draw_cursor = tool->draw_cursor != nullptr ||
                               (tool->pending & TOOL_PENDING_DRAW_CURSOR);
      fprintf(fp,
              "ACT %s %s %s %s gizmo_props=%s draw_cursor=%s\n",
              space,
              label,
              tool->idname,
              activation_line(args).c_str(),
              gizmo_props_repr(*tool).c_str(),
              draw_cursor ? "si" : "no");
      lines++;
    }

    for (const ToolDecl *sub : fallback_group_tools(C, toolbar, sm.mode)) {
      group_active_clear(toolbar.space_type);
      ActivationArgs args;
      if (!activation_compute(C, toolbar, sm.mode, *sub, 0, true, active, "", args)) {
        fprintf(fp, "FALLBACK %s %s %s ERROR\n", space, label, sub->idname);
        lines++;
        continue;
      }
      fprintf(fp,
              "FALLBACK %s %s %s activa=%s %s\n",
              space,
              label,
              sub->idname,
              active,
              activation_line(args).c_str());
      lines++;
    }
  }
  fclose(fp);
  group_active_clear(SPACE_VIEW3D);
  printf("ACTIVATION_DUMP_NATIVE_OK %d\n", lines);
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volcado de las consultas
 *
 * Mismo formato que `tests/flipendo/toolsystem/dump_queries_gui.py`. Tiene que correr
 * en modo grafico: las descripciones salen de los keymaps de usuario.
 * \{ */

bool dump_queries_native(const bContext *C, const char *filepath)
{
  FILE *fp = BLI_fopen(filepath, "w");
  if (fp == nullptr) {
    fprintf(stderr, "No se pudo abrir '%s' para escribir.\n", filepath);
    return false;
  }
  int lines = 0;
  for (const SpaceMode &sm : space_modes_ordered()) {
    const ToolbarDecl &toolbar = *sm.toolbar;
    const char *label = sm.mode != nullptr ? sm.mode : "None";
    const char *space = space_type_identifier(toolbar.space_type);
    for (const ToolDecl *tool : tools_for_space_mode(C, toolbar, sm.mode)) {
      std::string group;
      for (const blender::StringRefNull id :
           tool_group_idnames_in(C, toolbar, sm.mode, tool->idname, true))
      {
        group += group.empty() ? "" : ",";
        group += id;
      }
      const std::string desc = tool_description_in(C, toolbar, sm.mode, tool->idname, true);
      fprintf(fp,
              "Q %s %s %s label=%s group=%s desc=%s\n",
              space,
              label,
              tool->idname,
              tool->label,
              group.empty() ? "-" : group.c_str(),
              py_repr(desc.c_str()).c_str());
      lines++;
    }
  }
  fclose(fp);
  printf("QUERIES_DUMP_NATIVE_OK %d\n", lines);
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volcado y comprobacion
 * \{ */

bool dump_native(const bContext *C, const char *filepath)
{
  FILE *fp = BLI_fopen(filepath, "w");
  if (fp == nullptr) {
    fprintf(stderr, "No se pudo abrir '%s' para escribir.\n", filepath);
    return false;
  }
  for (const Section &section : sections_build(C)) {
    fprintf(fp, "%s\n", section.header.c_str());
    for (const std::string &line : section.lines) {
      fprintf(fp, "%s\n", line.c_str());
    }
  }
  fclose(fp);
  printf("TOOLS_DUMP_NATIVE_OK %s\n", filepath);
  return true;
}

/** La clave de una seccion: "SPACE X MODE Y", sin la cuenta. */
static std::string section_key(const std::string &header)
{
  const size_t pos = header.rfind(" tools=");
  return pos == std::string::npos ? header : header.substr(0, pos);
}

bool check_native(const bContext *C, const char *baseline_filepath)
{
  size_t baseline_size = 0;
  char *baseline_text = static_cast<char *>(BLI_file_read_text_as_mem(baseline_filepath, 0, &baseline_size));
  if (baseline_text == nullptr) {
    fprintf(stderr, "No se pudo leer la linea base '%s'.\n", baseline_filepath);
    return false;
  }

  /* La linea base, troceada en secciones por su cabecera. */
  blender::Map<std::string, Section> baseline;
  blender::Vector<std::string> baseline_order;
  {
    std::string current;
    const std::string text(baseline_text, baseline_size);
    size_t start = 0;
    while (start <= text.size()) {
      const size_t end = text.find('\n', start);
      const std::string line = text.substr(start, (end == std::string::npos ? text.size() : end) - start);
      if (!line.empty()) {
        if (line.rfind("SPACE ", 0) == 0) {
          Section section;
          section.header = line;
          current = section_key(line);
          baseline_order.append(current);
          baseline.add_overwrite(current, std::move(section));
        }
        else if (!current.empty()) {
          baseline.lookup(current).lines.append(line);
        }
      }
      if (end == std::string::npos) {
        break;
      }
      start = end + 1;
    }
  }
  MEM_freeN(baseline_text);

  const blender::Vector<Section> native = sections_build(C);

  int differences = 0;
  blender::Vector<std::string> checked;
  for (const Section &section : native) {
    const std::string key = section_key(section.header);
    checked.append(key);
    const Section *expected = baseline.lookup_ptr(key);
    if (expected == nullptr) {
      printf("SOBRA   %s (no esta en la linea base)\n", section.header.c_str());
      differences++;
      continue;
    }
    if (expected->header != section.header) {
      printf("CUENTA  %s\n  esperado: %s\n", section.header.c_str(), expected->header.c_str());
      differences++;
    }
    const int64_t n = std::max(expected->lines.size(), section.lines.size());
    for (const int64_t i : blender::IndexRange(n)) {
      const std::string *a = i < expected->lines.size() ? &expected->lines[i] : nullptr;
      const std::string *b = i < section.lines.size() ? &section.lines[i] : nullptr;
      if (a != nullptr && b != nullptr && *a == *b) {
        continue;
      }
      printf("DIFIERE %s linea %d\n  python: %s\n  nativo: %s\n",
             key.c_str(),
             int(i + 1),
             a != nullptr ? a->c_str() : "(no hay)",
             b != nullptr ? b->c_str() : "(no hay)");
      differences++;
    }
  }

  int pending = 0;
  for (const std::string &key : baseline_order) {
    if (!checked.contains(key)) {
      pending++;
    }
  }

  printf("\nHerramientas: %d secciones comprobadas, %d diferencias, %d secciones sin trasladar.\n",
         int(checked.size()),
         differences,
         pending);
  if (pending != 0) {
    printf("Pendientes:\n");
    for (const std::string &key : baseline_order) {
      if (!checked.contains(key)) {
        printf("  %s\n", key.c_str());
      }
    }
  }

  /* La deuda se lista aqui a proposito: es la parte de la migracion que la linea base
   * NO puede detectar, porque sus nueve campos no incluyen ni los ajustes ni el dibujo
   * sobre la vista. Si no saliera por aqui, se perderia sin que nada avisara. */
  struct {
    int flag;
    const char *titulo;
  } deudas[] = {
      {TOOL_PENDING_SETTINGS, "Ajustes sin trasladar"},
      {TOOL_PENDING_DRAW_CURSOR, "Dibujo sobre la vista sin trasladar"},
  };
  for (const auto &deuda : deudas) {
    const blender::Vector<blender::StringRefNull> lista = pending_list(deuda.flag);
    if (lista.is_empty()) {
      continue;
    }
    printf("\n%s (%d herramientas):\n", deuda.titulo, int(lista.size()));
    for (const blender::StringRefNull idname : lista) {
      printf("  %s\n", idname.c_str());
    }
  }

  return differences == 0;
}

/** \} */

}  // namespace flipendo::toolsystem
