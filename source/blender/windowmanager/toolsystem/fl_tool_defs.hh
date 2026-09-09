/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Indice de las declaraciones de herramientas del catalogo.
 *
 * Un espacio a menudo usa herramientas definidas "para otro" — las de anotacion salen
 * en los cuatro — asi que las definiciones viven en ficheros por familia, en espacios
 * de nombres que corresponden uno a uno con las clases `_defs_*` del Python
 * (`scripts/startup/bl_ui/space_toolsystem_toolbar.py`), y las barras de cada espacio
 * solo las referencian.
 *
 * Mantener la correspondencia de nombres no es cosmetico: es lo que permite revisar
 * una herramienta abriendo el Python al lado y comparar linea a linea.
 *
 * Este fichero no declara nada por su cuenta; solo reune las cabeceras para quien
 * necesite el catalogo entero.
 */

#ifndef __FL_TOOL_DEFS_HH__
#define __FL_TOOL_DEFS_HH__

#include "FL_toolsystem.hpp"

#include "fl_tool_defs_annotate.hh"
#include "fl_tool_defs_edit_mesh.hh"
#include "fl_tool_defs_edit_misc.hh"
#include "fl_tool_defs_grease_pencil_misc.hh"
#include "fl_tool_defs_grease_pencil_paint.hh"
#include "fl_tool_defs_image_uv.hh"
#include "fl_tool_defs_node.hh"
#include "fl_tool_defs_paint.hh"
#include "fl_tool_defs_sculpt.hh"
#include "fl_tool_defs_sequencer.hh"
#include "fl_tool_defs_transform.hh"
#include "fl_tool_defs_uv_curves_sculpt.hh"
#include "fl_tool_defs_view3d.hh"
#include "fl_tool_defs_view3d_add.hh"

#include "fl_toolbar_image.hh"
#include "fl_toolbar_node.hh"
#include "fl_toolbar_sequencer.hh"
#include "fl_toolbar_view3d.hh"

#endif /* __FL_TOOL_DEFS_HH__ */
