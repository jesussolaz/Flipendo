/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 *
 * Menus nativos del explorador de ficheros y del de recursos (ver
 * `fl_file_menus.cc`). Sustituyen a las clases `Menu` de
 * `scripts/startup/bl_ui/space_filebrowser.py` que el keymap nativo abre por
 * nombre. Ver `politicas/MENUS-DEL-KEYMAP-A-CPP.md`.
 */

#pragma once

namespace blender::ed::file {

/** Da de alta los `MenuType` nativos del explorador de ficheros. */
void file_menus_register();

}  // namespace blender::ed::file
