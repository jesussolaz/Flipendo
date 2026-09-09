/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Herramientas del editor de imagen y de su modo UV. Transliteracion de
 * `_defs_image_generic`, `_defs_image_uv_transform`, `_defs_image_uv_select` y
 * `_defs_image_uv_edit` (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:2425`,
 * `:2469`, `:2518` y `:2588`).
 *
 * Ninguna de las once tiene ajustes con flujo de control, asi que las cuatro que los
 * llevan caben enteras en filas `PropRow` y no queda nada marcado como pendiente.
 */

#include "BLT_translation.hh"

#include "fl_tool_defs_image_uv.hh"

namespace flipendo::toolsystem {

/* -------------------------------------------------------------------- */
/** \name `_defs_image_generic`
 * \{ */

namespace defs_image_generic {

const ToolDecl cursor = {
    /*idname*/ "builtin.cursor",
    /*label*/ N_("Cursor"),
    /*description*/ N_("Set the cursor location, drag to transform"),
    /*description_fn*/ nullptr,
    /*icon*/ "ops.generic.cursor",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "Image Editor Tool: Uv, Cursor",
};

/* El icono es el del muestreo de pesos porque `sample` nunca llego a tener el suyo (hay
 * un XXX en el Python). Se copia tal cual: cambiarlo aqui seria un cambio de aspecto
 * disfrazado de migracion. */
static const PropRow sample_settings[] = {
    {PropSource::Operator, "image.sample", "size"},
};

/* Unica del catalogo del editor de imagen cuyo keymap NO lleva el modo dentro: el
 * Python se lo da literal ("Image Editor Tool: Sample") en vez de dejar que se genere,
 * porque vive en el modo VIEW y no en el de UV. */
const ToolDecl sample = {
    /*idname*/ "builtin.sample",
    /*label*/ N_("Sample"),
    /*description*/ N_("Sample pixel values under the cursor"),
    /*description_fn*/ nullptr,
    /*icon*/ "ops.paint.weight_sample",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "Image Editor Tool: Sample",
    /*keymap_fallback*/ nullptr,
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(sample_settings),
};

}  // namespace defs_image_generic

/** \} */

/* -------------------------------------------------------------------- */
/** \name `_defs_image_uv_transform`
 * \{ */

namespace defs_image_uv_transform {

/* Las tres primeras declaran `operator` ademas del gizmo. No es redundante: el gizmo
 * hace el trabajo, pero el operador es de donde salen el tooltip y las teclas de acceso
 * cuando se consulta la herramienta. */
const ToolDecl translate = {
    /*idname*/ "builtin.move",
    /*label*/ N_("Move"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.transform.translate",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "IMAGE_GGT_gizmo2d_translate",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "Image Editor Tool: Uv, Move",
    /*keymap_fallback*/ nullptr,
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
    /*gizmo_group*/ "IMAGE_GGT_gizmo2d_rotate",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "Image Editor Tool: Uv, Rotate",
    /*keymap_fallback*/ nullptr,
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ "transform.rotate",
};

/* El idname dice `scale` y el operador `resize`: son nombres de dos epocas distintas y
 * el par tiene que quedarse asi, porque el idname es lo que se guarda en el fichero y
 * el operador lo que existe en el RNA. */
const ToolDecl scale = {
    /*idname*/ "builtin.scale",
    /*label*/ N_("Scale"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.transform.resize",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "IMAGE_GGT_gizmo2d_resize",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "Image Editor Tool: Uv, Scale",
    /*keymap_fallback*/ nullptr,
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ "transform.resize",
};

/* Sin keymap y sin operador a proposito: esta herramienta es solo el gizmo combinado,
 * asi que fuera de sus asas no hay accion que ejecutar. Por eso la linea base la da con
 * keymap=None mientras las otras tres si lo llevan. */
const ToolDecl transform = {
    /*idname*/ "builtin.transform",
    /*label*/ N_("Transform"),
    /*description*/ N_("Supports any combination of grab, rotate, and scale at once"),
    /*description_fn*/ nullptr,
    /*icon*/ "ops.transform.transform",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "IMAGE_GGT_gizmo2d",
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
};

}  // namespace defs_image_uv_transform

/** \} */

/* -------------------------------------------------------------------- */
/** \name `_defs_image_uv_select`
 * \{ */

namespace defs_image_uv_select {

const ToolDecl select = {
    /*idname*/ "builtin.select",
    /*label*/ N_("Tweak"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.generic.select",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "Image Editor Tool: Uv, Tweak",
};

/* Las tres de seleccion pintan el mismo control que sus gemelas del editor de nodos: el
 * modo de seleccion del operador, sin etiqueta, expandido y solo con iconos. Cambia
 * unicamente el operador, que aqui es del espacio de nombres `uv`. */
static const PropRow box_settings[] = {
    {PropSource::Operator,
     "uv.select_box",
     "mode",
     nullptr,
     PROP_ROW_EXPAND | PROP_ROW_NO_TEXT | PROP_ROW_ICON_ONLY},
};

const ToolDecl box = {
    /*idname*/ "builtin.select_box",
    /*label*/ N_("Select Box"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.generic.select_box",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "Image Editor Tool: Uv, Select Box",
    /*keymap_fallback*/ nullptr,
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(box_settings),
};

static const PropRow lasso_settings[] = {
    {PropSource::Operator,
     "uv.select_lasso",
     "mode",
     nullptr,
     PROP_ROW_EXPAND | PROP_ROW_NO_TEXT | PROP_ROW_ICON_ONLY},
};

const ToolDecl lasso = {
    /*idname*/ "builtin.select_lasso",
    /*label*/ N_("Select Lasso"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.generic.select_lasso",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "Image Editor Tool: Uv, Select Lasso",
    /*keymap_fallback*/ nullptr,
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(lasso_settings),
};

static const PropRow circle_settings[] = {
    {PropSource::Operator,
     "uv.select_circle",
     "mode",
     nullptr,
     PROP_ROW_EXPAND | PROP_ROW_NO_TEXT | PROP_ROW_ICON_ONLY},
    {PropSource::Operator, "uv.select_circle", "radius"},
};

/* El circulo pinta ademas su radio sobre la vista mientras esta activo. El dibujo va en
 * la fase de dibujado, con el resto de `draw_cursor`. */
const ToolDecl circle = {
    /*idname*/ "builtin.select_circle",
    /*label*/ N_("Select Circle"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.generic.select_circle",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "Image Editor Tool: Uv, Select Circle",
    /*keymap_fallback*/ nullptr,
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(circle_settings),
};

}  // namespace defs_image_uv_select

/** \} */

/* -------------------------------------------------------------------- */
/** \name `_defs_image_uv_edit`
 * \{ */

namespace defs_image_uv_edit {

/* La de la vista 3D lleva el gizmo `VIEW3D_GGT_tool_generic_handle_free`; esta se queda
 * sin gizmo porque nadie ha escrito la version 2D (hay un TODO en el Python). No es un
 * olvido de la migracion: la linea base la da con widget=None. */
const ToolDecl rip_region = {
    /*idname*/ "builtin.rip_region",
    /*label*/ N_("Rip Region"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.mesh.rip",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "Image Editor Tool: Uv, Rip Region",
    /*keymap_fallback*/ nullptr,
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_KEYMAP_FALLBACK,
};

}  // namespace defs_image_uv_edit

/** \} */

}  // namespace flipendo::toolsystem
