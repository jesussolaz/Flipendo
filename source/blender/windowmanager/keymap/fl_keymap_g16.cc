/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Mapa de teclado, grupo 16: la mascara de seleccion de vertices que comparten los
 * modos de pintado de vertices y de pesos, y los modos de escultura de malla
 * (Sculpt) y de curvas (Sculpt Curves).
 * Transliterado de blender_default.py.
 */

#include <string>

#include "FL_keymap_build.hpp"
#include "FL_keymap_params.hpp"

namespace flipendo::keymap {

/* `NUMBERS_0` del Python: la fila de numeros en orden NUMERICO, o sea el cero
 * primero. `NUMBERS_1` es la misma fila en su orden FISICO, con el cero al final. */
static const char *const NUMBERS_0[10] = {
    "ZERO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE"};
static const char *const NUMBERS_1[10] = {
    "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE", "ZERO"};

/* -------------------------------------------------------------------- */
/** \name Plantillas que se repiten en este grupo
 * \{ */

/**
 * `radial_control_properties(paint, prop, secondary_prop)` del Python.
 *
 * Las rutas RNA se componen con el nombre del modo de pintado, asi que hay que
 * construirlas en tiempo de ejecucion. Se escriben TODAS las propiedades, incluidas
 * las que quedan en cadena vacia, porque el Python tambien las asigna explicitamente
 * y el volcado compara propiedad a propiedad.
 *
 * `secondary_prop == nullptr` es el `None` del Python: sin valor unificado.
 * Aqui nunca hacen falta `secondary_rotation`, `color` ni `zoom`, que en las dos
 * llamadas de este grupo van a su valor por defecto (falso).
 */
static void radial_control(wmKeyMap *km,
                           const Event &event,
                           const char *paint,
                           const char *prop,
                           const char *secondary_prop)
{
  const std::string brush_path = std::string("tool_settings.") + paint + ".brush";
  const std::string unified_path = "tool_settings.unified_paint_settings";

  item(km, "wm.radial_control", event)
      .string("data_path_primary", (brush_path + "." + prop).c_str())
      .string("data_path_secondary", secondary_prop ? (unified_path + "." + prop).c_str() : "")
      .string("use_secondary",
              secondary_prop ? (unified_path + "." + secondary_prop).c_str() : "")
      .string("rotation_path", (brush_path + ".texture_slot.angle").c_str())
      .string("color_path", (brush_path + ".cursor_color_add").c_str())
      .string("fill_color_path", "")
      .string("fill_color_override_path", "")
      .string("fill_color_override_test_path", "")
      .string("zoom_path", "")
      .string("image_id", brush_path.c_str())
      .boolean("secondary_tex", false);
}

/** `_template_paint_radial_control(paint, rotation=...)` del Python. */
static void template_paint_radial_control(wmKeyMap *km, const char *paint, const bool rotation)
{
  radial_control(km, ev("F", "PRESS"), paint, "size", "use_unified_size");
  radial_control(km, ev("F", "PRESS").shift(), paint, "strength", "use_unified_strength");

  if (rotation) {
    radial_control(km, ev("F", "PRESS").ctrl(), paint, "texture_slot.angle", nullptr);
  }
}

/** `_template_items_select_actions(params, operator)` del Python. */
static void template_items_select_actions(wmKeyMap *km, const Params &params, const char *op)
{
  if (!params.use_select_all_toggle) {
    item(km, op, ev("A", "PRESS")).enum_("action", "SELECT");
    item(km, op, ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, op, ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
    item(km, op, ev("A", "DOUBLE_CLICK")).enum_("action", "DESELECT");
  }
  else if (params.legacy) {
    /* En el keymap heredado Alt+A es la reproduccion, asi que ahi no hay "deseleccionar". */
    item(km, op, ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, op, ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }
  else {
    item(km, op, ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, op, ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, op, ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }
}

/** `_template_asset_shelf_popup(asset_shelf, spacebar_action)` del Python. */
static void template_asset_shelf_popup(wmKeyMap *km,
                                       const char *asset_shelf,
                                       const SpacebarAction spacebar_action)
{
  /* Con la barra espaciadora dedicada a la busqueda no queda tecla libre: el Python
   * devuelve una lista vacia. */
  if (spacebar_action == SpacebarAction::Search) {
    return;
  }

  const Event event = (spacebar_action == SpacebarAction::Play) ? ev("SPACE", "PRESS").shift() :
                                                                 ev("SPACE", "PRESS");
  item(km, "wm.call_asset_shelf_popover", event).string("name", asset_shelf);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mascara de seleccion de vertices (pintado de vertices y de pesos)
 * \{ */

static void km_paint_vertex_mask(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Paint Vertex Selection (Weight, Vertex)", "EMPTY", "WINDOW");

  template_items_select_actions(km, params, "paint.vert_select_all");

  /* `_template_items_select_lasso(params, "view3d.select_lasso")`. Con seleccion por
   * boton derecho, Ctrl+LMB ya esta cogido, asi que "anadir" se pide con todos los
   * modificadores a la vez. */
  if (params.select_mouse_right) {
    item(km, "view3d.select_lasso", ev(params.action_mouse, "CLICK_DRAG").shift().ctrl())
        .enum_("mode", "SUB");
    item(km, "view3d.select_lasso", ev(params.action_mouse, "CLICK_DRAG").shift().ctrl().alt())
        .enum_("mode", "ADD");
  }
  else {
    item(km, "view3d.select_lasso", ev(params.action_mouse, "CLICK_DRAG").shift().ctrl())
        .enum_("mode", "SUB");
    item(km, "view3d.select_lasso", ev(params.action_mouse, "CLICK_DRAG").ctrl())
        .enum_("mode", "ADD");
  }

  /* `_template_items_hide_reveal_actions("paint.vert_select_hide", "paint.face_vert_reveal")`. */
  item(km, "paint.face_vert_reveal", ev("H", "PRESS").alt());
  item(km, "paint.vert_select_hide", ev("H", "PRESS")).boolean("unselected", false);
  item(km, "paint.vert_select_hide", ev("H", "PRESS").shift()).boolean("unselected", true);

  item(km, "view3d.select_box", ev("B", "PRESS"));
  item(km, "view3d.select_circle", ev("C", "PRESS"));
  item(km, "paint.vert_select_linked", ev("L", "PRESS").ctrl());
  item(km, "paint.vert_select_linked_pick", ev("L", "PRESS")).boolean("select", true);
  item(km, "paint.vert_select_linked_pick", ev("L", "PRESS").shift()).boolean("select", false);
  item(km, "paint.vert_select_more", ev("NUMPAD_PLUS", "PRESS").ctrl());
  item(km, "paint.vert_select_less", ev("NUMPAD_MINUS", "PRESS").ctrl());

  /* Nota del Python original: aqui faltaria `_template_view3d_paint_mask_select_loop`
   * si algun dia hay seleccion por bucle, como en `km_paint_face_mask`. */
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Escultura de malla (Sculpt)
 * \{ */

static void km_sculpt(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Sculpt", "EMPTY", "WINDOW");

  /* Trazos de pincel. */
  item(km, "sculpt.brush_stroke", ev("LEFTMOUSE", "PRESS"));
  item(km, "sculpt.brush_stroke", ev("LEFTMOUSE", "PRESS").ctrl()).enum_("mode", "INVERT");
  item(km, "sculpt.brush_stroke", ev("LEFTMOUSE", "PRESS").shift()).enum_("mode", "SMOOTH");

  /* Expandir. */
  item(km, "sculpt.expand", ev("A", "PRESS").shift())
      .enum_("target", "MASK")
      .enum_("falloff_type", "GEODESIC")
      .boolean("invert", false)
      .boolean("use_auto_mask", false)
      .boolean("use_mask_preserve", true);
  item(km, "sculpt.expand", ev("A", "PRESS").shift().alt())
      .enum_("target", "MASK")
      .enum_("falloff_type", "NORMALS")
      .boolean("invert", false)
      .boolean("use_mask_preserve", true);
  item(km, "sculpt.expand", ev("W", "PRESS").shift())
      .enum_("target", "FACE_SETS")
      .enum_("falloff_type", "GEODESIC")
      .boolean("invert", false)
      .boolean("use_mask_preserve", false)
      .boolean("use_modify_active", false);
  item(km, "sculpt.expand", ev("W", "PRESS").shift().alt())
      .enum_("target", "FACE_SETS")
      .enum_("falloff_type", "BOUNDARY_FACE_SET")
      .boolean("invert", false)
      .boolean("use_mask_preserve", false)
      .boolean("use_modify_active", true);

  /* Visibilidad parcial (mostrar/ocultar). Repite las teclas de
   * `_template_items_hide_reveal_actions` pero no puede usarla: los operadores no
   * tienen las mismas propiedades. */
  item(km, "sculpt.face_set_change_visibility", ev("H", "PRESS").shift())
      .enum_("mode", "TOGGLE");
  item(km, "sculpt.face_set_change_visibility", ev("H", "PRESS")).enum_("mode", "HIDE_ACTIVE");
  item(km, "paint.hide_show_all", ev("H", "PRESS").alt()).enum_("action", "SHOW");
  item(km, "paint.visibility_filter", ev("PAGE_UP", "PRESS").repeat()).enum_("action", "GROW");
  item(km, "paint.visibility_filter", ev("PAGE_DOWN", "PRESS").repeat())
      .enum_("action", "SHRINK");
  item(km, "sculpt.face_set_edit", ev("W", "PRESS").ctrl()).enum_("mode", "GROW");
  item(km, "sculpt.face_set_edit", ev("W", "PRESS").ctrl().alt()).enum_("mode", "SHRINK");

  /* Niveles de subdivision. `_template_items_object_subdivision_set()`: el nivel es el
   * numero de la tecla, por eso va con `NUMBERS_0` (cero primero). */
  for (int i = 0; i < 6; i++) {
    item(km, "object.subdivision_set", ev(NUMBERS_0[i], "PRESS").ctrl())
        .integer("level", i)
        .boolean("relative", false);
  }
  item(km, "object.subdivision_set", ev("ONE", "PRESS").alt().repeat())
      .integer("level", -1)
      .boolean("relative", true);
  item(km, "object.subdivision_set", ev("TWO", "PRESS").alt().repeat())
      .integer("level", 1)
      .boolean("relative", true);

  /* Mascara. */
  item(km, "paint.mask_flood_fill", ev("M", "PRESS").alt())
      .enum_("mode", "VALUE")
      .number("value", 0.0f);
  item(km, "paint.mask_flood_fill", ev("I", "PRESS").ctrl()).enum_("mode", "INVERT");
  item(km, "paint.mask_box_gesture", ev("B", "PRESS"))
      .enum_("mode", "VALUE")
      .number("value", 0.0f);

  /* Topologia dinamica. */
  item(km, "sculpt.dyntopo_detail_size_edit", ev("R", "PRESS"));
  item(km, "sculpt.detail_flood_fill", ev("R", "PRESS").ctrl());

  /* Remallado. */
  item(km, "object.voxel_remesh", ev("R", "PRESS").ctrl());
  item(km, "object.voxel_size_edit", ev("R", "PRESS"));

  /* Color. */
  item(km, "sculpt.sample_color", ev("X", "PRESS").shift());
  item(km, "paint.brush_colors_flip", ev("X", "PRESS"));

  /* Propiedades del pincel. */
  item(km, "brush.scale_size", ev("LEFT_BRACKET", "PRESS").repeat()).number("scalar", 0.9f);
  item(km, "brush.scale_size", ev("RIGHT_BRACKET", "PRESS").repeat())
      .number("scalar", 1.0f / 0.9f);
  template_paint_radial_control(km, "sculpt", true);

  /* Estarcido (stencil). */
  item(km, "brush.stencil_control", ev("RIGHTMOUSE", "PRESS")).enum_("mode", "TRANSLATION");
  item(km, "brush.stencil_control", ev("RIGHTMOUSE", "PRESS").shift()).enum_("mode", "SCALE");
  item(km, "brush.stencil_control", ev("RIGHTMOUSE", "PRESS").ctrl()).enum_("mode", "ROTATION");
  item(km, "brush.stencil_control", ev("RIGHTMOUSE", "PRESS").alt())
      .enum_("mode", "TRANSLATION")
      .enum_("texmode", "SECONDARY");
  item(km, "brush.stencil_control", ev("RIGHTMOUSE", "PRESS").shift().alt())
      .enum_("mode", "SCALE")
      .enum_("texmode", "SECONDARY");
  item(km, "brush.stencil_control", ev("RIGHTMOUSE", "PRESS").ctrl().alt())
      .enum_("mode", "ROTATION")
      .enum_("texmode", "SECONDARY");

  /* Punto de pivote de la sesion de escultura. */
  item(km, "sculpt.set_pivot_position", ev("RIGHTMOUSE", "PRESS").shift())
      .enum_("mode", "SURFACE");

  /* Menus. */
  item(km, "wm.context_menu_enum", ev("E", "PRESS").alt())
      .string("data_path", "tool_settings.sculpt.brush.stroke_method");
  item(km, "wm.context_toggle", ev("S", "PRESS").shift())
      .string("data_path", "tool_settings.sculpt.brush.use_smooth_stroke");
  item_menu_pie(km, "VIEW3D_MT_sculpt_mask_edit_pie", ev("A", "PRESS"));
  item_menu_pie(km, "VIEW3D_MT_sculpt_automasking_pie", ev("A", "PRESS").alt());
  item_menu_pie(km, "VIEW3D_MT_sculpt_face_sets_edit_pie", ev("W", "PRESS").alt());

  /* `_template_items_context_panel("VIEW3D_PT_sculpt_context_menu", params.context_menu_event)`. */
  item_panel(km, "VIEW3D_PT_sculpt_context_menu", params.context_menu_event);
  item_panel(km, "VIEW3D_PT_sculpt_context_menu", ev("APP", "PRESS"));

  /* Pinceles. */
  item(km, "brush.asset_activate", ev("V", "PRESS"))
      .enum_("asset_library_type", "ESSENTIALS")
      .string("relative_asset_identifier",
              "brushes/essentials_brushes-mesh_sculpt.blend/Brush/Draw");
  item(km, "brush.asset_activate", ev("S", "PRESS"))
      .enum_("asset_library_type", "ESSENTIALS")
      .string("relative_asset_identifier",
              "brushes/essentials_brushes-mesh_sculpt.blend/Brush/Smooth");
  item(km, "brush.asset_activate", ev("P", "PRESS"))
      .enum_("asset_library_type", "ESSENTIALS")
      .string("relative_asset_identifier",
              "brushes/essentials_brushes-mesh_sculpt.blend/Brush/Pinch/Magnify");
  item(km, "brush.asset_activate", ev("I", "PRESS"))
      .enum_("asset_library_type", "ESSENTIALS")
      .string("relative_asset_identifier",
              "brushes/essentials_brushes-mesh_sculpt.blend/Brush/Inflate/Deflate");
  item(km, "brush.asset_activate", ev("G", "PRESS"))
      .enum_("asset_library_type", "ESSENTIALS")
      .string("relative_asset_identifier",
              "brushes/essentials_brushes-mesh_sculpt.blend/Brush/Grab");
  item(km, "brush.asset_activate", ev("T", "PRESS").shift())
      .enum_("asset_library_type", "ESSENTIALS")
      .string("relative_asset_identifier",
              "brushes/essentials_brushes-mesh_sculpt.blend/Brush/Scrape/Fill");
  item(km, "brush.asset_activate", ev("C", "PRESS"))
      .enum_("asset_library_type", "ESSENTIALS")
      .string("relative_asset_identifier",
              "brushes/essentials_brushes-mesh_sculpt.blend/Brush/Clay Strips");
  item(km, "brush.asset_activate", ev("C", "PRESS").shift())
      .enum_("asset_library_type", "ESSENTIALS")
      .string("relative_asset_identifier",
              "brushes/essentials_brushes-mesh_sculpt.blend/Brush/Crease Polish");
  item(km, "brush.asset_activate", ev("K", "PRESS"))
      .enum_("asset_library_type", "ESSENTIALS")
      .string("relative_asset_identifier",
              "brushes/essentials_brushes-mesh_sculpt.blend/Brush/Snake Hook");
  item(km, "brush.asset_activate", ev("M", "PRESS"))
      .enum_("asset_library_type", "ESSENTIALS")
      .string("relative_asset_identifier",
              "brushes/essentials_brushes-mesh_sculpt.blend/Brush/Mask")
      .boolean("use_toggle", true);

  template_asset_shelf_popup(km, "VIEW3D_AST_brush_sculpt", params.spacebar_action);

  /* Mascara por lazo. Hace falta el caso aparte porque con seleccion por boton
   * derecho Ctrl+LMB ya esta cogido: se usan todos los modificadores a la vez para
   * desenmascarar (el equivalente a seleccionar). */
  if (params.select_mouse_right) {
    item(km, "paint.mask_lasso_gesture", ev("LEFTMOUSE", "PRESS").shift().ctrl())
        .number("value", 1.0f);
    item(km, "paint.mask_lasso_gesture", ev("LEFTMOUSE", "PRESS").shift().ctrl().alt())
        .number("value", 0.0f);
  }
  else {
    item(km, "paint.mask_lasso_gesture", ev("RIGHTMOUSE", "PRESS").shift().ctrl())
        .number("value", 1.0f);
    item(km, "paint.mask_lasso_gesture", ev("RIGHTMOUSE", "PRESS").ctrl())
        .number("value", 0.0f);
  }

  if (params.legacy) {
    /* `_template_items_legacy_tools_from_numbers()`: veinte herramientas, la fila de
     * numeros sola y con Mayus. */
    for (int i = 0; i < 20; i++) {
      Event event = ev(NUMBERS_1[i % 10], "PRESS");
      if (i >= 10) {
        event.shift();
      }
      item(km, "wm.tool_set_by_index", event).integer("index", i);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Escultura de curvas (Sculpt Curves)
 * \{ */

static void km_sculpt_curves(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Sculpt Curves", "EMPTY", "WINDOW");

  item(km, "sculpt_curves.brush_stroke", ev("LEFTMOUSE", "PRESS"));
  item(km, "sculpt_curves.brush_stroke", ev("LEFTMOUSE", "PRESS").ctrl())
      .enum_("mode", "INVERT");
  item(km, "sculpt_curves.brush_stroke", ev("LEFTMOUSE", "PRESS").shift())
      .enum_("mode", "SMOOTH");
  item(km, "curves.set_selection_domain", ev("ONE", "PRESS")).enum_("domain", "POINT");
  item(km, "curves.set_selection_domain", ev("TWO", "PRESS")).enum_("domain", "CURVE");

  template_paint_radial_control(km, "curves_sculpt", false);

  item(km, "brush.scale_size", ev("LEFT_BRACKET", "PRESS").repeat()).number("scalar", 0.9f);
  item(km, "brush.scale_size", ev("RIGHT_BRACKET", "PRESS").repeat())
      .number("scalar", 1.0f / 0.9f);

  template_items_select_actions(km, params, "curves.select_all");

  item(km, "sculpt_curves.min_distance_edit", ev("R", "PRESS"));
  item(km, "sculpt_curves.select_grow", ev("A", "PRESS").shift());

  template_asset_shelf_popup(km, "VIEW3D_AST_brush_sculpt_curves", params.spacebar_action);
}

/** \} */

void register_group_16(wmKeyConfig *kc, const Params &params)
{
  km_paint_vertex_mask(kc, params);
  km_sculpt(kc, params);
  km_sculpt_curves(kc, params);
}

}  // namespace flipendo::keymap
