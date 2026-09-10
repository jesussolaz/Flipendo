/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * `draw_settings` de las herramientas de anotacion. Transliteracion linea a linea de
 * `_defs_annotate.draw_settings_common`
 * (`scripts/startup/bl_ui/space_toolsystem_toolbar.py`, hacia la linea 164).
 *
 * En el Python es UNA funcion que comparten `builtin.annotate`, `builtin.annotate_line`
 * y `builtin.annotate_polygon`, y que mira el idname de la propia herramienta para
 * pintar cosas distintas. Aqui igual: una sola funcion, `draw_annotate_common`, con los
 * tres ToolDecl apuntando a ella. El borrador no la usa (su unico ajuste es una fila
 * declarativa en `fl_tool_defs_annotate.cc`).
 *
 * El popover a `TOPBAR_PT_annotation_layers` solo pone el boton: el contenido lo pinta
 * su panel (`space_topbar.py`) al abrirse, y se busca por nombre en
 * `uiItemPopoverPanel`, lo mismo que hace `rna_uiItemPopoverPanel`.
 *
 * El Python no acepta `extra`; aqui se recibe y se ignora.
 */

#include <string>

#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "DNA_space_enums.h"
#include "DNA_workspace_types.h"

#include "RNA_access.hh"

#include "FL_tool_settings_ui.hh"

