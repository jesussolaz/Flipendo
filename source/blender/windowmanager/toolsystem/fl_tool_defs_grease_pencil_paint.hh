/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Declaracion de las herramientas del modo de pintura de lapiz de cera.
 * Transliteracion de `_defs_grease_pencil_paint`
 * (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:2149`).
 *
 * Van en su propia cabecera y no en `fl_tool_defs.hh` porque son once herramientas de
 * un unico modo: solo las referencia la barra de la vista 3D, y meterlas en la cabecera
 * comun obligaria a recompilar todo el catalogo cada vez que se toca una de ellas.
 */

#ifndef __FL_TOOL_DEFS_GREASE_PENCIL_PAINT_HH__
#define __FL_TOOL_DEFS_GREASE_PENCIL_PAINT_HH__

#include "FL_toolsystem.hpp"

namespace flipendo::toolsystem {

/**
 * `_defs_grease_pencil_paint` del Python.
 *
 * Ojo con `interpolate`: `_defs_grease_pencil_edit` declara OTRA herramienta con el
 * mismo `idname` ("builtin.interpolate"), y por eso la linea base la lista dos veces con
 * keymaps distintos ("... Paint Grease Pencil, Interpolate" y "... Edit Grease Pencil,
 * Interpolate"). No son la misma herramienta vista en dos modos, son dos `ToolDef`
 * separados en el Python; el de aqui es el del modo de pintura y solo lo usa ese modo.
 */
namespace defs_grease_pencil_paint {

extern const ToolDecl fill;
extern const ToolDecl erase;
extern const ToolDecl trim;
extern const ToolDecl line;
extern const ToolDecl polyline;
extern const ToolDecl arc;
extern const ToolDecl curve;
extern const ToolDecl box;
extern const ToolDecl circle;
extern const ToolDecl interpolate;
extern const ToolDecl eyedropper;

}  // namespace defs_grease_pencil_paint

}  // namespace flipendo::toolsystem

#endif /* __FL_TOOL_DEFS_GREASE_PENCIL_PAINT_HH__ */
