/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Barra de herramientas del editor de nodos.
 *
 * Transliteracion de `NODE_PT_tools_active` (`space_toolsystem_toolbar.py:3173`).
 */

#include "DNA_space_types.h"

#include "fl_tool_defs.hh"

namespace flipendo::toolsystem {

/* Los cuatro de seleccion van bajo un mismo boton, con ciclo entre ellos: en el
 * Python son una tupla dentro de la lista, y aqui una entrada con cuatro
 * herramientas. Las de anotacion, igual. */
static const ToolDecl *node_select_group[] = {
    &defs_node_select::select,
    &defs_node_select::box,
    &defs_node_select::lasso,
    &defs_node_select::circle,
};

static const ToolDecl *node_annotate_group[] = {
    &defs_annotate::scribble,
    &defs_annotate::line,
    &defs_annotate::poly,
    &defs_annotate::eraser,
};

static const ToolDecl *node_links_cut[] = {&defs_node_edit::links_cut};

static const ToolEntry node_entries[] = {
    {span(node_select_group)},
    /* Separador: entrada vacia, el `None` del Python. */
    {},
    {span(node_annotate_group)},
    {},
    {span(node_links_cut)},
};

/* El editor de nodos NO tiene modos. Lo que el Python llama su "modo" es el
 * `tree_type` del espacio ('ShaderNodeTree', 'GeometryNodeTree'...), que nunca es una
 * clave de su tabla de herramientas: sirva lo que sirva el arbol abierto, las nueve
 * herramientas son las mismas. Por eso aqui hay una sola entrada de modo, la comun, y
 * `mode_from_context` no existe. Ver tests/flipendo/toolsystem/README.md. */
static const ModeTools node_modes[] = {
    {nullptr, span(node_entries)},
};

const ToolbarDecl toolbar_node = {
    /*space_type*/ SPACE_NODE,
    /*keymap_prefix*/ "Node Editor Tool:",
    /*tool_fallback_id*/ "builtin.select",
    /*mode_from_context*/ nullptr,
    /*modes*/ span(node_modes),
};

}  // namespace flipendo::toolsystem
