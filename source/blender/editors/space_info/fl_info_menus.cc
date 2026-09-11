/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spinfo
 *
 * Ver FL_info_menus.hh. `INFO_MT_context_menu`, el del boton derecho en el
 * registro de informes, que el keymap nativo abre por nombre.
 */

#include <optional>

#include "BLI_utildefines.h"

#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "FL_ui_registry.hh"

#include "FL_info_menus.hh"

namespace blender::ed::info {

static void context_menu_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;

  layout->op("INFO_OT_report_copy", IFACE_("Copy"), ICON_NONE);
  layout->op("INFO_OT_report_delete", IFACE_("Delete"), ICON_NONE);
}

static const flipendo::MenuDecl info_menus[] = {
    {
        /*idname*/ "INFO_MT_context_menu",
        /*label*/ N_("Info"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ context_menu_draw,
    },
};

void info_menus_register()
{
  flipendo::menus_register({info_menus, ARRAY_SIZE(info_menus)});
}

}  // namespace blender::ed::info
