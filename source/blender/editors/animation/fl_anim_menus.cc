/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edanimation
 *
 * `ANIM_MT_keyframe_insert_pie`, el radial de insertar clave que el keymap
 * nativo abre con K cuando el usuario activa esa preferencia. Sustituye a la
 * clase de `scripts/startup/bl_ui/anim.py`. Ver
 * `politicas/MENUS-DEL-KEYMAP-A-CPP.md`.
 *
 * La animacion no es un editor, asi que el alta cuelga de
 * `ED_operatortypes_anim()`.
 *
 * Ojo con el `type` de `anim.keyframe_insert_by_name`: **no es una enumeracion**,
 * es una cadena con el nombre del juego de claves ("Location", "Scaling",
 * "Available", "Rotation"), y con esos valores exactos — «Scaling», no «Scale».
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

#include "ED_anim_api.hh"

#include "FL_anim_menus.hh"

namespace blender::ed::animation {

static void keyframe_insert_item(uiLayout &pie, const char *text, const char *type)
{
  PointerRNA props = pie.op("ANIM_OT_keyframe_insert_by_name", IFACE_(text), ICON_NONE);
  if (props.data) {
    RNA_string_set(&props, "type", type);
  }
}

static void keyframe_insert_pie_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  keyframe_insert_item(pie, N_("Location"), "Location");
  keyframe_insert_item(pie, N_("Scale"), "Scaling");
  keyframe_insert_item(pie, N_("Available"), "Available");
  keyframe_insert_item(pie, N_("Rotation"), "Rotation");
}

static const flipendo::MenuDecl anim_menus[] = {
    {
        /*idname*/ "ANIM_MT_keyframe_insert_pie",
        /*label*/ N_("Keyframe Insert Pie"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ keyframe_insert_pie_draw,
    },
};

void menus_register()
{
  flipendo::menus_register({anim_menus, ARRAY_SIZE(anim_menus)});
}

}  // namespace blender::ed::animation
