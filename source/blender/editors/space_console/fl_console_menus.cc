/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spconsole
 *
 * Ver FL_console_menus.hh. `CONSOLE_MT_context_menu`, el del boton derecho en la
 * consola, que el keymap nativo abre por nombre.
 *
 * Deuda: `console.copy_as_script` sigue siendo un operador de Python
 * (`scripts/modules/console_python.py`), como `console.execute`,
 * `console.autocomplete` y `console.banner`. La consola de Python sin Python es
 * una decision que no toca a este carril; el menu se migra igual porque el
 * `idname` se resuelve al dibujar.
 */

#include <optional>

#include "BLI_utildefines.h"

#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "FL_ui_registry.hh"

#include "FL_console_menus.hh"

namespace blender::ed::console {

static void context_menu_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;

  layout->op("CONSOLE_OT_clear", std::nullopt, ICON_NONE);
  layout->op("CONSOLE_OT_clear_line", std::nullopt, ICON_NONE);
  PointerRNA props = layout->op("CONSOLE_OT_delete", IFACE_("Delete Previous Word"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "type", "PREVIOUS_WORD");
  }
  props = layout->op("CONSOLE_OT_delete", IFACE_("Delete Next Word"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "type", "NEXT_WORD");
  }

  layout->separator();

  layout->op("CONSOLE_OT_copy_as_script", IFACE_("Copy as Script"), ICON_NONE);
  props = layout->op("CONSOLE_OT_copy", IFACE_("Cut"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "delete", true);
  }
  layout->op("CONSOLE_OT_copy", IFACE_("Copy"), ICON_NONE);
  layout->op("CONSOLE_OT_paste", IFACE_("Paste"), ICON_NONE);

  layout->separator();

  layout->op("CONSOLE_OT_indent", std::nullopt, ICON_NONE);
  layout->op("CONSOLE_OT_unindent", std::nullopt, ICON_NONE);

  layout->separator();

  props = layout->op("CONSOLE_OT_history_cycle", IFACE_("Backward in History"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "reverse", true);
  }
  props = layout->op("CONSOLE_OT_history_cycle", IFACE_("Forward in History"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "reverse", false);
  }

  layout->separator();

  layout->op("CONSOLE_OT_autocomplete", IFACE_("Autocomplete"), ICON_NONE);
}

static const flipendo::MenuDecl console_menus[] = {
    {
        /*idname*/ "CONSOLE_MT_context_menu",
        /*label*/ N_("Console"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ context_menu_draw,
    },
};

void console_menus_register()
{
  flipendo::menus_register({console_menus, ARRAY_SIZE(console_menus)});
}

}  // namespace blender::ed::console
