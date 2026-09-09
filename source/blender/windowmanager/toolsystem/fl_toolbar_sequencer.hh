/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Barra de herramientas del editor de secuencias, para que el registro la referencie.
 *
 * Va en cabecera propia y no en `fl_tool_defs.hh` porque ese fichero es de otro modulo:
 * lo comparten los cuatro espacios y lo integra quien monta el catalogo. Asi este
 * espacio se puede trasladar entero sin tocar nada compartido.
 */

#ifndef __FL_TOOLBAR_SEQUENCER_HH__
#define __FL_TOOLBAR_SEQUENCER_HH__

#include "FL_toolsystem.hpp"

namespace flipendo::toolsystem {

/** `SEQUENCER_PT_tools_active` del Python. */
extern const ToolbarDecl toolbar_sequencer;

}  // namespace flipendo::toolsystem

#endif /* __FL_TOOLBAR_SEQUENCER_HH__ */
