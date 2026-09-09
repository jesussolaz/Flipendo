/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Herramientas comunes de la vista 3D y las de seleccion. Transliteracion de
 * `_defs_view3d_generic` y `_defs_view3d_select`
 * (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:105` y `:425`).
 *
 * Las dos clases van en el mismo fichero porque son las unicas de la vista 3D que no
 * pertenecen a un modo: el cursor y la regla salen en la parte comun de la barra y las
 * cuatro de seleccion encabezan TODOS los modos. Repartirlas obligaria a abrir dos
 * ficheros para revisar la cabecera de cualquier modo.
 *
 * Ninguna de las dos clases tiene filtros `poll_*` ni ajustes con flujo de control: en
 * la vista 3D quien decide que se ve es el modo, no la herramienta. Por eso aqui no hay
 * prototipos sueltos que implementar mas adelante.
 *
 * Los nombres de keymap estan copiados de la linea base congelada
 * (`tests/flipendo/toolsystem/baseline-python.txt`), nunca sintetizados. Aqui la
 * tentacion es especialmente fuerte porque los seis siguen el patron "3D View Tool:
 * <etiqueta>" sin el modo dentro, justo lo contrario que en el editor de imagen.
 */

#ifndef __FL_TOOL_DEFS_VIEW3D_HH__
#define __FL_TOOL_DEFS_VIEW3D_HH__

#include "FL_toolsystem.hpp"

namespace flipendo::toolsystem {

/** `_defs_view3d_generic` del Python. Las que la vista 3D ofrece en cualquier modo. */
namespace defs_view3d_generic {

extern const ToolDecl cursor;

/**
 * `_defs_view3d_generic.cursor_click`, o sea `builtin.none`.
 *
 * No la referencia ninguna barra del Python, asi que no llega a la linea base y no hay
 * con que contrastar sus campos. Se traslada igualmente porque perderla en el camino y
 * no declararla nunca son indistinguibles a posteriori.
 */
extern const ToolDecl cursor_click;

extern const ToolDecl ruler;

}  // namespace defs_view3d_generic

/**
 * `_defs_view3d_select`. Las cuatro de seleccion, que ademas son el grupo de reserva de
 * la vista 3D: `tool_fallback_id` apunta a `builtin.select` y el reparto de teclas tiene
 * una variante "... (fallback)" para cada una de las cuatro.
 *
 * Aun asi las cuatro llevan `keymap_fallback` a `nullptr`, igual que en el editor de
 * nodos: el Python NO guarda ese nombre en la herramienta, lo compone al activarla a
 * partir de la reserva que tenga puesta el espacio (`space_toolsystem_common.py:1040`).
 * O sea que es cosa de la barra, no del catalogo, y ponerlo aqui seria decidir por ella.
 */
namespace defs_view3d_select {
extern const ToolDecl select;
extern const ToolDecl box;
extern const ToolDecl lasso;
extern const ToolDecl circle;
}  // namespace defs_view3d_select

}  // namespace flipendo::toolsystem

#endif /* __FL_TOOL_DEFS_VIEW3D_HH__ */
