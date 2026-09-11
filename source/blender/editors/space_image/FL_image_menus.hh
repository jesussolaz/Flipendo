/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spimage
 *
 * Menus nativos del editor de imagen y UV (ver `fl_image_menus.cc`). Sustituyen
 * a las clases `Menu` de `scripts/startup/bl_ui/space_image.py` que el keymap
 * nativo abre por nombre. Ver `politicas/MENUS-DEL-KEYMAP-A-CPP.md`.
 */

#pragma once

namespace blender::ed::image {

/** Da de alta los `MenuType` nativos del editor de imagen y UV. */
void image_menus_register();

}  // namespace blender::ed::image
