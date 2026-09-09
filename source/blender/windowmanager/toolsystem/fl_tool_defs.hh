/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Declaraciones de las herramientas del catalogo, para uso interno del subsistema.
 *
 * Un espacio a menudo usa herramientas definidas "para otro" — las de anotacion salen
 * en los cuatro — asi que las definiciones viven aqui, en espacios de nombres que
 * corresponden uno a uno con las clases `_defs_*` del Python
 * (`scripts/startup/bl_ui/space_toolsystem_toolbar.py`), y las barras de cada espacio
 * solo las referencian.
 *
 * Mantener la correspondencia de nombres no es cosmetico: es lo que permite revisar
 * una herramienta abriendo el Python al lado y comparar linea a linea.
 */

#ifndef __FL_TOOL_DEFS_HH__
#define __FL_TOOL_DEFS_HH__

#include "FL_toolsystem.hpp"

namespace flipendo::toolsystem {

/** `_defs_annotate` del Python. Compartidas por vista 3D, imagen, nodos y secuencias. */
namespace defs_annotate {
extern const ToolDecl scribble;
extern const ToolDecl line;
extern const ToolDecl poly;
extern const ToolDecl eraser;
}  // namespace defs_annotate

/** `_defs_node_select`. */
namespace defs_node_select {
extern const ToolDecl select;
extern const ToolDecl box;
extern const ToolDecl lasso;
extern const ToolDecl circle;
}  // namespace defs_node_select

/** `_defs_node_edit`. */
namespace defs_node_edit {
extern const ToolDecl links_cut;
}  // namespace defs_node_edit

/* -------------------------------------------------------------------- */
/** \name Barras de cada espacio
 * \{ */

extern const ToolbarDecl toolbar_node;

/** \} */

}  // namespace flipendo::toolsystem

#endif /* __FL_TOOL_DEFS_HH__ */
