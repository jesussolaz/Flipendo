/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Herramientas de los modos de lapiz de cera que no son el de pintar. Transliteracion
 * de `_defs_grease_pencil_edit`, `_defs_grease_pencil_sculpt`, `_defs_gpencil_weight`,
 * `_defs_grease_pencil_weight` y `_defs_grease_pencil_vertex`
 * (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:2380`, `:2684`, `:2711`, `:2716`
 * y `:2748`).
 *
 * Nueve de las once son pinceles, y en el Python los pinceles son declaraciones de tres
 * campos: el motor ya sabe pintar con ellos, la herramienta solo dice CUAL. Por eso
 * salen sin keymap ni operador: los atajos vienen del modo, no de la herramienta.
 */

#include "BLT_translation.hh"

#include "fl_tool_defs_grease_pencil_misc.hh"

namespace flipendo::toolsystem {

/* -------------------------------------------------------------------- */
/** \name `_defs_grease_pencil_edit`
 * \{ */

namespace defs_grease_pencil_edit {

/* El cizallado del lapiz de cera es el mismo gizmo que el de `_defs_transform`, pero
 * con su propio icono; por eso es una herramienta aparte y no la de transformacion
 * reutilizada. El keymap si es el compartido, "3D View Tool: Shear", literal en el
 * Python.
 *
 * Su unico ajuste es `_template_widget.VIEW3D_GGT_xform_gizmo.draw_settings_with_index(
 * context, layout, 2)`, que pinta el `type` de `scene.transform_orientation_slots[2]`.
 * No tiene flujo de control, pero tampoco cabe en una fila: `PropSource` no sabe nombrar
 * un elemento indexado de la escena, y inventarse la ruta seria adivinar. Queda marcado
 * como pendiente para que el verificador lo siga listando en cada pasada. */
const ToolDecl shear = {
    /*idname*/ "builtin.shear",
    /*label*/ N_("Shear"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.gpencil.edit_shear",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_xform_shear",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Shear",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ {},
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
    /*settings_pending*/ true,
};

static const PropRow interpolate_settings[] = {
    {PropSource::Operator, "grease_pencil.interpolate", "layers"},
    {PropSource::Operator, "grease_pencil.interpolate", "exclude_breakdowns"},
    {PropSource::Operator, "grease_pencil.interpolate", "flip"},
    {PropSource::Operator, "grease_pencil.interpolate", "smooth_factor"},
    {PropSource::Operator, "grease_pencil.interpolate", "smooth_steps"},
};

/* Hay otra `builtin.interpolate` en el modo de pintar, con el mismo icono y los mismos
 * ajustes pero definida aparte en el Python; son dos herramientas y cada una lleva el
 * keymap de SU modo. Copiar aqui el nombre de la otra dejaria una de las dos sin
 * atajos. */
const ToolDecl interpolate = {
    /*idname*/ "builtin.interpolate",
    /*label*/ N_("Interpolate"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.pose.breakdowner",
    /*cursor*/ "DEFAULT",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Grease Pencil, Interpolate",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(interpolate_settings),
};

/* El icono es el del degradado de PESOS aunque la herramienta pinte color de vertice:
 * viene asi del Python y la linea base lo confirma. No es una errata que arreglar aqui. */
const ToolDecl texture_gradient = {
    /*idname*/ "builtin.texture_gradient",
    /*label*/ N_("Gradient"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.paint.weight_gradient",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Grease Pencil, Gradient",
};

}  // namespace defs_grease_pencil_edit

/** \} */

/* -------------------------------------------------------------------- */
/** \name `_defs_grease_pencil_sculpt`
 * \{ */

namespace defs_grease_pencil_sculpt {

const ToolDecl clone = {
    /*idname*/ "builtin_brush.clone",
    /*label*/ N_("Clone"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.gpencil.sculpt_clone",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
    /*brush_type*/ "CLONE",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES,
};

}  // namespace defs_grease_pencil_sculpt

/** \} */

/* -------------------------------------------------------------------- */
/* `_defs_gpencil_weight` no define ninguna herramienta: el Python es `pass`. No abre
 * seccion propia porque no hay nada que definir; el porque esta en el `.hh`. */

/* -------------------------------------------------------------------- */
/** \name `_defs_grease_pencil_weight`
 * \{ */

namespace defs_grease_pencil_weight {

const ToolDecl blur = {
    /*idname*/ "builtin_brush.blur",
    /*label*/ N_("Blur"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.gpencil.sculpt_blur",
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
    /*icon*/ "ops.gpencil.sculpt_average",
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
    /*icon*/ "ops.gpencil.sculpt_smear",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
    /*brush_type*/ "SMEAR",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES,
};

}  // namespace defs_grease_pencil_weight

/** \} */

/* -------------------------------------------------------------------- */
/** \name `_defs_grease_pencil_vertex`
 *
 * Las tres primeras se llaman igual que las de peso y llevan el mismo `brush_type`;
 * lo unico que cambia es el icono, porque aqui el pincel es de color y alli de peso.
 * \{ */

namespace defs_grease_pencil_vertex {

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

const ToolDecl replace = {
    /*idname*/ "builtin_brush.replace",
    /*label*/ N_("Replace"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "brush.paint_vertex.replace",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
    /*brush_type*/ "REPLACE",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES,
};

}  // namespace defs_grease_pencil_vertex

/** \} */

}  // namespace flipendo::toolsystem
