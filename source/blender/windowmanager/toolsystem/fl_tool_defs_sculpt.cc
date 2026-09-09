/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Herramientas del modo escultura. Transliteracion de `_defs_sculpt`
 * (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:1400`).
 *
 * Son 28 herramientas y ninguna lleva gizmo: seis pinceles y veintidos gestos que
 * dibujan una figura sobre la vista (caja, lazo, linea, polilinea) o filtran la malla
 * entera. Casi todas tienen ajustes, y casi todos son la misma forma — una sucesion de
 * `layout.prop` sobre las propiedades de un unico operador — asi que caben en filas
 * declarativas.
 *
 * Las seis que NO caben son las cuatro de lazo (`mask_lasso`, `hide_lasso`,
 * `face_set_lasso`, `trim_lasso`) y los dos filtros que cambian de controles segun el
 * tipo elegido (`mesh_filter`, `color_filter`). Van marcadas con `settings_pending`;
 * el porque de cada una esta junto a su declaracion.
 */

#include "BLT_translation.hh"

#include "fl_tool_defs_sculpt.hh"

namespace flipendo::toolsystem::defs_sculpt {

/* -------------------------------------------------------------------- */
/** \name Pinceles
 *
 * No tienen keymap propio: el atajo se lo da el pincel, no la herramienta. Por eso
 * `keymap_name` va a `nullptr` — asi sale en la linea base — y lo unico que los
 * distingue es `brush_type`.
 * \{ */

const ToolDecl mask = {
    /*idname*/ "builtin_brush.mask",
    /*label*/ N_("Mask"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "brush.sculpt.mask",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
    /*brush_type*/ "MASK",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES,
};

const ToolDecl draw_face_sets = {
    /*idname*/ "builtin_brush.draw_face_sets",
    /*label*/ N_("Draw Face Sets"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "brush.sculpt.draw_face_sets",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
    /*brush_type*/ "DRAW_FACE_SETS",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES,
};

const ToolDecl paint = {
    /*idname*/ "builtin_brush.paint",
    /*label*/ N_("Paint"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "brush.sculpt.paint",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
    /*brush_type*/ "PAINT",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES,
};

/* La funcion se llama `dyntopo_density` y la herramienta `builtin_brush.simplify`: el
 * pincel se llama Simplify por dentro y Density en la barra. No es un error de copia.
 *
 * Es la unica de las 28 que no aparece en la linea base, porque `poll_dyntopo` la
 * esconde cuando no hay objeto esculpido y el volcado se hace sin escena. Sus campos
 * salen entonces del Python, no de la linea base; el resto de pinceles del bloque
 * confirma que la forma es la correcta. */
const ToolDecl dyntopo_density = {
    /*idname*/ "builtin_brush.simplify",
    /*label*/ N_("Density"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "brush.sculpt.simplify",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
    /*brush_type*/ "SIMPLIFY",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES,
};

const ToolDecl multires_eraser = {
    /*idname*/ "builtin_brush.displacement_eraser",
    /*label*/ N_("Multires Displacement Eraser"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "brush.sculpt.displacement_eraser",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
    /*brush_type*/ "DISPLACEMENT_ERASER",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES,
};

const ToolDecl multires_smear = {
    /*idname*/ "builtin_brush.displacement_smear",
    /*label*/ N_("Multires Displacement Smear"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "brush.sculpt.displacement_smear",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
    /*brush_type*/ "DISPLACEMENT_SMEAR",
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Gestos de mascara
 *
 * Las cuatro figuras del mismo gesto — caja, lazo, linea y polilinea — llaman a cuatro
 * operadores DISTINTOS (`paint.mask_box_gesture`, `..._lasso_...`, `..._line_...`,
 * `..._polyline_...`), asi que sus filas no se pueden compartir aunque pinten la misma
 * propiedad: cada fila nombra a su operador.
 *
 * Las de lazo no se trasladan. Su `draw_settings` comparte
 * `_defs_sculpt.draw_lasso_stroke_settings`, que decide que pinta mirando si la region
 * es `TOOL_HEADER` y, cuando lo es, abre un popover al panel
 * `TOPBAR_PT_tool_settings_extra`. Ni el tipo de region ni ese panel estan disponibles
 * desde una fila declarativa, asi que van con `settings_pending`: preferimos la deuda
 * listada a un `draw_settings` nulo que nadie distingue de "no tiene ajustes".
 * \{ */

static const PropRow mask_border_settings[] = {
    {PropSource::Operator, "paint.mask_box_gesture", "use_front_faces_only"},
};

const ToolDecl mask_border = {
    /*idname*/ "builtin.box_mask",
    /*label*/ N_("Box Mask"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.sculpt.border_mask",
    /*cursor*/ "PAINT_CROSS",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Sculpt, Box Mask",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(mask_border_settings),
};

const ToolDecl mask_lasso = {
    /*idname*/ "builtin.lasso_mask",
    /*label*/ N_("Lasso Mask"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.sculpt.lasso_mask",
    /*cursor*/ "PAINT_CROSS",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Sculpt, Lasso Mask",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ {},
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
    /*settings_pending*/ true,
};

static const PropRow mask_line_settings[] = {
    {PropSource::Operator, "paint.mask_line_gesture", "use_front_faces_only"},
    {PropSource::Operator, "paint.mask_line_gesture", "use_limit_to_segment"},
};

const ToolDecl mask_line = {
    /*idname*/ "builtin.line_mask",
    /*label*/ N_("Line Mask"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.sculpt.line_mask",
    /*cursor*/ "PAINT_CROSS",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Sculpt, Line Mask",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(mask_line_settings),
};

static const PropRow mask_polyline_settings[] = {
    {PropSource::Operator, "paint.mask_polyline_gesture", "use_front_faces_only"},
};

const ToolDecl mask_polyline = {
    /*idname*/ "builtin.polyline_mask",
    /*label*/ N_("Polyline Mask"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.sculpt.polyline_mask",
    /*cursor*/ "PAINT_CROSS",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Sculpt, Polyline Mask",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(mask_polyline_settings),
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Gestos de ocultacion
 *
 * La de caja es la unica del bloque que llama al operador viejo `paint.hide_show` en
 * vez de a un `*_gesture`; las otras tres si tienen operador propio. Copiarle el
 * operador a una hermana pondria la propiedad en un sitio que no existe.
 * \{ */

static const PropRow hide_border_settings[] = {
    {PropSource::Operator, "paint.hide_show", "area"},
};

const ToolDecl hide_border = {
    /*idname*/ "builtin.box_hide",
    /*label*/ N_("Box Hide"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.sculpt.border_hide",
    /*cursor*/ "PAINT_CROSS",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Sculpt, Box Hide",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(hide_border_settings),
};

const ToolDecl hide_lasso = {
    /*idname*/ "builtin.lasso_hide",
    /*label*/ N_("Lasso Hide"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.sculpt.lasso_hide",
    /*cursor*/ "PAINT_CROSS",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Sculpt, Lasso Hide",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ {},
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
    /*settings_pending*/ true,
};

static const PropRow hide_line_settings[] = {
    {PropSource::Operator, "paint.hide_show_line_gesture", "use_limit_to_segment"},
};

const ToolDecl hide_line = {
    /*idname*/ "builtin.line_hide",
    /*label*/ N_("Line Hide"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.sculpt.line_hide",
    /*cursor*/ "PAINT_CROSS",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Sculpt, Line Hide",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(hide_line_settings),
};

static const PropRow hide_polyline_settings[] = {
    {PropSource::Operator, "paint.hide_show_polyline_gesture", "area"},
};

const ToolDecl hide_polyline = {
    /*idname*/ "builtin.polyline_hide",
    /*label*/ N_("Polyline Hide"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.sculpt.polyline_hide",
    /*cursor*/ "PAINT_CROSS",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Sculpt, Polyline Hide",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(hide_polyline_settings),
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Gestos de conjuntos de caras
 * \{ */

static const PropRow face_set_box_settings[] = {
    {PropSource::Operator, "sculpt.face_set_box_gesture", "use_front_faces_only"},
};

const ToolDecl face_set_box = {
    /*idname*/ "builtin.box_face_set",
    /*label*/ N_("Box Face Set"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.sculpt.border_face_set",
    /*cursor*/ "PAINT_CROSS",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Sculpt, Box Face Set",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(face_set_box_settings),
};

const ToolDecl face_set_lasso = {
    /*idname*/ "builtin.lasso_face_set",
    /*label*/ N_("Lasso Face Set"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.sculpt.lasso_face_set",
    /*cursor*/ "PAINT_CROSS",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Sculpt, Lasso Face Set",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ {},
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
    /*settings_pending*/ true,
};

static const PropRow face_set_line_settings[] = {
    {PropSource::Operator, "sculpt.face_set_line_gesture", "use_front_faces_only"},
    {PropSource::Operator, "sculpt.face_set_line_gesture", "use_limit_to_segment"},
};

const ToolDecl face_set_line = {
    /*idname*/ "builtin.line_face_set",
    /*label*/ N_("Line Face Set"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.sculpt.line_face_set",
    /*cursor*/ "PAINT_CROSS",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Sculpt, Line Face Set",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(face_set_line_settings),
};

static const PropRow face_set_polyline_settings[] = {
    {PropSource::Operator, "sculpt.face_set_polyline_gesture", "use_front_faces_only"},
};

const ToolDecl face_set_polyline = {
    /*idname*/ "builtin.polyline_face_set",
    /*label*/ N_("Polyline Face Set"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.sculpt.polyline_face_set",
    /*cursor*/ "PAINT_CROSS",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Sculpt, Polyline Face Set",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(face_set_polyline_settings),
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Gestos de recorte
 *
 * Los cuatro son el unico bloque sin `cursor`: recortan geometria de verdad, no pintan
 * sobre la superficie, asi que se quedan con el puntero normal. En la linea base salen
 * con `cursor=None` frente al `PAINT_CROSS` de mascara y ocultacion.
 *
 * La de linea no pinta `trim_mode`, que si pintan la de caja y la de polilinea: una
 * linea no encierra area, asi que el modo de recorte no le aplica. Es una diferencia
 * real del Python, no una omision.
 * \{ */

static const PropRow trim_box_settings[] = {
    {PropSource::Operator, "sculpt.trim_box_gesture", "trim_solver"},
    {PropSource::Operator, "sculpt.trim_box_gesture", "trim_mode"},
    {PropSource::Operator, "sculpt.trim_box_gesture", "trim_orientation"},
    {PropSource::Operator, "sculpt.trim_box_gesture", "trim_extrude_mode"},
    {PropSource::Operator, "sculpt.trim_box_gesture", "use_cursor_depth"},
};

const ToolDecl trim_box = {
    /*idname*/ "builtin.box_trim",
    /*label*/ N_("Box Trim"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.sculpt.box_trim",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Sculpt, Box Trim",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(trim_box_settings),
};

const ToolDecl trim_lasso = {
    /*idname*/ "builtin.lasso_trim",
    /*label*/ N_("Lasso Trim"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.sculpt.lasso_trim",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Sculpt, Lasso Trim",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ {},
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
    /*settings_pending*/ true,
};

static const PropRow trim_line_settings[] = {
    {PropSource::Operator, "sculpt.trim_line_gesture", "trim_solver"},
    {PropSource::Operator, "sculpt.trim_line_gesture", "trim_orientation"},
    {PropSource::Operator, "sculpt.trim_line_gesture", "trim_extrude_mode"},
    {PropSource::Operator, "sculpt.trim_line_gesture", "use_cursor_depth"},
    {PropSource::Operator, "sculpt.trim_line_gesture", "use_limit_to_segment"},
};

const ToolDecl trim_line = {
    /*idname*/ "builtin.line_trim",
    /*label*/ N_("Line Trim"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.sculpt.line_trim",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Sculpt, Line Trim",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(trim_line_settings),
};

static const PropRow trim_polyline_settings[] = {
    {PropSource::Operator, "sculpt.trim_polyline_gesture", "trim_solver"},
    {PropSource::Operator, "sculpt.trim_polyline_gesture", "trim_mode"},
    {PropSource::Operator, "sculpt.trim_polyline_gesture", "trim_orientation"},
    {PropSource::Operator, "sculpt.trim_polyline_gesture", "trim_extrude_mode"},
    {PropSource::Operator, "sculpt.trim_polyline_gesture", "use_cursor_depth"},
};

const ToolDecl trim_polyline = {
    /*idname*/ "builtin.polyline_trim",
    /*label*/ N_("Polyline Trim"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.sculpt.polyline_trim",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Sculpt, Polyline Trim",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(trim_polyline_settings),
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Proyeccion, filtros y edicion de conjuntos de caras
 * \{ */

static const PropRow project_line_settings[] = {
    {PropSource::Operator, "sculpt.project_line_gesture", "use_limit_to_segment"},
};

const ToolDecl project_line = {
    /*idname*/ "builtin.line_project",
    /*label*/ N_("Line Project"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.sculpt.line_project",
    /*cursor*/ "PAINT_CROSS",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Sculpt, Line Project",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(project_line_settings),
};

/* Sus ajustes cambian con el tipo de filtro elegido: `SURFACE_SMOOTH` anade tres
 * controles propios y `SHARPEN` otros tres. Una tabla de filas no puede leer el valor
 * de una propiedad para decidir si pinta la siguiente, asi que queda pendiente entera
 * en vez de dejar fuera, en silencio, seis controles. */
const ToolDecl mesh_filter = {
    /*idname*/ "builtin.mesh_filter",
    /*label*/ N_("Mesh Filter"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.sculpt.mesh_filter",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Sculpt, Mesh Filter",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ {},
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
    /*settings_pending*/ true,
};

/* El filtro de tela es el hermano de `mesh_filter` que SI cabe en filas: pinta los
 * mismos ocho controles siempre, sin mirar el tipo. Lo unico que se pierde es que el
 * Python mete `force_axis` en un `row(align=True)` de un solo elemento, que no tiene
 * vecinos con los que alinearse y por tanto se ve igual que una fila normal. */
static const PropRow cloth_filter_settings[] = {
    {PropSource::Operator, "sculpt.cloth_filter", "type"},
    {PropSource::Operator, "sculpt.cloth_filter", "strength"},
    {PropSource::Operator, "sculpt.cloth_filter", "force_axis"},
    {PropSource::Operator, "sculpt.cloth_filter", "orientation"},
    {PropSource::Operator, "sculpt.cloth_filter", "cloth_mass"},
    {PropSource::Operator, "sculpt.cloth_filter", "cloth_damping"},
    {PropSource::Operator, "sculpt.cloth_filter", "use_face_sets"},
    {PropSource::Operator, "sculpt.cloth_filter", "use_collisions"},
};

const ToolDecl cloth_filter = {
    /*idname*/ "builtin.cloth_filter",
    /*label*/ N_("Cloth Filter"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.sculpt.cloth_filter",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Sculpt, Cloth Filter",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(cloth_filter_settings),
};

/* Pendiente por lo mismo que `mesh_filter`, aunque aqui sea un solo control: el color
 * de relleno solo se pinta con el tipo `FILL`, y ademas va EN MEDIO de los otros dos.
 * Trasladar solo `type` y `strength` dejaria la herramienta sin poder elegir color. */
const ToolDecl color_filter = {
    /*idname*/ "builtin.color_filter",
    /*label*/ N_("Color Filter"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.sculpt.color_filter",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Sculpt, Color Filter",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ {},
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
    /*settings_pending*/ true,
};

static const PropRow mask_by_color_settings[] = {
    {PropSource::Operator, "sculpt.mask_by_color", "threshold"},
    {PropSource::Operator, "sculpt.mask_by_color", "contiguous"},
    {PropSource::Operator, "sculpt.mask_by_color", "invert"},
    {PropSource::Operator, "sculpt.mask_by_color", "preserve_previous_mask"},
};

const ToolDecl mask_by_color = {
    /*idname*/ "builtin.mask_by_color",
    /*label*/ N_("Mask by Color"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.sculpt.mask_by_color",
    /*cursor*/ "PAINT_CROSS",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Sculpt, Mask by Color",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(mask_by_color_settings),
};

static const PropRow face_set_edit_settings[] = {
    {PropSource::Operator, "sculpt.face_set_edit", "mode"},
    {PropSource::Operator, "sculpt.face_set_edit", "modify_hidden"},
};

/* La unica de las 28 cuyo keymap esta escrito a mano en el Python en vez de salir del
 * `keymap=()` que lo calcula. Sale igual que los calculados, asi que aqui no se nota;
 * se anota porque explica por que su nombre no sigue del todo a su etiqueta: la
 * herramienta se llama "Edit Face Set" y el keymap dice "Face Set Edit". */
const ToolDecl face_set_edit = {
    /*idname*/ "builtin.face_set_edit",
    /*label*/ N_("Edit Face Set"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.sculpt.face_set_edit",
    /*cursor*/ "PAINT_CROSS",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Sculpt, Face Set Edit",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(face_set_edit_settings),
};

/** \} */

}  // namespace flipendo::toolsystem::defs_sculpt
