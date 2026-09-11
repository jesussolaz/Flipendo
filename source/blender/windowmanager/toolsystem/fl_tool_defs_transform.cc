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
 * Seis de las siete tienen ajustes, y todos acaban en
 * `_template_widget.VIEW3D_GGT_xform_gizmo.draw_settings_with_index`, que pinta la
 * orientacion de una ranura de la escena elegida por indice. Eso no cabe en una fila, asi
 * que las seis se pintan con codigo: `draw_transform_*` de
 * `editors/interface/fl_tool_settings_transform.cc`, transliterados linea a linea.
 */

#include "BLT_translation.hh"

#include "fl_tool_defs_transform.hh"

/* Los `draw_settings` de esta familia. Viven en `editors/interface`, que es donde esta el
 * dibujo; aqui solo hace falta el puntero. */
namespace flipendo::ui::settings {
void draw_transform_translate(const bContext *C, uiLayout *layout, bToolRef *tref, bool extra);
void draw_transform_rotate(const bContext *C, uiLayout *layout, bToolRef *tref, bool extra);
void draw_transform_scale(const bContext *C, uiLayout *layout, bToolRef *tref, bool extra);
void draw_transform_scale_cage(const bContext *C, uiLayout *layout, bToolRef *tref, bool extra);
void draw_transform_shear(const bContext *C, uiLayout *layout, bToolRef *tref, bool extra);
void draw_transform_transform(const bContext *C, uiLayout *layout, bToolRef *tref, bool extra);
}  // namespace flipendo::ui::settings

namespace flipendo::toolsystem {

/* -------------------------------------------------------------------- */
/** \name `_template_widget`
 * \{ */

namespace template_widget::view3d_ggt_xform_extrude {

static const PropRow settings_rows[] = {
    {PropSource::GizmoGroup,
     "VIEW3D_GGT_xform_extrude",
     "axis_type",
     nullptr,
     PROP_ROW_EXPAND | PROP_ROW_OWN_ROW | PROP_ROW_ALIGN},
};

const blender::Span<PropRow> settings = span(settings_rows);

}  // namespace template_widget::view3d_ggt_xform_extrude

/* `template_widget::view3d_ggt_xform_gizmo::draw_settings_with_index` no se define aqui:
 * su transliteracion es `xform_gizmo_draw_settings_with_index`, `static` en
 * `editors/interface/fl_tool_settings_transform.cc`, que es donde se dibuja. La
 * declaracion del `.hh` (en este modulo y sin cuerpo) ya no la usa nadie. */

/** \} */

/* -------------------------------------------------------------------- */
/** \name `_defs_transform`
 * \{ */

namespace defs_transform {

/* Mover, girar y escalar tienen el mismo `draw_settings` salvo el indice de la ranura
 * (1, 2 y 3): los ajustes del escultor y la orientacion de esa ranura. Ni lo uno ni lo
 * otro cabe en filas, asi que los tres se pintan con codigo. */

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
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ "transform.translate",
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ {},
    /*draw_settings*/ flipendo::ui::settings::draw_transform_translate,
    /*draw_cursor*/ nullptr,
    /*pending*/ TOOL_PENDING_NONE,
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
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ "transform.rotate",
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ {},
    /*draw_settings*/ flipendo::ui::settings::draw_transform_rotate,
    /*draw_cursor*/ nullptr,
    /*pending*/ TOOL_PENDING_NONE,
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
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ "transform.resize",
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ {},
    /*draw_settings*/ flipendo::ui::settings::draw_transform_scale,
    /*draw_cursor*/ nullptr,
    /*pending*/ TOOL_PENDING_NONE,
};

/* La jaula es una herramienta distinta de la de escalar (otro gizmo, otro icono), pero
 * comparte su keymap y su operador. Copiado de la linea base, no deducido del nombre.
 * Tambien comparte la ranura de orientacion de escalar (la 3), pero no pinta los ajustes
 * del escultor. */
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
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ "transform.resize",
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ {},
    /*draw_settings*/ flipendo::ui::settings::draw_transform_scale_cage,
    /*draw_cursor*/ nullptr,
    /*pending*/ TOOL_PENDING_NONE,
};

/* El sesgado, como la jaula, no pinta los ajustes del escultor: solo la orientacion de
 * la ranura, y la que usa es la de girar (la 2). */
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
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ {},
    /*draw_settings*/ flipendo::ui::settings::draw_transform_shear,
    /*draw_cursor*/ nullptr,
    /*pending*/ TOOL_PENDING_NONE,
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
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
};

/* La de transformar tiene ademas una fila propia, `drag_action` del grupo de gizmos, que
 * el Python solo pinta si `tool_settings.workspace_tool_type` (el "Drag" de la cabecera)
 * no es 'FALLBACK'; y en el panel lateral la precede el rotulo "Gizmos:". Las dos
 * condiciones forman parte del ajuste: declararlo como filas fijas cambiaria la interfaz. */
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
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ {},
    /*draw_settings*/ flipendo::ui::settings::draw_transform_transform,
    /*draw_cursor*/ nullptr,
    /*pending*/ TOOL_PENDING_NONE,
};

}  // namespace defs_transform

/** \} */

}  // namespace flipendo::toolsystem
