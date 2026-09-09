/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Declaraciones de las herramientas del modo escultura. Transliteracion de
 * `_defs_sculpt` (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:1400`).
 *
 * Los nombres del espacio de nombres y de cada herramienta se mantienen uno a uno con
 * los del Python — `defs_sculpt::mask_border` es `_defs_sculpt.mask_border` — porque es
 * lo que permite revisar el traslado abriendo los dos ficheros lado a lado. Ojo con la
 * pareja idname/nombre: la que se llama `mask_border` declara `builtin.box_mask`, y la
 * que se llama `dyntopo_density` declara `builtin_brush.simplify`; en este bloque el
 * nombre de la funcion y el identificador casi nunca coinciden.
 */

#ifndef __FL_TOOL_DEFS_SCULPT_HH__
#define __FL_TOOL_DEFS_SCULPT_HH__

#include "FL_toolsystem.hpp"

namespace flipendo::toolsystem {

/** `_defs_sculpt` del Python. Solo aparecen en el modo escultura de la vista 3D. */
namespace defs_sculpt {

/* Pinceles: los cinco tipos de pincel de escultura que tienen entrada propia en la
 * barra, ademas del `builtin.brush` generico (que no es de este bloque). */

extern const ToolDecl mask;
extern const ToolDecl draw_face_sets;
extern const ToolDecl paint;

/**
 * `_defs_sculpt.poll_dyntopo`: el pincel de densidad solo sale con topologia dinamica
 * activa en el objeto esculpido.
 *
 * Sin implementar aqui a proposito. Los filtros son de la COLOCACION, no de la
 * herramienta: quien los llama es la entrada de la barra (`ToolEntry::poll`), que
 * todavia no existe. Implementarlos ahora obligaria a este fichero a conocer el
 * contexto y los modificadores, y quedaria un cuerpo sin nadie que lo llamase.
 */
bool poll_dyntopo(const bContext *C);

extern const ToolDecl dyntopo_density;

/** `_defs_sculpt.poll_multires`: las dos de multiresolucion solo salen si el objeto
 * lleva un modificador MULTIRES. Sin implementar, por lo mismo que `poll_dyntopo`. */
bool poll_multires(const bContext *C);

extern const ToolDecl multires_eraser;
extern const ToolDecl multires_smear;

/* Gestos de mascara, ocultacion, conjuntos de caras y recorte. */

extern const ToolDecl mask_border;
extern const ToolDecl mask_lasso;
extern const ToolDecl mask_line;
extern const ToolDecl mask_polyline;

extern const ToolDecl hide_border;
extern const ToolDecl hide_lasso;
extern const ToolDecl hide_line;
extern const ToolDecl hide_polyline;

extern const ToolDecl face_set_box;
extern const ToolDecl face_set_lasso;
extern const ToolDecl face_set_line;
extern const ToolDecl face_set_polyline;

extern const ToolDecl trim_box;
extern const ToolDecl trim_lasso;
extern const ToolDecl trim_line;
extern const ToolDecl trim_polyline;

extern const ToolDecl project_line;

/* Filtros y edicion sobre la malla entera. */

extern const ToolDecl mesh_filter;
extern const ToolDecl cloth_filter;
extern const ToolDecl color_filter;
extern const ToolDecl mask_by_color;
extern const ToolDecl face_set_edit;

}  // namespace defs_sculpt

}  // namespace flipendo::toolsystem

#endif /* __FL_TOOL_DEFS_SCULPT_HH__ */
