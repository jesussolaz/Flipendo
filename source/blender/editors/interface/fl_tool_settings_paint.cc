/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Ajustes de las herramientas de pintado de pesos que no caben en filas declarativas.
 * Transliteracion linea a linea de los `draw_settings` de `_defs_weight_paint`
 * (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:2080` y `:2111`).
 *
 * De las familias de pintado solo estas dos tienen ajustes: el resto son pinceles puros
 * y los pinta el panel del pincel, no la herramienta.
 *
 * Ninguno de los dos `draw_settings` del Python acepta `extra`. El popover
 * `TOPBAR_PT_tool_settings_extra` del Python los llamaba con `extra=True` y habria
 * saltado un TypeError; en la practica nunca pasaba, porque ese popover solo lo abren
 * las herramientas que lo pintan en su propia cabecera, y estas dos no lo hacen. Aqui
 * `extra` no dibuja nada, que es lo mismo que pintaban antes con las filas vacias.
 */

#include <cstdio>

#include "RNA_access.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "FL_tool_settings_ui.hh"

namespace flipendo::ui::settings {

/* -------------------------------------------------------------------- */
/** \name Ayudantes de `bl_ui/properties_paint_common.py`
 * \{ */

/**
 * `UnifiedPaintPanel.prop_unified` (`properties_paint_common.py:260`), solo con lo que usa
 * `gradient`: sin `pressure_name`, y con `icon` y `text` en sus valores por defecto
 * (`'NONE'` y `None`), que por eso no son argumentos aqui.
 *
 * Devuelve la fila como el Python, aunque `gradient` la descarta.
 */
static uiLayout *prop_unified(uiLayout *layout,
                              const bContext *C,
                              PointerRNA *brush,
                              const char *prop_name,
                              const char *unified_name,
                              const bool slider,
                              const bool header)
{
  uiLayout *row = &settings::row(layout, true);
  PointerRNA ts = tool_settings(C);
  PointerRNA ups = pointer_get(&ts, "unified_paint_settings");
  PointerRNA *prop_owner = brush;
  /* `if unified_name and getattr(ups, unified_name)`: la cadena vacia es falsa. */
  const bool has_unified_name = unified_name != nullptr && unified_name[0] != '\0';
  if (has_unified_name && ups.data != nullptr && RNA_boolean_get(&ups, unified_name)) {
    prop_owner = &ups;
  }

  prop(row, prop_owner, prop_name, Prop().icon(ICON_NONE).text(nullptr).slider(slider));

  /* `if pressure_name: row.prop(brush, pressure_name, text="")`: `gradient` no lo pasa. */

  if (has_unified_name && !header) {
    /* NOTE del Python: los ajustes unificados no se pintan en la cabecera, para no
     * recargarla (D5928#136281). */
    prop(row, &ups, unified_name, Prop().text("").icon(ICON_BRUSHES_ALL));
  }

  return row;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name `_defs_weight_paint`
 * \{ */

/**
 * `_defs_weight_paint.sample_weight.draw_settings` (`space_toolsystem_toolbar.py:2080`).
 *
 * El texto se formatea ANTES de llegar a `layout.label`, que despues lo intenta traducir
 * entero: "Weight: 0.500" no esta en ningun catalogo, asi que "Weight" sale siempre en
 * ingles. Es una rareza del Python y se conserva pasando la cadena ya formateada a
 * `label`, que traduce igual que RNA.
 */
void draw_sample_weight(const bContext *C, uiLayout *layout, bToolRef * /*tref*/, const bool extra)
{
  if (extra) {
    return;
  }
  PointerRNA ts = tool_settings(C);
  PointerRNA ups = pointer_get(&ts, "unified_paint_settings");
  if (ups.data == nullptr) {
    /* Sin `tool_settings` el Python salta con AttributeError y no pinta nada. */
    return;
  }
  float weight;
  if (RNA_boolean_get(&ups, "use_unified_weight")) {
    weight = RNA_float_get(&ups, "weight");
  }
  else {
    PointerRNA wpaint = pointer_get(&ts, "weight_paint");
    if (wpaint.data == nullptr) {
      /* `context.tool_settings.weight_paint.brush` con `weight_paint` a None: AttributeError
       * en el Python, que corta aqui el dibujo. */
      return;
    }
    /* El Python lee `weight_paint.brush` dos veces, en el `elif` y en su cuerpo; las dos
     * dan el mismo pincel, asi que basta con una. */
    PointerRNA brush = pointer_get(&wpaint, "brush");
    if (brush.data != nullptr) {
      weight = RNA_float_get(&brush, "weight");
    }
    else {
      return;
    }
  }
  /* `"Weight: {:.3f}".format(weight)`: el float de Python es el `double` del valor RNA, y
   * `%.3f` redondea igual que `{:.3f}` (los dos parten del valor binario exacto). */
  char text[64];
  std::snprintf(text, sizeof(text), "Weight: %.3f", double(weight));
  label(layout, text);
}

/**
 * `_defs_weight_paint.gradient.draw_settings` (`space_toolsystem_toolbar.py:2111`).
 *
 * Rareza conservada: pasa `header=True` a `prop_unified` tambien cuando se pinta en el
 * panel lateral "Active Tool", asi que el interruptor de ajustes unificados no sale en
 * ninguno de los dos sitios.
 */
void draw_gradient(const bContext *C, uiLayout *layout, bToolRef *tref, const bool extra)
{
  if (extra) {
    return;
  }
  PointerRNA ts = tool_settings(C);
  PointerRNA wpaint = pointer_get(&ts, "weight_paint");
  if (wpaint.data == nullptr) {
    /* `context.tool_settings.weight_paint.brush` con `weight_paint` a None: AttributeError
     * en el Python, que corta el dibujo antes de la fila del tipo y del popover. */
    return;
  }
  PointerRNA brush = pointer_get(&wpaint, "brush");
  if (brush.data != nullptr) {
    prop_unified(layout, C, &brush, "weight", "use_unified_weight", true, true);
    /* Sin `slider`: la fuerza sale como campo numerico normal, no como barra. */
    prop_unified(layout, C, &brush, "strength", "use_unified_strength", false, true);
  }

  PointerRNA props = op_props(tref, "paint.weight_gradient");
  uiLayout *row = &settings::row(layout);
  prop(row, &props, "type", Prop().expand());
  row = &settings::row(layout);
  /* El panel del popover sigue definido en `space_view3d_toolbar.py:884`; aqui solo se
   * pinta el boton, por nombre, como hace `rna_uiItemPopoverPanel`. */
  popover(row, C, "VIEW3D_PT_tools_weight_gradient");
}

/** \} */

}  // namespace flipendo::ui::settings
