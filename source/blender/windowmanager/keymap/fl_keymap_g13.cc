/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Mapa de teclado, grupo 13: modos de Grease Pencil que no son dibujo: escultura,
 * pintado de pesos y pintado de vertices, mas la herramienta de relleno.
 * Transliterado de blender_default.py.
 */

#include <string>

#include "FL_keymap_build.hpp"
#include "FL_keymap_params.hpp"

namespace flipendo::keymap {

/* -------------------------------------------------------------------- */
/** \name Plantillas que se repiten en todo el grupo
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
 * Aqui nunca hacen falta `rotation`, `secondary_rotation`, `color` ni `zoom`, que en
 * este grupo van siempre a su valor por defecto (falso).
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
      .string("data_path_secondary",
              secondary_prop ? (unified_path + "." + prop).c_str() : "")
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

/** `_template_paint_radial_control(paint)` del Python, sin las opciones extra. */
static void template_paint_radial_control(wmKeyMap *km, const char *paint)
{
  radial_control(km, ev("F", "PRESS"), paint, "size", "use_unified_size");
  radial_control(km, ev("F", "PRESS").shift(), paint, "strength", "use_unified_strength");
}

/** `_template_items_context_panel(menu, key_args_primary)` del Python. */
static void template_items_context_panel(wmKeyMap *km, const char *panel, const Event &primary)
{
  item_panel(km, panel, primary);
  item_panel(km, panel, ev("APP", "PRESS"));
}

/** `_template_asset_shelf_popup(asset_shelf, spacebar_action)` del Python. */
static void template_asset_shelf_popup(wmKeyMap *km,
                                       const char *asset_shelf,
                                       SpacebarAction spacebar_action)
{
  /* Con la barra espaciadora dedicada a la busqueda no queda tecla libre: el Python
   * devuelve una lista vacia. */
  if (spacebar_action == SpacebarAction::Search) {
    return;
  }

  Event event = (spacebar_action == SpacebarAction::Play) ? ev("SPACE", "PRESS").shift() :
                                                            ev("SPACE", "PRESS");
  item(km, "wm.call_asset_shelf_popover", event).string("name", asset_shelf);
}

/** `_template_items_hide_reveal_actions(op_hide, op_reveal)` del Python. */
static void template_items_hide_reveal_actions(wmKeyMap *km,
                                               const char *op_hide,
                                               const char *op_reveal)
{
  item(km, op_reveal, ev("H", "PRESS").alt());
  item(km, op_hide, ev("H", "PRESS")).boolean("unselected", false);
  item(km, op_hide, ev("H", "PRESS").shift()).boolean("unselected", true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Grease Pencil: modo escultura
 * \{ */

static void km_grease_pencil_sculpt_mode(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Grease Pencil Sculpt Mode", "EMPTY", "WINDOW");

  item(km, "brush.scale_size", ev("LEFT_BRACKET", "PRESS").repeat()).number("scalar", 0.9f);
  item(km, "brush.scale_size", ev("RIGHT_BRACKET", "PRESS").repeat())
      .number("scalar", 1.0f / 0.9f);

  /* Invoca el operador de escultura. */
  item(km, "grease_pencil.sculpt_paint", ev("LEFTMOUSE", "PRESS"));
  item(km, "grease_pencil.sculpt_paint", ev("LEFTMOUSE", "PRESS").ctrl())
      .enum_("mode", "INVERT");
  item(km, "grease_pencil.sculpt_paint", ev("LEFTMOUSE", "PRESS").shift())
      .enum_("mode", "SMOOTH");

  /* Modo de seleccion. */
  item(km, "wm.context_toggle", ev("ONE", "PRESS"))
      .string("data_path", "scene.tool_settings.use_gpencil_select_mask_point");
  item(km, "wm.context_toggle", ev("TWO", "PRESS"))
      .string("data_path", "scene.tool_settings.use_gpencil_select_mask_stroke");
  item(km, "wm.context_toggle", ev("THREE", "PRESS"))
      .string("data_path", "scene.tool_settings.use_gpencil_select_mask_segment");

  /* Superposicion de lineas de edicion. */
  item(km, "wm.context_toggle", ev("Q", "PRESS").shift())
      .string("data_path", "space_data.overlay.use_gpencil_edit_lines");
  item(km, "wm.context_toggle", ev("Q", "PRESS").shift().alt())
      .string("data_path", "space_data.overlay.use_gpencil_multiedit_line_only");

  /* Menu de fotogramas clave. */
  item_menu(km, "VIEW3D_MT_edit_greasepencil_animation", ev("I", "PRESS"));

  /* Insertar fotograma clave vacio. */
  item(km, "grease_pencil.insert_blank_frame", ev("I", "PRESS").shift());

  /* Menu de borrado de animacion. */
  item_menu(km, "GREASE_PENCIL_MT_draw_delete", ev("I", "PRESS").alt());

  /* Borrar todos los fotogramas activos. */
  item(km, "grease_pencil.delete_frame", ev("DEL", "PRESS").shift())
      .enum_("type", "ALL_FRAMES");

  /* Fusionar con la capa de abajo. */
  item(km, "grease_pencil.layer_merge", ev("M", "PRESS").ctrl().shift())
      .enum_("mode", "ACTIVE");

  /* Copiar y pegar. */
  item(km, "grease_pencil.copy", ev("C", "PRESS").ctrl());
  item(km, "grease_pencil.paste", ev("V", "PRESS").ctrl());
  item(km, "grease_pencil.paste", ev("V", "PRESS").shift().ctrl())
      .boolean("paste_back", true);

  /* Material activo. */
  item_menu(km, "VIEW3D_MT_greasepencil_material_active", ev("U", "PRESS"));

  /* Capa activa. */
  item_menu(km, "GREASE_PENCIL_MT_layer_active", ev("Y", "PRESS"));

  /* Menu radial de auto-enmascarado. */
  item_menu_pie(km,
                "VIEW3D_MT_grease_pencil_sculpt_automasking_pie",
                ev("A", "PRESS").shift().alt());

  template_paint_radial_control(km, "gpencil_sculpt_paint");
  template_asset_shelf_popup(km, "VIEW3D_AST_brush_gpencil_sculpt", params.spacebar_action);
  template_items_context_panel(
      km, "VIEW3D_PT_greasepencil_sculpt_context_menu", params.context_menu_event);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Grease Pencil: pintado de pesos
 *
 * Este keymap cae hacia "Pose" cuando hay un esqueleto seleccionado que deforma el
 * objeto de Grease Pencil, asi que al tocarlo hay que vigilar no pisar las
 * operaciones de pose (transformar huesos, por ejemplo).
 * \{ */

static void km_grease_pencil_weight_paint(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Grease Pencil Weight Paint", "EMPTY", "WINDOW");

  /* Pintar pesos. */
  item(km, "grease_pencil.weight_brush_stroke", ev("LEFTMOUSE", "PRESS"));
  item(km, "grease_pencil.weight_brush_stroke", ev("LEFTMOUSE", "PRESS").ctrl())
      .enum_("mode", "INVERT");

  /* Aumentar y reducir el tamano del pincel. */
  item(km, "brush.scale_size", ev("LEFT_BRACKET", "PRESS").repeat()).number("scalar", 0.9f);
  item(km, "brush.scale_size", ev("RIGHT_BRACKET", "PRESS").repeat())
      .number("scalar", 1.0f / 0.9f);

  /* Controles radiales. */
  template_paint_radial_control(km, "gpencil_weight_paint");
  radial_control(km,
                 ev("F", "PRESS").ctrl(),
                 "gpencil_weight_paint",
                 "weight",
                 "use_unified_weight");

  /* Alternar sumar/restar en la herramienta de dibujo de pesos. */
  item(km, "grease_pencil.weight_toggle_direction", ev("D", "PRESS"));

  /* Superposicion de lineas de edicion. */
  item(km, "wm.context_toggle", ev("Q", "PRESS").shift())
      .string("data_path", "space_data.overlay.use_gpencil_edit_lines");
  item(km, "wm.context_toggle", ev("Q", "PRESS").shift().alt())
      .string("data_path", "space_data.overlay.use_gpencil_multiedit_line_only");

  /* Capa activa. */
  item_menu(km, "GREASE_PENCIL_MT_layer_active", ev("Y", "PRESS"));

  /* Fusionar con la capa de abajo. */
  item(km, "grease_pencil.layer_merge", ev("M", "PRESS").ctrl().shift())
      .enum_("mode", "ACTIVE");

  /* Menu de fotogramas clave. */
  item_menu(km, "VIEW3D_MT_edit_greasepencil_animation", ev("I", "PRESS"));

  /* Insertar fotograma clave vacio. */
  item(km, "grease_pencil.insert_blank_frame", ev("I", "PRESS").shift());

  /* Menu de borrado de animacion. */
  item_menu(km, "GREASE_PENCIL_MT_draw_delete", ev("I", "PRESS").alt());

  /* Borrar todos los fotogramas activos. */
  item(km, "grease_pencil.delete_frame", ev("DEL", "PRESS").shift())
      .enum_("type", "ALL_FRAMES");

  /* Muestrear el peso bajo el cursor. */
  item(km, "grease_pencil.weight_sample", ev("X", "PRESS").shift());

  /* Menu contextual. */
  template_items_context_panel(
      km, "VIEW3D_PT_greasepencil_weight_context_menu", params.context_menu_event);

  /* Mostrar y ocultar capas. */
  template_items_hide_reveal_actions(km, "grease_pencil.layer_hide", "grease_pencil.layer_reveal");

  template_asset_shelf_popup(km, "VIEW3D_AST_brush_gpencil_weight", params.spacebar_action);

  if (!params.select_mouse_right) {
    /* Seleccion de huesos cuando se combina pintado de pesos con modo pose (Alt). */
    item(km, "view3d.select", ev("LEFTMOUSE", "PRESS").alt());
    item(km, "view3d.select", ev("LEFTMOUSE", "PRESS").shift().alt()).boolean("toggle", true);

    /* Ctrl-Shift-LMB hace falta para la emulacion del boton central, que choca con
     * Alt. Va bien en modo pose, donde suele bastar con seleccionar un hueso; para
     * seleccionar caras o vertices no sirve de mucho y hacen falta las herramientas
     * de seleccion. */
    item(km, "view3d.select", ev("LEFTMOUSE", "PRESS").ctrl().shift());
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Grease Pencil: pintado de vertices
 * \{ */

static void km_grease_pencil_vertex_paint(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Grease Pencil Vertex Paint", "EMPTY", "WINDOW");

  /* Pintar vertices. */
  item(km, "grease_pencil.vertex_brush_stroke", ev("LEFTMOUSE", "PRESS"));
  item(km, "grease_pencil.vertex_brush_stroke", ev("LEFTMOUSE", "PRESS").ctrl())
      .enum_("mode", "INVERT");

  /* Aumentar y reducir el tamano del pincel. */
  item(km, "brush.scale_size", ev("LEFT_BRACKET", "PRESS").repeat()).number("scalar", 0.9f);
  item(km, "brush.scale_size", ev("RIGHT_BRACKET", "PRESS").repeat())
      .number("scalar", 1.0f / 0.9f);

  /* Modo de seleccion. */
  item(km, "wm.context_toggle", ev("ONE", "PRESS"))
      .string("data_path", "scene.tool_settings.use_gpencil_vertex_select_mask_point");
  item(km, "wm.context_toggle", ev("TWO", "PRESS"))
      .string("data_path", "scene.tool_settings.use_gpencil_vertex_select_mask_stroke");
  item(km, "wm.context_toggle", ev("THREE", "PRESS"))
      .string("data_path", "scene.tool_settings.use_gpencil_vertex_select_mask_segment");

  /* Intercambiar color primario y secundario. */
  item(km, "paint.brush_colors_flip", ev("X", "PRESS"));

  /* Superposicion de lineas de edicion. */
  item(km, "wm.context_toggle", ev("Q", "PRESS").shift())
      .string("data_path", "space_data.overlay.use_gpencil_edit_lines");
  item(km, "wm.context_toggle", ev("Q", "PRESS").shift().alt())
      .string("data_path", "space_data.overlay.use_gpencil_multiedit_line_only");

  /* Capa activa. */
  item_menu(km, "GREASE_PENCIL_MT_layer_active", ev("Y", "PRESS"));

  /* Fusionar con la capa de abajo. */
  item(km, "grease_pencil.layer_merge", ev("M", "PRESS").ctrl().shift())
      .enum_("mode", "ACTIVE");

  /* Menu de fotogramas clave. */
  item_menu(km, "VIEW3D_MT_edit_greasepencil_animation", ev("I", "PRESS"));

  /* Insertar fotograma clave vacio. */
  item(km, "grease_pencil.insert_blank_frame", ev("I", "PRESS").shift());

  /* Menu de borrado de animacion. */
  item_menu(km, "GREASE_PENCIL_MT_draw_delete", ev("I", "PRESS").alt());

  /* Borrar todos los fotogramas activos. */
  item(km, "grease_pencil.delete_frame", ev("DEL", "PRESS").shift())
      .enum_("type", "ALL_FRAMES");

  /* Controles radiales. */
  template_paint_radial_control(km, "gpencil_vertex_paint");

  /* Menu contextual. */
  template_items_context_panel(
      km, "VIEW3D_PT_greasepencil_vertex_paint_context_menu", params.context_menu_event);

  template_asset_shelf_popup(km, "VIEW3D_AST_brush_gpencil_vertex", params.spacebar_action);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Grease Pencil: herramienta de relleno
 * \{ */

static void km_grease_pencil_fill_tool(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap(kc, "Grease Pencil Fill Tool", "EMPTY", "WINDOW");

  /* Operador de relleno. */
  item(km, "grease_pencil.fill", ev("LEFTMOUSE", "PRESS"));
  item(km, "grease_pencil.fill", ev("LEFTMOUSE", "PRESS").ctrl()).boolean("invert", true);

  /* Con Alt se usa el trazo normal, para dibujar las guias del relleno. */
  item(km, "grease_pencil.brush_stroke", ev("LEFTMOUSE", "PRESS").alt());
}

/** \} */

void register_group_13(wmKeyConfig *kc, const Params &params)
{
  km_grease_pencil_sculpt_mode(kc, params);
  km_grease_pencil_weight_paint(kc, params);
  km_grease_pencil_vertex_paint(kc, params);
  km_grease_pencil_fill_tool(kc, params);
}

}  // namespace flipendo::keymap
