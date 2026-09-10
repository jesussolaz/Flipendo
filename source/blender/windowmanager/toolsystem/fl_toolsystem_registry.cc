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

#include <cctype>
#include <cstring>
#include <memory>
#include <string>

#include "BKE_context.hh"

#include "BLI_index_range.hh"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"
#include "BLI_string_ref.hh"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BLT_translation.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"
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
    &toolbar_view3d,
    &toolbar_image,
    &toolbar_node,
    &toolbar_sequencer,
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
 * Las herramientas de una enumeracion RNA, generadas una sola vez.
 *
 * El resultado se cachea porque la API devuelve punteros a `ToolDecl`, y esos tienen
 * que seguir siendo validos despues de volver. La reserva EXACTA de los dos vectores
 * es la que lo garantiza: si crecieran, moverian su contenido y los punteros — y los
 * `c_str()` de los idnames — se quedarian colgando.
 */
struct GeneratedTools {
  /** Dos por herramienta: su idname y su icono, que son los unicos campos calculados.
   * El resto apunta a las cadenas de la enumeracion RNA, que viven para siempre. */
  blender::Vector<std::string> strings;
  blender::Vector<ToolDecl> tools;
  blender::Vector<const ToolDecl *> pointers;
};

static blender::Span<const ToolDecl *> generated_tools(const EnumToolsDecl &decl)
{
  static blender::Map<const EnumToolsDecl *, std::unique_ptr<GeneratedTools>> cache;

  if (const std::unique_ptr<GeneratedTools> *found = cache.lookup_ptr(&decl)) {
    return (*found)->pointers;
  }

  auto generated = std::make_unique<GeneratedTools>();

  StructRNA *srna = decl.type_fn != nullptr ? decl.type_fn() : nullptr;
  PropertyRNA *prop = srna != nullptr ? RNA_struct_type_find_property(srna, decl.attr) : nullptr;
  if (prop == nullptr) {
    fprintf(stderr,
            "Herramientas: no existe la propiedad '%s' de la que generar '%s*'.\n",
            decl.attr != nullptr ? decl.attr : "(null)",
            decl.idname_prefix != nullptr ? decl.idname_prefix : "");
    GeneratedTools *raw = generated.get();
    cache.add_new(&decl, std::move(generated));
    return raw->pointers;
  }

  /* La tabla ESTATICA de la enumeracion, y ademas pedida como tal.
   *
   * El Python lee `enum_items_static_ui`, que por dentro pide los dinamicos... pero
   * pasando contexto nulo, asi que la funcion del enum no puede correr y acaba cayendo
   * en la misma tabla estatica. Pedirla directamente es equivalente y ademas es lo
   * unico seguro aqui: por el camino dinamico, `RNA_property_enum_items_ex` desreferencia
   * el `PointerRNA`, y aqui no hay ninguno que dar. `ParticleEdit.tool` SI tiene funcion
   * de enum (`rna_ParticleEdit_tool_itemf`), asi que no es un caso hipotetico: pedirlo
   * mal reventaba nada mas arrancar.
   *
   * La tabla estatica de la unica enumeracion en juego no lleva separadores ni
   * encabezados, asi que el filtro de abajo no descarta nada hoy; esta para que siga
   * siendo correcto si alguna vez los lleva. */
  const EnumPropertyItem *items = nullptr;
  int items_num = 0;
  bool items_free = false;
  RNA_property_enum_items_ex(nullptr, nullptr, prop, true, &items, &items_num, &items_free);

  /* Primero se cuenta, para reservar exacto. Ver el comentario de GeneratedTools. */
  int keep_num = 0;
  for (const int i : blender::IndexRange(items_num)) {
    const EnumPropertyItem &item = items[i];
    if (decl.use_separators) {
      if (item.name == nullptr || item.name[0] == '\0') {
        /* Separador de la barra: no es una herramienta. */
        continue;
      }
      if (item.identifier == nullptr || item.identifier[0] == '\0') {
        /* Encabezado: no se muestra. */
        continue;
      }
    }
    keep_num++;
  }
  generated->strings.reserve(keep_num * 2);
  generated->tools.reserve(keep_num);
  generated->pointers.reserve(keep_num);

  for (const int i : blender::IndexRange(items_num)) {
    const EnumPropertyItem &item = items[i];
    if (decl.use_separators) {
      if (item.name == nullptr || item.name[0] == '\0') {
        continue;
      }
      if (item.identifier == nullptr || item.identifier[0] == '\0') {
        continue;
      }
    }

    /* El icono es el IDENTIFICADOR en minusculas; el idname, el NOMBRE. No son lo
     * mismo: 'COMB' da el icono 'brush.particle.comb' y el idname
     * 'builtin_brush.Comb'. */
    std::string icon = std::string(decl.icon_prefix);
    for (const char *c = item.identifier; *c != '\0'; c++) {
      icon += char(tolower(*c));
    }

    generated->strings.append(std::string(decl.idname_prefix) + item.name);
    generated->strings.append(std::move(icon));

    ToolDecl tool{};
    tool.idname = generated->strings[generated->strings.size() - 2].c_str();
    tool.label = item.name;
    tool.description = item.description;
    tool.icon = generated->strings.last().c_str();
    tool.cursor = decl.cursor;
    tool.data_block = item.identifier;
    tool.options = decl.options;
    generated->tools.append(tool);
  }
  for (ToolDecl &tool : generated->tools) {
    generated->pointers.append(&tool);
  }

  /* Las cadenas de `label`, `description` y `data_block` apuntan a los elementos, asi
   * que si fueran temporales habria que copiarlas. Con la tabla estatica nunca lo son;
   * la comprobacion esta por si eso cambia. */
  BLI_assert(!items_free);
  UNUSED_VARS_NDEBUG(items_free);

  GeneratedTools *raw = generated.get();
  cache.add_new(&decl, std::move(generated));
  return raw->pointers;
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
    if (entry.generated != nullptr) {
      for (const ToolDecl *tool : generated_tools(*entry.generated)) {
        out.append(tool);
      }
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

/**
 * Las entradas tal como las recorre el Python ANTES de aplanar. Ver `ToolGroupView`.
 */
static void entries_unexpanded(const bContext *C,
                               const blender::Span<ToolEntry> entries,
                               blender::Vector<ToolGroupView> &out)
{
  for (const ToolEntry &entry : entries) {
    if (entry.poll != nullptr && !entry.poll(C)) {
      continue;
    }
    if (entry.generated != nullptr) {
      const blender::Span<const ToolDecl *> tools = generated_tools(*entry.generated);
      for (const int i : tools.index_range()) {
        out.append({tools.slice(i, 1)});
      }
      continue;
    }
    if (entry.tools.is_empty()) {
      /* Separador. */
      continue;
    }
    out.append({entry.tools});
  }
}

blender::Vector<ToolGroupView> tools_unexpanded_for_space_mode(const bContext *C,
                                                               const ToolbarDecl &toolbar,
                                                               const char *mode)
{
  blender::Vector<ToolGroupView> out;
  /* Mismo orden que `tools_for_space_mode`: comunes y luego las del modo. */
  for (const ModeTools &mode_tools : toolbar.modes) {
    if (mode_tools.mode == nullptr) {
      entries_unexpanded(C, mode_tools.entries, out);
    }
  }
  if (mode != nullptr) {
    for (const ModeTools &mode_tools : toolbar.modes) {
      if (mode_tools.mode != nullptr && STREQ(mode_tools.mode, mode)) {
        entries_unexpanded(C, mode_tools.entries, out);
      }
    }
  }
  return out;
}

/* -------------------------------------------------------------------- */
/** \name Consultas
 * \{ */

static const char *mode_for_toolbar(const bContext *C, const ToolbarDecl &toolbar)
{
  return toolbar.mode_from_context != nullptr ? toolbar.mode_from_context(C) : nullptr;
}

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

const ToolDecl *tool_find_in(const bContext *C,
                             const ToolbarDecl &toolbar,
                             const char *mode,
                             const blender::StringRefNull idname)
{
  for (const ToolDecl *tool : tools_for_space_mode(C, toolbar, mode)) {
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
 * `_tool_get_group_by_id`: el grupo que contiene la herramienta, en CUALQUIER posicion
 * (no solo como lider). El primero que la contenga gana, igual que en el Python.
 */
blender::Vector<blender::StringRefNull> tool_group_idnames_in(const bContext *C,
                                                              const ToolbarDecl &toolbar,
                                                              const char *mode,
                                                              const blender::StringRefNull idname,
                                                              const bool coerce)
{
  for (const ToolGroupView &group : tools_unexpanded_for_space_mode(C, toolbar, mode)) {
    for (const ToolDecl *tool : group.tools) {
      if (idname != tool->idname) {
        continue;
      }
      if (!group.is_group() && !coerce) {
        return {};
      }
      blender::Vector<blender::StringRefNull> out;
      for (const ToolDecl *member : group.tools) {
        out.append(blender::StringRefNull(member->idname));
      }
      return out;
    }
  }
  return {};
}

blender::Vector<blender::StringRefNull> tool_group_idnames_for_id(const bContext *C,
                                                                  const int space_type,
                                                                  const blender::StringRefNull idname,
                                                                  const bool coerce)
{
  const ToolbarDecl *toolbar = toolbar_for_space(space_type);
  if (toolbar == nullptr) {
    return {};
  }
  return tool_group_idnames_in(C, *toolbar, mode_for_toolbar(C, *toolbar), idname, coerce);
}

/**
 * `_keymap_from_item`: el keymap del USUARIO con ese nombre.
 *
 * Se busca solo por nombre, en cualquier espacio, porque asi lo hace el Python
 * (`keyconfigs.user.keymaps.get(nombre)`). Es la configuracion del usuario y no la de
 * por defecto: es la que tiene sus cambios de teclas.
 */
static wmKeyMap *user_keymap_by_name(const bContext *C, const char *name)
{
  if (name == nullptr || name[0] == '\0') {
    return nullptr;
  }
  wmWindowManager *wm = CTX_wm_manager(C);
  if (wm == nullptr || wm->userconf == nullptr) {
    return nullptr;
  }
  LISTBASE_FOREACH (wmKeyMap *, km, &wm->userconf->keymaps) {
    if (STREQ(km->idname, name)) {
      return km;
    }
  }
  return nullptr;
}

/**
 * `description_from_id` (`space_toolsystem_common.py:1136`), en el mismo orden: la
 * descripcion calculada, la literal y, si no hay ninguna, la del operador.
 *
 * El operador sale del campo `op` o, si no lo tiene, del primer atajo ACTIVO del keymap
 * de la herramienta. Es lo que hace que la mayoria de herramientas tengan tooltip sin
 * haber escrito ninguno.
 */
std::string tool_description_in(const bContext *C,
                                const ToolbarDecl &toolbar,
                                const char *mode,
                                const blender::StringRefNull idname,
                                const bool use_operator)
{
  const ToolDecl *item = tool_find_in(C, toolbar, mode, idname);
  if (item == nullptr) {
    return "";
  }
  if (item->description_fn != nullptr) {
    return item->description_fn(C, user_keymap_by_name(C, item->keymap_name));
  }
  if (item->description != nullptr) {
    return TIP_(item->description);
  }
  if (use_operator) {
    const char *op = item->op;
    if (op == nullptr) {
      if (const wmKeyMap *km = user_keymap_by_name(C, item->keymap_name)) {
        LISTBASE_FOREACH (const wmKeyMapItem *, kmi, &km->items) {
          if ((kmi->flag & KMI_INACTIVE) == 0) {
            op = kmi->idname;
            break;
          }
        }
      }
    }
    if (op != nullptr) {
      if (const wmOperatorType *ot = WM_operatortype_find(op, true)) {
        if (ot->description != nullptr) {
          return TIP_(ot->description);
        }
      }
    }
  }
  return "";
}

std::string tool_description_for_id(const bContext *C,
                                    const int space_type,
                                    const blender::StringRefNull idname,
                                    const bool use_operator)
{
  const ToolbarDecl *toolbar = toolbar_for_space(space_type);
  if (toolbar == nullptr) {
    return "";
  }
  return tool_description_in(C, *toolbar, mode_for_toolbar(C, *toolbar), idname, use_operator);
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

void group_active_clear(const int space_type)
{
  const std::string prefix = std::to_string(space_type) + "\n";
  blender::Vector<std::string> keys;
  for (const std::string &key : group_active_map().keys()) {
    if (key.rfind(prefix, 0) == 0) {
      keys.append(key);
    }
  }
  for (const std::string &key : keys) {
    group_active_map().remove(key);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deuda pendiente
 * \{ */

blender::Vector<blender::StringRefNull> pending_list(const int flag)
{
  blender::Vector<blender::StringRefNull> out;
  for (const ToolbarDecl *toolbar : toolbars_all()) {
    for (const ModeTools &mode_tools : toolbar->modes) {
      for (const ToolEntry &entry : mode_tools.entries) {
        for (const ToolDecl *tool : entry.tools) {
          if ((tool->pending & flag) == 0) {
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
