/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Barra de herramientas del editor de imagen, para que el registro la referencie.
 *
 * La barra vive en su propio fichero y no en `fl_tool_defs.hh` porque ese cabecero es
 * de las HERRAMIENTAS, que las comparten varios espacios, mientras que una barra la usa
 * un unico sitio: el registro. Separarlos evita que anadir un espacio obligue a
 * recompilar todo el catalogo.
 */

#ifndef __FL_TOOLBAR_IMAGE_HH__
#define __FL_TOOLBAR_IMAGE_HH__

#include "FL_toolsystem.hpp"

namespace flipendo::toolsystem {

/** `IMAGE_PT_tools_active` del Python. */
extern const ToolbarDecl toolbar_image;

}  // namespace flipendo::toolsystem

#endif /* __FL_TOOLBAR_IMAGE_HH__ */
