/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Herramientas del editor de nodos. Transliteracion de `_defs_node_select` y
 * `_defs_node_edit` (`space_toolsystem_toolbar.py:2848` y `:2918`).
 *
 * Es el espacio mas pequeno del catalogo — 9 herramientas — y por eso es el piloto que
 * valida el registro y el volcado antes de trasladar los otros tres espacios.
 */

#include "BLT_translation.hh"

#include "fl_tool_defs_node.hh"

/* Definida en editors/interface/fl_tool_cursor_ui.cc: el circulo del radio bajo el raton. */
namespace flipendo::ui::cursor {
void draw_select_circle_node(bContext *C, bToolRef *tref, const blender::int2 &xy);
}  // namespace flipendo::ui::cursor

namespace flipendo::toolsystem {

/* -------------------------------------------------------------------- */
/** \name `_defs_node_select`
 * \{ */

namespace defs_node_select {

const ToolDecl select = {
    /*idname*/ "builtin.select",
    /*label*/ N_("Tweak"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.generic.select",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "Node Tool: Tweak",
};

/* Las tres de seleccion pintan el mismo control: el modo de seleccion del operador,
 * sin etiqueta, expandido y solo con iconos. En el Python son tres `draw_settings`
 * copiadas; aqui es la misma fila declarada tres veces con distinto operador. */
static const PropRow box_settings[] = {
    {PropSource::Operator,
     "node.select_box",
     "mode",
     nullptr,
     PROP_ROW_EXPAND | PROP_ROW_NO_TEXT | PROP_ROW_ICON_ONLY | PROP_ROW_OWN_ROW | PROP_ROW_NO_SPLIT},
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
    /*keymap_name*/ "Node Tool: Select Box",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(box_settings),
};

static const PropRow lasso_settings[] = {
    {PropSource::Operator,
     "node.select_lasso",
     "mode",
     nullptr,
     PROP_ROW_EXPAND | PROP_ROW_NO_TEXT | PROP_ROW_ICON_ONLY | PROP_ROW_OWN_ROW | PROP_ROW_NO_SPLIT},
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
    /*keymap_name*/ "Node Tool: Select Lasso",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(lasso_settings),
};

static const PropRow circle_settings[] = {
    {PropSource::Operator,
     "node.select_circle",
     "mode",
     nullptr,
     PROP_ROW_EXPAND | PROP_ROW_NO_TEXT | PROP_ROW_ICON_ONLY | PROP_ROW_OWN_ROW | PROP_ROW_NO_SPLIT},
    {PropSource::Operator, "node.select_circle", "radius"},
};

/* El circulo pinta ademas su radio sobre la vista mientras esta activo. El dibujo va
 * en la fase de dibujado, con el resto de `draw_cursor`. */
const ToolDecl circle = {
    /*idname*/ "builtin.select_circle",
    /*label*/ N_("Select Circle"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.generic.select_circle",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "Node Tool: Select Circle",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(circle_settings),
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ flipendo::ui::cursor::draw_select_circle_node,
    /*pending*/ TOOL_PENDING_NONE,
};

}  // namespace defs_node_select

/** \} */

/* -------------------------------------------------------------------- */
/** \name `_defs_node_edit`
 * \{ */

namespace defs_node_edit {

const ToolDecl links_cut = {
    /*idname*/ "builtin.links_cut",
    /*label*/ N_("Links Cut"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.node.links_cut",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "Node Tool: Links Cut",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_KEYMAP_FALLBACK,
};

}  // namespace defs_node_edit

/** \} */

}  // namespace flipendo::toolsystem
