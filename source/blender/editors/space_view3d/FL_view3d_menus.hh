/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 *
 * Menus nativos de la vista 3D: sustituyen, uno a uno, a las clases `Menu` de
 * `scripts/startup/bl_ui/space_view3d.py`.
 *
 * Por que existe este fichero
 * --------------------------
 * El keymap por defecto ya es C++ (`windowmanager/keymap/fl_keymap_g*.cc`) y
 * abre menus **por nombre** con `wm.call_menu` / `wm.call_menu_pie` /
 * `wm.call_panel`. Ese nombre se resuelve en `WM_menutype_find()`, que solo
 * conoce lo que alguien haya dado de alta. Mientras el menu solo exista como
 * clase de Python, un editor compilado con `WITH_PYTHON=OFF` no encuentra nada
 * y **no da error**: la tecla no hace nada. Ver `politicas/UI-A-CPP.md` y
 * `politicas/INVENTARIO-PYTHON.md` (§4.6).
 *
 * Cada menu se declara con `flipendo::MenuDecl` y se da de alta en
 * `view3d_menus_register()`, llamada desde `ED_spacetype_view3d()`.
 */

#pragma once

namespace blender::ed::view3d {

/** Da de alta los `MenuType` nativos de la vista 3D en el registro global. */
void view3d_menus_register();

/** Familia 2: los menus de borrado de los modos de edicion (tecla X) y vecinos. */
void view3d_edit_menus_register();

/** Familia 3: los submenus de la edicion de malla (Ctrl-V, Ctrl-E, Ctrl-F, Alt-N...). */
void view3d_mesh_menus_register();

/** Familia 4: objeto, pose y mapeado UV (Ctrl-A, U, Ctrl-L, Ctrl-H, Ctrl-G, Alt-P). */
void view3d_object_menus_register();

/** Familia 5: menus contextuales cortos y los del lapiz de cera. */
void view3d_context_menus_register();

/** Familia 6: los contextuales grandes de particulas, pose, curva y esqueleto. */
void view3d_ctxmode_menus_register();

}  // namespace blender::ed::view3d
