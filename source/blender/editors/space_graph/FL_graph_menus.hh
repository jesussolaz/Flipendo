/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spgraph
 *
 * Menus nativos del editor de curvas (ver `fl_graph_menus.cc`). Sustituyen a las
 * clases `Menu` de `scripts/startup/bl_ui/space_graph.py` que el keymap nativo
 * abre por nombre; sin ellos, con `WITH_PYTHON=OFF` esas teclas no abren nada y
 * no dan error. Ver `politicas/MENUS-DEL-KEYMAP-A-CPP.md`.
 */

#pragma once

namespace blender::ed::graph {

/** Da de alta los `MenuType` nativos del editor de curvas. */
void graph_menus_register();

}  // namespace blender::ed::graph
