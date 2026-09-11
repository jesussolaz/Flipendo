/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spnode
 *
 * Ver FL_node_menus.hh. Los tres menus que el keymap nativo abre por nombre en
 * el editor de nodos: `NODE_MT_add` (Shift-A), `NODE_MT_view_pie` (acento
 * grave) y `NODE_MT_context_menu` (boton derecho).
 *
 * UNA DECISION, COMO LA DE `is_extended()`
 * ----------------------------------------
 * El `NODE_MT_add` de Python termina con una rama para las **categorias de nodos
 * heredadas**:
 *
 *     elif nodeitems_utils.has_node_categories(context):
 *         nodeitems_utils.draw_node_categories_menu(self, context)
 *
 * Esa API (`scripts/modules/nodeitems_utils.py`) es el registro de categorias de
 * nodos para **complementos**: solo tiene contenido si un addon de Python ha
 * llamado a `register_node_categories()`. En el Flipendo de destino no hay
 * interprete, luego no hay addons, luego `has_node_categories()` es
 * estructuralmente falso y esa rama no se puede alcanzar. Se deja fuera, igual
 * que la rama «extendido» de `VIEW3D_MT_add`, y queda escrito aqui y en la
 * politica: es una decision, no un olvido.
 *
 * Los cuatro arboles de serie (geometria, composicion, sombreado y textura)
 * siguen yendo por `uiItemMContents()`, que es lo que hace `menu_contents`.
 */

#include <optional>

#include "BLI_listbase.h"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "DNA_node_types.h"
#include "DNA_space_types.h"

#include "BKE_node.hh"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_types.hh"

#include "FL_ui_registry.hh"

#include "FL_node_menus.hh"

namespace blender::ed::space_node {

static void add_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  /* Igual que en `VIEW3D_MT_add`: si se llega desde la busqueda, el Python
   * ofrece primero su propia busqueda dentro del menu. */
  if (uiLayoutGetOperatorContext(layout) == WM_OP_EXEC_REGION_WIN) {
    uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);
    PointerRNA props = layout->op("WM_OT_search_single_menu", IFACE_("Search..."), ICON_VIEWZOOM);
    if (props.data) {
      RNA_string_set(&props, "menu_idname", "NODE_MT_add");
    }
    layout->separator();
  }

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);

  const SpaceNode *snode = CTX_wm_space_node(C);
  if (snode == nullptr) {
    return;
  }
  const blender::StringRef tree_type = snode->tree_idname;

  if (tree_type == "GeometryNodeTree") {
    uiItemMContents(layout, "NODE_MT_geometry_node_add_all");
  }
  else if (tree_type == "CompositorNodeTree") {
    uiItemMContents(layout, "NODE_MT_compositor_node_add_all");
  }
  else if (tree_type == "ShaderNodeTree") {
    uiItemMContents(layout, "NODE_MT_shader_node_add_all");
  }
  else if (tree_type == "TextureNodeTree") {
    uiItemMContents(layout, "NODE_MT_texture_node_add_all");
  }
  /* La rama de `nodeitems_utils` no se escribe: ver la cabecera del fichero. */
}

static void view_pie_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  pie.op("NODE_OT_view_all", std::nullopt, ICON_NONE);
  pie.op("NODE_OT_view_selected", std::nullopt, ICON_ZOOM_SELECTED);
}


/* -------------------------------------------------------------------- */
/** \name NODE_MT_context_menu
 * \{ */

