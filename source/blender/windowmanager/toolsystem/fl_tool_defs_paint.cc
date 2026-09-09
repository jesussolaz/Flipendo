/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Herramientas de pintado de vertices, de textura y de pesos. Transliteracion de
 * `_defs_vertex_paint`, `_defs_texture_paint` y `_defs_weight_paint`
 * (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:1920`, `:1962` y `:2032`).
 *
 * Doce de las quince son pinceles puros: `USE_BRUSHES` mas un `brush_type`, y ni
 * keymap ni gizmo ni ajustes. Los mismos tres nombres (`blur`, `average`, `smear`)
 * se repiten en pintado de vertices y en pintado de pesos con el mismo idname y
 * distinto icono; por eso cada modo tiene su espacio de nombres y no se comparte una
 * sola declaracion, que serviria el icono equivocado en uno de los dos modos.
 *
 * Los `poll_*` solo estan declarados en la cabecera: quien los necesita es la barra, y
 * hasta que las barras sean nativas no hay contexto donde probarlos.
 */

#include "BLT_translation.hh"

#include "fl_tool_defs_paint.hh"

namespace flipendo::toolsystem {

/* -------------------------------------------------------------------- */
/** \name `_defs_vertex_paint`
 * \{ */

namespace defs_vertex_paint {

const ToolDecl blur = {
    /*idname*/ "builtin_brush.blur",
    /*label*/ N_("Blur"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "brush.paint_vertex.blur",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
    /*brush_type*/ "BLUR",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES,
};

const ToolDecl average = {
    /*idname*/ "builtin_brush.average",
    /*label*/ N_("Average"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "brush.paint_vertex.average",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
    /*brush_type*/ "AVERAGE",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES,
};

const ToolDecl smear = {
    /*idname*/ "builtin_brush.smear",
    /*label*/ N_("Smear"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "brush.paint_vertex.smear",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
    /*brush_type*/ "SMEAR",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES,
};

}  // namespace defs_vertex_paint

/** \} */

/* -------------------------------------------------------------------- */
/** \name `_defs_texture_paint`
 * \{ */

namespace defs_texture_paint {

/* Sobra en el Python: ninguna barra la referencia, porque tanto la vista 3D como el
 * editor de imagen ponen su propio `_brush_tool`. Ese es el `builtin.brush` que sale
 * en la linea base, con etiqueta "Brush" e icono `brush.generic`; este otro conserva
 * los valores que declara el Python aqui, que ya nadie ve. Se traslada para no perder
 * la pista, no para enlazarla. */
const ToolDecl brush = {
    /*idname*/ "builtin.brush",
    /*label*/ N_("Paint"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "brush.sculpt.paint",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES,
};

/* La etiqueta dice "Blur" pero el idname y el tipo de pincel dicen "soften": el nombre
 * visible se cambio y el interno no, y cambiarlo ahora romperia los ficheros guardados
 * que ya tienen la herramienta activa. Copiar los dos tal cual es lo correcto. */
const ToolDecl blur = {
    /*idname*/ "builtin_brush.soften",
    /*label*/ N_("Blur"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "brush.paint_texture.soften",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
    /*brush_type*/ "SOFTEN",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES,
};

const ToolDecl smear = {
    /*idname*/ "builtin_brush.smear",
    /*label*/ N_("Smear"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "brush.paint_texture.smear",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
    /*brush_type*/ "SMEAR",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES,
};

const ToolDecl clone = {
    /*idname*/ "builtin_brush.clone",
    /*label*/ N_("Clone"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "brush.paint_texture.clone",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
    /*brush_type*/ "CLONE",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES,
};

const ToolDecl fill = {
    /*idname*/ "builtin_brush.fill",
    /*label*/ N_("Fill"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "brush.paint_texture.fill",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
    /*brush_type*/ "FILL",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES,
};

/* Pinta la mascara de esteneil, que es una capa mas de la textura; no tiene nada que
 * ver con la mascara de caras que consulta `poll_select_mask`. */
const ToolDecl mask = {
    /*idname*/ "builtin_brush.mask",
    /*label*/ N_("Mask"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "brush.paint_texture.mask",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
    /*brush_type*/ "MASK",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES,
};

}  // namespace defs_texture_paint

/** \} */

/* -------------------------------------------------------------------- */
/** \name `_defs_weight_paint`
 * \{ */

namespace defs_weight_paint {

const ToolDecl blur = {
    /*idname*/ "builtin_brush.blur",
    /*label*/ N_("Blur"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "brush.paint_weight.blur",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
    /*brush_type*/ "BLUR",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES,
};

const ToolDecl average = {
    /*idname*/ "builtin_brush.average",
    /*label*/ N_("Average"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "brush.paint_weight.average",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
    /*brush_type*/ "AVERAGE",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES,
};

const ToolDecl smear = {
    /*idname*/ "builtin_brush.smear",
    /*label*/ N_("Smear"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "brush.paint_weight.smear",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
    /*brush_type*/ "SMEAR",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES,
};

/* Sus ajustes son una etiqueta con el peso activo, y de donde sale ese peso depende de
 * si los ajustes unificados estan puestos y de si hay pincel; ademas es `layout.label`
 * con el numero ya formateado, no un `prop`. Nada de eso cabe en filas declarativas,
 * asi que queda pendiente en vez de traducido a medias. */
const ToolDecl sample_weight = {
    /*idname*/ "builtin.sample_weight",
    /*label*/ N_("Sample Weight"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.paint.weight_sample",
    /*cursor*/ "EYEDROPPER",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Paint Weight, Sample Weight",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ {},
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
    /*settings_pending*/ true,
};

const ToolDecl sample_weight_group = {
    /*idname*/ "builtin.sample_vertex_group",
    /*label*/ N_("Sample Vertex Group"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.paint.weight_sample_group",
    /*cursor*/ "EYEDROPPER",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Paint Weight, Sample Vertex Group",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
};

/* Pendiente por partida doble: el peso y la fuerza los pinta `prop_unified`, que elige
 * entre la propiedad del pincel y la unificada segun un interruptor, y encima abre un
 * popover a `VIEW3D_PT_tools_weight_gradient`, que sigue siendo un panel de Python.
 * Solo la fila del tipo de degradado seria declarativa, y media traduccion es peor que
 * ninguna: parece completa y no lo esta. */
const ToolDecl gradient = {
    /*idname*/ "builtin.gradient",
    /*label*/ N_("Gradient"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.paint.weight_gradient",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Paint Weight, Gradient",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ {},
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
    /*settings_pending*/ true,
};

}  // namespace defs_weight_paint

/** \} */

}  // namespace flipendo::toolsystem
