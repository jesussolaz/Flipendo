/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Barra de herramientas de la vista 3D.
 *
 * Tiene cabecera propia, al contrario que la del editor de nodos (que se declara en
 * `fl_tool_defs.hh`), porque es la barra grande del catalogo: veintidos modos, ciento
 * y pico herramientas y las siete funciones `poll_*` que filtran sus bloques. Ponerla
 * en la cabecera comun obligaria a recompilar los cuatro espacios cada vez que se toca
 * un modo de la vista 3D.
 */

#ifndef __FL_TOOLBAR_VIEW3D_HH__
#define __FL_TOOLBAR_VIEW3D_HH__

#include "FL_toolsystem.hpp"

namespace flipendo::toolsystem {

/** `VIEW3D_PT_tools_active` del Python. La referencia el registro del catalogo. */
extern const ToolbarDecl toolbar_view3d;

}  // namespace flipendo::toolsystem

#endif /* __FL_TOOLBAR_VIEW3D_HH__ */
