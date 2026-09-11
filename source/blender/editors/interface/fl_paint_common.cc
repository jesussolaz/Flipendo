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
#include "DNA_space_types.h"
#include "DNA_workspace_types.h"

#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"

#include "BLT_translation.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_toolsystem.hh"

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

  /* `ups = context.tool_settings.unified_paint_settings`: sin escena el Python revienta
   * AQUI, con la fila ya creada. `unified_paint_settings` es un struct dentro de
   * `ToolSettings`, asi que solo sale nulo si no hay `ToolSettings` — que es justo el
   * caso en el que el Python lanza el `AttributeError`. */
  PointerRNA ups = unified_paint_settings(C);
  if (ups.data == nullptr) {
    return nullptr;
  }
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
  /* `ups = context.tool_settings.unified_paint_settings` antes que nada: sin el, el
   * Python revienta aqui y no dibuja. */
  PointerRNA ups = unified_paint_settings(C);
  if (ups.data == nullptr) {
    return false;
  }
  PointerRNA *prop_owner = RNA_boolean_get(&ups, "use_unified_color") ? &ups : brush;
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
  if (ups.data == nullptr) {
    return false;
  }
  PointerRNA *prop_owner = RNA_boolean_get(&ups, "use_unified_color") ? &ups : brush;
  if (prop_owner == nullptr || prop_owner->data == nullptr) {
    return false;
  }
  /* `template_color_picker(..., lock=False, lock_luminosity=False, cubic=False)`. */
  uiTemplateColorPicker(parent, prop_owner, prop_name, value_slider, false, false, false);
  return true;
}

/* -------------------------------------------------------------------- */
/** \name `get_brush_mode` y `paint_settings`
 * \{ */

const char *get_brush_mode(const bContext *C)
{
  /* `mode = context.mode`. */
  const char *mode = CTX_data_mode_string(C);
  if (mode == nullptr) {
    return nullptr;
  }
  /* NOTE del Python: los pinceles de particulas van completamente por su cuenta. */
  if (STREQ(mode, "PARTICLE")) {
    return nullptr;
  }

  /* `tool = context.workspace.tools.from_active_space()`. */
  const bToolRef *tref = WM_toolsystem_ref_from_context(C);
  if (tref == nullptr) {
    return nullptr;
  }
  /* `if not tool.use_brushes` — `rna_WorkSpaceTool_use_brushes_get`. */
  if (tref->runtime == nullptr || (tref->runtime->flag & TOOLREF_FLAG_USE_BRUSHES) == 0) {
    return nullptr;
  }

  const SpaceLink *space_data = CTX_wm_space_data(C);
  if (space_data == nullptr) {
    return nullptr;
  }
  if (space_data->spacetype == SPACE_IMAGE) {
    return "PAINT_2D";
  }
  if (space_data->spacetype == SPACE_VIEW3D || space_data->spacetype == SPACE_PROPERTIES) {
    if (STREQ(mode, "PAINT_TEXTURE")) {
      /* `tool_settings.image_paint` es un struct dentro de `ToolSettings`, asi que en la
       * practica nunca es falso; se conserva la comprobacion por fidelidad. */
      const Scene *scene = CTX_data_scene(C);
      if (scene != nullptr && scene->toolsettings != nullptr) {
        return mode;
      }
      return nullptr;
    }
    return mode;
  }
  return nullptr;
}

PointerRNA paint_settings(const bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  if (scene == nullptr || scene->toolsettings == nullptr) {
    return PointerRNA_NULL;
  }
  const char *mode = get_brush_mode(C);
  if (mode == nullptr) {
    return PointerRNA_NULL;
  }

  /* Misma cadena de `elif` que el Python, en el mismo orden. Las dos ramas repetidas de
   * `PAINT_GREASE_PENCIL` del original son inalcanzables (la segunda), y se conserva la
   * primera, que es la que decide. */
  struct {
    const char *mode;
    const char *member;
  } static const table[] = {
      {"SCULPT", "sculpt"},
      {"PAINT_VERTEX", "vertex_paint"},
      {"PAINT_WEIGHT", "weight_paint"},
      {"PAINT_TEXTURE", "image_paint"},
      {"PARTICLE", "particle_edit"},
      {"PAINT_2D", "image_paint"},
      {"PAINT_GPENCIL", "gpencil_paint"},
      {"SCULPT_GPENCIL", "gpencil_sculpt_paint"},
      {"WEIGHT_GPENCIL", "gpencil_weight_paint"},
      {"VERTEX_GPENCIL", "gpencil_vertex_paint"},
      {"PAINT_GREASE_PENCIL", "gpencil_paint"},
      {"SCULPT_CURVES", "curves_sculpt"},
      {"SCULPT_GREASE_PENCIL", "gpencil_sculpt_paint"},
      {"WEIGHT_GREASE_PENCIL", "gpencil_weight_paint"},
      {"VERTEX_GREASE_PENCIL", "gpencil_vertex_paint"},
  };

  PointerRNA ts = RNA_pointer_create_discrete(&scene->id, &RNA_ToolSettings, scene->toolsettings);
  for (const auto &row : table) {
    if (STREQ(mode, row.mode)) {
      return RNA_pointer_get(&ts, row.member);
    }
  }
  return PointerRNA_NULL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name `brush_basic_grease_pencil_weight_settings`
 * \{ */

bool brush_basic_grease_pencil_weight_settings(uiLayout *layout,
                                               const bContext *C,
                                               PointerRNA *brush,
                                               const bool compact)
{
  if (prop_unified(layout,
                   C,
                   brush,
                   "size",
                   "use_unified_size",
                   "use_pressure_size",
                   ICON_NONE,
                   IFACE_("Radius"),
                   true,
                   compact) == nullptr)
  {
    return false;
  }

  /* `capabilities = brush.sculpt_capabilities`: con el pincel a `None`, AttributeError. */
  if (brush == nullptr || brush->data == nullptr) {
    return false;
  }
  PropertyRNA *caps_prop = RNA_struct_find_property(brush, "sculpt_capabilities");
  if (caps_prop == nullptr) {
    return false;
  }
  PointerRNA caps = RNA_property_pointer_get(brush, caps_prop);
  const char *pressure_name = (caps.data != nullptr &&
                               RNA_boolean_get(&caps, "has_strength_pressure")) ?
                                  "use_pressure_strength" :
                                  nullptr;

  /* Sin `slider`: el Python no se lo pasa a la fuerza en este ayudante. */
  if (prop_unified(layout,
                   C,
                   brush,
                   "strength",
                   "use_unified_strength",
                   pressure_name,
                   ICON_NONE,
                   IFACE_("Strength"),
                   false,
                   compact) == nullptr)
  {
    return false;
  }

  if (RNA_enum_is_equal(const_cast<bContext *>(C), brush, "gpencil_weight_tool", "WEIGHT")) {
    if (prop_unified(layout,
                     C,
                     brush,
                     "weight",
                     "use_unified_weight",
                     nullptr,
                     ICON_NONE,
                     IFACE_("Weight"),
                     true,
                     compact) == nullptr)
    {
      return false;
    }
    layout->prop(brush,
                 "direction",
                 UI_ITEM_R_EXPAND,
                 compact ? std::optional<blender::StringRef>("") :
                           std::optional<blender::StringRef>(IFACE_("Direction")),
                 ICON_NONE);
  }
  return true;
}

/** \} */

}  // namespace flipendo::paint_common
