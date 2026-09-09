/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Herramientas del editor de secuencias. Transliteracion de `_defs_sequencer_generic` y
 * `_defs_sequencer_select` (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:2932` y
 * `:3022`).
 *
 * Ninguna de las diez tiene ajustes con flujo de control: las tres que los llevan son
 * una sola `layout.prop` cada una, asi que caben enteras en filas `PropRow` y no queda
 * nada marcado como pendiente. Tampoco hay `draw_cursor` en este espacio.
 */

#include "BLT_translation.hh"

#include "fl_tool_defs_sequencer.hh"

namespace flipendo::toolsystem {

/* -------------------------------------------------------------------- */
/** \name `_defs_sequencer_generic`
 * \{ */

namespace defs_sequencer_generic {

const ToolDecl cursor = {
    /*idname*/ "builtin.cursor",
    /*label*/ N_("Cursor"),
    /*description*/ N_("Set the cursor location, drag to transform"),
    /*description_fn*/ nullptr,
    /*icon*/ "ops.generic.cursor",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "Preview Tool: Cursor",
    /*keymap_fallback*/ "Preview Tool: Cursor (fallback)",
};

/* El unico ajuste es el tipo de corte del operador de division (blando o duro), en una
 * fila expandida para que las dos opciones se vean a la vez en la cabecera. */
static const PropRow blade_settings[] = {
    {PropSource::Operator, "sequencer.split", "type", nullptr, PROP_ROW_EXPAND},
};

/* La cuchilla es la unica del grupo que vive en la linea de tiempo y no en la
 * previsualizacion, y de ahi que su keymap use otro prefijo. Tambien es la unica que
 * puede actuar de reserva. */
const ToolDecl blade = {
    /*idname*/ "builtin.blade",
    /*label*/ N_("Blade"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.sequencer.blade",
    /*cursor*/ "CROSSHAIR",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "Sequencer Tool: Blade",
    /*keymap_fallback*/ "Sequencer Tool: Blade (fallback)",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_KEYMAP_FALLBACK,
    /*settings*/ span(blade_settings),
};

/* El icono es el del muestreo de pesos porque este `sample` nunca llego a tener el suyo
 * (hay un XXX en el Python). Se copia tal cual: cambiarlo aqui seria un cambio de
 * aspecto disfrazado de migracion.
 *
 * No confundirla con `_defs_image_generic.sample`, que hace lo mismo en el editor de
 * imagen pero SI tiene ajustes y otro keymap. */
const ToolDecl sample = {
    /*idname*/ "builtin.sample",
    /*label*/ N_("Sample"),
    /*description*/ N_("Sample pixel values under the cursor"),
    /*description_fn*/ nullptr,
    /*icon*/ "ops.paint.weight_sample",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "Preview Tool: Sample",
    /*keymap_fallback*/ "Preview Tool: Sample (fallback)",
};

/* Las cuatro de transformacion son la version 2D de las de la vista 3D: mismos iconos y
 * mismos operadores, pero con los gizmos `SEQUENCER_GGT_gizmo2d*` de la
 * previsualizacion. El `op` no arrastra el operador a la ejecucion, solo sirve para
 * sacar el tooltip y las teclas de acceso. */
const ToolDecl translate = {
    /*idname*/ "builtin.move",
    /*label*/ N_("Move"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.transform.translate",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "SEQUENCER_GGT_gizmo2d_translate",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "Preview Tool: Move",
    /*keymap_fallback*/ "Preview Tool: Move (fallback)",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ "transform.translate",
};

const ToolDecl rotate = {
    /*idname*/ "builtin.rotate",
    /*label*/ N_("Rotate"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.transform.rotate",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "SEQUENCER_GGT_gizmo2d_rotate",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "Preview Tool: Rotate",
    /*keymap_fallback*/ "Preview Tool: Rotate (fallback)",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ "transform.rotate",
};

/* El icono y el operador se llaman `resize` aunque la herramienta se llame `scale`; el
 * idname va con la etiqueta y no con el operador. */
const ToolDecl scale = {
    /*idname*/ "builtin.scale",
    /*label*/ N_("Scale"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.transform.resize",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "SEQUENCER_GGT_gizmo2d_resize",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "Preview Tool: Scale",
    /*keymap_fallback*/ "Preview Tool: Scale (fallback)",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ "transform.resize",
};

/* La unica sin keymap de todo el espacio, y a proposito: no tiene accion por defecto
 * porque todo se hace arrastrando el gizmo combinado. Sin keymap tampoco hay variante de
 * reserva que registrar. */
const ToolDecl transform = {
    /*idname*/ "builtin.transform",
    /*label*/ N_("Transform"),
    /*description*/ N_("Supports any combination of grab, rotate, and scale at once"),
    /*description_fn*/ nullptr,
    /*icon*/ "ops.transform.transform",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "SEQUENCER_GGT_gizmo2d",
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
};

}  // namespace defs_sequencer_generic

/** \} */

/* -------------------------------------------------------------------- */
/** \name `_defs_sequencer_select`
 * \{ */

namespace defs_sequencer_select {

const ToolDecl select_preview = {
    /*idname*/ "builtin.select",
    /*label*/ N_("Tweak"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.generic.select",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "Preview Tool: Tweak",
    /*keymap_fallback*/ "Preview Tool: Tweak (fallback)",
};

/* Las dos cajas de seleccion pintan el mismo control - el modo de seleccion del
 * operador, sin etiqueta, expandido y solo con iconos - y comparten la tabla en vez de
 * repetirla, porque tambien comparten el operador. Lo unico que las separa es el keymap.
 *
 * El `use_property_split = False` del Python no viaja aqui: es estado de la fila que
 * dibuja, no una propiedad de la herramienta, y lo pone el pintor generico. */
static const PropRow box_settings[] = {
    {PropSource::Operator,
     "sequencer.select_box",
     "mode",
     nullptr,
     PROP_ROW_EXPAND | PROP_ROW_NO_TEXT | PROP_ROW_ICON_ONLY},
};

/* Misma herramienta que `box_preview` para el usuario, pero declarada aparte porque en
 * la linea de tiempo la seleccion en caja usa otro keymap. Fundirlas dejaria una de las
 * dos vistas sin sus atajos. */
const ToolDecl box_timeline = {
    /*idname*/ "builtin.select_box",
    /*label*/ N_("Select Box"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.generic.select_box",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "Sequencer Tool: Select Box",
    /*keymap_fallback*/ "Sequencer Tool: Select Box (fallback)",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(box_settings),
};

const ToolDecl box_preview = {
    /*idname*/ "builtin.select_box",
    /*label*/ N_("Select Box"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.generic.select_box",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "Preview Tool: Select Box",
    /*keymap_fallback*/ "Preview Tool: Select Box (fallback)",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(box_settings),
};

}  // namespace defs_sequencer_select

/** \} */

}  // namespace flipendo::toolsystem
