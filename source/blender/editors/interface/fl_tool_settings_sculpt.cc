/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * `draw_settings` de las herramientas de escultura que no caben en filas. Transliteracion
 * linea a linea de `_defs_sculpt` (`scripts/startup/bl_ui/space_toolsystem_toolbar.py`,
 * hacia las lineas 1478-1881): las cuatro de lazo (`mask_lasso`, `hide_lasso`,
 * `face_set_lasso`, `trim_lasso`) y los dos filtros que cambian de controles segun el
 * tipo (`mesh_filter`, `color_filter`).
 *
 * Las cuatro de lazo tienen el mismo cuerpo salvo el operador y las propiedades de la
 * primera parte. Se mantienen como cuatro funciones completas, igual que en el Python,
 * para poder revisarlas con el Python al lado; lo que el Python si comparte
 * (`_defs_sculpt.draw_lasso_stroke_settings`) se comparte aqui tambien.
 *
 * Como se llega a cada rama, segun quien llama (ver `fl_toolbar_ui.cc`):
 *   - Cabecera de herramienta (`extra` falso, region `TOOL_HEADER`): propiedades del
 *     gesto y el boton del popover "Stroke".
 *   - Panel lateral "Active Tool" (`extra` falso, region `UI`): propiedades del gesto y
 *     los ajustes de trazo en linea, sin popover.
 *   - Popover `TOPBAR_PT_tool_settings_extra` (`extra` verdadero): SOLO los ajustes de
 *     trazo; las propiedades del gesto no se repiten.
 */

#include "BLI_utildefines.h"

#include "RNA_access.hh"

#include "FL_tool_settings_ui.hh"

