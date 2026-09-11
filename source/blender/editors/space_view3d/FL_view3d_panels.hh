/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 *
 * Paneles nativos de la vista 3D: sustituyen, uno a uno, a las clases `Panel` de
 * `scripts/startup/bl_ui/space_view3d.py`.
 *
 * Por que hay un fichero aparte de los menus
 * -----------------------------------------
 * El keymap nativo abre doce de sus objetivos con `wm.call_panel`, no con
 * `wm.call_menu`. Un `PanelType` no vive en el registro global y ya esta, como un
 * `MenuType`: vive TAMBIEN en la lista ordenada de su region, y el volcado de registro
 * compara esa lista **en orden**. Con `order` a cero en todos, `panel_insert_ordered()`
 * anade al final entre iguales, asi que el orden de la lista es el orden de alta; y el
 * C++ se da de alta en `ED_spacetypes_init()`, mucho antes de que carguen los scripts.
 *
 * De ahi la regla de esta migracion, medida y escrita en
 * `politicas/MENUS-DEL-KEYMAP-A-CPP.md`: **lo que se migra de una region es un PREFIJO de
 * su lista**. Migrar los N primeros paneles de la region los deja en las N primeras
 * posiciones, que es donde ya estaban; migrar uno de en medio lo sube a la cabeza y el
 * volcado lo canta como diferencia aunque el panel sea identico.
 *
 * `VIEW_3D WINDOW` tiene once paneles y los once salen de `space_view3d.py`, asi que la
 * region se puede cerrar entera desde aqui.
 */

#pragma once

struct ARegionType;

namespace blender::ed::view3d {

/**
 * Paneles 1 a 4 de la region `VIEW_3D WINDOW`: los contextuales de los modos de pintado
 * y de escultura, que abre la tecla W (`wm.call_panel`).
 */
void view3d_paint_panels_register(ARegionType *art);

}  // namespace blender::ed::view3d
