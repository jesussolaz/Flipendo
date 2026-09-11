/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spbuttons
 *
 * La pestana Coleccion del editor de Propiedades, en C++ nativo. Sustituye
 * `scripts/startup/bl_ui/properties_collection.py` **entera**: sus seis paneles
 * y su menu.
 *
 * Es la primera que se apoya en los dos ayudantes compartidos de
 * `FL_properties_ui.hpp` (el `PropertyPanel` de `rna_prop_ui`) en vez de
 * reescribirlos.
 */

#include "DNA_collection_types.h"
#include "DNA_layer_types.h"
#include "DNA_scene_types.h"
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

static Collection *context_collection(const bContext *C)
{
  return static_cast<Collection *>(
      CTX_data_pointer_get_type(C, "collection", &RNA_Collection).data);
}

static PointerRNA collection_ptr(Collection *collection)
{
  return RNA_pointer_create_discrete(&collection->id, &RNA_Collection, collection);
}

/** `poll` de `CollectionButtonsPanel`: `context.collection != context.scene.collection`. */
static bool collection_poll(const bContext *C, PanelType * /*pt*/)
{
  const Collection *collection = context_collection(C);
  const Scene *scene = CTX_data_scene(C);
  return collection != nullptr && scene != nullptr && collection != scene->master_collection;
}

static void collection_flags_draw(const bContext *C, Panel *panel)
{
  Collection *collection = context_collection(C);
  if (collection == nullptr) {
    return;
  }
  PointerRNA ptr = collection_ptr(collection);
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  /* `invert_checkbox=True` es `UI_ITEM_R_CHECKBOX_INVERT`: la casilla dice
   * «Selectable» y la propiedad se llama `hide_select`.
   *
   * Y `toggle=False` NO es el valor por defecto ni sobra: `hide_select` tiene
   * icono (`RESTRICT_SELECT_OFF`), y una booleana con icono se dibuja como
   * ICON_TOGGLE salvo que se fuerce lo contrario. `toggle=False` fuerza la
   * casilla, que en C++ es `UI_ITEM_R_ICON_NEVER`. Sin el, el volcado daba
   * `type=ICON_TOGGLE icon=RESTRICT_SELECT_OFF text=''` donde el Python daba
   * `type=CHECKBOX icon=NONE text='Selectable'`. */
  layout->prop(&ptr,
               "hide_select",
               UI_ITEM_R_CHECKBOX_INVERT | UI_ITEM_R_ICON_NEVER,
               IFACE_("Selectable"),
               ICON_NONE);

  uiLayout *col = &layout->column(true, IFACE_("Show In"));
  col->prop(&ptr,
            "hide_render",
            UI_ITEM_R_CHECKBOX_INVERT | UI_ITEM_R_ICON_NEVER,
            IFACE_("Renders"),
            ICON_NONE);
}

static void viewlayer_flags_draw(const bContext *C, Panel *panel)
{
  const ViewLayer *view_layer = CTX_data_view_layer(C);
  if (view_layer == nullptr || view_layer->active_collection == nullptr) {
    return;
  }
  PointerRNA ptr = RNA_pointer_create_discrete(
      &CTX_data_scene(C)->id, &RNA_LayerCollection, view_layer->active_collection);

  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  uiLayout *col = &layout->column(true);
  col->prop(&ptr,
            "exclude",
            UI_ITEM_R_CHECKBOX_INVERT | UI_ITEM_R_ICON_NEVER,
            IFACE_("Include"),
            ICON_NONE);
  col->prop(&ptr, "holdout", UI_ITEM_R_ICON_NEVER, std::nullopt, ICON_NONE);
  col->prop(&ptr, "indirect_only", UI_ITEM_R_ICON_NEVER, std::nullopt, ICON_NONE);
}

static void exporters_draw(const bContext *C, Panel *panel)
{
  uiTemplateCollectionExporters(panel->layout, const_cast<bContext *>(C));
}

static void instance_offset_menu_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;
  layout->op("OBJECT_OT_instance_offset_from_cursor", std::nullopt, ICON_NONE);
  layout->op("OBJECT_OT_instance_offset_from_object", std::nullopt, ICON_NONE);
  layout->op("OBJECT_OT_instance_offset_to_cursor", std::nullopt, ICON_NONE);
}

