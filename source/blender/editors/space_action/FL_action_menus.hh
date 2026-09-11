/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spaction
 *
 * Menus nativos de la hoja de exposicion (ver `fl_action_menus.cc`). Sustituyen
 * a las clases `Menu` de `scripts/startup/bl_ui/space_dopesheet.py` que el
 * keymap nativo abre por nombre. Ver `politicas/MENUS-DEL-KEYMAP-A-CPP.md`.
 */

#pragma once

namespace blender::ed::action {

/** Da de alta los `MenuType` nativos de la hoja de exposicion. */
void action_menus_register();

}  // namespace blender::ed::action
