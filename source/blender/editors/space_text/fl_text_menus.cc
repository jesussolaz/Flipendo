/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sptext
 *
 * Ver FL_text_menus.hh. `TEXT_MT_context_menu`, el del boton derecho en el
 * editor de texto, que el keymap nativo abre por nombre.
 */

#include <optional>

#include "BLI_utildefines.h"

#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_types.hh"

#include "FL_ui_registry.hh"

#include "FL_text_menus.hh"

namespace blender::ed::text {

static void context_menu_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);

  layout->op("TEXT_OT_cut", std::nullopt, ICON_NONE);
  layout->op("TEXT_OT_copy", std::nullopt, ICON_COPYDOWN);
  layout->op("TEXT_OT_paste", std::nullopt, ICON_PASTEDOWN);
  layout->op("TEXT_OT_duplicate_line", std::nullopt, ICON_NONE);

  layout->separator();

  PointerRNA props = layout->op("TEXT_OT_move_lines", IFACE_("Move Line(s) Up"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "direction", "UP");
  }
  props = layout->op("TEXT_OT_move_lines", IFACE_("Move Line(s) Down"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "direction", "DOWN");
  }

  layout->separator();

  layout->op("TEXT_OT_indent", std::nullopt, ICON_NONE);
  layout->op("TEXT_OT_unindent", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("TEXT_OT_comment_toggle", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("TEXT_OT_autocomplete", std::nullopt, ICON_NONE);
}

static const flipendo::MenuDecl text_menus[] = {
    {
        /*idname*/ "TEXT_MT_context_menu",
        /*label*/ "",
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ context_menu_draw,
    },
};

void text_menus_register()
{
  flipendo::menus_register({text_menus, ARRAY_SIZE(text_menus)});
}

}  // namespace blender::ed::text
