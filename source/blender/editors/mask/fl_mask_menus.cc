/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmask
 *
 * `MASK_MT_add`, el menu «Anadir» de las mascaras que el keymap nativo abre con
 * Shift-A en el editor de imagen, el de clips y el secuenciador. Sustituye a la
 * clase de `scripts/startup/bl_ui/properties_mask_common.py`. Ver
 * `politicas/MENUS-DEL-KEYMAP-A-CPP.md`.
 *
 * Las mascaras no tienen editor propio —viven dentro de otros tres—, asi que el
 * alta cuelga de `ED_operatortypes_mask()`, que es lo que corre al arrancar.
 */

#include <optional>

#include "BLI_utildefines.h"

#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_types.hh"

#include "FL_ui_registry.hh"

#include "ED_mask.hh"

#include "FL_mask_menus.hh"

namespace blender::ed::mask {

static void add_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);
  layout->op("MASK_OT_primitive_circle_add", IFACE_("Circle"), ICON_MESH_CIRCLE);
  layout->op("MASK_OT_primitive_square_add", IFACE_("Square"), ICON_MESH_PLANE);
}

static const flipendo::MenuDecl mask_menus[] = {
    {
        /*idname*/ "MASK_MT_add",
        /*label*/ N_("Add"),
        /*description*/ nullptr,
        /*translation_context*/ BLT_I18NCONTEXT_OPERATOR_DEFAULT,
        /*draw*/ add_draw,
    },
};

void menus_register()
{
  flipendo::menus_register({mask_menus, ARRAY_SIZE(mask_menus)});
}

}  // namespace blender::ed::mask
