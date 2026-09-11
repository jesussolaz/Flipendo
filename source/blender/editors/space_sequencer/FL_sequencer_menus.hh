/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 *
 * Menus nativos del secuenciador (ver `fl_sequencer_menus.cc`). Sustituyen a las
 * clases `Menu` de `scripts/startup/bl_ui/space_sequencer.py` que el keymap
 * nativo abre por nombre. Ver `politicas/MENUS-DEL-KEYMAP-A-CPP.md`.
 */

#pragma once

namespace blender::ed::sequencer {

/** Da de alta los `MenuType` nativos del secuenciador. */
void sequencer_menus_register();

}  // namespace blender::ed::sequencer
