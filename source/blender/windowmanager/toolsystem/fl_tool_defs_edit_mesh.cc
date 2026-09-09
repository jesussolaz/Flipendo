/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Herramientas de edicion de malla. Transliteracion de `_defs_edit_mesh`
 * (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:739`).
 *
 * Es el bloque mas grande de la vista 3D: 22 herramientas, de las que 15 llevan
 * ajustes. Tres cosas que no se ven leyendo el Python de corrido:
 *
 * - Cuatro de esos ajustes no tocan el operador de la herramienta sino un
 *   sub-operador de su MACRO (`props.MESH_OT_rip.use_fill` y compania). La ruta va
 *   entera dentro del nombre de la propiedad, porque `PropRow` no tiene un campo para
 *   el sub-operador; quien pinte las filas tiene que resolverla como ruta RNA y no
 *   como nombre suelto, o esos cuatro controles desaparecen sin ruido.
 *
 * - `bevel` y `knife` miran el tipo de region para repartir sus ajustes entre la
 *   cabecera y el popover "extra", y `bevel` ademas cambia lo que pinta segun el
 *   modo de bisel y el tipo de perfil. Ninguna de las dos cabe en filas, asi que van
 *   con `settings_pending` para que la deuda salga listada en cada verificacion.
 *
 * - `builtin.rip_region` y `builtin.extrude_to_cursor` existen tambien en el editor
 *   de imagen y en la edicion de esqueleto, pero son ToolDef DISTINTOS de otras
 *   clases del Python, con otro icono y otro keymap. Los idnames repetidos en la
 *   linea base no son un error: son herramientas homonimas de espacios distintos.
 */

#include <fmt/format.h>

#include "BLT_translation.hh"

#include "WM_api.hh"
#include "WM_keymap.hh"
#include "wm_event_types.hh"

#include "fl_tool_description.hh"

#include "fl_tool_defs_edit_mesh.hh"

namespace flipendo::toolsystem {

namespace defs_edit_mesh {

/* -------------------------------------------------------------------- */
/** \name Rasgar
 * \{ */

/* El Python hace `props = tool.operator_properties("mesh.rip_move")` y luego
 * `props.MESH_OT_rip`: la propiedad vive en el sub-operador del macro, no en el macro.
 * La ruta se guarda completa por lo dicho en la cabecera del fichero. */
static const PropRow rip_region_settings[] = {
    {PropSource::Operator, "mesh.rip_move", "MESH_OT_rip.use_fill"},
};

const ToolDecl rip_region = {
    /*idname*/ "builtin.rip_region",
    /*label*/ N_("Rip Region"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.mesh.rip",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_tool_generic_handle_free",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Mesh, Rip Region",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(rip_region_settings),
};

const ToolDecl rip_edge = {
    /*idname*/ "builtin.rip_edge",
    /*label*/ N_("Rip Edge"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.mesh.rip_edge",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_tool_generic_handle_free",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Mesh, Rip Edge",
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Construir a mano
 * \{ */

/* Otro ajuste de sub-operador de macro: `props.MESH_OT_polybuild_face_at_cursor`. */
static const PropRow poly_build_settings[] = {
    {PropSource::Operator,
     "mesh.polybuild_face_at_cursor_move",
     "MESH_OT_polybuild_face_at_cursor.create_quads"},
};

/**
 * `_defs_edit_mesh.poly_build.description` (`space_toolsystem_toolbar.py:774`).
 *
 * Los tres atajos que enseña son TODO el manejo de la herramienta: sin ellos el usuario
 * ve una etiqueta y nada mas. Y no hay red de seguridad: `poly_build` tampoco tiene `op`
 * del que heredar descripcion.
 *
 * Se buscan dentro del keymap de la propia herramienta, que es donde viven esas teclas y
 * donde el usuario puede haberlas cambiado. Los operadores van en su forma interna
 * (`MESH_OT_...`): el Python los escribe como `mesh....` y es el RNA quien los convierte
 * al vuelo, pero aqui no hay RNA por medio.
 */
static std::string description_poly_build(const bContext * /*C*/, const wmKeyMap *km)
{
  const wmKeyMapItem *kmi_add = nullptr;
  const wmKeyMapItem *kmi_extrude = nullptr;
  const wmKeyMapItem *kmi_delete = nullptr;
  if (km != nullptr) {
    /* La busqueda no toca el keymap, pero la API de ventanas no es const-correcta y el
     * contrato si lo es. */
    wmKeyMap *km_mut = const_cast<wmKeyMap *>(km);
    kmi_add = WM_key_event_operator_from_keymap(
        km_mut, "MESH_OT_polybuild_face_at_cursor_move", nullptr, EVT_TYPE_MASK_ALL, 0);
    kmi_extrude = WM_key_event_operator_from_keymap(
        km_mut, "MESH_OT_polybuild_extrude_at_cursor_move", nullptr, EVT_TYPE_MASK_ALL, 0);
    kmi_delete = WM_key_event_operator_from_keymap(
        km_mut, "MESH_OT_polybuild_delete_at_cursor", nullptr, EVT_TYPE_MASK_ALL, 0);
  }

  return fmt::format(
      fmt::runtime(TIP_("Use multiple operators in an interactive way to add, delete, or move "
                        "geometry\n"
                        " \u2022 {:s} - Add geometry by moving the cursor close to an element\n"
                        " \u2022 {:s} - Extrude edges by moving the cursor\n"
                        " \u2022 {:s} - Delete mesh element")),
      kmi_to_string_or_none(kmi_add),
      kmi_to_string_or_none(kmi_extrude),
      kmi_to_string_or_none(kmi_delete));
}


/* La descripcion la calcula el Python con los atajos que el usuario tenga puestos para
 * anadir, extruir y borrar. `description_fn` sigue a `nullptr` a proposito: el simbolo
 * esta declarado en la cabecera pero todavia sin definir, y apuntarlo aqui romperia el
 * enlazado de todo el que incluya el catalogo. Ver `poly_build_description`. */
const ToolDecl poly_build = {
    /*idname*/ "builtin.poly_build",
    /*label*/ N_("Poly Build"),
    /*description*/ nullptr,
    /*description_fn*/ description_poly_build,
    /*icon*/ "ops.mesh.polybuild_hover",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_mesh_preselect_elem",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Mesh, Poly Build",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(poly_build_settings),
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deslizar
 * \{ */

static const PropRow edge_slide_settings[] = {
    {PropSource::Operator, "transform.edge_slide", "correct_uv"},
};

const ToolDecl edge_slide = {
    /*idname*/ "builtin.edge_slide",
    /*label*/ N_("Edge Slide"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.transform.edge_slide",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_tool_generic_handle_normal",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Mesh, Edge Slide",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(edge_slide_settings),
};

/* El nombre de la funcion del Python es `vert_slide` pero el idname dice `vertex`; se
 * respetan los dos, cada uno donde estaba. */
static const PropRow vert_slide_settings[] = {
    {PropSource::Operator, "transform.vert_slide", "correct_uv"},
};

const ToolDecl vert_slide = {
    /*idname*/ "builtin.vertex_slide",
    /*label*/ N_("Vertex Slide"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.transform.vert_slide",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_tool_generic_handle_free",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Mesh, Vertex Slide",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(vert_slide_settings),
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Girar y meter hacia dentro
 * \{ */

/* Las dos primeras filas salen del operador y la tercera del grupo de gizmos: es la
 * unica herramienta de la malla que mezcla las dos fuentes en la misma cabecera. */
static const PropRow spin_settings[] = {
    {PropSource::Operator, "mesh.spin", "steps"},
    {PropSource::Operator, "mesh.spin", "dupli"},
    {PropSource::GizmoGroup, "MESH_GGT_spin", "axis", nullptr, PROP_ROW_EXPAND},
};

const ToolDecl spin = {
    /*idname*/ "builtin.spin",
    /*label*/ N_("Spin"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.mesh.spin",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "MESH_GGT_spin",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Mesh, Spin",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(spin_settings),
};

/* Unica herramienta de TODO el Python con `widget_properties`
 * (`space_toolsystem_toolbar.py:866`): arranca el asa generica con radio 75 y sin
 * relleno de fondo, para que no tape la cara mientras se mete el inset. El grupo de
 * gizmos es el del propio `gizmo_group`, asi que no hace falta repetirlo por fila. */
static const GizmoProp inset_gizmo_properties[] = {
    gizmo_number("radius", 75.0f),
    gizmo_number("backdrop_fill_alpha", 0.0f),
};

static const PropRow inset_settings[] = {
    {PropSource::Operator, "mesh.inset", "use_outset"},
    {PropSource::Operator, "mesh.inset", "use_individual"},
    {PropSource::Operator, "mesh.inset", "use_even_offset"},
    {PropSource::Operator, "mesh.inset", "use_relative_offset"},
};

const ToolDecl inset = {
    /*idname*/ "builtin.inset_faces",
    /*label*/ N_("Inset Faces"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.mesh.inset",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_tool_generic_handle_free",
    /*gizmo_properties*/ span(inset_gizmo_properties),
    /*keymap_name*/ "3D View Tool: Edit Mesh, Inset Faces",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(inset_settings),
};

/*
 * El bisel tiene los ajustes mas retorcidos del catalogo: reparte quince propiedades
 * entre la cabecera y el popover "extra" segun `region.type`, apaga media columna
 * cuando el bisel no es de aristas, ensena `spread` solo si el remate interior es un
 * arco y saca una plantilla de curva si el perfil es personalizado.
 *
 * Nada de eso cabe en filas, y traducirlo a ojo seria inventarse una interfaz distinta
 * de la que hay hoy. Va marcado como pendiente para que el verificador lo recuerde en
 * cada pasada en vez de quedar como "no tiene ajustes".
 */
const ToolDecl bevel = {
    /*idname*/ "builtin.bevel",
    /*label*/ N_("Bevel"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.mesh.bevel",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_tool_generic_handle_normal",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Mesh, Bevel",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ {},
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
    /*pending*/ TOOL_PENDING_SETTINGS,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Extruir
 * \{ */

/* Los ajustes son los de `_template_widget.VIEW3D_GGT_xform_extrude` (`:90`), que no es
 * una clase de herramientas sino un trozo de dibujo compartido; como es una sola fila,
 * se declara aqui en vez de darle nombre propio. */
static const PropRow extrude_settings[] = {
    {PropSource::GizmoGroup, "VIEW3D_GGT_xform_extrude", "axis_type", nullptr, PROP_ROW_EXPAND},
};

/* El operador NO es el que extruye por defecto sino el mismo que la tecla `E`, para que
 * la herramienta y el atajo hagan exactamente lo mismo. */
const ToolDecl extrude = {
    /*idname*/ "builtin.extrude_region",
    /*label*/ N_("Extrude Region"),
    /*description*/ N_("Extrude freely or along an axis"),
    /*description_fn*/ nullptr,
    /*icon*/ "ops.mesh.extrude_region_move",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_xform_extrude",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Mesh, Extrude Region",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ "view3d.edit_mesh_extrude_move_normal",
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(extrude_settings),
};

const ToolDecl extrude_manifold = {
    /*idname*/ "builtin.extrude_manifold",
    /*label*/ N_("Extrude Manifold"),
    /*description*/
    N_("Extrude, dissolves edges whose faces form a flat surface and intersect new edges"),
    /*description_fn*/ nullptr,
    /*icon*/ "ops.mesh.extrude_manifold",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_tool_generic_handle_normal",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Mesh, Extrude Manifold",
};

/* Tercer ajuste de sub-operador de macro: `props.TRANSFORM_OT_shrink_fatten`. */
static const PropRow extrude_normals_settings[] = {
    {PropSource::Operator,
     "mesh.extrude_region_shrink_fatten",
     "TRANSFORM_OT_shrink_fatten.use_even_offset"},
};

const ToolDecl extrude_normals = {
    /*idname*/ "builtin.extrude_along_normals",
    /*label*/ N_("Extrude Along Normals"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.mesh.extrude_region_shrink_fatten",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_tool_generic_handle_normal",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Mesh, Extrude Along Normals",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ "mesh.extrude_region_shrink_fatten",
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(extrude_normals_settings),
};

const ToolDecl extrude_individual = {
    /*idname*/ "builtin.extrude_individual",
    /*label*/ N_("Extrude Individual"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.mesh.extrude_faces_move",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_tool_generic_handle_normal",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Mesh, Extrude Individual",
};

/* Sin gizmo: se trabaja apuntando con el raton, y por eso es de las dos unicas de la
 * malla que cambian el cursor. */
static const PropRow extrude_cursor_settings[] = {
    {PropSource::Operator, "mesh.dupli_extrude_cursor", "rotate_source"},
};

const ToolDecl extrude_cursor = {
    /*idname*/ "builtin.extrude_to_cursor",
    /*label*/ N_("Extrude to Cursor"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.mesh.dupli_extrude_cursor",
    /*cursor*/ "CROSSHAIR",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Mesh, Extrude to Cursor",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(extrude_cursor_settings),
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cortes de bucle
 * \{ */

/* Las dos filas salen de DOS sub-operadores distintos del mismo macro: el corte pone
 * el numero de cortes y el deslizamiento posterior la correccion de UV. */
static const PropRow loopcut_slide_settings[] = {
    {PropSource::Operator, "mesh.loopcut_slide", "MESH_OT_loopcut.number_cuts"},
    {PropSource::Operator, "mesh.loopcut_slide", "TRANSFORM_OT_edge_slide.correct_uv"},
};

const ToolDecl loopcut_slide = {
    /*idname*/ "builtin.loop_cut",
    /*label*/ N_("Loop Cut"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.mesh.loopcut_slide",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_mesh_preselect_edgering",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Mesh, Loop Cut",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(loopcut_slide_settings),
};

const ToolDecl offset_edge_loops_slide = {
    /*idname*/ "builtin.offset_edge_loop_cut",
    /*label*/ N_("Offset Edge Loop Cut"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.mesh.offset_edge_loops_slide",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Mesh, Offset Edge Loop Cut",
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deformaciones
 * \{ */

static const PropRow vertex_smooth_settings[] = {
    {PropSource::Operator, "mesh.vertices_smooth", "repeat"},
};

const ToolDecl vertex_smooth = {
    /*idname*/ "builtin.smooth",
    /*label*/ N_("Smooth"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.mesh.vertices_smooth",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_tool_generic_handle_normal",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Mesh, Smooth",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(vertex_smooth_settings),
};

/* Ojo al icono: la de la malla usa `ops.transform.vertex_random` y la homonima de la
 * curva `ops.curve.vertex_random`. Son dos herramientas distintas con el mismo idname
 * en modos distintos, no una repetida. */
static const PropRow vertex_randomize_settings[] = {
    {PropSource::Operator, "transform.vertex_random", "uniform"},
    {PropSource::Operator, "transform.vertex_random", "normal"},
    {PropSource::Operator, "transform.vertex_random", "seed"},
};

const ToolDecl vertex_randomize = {
    /*idname*/ "builtin.randomize",
    /*label*/ N_("Randomize"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.transform.vertex_random",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_tool_generic_handle_normal",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Mesh, Randomize",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(vertex_randomize_settings),
};

/* La reutiliza la edicion de Grease Pencil (`:3472`), y alli sale con este mismo nombre
 * de keymap: el Python lo fija la primera vez que la registra, en la malla, y el otro
 * modo lo hereda. Por eso "Edit Mesh" tambien aparece en un modo que no es de malla. */
const ToolDecl tosphere = {
    /*idname*/ "builtin.to_sphere",
    /*label*/ N_("To Sphere"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.transform.tosphere",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Mesh, To Sphere",
};

static const PropRow shrink_fatten_settings[] = {
    {PropSource::Operator, "transform.shrink_fatten", "use_even_offset"},
};

const ToolDecl shrink_fatten = {
    /*idname*/ "builtin.shrink_fatten",
    /*label*/ N_("Shrink/Fatten"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.transform.shrink_fatten",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_tool_generic_handle_normal",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Mesh, Shrink/Fatten",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(shrink_fatten_settings),
};

const ToolDecl push_pull = {
    /*idname*/ "builtin.push_pull",
    /*label*/ N_("Push/Pull"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.transform.push_pull",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_tool_generic_handle_normal",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Mesh, Push/Pull",
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cortar
 * \{ */

/*
 * Los ajustes del cuchillo tambien miran el tipo de region: en la cabecera pinta tres
 * casillas y un popover, y fuera de ella pinta ademas las medidas y el ajuste angular.
 * Como el bisel, no cabe en filas y queda marcada como pendiente.
 *
 * Es la unica de la malla que puede usarse como herramienta de reserva.
 */
const ToolDecl knife = {
    /*idname*/ "builtin.knife",
    /*label*/ N_("Knife"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.mesh.knife_tool",
    /*cursor*/ "KNIFE",
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Mesh, Knife",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_KEYMAP_FALLBACK,
    /*settings*/ {},
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
    /*pending*/ TOOL_PENDING_SETTINGS,
};

static const PropRow bisect_settings[] = {
    {PropSource::Operator, "mesh.bisect", "use_fill"},
    {PropSource::Operator, "mesh.bisect", "clear_inner"},
    {PropSource::Operator, "mesh.bisect", "clear_outer"},
    {PropSource::Operator, "mesh.bisect", "threshold"},
};

const ToolDecl bisect = {
    /*idname*/ "builtin.bisect",
    /*label*/ N_("Bisect"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.mesh.bisect",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Edit Mesh, Bisect",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(bisect_settings),
};

/** \} */

}  // namespace defs_edit_mesh

}  // namespace flipendo::toolsystem
