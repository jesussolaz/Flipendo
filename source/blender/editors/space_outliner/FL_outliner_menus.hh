/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spoutliner
 *
 * Menus nativos de este editor (ver `fl_outliner_menus.cc`). Sustituyen a las
 * clases `Menu` de Python que el keymap nativo abre por nombre; sin ellos, con
 * `WITH_PYTHON=OFF` la tecla no abre nada y no da error. Ver
 * `politicas/MENUS-DEL-KEYMAP-A-CPP.md`.
 */

#pragma once

namespace blender::ed::outliner {

/** Da de alta los `MenuType` nativos de este editor. */
void outliner_menus_register();

}  // namespace blender::ed::outliner
