/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Herramientas de transformar de la vista 3D. Transliteracion de `_template_widget`
 * (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:89`) y de `_defs_transform`
 * (`:301`).
 *
 * Estas siete salen en casi todos los modos de la vista 3D, y son el ejemplo mas claro
 * de por que el keymap se copia de la linea base y no se sintetiza: `builtin.scale_cage`
 * se llama "Scale Cage" pero su keymap es "3D View Tool: Scale", el de `builtin.scale`.
 * Calcularlo por etiqueta daria un keymap que no existe y la herramienta se quedaria sin
 * atajos sin que nada avisara.
 *
 * Seis de las siete van con `settings_pending`, y todas por lo mismo: sus ajustes acaban
 * en `_template_widget.VIEW3D_GGT_xform_gizmo.draw_settings_with_index`, que pinta la
 * orientacion de una ranura de la escena elegida por indice y no cabe ni en una fila ni
 * en el contrato de `DrawSettingsFn`. El por que esta en el `.hh`.
 */

#include "BLT_translation.hh"

#include "fl_tool_defs_transform.hh"

namespace flipendo::toolsystem {

/* -------------------------------------------------------------------- */
/** \name `_template_widget`
 * \{ */

namespace template_widget::view3d_ggt_xform_extrude {

static const PropRow settings_rows[] = {
    {PropSource::GizmoGroup, "VIEW3D_GGT_xform_extrude", "axis_type", nullptr, PROP_ROW_EXPAND},
};

const blender::Span<PropRow> settings = span(settings_rows);

}  // namespace template_widget::view3d_ggt_xform_extrude

/* `template_widget::view3d_ggt_xform_gizmo::draw_settings_with_index` no se define aqui:
 * ver el `.hh`. */

/** \} */

/* -------------------------------------------------------------------- */
/** \name `_defs_transform`
 * \{ */

namespace defs_transform {

/* Mover, girar y escalar tienen el mismo `draw_settings` salvo el indice de la ranura
 * (1, 2 y 3): los ajustes del escultor y la orientacion de esa ranura. Ni lo uno ni lo
 * otro cabe en filas, asi que las tres van con `settings_pending` en vez de con unos
 * ajustes inventados que se le parezcan. */

const ToolDecl translate = {
    /*idname*/ "builtin.move",
    /*label*/ N_("Move"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.transform.translate",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_xform_gizmo",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Move",
    /*keymap_fallback*/ nullptr,
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ "transform.translate",
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ {},
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
    /*settings_pending*/ true,
};

const ToolDecl rotate = {
    /*idname*/ "builtin.rotate",
    /*label*/ N_("Rotate"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.transform.rotate",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_xform_gizmo",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Rotate",
    /*keymap_fallback*/ nullptr,
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ "transform.rotate",
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ {},
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
    /*settings_pending*/ true,
};

const ToolDecl scale = {
    /*idname*/ "builtin.scale",
    /*label*/ N_("Scale"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.transform.resize",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_xform_gizmo",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Scale",
    /*keymap_fallback*/ nullptr,
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ "transform.resize",
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ {},
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
    /*settings_pending*/ true,
};

/* La jaula es una herramienta distinta de la de escalar (otro gizmo, otro icono), pero
 * comparte su keymap y su operador. Copiado de la linea base, no deducido del nombre. */
const ToolDecl scale_cage = {
    /*idname*/ "builtin.scale_cage",
    /*label*/ N_("Scale Cage"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.transform.resize.cage",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_xform_cage",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Scale",
    /*keymap_fallback*/ nullptr,
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ "transform.resize",
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ {},
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
    /*settings_pending*/ true,
};

/* El sesgado, como la jaula, no pinta los ajustes del escultor: solo la orientacion de
 * la ranura. Sigue sin caber en filas, asi que queda igual de pendiente. */
const ToolDecl shear = {
    /*idname*/ "builtin.shear",
    /*label*/ N_("Shear"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.transform.shear",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_xform_shear",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Shear",
    /*keymap_fallback*/ nullptr,
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ {},
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
    /*settings_pending*/ true,
};

/* La unica de las siete sin gizmo y sin ajustes, asi que no hay nada pendiente que
 * marcar. Y la unica que sale en un solo modo, el de editar lapiz de cera, que es de
 * donde le viene el icono. */
const ToolDecl bend = {
    /*idname*/ "builtin.bend",
    /*label*/ N_("Bend"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.gpencil.edit_bend",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Bend",
    /*keymap_fallback*/ nullptr,
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
};

/* La de transformar tiene ademas una fila propia, `drag_action` del grupo de gizmos, pero
 * el Python solo la pinta si la herramienta no esta actuando de reserva. Esa condicion
 * forma parte del ajuste: declararla como fila fija cambiaria la interfaz de la reserva.
 * Por eso tambien queda pendiente entera. */
const ToolDecl transform = {
    /*idname*/ "builtin.transform",
    /*label*/ N_("Transform"),
    /*description*/ N_("Supports any combination of grab, rotate, and scale at once"),
    /*description_fn*/ nullptr,
    /*icon*/ "ops.transform.transform",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_xform_gizmo",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Transform",
    /*keymap_fallback*/ nullptr,
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ {},
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
    /*settings_pending*/ true,
};

}  // namespace defs_transform

/** \} */

}  // namespace flipendo::toolsystem
