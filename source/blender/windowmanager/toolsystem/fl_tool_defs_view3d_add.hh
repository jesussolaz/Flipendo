/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Herramientas de anadir primitivas de forma interactiva en la vista 3D.
 * Transliteracion de `_defs_view3d_add`
 * (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:495`).
 *
 * Las cinco son la misma herramienta con distinta primitiva: mismo gizmo de
 * colocacion, mismo keymap y las mismas dos funciones auxiliares. Lo unico que cambia
 * es el icono, la etiqueta, el prefijo de la descripcion y los dos o tres controles
 * propios de la primitiva que cada una anade al final de sus ajustes.
 */

#ifndef __FL_TOOL_DEFS_VIEW3D_ADD_HH__
#define __FL_TOOL_DEFS_VIEW3D_ADD_HH__

#include "FL_toolsystem.hpp"

namespace flipendo::toolsystem {

/** `_defs_view3d_add` del Python. Sale en los modos Object y Edit Mesh. */
namespace defs_view3d_add {

/* -------------------------------------------------------------------- */
/** \name Auxiliares compartidas por las cinco
 *
 * En el Python son dos metodos estaticos de la clase, no herramientas. Se declaran
 * aqui porque las cinco los comparten y porque de otro modo la deuda que representan
 * no quedaria escrita en ningun sitio: ambas son codigo, no dato, y ninguna de las dos
 * pertenece a esta fase.
 * \{ */

/* `description_interactive_add` NO se declara aqui: es una funcion interna del `.cc`.
 *
 * En el Python es un metodo estatico que las cinco comparten, pero fuera de ellas no la
 * usa nadie, y sacarla a la cabecera solo servia para que pareciera parte del contrato.
 * Lo que si comparte de verdad todo el catalogo son las ayudas de
 * `fl_tool_description.hh`.
 */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Las cinco primitivas
 * \{ */

extern const ToolDecl cube_add;
extern const ToolDecl cone_add;
extern const ToolDecl cylinder_add;
extern const ToolDecl uv_sphere_add;
extern const ToolDecl ico_sphere_add;

/** \} */

}  // namespace defs_view3d_add

}  // namespace flipendo::toolsystem

#endif /* __FL_TOOL_DEFS_VIEW3D_ADD_HH__ */
