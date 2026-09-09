/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Herramientas del editor de secuencias. Transliteracion de `_defs_sequencer_generic` y
 * `_defs_sequencer_select` (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:2932` y
 * `:3022`).
 *
 * Las dos clases van juntas porque son un unico espacio que reparte sus diez
 * herramientas entre las tres vistas de `space_data.view_type` (PREVIEW, SEQUENCER y
 * SEQUENCER_PREVIEW); separarlas obligaria a saltar de fichero para revisar una vista.
 *
 * Ojo con `builtin.select_box`, que sale DOS veces: `box_timeline` para la linea de
 * tiempo y `box_preview` para la previsualizacion. Son identicas salvo el keymap, y por
 * eso no se pueden fundir en una sola: cada vista instala el suyo. El idname repetido no
 * choca porque las dos nunca aparecen en la misma vista.
 *
 * Ojo tambien con los nombres de keymap: el prefijo del espacio es "Sequence Editor
 * Tool:" y NINGUNA de las diez lo usa; todas llevan "Preview Tool: ..." o "Sequencer
 * Tool: ..." literal. Es la prueba de que el keymap se copia de la linea base y jamas se
 * sintetiza a partir del prefijo y el modo.
 */

#ifndef __FL_TOOL_DEFS_SEQUENCER_HH__
#define __FL_TOOL_DEFS_SEQUENCER_HH__

#include "FL_toolsystem.hpp"

namespace flipendo::toolsystem {

/** `_defs_sequencer_generic` del Python. Mezcla las de la previsualizacion (cursor,
 * transformacion y muestreo, con keymaps "Preview Tool: ...") con la cuchilla, que es la
 * unica de la linea de tiempo. */
namespace defs_sequencer_generic {
extern const ToolDecl cursor;
extern const ToolDecl blade;
extern const ToolDecl sample;
extern const ToolDecl translate;
extern const ToolDecl rotate;
extern const ToolDecl scale;
extern const ToolDecl transform;
}  // namespace defs_sequencer_generic

/** `_defs_sequencer_select`. */
namespace defs_sequencer_select {
extern const ToolDecl select_preview;
extern const ToolDecl box_timeline;
extern const ToolDecl box_preview;
}  // namespace defs_sequencer_select

}  // namespace flipendo::toolsystem

#endif /* __FL_TOOL_DEFS_SEQUENCER_HH__ */
