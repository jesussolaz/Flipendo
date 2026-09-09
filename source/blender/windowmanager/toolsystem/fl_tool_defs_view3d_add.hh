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

/**
 * `description_interactive_add` (`space_toolsystem_toolbar.py:498`).
 *
 * Las cinco descripciones son la misma plantilla con distinto prefijo, y NINGUNA es un
 * literal: la funcion abre el keymap DEL USUARIO "View3D Placement Modal", busca los
 * elementos con `propvalue` `SNAP_ON`, `PIVOT_CENTER_ON` y `FIXED_ASPECT_ON`, y mete
 * sus atajos dentro del texto. Por eso las cinco llevan `description` a `nullptr`: no
 * hay cadena que copiar.
 *
 * Queda declarada y sin definir a proposito. Es una de las siete descripciones
 * calculadas del catalogo, que se escriben a mano en su propia fase; hasta entonces
 * `description_fn` sigue a `nullptr` y el tooltip cae en el del operador, que es
 * exactamente lo que hace hoy el motor cuando el Python no responde. El prefijo de
 * cada herramienta queda anotado en su declaracion del `.cc` para que no se pierda.
 */
std::string description_interactive_add(const bContext *C, const wmKeyMap *km, const char *prefix);

/**
 * `draw_settings_interactive_add` (`space_toolsystem_toolbar.py:530`).
 *
 * Una de las seis funciones de ajustes con flujo de control real de todo el catalogo:
 * decide segun `extra` y segun el TIPO DE REGION (`TOOL_HEADER` o no) si pinta la fila
 * de profundidad, orientacion y ajuste, o el bloque de ejes y origenes del popover. No
 * cabe en filas `PropRow`, asi que las cinco herramientas van con `settings_pending`.
 *
 * Devuelve, como en el Python, si la envolvente debe rematar pintando el popover
 * `TOPBAR_PT_tool_settings_extra`. Sin ese valor de vuelta las cinco pintarian el
 * popover dos veces o ninguna, segun la region.
 */
bool draw_settings_interactive_add(const bContext *C, uiLayout *layout, bToolRef *tref, bool extra);

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
