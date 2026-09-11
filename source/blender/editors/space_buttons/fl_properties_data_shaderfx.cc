/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spbuttons
 *
 * Panel nativo de la pestaña Effects. Sustituye `properties_data_shaderfx.py`.
 */

#include "BLI_utildefines.h"

#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "DNA_space_types.h"

#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"

#include "FL_ui_registry.hh"

#include "buttons_intern.hh"

static void properties_data_shaderfx_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  uiItemMenuEnumO(
      layout, C, "OBJECT_OT_shaderfx_add", "type", IFACE_("Add Effect"), ICON_NONE);
  uiTemplateShaderFx(layout, const_cast<bContext *>(C));
}

void buttons_properties_data_shaderfx_register(ARegionType *art)
{
  static const flipendo::PanelDecl panels[] = {
      {
          /*idname*/ "DATA_PT_shader_fx",
          /*label*/ N_("Effects"),
          /*category*/ nullptr,
          /*context*/ "shaderfx",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ properties_data_shaderfx_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ nullptr,
          /*flag*/ PANEL_TYPE_NO_HEADER,
      },
  };
  flipendo::panels_register(art, SPACE_PROPERTIES, {panels, ARRAY_SIZE(panels)});
}
