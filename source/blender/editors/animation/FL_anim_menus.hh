/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edanimation
 *
 * Ver `fl_anim_menus.cc`. La animacion no es un editor, asi que el alta cuelga
 * de `ED_operatortypes_anim()`.
 */

#pragma once

namespace blender::ed::animation {

/** Da de alta los `MenuType` nativos de animacion. */
void menus_register();

}  // namespace blender::ed::animation
