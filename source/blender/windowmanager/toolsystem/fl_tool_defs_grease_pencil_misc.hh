/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Declaraciones de los modos de lapiz de cera que NO son el de pintar: edicion,
 * escultura, peso y vertices. Transliteracion de `_defs_grease_pencil_edit`,
 * `_defs_grease_pencil_sculpt`, `_defs_gpencil_weight`, `_defs_grease_pencil_weight` y
 * `_defs_grease_pencil_vertex` (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:2380`,
 * `:2684`, `:2711`, `:2716` y `:2748`).
 *
 * Van juntas porque son cinco grupos cortos del mismo objeto (once herramientas entre
 * todos) y separarlos en cinco ficheros solo repartiria la misma cabecera cinco veces.
 * El grupo de pintar, que es el grande, tiene fichero propio.
 *
 * Ojo con los `idname` repetidos: `builtin_brush.blur` esta en peso y en vertices, y
 * son herramientas DISTINTAS (distinto icono). El idname solo es unico dentro de un
 * modo, que es como los busca la barra; por eso cada grupo tiene su espacio de nombres
 * y no se puede colapsar en una sola declaracion compartida.
 */

#ifndef __FL_TOOL_DEFS_GREASE_PENCIL_MISC_HH__
#define __FL_TOOL_DEFS_GREASE_PENCIL_MISC_HH__

#include "FL_toolsystem.hpp"

namespace flipendo::toolsystem {

/** `_defs_grease_pencil_edit`. Solo sale en el modo `EDIT_GREASE_PENCIL`. */
namespace defs_grease_pencil_edit {
extern const ToolDecl shear;
extern const ToolDecl interpolate;
extern const ToolDecl texture_gradient;
}  // namespace defs_grease_pencil_edit

/** `_defs_grease_pencil_sculpt`. Solo sale en el modo `SCULPT_GREASE_PENCIL`. */
namespace defs_grease_pencil_sculpt {

/**
 * Filtro del bloque de seleccion: pide un objeto de lapiz de cera con alguna de las
 * tres mascaras de seleccion activas (`use_gpencil_select_mask_point`, `_stroke` o
 * `_segment`).
 *
 * Se declara aqui y se implementa en la fase de las barras, que es donde se usa: en el
 * Python es el `if` de una lambda dentro de la lista del modo, no un dato de la
 * herramienta.
 */
bool poll_select_mask(const bContext *C);

extern const ToolDecl clone;

}  // namespace defs_grease_pencil_sculpt

/**
 * `_defs_gpencil_weight`.
 *
 * El Python es literalmente `pass`, con el comentario "No mode specific tools currently
 * (only general ones)", y ninguna barra lo referencia. El espacio de nombres se declara
 * vacio a proposito: sin el, quien compare el catalogo con el Python no sabria si el
 * grupo esta vacio de verdad o si se olvido al trasladarlo.
 */
namespace defs_gpencil_weight {
}  // namespace defs_gpencil_weight

/** `_defs_grease_pencil_weight`. Solo sale en el modo `WEIGHT_GREASE_PENCIL`. */
namespace defs_grease_pencil_weight {
extern const ToolDecl blur;
extern const ToolDecl average;
extern const ToolDecl smear;
}  // namespace defs_grease_pencil_weight

/** `_defs_grease_pencil_vertex`. Solo sale en el modo `VERTEX_GREASE_PENCIL`. */
namespace defs_grease_pencil_vertex {

/**
 * Como el `poll_select_mask` de escultura pero sobre las mascaras de vertices
 * (`use_gpencil_vertex_select_mask_*`), y con una diferencia que no es cosmetica: sin
 * contexto este devuelve `false` y el de escultura devuelve `true`. Se implementa en la
 * fase de las barras.
 */
bool poll_select_mask(const bContext *C);

extern const ToolDecl blur;
extern const ToolDecl average;
extern const ToolDecl smear;
extern const ToolDecl replace;

}  // namespace defs_grease_pencil_vertex

}  // namespace flipendo::toolsystem

#endif /* __FL_TOOL_DEFS_GREASE_PENCIL_MISC_HH__ */
