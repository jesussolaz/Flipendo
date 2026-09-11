/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sptopbar
 *
 * Menus nativos de la barra superior (ver `fl_topbar_menus.cc`). Sustituyen a
 * las clases `Menu` de `scripts/startup/bl_ui/space_topbar.py` que el keymap
 * nativo abre por nombre. Ver `politicas/MENUS-DEL-KEYMAP-A-CPP.md`.
 */

#pragma once

namespace blender::ed::topbar {

/** Da de alta los `MenuType` nativos de la barra superior. */
void topbar_menus_register();

}  // namespace blender::ed::topbar