namespace flipendo::ui::settings {

/**
 * `props.<name> == 'IDENTIFIER'` para una enumeracion, leida como la lee Python:
 * `pyrna_enum_to_py` pasa el valor a su identificador con `RNA_property_enum_identifier`
 * y el Python compara cadenas. Se compara igual, por identificador y no por valor, para
 * no depender de los enteros internos del operador.
 */
static bool enum_is(const bContext *C, PointerRNA *ptr, const char *name, const char *identifier)
{
  PropertyRNA *property = RNA_struct_find_property(ptr, name);
  if (property == nullptr) {
    return false;
  }
  const char *current = nullptr;
  RNA_property_enum_identifier(const_cast<bContext *>(C),
                               ptr,
                               property,
                               RNA_property_enum_get(ptr, property),
                               &current);
  return current != nullptr && STREQ(current, identifier);
}

/**
 * `_defs_sculpt.draw_lasso_stroke_settings` (`space_toolsystem_toolbar.py:1478`).
 *
 * Rareza conservada: con `draw_inline` pone `use_property_split` y quita
 * `use_property_decorate` sobre el `layout` RECIBIDO, no sobre una sub-maqueta. El cambio
 * no se deshace y alcanza a todo lo que se pinte despues en ese mismo `layout` (en el
 * panel lateral, por ejemplo, los ajustes de la herramienta de reserva si los hubiera).
 * El Python lo hace asi y asi se deja.
 *
 * `C` no esta en el Python: `layout.popover` lo toma de `bpy.context`, y aqui hay que
 * pasarlo explicito.
 */
static void draw_lasso_stroke_settings(const bContext *C,
                                       uiLayout *layout,
                                       PointerRNA *props,
                                       const bool draw_inline,
                                       const bool draw_popover)
{
  if (draw_inline) {
    prop(layout, props, "use_smooth_stroke", Prop().text("Stabilize Stroke"));

    uiLayoutSetPropSep(layout, true);
    uiLayoutSetPropDecorate(layout, false);
    uiLayout &col = column(layout);
    uiLayoutSetActive(&col, RNA_boolean_get(props, "use_smooth_stroke"));
    prop(&col, props, "smooth_stroke_radius", Prop().text("Radius").slider());
    prop(&col, props, "smooth_stroke_factor", Prop().text("Factor").slider());
  }

  if (draw_popover) {
    popover(layout, C, "TOPBAR_PT_tool_settings_extra", "Stroke");
  }
}

/* -------------------------------------------------------------------- */
/** \name Gestos de lazo
 *
 * Rarezas comunes a las cuatro, conservadas:
 *   - `extra` se REASIGNA dentro de la funcion: fuera de la cabecera se pone a verdadero
 *     para que el panel lateral pinte los ajustes de trazo en linea. Aqui se copia a una
 *     variable local, porque el parametro no deberia cambiar para quien llama.
 *   - El Python mira `bpy.context.region` y no el `_context` que recibe. Mientras se
 *     dibuja son el mismo contexto, asi que aqui se lee de `C`.
 *   - Si `tool.operator_properties` falla, el Python lanza una excepcion y no pinta nada
 *     mas; por eso se sale en cuanto las propiedades salen nulas. Tambien evita leer
 *     `use_smooth_stroke` de un puntero vacio.
 * \{ */

void draw_mask_lasso(const bContext *C, uiLayout *layout, bToolRef *tref, const bool extra_arg)
{
  bool extra = extra_arg;
  bool draw_popover = false;
  PointerRNA props = op_props(tref, "paint.mask_lasso_gesture");
  if (props.data == nullptr) {
    return;
  }

  if (!extra) {
    prop(layout, &props, "use_front_faces_only", Prop().expand(false));
    const bool region_is_header = region_is_tool_header(C);
    if (region_is_header) {
      draw_popover = true;
    }
    else {
      extra = true;
    }
  }

  draw_lasso_stroke_settings(C, layout, &props, extra, draw_popover);
}

void draw_hide_lasso(const bContext *C, uiLayout *layout, bToolRef *tref, const bool extra_arg)
{
  bool extra = extra_arg;
  bool draw_popover = false;
  PointerRNA props = op_props(tref, "paint.hide_show_lasso_gesture");
  if (props.data == nullptr) {
    return;
  }

  if (!extra) {
    prop(layout, &props, "area", Prop().expand(false));
    const bool region_is_header = region_is_tool_header(C);
    if (region_is_header) {
      draw_popover = true;
    }
    else {
      extra = true;
    }
  }

  draw_lasso_stroke_settings(C, layout, &props, extra, draw_popover);
}

void draw_face_set_lasso(const bContext *C,
                         uiLayout *layout,
                         bToolRef *tref,
                         const bool extra_arg)
{
  bool extra = extra_arg;
  bool draw_popover = false;
  PointerRNA props = op_props(tref, "sculpt.face_set_lasso_gesture");
  if (props.data == nullptr) {
    return;
  }

  if (!extra) {
    prop(layout, &props, "use_front_faces_only", Prop().expand(false));
    const bool region_is_header = region_is_tool_header(C);
    if (region_is_header) {
      draw_popover = true;
    }
    else {
      extra = true;
    }
  }

  draw_lasso_stroke_settings(C, layout, &props, extra, draw_popover);
}

void draw_trim_lasso(const bContext *C, uiLayout *layout, bToolRef *tref, const bool extra_arg)
{
  bool extra = extra_arg;
  bool draw_popover = false;
  PointerRNA props = op_props(tref, "sculpt.trim_lasso_gesture");
  if (props.data == nullptr) {
    return;
  }

  if (!extra) {
    prop(layout, &props, "trim_solver", Prop().expand(false));
    prop(layout, &props, "trim_mode", Prop().expand(false));
    prop(layout, &props, "trim_orientation", Prop().expand(false));
    prop(layout, &props, "trim_extrude_mode", Prop().expand(false));
    prop(layout, &props, "use_cursor_depth", Prop().expand(false));
    const bool region_is_header = region_is_tool_header(C);
    if (region_is_header) {
      draw_popover = true;
    }
    else {
      extra = true;
    }
  }

  draw_lasso_stroke_settings(C, layout, &props, extra, draw_popover);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Filtros
 *
 * Rareza conservada: en el Python su `draw_settings(_context, layout, tool)` NO acepta
 * `extra`, y `TOPBAR_PT_tool_settings_extra.draw` llama SIEMPRE a
 * `item.draw_settings(context, layout, tool, extra=True)` sobre la herramienta activa
 * (`space_topbar.py`, linea 86 en la 4.5 instalada). Con un filtro activo eso lanza
 * `TypeError` y el popover sale vacio. Solo pasa si el popover llega a abrirse con un
 * filtro activo (los filtros no ponen su boton), pero se reproduce igual: con `extra` no
 * se pinta nada.
 * \{ */

void draw_mesh_filter(const bContext *C, uiLayout *layout, bToolRef *tref, const bool extra)
{
  /* El `TypeError` del Python con `extra=True`: ver el comentario del bloque. */
  if (extra) {
    return;
  }
  PointerRNA props = op_props(tref, "sculpt.mesh_filter");
  if (props.data == nullptr) {
    return;
  }
  prop(layout, &props, "type", Prop().expand(false));
  prop(layout, &props, "strength");
  /* Rareza conservada: una fila alineada con un solo elemento. `deform_axis` es una
   * enumeracion de bits (X, Y, Z) y se pinta como botones pegados dentro de la fila. */
  uiLayout &row_ = row(layout, true);
  prop(&row_, &props, "deform_axis");
  prop(layout, &props, "orientation", Prop().expand(false));
  if (enum_is(C, &props, "type", "SURFACE_SMOOTH")) {
    prop(layout, &props, "surface_smooth_shape_preservation", Prop().expand(false));
    prop(layout, &props, "surface_smooth_current_vertex", Prop().expand(false));
  }
  else if (enum_is(C, &props, "type", "SHARPEN")) {
    prop(layout, &props, "sharpen_smooth_ratio", Prop().expand(false));
    prop(layout, &props, "sharpen_intensify_detail_strength", Prop().expand(false));
    prop(layout, &props, "sharpen_curvature_smooth_iterations", Prop().expand(false));
  }
}

void draw_color_filter(const bContext *C, uiLayout *layout, bToolRef *tref, const bool extra)
{
  /* El `TypeError` del Python con `extra=True`: ver el comentario del bloque. */
  if (extra) {
    return;
  }
  PointerRNA props = op_props(tref, "sculpt.color_filter");
  if (props.data == nullptr) {
    return;
  }
  prop(layout, &props, "type", Prop().expand(false));
  /* Rareza conservada: el color va EN MEDIO, entre el tipo y la fuerza, no al final. */
  if (enum_is(C, &props, "type", "FILL")) {
    prop(layout, &props, "fill_color", Prop().expand(false));
  }
  prop(layout, &props, "strength");
}

/** \} */

}  // namespace flipendo::ui::settings
