/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Registro del catalogo de herramientas y consultas sobre el.
 *
 * Sustituye a la parte de `bl_ui/space_toolsystem_common.py` que resuelve "que
 * herramientas hay aqui", "cual es esta" y "cual es su keymap". Ver el contrato en
 * FL_toolsystem.hpp.
 */

#include <cstring>
#include <string>

#include "BKE_context.hh"

#include "BLI_map.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"
#include "BLI_string_ref.hh"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "WM_api.hh"
#include "WM_keymap.hh"

#include "FL_toolsystem.hpp"
#include "fl_tool_defs.hh"

namespace flipendo::toolsystem {

/* -------------------------------------------------------------------- */
/** \name El catalogo
 * \{ */

/* Una barra por espacio. El orden no importa: la busqueda es por `space_type`. */
static const ToolbarDecl *g_toolbars[] = {
    &toolbar_node,
};

blender::Span<const ToolbarDecl *> toolbars_all()
{
  return blender::Span<const ToolbarDecl *>(g_toolbars, ARRAY_SIZE(g_toolbars));
}

const ToolbarDecl *toolbar_for_space(const int space_type)
{
  for (const ToolbarDecl *decl : toolbars_all()) {
    if (decl->space_type == space_type) {
      return decl;
    }
  }
  return nullptr;
}

/**
 * Anade a `out` las herramientas de una lista de entradas.
 *
 * Aplanar aqui es exactamente lo que hace `_tools_flatten` del Python: una entrada con
 * varias herramientas aporta todas, en orden, y una entrada vacia (el separador) no
 * aporta ninguna. El separador si cuenta para el DIBUJO, pero no es una herramienta y
 * no aparece en las consultas.
 */
static void entries_flatten(const bContext *C,
                            const blender::Span<ToolEntry> entries,
                            blender::Vector<const ToolDecl *> &out)
{
  for (const ToolEntry &entry : entries) {
    if (entry.poll != nullptr && !entry.poll(C)) {
      continue;
    }
    for (const ToolDecl *tool : entry.tools) {
      out.append(tool);
    }
  }
}

blender::Vector<const ToolDecl *> tools_for_space_mode(const bContext *C,
                                                       const ToolbarDecl &toolbar,
                                                       const char *mode)
{
  blender::Vector<const ToolDecl *> out;

  /* Primero las comunes del espacio, luego las del modo, como en el Python. La entrada
   * comun es la que tiene `mode == nullptr`; buscando por nombre no puede volver a
   * salir como si fuera la del modo activo, que es como el Python duplicaba el
   * catalogo de un espacio sin modos. */
  for (const ModeTools &mode_tools : toolbar.modes) {
    if (mode_tools.mode == nullptr) {
      entries_flatten(C, mode_tools.entries, out);
    }
  }
  if (mode != nullptr) {
    for (const ModeTools &mode_tools : toolbar.modes) {
      if (mode_tools.mode != nullptr && STREQ(mode_tools.mode, mode)) {
        entries_flatten(C, mode_tools.entries, out);
      }
    }
  }
  return out;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Consultas
 * \{ */

blender::Vector<const ToolDecl *> tools_for_context(const bContext *C, const int space_type)
{
  const ToolbarDecl *toolbar = toolbar_for_space(space_type);
  if (toolbar == nullptr) {
    return {};
  }
  const char *mode = toolbar->mode_from_context != nullptr ? toolbar->mode_from_context(C) :
                                                             nullptr;
  return tools_for_space_mode(C, *toolbar, mode);
}

const ToolDecl *tool_find_by_id(const bContext *C,
                                const int space_type,
                                const blender::StringRefNull idname)
{
  for (const ToolDecl *tool : tools_for_context(C, space_type)) {
    if (idname == tool->idname) {
      return tool;
    }
  }
  return nullptr;
}

const ToolDecl *tool_find_by_index(const bContext *C, const int space_type, const int index)
{
  const blender::Vector<const ToolDecl *> tools = tools_for_context(C, space_type);
  if (index < 0 || index >= tools.size()) {
    return nullptr;
  }
  return tools[index];
}

blender::StringRefNull tool_label_for_id(const bContext *C,
                                         const int space_type,
                                         const blender::StringRefNull idname)
{
  const ToolDecl *tool = tool_find_by_id(C, space_type, idname);
  return tool != nullptr ? blender::StringRefNull(tool->label) : blender::StringRefNull("");
}

/**
 * Los idnames del grupo al que pertenece una herramienta.
 *
 * Un grupo es una entrada con mas de una herramienta: comparten boton en la barra y se
 * cicla entre ellas. Si la herramienta no esta en un grupo se devuelve vacio, no una
 * lista de uno; asi el que llama distingue "suelta" de "grupo de una".
 */
blender::Vector<blender::StringRefNull> tool_group_idnames_for_id(
    const bContext *C, const int space_type, const blender::StringRefNull idname)
{
  const ToolbarDecl *toolbar = toolbar_for_space(space_type);
  if (toolbar == nullptr) {
    return {};
  }
  const char *mode = toolbar->mode_from_context != nullptr ? toolbar->mode_from_context(C) :
                                                             nullptr;

  for (const ModeTools &mode_tools : toolbar->modes) {
    const bool is_common = mode_tools.mode == nullptr;
    if (!is_common && (mode == nullptr || !STREQ(mode_tools.mode, mode))) {
      continue;
    }
    for (const ToolEntry &entry : mode_tools.entries) {
      if (entry.tools.size() <= 1) {
        continue;
      }
      if (entry.poll != nullptr && !entry.poll(C)) {
        continue;
      }
      bool found = false;
      for (const ToolDecl *tool : entry.tools) {
        if (idname == tool->idname) {
          found = true;
          break;
        }
      }
      if (found) {
        blender::Vector<blender::StringRefNull> out;
        for (const ToolDecl *tool : entry.tools) {
          out.append(blender::StringRefNull(tool->idname));
        }
        return out;
      }
    }
  }
  return {};
}

wmKeyMap *tool_keymap_for_id(const bContext *C,
                             const int space_type,
                             const blender::StringRefNull idname)
{
  const ToolDecl *tool = tool_find_by_id(C, space_type, idname);
  if (tool == nullptr || tool->keymap_name == nullptr) {
    return nullptr;
  }
  wmWindowManager *wm = CTX_wm_manager(C);
  if (wm == nullptr) {
    return nullptr;
  }
  /* Los keymaps de herramienta se registran en el espacio al que pertenecen y en la
   * region de ventana; `WM_keymap_find_all` los busca en la configuracion activa. */
  return WM_keymap_find_all(wm, tool->keymap_name, space_type, RGN_TYPE_WINDOW);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Memoria de grupo
 * \{ */

/* Por espacio y por herramienta que encabeza el grupo. Igual que el diccionario de
 * clase del Python (`space_toolsystem_common.py:527`), no se serializa: se pierde al
 * cerrar, y eso es lo que se quiere. */
static blender::Map<std::string, int> &group_active_map()
{
  static blender::Map<std::string, int> map;
  return map;
}

static std::string group_active_key(const int space_type,
                                    const blender::StringRefNull group_leader_idname)
{
  return std::to_string(space_type) + "\n" + std::string(group_leader_idname);
}

int group_active_get(const int space_type, const blender::StringRefNull group_leader_idname)
{
  return group_active_map().lookup_default(group_active_key(space_type, group_leader_idname), 0);
}

void group_active_set(const int space_type,
                      const blender::StringRefNull group_leader_idname,
                      const int index)
{
  group_active_map().add_overwrite(group_active_key(space_type, group_leader_idname), index);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deuda pendiente
 * \{ */

blender::Vector<blender::StringRefNull> settings_pending_list()
{
  blender::Vector<blender::StringRefNull> out;
  for (const ToolbarDecl *toolbar : toolbars_all()) {
    for (const ModeTools &mode_tools : toolbar->modes) {
      for (const ToolEntry &entry : mode_tools.entries) {
        for (const ToolDecl *tool : entry.tools) {
          if (!tool->settings_pending) {
            continue;
          }
          const blender::StringRefNull idname(tool->idname);
          if (!out.contains(idname)) {
            out.append(idname);
          }
        }
      }
    }
  }
  return out;
}

/** \} */

}  // namespace flipendo::toolsystem