static void instancing_draw(const bContext *C, Panel *panel)
{
  Collection *collection = context_collection(C);
  if (collection == nullptr) {
    return;
  }
  PointerRNA ptr = collection_ptr(collection);
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  uiLayout *row = &layout->row(true);
  row->prop(&ptr, "instance_offset", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  row->menu("COLLECTION_MT_context_menu_instance_offset", "", ICON_DOWNARROW_HLT);
}

static void lineart_draw(const bContext *C, Panel *panel)
{
  Collection *collection = context_collection(C);
  if (collection == nullptr) {
    return;
  }
  PointerRNA ptr = collection_ptr(collection);
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  uiLayout *row = &layout->row(false);
  row->prop(&ptr, "lineart_usage", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  layout->prop(&ptr,
               "lineart_use_intersection_mask",
               UI_ITEM_NONE,
               IFACE_("Collection Mask"),
               ICON_NONE);

  const bool use_mask = RNA_boolean_get(&ptr, "lineart_use_intersection_mask");
  uiLayout *col = &layout->column(true);
  uiLayoutSetActive(col, use_mask);
  PropertyRNA *prop_mask = RNA_struct_find_property(&ptr, "lineart_intersection_mask");
  /* Ocho casillas en dos filas de cuatro: el Python abre una fila nueva DESPUES
   * de pintar la cuarta (`if i == 3`), no antes de la quinta. Da lo mismo salvo
   * por el `heading`, que solo lleva la primera. */
  row = &col->row(true, IFACE_("Masks"));
  for (int i = 0; i < 8; i++) {
    row->prop(&ptr, prop_mask, i, 0, UI_ITEM_R_TOGGLE, " ", ICON_NONE);
    if (i == 3) {
      row = &col->row(true);
    }
  }

  row = &layout->row(false, IFACE_("Intersection Priority"));
  row->prop(&ptr, "use_lineart_intersection_priority", UI_ITEM_NONE, "", ICON_NONE);
  uiLayout *subrow = &row->row(false);
  uiLayoutSetActive(subrow, RNA_boolean_get(&ptr, "use_lineart_intersection_priority"));
  subrow->prop(&ptr, "lineart_intersection_priority", UI_ITEM_NONE, "", ICON_NONE);
}

static void custom_props_draw(const bContext *C, Panel *panel)
{
  Collection *collection = context_collection(C);
  if (collection == nullptr) {
    return;
  }
  PointerRNA ptr = collection_ptr(collection);
  flipendo::properties_ui::draw_custom_properties(
      C, panel->layout, &ptr, &collection->id, "collection");
}

void fl_properties_collection_register(ARegionType *art)
{
  static const flipendo::MenuDecl menus[] = {
      {
          /*idname*/ "COLLECTION_MT_context_menu_instance_offset",
          /*label*/ N_("Instance Offset"),
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ instance_offset_menu_draw,
      },
  };
  flipendo::menus_register({menus, ARRAY_SIZE(menus)});

  static const flipendo::PanelDecl panels[] = {
      {
          /*idname*/ "COLLECTION_PT_collection_flags",
          /*label*/ N_("Visibility"),
          /*category*/ nullptr,
          /*context*/ "collection",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ collection_flags_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ collection_poll,
      },
      {
          /*idname*/ "COLLECTION_PT_viewlayer_flags",
          /*label*/ N_("View Layer"),
          /*category*/ nullptr,
          /*context*/ "collection",
          /*parent_id*/ "COLLECTION_PT_collection_flags",
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ viewlayer_flags_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ collection_poll,
      },
      {
          /*idname*/ "COLLECTION_PT_instancing",
          /*label*/ N_("Instancing"),
          /*category*/ nullptr,
          /*context*/ "collection",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ instancing_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ collection_poll,
      },
      {
          /*idname*/ "COLLECTION_PT_lineart_collection",
          /*label*/ N_("Line Art"),
          /*category*/ nullptr,
          /*context*/ "collection",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ lineart_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ collection_poll,
          /*flag*/ 0,
          /*order*/ 10,
      },
      {
          /*idname*/ "COLLECTION_PT_collection_custom_props",
          /*label*/ N_("Custom Properties"),
          /*category*/ nullptr,
          /*context*/ "collection",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ custom_props_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ collection_poll,
          /*flag*/ PANEL_TYPE_DEFAULT_CLOSED,
          /* El mixin `PropertyPanel` de `rna_prop_ui` lleva `bl_order = 1000`:
           * las propiedades personalizadas van siempre al final de la pestana. */
          /*order*/ 1000,
      },
      {
          /*idname*/ "COLLECTION_PT_exporters",
          /*label*/ N_("Exporters"),
          /*category*/ nullptr,
          /*context*/ "collection",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ exporters_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ collection_poll,
      },
  };
  flipendo::panels_register(art, SPACE_PROPERTIES, {panels, ARRAY_SIZE(panels)});
}