static void context_menu_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  const SpaceNode *snode = CTX_wm_space_node(C);
  if (snode == nullptr) {
    return;
  }
  /* `len(snode.path) > 1`. */
  const bool is_nested = BLI_listbase_count(&snode->treepath) > 1;
  const bool is_geometrynodes = blender::StringRef(snode->tree_idname) == "GeometryNodeTree";
  bNodeTree *group = snode->edittree;

  const blender::Vector<PointerRNA> selected_nodes = CTX_data_collection_get(C, "selected_nodes");
  const int selected_nodes_len = selected_nodes.size();
  PointerRNA active_node = CTX_data_pointer_get_type(C, "active_node", &RNA_Node);

  /* Sin nodos seleccionados. */
  if (selected_nodes_len == 0) {
    uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);
    layout->menu("NODE_MT_add", std::nullopt, ICON_ADD);
    layout->op("NODE_OT_clipboard_paste", IFACE_("Paste"), ICON_PASTEDOWN);

    layout->separator();

    layout->op("NODE_OT_find_node", IFACE_("Find..."), ICON_VIEWZOOM);

    layout->separator();

    if (is_geometrynodes) {
      uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);
      PointerRNA props = layout->op("NODE_OT_select", IFACE_("Clear Viewer"), ICON_HIDE_ON);
      if (props.data) {
        RNA_boolean_set(&props, "clear_viewer", true);
      }
    }

    layout->op("NODE_OT_links_cut", std::nullopt, ICON_NONE);
    layout->op("NODE_OT_links_mute", std::nullopt, ICON_NONE);

    if (is_nested) {
      layout->separator();

      layout->op("NODE_OT_tree_path_parent", IFACE_("Exit Group"), ICON_FILE_PARENT);
    }

    return;
  }

  if (is_geometrynodes) {
    uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);
    layout->op("NODE_OT_link_viewer", IFACE_("Link to Viewer"), ICON_HIDE_OFF);

    layout->separator();
  }

  layout->op("NODE_OT_clipboard_copy", IFACE_("Copy"), ICON_COPYDOWN);
  layout->op("NODE_OT_clipboard_paste", IFACE_("Paste"), ICON_PASTEDOWN);

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);
  layout->op("NODE_OT_duplicate_move", std::nullopt, ICON_DUPLICATE);

  layout->separator();

  layout->op("NODE_OT_delete", std::nullopt, ICON_X);
  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_REGION_WIN);
  layout->op("NODE_OT_delete_reconnect", IFACE_("Dissolve"), ICON_NONE);

  if (selected_nodes_len > 1) {
    layout->separator();

    PointerRNA props = layout->op("NODE_OT_link_make", std::nullopt, ICON_NONE);
    if (props.data) {
      RNA_boolean_set(&props, "replace", false);
    }
    props = layout->op("NODE_OT_link_make", IFACE_("Make and Replace Links"), ICON_NONE);
    if (props.data) {
      RNA_boolean_set(&props, "replace", true);
    }
    layout->op("NODE_OT_links_detach", std::nullopt, ICON_NONE);
  }

  layout->separator();

  /* `group.bl_use_group_interface` es `!typeinfo->no_group_interface`. */
  const bool use_group_interface = group != nullptr && group->typeinfo != nullptr &&
                                   group->typeinfo->no_group_interface == 0;
  if (use_group_interface) {
    layout->op("NODE_OT_group_make", IFACE_("Make Group"), ICON_NODETREE);
    layout->op("NODE_OT_group_insert", IFACE_("Insert Into Group"), ICON_NONE);

    const bool active_is_group = active_node.data != nullptr &&
                                 static_cast<const bNode *>(active_node.data)->type_legacy ==
                                     NODE_GROUP;
    if (active_is_group) {
      PointerRNA props = layout->op("NODE_OT_group_edit", std::nullopt, ICON_NONE);
      if (props.data) {
        RNA_boolean_set(&props, "exit", false);
      }
      layout->op("NODE_OT_group_ungroup", IFACE_("Ungroup"), ICON_NONE);
    }

    if (is_nested) {
      layout->op("NODE_OT_tree_path_parent", IFACE_("Exit Group"), ICON_FILE_PARENT);
    }

    layout->separator();
  }

  layout->op("NODE_OT_join", IFACE_("Join in New Frame"), ICON_NONE);
  layout->op("NODE_OT_detach", IFACE_("Remove from Frame"), ICON_NONE);

  layout->separator();

  PointerRNA props = layout->op("WM_OT_call_panel", IFACE_("Rename..."), ICON_NONE);
  if (props.data) {
    RNA_string_set(&props, "name", "TOPBAR_PT_name");
    RNA_boolean_set(&props, "keep_open", false);
  }

  layout->separator();

  layout->menu("NODE_MT_context_menu_select_menu", std::nullopt, ICON_NONE);
  layout->menu("NODE_MT_context_menu_show_hide_menu", std::nullopt, ICON_NONE);

  if (active_node.data) {
    layout->separator();
    props = layout->op("WM_OT_doc_view_manual", IFACE_("Online Manual"), ICON_URL);
    if (props.data) {
      char *doc_id = RNA_string_get_alloc(&active_node, "bl_idname", nullptr, 0, nullptr);
      if (doc_id) {
        RNA_string_set(&props, "doc_id", doc_id);
        MEM_freeN(doc_id);
      }
    }
  }
}

/** \} */

static const flipendo::MenuDecl node_menus[] = {
    {
        /*idname*/ "NODE_MT_add",
        /*label*/ N_("Add"),
        /*description*/ nullptr,
        /*translation_context*/ BLT_I18NCONTEXT_OPERATOR_DEFAULT,
        /*draw*/ add_draw,
        /*poll*/ nullptr,
        /*flag*/ int(MenuTypeFlag::SearchOnKeyPress),
    },
    {
        /*idname*/ "NODE_MT_context_menu",
        /*label*/ N_("Node"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ context_menu_draw,
    },
    {
        /*idname*/ "NODE_MT_view_pie",
        /*label*/ N_("View"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ view_pie_draw,
    },
};

void node_menus_register()
{
  flipendo::menus_register({node_menus, ARRAY_SIZE(node_menus)});
}

}  // namespace blender::ed::space_node
