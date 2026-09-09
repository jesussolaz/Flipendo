/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Declaraciones de las herramientas de escultura de UV y de escultura de curvas.
 * Transliteracion de `_defs_image_uv_sculpt` y `_defs_curves_sculpt`
 * (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:2603` y `:2806`).
 *
 * Los dos grupos viajan juntos por comodidad de la migracion, no porque compartan
 * nada: el primero solo sale en el modo UV del editor de imagen y el segundo solo en
 * el modo SCULPT_CURVES de la vista 3D. Cada uno conserva su espacio de nombres, con
 * el mismo nombre que su clase del Python, para poder revisarlos con el fichero
 * original abierto al lado.
 */

#ifndef __FL_TOOL_DEFS_UV_CURVES_SCULPT_HH__
#define __FL_TOOL_DEFS_UV_CURVES_SCULPT_HH__

#include "FL_toolsystem.hpp"

namespace flipendo::toolsystem {

/** `_defs_image_uv_sculpt`. Solo el modo UV del editor de imagen. */
namespace defs_image_uv_sculpt {
extern const ToolDecl grab;
extern const ToolDecl relax;
extern const ToolDecl pinch;
}  // namespace defs_image_uv_sculpt

/** `_defs_curves_sculpt`. Solo el modo SCULPT_CURVES de la vista 3D. */
namespace defs_curves_sculpt {
extern const ToolDecl select;
extern const ToolDecl density;
extern const ToolDecl add;
/* En el Python se llama `delete` a secas, pero `delete` es palabra reservada de C++.
 * Se le anade el guion bajo final en el NOMBRE C++ y nada mas: el `idname` que ve el
 * usuario y el motor sigue siendo `builtin_brush.delete`. */
extern const ToolDecl delete_;
}  // namespace defs_curves_sculpt

}  // namespace flipendo::toolsystem

#endif /* __FL_TOOL_DEFS_UV_CURVES_SCULPT_HH__ */
