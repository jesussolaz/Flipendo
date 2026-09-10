/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Herramientas del modo de pintura de lapiz de cera. Transliteracion de
 * `_defs_grease_pencil_paint` (`space_toolsystem_toolbar.py:2149`).
 *
 * Las once salen en un solo modo, `PAINT_GREASE_PENCIL`, y forman tres familias: los
 * dos pinceles que solo fijan `brush_type` (relleno y borrado), las seis primitivas de
 * trazo y las tres sueltas (recorte, cuentagotas e interpolacion).
 *
 * Las seis primitivas comparten en el Python un unico `grease_pencil_primitive_toolbar`,
 * y el cuentagotas cambia de forma segun su modo. Eso es codigo, no una tabla de filas:
 * sus `draw_settings` estan transliterados en
 * `editors/interface/fl_tool_settings_grease_pencil_paint.cc`, junto con las piezas de
 * `bl_ui/properties_paint_common.py` de las que tiran.
 *
 * Ninguna de las once registra `draw_cursor`.
 */

#include "BLT_translation.hh"

#include "fl_tool_defs_grease_pencil_paint.hh"

/* En `editors/interface/fl_tool_settings_grease_pencil_paint.cc`. */
namespace flipendo::ui::settings {
void draw_grease_pencil_paint_line(const bContext *C, uiLayout *layout, bToolRef *tref, bool extra);
void draw_grease_pencil_paint_polyline(const bContext *C,
                                       uiLayout *layout,
                                       bToolRef *tref,
                                       bool extra);
void draw_grease_pencil_paint_arc(const bContext *C, uiLayout *layout, bToolRef *tref, bool extra);
void draw_grease_pencil_paint_curve(const bContext *C,
                                    uiLayout *layout,
                                    bToolRef *tref,
                                    bool extra);
void draw_grease_pencil_paint_box(const bContext *C, uiLayout *layout, bToolRef *tref, bool extra);
void draw_grease_pencil_paint_circle(const bContext *C,
                                     uiLayout *layout,
                                     bToolRef *tref,
                                     bool extra);
void draw_grease_pencil_paint_eyedropper(const bContext *C,
                                         uiLayout *layout,
                                         bToolRef *tref,
                                         bool extra);
}  // namespace flipendo::ui::settings

