/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spnode
 *
 * Menus nativos del editor de nodos (ver `fl_node_menus.cc`). Sustituyen a las
 * clases `Menu` de `scripts/startup/bl_ui/space_node.py` que el keymap nativo
 * abre por nombre. Ver `politicas/MENUS-DEL-KEYMAP-A-CPP.md`.
 */

#pragma once

namespace blender::ed::space_node {

/** Da de alta los `MenuType` nativos del editor de nodos. */
void node_menus_register();

}  // namespace blender::ed::space_node
