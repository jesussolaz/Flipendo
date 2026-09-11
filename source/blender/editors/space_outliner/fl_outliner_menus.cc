/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 *
 * Ver FL_outliner_menus.hh. `OUTLINER_MT_view_pie`, el radial de la tecla del
 * acento grave en el esquema, que el keymap nativo abre por nombre.
 *
 * `OUTLINER_MT_context_menu` — el otro de la lista — se queda por ahora: llama a
 * `OUTLINER_MT_collection_new.draw_without_context_menu()`, que es un metodo de
 * clase de Python distinto de `draw()`, asi que `uiItemMContents()` no sirve y
 * hay que reescribir tambien ese trozo. Anotado en la politica.
 */

#include <optional>

#include "BLI_utildefines.h"

#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "FL_ui_registry.hh"

#include "FL_outliner_menus.hh"

namespace blender::ed::outliner {

static void view_pie_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  pie.op("OUTLINER_OT_show_hierarchy", std::nullopt, ICON_NONE);
  pie.op("OUTLINER_OT_show_active", std::nullopt, ICON_ZOOM_SELECTED);
}

static const flipendo::MenuDecl outliner_menus[] = {
    {
        /*idname*/ "OUTLINER_MT_view_pie",
        /*label*/ N_("View"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ view_pie_draw,
    },
};

void outliner_menus_register()
{
  flipendo::menus_register({outliner_menus, ARRAY_SIZE(outliner_menus)});
}

}  // namespace blender::ed::outliner
