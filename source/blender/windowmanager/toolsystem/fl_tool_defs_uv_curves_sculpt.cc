/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Herramientas de escultura de UV y de escultura de curvas. Transliteracion de
 * `_defs_image_uv_sculpt` y `_defs_curves_sculpt`
 * (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:2603` y `:2806`).
 *
 * Son los dos extremos del catalogo. Las de UV llevan ajustes y dibujo sobre la vista
 * y ninguna toca el pincel activo; las de curvas son cuatro fichas de cuatro campos
 * que solo seleccionan un tipo de pincel y dejan que el resto lo pinte el sistema de
 * pinceles. Por eso las de curvas no tienen keymap propio: en el Python ni siquiera
 * declaran `keymap`, y la linea base lo confirma con `keymap=None`.
 */

#include "BLT_translation.hh"

#include "fl_tool_defs_uv_curves_sculpt.hh"

namespace flipendo::toolsystem {

/* -------------------------------------------------------------------- */
/** \name `_defs_image_uv_sculpt`
 * \{ */

namespace defs_image_uv_sculpt {

/* Las tres pintan el tamano y la fuerza del escultor de UV, que viven en
 * `tool_settings.uv_sculpt` y no en el operador: son el estado del modo, no argumentos
 * de una invocacion. En el Python son tres funciones copiadas; aqui `grab` y `pinch`
 * comparten la tabla porque pintan exactamente lo mismo, y `relax` repite las dos
 * filas para poder anadir la suya.
 *
 * Lo que NO cabe aqui son los dos `layout.popover` que van detras
 * (`IMAGE_PT_uv_sculpt_curve` e `IMAGE_PT_uv_sculpt_options`): siguen siendo paneles
 * de Python, igual que le pasa a las anotaciones con `TOPBAR_PT_annotation_layers`.
 * Por eso las tres quedan marcadas con `settings_pending`, aunque sus filas ya esten
 * puestas: asi se pinta hoy lo que ya se puede pintar y la deuda sigue saliendo
 * listada en cada verificacion en vez de perderse. */
static const PropRow uv_sculpt_common_settings[] = {
    {PropSource::ToolSettingsSub, "uv_sculpt", "size"},
    {PropSource::ToolSettingsSub, "uv_sculpt", "strength"},
};

const ToolDecl grab = {
    /*idname*/ "sculpt.uv_sculpt_grab",
    /*label*/ N_("Grab"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "brush.uv_sculpt.grab",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "Image Editor Tool: Uv, Grab",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_KEYMAP_FALLBACK,
    /*settings*/ span(uv_sculpt_common_settings),
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
    /*settings_pending*/ true,
};

/* `relax` es la unica de las tres que ademas elige metodo, y ese si es una propiedad
 * del operador. En el Python la fila va DETRAS de los dos popovers; al no existir
 * todavia los popovers queda pegada a las otras dos, asi que cuando se hagan nativos
 * hay que reinsertarlos en medio y no al final. */
static const PropRow relax_settings[] = {
    {PropSource::ToolSettingsSub, "uv_sculpt", "size"},
    {PropSource::ToolSettingsSub, "uv_sculpt", "strength"},
    {PropSource::Operator, "sculpt.uv_sculpt_relax", "relax_method", N_("Method")},
};

const ToolDecl relax = {
    /*idname*/ "sculpt.uv_sculpt_relax",
    /*label*/ N_("Relax"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "brush.uv_sculpt.relax",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "Image Editor Tool: Uv, Relax",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_KEYMAP_FALLBACK,
    /*settings*/ span(relax_settings),
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
    /*settings_pending*/ true,
};

const ToolDecl pinch = {
    /*idname*/ "sculpt.uv_sculpt_pinch",
    /*label*/ N_("Pinch"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "brush.uv_sculpt.pinch",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "Image Editor Tool: Uv, Pinch",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_KEYMAP_FALLBACK,
    /*settings*/ span(uv_sculpt_common_settings),
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
    /*settings_pending*/ true,
};

}  // namespace defs_image_uv_sculpt

/** \} */

/* -------------------------------------------------------------------- */
/** \name `_defs_curves_sculpt`
 * \{ */

namespace defs_curves_sculpt {

/* Las cuatro son la misma ficha con distinto `brush_type`: no llevan ajustes, ni
 * gizmos, ni keymap, porque con `USE_BRUSHES` quien manda es el pincel activo y la
 * herramienta solo dice a que tipo de pincel se limita la barra. Ojo con el par
 * `brush_type` / `data_block`: aqui va el primero, el segundo es `None` en la linea
 * base y solo lo usan las siete de particulas. */

const ToolDecl select = {
    /*idname*/ "builtin_brush.selection_paint",
    /*label*/ N_("Selection Paint"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.generic.select_paint",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
    /*brush_type*/ "SELECTION_PAINT",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES,
};

const ToolDecl density = {
    /*idname*/ "builtin_brush.density",
    /*label*/ N_("Density"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.curves.sculpt_density",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
    /*brush_type*/ "DENSITY",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES,
};

const ToolDecl add = {
    /*idname*/ "builtin_brush.add",
    /*label*/ N_("Add"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.curves.sculpt_add",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
    /*brush_type*/ "ADD",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES,
};

const ToolDecl delete_ = {
    /*idname*/ "builtin_brush.delete",
    /*label*/ N_("Delete"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.curves.sculpt_delete",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
    /*brush_type*/ "DELETE",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES,
};

}  // namespace defs_curves_sculpt

/** \} */

}  // namespace flipendo::toolsystem
