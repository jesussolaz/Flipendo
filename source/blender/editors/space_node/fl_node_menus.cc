/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spnode
 *
 * Ver FL_node_menus.hh. Dos de los tres menus que el keymap nativo abre por
 * nombre en el editor de nodos: `NODE_MT_add` (Shift-A) y `NODE_MT_view_pie`
 * (acento grave). El tercero, `NODE_MT_context_menu`, son 100 lineas con muchas
 * ramas y va en su propia tanda.
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

#include "BLI_string_ref.hh"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "DNA_space_types.h"

#include "RNA_access.hh"

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
