/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 *
 * Ver `fl_grease_pencil_menus.cc`. No hay editor de lapiz de cera, asi que el
 * alta cuelga de `ED_operatortypes_grease_pencil()`.
 */

#pragma once

namespace blender::ed::greasepencil {

/** Da de alta los `MenuType` nativos del lapiz de cera. */
void menus_register();

}  // namespace blender::ed::greasepencil
