/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spbuttons
 *
 * La pestana de datos del Metaball, en C++ nativo. Sustituye
 * `scripts/startup/bl_ui/properties_data_metaball.py` **entera**: sus seis
 * paneles. Nunca un panel suelto: un panel aislado cambia de posicion dentro de
 * la lista de su region y el volcado marca diferencia aunque no cambie ningun
 * campo (`UI-A-CPP.md`).
 *
 * Los dos ultimos paneles son los mixins compartidos de `FL_properties_ui.hpp`:
 * `DATA_PT_metaball_animation` es el `PropertiesAnimationMixin` generico (no lo
 * sobrescribe) y `DATA_PT_custom_props_metaball` es el `PropertyPanel`.
 */

#include "DNA_meta_types.h"
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

static MetaBall *context_metaball(const bContext *C)
{
  return static_cast<MetaBall *>(CTX_data_pointer_get_type(C, "meta_ball", &RNA_MetaBall).data);
}

static PointerRNA metaball_ptr(MetaBall *mball)
{
  return RNA_pointer_create_discrete(&mball->id, &RNA_MetaBall, mball);
}

/** `poll` de `DataButtonsPanel`: `context.meta_ball`. */
static bool metaball_poll(const bContext *C, PanelType * /*pt*/)
{
  return context_metaball(C) != nullptr;
}

/**
 * `poll` de `DATA_PT_metaball_element`:
 * `context.meta_ball and context.meta_ball.elements.active`.
 *
 * `elements.active` es `MetaBall::lastelem`, que **solo esta puesto en modo
 * edicion**: `ED_mball_editmball_make()` lo asigna al entrar y
 * `ED_mball_editmball_free()` lo borra al salir, y ademas no sobrevive al
 * `.blend` (`mball.cc` lo pone a nullptr al leer). O sea que este panel solo se
 * dibuja editando el metaball; no es una peculiaridad de la migracion.
 */
static bool metaball_element_poll(const bContext *C, PanelType * /*pt*/)
{
  const MetaBall *mball = context_metaball(C);
  return mball != nullptr && mball->lastelem != nullptr;
}

static void context_metaball_draw(const bContext *C, Panel *panel)
{
  Object *ob = CTX_data_active_object(C);
  uiLayout *layout = panel->layout;
  if (ob != nullptr) {
    PointerRNA ob_ptr = RNA_pointer_create_discrete(&ob->id, &RNA_Object, ob);
    uiTemplateID(layout, C, &ob_ptr, "data", nullptr, nullptr, nullptr);
  }
  else if (context_metaball(C) != nullptr) {
    SpaceProperties *space = CTX_wm_space_properties(C);
    PointerRNA space_ptr = RNA_pointer_create_discrete(
        reinterpret_cast<ID *>(CTX_wm_screen(C)), &RNA_SpaceProperties, space);
    uiTemplateID(layout, C, &space_ptr, "pin_id", nullptr, nullptr, nullptr);
  }
}

static void metaball_draw(const bContext *C, Panel *panel)
{
  MetaBall *mball = context_metaball(C);
  if (mball == nullptr) {
    return;
  }
  PointerRNA ptr = metaball_ptr(mball);
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);

  uiLayout *col = &layout->column(true);
  col->prop(&ptr, "resolution", UI_ITEM_NONE, IFACE_("Resolution Viewport"), ICON_NONE);
  col->prop(&ptr, "render_resolution", UI_ITEM_NONE, IFACE_("Render"), ICON_NONE);

  col->separator();

  col->prop(&ptr, "threshold", UI_ITEM_NONE, IFACE_("Influence Threshold"), ICON_NONE);

  col->separator();

  col->prop(&ptr, "update_method", UI_ITEM_NONE, IFACE_("Update on Edit"), ICON_NONE);
}