namespace flipendo::toolsystem::defs_grease_pencil_paint {

/* -------------------------------------------------------------------- */
/** \name Pinceles
 *
 * No declaran keymap ni ajustes: son la herramienta generica de pincel limitada a un
 * tipo, y todo lo demas lo pone el pincel activo.
 * \{ */

const ToolDecl fill = {
    /*idname*/ "builtin_brush.Fill",
    /*label*/ N_("Fill"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "brush.gpencil_draw.fill",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
    /*brush_type*/ "FILL",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES,
};

const ToolDecl erase = {
    /*idname*/ "builtin_brush.Erase",
    /*label*/ N_("Erase"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "brush.gpencil_draw.erase",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
    /*brush_type*/ "ERASE",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Recorte
 * \{ */

/* Los dos ajustes del recorte no viven en la herramienta ni en el operador, sino en los
 * ajustes de lapiz del PINCEL activo, asi que la sub-ruta atraviesa un puntero a ID.
 * Cuando no hay pincel la ruta RNA no resuelve y la fila no se pinta; el Python en ese
 * mismo caso revienta con un AttributeError, con lo que no se pierde nada.
 *
 * Las dos van en la misma fila y con `use_property_split` desactivado en el Python. Eso
 * es colocacion, no dato: `PropRow` todavia no la expresa y se pinta una debajo de otra. */
static const PropRow trim_settings[] = {
    {PropSource::ToolSettingsSub, "gpencil_paint.brush.gpencil_settings", "use_active_layer_only"},
    {PropSource::ToolSettingsSub, "gpencil_paint.brush.gpencil_settings", "use_keep_caps_eraser"},
};

const ToolDecl trim = {
    /*idname*/ "builtin.trim",
    /*label*/ N_("Trim"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.gpencil.stroke_trim",
    /*cursor*/ "KNIFE",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Paint Grease Pencil, Trim",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(trim_settings),
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Primitivas de trazo
 *
 * Las seis son la misma herramienta con distinto operador de primitiva, y las seis se
 * limitan a los pinceles de dibujo: nada de borrar, rellenar ni tintar con ellas.
 * Sus ajustes son los del pincel activo y los pinta codigo; ver la cabecera.
 * \{ */

const ToolDecl line = {
    /*idname*/ "builtin.line",
    /*label*/ N_("Line"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.gpencil.primitive_line",
    /*cursor*/ "CROSSHAIR",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Paint Grease Pencil, Line",
    /*brush_type*/ "DRAW",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES | TOOL_OPTION_NO_BRUSH_FALLBACK,
    /*settings*/ {},
    /*draw_settings*/ flipendo::ui::settings::draw_grease_pencil_paint_line,
    /*draw_cursor*/ nullptr,
    /*pending*/ TOOL_PENDING_NONE,
};

const ToolDecl polyline = {
    /*idname*/ "builtin.polyline",
    /*label*/ N_("Polyline"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.gpencil.primitive_polyline",
    /*cursor*/ "CROSSHAIR",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Paint Grease Pencil, Polyline",
    /*brush_type*/ "DRAW",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES | TOOL_OPTION_NO_BRUSH_FALLBACK,
    /*settings*/ {},
    /*draw_settings*/ flipendo::ui::settings::draw_grease_pencil_paint_polyline,
    /*draw_cursor*/ nullptr,
    /*pending*/ TOOL_PENDING_NONE,
};

const ToolDecl arc = {
    /*idname*/ "builtin.arc",
    /*label*/ N_("Arc"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.gpencil.primitive_arc",
    /*cursor*/ "CROSSHAIR",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Paint Grease Pencil, Arc",
    /*brush_type*/ "DRAW",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES | TOOL_OPTION_NO_BRUSH_FALLBACK,
    /*settings*/ {},
    /*draw_settings*/ flipendo::ui::settings::draw_grease_pencil_paint_arc,
    /*draw_cursor*/ nullptr,
    /*pending*/ TOOL_PENDING_NONE,
};

const ToolDecl curve = {
    /*idname*/ "builtin.curve",
    /*label*/ N_("Curve"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.gpencil.primitive_curve",
    /*cursor*/ "CROSSHAIR",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Paint Grease Pencil, Curve",
    /*brush_type*/ "DRAW",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES | TOOL_OPTION_NO_BRUSH_FALLBACK,
    /*settings*/ {},
    /*draw_settings*/ flipendo::ui::settings::draw_grease_pencil_paint_curve,
    /*draw_cursor*/ nullptr,
    /*pending*/ TOOL_PENDING_NONE,
};

const ToolDecl box = {
    /*idname*/ "builtin.box",
    /*label*/ N_("Box"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.gpencil.primitive_box",
    /*cursor*/ "CROSSHAIR",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Paint Grease Pencil, Box",
    /*brush_type*/ "DRAW",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES | TOOL_OPTION_NO_BRUSH_FALLBACK,
    /*settings*/ {},
    /*draw_settings*/ flipendo::ui::settings::draw_grease_pencil_paint_box,
    /*draw_cursor*/ nullptr,
    /*pending*/ TOOL_PENDING_NONE,
};

const ToolDecl circle = {
    /*idname*/ "builtin.circle",
    /*label*/ N_("Circle"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.gpencil.primitive_circle",
    /*cursor*/ "CROSSHAIR",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Paint Grease Pencil, Circle",
    /*brush_type*/ "DRAW",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES | TOOL_OPTION_NO_BRUSH_FALLBACK,
    /*settings*/ {},
    /*draw_settings*/ flipendo::ui::settings::draw_grease_pencil_paint_circle,
    /*draw_cursor*/ nullptr,
    /*pending*/ TOOL_PENDING_NONE,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Interpolacion y cuentagotas
 * \{ */

/* El orden de las filas es el del Python y no el alfabetico ni el del operador: es lo
 * que ve el usuario en la cabecera. Ojo, la interpolacion del modo de EDICION pinta las
 * mismas cinco propiedades en otro orden; son dos herramientas distintas que comparten
 * idname, asi que no se pueden compartir estas filas. */
static const PropRow interpolate_settings[] = {
    {PropSource::Operator, "grease_pencil.interpolate", "layers"},
    {PropSource::Operator, "grease_pencil.interpolate", "flip"},
    {PropSource::Operator, "grease_pencil.interpolate", "smooth_factor"},
    {PropSource::Operator, "grease_pencil.interpolate", "smooth_steps"},
    {PropSource::Operator, "grease_pencil.interpolate", "exclude_breakdowns"},
};

const ToolDecl interpolate = {
    /*idname*/ "builtin.interpolate",
    /*label*/ N_("Interpolate"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.pose.breakdowner",
    /*cursor*/ "DEFAULT",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Paint Grease Pencil, Interpolate",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(interpolate_settings),
};

/* Los ajustes del cuentagotas cambian de forma segun el modo elegido: con material
 * pintan otra propiedad, y con paleta montan un selector de ID y una rejilla de colores
 * de la paleta activa. Eso es flujo de control y plantillas de interfaz, no una lista de
 * filas, asi que los pinta codigo; ver la cabecera. */
const ToolDecl eyedropper = {
    /*idname*/ "builtin.eyedropper",
    /*label*/ N_("Eyedropper"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.paint.eyedropper_add",
    /*cursor*/ "EYEDROPPER",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Paint Grease Pencil, Eyedropper",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ {},
    /*draw_settings*/ flipendo::ui::settings::draw_grease_pencil_paint_eyedropper,
    /*draw_cursor*/ nullptr,
    /*pending*/ TOOL_PENDING_NONE,
};

/** \} */

}  // namespace flipendo::toolsystem::defs_grease_pencil_paint
