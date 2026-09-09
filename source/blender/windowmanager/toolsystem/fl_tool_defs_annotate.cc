/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Herramientas de anotacion. Transliteracion de `_defs_annotate`
 * (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:162`).
 *
 * Tres de las cuatro comparten `draw_settings_common`, que es una de las SEIS
 * funciones de ajustes con flujo de control real de todo el catalogo: mira el tipo de
 * espacio, el tipo de region y el idname de la propia herramienta para pintar cosas
 * distintas. No cabe en filas declarativas, asi que ira como codigo.
 *
 * Todavia no esta: dentro dibuja un popover al panel `TOPBAR_PT_annotation_layers`,
 * que sigue siendo un panel de Python. Hasta que la migracion de `bl_ui` lo haga
 * nativo, esas tres van marcadas con `settings_pending` para que la deuda salga
 * listada en cada verificacion en vez de desaparecer.
 */

#include "BLT_translation.hh"

#include "fl_tool_defs_annotate.hh"

namespace flipendo::toolsystem::defs_annotate {

const ToolDecl scribble = {
    /*idname*/ "builtin.annotate",
    /*label*/ N_("Annotate"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.gpencil.draw",
    /*cursor*/ "PAINT_BRUSH",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "Generic Tool: Annotate",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_KEYMAP_FALLBACK,
    /*settings*/ {},
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
    /*settings_pending*/ true,
};

const ToolDecl line = {
    /*idname*/ "builtin.annotate_line",
    /*label*/ N_("Annotate Line"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.gpencil.draw.line",
    /*cursor*/ "PAINT_BRUSH",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "Generic Tool: Annotate Line",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_KEYMAP_FALLBACK,
    /*settings*/ {},
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
    /*settings_pending*/ true,
};

const ToolDecl poly = {
    /*idname*/ "builtin.annotate_polygon",
    /*label*/ N_("Annotate Polygon"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.gpencil.draw.poly",
    /*cursor*/ "PAINT_BRUSH",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "Generic Tool: Annotate Polygon",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_KEYMAP_FALLBACK,
    /*settings*/ {},
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
    /*settings_pending*/ true,
};

/* El borrador es el unico de los cuatro que NO usa `draw_settings_common`: su unico
 * ajuste es el radio, y ese sigue viviendo en las preferencias del usuario en vez de
 * en `tool_settings` (hay un TODO en el Python desde hace anos). Por eso cabe en una
 * fila declarativa y las otras tres no. */
static const PropRow eraser_settings[] = {
    {PropSource::PreferencesEdit, nullptr, "grease_pencil_eraser_radius", N_("Radius")},
};

const ToolDecl eraser = {
    /*idname*/ "builtin.annotate_eraser",
    /*label*/ N_("Annotate Eraser"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.gpencil.draw.eraser",
    /*cursor*/ "ERASER",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "Generic Tool: Annotate Eraser",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_KEYMAP_FALLBACK,
    /*settings*/ span(eraser_settings),
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
};

}  // namespace flipendo::toolsystem::defs_annotate
