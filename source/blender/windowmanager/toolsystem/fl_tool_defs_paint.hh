/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Herramientas de los tres modos de pintado sobre malla. Transliteracion de
 * `_defs_vertex_paint`, `_defs_texture_paint` y `_defs_weight_paint`
 * (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:1920`, `:1962` y `:2032`).
 *
 * Casi todas son pinceles: sin keymap propio, sin gizmos y sin ajustes. Lo unico que
 * aportan sobre el `builtin.brush` generico de la barra es el `brush_type` al que se
 * limitan, asi que activarlas equivale a elegir un tipo de pincel. Las tres unicas que
 * son operadores de verdad estan en pintado de pesos: `gradient`, `sample_weight` y
 * `sample_vertex_group`, que si llevan keymap y cursor propios.
 *
 * Los `poll_*` se declaran aqui pero se implementan en la fase de las barras, que es
 * donde se usan: filtran el bloque de seleccion segun las mascaras de pintado de la
 * malla activa. Declararlos ya evita que al montar las barras se inventen otros con
 * distinta semantica.
 */

#ifndef __FL_TOOL_DEFS_PAINT_HH__
#define __FL_TOOL_DEFS_PAINT_HH__

#include "FL_toolsystem.hpp"

namespace flipendo::toolsystem {

/** `_defs_vertex_paint` del Python. */
namespace defs_vertex_paint {

/** El bloque de seleccion solo sale si la malla activa tiene mascara de caras o de
 * vertices; sin mascara no hay nada que seleccionar en modo pintado. */
bool poll_select_mask(const bContext *C);

extern const ToolDecl blur;
extern const ToolDecl average;
extern const ToolDecl smear;

}  // namespace defs_vertex_paint

/** `_defs_texture_paint`. */
namespace defs_texture_paint {

/** Como el de pintado de vertices, pero aqui solo cuenta la mascara de CARAS: pintar
 * textura se hace por cara, no por vertice. */
bool poll_select_mask(const bContext *C);

/**
 * OJO: no la referencia ninguna barra del Python, ni la de la vista 3D ni la del
 * editor de imagen. Ambas usan su propio `_brush_tool` (`builtin.brush`, etiqueta
 * "Brush", icono `brush.generic`), asi que esta declaracion no llega a la linea base y
 * sus campos NO coinciden con los del `builtin.brush` que si sale alli.
 *
 * Se traslada igualmente para no perderla en el camino, pero enlazarla en una barra
 * duplicaria el idname `builtin.brush` con otra etiqueta y otro icono.
 */
extern const ToolDecl brush;

extern const ToolDecl blur;
extern const ToolDecl smear;
extern const ToolDecl clone;
extern const ToolDecl fill;
extern const ToolDecl mask;

}  // namespace defs_texture_paint

/** `_defs_weight_paint`. */
namespace defs_weight_paint {

/**
 * En el Python esto no devuelve un booleano sino la lista de herramientas de
 * seleccion, o una tupla vacia. Aqui es un filtro sobre el bloque de seleccion, que es
 * lo mismo dicho de la otra forma: hay mascara de pintado, o hay un armazon en pose, o
 * el bloque no se muestra.
 */
bool poll_select_tools(const bContext *C);

extern const ToolDecl blur;
extern const ToolDecl average;
extern const ToolDecl smear;
extern const ToolDecl sample_weight;
extern const ToolDecl sample_weight_group;
extern const ToolDecl gradient;

}  // namespace defs_weight_paint

}  // namespace flipendo::toolsystem

#endif /* __FL_TOOL_DEFS_PAINT_HH__ */
