/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spnla
 *
 * Menus nativos del editor de NLA (ver `fl_nla_menus.cc`). Sustituyen a las
 * clases `Menu` de `scripts/startup/bl_ui/space_nla.py` que el keymap nativo
 * abre por nombre. Ver `politicas/MENUS-DEL-KEYMAP-A-CPP.md`.
 */

#pragma once

namespace blender::ed::nla {

/** Da de alta los `MenuType` nativos del editor de NLA. */
void nla_menus_register();

}  // namespace blender::ed::nla
