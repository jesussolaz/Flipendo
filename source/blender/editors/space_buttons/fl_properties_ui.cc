/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spbuttons
 *
 * Los dos ayudantes de dibujo que comparten casi todas las pestanas de
 * Propiedades, en un sitio donde se puedan usar.
 *
 * En Python eran dos mixins: `PropertyPanel` (de `rna_prop_ui`) y
 * `PropertiesAnimationMixin` (de `bl_ui.space_properties`). Los hereda el 80 %
 * de los ficheros `properties_*.py`, asi que sin ellos cada pestana que se migre
 * tiene que reescribirlos o copiarlos.
 *
 * Estaban ya escritos en C++, pero `static` dentro de `fl_world_buttons.cc`, que
 * es donde hicieron falta por primera vez. Aqui salen a `FL_properties_ui.hpp`
 * sin tocar ni una linea de su cuerpo: es una extraccion, no una reescritura, y
 * por eso el volcado no se mueve ni un bloque.
 */

#include "FL_properties_ui.hpp"

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

#include "DNA_ID.h"
#include "DNA_anim_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_utils.hh"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_anim_data.hh"
#include "BKE_idprop.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

namespace flipendo::properties_ui {

void draw_action_and_slot_selector(const bContext *C, uiLayout *layout, ID *id)
{
  uiTemplateAction(layout, C, id, "ACTION_OT_new", "ACTION_OT_unlink", std::nullopt);

  AnimData *adt = BKE_animdata_from_id(id);
  if (adt == nullptr || adt->action == nullptr) {
    return;
  }
  PointerRNA action = RNA_id_pointer_create(&adt->action->id);
  if (!RNA_boolean_get(&action, "is_action_layered")) {
    return;
  }

  PointerRNA adt_ptr = RNA_pointer_create_discrete(id, &RNA_AnimData, adt);
  PointerRNA id_ptr = RNA_id_pointer_create(id);
  uiLayoutSetContextPointer(layout, "animated_id", &id_ptr);
  uiTemplateSearch(layout,
                   C,
                   &adt_ptr,
                   "action_slot",
                   &adt_ptr,
                   "action_suitable_slots",
                   "ANIM_OT_slot_new_for_id",
                   "ANIM_OT_slot_unassign_from_id");
}

/* Núcleo de `rna_prop_ui.draw()` para un ID. Mantiene también las ramas que la
 * escena de fábrica no ejercita: valores complejos, punteros ID y edición. */
void draw_custom_properties(const bContext *C,
                            uiLayout *layout,
                            PointerRNA *ptr,
                            ID *id,
                            const char *data_path)
{
  const bool use_edit = !ID_IS_LINKED(id) && !ID_IS_OVERRIDE_LIBRARY_REAL(id);
  if (use_edit) {
    uiLayout *row = &layout->row(false);
    PointerRNA op = row->op("WM_OT_properties_add", IFACE_("New"), ICON_ADD);
    RNA_string_set(&op, "data_path", data_path);
    layout->separator();
  }
  uiLayoutSetPropDecorate(layout, false);

  IDProperty *group = IDP_GetProperties(id);
  if (group == nullptr) {
    return;
  }

  std::vector<IDProperty *> properties;
  LISTBASE_FOREACH (IDProperty *, property, &group->data.group) {
    properties.push_back(property);
  }
  std::sort(properties.begin(), properties.end(), [](const IDProperty *a, const IDProperty *b) {
    return strcmp(a->name, b->name) < 0;
  });

  for (IDProperty *property : properties) {
    const std::string property_path = "[\"" + BLI_str_escape(property->name) + "\"]";
    uiLayout *split = &layout->split(0.4f, true);
    uiLayout *label_row = &split->row(false);
    uiLayoutSetAlignment(label_row, UI_LAYOUT_ALIGN_RIGHT);
    label_row->label(property->name, ICON_NONE);

    uiLayout *value_row = &split->row(true);
    uiLayout *value_column = &value_row->column(true);
    const bool edit_value = property->type == IDP_GROUP ||
                            (property->type == IDP_ARRAY && property->len >= 8);
    if (edit_value) {
      PointerRNA op = value_column->op(
          "WM_OT_properties_edit_value", IFACE_("Edit Value"), ICON_NONE);
      RNA_string_set(&op, "data_path", data_path);
      RNA_string_set(&op, "property_name", property->name);
    }
    else if (property->type == IDP_ID) {
      uiTemplateID(value_column,
                   C,
                   ptr,
                   property_path,
                   nullptr,
                   nullptr,
                   nullptr,
                   UI_TEMPLATE_ID_FILTER_ALL,
                   false,
                   "");
    }
    else {
      value_column->prop(ptr, property_path, UI_ITEM_NONE, "", ICON_NONE);
    }

    if (use_edit) {
      uiLayout *operator_row = &value_row->row(true);
      uiLayoutSetAlignment(operator_row, UI_LAYOUT_ALIGN_RIGHT);
      uiLayoutSetEmboss(operator_row, blender::ui::EmbossType::None);
      PointerRNA edit = operator_row->op("WM_OT_properties_edit", "", ICON_PREFERENCES);
      RNA_string_set(&edit, "data_path", data_path);
      RNA_string_set(&edit, "property_name", property->name);
      PointerRNA remove = operator_row->op("WM_OT_properties_remove", "", ICON_X);
      RNA_string_set(&remove, "data_path", data_path);
      RNA_string_set(&remove, "property_name", property->name);
    }
  }
}

}  // namespace flipendo::properties_ui
