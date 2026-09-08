/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Mapa de teclado, grupo 26: herramientas de la vista 3D en modo escultura (ocultar
 * por lazo/linea/polilinea, conjuntos de caras, recortes, filtros de malla, tela y
 * color, mascara por color y edicion de conjuntos de caras), herramientas de pintado
 * de pesos, el recorte de Grease Pencil, el mapa modal de la herramienta de
 * primitivas y la primitiva de linea de Grease Pencil.
 * Transliterado de blender_default.py.
 */

#include "FL_keymap_build.hpp"
#include "FL_keymap_params.hpp"

namespace flipendo::keymap {

/* -------------------------------------------------------------------- */
/** \name Herramientas de escultura: ocultar y mostrar
 * \{ */

static void km_3d_view_tool_sculpt_lasso_hide(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Sculpt, Lasso Hide", "VIEW_3D", "WINDOW");

  item(km, "paint.hide_show_lasso_gesture", params.tool_maybe_tweak_event)
      .enum_("action", "HIDE");
  /* `{**params.tool_maybe_tweak_event, "ctrl": True}`: se copia el evento de la
   * preferencia y se le anade el modificador, para no perder su tipo ni su valor. */
  item(km, "paint.hide_show_lasso_gesture", Event(params.tool_maybe_tweak_event).ctrl())
      .enum_("action", "SHOW");
  item(km, "paint.hide_show_all", ev(params.select_mouse, params.select_mouse_value))
      .enum_("action", "SHOW");
}

static void km_3d_view_tool_sculpt_line_hide(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Sculpt, Line Hide", "VIEW_3D", "WINDOW");

  item(km, "paint.hide_show_line_gesture", params.tool_maybe_tweak_event)
      .enum_("action", "HIDE");
  item(km, "paint.hide_show_line_gesture", Event(params.tool_maybe_tweak_event).ctrl())
      .enum_("action", "SHOW");
  item(km, "paint.hide_show_all", ev(params.select_mouse, params.select_mouse_value))
      .enum_("action", "SHOW");
}

static void km_3d_view_tool_sculpt_polyline_hide(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Sculpt, Polyline Hide", "VIEW_3D", "WINDOW");

  item(km, "paint.hide_show_polyline_gesture", ev(params.tool_mouse, "PRESS"))
      .enum_("action", "HIDE");
  item(km, "paint.hide_show_polyline_gesture", ev(params.tool_mouse, "PRESS").ctrl())
      .enum_("action", "SHOW");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Herramientas de escultura: conjuntos de caras
 * \{ */

static void km_3d_view_tool_sculpt_box_face_set(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Sculpt, Box Face Set", "VIEW_3D", "WINDOW");

  item(km, "sculpt.face_set_box_gesture", params.tool_maybe_tweak_event);
}

static void km_3d_view_tool_sculpt_lasso_face_set(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Sculpt, Lasso Face Set", "VIEW_3D", "WINDOW");

  item(km, "sculpt.face_set_lasso_gesture", params.tool_maybe_tweak_event);
}

static void km_3d_view_tool_sculpt_line_face_set(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Sculpt, Line Face Set", "VIEW_3D", "WINDOW");

  item(km, "sculpt.face_set_line_gesture", params.tool_maybe_tweak_event);
}

static void km_3d_view_tool_sculpt_polyline_face_set(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Sculpt, Polyline Face Set", "VIEW_3D", "WINDOW");

  item(km, "sculpt.face_set_polyline_gesture", ev(params.tool_mouse, "PRESS"));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Herramientas de escultura: recorte y proyeccion
 * \{ */

static void km_3d_view_tool_sculpt_box_trim(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Sculpt, Box Trim", "VIEW_3D", "WINDOW");

  item(km, "sculpt.trim_box_gesture", params.tool_maybe_tweak_event);
}

static void km_3d_view_tool_sculpt_lasso_trim(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Sculpt, Lasso Trim", "VIEW_3D", "WINDOW");

  item(km, "sculpt.trim_lasso_gesture", params.tool_maybe_tweak_event);
}

static void km_3d_view_tool_sculpt_line_trim(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Sculpt, Line Trim", "VIEW_3D", "WINDOW");

  item(km, "sculpt.trim_line_gesture", params.tool_maybe_tweak_event);
}

static void km_3d_view_tool_sculpt_polyline_trim(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Sculpt, Polyline Trim", "VIEW_3D", "WINDOW");

  item(km, "sculpt.trim_polyline_gesture", ev(params.tool_mouse, "PRESS"));
}

static void km_3d_view_tool_sculpt_line_project(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Sculpt, Line Project", "VIEW_3D", "WINDOW");

  item(km, "sculpt.project_line_gesture", params.tool_maybe_tweak_event);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Herramientas de escultura: filtros y mascaras
 * \{ */

static void km_3d_view_tool_sculpt_mesh_filter(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Sculpt, Mesh Filter", "VIEW_3D", "WINDOW");

  item(km, "sculpt.mesh_filter", params.tool_maybe_tweak_event);
}

static void km_3d_view_tool_sculpt_cloth_filter(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Sculpt, Cloth Filter", "VIEW_3D", "WINDOW");

  item(km, "sculpt.cloth_filter", params.tool_maybe_tweak_event);
}

static void km_3d_view_tool_sculpt_color_filter(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Sculpt, Color Filter", "VIEW_3D", "WINDOW");

  item(km, "sculpt.color_filter", params.tool_maybe_tweak_event);
}

static void km_3d_view_tool_sculpt_mask_by_color(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Sculpt, Mask by Color", "VIEW_3D", "WINDOW");

  item(km, "sculpt.mask_by_color", ev(params.tool_mouse, "PRESS"));
}

static void km_3d_view_tool_sculpt_face_set_edit(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Sculpt, Face Set Edit", "VIEW_3D", "WINDOW");

  item(km, "sculpt.face_set_edit", ev(params.tool_mouse, "PRESS"));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sistema de herramientas (vista 3D, pintado de pesos)
 * \{ */

static void km_3d_view_tool_paint_weight_sample_weight(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Paint Weight, Sample Weight", "VIEW_3D", "WINDOW");

  item(km, "paint.weight_sample", ev(params.tool_mouse, "PRESS"));
  item(km, "grease_pencil.weight_sample", ev(params.tool_mouse, "PRESS"));
}

static void km_3d_view_tool_paint_weight_sample_vertex_group(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(
      kc, "3D View Tool: Paint Weight, Sample Vertex Group", "VIEW_3D", "WINDOW");

  item(km, "paint.weight_sample_group", ev(params.tool_mouse, "PRESS"));
}

static void km_3d_view_tool_paint_weight_gradient(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Paint Weight, Gradient", "VIEW_3D", "WINDOW");

  item(km, "paint.weight_gradient", params.tool_maybe_tweak_event);
}

static void km_3d_view_tool_paint_grease_pencil_trim(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Paint Grease Pencil, Trim", "VIEW_3D", "WINDOW");

  item(km, "grease_pencil.stroke_trim", ev(params.tool_mouse, "PRESS"));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sistema de herramientas (vista 3D, Grease Pencil, pintar)
 * \{ */

static void km_grease_pencil_primitive_tool_modal_map(wmKeyConfig *kc,
                                                      const Params & /*params*/)
{
  wmKeyMap *km = keymap_modal(kc, "Primitive Tool Modal Map");

  item_modal(km, "CANCEL", ev("ESC", "PRESS").any());
  item_modal(km, "CANCEL", ev("Q", "PRESS").any());
  item_modal(km, "PANNING", ev("MIDDLEMOUSE", "ANY").shift());
  item_modal(km, "CONFIRM", ev("RET", "PRESS").any());
  item_modal(km, "CONFIRM", ev("NUMPAD_ENTER", "PRESS").any());
  item_modal(km, "CONFIRM", ev("MIDDLEMOUSE", "PRESS"));
  item_modal(km, "EXTRUDE", ev("E", "PRESS"));
  item_modal(km, "GRAB", ev("G", "PRESS"));
  item_modal(km, "ROTATE", ev("R", "PRESS"));
  item_modal(km, "SCALE", ev("S", "PRESS"));
  item_modal(km, "INCREASE_SUBDIVISION", ev("UP_ARROW", "PRESS").repeat());
  item_modal(km, "DECREASE_SUBDIVISION", ev("DOWN_ARROW", "PRESS").repeat());
  item_modal(km, "CHANGE_RADIUS", ev("F", "PRESS"));
  item_modal(km, "CHANGE_OPACITY", ev("F", "PRESS").shift());
}

static void km_3d_view_tool_paint_grease_pencil_primitive_line(wmKeyConfig *kc,
                                                               const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Paint Grease Pencil, Line", "VIEW_3D", "WINDOW");

  /* Las tres variantes llevan `{"properties": []}` en el Python: reservan la lista de
   * propiedades pero no fijan ninguna, o sea que equivalen a no pasar propiedades. */
  item(km, "grease_pencil.primitive_line", ev("LEFTMOUSE", "PRESS"));
  item(km, "grease_pencil.primitive_line", ev("LEFTMOUSE", "PRESS").shift());
  item(km, "grease_pencil.primitive_line", ev("LEFTMOUSE", "PRESS").alt());
  /* Seleccion por lazo. */
  item(km, "grease_pencil.select_lasso", ev(params.action_mouse, "CLICK_DRAG").ctrl().alt());
}

/** \} */

void register_group_26(wmKeyConfig *kc, const Params &params)
{
  km_3d_view_tool_sculpt_lasso_hide(kc, params);
  km_3d_view_tool_sculpt_line_hide(kc, params);
  km_3d_view_tool_sculpt_polyline_hide(kc, params);
  km_3d_view_tool_sculpt_box_face_set(kc, params);
  km_3d_view_tool_sculpt_lasso_face_set(kc, params);
  km_3d_view_tool_sculpt_line_face_set(kc, params);
  km_3d_view_tool_sculpt_polyline_face_set(kc, params);
  km_3d_view_tool_sculpt_box_trim(kc, params);
  km_3d_view_tool_sculpt_lasso_trim(kc, params);
  km_3d_view_tool_sculpt_line_trim(kc, params);
  km_3d_view_tool_sculpt_polyline_trim(kc, params);
  km_3d_view_tool_sculpt_line_project(kc, params);
  km_3d_view_tool_sculpt_mesh_filter(kc, params);
  km_3d_view_tool_sculpt_cloth_filter(kc, params);
  km_3d_view_tool_sculpt_color_filter(kc, params);
  km_3d_view_tool_sculpt_mask_by_color(kc, params);
  km_3d_view_tool_sculpt_face_set_edit(kc, params);
  km_3d_view_tool_paint_weight_sample_weight(kc, params);
  km_3d_view_tool_paint_weight_sample_vertex_group(kc, params);
  km_3d_view_tool_paint_weight_gradient(kc, params);
  km_3d_view_tool_paint_grease_pencil_trim(kc, params);
  km_grease_pencil_primitive_tool_modal_map(kc, params);
  km_3d_view_tool_paint_grease_pencil_primitive_line(kc, params);
}

}  // namespace flipendo::keymap