namespace flipendo::ui::settings {

/**
 * `getattr(ptr, name)` de una propiedad enum NO bandera, como cadena. Reproduce
 * `pyrna_enum_to_py` (`python/intern/bpy_rna.cc`): el identificador del elemento cuyo
 * valor casa, resuelto con el contexto (hay enums dinamicos, como `active_note`); y si
 * ninguno casa, la cadena vacia, NUNCA `None`.
 */
static std::string enum_identifier_get(const bContext *C, PointerRNA *ptr, const char *name)
{
  if (ptr == nullptr || ptr->data == nullptr) {
    return "";
  }
  PropertyRNA *property = RNA_struct_find_property(ptr, name);
  if (property == nullptr) {
    return "";
  }
  const int value = RNA_property_enum_get(ptr, property);
  const char *identifier = nullptr;
  if (RNA_property_enum_identifier(
          const_cast<bContext *>(C), ptr, property, value, &identifier))
  {
    return identifier;
  }
  return "";
}

/**
 * `gpd.layers.active_note`. `layers` es una coleccion con struct propio
 * (`GreasePencilLayers`, sobre el mismo `bGPdata`); pedir un atributo a la coleccion es
 * pedirselo a ese struct, que es lo que hace `RNA_property_collection_type_get`, igual
 * que `pyrna_prop_collection_getattro`.
 */
static std::string annotation_active_note_get(const bContext *C, PointerRNA *gpd)
{
  PropertyRNA *layers_prop = RNA_struct_find_property(gpd, "layers");
  if (layers_prop == nullptr) {
    return "";
  }
  PointerRNA layers;
  if (!RNA_property_collection_type_get(gpd, layers_prop, &layers)) {
    return "";
  }
  return enum_identifier_get(C, &layers, "active_note");
}

/**
 * `text[:maxw - 5] + '..' + text[-3:]`. Python corta por caracteres, no por bytes, y el
 * nombre de la capa puede llevar caracteres de varios bytes: se cuenta y se corta en
 * UTF-8 para dar exactamente el mismo texto.
 */
static std::string python_slice_head_tail(const std::string &text, const int head, const int tail)
{
  const size_t text_len = text.size();
  const int chars = int(BLI_strlen_utf8(text.c_str()));
  const int head_end = BLI_str_utf8_offset_from_index(text.c_str(), text_len, head);
  const int tail_start = BLI_str_utf8_offset_from_index(text.c_str(), text_len, chars - tail);
  return text.substr(0, head_end) + ".." + text.substr(tail_start);
}

void draw_annotate_common(const bContext *C,
                          uiLayout *layout,
                          bToolRef *tref,
                          const bool /*extra*/)
{
  PointerRNA gpd = context_pointer(C, "annotation_data");
  const bool region_tool_header = region_is_tool_header(C);

  if (gpd.data != nullptr) {
    /* Rareza conservada: el Python pregunta `if gpd.layers.active_note is not None:`,
     * pero un enum nunca sale `None` desde RNA (sin elemento que case sale ""), asi que
     * esa condicion es siempre cierta y su `else: text = ""` no se da nunca. Solo se
     * transcribe la rama viva; sin capa activa, `text` sale "" igual que en el Python. */
    std::string text = annotation_active_note_get(C, &gpd);
    const int maxw = 25;
    if (int(BLI_strlen_utf8(text.c_str())) > maxw) {
      text = python_slice_head_tail(text, maxw - 5, 3);
    }

    PointerRNA gpl = context_pointer(C, "active_annotation_layer");
    if (gpl.data != nullptr) {
      const int space_data_type = space_type(C);
      if (ELEM(space_data_type, SPACE_VIEW3D, SPACE_SEQ, SPACE_IMAGE, SPACE_NODE)) {
        label(layout, "Annotation:");
        uiLayout *sub;
        if (region_tool_header) {
          sub = &layout->split(0.5f, true);
          uiLayoutSetUnitsX(sub, 6.5f);
          prop(sub, &gpl, "color", Prop().text(""));
        }
        else {
          sub = &row(layout, true);
          prop(sub, &gpl, "color", Prop().text(""));
        }
        popover(sub, C, "TOPBAR_PT_annotation_layers", text.c_str());
      }
      else if (space_data_type == SPACE_PROPERTIES) {
        uiLayout *row_ = &row(layout, true);
        prop(row_, &gpl, "color", Prop().text("Annotation"));
        popover(row_, C, "TOPBAR_PT_annotation_layers", text.c_str());
      }
      else {
        label(layout, "Annotation:");
        prop(layout, &gpl, "color", Prop().text(""));
      }
    }
  }

  /* `tool.space_type`: el espacio de la herramienta, no el del contexto
   * (`context.space_data.type`, que es el que se mira arriba). */
  const int tool_space_type = tref->space_type;
  PointerRNA tool_settings_ptr = tool_settings(C);

  if (tool_space_type == SPACE_VIEW3D) {
    uiLayout *row_;
    if (region_tool_header) {
      row_ = &row(layout, true);
    }
    else {
      row_ = &column(&row(layout), true);
    }
    prop(row_, &tool_settings_ptr, "annotation_stroke_placement_view3d", Prop().text("Placement"));
    /* Rareza conservada: `annotation_stroke_placement_view3d` ya no tiene elemento
     * 'STROKE' (solo CURSOR, VIEW y SURFACE, en `rna_scene.cc`), asi que de los dos solo
     * casa 'SURFACE'. Se compara con los dos, como el Python. */
    const std::string placement = enum_identifier_get(
        C, &tool_settings_ptr, "annotation_stroke_placement_view3d");
    if (ELEM(placement, "SURFACE", "STROKE")) {
      prop(row_, &tool_settings_ptr, "use_annotation_stroke_endpoints");
      prop(row_, &tool_settings_ptr, "use_annotation_project_only_selected");
    }
  }
  else if (ELEM(tool_space_type, SPACE_IMAGE, SPACE_NODE, SPACE_SEQ, SPACE_CLIP)) {
    uiLayout *row_ = &row(layout, true);
    prop(row_, &tool_settings_ptr, "annotation_stroke_placement_view2d", Prop().text("Placement"));
  }

  if (STREQ(tref->idname, "builtin.annotate_line")) {
    PointerRNA props = op_props(tref, "gpencil.annotate");
    if (region_tool_header) {
      uiLayout *row_ = &row(layout);
      uiLayoutSetUnitsX(row_, 15.0f);
      prop(row_, &props, "arrowstyle_start", Prop().text("Start"));
      row_->separator();
      prop(row_, &props, "arrowstyle_end", Prop().text("End"));
    }
    else {
      uiLayout *col = &column(&row(layout), true);
      prop(col, &props, "arrowstyle_start", Prop().text("Style Start"));
      prop(col, &props, "arrowstyle_end", Prop().text("End"));
    }
  }
  else if (STREQ(tref->idname, "builtin.annotate")) {
    PointerRNA props = op_props(tref, "gpencil.annotate");
    const bool use_stabilizer = props.data != nullptr && RNA_boolean_get(&props, "use_stabilizer");
    if (region_tool_header) {
      /* Rareza conservada: `row` solo lleva la casilla, y `subrow` no cuelga de ella sino
       * de `layout`: son dos filas hermanas, no una dentro de otra. */
      uiLayout *row_ = &row(layout);
      prop(row_, &props, "use_stabilizer", Prop().text("Stabilize Stroke"));
      uiLayout *subrow = &row(layout, false);
      uiLayoutSetActive(subrow, use_stabilizer);
      prop(subrow, &props, "stabilizer_radius", Prop().text("Radius").slider());
      prop(subrow, &props, "stabilizer_factor", Prop().text("Factor").slider());
    }
    else {
      prop(layout, &props, "use_stabilizer", Prop().text("Stabilize Stroke"));
      uiLayout *col = &column(layout, false);
      uiLayoutSetActive(col, use_stabilizer);
      prop(col, &props, "stabilizer_radius", Prop().text("Radius").slider());
      prop(col, &props, "stabilizer_factor", Prop().text("Factor").slider());
    }
  }
}

}  // namespace flipendo::ui::settings
