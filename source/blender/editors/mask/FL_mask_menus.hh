/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmask
 *
 * Ver `fl_mask_menus.cc`. Las mascaras no tienen editor propio, asi que el alta
 * cuelga de `ED_operatortypes_mask()`.
 */

#pragma once

namespace blender::ed::mask {

/** Da de alta los `MenuType` nativos de las mascaras. */
void menus_register();

}  // namespace blender::ed::mask
