/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spimage
 *
 * Los menus de la cabecera del editor de imagen y UV (ver `fl_image_ui.cc`).
 * Sustituyen a las clases `Menu` de `scripts/startup/bl_ui/space_image.py` que
 * NO abre el keymap (esas ya estan en `fl_image_menus.cc`).
 *
 * Ver `politicas/UI-A-CPP.md`.
 */

#pragma once

namespace blender::ed::image {

/** Da de alta los once menus de la cabecera del editor de imagen y UV. */
void image_ui_menus_register();

}  // namespace blender::ed::image
