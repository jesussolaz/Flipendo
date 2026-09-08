/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Mapa de teclado, grupo 25: herramientas de la vista 3D — el final de las de edicion
 * de malla (aleatorizar, deslizar arista y vertice, encoger/engordar, empujar/tirar,
 * a esfera, rasgar region y arista), todas las de edicion de curva (dibujar, pluma,
 * inclinar, radio, aleatorizar, extruir y extruir al cursor) y el principio de las de
 * escultura (mascaras por caja, lazo, linea y polilinea, y ocultar por caja).
 * Transliterado de blender_default.py.
 */

#include "FL_keymap_build.hpp"
#include "FL_keymap_params.hpp"

namespace flipendo::keymap {

/* `{**params.tool_maybe_tweak_event, **params.tool_modifier}`, que es el evento con el
 * que se invoca casi toda herramienta de este grupo. Se hace funcion porque se repite
 * doce veces dentro del fichero, no porque el Python tenga nada parecido.
 *
 * TODO(keymap): `params.tool_modifier` vale `{"alt": -1}` (o sea, Alt pulsado o no)
 * cuando se selecciona con el boton izquierdo y `use_alt_tool_or_cursor` esta activo
 * (`Params.__init__`, blender_default.py:178). `Event` solo sabe poner TODOS los
 * modificadores en "cualquiera" (`.any()`), no uno solo, asi que ese caso no se aplica
 * y con esa preferencia la herramienta no respondera con Alt pulsado. Con los
 * parametros por defecto `tool_modifier` es `{}` y el resultado es exacto, por eso no
 * afecta al baseline. La bandera equivalente en C++ es `params.tool_modifier_alt_any`.
 */
static Event tool_maybe_tweak(const Params &params)
{
  return params.tool_maybe_tweak_event;
}

/* -------------------------------------------------------------------- */
/** \name Sistema de herramientas (vista 3D, edicion de malla)
 * \{ */

static void km_3d_view_tool_edit_mesh_randomize(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Mesh, Randomize", "VIEW_3D", "WINDOW");

  item(km, "transform.vertex_random", tool_maybe_tweak(params)).boolean("wait_for_input", false);
}

static void km_3d_view_tool_edit_mesh_edge_slide(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Mesh, Edge Slide", "VIEW_3D", "WINDOW");

  item(km, "transform.edge_slide", tool_maybe_tweak(params)).boolean("release_confirm", true);
}

static void km_3d_view_tool_edit_mesh_vertex_slide(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Mesh, Vertex Slide", "VIEW_3D", "WINDOW");

  item(km, "transform.vert_slide", tool_maybe_tweak(params)).boolean("release_confirm", true);
}

static void km_3d_view_tool_edit_mesh_shrink_fatten(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Mesh, Shrink/Fatten", "VIEW_3D", "WINDOW");

  item(km, "transform.shrink_fatten", tool_maybe_tweak(params)).boolean("release_confirm", true);
}

static void km_3d_view_tool_edit_mesh_push_pull(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Mesh, Push/Pull", "VIEW_3D", "WINDOW");

  item(km, "transform.push_pull", tool_maybe_tweak(params)).boolean("release_confirm", true);
}

static void km_3d_view_tool_edit_mesh_to_sphere(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Mesh, To Sphere", "VIEW_3D", "WINDOW");

  item(km, "transform.tosphere", tool_maybe_tweak(params)).boolean("release_confirm", true);
}

static void km_3d_view_tool_edit_mesh_rip_region(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Mesh, Rip Region", "VIEW_3D", "WINDOW");

  item(km, "mesh.rip_move", tool_maybe_tweak(params))
      .sub("TRANSFORM_OT_translate")
      .boolean("release_confirm", true);
}

static void km_3d_view_tool_edit_mesh_rip_edge(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Mesh, Rip Edge", "VIEW_3D", "WINDOW");

  item(km, "mesh.rip_edge_move", tool_maybe_tweak(params))
      .sub("TRANSFORM_OT_translate")
      .boolean("release_confirm", true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sistema de herramientas (vista 3D, edicion de curva)
 * \{ */

static void km_3d_view_tool_edit_curve_draw(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Curve, Draw", "VIEW_3D", "WINDOW");

  /* Sin `tool_modifier`: esta herramienta se queda con toda la entrada. */
  item(km, "curve.draw", ev(params.tool_mouse, "PRESS")).boolean("wait_for_input", false);
}

static void km_3d_view_tool_edit_curves_draw(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Curves, Draw", "VIEW_3D", "WINDOW");

  /* Sin `tool_modifier`: esta herramienta se queda con toda la entrada. */
  item(km, "curves.draw", ev(params.tool_mouse, "PRESS")).boolean("wait_for_input", false);
}

static void km_3d_view_tool_edit_curve_pen(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Curve, Curve Pen", "VIEW_3D", "WINDOW");

  item(km, "curve.pen", ev(params.tool_mouse, "PRESS"))
      .boolean("extrude_point", true)
      .boolean("move_segment", true)
      .boolean("select_point", true)
      .boolean("move_point", true)
      .enum_("close_spline_method", "ON_CLICK");
  item(km, "curve.pen", ev(params.tool_mouse, "PRESS").ctrl())
      .boolean("insert_point", true)
      .boolean("delete_point", true);
  item(km, "curve.pen", ev(params.tool_mouse, "DOUBLE_CLICK"))
      .boolean("toggle_vector", true)
      .boolean("cycle_handle_type", true);
}

static void km_3d_view_tool_edit_curve_tilt(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Curve, Tilt", "VIEW_3D", "WINDOW");

  item(km, "transform.tilt", tool_maybe_tweak(params)).boolean("release_confirm", true);
}

static void km_3d_view_tool_edit_curve_radius(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Curve, Radius", "VIEW_3D", "WINDOW");

  item(km, "transform.transform", tool_maybe_tweak(params))
      .enum_("mode", "CURVE_SHRINKFATTEN")
      .boolean("release_confirm", true);
}

static void km_3d_view_tool_edit_curve_randomize(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Curve, Randomize", "VIEW_3D", "WINDOW");

  item(km, "transform.vertex_random", tool_maybe_tweak(params)).boolean("wait_for_input", false);
}

static void km_3d_view_tool_edit_curve_extrude(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Curve, Extrude", "VIEW_3D", "WINDOW");

  item(km, "curve.extrude_move", tool_maybe_tweak(params))
      .sub("TRANSFORM_OT_translate")
      .boolean("release_confirm", true);
}

static void km_3d_view_tool_edit_curve_extrude_to_cursor(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Curve, Extrude to Cursor", "VIEW_3D", "WINDOW");

  /* Sin `tool_modifier`: esta herramienta se queda con toda la entrada. */
  item(km, "curve.vertex_add", ev(params.tool_mouse, "PRESS"));
  /* Arrastrar con el izquierdo tambien vale cuando se selecciona con el derecho. */
  if (params.select_mouse_right) {
    item(km, "transform.translate", ev(params.tool_mouse, "CLICK_DRAG"))
        .boolean("release_confirm", true);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sistema de herramientas (vista 3D, escultura)
 * \{ */

static void km_3d_view_tool_sculpt_box_mask(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Sculpt, Box Mask", "VIEW_3D", "WINDOW");

  item(km, "paint.mask_box_gesture", params.tool_maybe_tweak_event).number("value", 1.0f);
  item(km, "paint.mask_box_gesture", Event(params.tool_maybe_tweak_event).ctrl())
      .number("value", 0.0f);
}

static void km_3d_view_tool_sculpt_lasso_mask(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Sculpt, Lasso Mask", "VIEW_3D", "WINDOW");

  item(km, "paint.mask_lasso_gesture", params.tool_maybe_tweak_event).number("value", 1.0f);
  item(km, "paint.mask_lasso_gesture", Event(params.tool_maybe_tweak_event).ctrl())
      .number("value", 0.0f);
}

static void km_3d_view_tool_sculpt_line_mask(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Sculpt, Line Mask", "VIEW_3D", "WINDOW");

  item(km, "paint.mask_line_gesture", params.tool_maybe_tweak_event).number("value", 1.0f);
  item(km, "paint.mask_line_gesture", Event(params.tool_maybe_tweak_event).ctrl())
      .number("value", 0.0f);
}

static void km_3d_view_tool_sculpt_polyline_mask(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Sculpt, Polyline Mask", "VIEW_3D", "WINDOW");

  item(km, "paint.mask_polyline_gesture", ev(params.tool_mouse, "PRESS")).number("value", 1.0f);
  item(km, "paint.mask_polyline_gesture", ev(params.tool_mouse, "PRESS").ctrl())
      .number("value", 0.0f);
}

static void km_3d_view_tool_sculpt_box_hide(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Sculpt, Box Hide", "VIEW_3D", "WINDOW");

  item(km, "paint.hide_show", params.tool_maybe_tweak_event).enum_("action", "HIDE");
  item(km, "paint.hide_show", Event(params.tool_maybe_tweak_event).ctrl())
      .enum_("action", "SHOW");
  item(km, "paint.hide_show_all", ev(params.select_mouse, params.select_mouse_value))
      .enum_("action", "SHOW");
}

/** \} */

void register_group_25(wmKeyConfig *kc, const Params &params)
{
  km_3d_view_tool_edit_mesh_randomize(kc, params);
  km_3d_view_tool_edit_mesh_edge_slide(kc, params);
  km_3d_view_tool_edit_mesh_vertex_slide(kc, params);
  km_3d_view_tool_edit_mesh_shrink_fatten(kc, params);
  km_3d_view_tool_edit_mesh_push_pull(kc, params);
  km_3d_view_tool_edit_mesh_to_sphere(kc, params);
  km_3d_view_tool_edit_mesh_rip_region(kc, params);
  km_3d_view_tool_edit_mesh_rip_edge(kc, params);

  km_3d_view_tool_edit_curve_draw(kc, params);
  km_3d_view_tool_edit_curves_draw(kc, params);
  km_3d_view_tool_edit_curve_pen(kc, params);
  km_3d_view_tool_edit_curve_tilt(kc, params);
  km_3d_view_tool_edit_curve_radius(kc, params);
  km_3d_view_tool_edit_curve_randomize(kc, params);
  km_3d_view_tool_edit_curve_extrude(kc, params);
  km_3d_view_tool_edit_curve_extrude_to_cursor(kc, params);

  km_3d_view_tool_sculpt_box_mask(kc, params);
  km_3d_view_tool_sculpt_lasso_mask(kc, params);
  km_3d_view_tool_sculpt_line_mask(kc, params);
  km_3d_view_tool_sculpt_polyline_mask(kc, params);
  km_3d_view_tool_sculpt_box_hide(kc, params);
}

}  // namespace flipendo::keymap
