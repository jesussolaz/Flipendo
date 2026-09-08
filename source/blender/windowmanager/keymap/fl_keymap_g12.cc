/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Mapa de teclado, grupo 12: anotaciones (Grease Pencil "clasico") y el Grease Pencil
 * nuevo: seleccion, modo pintura, trazo de pincel y modo edicion.
 * Transliterado de blender_default.py.
 */

#include "FL_keymap_build.hpp"
#include "FL_keymap_params.hpp"

namespace flipendo::keymap {

/* `op_tool_cycle(tool, args)` del Python: activa la herramienta anadiendo `cycle`.
 * `item_tool` solo pone `name` (es el equivalente de `op_tool`), asi que el `cycle`
 * hay que ponerlo aparte o la tecla no ciclaria entre las variantes de la
 * herramienta. Se usa en todos los `op_tool_optional` de este grupo. */
static Item tool_cycle(wmKeyMap *km, const char *tool, const Event &event)
{
  return item_tool(km, tool, event).boolean("cycle", true);
}

/* -------------------------------------------------------------------- */
/** \name Anotaciones
 *
 * `km_annotate()` del Python. El keymap se llama "Grease Pencil" por herencia: es el
 * de las anotaciones, no el del Grease Pencil de objeto.
 * \{ */

static void km_annotate(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Grease Pencil", "EMPTY", "WINDOW");

  if (params.use_key_activate_tools) {
    tool_cycle(km, "builtin.annotate", ev("D", "PRESS"));
  }
  else {
    /* Dibujar. */
    item(km, "gpencil.annotate", ev("LEFTMOUSE", "PRESS").key_modifier("D"))
        .enum_("mode", "DRAW")
        .boolean("wait_for_input", false);
    item(km, "gpencil.annotate", ev("LEFTMOUSE", "PRESS").key_modifier("D").shift())
        .enum_("mode", "DRAW")
        .boolean("wait_for_input", false);
    /* Dibujar - lineas rectas. */
    item(km, "gpencil.annotate", ev("LEFTMOUSE", "PRESS").alt().key_modifier("D"))
        .enum_("mode", "DRAW_STRAIGHT")
        .boolean("wait_for_input", false);
    /* Dibujar - polilineas. */
    item(km, "gpencil.annotate", ev("LEFTMOUSE", "PRESS").shift().alt().key_modifier("D"))
        .enum_("mode", "DRAW_POLY")
        .boolean("wait_for_input", false);
    /* Borrar. */
    item(km, "gpencil.annotate", ev("RIGHTMOUSE", "PRESS").key_modifier("D"))
        .enum_("mode", "ERASER")
        .boolean("wait_for_input", false);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Grease Pencil: seleccion
 *
 * `km_grease_pencil_selection()` del Python.
 * \{ */

static void km_grease_pencil_selection(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Grease Pencil Selection", "EMPTY", "WINDOW");

  /* `_template_items_select_actions(params, "grease_pencil.select_all")`. */
  if (!params.use_select_all_toggle) {
    item(km, "grease_pencil.select_all", ev("A", "PRESS")).enum_("action", "SELECT");
    item(km, "grease_pencil.select_all", ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, "grease_pencil.select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
    item(km, "grease_pencil.select_all", ev("A", "DOUBLE_CLICK")).enum_("action", "DESELECT");
  }
  else if (params.legacy) {
    /* En el keymap heredado Alt+A es reproducir, asi que ahi no hay "deseleccionar". */
    item(km, "grease_pencil.select_all", ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, "grease_pencil.select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }
  else {
    item(km, "grease_pencil.select_all", ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, "grease_pencil.select_all", ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, "grease_pencil.select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }

  /* Seleccionar enlazado. */
  item(km, "grease_pencil.select_linked", ev("L", "PRESS"));
  item(km, "grease_pencil.select_linked", ev("L", "PRESS").ctrl());
  /* Mas / menos seleccion. */
  item(km, "grease_pencil.select_more", ev("NUMPAD_PLUS", "PRESS").ctrl().repeat());
  item(km, "grease_pencil.select_less", ev("NUMPAD_MINUS", "PRESS").ctrl().repeat());
  /* Seleccionar similar. */
  item(km, "grease_pencil.select_similar", ev("G", "PRESS").shift());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Grease Pencil: modo pintura
 *
 * `km_grease_pencil_paint_mode()` del Python.
 * \{ */

static void km_grease_pencil_paint_mode(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Grease Pencil Paint Mode", "EMPTY", "WINDOW");

  /* Material activo. */
  item_menu(km, "VIEW3D_MT_greasepencil_material_active", ev("U", "PRESS"));
  /* Capa activa. */
  item_menu(km, "GREASE_PENCIL_MT_layer_active", ev("Y", "PRESS"));

  /* `_template_items_hide_reveal_actions("grease_pencil.layer_hide",
   * "grease_pencil.layer_reveal")`: primero el "mostrar", luego los dos "ocultar". */
  item(km, "grease_pencil.layer_reveal", ev("H", "PRESS").alt());
  item(km, "grease_pencil.layer_hide", ev("H", "PRESS")).boolean("unselected", false);
  item(km, "grease_pencil.layer_hide", ev("H", "PRESS").shift()).boolean("unselected", true);

  /* Intercambiar color primario y secundario. */
  item(km, "paint.brush_colors_flip", ev("X", "PRESS"));
  item(km, "paint.sample_color", ev("X", "PRESS").shift()).boolean("merged", false);

  /* Aislar capa. */
  item(km, "grease_pencil.layer_isolate", ev("NUMPAD_ASTERIX", "PRESS"));

  /* Menu de fotogramas clave. */
  item_menu(km, "VIEW3D_MT_edit_greasepencil_animation", ev("I", "PRESS"));

  /* Insertar fotograma clave vacio. */
  item(km, "grease_pencil.insert_blank_frame", ev("I", "PRESS").shift());

  /* Borrar todos los fotogramas activos. */
  item(km, "grease_pencil.delete_frame", ev("DEL", "PRESS").shift())
      .enum_("type", "ALL_FRAMES");

  /* Menu de borrado de animacion. */
  item_menu(km, "GREASE_PENCIL_MT_draw_delete", ev("I", "PRESS").alt());

  /* Fusionar con la de abajo. */
  item(km, "grease_pencil.layer_merge", ev("M", "PRESS").ctrl().shift())
      .enum_("mode", "ACTIVE");

  /* `op_tool_optional`: con "activar herramientas por tecla" la tecla activa la
   * herramienta en vez de lanzar el operador. */
  if (params.use_key_activate_tools) {
    tool_cycle(km, "builtin.interpolate", ev("E", "PRESS").ctrl());
  }
  else {
    item(km, "grease_pencil.interpolate", ev("E", "PRESS").ctrl())
        .boolean("use_selection", false);
  }
  item(km, "grease_pencil.interpolate_sequence", ev("E", "PRESS").shift().ctrl())
      .boolean("use_selection", false);

  /* Borrado por lazo / caja. */
  item(km, "grease_pencil.erase_lasso", ev("RIGHTMOUSE", "PRESS").ctrl().alt());
  item(km, "grease_pencil.erase_box", ev("B", "PRESS")).boolean("wait_for_input", true);
  /* Tamano del pincel. */
  item(km, "wm.radial_control", ev("F", "PRESS"))
      .string("data_path_primary", "tool_settings.gpencil_paint.brush.size");
  /* Fuerza del pincel. */
  item(km, "wm.radial_control", ev("F", "PRESS").shift())
      .string("data_path_primary", "tool_settings.gpencil_paint.brush.strength");

  /* `_template_asset_shelf_popup("VIEW3D_AST_brush_gpencil_paint",
   * params.spacebar_action)`: con la barra espaciadora dedicada a la busqueda no hay
   * atajo para el estante de recursos. */
  if (params.spacebar_action == SpacebarAction::Tool) {
    item(km, "wm.call_asset_shelf_popover", ev("SPACE", "PRESS"))
        .string("name", "VIEW3D_AST_brush_gpencil_paint");
  }
  else if (params.spacebar_action == SpacebarAction::Play) {
    item(km, "wm.call_asset_shelf_popover", ev("SPACE", "PRESS").shift())
        .string("name", "VIEW3D_AST_brush_gpencil_paint");
  }

  /* `_template_items_context_panel("VIEW3D_PT_greasepencil_draw_context_menu",
   * params.context_menu_event)`. */
  item_panel(km, "VIEW3D_PT_greasepencil_draw_context_menu", params.context_menu_event);
  item_panel(km, "VIEW3D_PT_greasepencil_draw_context_menu", ev("APP", "PRESS"));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Grease Pencil: trazo de pincel
 *
 * `km_grease_pencil_brush_stroke()` del Python.
 * \{ */

static void km_grease_pencil_brush_stroke(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap(kc, "Grease Pencil Brush Stroke", "EMPTY", "WINDOW");

  item(km, "grease_pencil.brush_stroke", ev("LEFTMOUSE", "PRESS"));
  item(km, "grease_pencil.brush_stroke", ev("LEFTMOUSE", "PRESS").ctrl())
      .enum_("mode", "ERASE");
  item(km, "grease_pencil.brush_stroke", ev("LEFTMOUSE", "PRESS").shift())
      .enum_("mode", "SMOOTH");
  item(km, "grease_pencil.brush_stroke", ev("ERASER", "PRESS")).enum_("mode", "ERASE");
  /* Aumentar / reducir el tamano del pincel. */
  item(km, "brush.scale_size", ev("LEFT_BRACKET", "PRESS").repeat()).number("scalar", 0.9f);
  item(km, "brush.scale_size", ev("RIGHT_BRACKET", "PRESS").repeat())
      .number("scalar", 1.0f / 0.9f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Grease Pencil: modo edicion
 *
 * `km_grease_pencil_edit_mode()` del Python.
 * \{ */

static void km_grease_pencil_edit_mode(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Grease Pencil Edit Mode", "EMPTY", "WINDOW");

  /* Menu de borrado. */
  item_menu(km, "VIEW3D_MT_edit_greasepencil_delete", ev("X", "PRESS"));
  item_menu(km, "VIEW3D_MT_edit_greasepencil_delete", ev("DEL", "PRESS"));
  /* Disolver. */
  item(km, "grease_pencil.dissolve", ev("X", "PRESS").ctrl());
  item(km, "grease_pencil.dissolve", ev("DEL", "PRESS").ctrl());
  /* Copiar / pegar. */
  item(km, "grease_pencil.copy", ev("C", "PRESS").ctrl());
  item(km, "grease_pencil.paste", ev("V", "PRESS").ctrl());
  item(km, "grease_pencil.paste", ev("V", "PRESS").shift().ctrl()).boolean("paste_back", true);
  /* Imantar. */
  item_menu_pie(km, "GREASE_PENCIL_MT_snap_pie", ev("S", "PRESS").shift());
  /* Separar. */
  item(km, "grease_pencil.separate", ev("P", "PRESS"));
  /* Borrar todos los fotogramas activos. */
  item(km, "grease_pencil.delete_frame", ev("DEL", "PRESS").shift())
      .enum_("type", "ALL_FRAMES");
  /* Menu de fotogramas clave. */
  item_menu(km, "VIEW3D_MT_edit_greasepencil_animation", ev("I", "PRESS"));

  /* Insertar fotograma clave vacio. */
  item(km, "grease_pencil.insert_blank_frame", ev("I", "PRESS").shift());

  /* Menu de borrado de animacion. */
  item_menu(km, "GREASE_PENCIL_MT_draw_delete", ev("I", "PRESS").alt());

  /* `_template_items_hide_reveal_actions("grease_pencil.layer_hide",
   * "grease_pencil.layer_reveal")`. */
  item(km, "grease_pencil.layer_reveal", ev("H", "PRESS").alt());
  item(km, "grease_pencil.layer_hide", ev("H", "PRESS")).boolean("unselected", false);
  item(km, "grease_pencil.layer_hide", ev("H", "PRESS").shift()).boolean("unselected", true);

  /* `_template_items_transform_actions(params, use_bend=True, use_mirror=True,
   * use_tosphere=True, use_shear=True)`. */
  if (params.use_key_activate_tools) {
    tool_cycle(km, "builtin.move", ev("G", "PRESS"));
  }
  else {
    item(km, "transform.translate", ev("G", "PRESS"));
  }
  if (params.use_key_activate_tools) {
    tool_cycle(km, "builtin.rotate", ev("R", "PRESS"));
  }
  else {
    item(km, "transform.rotate", ev("R", "PRESS"));
  }
  if (params.use_key_activate_tools) {
    tool_cycle(km, "builtin.scale", ev("S", "PRESS"));
  }
  else {
    item(km, "transform.resize", ev("S", "PRESS"));
  }
  item(km, "transform.translate", ev(params.select_mouse, "CLICK_DRAG"));
  /* use_bend. */
  item(km, "transform.bend", ev("W", "PRESS").shift());
  /* use_mirror. */
  item(km, "transform.mirror", ev("M", "PRESS").ctrl());
  /* use_tosphere. */
  if (params.use_key_activate_tools) {
    tool_cycle(km, "builtin.to_sphere", ev("S", "PRESS").shift().alt());
  }
  else {
    item(km, "transform.tosphere", ev("S", "PRESS").shift().alt());
  }
  /* use_shear. */
  if (params.use_key_activate_tools) {
    tool_cycle(km, "builtin.shear", ev("S", "PRESS").shift().ctrl().alt());
  }
  else {
    item(km, "transform.shear", ev("S", "PRESS").shift().ctrl().alt());
  }

  item(km, "transform.transform", ev("S", "PRESS").alt()).enum_("mode", "CURVE_SHRINKFATTEN");
  item(km, "transform.transform", ev("F", "PRESS").shift()).enum_("mode", "GPENCIL_OPACITY");

  /* `_template_items_proportional_editing(params, connected=True,
   * toggle_data_path="tool_settings.use_proportional_edit")`. */
  if (!params.legacy) {
    item_menu_pie(km, "VIEW3D_MT_proportional_editing_falloff_pie", ev("O", "PRESS").shift());
  }
  else {
    item(km, "wm.context_cycle_enum", ev("O", "PRESS").shift())
        .string("data_path", "tool_settings.proportional_edit_falloff")
        .boolean("wrap", true);
  }
  item(km, "wm.context_toggle", ev("O", "PRESS"))
      .string("data_path", "tool_settings.use_proportional_edit");
  item(km, "wm.context_toggle", ev("O", "PRESS").alt())
      .string("data_path", "tool_settings.use_proportional_connected");

  /* Cerrar / alternar ciclo. */
  item(km, "grease_pencil.cyclical_set", ev("F", "PRESS"))
      .enum_("type", "CLOSE")
      .boolean("subdivide_cyclic_segment", true);
  item(km, "grease_pencil.cyclical_set", ev("C", "PRESS").alt())
      .enum_("type", "TOGGLE")
      .boolean("subdivide_cyclic_segment", false);

  /* Unir seleccion. */
  item(km, "grease_pencil.join_selection", ev("J", "PRESS").ctrl()).enum_("type", "JOIN");
  item(km, "grease_pencil.join_selection", ev("J", "PRESS").shift().ctrl())
      .enum_("type", "JOINCOPY");

  item(km, "grease_pencil.duplicate_move", ev("D", "PRESS").shift());

  /* Dividir trazo. */
  item(km, "grease_pencil.stroke_split", ev("V", "PRESS").shift());

  /* Extruir y mover los puntos seleccionados. */
  if (params.use_key_activate_tools) {
    tool_cycle(km, "builtin.extrude", ev("E", "PRESS"));
  }
  else {
    item(km, "grease_pencil.extrude_move", ev("E", "PRESS"));
  }

  /* Capa activa. */
  item_menu(km, "GREASE_PENCIL_MT_layer_active", ev("Y", "PRESS"));

  /* Mover a la capa. */
  item_menu(km, "GREASE_PENCIL_MT_move_to_layer", ev("M", "PRESS"));

  /* Fusionar con la de abajo. */
  item(km, "grease_pencil.layer_merge", ev("M", "PRESS").ctrl().shift())
      .enum_("mode", "ACTIVE");

  /* Superposicion de lineas de edicion. */
  item(km, "wm.context_toggle", ev("Q", "PRESS").shift())
      .string("data_path", "space_data.overlay.use_gpencil_edit_lines");
  item(km, "wm.context_toggle", ev("Q", "PRESS").shift().alt())
      .string("data_path", "space_data.overlay.use_gpencil_multiedit_line_only");

  /* `_template_items_context_menu("VIEW3D_MT_greasepencil_edit_context_menu",
   * params.context_menu_event)`. */
  item_menu(km, "VIEW3D_MT_greasepencil_edit_context_menu", params.context_menu_event);
  item_menu(km, "VIEW3D_MT_greasepencil_edit_context_menu", ev("APP", "PRESS"));

  /* Grupos de vertices. */
  item_menu(km, "VIEW3D_MT_greasepencil_vertex_group", ev("G", "PRESS").ctrl());

  /* Reordenar. */
  item(km, "grease_pencil.reorder", ev("UP_ARROW", "PRESS").ctrl().shift())
      .enum_("direction", "TOP");
  item(km, "grease_pencil.reorder", ev("UP_ARROW", "PRESS").ctrl().repeat())
      .enum_("direction", "UP");
  item(km, "grease_pencil.reorder", ev("DOWN_ARROW", "PRESS").ctrl().repeat())
      .enum_("direction", "DOWN");
  item(km, "grease_pencil.reorder", ev("DOWN_ARROW", "PRESS").ctrl().shift())
      .enum_("direction", "BOTTOM");

  /* Aislar capa. */
  item(km, "grease_pencil.layer_isolate", ev("NUMPAD_ASTERIX", "PRESS"));

  /* Modo de seleccion. */
  item(km, "grease_pencil.set_selection_mode", ev("ONE", "PRESS")).enum_("mode", "POINT");
  item(km, "grease_pencil.set_selection_mode", ev("TWO", "PRESS")).enum_("mode", "STROKE");
  item(km, "grease_pencil.set_selection_mode", ev("THREE", "PRESS")).enum_("mode", "SEGMENT");

  /* Tipo de manejador. */
  item(km, "grease_pencil.set_handle_type", ev("V", "PRESS"));

  if (params.use_key_activate_tools) {
    tool_cycle(km, "builtin.interpolate", ev("E", "PRESS").ctrl());
  }
  else {
    item(km, "grease_pencil.interpolate", ev("E", "PRESS").ctrl())
        .boolean("use_selection", true);
  }
  item(km, "grease_pencil.interpolate_sequence", ev("E", "PRESS").shift().ctrl())
      .boolean("use_selection", true);
}

/** \} */

void register_group_12(wmKeyConfig *kc, const Params &params)
{
  km_annotate(kc, params);
  km_grease_pencil_selection(kc, params);
  km_grease_pencil_paint_mode(kc, params);
  km_grease_pencil_brush_stroke(kc, params);
  km_grease_pencil_edit_mode(kc, params);
}

}  // namespace flipendo::keymap
