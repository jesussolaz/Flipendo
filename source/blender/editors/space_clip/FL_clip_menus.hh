/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spclip
 *
 * Menus nativos del editor de clips (ver `fl_clip_menus.cc`). Sustituyen a las
 * clases `Menu` de `scripts/startup/bl_ui/space_clip.py` que el keymap nativo
 * abre por nombre. Ver `politicas/MENUS-DEL-KEYMAP-A-CPP.md`.
 */

#pragma once

namespace blender::ed::clip {

/** Da de alta los `MenuType` nativos del editor de clips. */
void clip_menus_register();

}  // namespace blender::ed::clip
