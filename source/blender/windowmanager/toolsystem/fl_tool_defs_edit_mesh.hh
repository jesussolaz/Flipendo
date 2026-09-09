/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Declaraciones de las herramientas de edicion de malla. Transliteracion de
 * `_defs_edit_mesh` (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:739`).
 *
 * Tienen cabecera propia en vez de ir en `fl_tool_defs.hh` porque son 22 de las 43 que
 * ve el modo EDIT_MESH y ningun otro espacio las mira, salvo `to_sphere`, que la
 * edicion de Grease Pencil reutiliza tal cual (de ahi que su keymap diga "Edit Mesh"
 * tambien alli).
 */

#ifndef __FL_TOOL_DEFS_EDIT_MESH_HH__
#define __FL_TOOL_DEFS_EDIT_MESH_HH__

#include "FL_toolsystem.hpp"

namespace flipendo::toolsystem {

/** `_defs_edit_mesh` del Python. Solo las usa la vista 3D en modo EDIT_MESH. */
namespace defs_edit_mesh {

extern const ToolDecl rip_region;
extern const ToolDecl rip_edge;
extern const ToolDecl poly_build;
extern const ToolDecl edge_slide;
extern const ToolDecl vert_slide;
extern const ToolDecl spin;
extern const ToolDecl inset;
extern const ToolDecl bevel;
extern const ToolDecl extrude;
extern const ToolDecl extrude_manifold;
extern const ToolDecl extrude_normals;
extern const ToolDecl extrude_individual;
extern const ToolDecl extrude_cursor;
extern const ToolDecl loopcut_slide;
extern const ToolDecl offset_edge_loops_slide;
extern const ToolDecl vertex_smooth;
extern const ToolDecl vertex_randomize;
extern const ToolDecl tosphere;
extern const ToolDecl shrink_fatten;
extern const ToolDecl push_pull;
extern const ToolDecl knife;
extern const ToolDecl bisect;

/**
 * Descripcion calculada de `builtin.poly_build`, una de las siete del catalogo.
 *
 * El Python la arma leyendo TRES atajos del keymap del usuario
 * (`mesh.polybuild_face_at_cursor_move`, `mesh.polybuild_extrude_at_cursor_move` y
 * `mesh.polybuild_delete_at_cursor`) y metiendolos dentro del texto; con el keymap a
 * `None` pone "<none>" en cada hueco. No es traducible a una cadena literal, asi que
 * queda declarada aqui y se implementara en la fase de las descripciones, junto con
 * las otras seis. Hasta entonces `poly_build.description_fn` sigue a `nullptr`: darle
 * este simbolo sin definir dejaria el enlazado roto para todo el que incluya el
 * catalogo.
 */
std::string poly_build_description(const bContext *C, const wmKeyMap *km);

}  // namespace defs_edit_mesh

}  // namespace flipendo::toolsystem

#endif /* __FL_TOOL_DEFS_EDIT_MESH_HH__ */
