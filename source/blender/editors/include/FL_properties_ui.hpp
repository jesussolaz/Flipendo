/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 *
 * Dibujo compartido por paneles de Propiedades y sus clones en otros editores.
 */

#pragma once

struct bContext;
struct Panel;

namespace flipendo::properties_ui {

/** Dibujo de WORLD_PT_viewport_display, reutilizado como NODE_WORLD_PT_viewport_display. */
void world_viewport_display_draw(const bContext *C, Panel *panel);

}  // namespace flipendo::properties_ui
