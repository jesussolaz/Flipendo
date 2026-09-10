/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Ver FL_tool_settings_ui.hh. Cada funcion reproduce su gemela de
 * `makesrna/intern/rna_ui_api.cc`; el comentario de cada una dice cual.
 */

#include <cstdio>
#include <optional>

#include "BKE_context.hh"

#include "BLT_translation.hh"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "WM_api.hh"
#include "WM_toolsystem.hh"

#include "FL_tool_settings_ui.hh"

namespace flipendo::ui::settings {

/**
 * `rna_translate_ui_text` sin tipo RNA: `None` no es texto, el texto vacio o con la
 * traduccion apagada va tal cual, y el resto con su contexto o el de por defecto.
 */
static std::optional<blender::StringRef> ui_text(const char *text, const char *text_ctxt)
{
  if (text == nullptr) {
    return std::nullopt;
  }
  if (text[0] == '\0' || !BLT_translate_iface()) {
    return blender::StringRef(text);
  }
  return blender::StringRef(
      BLT_pgettext((text_ctxt != nullptr && text_ctxt[0] != '\0') ? text_ctxt :
                                                                      BLT_I18NCONTEXT_DEFAULT,
                   text));
}

/* -------------------------------------------------------------------- */
/** \name Fuentes de datos
 * \{ */

PointerRNA op_props(bToolRef *tref, const char *op_idname)
{
  /* `rna_WorkSpaceTool_operator_properties`. */
  wmOperatorType *ot = WM_operatortype_find(op_idname, true);
  if (tref == nullptr || ot == nullptr) {
    fprintf(stderr, "Herramientas: operador '%s' no encontrado.\n", op_idname);
    return PointerRNA_NULL;
  }
  PointerRNA ptr;
  WM_toolsystem_ref_properties_ensure_from_operator(tref, ot, &ptr);
  return ptr;
}

PointerRNA gizmo_props(bToolRef *tref, const char *group)
{
  /* `rna_WorkSpaceTool_gizmo_group_properties`. */
  wmGizmoGroupType *gzgt = WM_gizmogrouptype_find(group, false);
  if (tref == nullptr || gzgt == nullptr) {
    fprintf(stderr, "Herramientas: grupo de gizmos '%s' no encontrado.\n", group);
    return PointerRNA_NULL;
  }
  PointerRNA ptr;
  WM_toolsystem_ref_properties_ensure_from_gizmo_group(tref, gzgt, &ptr);
  return ptr;
}

PointerRNA tool_settings(const bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  if (scene == nullptr || scene->toolsettings == nullptr) {
    return PointerRNA_NULL;
  }
  return RNA_pointer_create_discrete(&scene->id, &RNA_ToolSettings, scene->toolsettings);
}

PointerRNA scene(const bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  return scene != nullptr ? RNA_id_pointer_create(&scene->id) : PointerRNA_NULL;
}

PointerRNA preferences_edit()
{
  return RNA_pointer_create_discrete(nullptr, &RNA_PreferencesEdit, &U);
}

PointerRNA context_pointer(const bContext *C, const char *member)
{
  return CTX_data_pointer_get(C, member);
}

PointerRNA pointer_get(PointerRNA *ptr, const char *name)
{
  if (ptr == nullptr || ptr->data == nullptr) {
    return PointerRNA_NULL;
  }
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);
  if (prop == nullptr || RNA_property_type(prop) != PROP_POINTER) {
    return PointerRNA_NULL;
  }
  return RNA_property_pointer_get(ptr, prop);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Contexto
 * \{ */

bool region_is_tool_header(const bContext *C)
{
  const ARegion *region = CTX_wm_region(C);
  return region != nullptr && region->regiontype == RGN_TYPE_TOOL_HEADER;
}

int space_type(const bContext *C)
{
  const SpaceLink *sl = CTX_wm_space_data(C);
  return sl != nullptr ? sl->spacetype : -1;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Elementos
 * \{ */

void prop(uiLayout *layout, PointerRNA *ptr, const char *name, const Prop &args)
{
  /* `rna_uiItemR`. */
  if (ptr == nullptr || ptr->data == nullptr) {
    return;
  }
  PropertyRNA *property = RNA_struct_find_property(ptr, name);
  if (property == nullptr) {
    fprintf(stderr,
            "Herramientas: propiedad no encontrada: %s.%s\n",
            RNA_struct_identifier(ptr->type),
            name);
    return;
  }
  eUI_Item_Flag flag = UI_ITEM_NONE;
  if (args.slider_) {
    flag |= UI_ITEM_R_SLIDER;
  }
  if (args.expand_) {
    flag |= UI_ITEM_R_EXPAND;
  }
  if (args.toggle_ == 1) {
    flag |= UI_ITEM_R_TOGGLE;
  }
  else if (args.toggle_ == 0) {
    flag |= UI_ITEM_R_ICON_NEVER;
  }
  if (args.icon_only_) {
    flag |= UI_ITEM_R_ICON_ONLY;
  }
  if (!args.emboss_) {
    flag |= UI_ITEM_R_NO_BG;
  }
  if (args.invert_checkbox_) {
    flag |= UI_ITEM_R_CHECKBOX_INVERT;
  }
  layout->prop(ptr, property, args.index_, 0, flag, ui_text(args.text_, args.text_ctxt_), args.icon_);
}

void prop_enum(uiLayout *layout,
               PointerRNA *ptr,
               const char *name,
               const char *value,
               const char *text,
               const int icon)
{
  /* `rna_uiItemEnumR_string`. */
  if (ptr == nullptr || ptr->data == nullptr) {
    return;
  }
  PropertyRNA *property = RNA_struct_find_property(ptr, name);
  if (property == nullptr) {
    fprintf(stderr,
            "Herramientas: propiedad no encontrada: %s.%s\n",
            RNA_struct_identifier(ptr->type),
            name);
    return;
  }
  std::optional<blender::StringRef> t = ui_text(text, nullptr);
  uiItemEnumR_string_prop(
      layout,
      ptr,
      property,
      value,
      t ? std::optional<blender::StringRefNull>(blender::StringRefNull(t->data())) : std::nullopt,
      icon);
}

void label(uiLayout *layout, const char *text, const int icon, const char *text_ctxt)
{
  /* `rna_uiItemL`. */
  layout->label(ui_text(text != nullptr ? text : "", text_ctxt).value_or(""), icon);
}

PointerRNA op(uiLayout *layout,
              const char *opname,
              const char *text,
              const int icon,
              const bool depress)
{
  /* `rna_uiItemO`: la etiqueta se traduce con el contexto del OPERADOR. */
  wmOperatorType *ot = WM_operatortype_find(opname, false);
  if (ot == nullptr || ot->srna == nullptr) {
    fprintf(stderr, "Herramientas: operador desconocido '%s'.\n", opname);
    return PointerRNA_NULL;
  }
  std::optional<blender::StringRef> t;
  if (text != nullptr) {
    t = (text[0] != '\0' && BLT_translate_iface()) ?
            blender::StringRef(BLT_pgettext(RNA_struct_translation_context(ot->srna), text)) :
            blender::StringRef(text);
  }
  return layout->op(ot,
                    t,
                    icon,
                    uiLayoutGetOperatorContext(layout),
                    depress ? UI_ITEM_O_DEPRESS : UI_ITEM_NONE);
}

void popover(uiLayout *layout, const bContext *C, const char *panel, const char *text, const int icon)
{
  /* `rna_uiItemPopoverPanel`. */
  uiItemPopoverPanel(layout, C, panel, ui_text(text, nullptr), icon);
}

uiLayout &row(uiLayout *layout, const bool align, const char *heading, const char *heading_ctxt)
{
  /* `rna_uiLayoutRowWithHeading`: desde Python SIEMPRE se usa la variante con
   * encabezado, con "" si no lo hay. */
  return layout->row(align, ui_text(heading, heading_ctxt).value_or(""));
}

uiLayout &column(uiLayout *layout, const bool align, const char *heading, const char *heading_ctxt)
{
  /* `rna_uiLayoutColumnWithHeading`: igual, siempre con encabezado. */
  return layout->column(align, ui_text(heading, heading_ctxt).value_or(""));
}

/** \} */

}  // namespace flipendo::ui::settings
