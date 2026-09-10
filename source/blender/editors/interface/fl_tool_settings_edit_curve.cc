/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Ajustes de `builtin.draw` en edicion de curva: transliteracion linea a linea de
 * `curve_draw_settings` (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:1167`).
 *
 * La misma funcion la usa `_defs_edit_curves.draw` (`:1325`), a traves de un envoltorio
 * de una linea que solo reenvia `extra`. Por eso las dos declaraciones apuntan aqui.
 *
 * Se pinta en dos sitios. En la cabecera de herramienta (`extra` falso) solo van tres
 * cosas y el popover "..."; en ese popover (`TOPBAR_PT_tool_settings_extra`, `extra`
 * verdadero) la region sigue siendo la cabecera, porque el popover hereda la region
 * desde la que se abrio, y se pinta el resto con las ramas de cabecera. En el panel
 * lateral "Active Tool" la region no es la cabecera y se pinta todo.
 */

#include "BLI_string.h"

#include "RNA_access.hh"

#include "UI_interface_layout.hh"

#include "FL_tool_settings_ui.hh"

namespace flipendo::ui::settings {

/**
 * `ptr.<name> == '<identifier>'` para una enumeracion, comparando IDENTIFICADORES como
 * el Python y no valores, igual que `workspace_tool_type_is` en `fl_toolbar_ui.cc`.
 */
static bool enum_is(const bContext *C, PointerRNA *ptr, const char *name, const char *identifier)
{
  PropertyRNA *property = RNA_struct_find_property(ptr, name);
  if (property == nullptr) {
    return false;
  }
  const char *current = nullptr;
  RNA_property_enum_identifier(
      const_cast<bContext *>(C), ptr, property, RNA_property_enum_get(ptr, property), &current);
  return current != nullptr && STREQ(current, identifier);
}

void draw_edit_curve_draw(const bContext *C, uiLayout *layout, bToolRef *tref, const bool extra)
{
  /* Tool settings initialize operator options. */
  PointerRNA ts = tool_settings(C);
  PointerRNA cps = pointer_get(&ts, "curve_paint_settings");
  /* En el Python `context.region.type` se lee una vez y se compara despues; aqui igual. */
  const bool region_is_header = region_is_tool_header(C);

  /* Sin escena no hay `curve_paint_settings`: el Python lanzaria una excepcion al leer
   * `cps.curve_type` y la cabecera se quedaria sin nada detras. Se sale antes para no
   * leer de un puntero nulo. */
  if (cps.data == nullptr) {
    return;
  }

  if (region_is_header) {
    if (!extra) {
      prop(layout, &cps, "curve_type", Prop().text(""));
      prop(layout, &cps, "depth_mode", Prop().expand());
      popover(layout, C, "TOPBAR_PT_tool_settings_extra", "...");
      return;
    }
  }

  /* `layout.use_property_split = True` y `layout.use_property_decorate = False`. */
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  if (!region_is_header) {
    prop(layout, &cps, "curve_type");
    layout->separator();
  }
  if (enum_is(C, &cps, "curve_type", "BEZIER")) {
    prop(layout, &cps, "fit_method");
    prop(layout, &cps, "error_threshold");
    uiLayout *row_corners;
    if (!region_is_header) {
      row_corners = &row(layout, true, "Detect Corners");
    }
    else {
      row_corners = &row(layout, true, "Corners");
    }
    prop(row_corners, &cps, "use_corners_detect", Prop().text(""));
    uiLayout &sub = row(row_corners, true);
    /* `sub.active = cps.use_corners_detect`. */
    uiLayoutSetActive(&sub, RNA_boolean_get(&cps, "use_corners_detect"));
    prop(&sub, &cps, "corner_angle", Prop().text(""));
    layout->separator();
  }

  uiLayout *col = &column(layout, true);
  prop(col, &cps, "radius_taper_start", Prop().text("Taper Start").slider());
  prop(col, &cps, "radius_taper_end", Prop().text("End").slider());
  col = &column(layout, true);
  prop(col, &cps, "radius_min", Prop().text("Radius Min"));
  prop(col, &cps, "radius_max", Prop().text("Max"));
  prop(col, &cps, "use_pressure_radius");

  if (!region_is_header || enum_is(C, &cps, "depth_mode", "SURFACE")) {
    layout->separator();
  }

  if (!region_is_header) {
    uiLayout &row_depth = row(layout);
    prop(&row_depth, &cps, "depth_mode", Prop().expand());
  }
  if (enum_is(C, &cps, "depth_mode", "SURFACE")) {
    col = &column(layout);
    prop(col, &cps, "use_project_only_selected");
    prop(col, &cps, "surface_offset");
    prop(col, &cps, "use_offset_absolute");
    prop(col, &cps, "use_stroke_endpoints");
    if (RNA_boolean_get(&cps, "use_stroke_endpoints")) {
      /* Rareza del Python conservada: una columna alineada para UNA sola propiedad,
       * colgada de `layout` y no de `col`. */
      uiLayout &colsub = column(layout, true);
      prop(&colsub, &cps, "surface_plane");
    }
  }

  PointerRNA props = op_props(tref, "curves.draw");
  col = &column(layout, true);
  prop(col, &props, "is_curve_2d", Prop().text("Curve 2D"));
  prop(col, &props, "bezier_as_nurbs", Prop().text("As NURBS"));
}

}  // namespace flipendo::ui::settings
