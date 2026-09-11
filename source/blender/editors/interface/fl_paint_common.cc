/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Ver `FL_paint_common.hh`: los tres ayudantes de `UnifiedPaintPanel`.
 *
 * Transliteracion linea a linea de `scripts/startup/bl_ui/properties_paint_common.py`
 * (`prop_unified` :260, `prop_unified_color` :287, `prop_unified_color_picker` :293).
 */

#include "DNA_scene_types.h"

#include "BKE_context.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "FL_paint_common.hh"

namespace flipendo::paint_common {

PointerRNA unified_paint_settings(const bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  if (scene == nullptr || scene->toolsettings == nullptr) {
    return PointerRNA_NULL;
  }
  PointerRNA ts = RNA_pointer_create_discrete(
      &scene->id, &RNA_ToolSettings, scene->toolsettings);
  return RNA_pointer_get(&ts, "unified_paint_settings");
}

/** `if unified_name and getattr(ups, unified_name)`: en Python la cadena vacia es falsa. */
static bool unified_owns(PointerRNA *ups, const char *unified_name)
{
  if (unified_name == nullptr || unified_name[0] == '\0' || ups->data == nullptr) {
    return false;
  }
  return RNA_boolean_get(ups, unified_name);
}

uiLayout *prop_unified(uiLayout *layout,
                       const bContext *C,
                       PointerRNA *brush,
                       const char *prop_name,
                       const char *unified_name,
                       const char *pressure_name,
                       const int icon,
                       const std::optional<blender::StringRef> text,
                       const bool slider,
                       const bool header)
{
  /* `row = layout.row(align=True)`: se crea ANTES de tocar el pincel, asi que sigue
   * estando cuando el Python revienta en la linea siguiente. */
  uiLayout *row = &layout->row(true);

  PointerRNA ups = unified_paint_settings(C);
  PointerRNA *prop_owner = brush;
  const bool has_unified_name = unified_name != nullptr && unified_name[0] != '\0';
  if (unified_owns(&ups, unified_name)) {
    prop_owner = &ups;
  }

  if (prop_owner == nullptr || prop_owner->data == nullptr) {
    /* `row.prop(None, ...)`: TypeError, el `draw()` se corta aqui. */
    return nullptr;
  }
  row->prop(prop_owner, prop_name, slider ? UI_ITEM_R_SLIDER : UI_ITEM_NONE, text, icon);

  if (pressure_name != nullptr && pressure_name[0] != '\0') {
    if (brush == nullptr || brush->data == nullptr) {
      return nullptr;
    }
    row->prop(brush, pressure_name, UI_ITEM_NONE, "", ICON_NONE);
  }

  if (has_unified_name && !header) {
    /* NOTE del Python: los ajustes unificados no se pintan en la cabecera, para no
     * recargarla (D5928#136281). */
    if (ups.data == nullptr) {
      return nullptr;
    }
    row->prop(&ups, unified_name, UI_ITEM_NONE, "", ICON_BRUSHES_ALL);
  }

  return row;
}

bool prop_unified_color(uiLayout *parent,
                        const bContext *C,
                        PointerRNA *brush,
                        const char *prop_name,
                        const std::optional<blender::StringRef> text)
{
  PointerRNA ups = unified_paint_settings(C);
  const bool use_unified = ups.data != nullptr && RNA_boolean_get(&ups, "use_unified_color");
  PointerRNA *prop_owner = use_unified ? &ups : brush;
  if (prop_owner == nullptr || prop_owner->data == nullptr) {
    return false;
  }
  parent->prop(prop_owner, prop_name, UI_ITEM_NONE, text, ICON_NONE);
  return true;
}

bool prop_unified_color_picker(uiLayout *parent,
                               const bContext *C,
                               PointerRNA *brush,
                               const char *prop_name,
                               const bool value_slider)
{
  PointerRNA ups = unified_paint_settings(C);
  const bool use_unified = ups.data != nullptr && RNA_boolean_get(&ups, "use_unified_color");
  PointerRNA *prop_owner = use_unified ? &ups : brush;
  if (prop_owner == nullptr || prop_owner->data == nullptr) {
    return false;
  }
  /* `template_color_picker(..., lock=False, lock_luminosity=False, cubic=False)`. */
  uiTemplateColorPicker(parent, prop_owner, prop_name, value_slider, false, false, false);
  return true;
}

}  // namespace flipendo::paint_common