static void mball_texture_space_draw(const bContext *C, Panel *panel)
{
  MetaBall *mball = context_metaball(C);
  if (mball == nullptr) {
    return;
  }
  PointerRNA ptr = metaball_ptr(mball);
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);

  layout->prop(&ptr, "use_auto_texspace", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  uiLayout *col = &layout->column(false);
  col->prop(&ptr, "texspace_location", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(&ptr, "texspace_size", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void metaball_element_draw(const bContext *C, Panel *panel)
{
  MetaBall *mball = context_metaball(C);
  if (mball == nullptr || mball->lastelem == nullptr) {
    return;
  }
  MetaElem *metaelem = mball->lastelem;
  PointerRNA ptr = RNA_pointer_create_discrete(&mball->id, &RNA_MetaElement, metaelem);
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);

  uiLayout *col = &layout->column(false);

  col->prop(&ptr, "type", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  col->separator();

  col->prop(&ptr, "stiffness", UI_ITEM_NONE, IFACE_("Stiffness"), ICON_NONE);
  col->prop(&ptr, "radius", UI_ITEM_NONE, IFACE_("Radius"), ICON_NONE);
  col->prop(&ptr, "use_negative", UI_ITEM_NONE, IFACE_("Negative"), ICON_NONE);
  col->prop(&ptr, "hide", UI_ITEM_NONE, IFACE_("Hide"), ICON_NONE);

  /* La subcolumna se crea SIEMPRE, tambien para `BALL`, donde queda vacia: el
   * Python la crea antes del `if`. Crearla solo dentro de las ramas quitaria un
   * `LAYOUT_COLUMN` del arbol y el volcado lo canta. */
  uiLayout *sub = &col->column(true);

  /* `metaelem.type in {'CUBE', 'ELLIPSOID'}` / `'CAPSULE'` / `'PLANE'`. En el DNA
   * son `MB_CUBE`, `MB_ELIPSOID`, `MB_TUBE` (el `CAPSULE` del enum RNA) y
   * `MB_PLANE`. Los `MB_TUBEX/Y/Z` estan marcados obsoletos, no salen en el enum
   * RNA y el Python no los nombra: caen al caso vacio igual que `BALL`. */
  switch (metaelem->type) {
    case MB_CUBE:
    case MB_ELIPSOID:
      sub->prop(&ptr, "size_x", UI_ITEM_NONE, IFACE_("Size X"), ICON_NONE);
      sub->prop(&ptr, "size_y", UI_ITEM_NONE, IFACE_("Y"), ICON_NONE);
      sub->prop(&ptr, "size_z", UI_ITEM_NONE, IFACE_("Z"), ICON_NONE);
      break;
    case MB_TUBE:
      sub->prop(&ptr, "size_x", UI_ITEM_NONE, IFACE_("Size X"), ICON_NONE);
      break;
    case MB_PLANE:
      sub->prop(&ptr, "size_x", UI_ITEM_NONE, IFACE_("Size X"), ICON_NONE);
      sub->prop(&ptr, "size_y", UI_ITEM_NONE, IFACE_("Y"), ICON_NONE);
      break;
    default:
      break;
  }
}

static void metaball_animation_draw(const bContext *C, Panel *panel)
{
  MetaBall *mball = context_metaball(C);
  if (mball == nullptr) {
    return;
  }
  flipendo::properties_ui::draw_animation_panel(C, panel->layout, &mball->id);
}

static void metaball_custom_props_draw(const bContext *C, Panel *panel)
{
  MetaBall *mball = context_metaball(C);
  if (mball == nullptr) {
    return;
  }
  PointerRNA ptr = metaball_ptr(mball);
  /* `_context_path = "object.data"`, que es lo que reciben los operadores
   * `WM_OT_properties_*`. */
  flipendo::properties_ui::draw_custom_properties(
      C, panel->layout, &ptr, &mball->id, "object.data");
}

void fl_properties_data_metaball_register(ARegionType *art)
{
  static const flipendo::PanelDecl panels[] = {
      {
          /*idname*/ "DATA_PT_context_metaball",
          /*label*/ "",
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ context_metaball_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ metaball_poll,
          /*flag*/ PANEL_TYPE_NO_HEADER,
      },
      {
          /*idname*/ "DATA_PT_metaball",
          /*label*/ N_("Metaball"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ metaball_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ metaball_poll,
      },
      {
          /*idname*/ "DATA_PT_mball_texture_space",
          /*label*/ N_("Texture Space"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ mball_texture_space_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ metaball_poll,
          /*flag*/ PANEL_TYPE_DEFAULT_CLOSED,
      },
      {
          /*idname*/ "DATA_PT_metaball_element",
          /*label*/ N_("Active Element"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ metaball_element_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ metaball_element_poll,
      },
      {
          /*idname*/ "DATA_PT_metaball_animation",
          /*label*/ N_("Animation"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ metaball_animation_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ metaball_poll,
          /*flag*/ PANEL_TYPE_DEFAULT_CLOSED,
          /* `PropertiesAnimationMixin.bl_order = PropertyPanel.bl_order - 1`. */
          /*order*/ 999,
      },
      {
          /*idname*/ "DATA_PT_custom_props_metaball",
          /*label*/ N_("Custom Properties"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ metaball_custom_props_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ metaball_poll,
          /*flag*/ PANEL_TYPE_DEFAULT_CLOSED,
          /*order*/ 1000,
      },
  };
  flipendo::panels_register(art, SPACE_PROPERTIES, {panels, ARRAY_SIZE(panels)});
}
