/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spbuttons
 *
 * La pestana de datos de la Rejilla, en C++ nativo. Sustituye
 * `scripts/startup/bl_ui/properties_data_lattice.py` **entera**: sus cuatro
 * paneles.
 *
 * Es la primera que usa los DOS ayudantes compartidos de `FL_properties_ui.hpp`
 * (`PropertyPanel` y `PropertiesAnimationMixin`), que es justo para lo que se
 * extrajeron.
 */

#include "DNA_key_types.h"
#include "DNA_lattice_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "FL_properties_ui.hpp"
#include "FL_ui_registry.hh"

#include "buttons_intern.hh"

static Lattice *context_lattice(const bContext *C)
{
  return static_cast<Lattice *>(CTX_data_pointer_get_type(C, "lattice", &RNA_Lattice).data);
}

static PointerRNA lattice_ptr(Lattice *lattice)
{
  return RNA_pointer_create_discrete(&lattice->id, &RNA_Lattice, lattice);
}

/** `poll` de `DataButtonsPanel`: `context.lattice`. */
static bool lattice_poll(const bContext *C, PanelType * /*pt*/)
{
  return context_lattice(C) != nullptr;
}

static void context_lattice_draw(const bContext *C, Panel *panel)
{
  Object *ob = CTX_data_active_object(C);
  uiLayout *layout = panel->layout;
  if (ob != nullptr) {
    PointerRNA ob_ptr = RNA_pointer_create_discrete(&ob->id, &RNA_Object, ob);
    uiTemplateID(layout, C, &ob_ptr, "data", nullptr, nullptr, nullptr);
  }
  else if (context_lattice(C) != nullptr) {
    SpaceProperties *space = CTX_wm_space_properties(C);
    PointerRNA space_ptr = RNA_pointer_create_discrete(
        reinterpret_cast<ID *>(CTX_wm_screen(C)), &RNA_SpaceProperties, space);
    uiTemplateID(layout, C, &space_ptr, "pin_id", nullptr, nullptr, nullptr);
  }
}

static void lattice_draw(const bContext *C, Panel *panel)
{
  Lattice *lattice = context_lattice(C);
  if (lattice == nullptr) {
    return;
  }
  PointerRNA ptr = lattice_ptr(lattice);
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);

  uiLayout *col = &layout->column(false);

  uiLayout *sub = &col->column(true);
  sub->prop(&ptr, "points_u", UI_ITEM_NONE, IFACE_("Resolution U"), ICON_NONE);
  sub->prop(&ptr, "points_v", UI_ITEM_NONE, IFACE_("V"), ICON_NONE);
  sub->prop(&ptr, "points_w", UI_ITEM_NONE, IFACE_("W"), ICON_NONE);

  col->separator();

  sub = &col->column(true);
  sub->prop(&ptr, "interpolation_type_u", UI_ITEM_NONE, IFACE_("Interpolation U"), ICON_NONE);
  sub->prop(&ptr, "interpolation_type_v", UI_ITEM_NONE, IFACE_("V"), ICON_NONE);
  sub->prop(&ptr, "interpolation_type_w", UI_ITEM_NONE, IFACE_("W"), ICON_NONE);

  col->separator();

  col->prop(&ptr, "use_outside", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  col->separator();

  /* `col.prop_search(lat, "vertex_group", context.object, "vertex_groups")`. */
  Object *ob = CTX_data_active_object(C);
  if (ob != nullptr) {
    PointerRNA ob_ptr = RNA_pointer_create_discrete(&ob->id, &RNA_Object, ob);
    uiItemPointerR(col, &ptr, "vertex_group", &ob_ptr, "vertex_groups", std::nullopt, ICON_NONE);
  }
}

static void lattice_animation_draw(const bContext *C, Panel *panel)
{
  Lattice *lattice = context_lattice(C);
  if (lattice == nullptr) {
    return;
  }
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  uiLayout *col = &layout->column(true);
  col->label(IFACE_("Lattice"), ICON_NONE);
  flipendo::properties_ui::draw_action_and_slot_selector(C, col, &lattice->id);

  if (lattice->key != nullptr) {
    col = &layout->column(true);
    col->label(IFACE_("Shape Keys"), ICON_NONE);
    flipendo::properties_ui::draw_action_and_slot_selector(C, col, &lattice->key->id);
  }
}

static void lattice_custom_props_draw(const bContext *C, Panel *panel)
{
  Lattice *lattice = context_lattice(C);
  if (lattice == nullptr) {
    return;
  }
  PointerRNA ptr = lattice_ptr(lattice);
  /* `_context_path = "object.data"`, que es lo que reciben los operadores
   * `WM_OT_properties_*`. */
  flipendo::properties_ui::draw_custom_properties(
      C, panel->layout, &ptr, &lattice->id, "object.data");
}

void fl_properties_data_lattice_register(ARegionType *art)
{
  static const flipendo::PanelDecl panels[] = {
      {
          /*idname*/ "DATA_PT_context_lattice",
          /*label*/ "",
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ context_lattice_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ lattice_poll,
          /*flag*/ PANEL_TYPE_NO_HEADER,
      },
      {
          /*idname*/ "DATA_PT_lattice",
          /*label*/ N_("Lattice"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ lattice_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ lattice_poll,
      },
      {
          /*idname*/ "DATA_PT_lattice_animation",
          /*label*/ N_("Animation"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ lattice_animation_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ lattice_poll,
          /*flag*/ PANEL_TYPE_DEFAULT_CLOSED,
          /* `PropertiesAnimationMixin.bl_order = PropertyPanel.bl_order - 1`. */
          /*order*/ 999,
      },
      {
          /*idname*/ "DATA_PT_custom_props_lattice",
          /*label*/ N_("Custom Properties"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ lattice_custom_props_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ lattice_poll,
          /*flag*/ PANEL_TYPE_DEFAULT_CLOSED,
          /*order*/ 1000,
      },
  };
  flipendo::panels_register(art, SPACE_PROPERTIES, {panels, ARRAY_SIZE(panels)});
}
