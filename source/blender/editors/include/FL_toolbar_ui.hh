/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * La barra de herramientas, dibujada en C++.
 *
 * Sustituye al dibujo de `ToolSelectPanelHelper` (`bl_ui/space_toolsystem_common.py`):
 * los cuatro paneles `*_PT_tools_active` de la region de herramientas y el menu
 * `WM_MT_toolsystem_submenu` que abre un grupo al mantener pulsado. Mismos
 * identificadores, para que todo lo que los nombra siga funcionando.
 *
 * Se verifica visualmente: `tests/flipendo/toolsystem/capture_toolbar_gui.py` recorta la
 * barra en 14 combinaciones de editor y modo, y el dibujo nativo tiene que salir
 * identico pixel a pixel al de Python.
 */

#pragma once

struct ARegionType;
struct bContext;
struct uiLayout;

namespace flipendo::ui {

/**
 * Da de alta el panel de la barra en la region de herramientas de un espacio, y (una sola
 * vez) el menu de grupo. `space_type`: `SPACE_VIEW3D`, `SPACE_IMAGE`, `SPACE_NODE` o
 * `SPACE_SEQ`.
 */
void toolbar_panels_register(ARegionType *art, int space_type);

/**
 * `draw_cls` del Python: la barra en `layout`.
 *
 * `detect_layout` elige una o dos columnas, y si hay texto, segun el ancho de la region;
 * sin el, una columna con texto (lo que usa el popover de `wm.toolbar`).
 */
void toolbar_draw(const bContext *C, uiLayout *layout, bool detect_layout, float scale_y);

/**
 * El icono de una herramienta ('ops.generic.select'...), cargado de
 * `datafiles/icons/<nombre>.dat` la primera vez y cacheado. Si falta o esta corrupto se
 * usa 'none', como en el Python.
 */
int tool_icon_value(const char *icon_name);

}  // namespace flipendo::ui
