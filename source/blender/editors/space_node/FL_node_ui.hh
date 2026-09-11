/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spnode
 *
 * La interfaz del editor de nodos (ver `fl_node_ui.cc`). Sustituye a las clases
 * `Header`, `Menu` y `Panel` de `scripts/startup/bl_ui/space_node.py`.
 *
 * Ver `politicas/UI-A-CPP.md`.
 */

#pragma once

struct ARegionType;

namespace blender::ed::space_node {

/** Los menus de la cabecera y de la barra lateral (registro global). */
void node_ui_menus_register();

/** La cabecera `NODE_HT_header`, en la region de cabecera. */
void node_ui_header_register(ARegionType *art);

/** Los seis paneles emergentes que cuelgan de la cabecera. */
void node_ui_header_panels_register(ARegionType *art);

/** Los paneles de la barra lateral (region `UI`). */
void node_ui_panels_register(ARegionType *art);

}  // namespace blender::ed::space_node
