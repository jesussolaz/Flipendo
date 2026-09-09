/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Herramientas del editor de nodos: `_defs_node_select` y `_defs_node_edit`
 * (`space_toolsystem_toolbar.py:2848` y `:2918`).
 */

#ifndef __FL_TOOL_DEFS_NODE_HH__
#define __FL_TOOL_DEFS_NODE_HH__

#include "FL_toolsystem.hpp"

namespace flipendo::toolsystem {

namespace defs_node_select {
extern const ToolDecl select;
extern const ToolDecl box;
extern const ToolDecl lasso;
extern const ToolDecl circle;
}  // namespace defs_node_select

namespace defs_node_edit {
extern const ToolDecl links_cut;
}  // namespace defs_node_edit

}  // namespace flipendo::toolsystem

#endif /* __FL_TOOL_DEFS_NODE_HH__ */
