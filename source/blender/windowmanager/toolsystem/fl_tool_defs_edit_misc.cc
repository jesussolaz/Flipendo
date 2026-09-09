/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Herramientas de los modos "menores" de la vista 3D: armadura, curva, curvas, texto,
 * pose y particulas. Transliteracion de `_defs_edit_armature`, `_defs_edit_curve`,
 * `_defs_edit_curves`, `_defs_edit_text`, `_defs_pose` y `_defs_particle`
 * (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:684`, `:1229`, `:1321`, `:1339`,
 * `:1353` y `:1386`).
 *
 * Todos los `keymap_name` de aqui estan copiados literalmente de la linea base
 * (`tests/flipendo/toolsystem/baseline-python.txt`), no sintetizados. Este grupo es
 * justo donde se ve por que: `builtin.radius` y `builtin.tilt` los declara
 * `_defs_edit_curve` y la barra los reutiliza en EDIT_CURVES y EDIT_GREASE_PENCIL, y
 * en los tres modos el keymap sigue diciendo "Edit Curve" porque el Python lo fija al
 * registrar el PRIMER modo. Calcularlo por modo daria dos keymaps inexistentes.
 *
 * Al reves pasa con las que solo comparten `idname`: `builtin.extrude` sale en
 * armadura y en curva, y son DOS declaraciones con dos keymaps distintos porque en el
 * Python son dos `ToolDef` distintas. Fundirlas por `idname` romperia una de las dos.
 */

#include "BLT_translation.hh"

#include "RNA_prototypes.hh"

#include "fl_tool_defs_edit_misc.hh"

namespace flipendo::toolsystem {

/* -------------------------------------------------------------------- */
/** \name Ajustes compartidos
 * \{ */

/* `_template_widget.VIEW3D_GGT_xform_extrude.draw_settings` del Python
 * (`space_toolsystem_toolbar.py:90`): el eje del gizmo de extrusion, expandido.
 *
 * En el Python es una funcion suelta que comparten las extrusiones de malla, curva y
 * armadura precisamente para no repetir la fila. Aqui se conserva como UNA tabla en
 * vez de copiarla en cada declaracion, por lo mismo. Es de fichero porque la de malla
 * vive en otra unidad de traduccion y tiene la suya; el dato duplicado que importa no
 * repetirlo es el de dentro del fichero. */
static const PropRow xform_extrude_settings[] = {
    {PropSource::GizmoGroup, "VIEW3D_GGT_xform_extrude", "axis_type", nullptr, PROP_ROW_EXPAND},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name `_defs_edit_armature`
 * \{ */

namespace defs_edit_armature {

const ToolDecl roll = {
    /*idname*/ "builtin.roll",
    /*label*/ N_("Roll"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.armature.bone.roll",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_tool_generic_handle_free",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Armature, Roll",
};

const ToolDecl bone_envelope = {
    /*idname*/ "builtin.bone_envelope",
    /*label*/ N_("Bone Envelope"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.transform.bone_envelope",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_tool_generic_handle_free",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Armature, Bone Envelope",
};

const ToolDecl bone_size = {
    /*idname*/ "builtin.bone_size",
    /*label*/ N_("Bone Size"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.transform.bone_size",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_tool_generic_handle_free",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Armature, Bone Size",
};

const ToolDecl extrude = {
    /*idname*/ "builtin.extrude",
    /*label*/ N_("Extrude"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.armature.extrude_move",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_xform_extrude",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Armature, Extrude",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(xform_extrude_settings),
};

/* El metodo del Python se llama `extrude_cursor` pero el idname es
 * `builtin.extrude_to_cursor`, al reves que en curva. No es un descuido que se pueda
 * "arreglar": el idname viaja a DNA con el fichero del usuario. */
const ToolDecl extrude_cursor = {
    /*idname*/ "builtin.extrude_to_cursor",
    /*label*/ N_("Extrude to Cursor"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.armature.extrude_cursor",
    /*cursor*/ "CROSSHAIR",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Armature, Extrude to Cursor",
};

}  // namespace defs_edit_armature

/** \} */

/* -------------------------------------------------------------------- */
/** \name `_defs_edit_curve`
 * \{ */

namespace defs_edit_curve {

/* Sus ajustes son `curve_draw_settings` (`space_toolsystem_toolbar.py:1167`), una de
 * las seis funciones con flujo de control real: mira el tipo de region, decide si es
 * la cabecera o el popover "extra" y ramifica segun `cps.curve_type`. No cabe en filas
 * y no se inventa; queda marcada como deuda para que el verificador la liste. */
const ToolDecl draw = {
    /*idname*/ "builtin.draw",
    /*label*/ N_("Draw"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.curve.draw",
    /*cursor*/ "PAINT_BRUSH",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Curve, Draw",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ {},
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
    /*settings_pending*/ true,
};

const ToolDecl extrude = {
    /*idname*/ "builtin.extrude",
    /*label*/ N_("Extrude"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.curve.extrude_move",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_xform_extrude",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Curve, Extrude",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(xform_extrude_settings),
};

/* Aqui el idname es `builtin.extrude_cursor`, sin el `_to_` que si lleva el de
 * armadura, aunque la etiqueta de las dos sea "Extrude to Cursor". */
const ToolDecl extrude_cursor = {
    /*idname*/ "builtin.extrude_cursor",
    /*label*/ N_("Extrude to Cursor"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.curve.extrude_cursor",
    /*cursor*/ "CROSSHAIR",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Curve, Extrude to Cursor",
};

static const PropRow pen_settings[] = {
    {PropSource::Operator, "curve.pen", "close_spline"},
    {PropSource::Operator, "curve.pen", "extrude_handle"},
};

const ToolDecl pen = {
    /*idname*/ "builtin.pen",
    /*label*/ N_("Curve Pen"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.curve.pen",
    /*cursor*/ "CROSSHAIR",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Curve, Curve Pen",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(pen_settings),
};

/* La barra de EDIT_CURVES reutiliza esta misma declaracion, asi que su keymap dice
 * "Edit Curve" tambien alli. */
const ToolDecl tilt = {
    /*idname*/ "builtin.tilt",
    /*label*/ N_("Tilt"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.transform.tilt",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_tool_generic_handle_free",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Curve, Tilt",
};

/* La reutilizan EDIT_CURVES y EDIT_GREASE_PENCIL; de ahi que su keymap diga "Edit
 * Curve" en los tres modos. Es el ejemplo que cita el contrato. */
const ToolDecl curve_radius = {
    /*idname*/ "builtin.radius",
    /*label*/ N_("Radius"),
    /*description*/ N_("Expand or contract the radius of the selected curve points"),
    /*description_fn*/ nullptr,
    /*icon*/ "ops.curve.radius",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_tool_generic_handle_free",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Curve, Radius",
};

static const PropRow curve_vertex_randomize_settings[] = {
    {PropSource::Operator, "transform.vertex_random", "uniform"},
    {PropSource::Operator, "transform.vertex_random", "normal"},
    {PropSource::Operator, "transform.vertex_random", "seed"},
};

const ToolDecl curve_vertex_randomize = {
    /*idname*/ "builtin.randomize",
    /*label*/ N_("Randomize"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.curve.vertex_random",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_tool_generic_handle_normal",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Curve, Randomize",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(curve_vertex_randomize_settings),
};

}  // namespace defs_edit_curve

/** \} */

/* -------------------------------------------------------------------- */
/** \name `_defs_edit_curves`
 * \{ */

namespace defs_edit_curves {

/* En el Python esta envuelta en una funcion de una linea que solo reenvia a
 * `curve_draw_settings`, la misma que usa la de curva. Vale el mismo motivo: tiene
 * flujo de control y queda como deuda declarada.
 *
 * Comparte `idname` e icono con `defs_edit_curve::draw` pero es otra declaracion, y su
 * keymap lo demuestra: aqui "Edit Curves" y alli "Edit Curve". */
const ToolDecl draw = {
    /*idname*/ "builtin.draw",
    /*label*/ N_("Draw"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.curve.draw",
    /*cursor*/ "PAINT_BRUSH",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Curves, Draw",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ {},
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
    /*settings_pending*/ true,
};

}  // namespace defs_edit_curves

/** \} */

/* -------------------------------------------------------------------- */
/** \name `_defs_edit_text`
 * \{ */

namespace defs_edit_text {

/* Reusa el icono de la caja de seleccion generica porque no tiene uno propio, pero es
 * una herramienta distinta con su idname y su keymap. */
const ToolDecl select_text = {
    /*idname*/ "builtin.select_text",
    /*label*/ N_("Select Text"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.generic.select_box",
    /*cursor*/ "TEXT",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Text, Select Text",
};

}  // namespace defs_edit_text

/** \} */

/* -------------------------------------------------------------------- */
/** \name `_defs_pose`
 * \{ */

namespace defs_pose {

/* Las tres de pose van sin gizmo y sin ajustes: el trabajo lo hace entero el keymap
 * modal de su operador. */
const ToolDecl breakdown = {
    /*idname*/ "builtin.breakdowner",
    /*label*/ N_("Breakdowner"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.pose.breakdowner",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Pose, Breakdowner",
};

const ToolDecl push = {
    /*idname*/ "builtin.push",
    /*label*/ N_("Push"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.pose.push",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Pose, Push",
};

const ToolDecl relax = {
    /*idname*/ "builtin.relax",
    /*label*/ N_("Relax"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.pose.relax",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Pose, Relax",
};

}  // namespace defs_pose

/** \} */

/* -------------------------------------------------------------------- */
/** \name `_defs_particle`
 * \{ */

namespace defs_particle {

/* El struct RNA se pasa como funcion y no como puntero para que `FL_toolsystem.hpp`
 * no obligue a incluir `RNA_prototypes.hh` en cada tabla del catalogo; este es el
 * unico fichero de definiciones que lo necesita. */
static StructRNA *particle_edit_type()
{
  return &RNA_ParticleEdit;
}

/* `_defs_particle.generate_from_brushes` (`space_toolsystem_toolbar.py:1386`).
 *
 * Ojo con los dos campos de la enumeracion, que no significan lo mismo: el `name`
 * ("Comb") forma el idname y la etiqueta, y el `identifier` ("COMB") forma el icono en
 * minusculas y el `data_block`. El `data_block` es el que el motor lee y escribe para
 * sincronizar la herramienta con el pincel activo, asi que confundirlos rompe el viaje
 * de ida y vuelta sin dar ningun error.
 *
 * `cursor` y `use_separators` se dejan explicitos aunque coincidan con el valor por
 * defecto del contrato: son los que pasa el Python, y verlos aqui evita tener que ir a
 * comprobar si el defecto de los dos lados es el mismo. */
const EnumToolsDecl generate_from_brushes = {
    /*idname_prefix*/ "builtin_brush.",
    /*icon_prefix*/ "brush.particle.",
    /*type_fn*/ particle_edit_type,
    /*attr*/ "tool",
    /*options*/ TOOL_OPTION_USE_BRUSHES,
    /*cursor*/ "DEFAULT",
    /*use_separators*/ true,
};

}  // namespace defs_particle

/** \} */

}  // namespace flipendo::toolsystem
