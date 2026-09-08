/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Mapa de teclado, grupo 18: modos de edicion de metaballs, lattice, particulas,
 * texto (Font) y el modo de edicion de curvas heredado.
 * Transliterado de blender_default.py.
 */

#include "FL_keymap_build.hpp"
#include "FL_keymap_params.hpp"

namespace flipendo::keymap {

/* -------------------------------------------------------------------- */
/** \name Plantillas repetidas dentro de este grupo
 *
 * Las cinco funciones `km_*` de este fichero usan las mismas plantillas del Python
 * una y otra vez (transformaciones, seleccion, ocultar/mostrar, edicion
 * proporcional, menu contextual). Se escriben una sola vez como `static` para que la
 * comparacion linea a linea con el Python siga siendo posible sin repetirlas cinco
 * veces. El orden de los atajos que emiten es el de la plantilla original.
 * \{ */

/** `_template_items_context_menu(menu, key_args_primary)`. */
static void template_items_context_menu(wmKeyMap *km, const char *menu, const Event &primary)
{
  item_menu(km, menu, primary);
  item_menu(km, menu, ev("APP", "PRESS"));
}

/** `_template_items_select_actions(params, operator)`. */
static void template_items_select_actions(wmKeyMap *km, const Params &params, const char *op)
{
  if (!params.use_select_all_toggle) {
    item(km, op, ev("A", "PRESS")).enum_("action", "SELECT");
    item(km, op, ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, op, ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
    item(km, op, ev("A", "DOUBLE_CLICK")).enum_("action", "DESELECT");
  }
  else if (params.legacy) {
    /* En el keymap heredado Alt+A es la reproduccion, por eso ahi no hay
     * "deseleccionar todo". */
    item(km, op, ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, op, ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }
  else {
    item(km, op, ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, op, ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, op, ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }
}

/** `_template_items_hide_reveal_actions(op_hide, op_reveal)`. */
static void template_items_hide_reveal_actions(wmKeyMap *km,
                                               const char *op_hide,
                                               const char *op_reveal)
{
  item(km, op_reveal, ev("H", "PRESS").alt());
  item(km, op_hide, ev("H", "PRESS")).boolean("unselected", false);
  item(km, op_hide, ev("H", "PRESS").shift()).boolean("unselected", true);
}

/** `_template_items_proportional_editing(params, connected=..., toggle_data_path=...)`. */
static void template_items_proportional_editing(wmKeyMap *km,
                                                const Params &params,
                                                const bool connected,
                                                const char *toggle_data_path)
{
  if (!params.legacy) {
    item_menu_pie(km, "VIEW3D_MT_proportional_editing_falloff_pie", ev("O", "PRESS").shift());
  }
  else {
    item(km, "wm.context_cycle_enum", ev("O", "PRESS").shift())
        .string("data_path", "tool_settings.proportional_edit_falloff")
        .boolean("wrap", true);
  }
  item(km, "wm.context_toggle", ev("O", "PRESS")).string("data_path", toggle_data_path);
  if (connected) {
    item(km, "wm.context_toggle", ev("O", "PRESS").alt())
        .string("data_path", "tool_settings.use_proportional_connected");
  }
}

/**
 * `_template_items_transform_actions(params, use_bend=..., use_mirror=...,
 * use_tosphere=..., use_shear=...)`.
 *
 * Los `op_tool_optional` de la plantilla: con `use_key_activate_tools` la tecla activa
 * la herramienta (ciclandola) en vez de lanzar el operador modal.
 */
static void template_items_transform_actions(wmKeyMap *km,
                                             const Params &params,
                                             const bool use_bend,
                                             const bool use_mirror,
                                             const bool use_tosphere,
                                             const bool use_shear)
{
  if (params.use_key_activate_tools) {
    item_tool(km, "builtin.move", ev("G", "PRESS")).boolean("cycle", true);
  }
  else {
    item(km, "transform.translate", ev("G", "PRESS"));
  }
  if (params.use_key_activate_tools) {
    item_tool(km, "builtin.rotate", ev("R", "PRESS")).boolean("cycle", true);
  }
  else {
    item(km, "transform.rotate", ev("R", "PRESS"));
  }
  if (params.use_key_activate_tools) {
    item_tool(km, "builtin.scale", ev("S", "PRESS")).boolean("cycle", true);
  }
  else {
    item(km, "transform.resize", ev("S", "PRESS"));
  }

  item(km, "transform.translate", ev(params.select_mouse, "CLICK_DRAG"));

  if (use_bend) {
    /* La plantilla no pasa "bend" por `op_tool_optional`: siempre es el operador. */
    item(km, "transform.bend", ev("W", "PRESS").shift());
  }
  if (use_mirror) {
    item(km, "transform.mirror", ev("M", "PRESS").ctrl());
  }
  if (use_tosphere) {
    if (params.use_key_activate_tools) {
      item_tool(km, "builtin.to_sphere", ev("S", "PRESS").shift().alt()).boolean("cycle", true);
    }
    else {
      item(km, "transform.tosphere", ev("S", "PRESS").shift().alt());
    }
  }
  if (use_shear) {
    if (params.use_key_activate_tools) {
      item_tool(km, "builtin.shear", ev("S", "PRESS").shift().ctrl().alt())
          .boolean("cycle", true);
    }
    else {
      item(km, "transform.shear", ev("S", "PRESS").shift().ctrl().alt());
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Modo edicion: metaballs
 * \{ */

static void km_edit_metaball(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Metaball", "EMPTY", "WINDOW");

  /* _template_items_transform_actions(params, use_mirror=True). */
  template_items_transform_actions(km,
                                   params,
                                   /*use_bend=*/false,
                                   /*use_mirror=*/true,
                                   /*use_tosphere=*/false,
                                   /*use_shear=*/false);

  item(km, "object.metaball_add", ev("A", "PRESS").shift());

  /* _template_items_hide_reveal_actions("mball.hide_metaelems", "mball.reveal_metaelems"). */
  template_items_hide_reveal_actions(km, "mball.hide_metaelems", "mball.reveal_metaelems");

  item(km, "mball.delete_metaelems", ev("X", "PRESS"));
  item(km, "mball.delete_metaelems", ev("DEL", "PRESS"));
  item(km, "mball.duplicate_move", ev("D", "PRESS").shift());

  /* _template_items_select_actions(params, "mball.select_all"). */
  template_items_select_actions(km, params, "mball.select_all");

  item(km, "mball.select_similar", ev("G", "PRESS").shift());

  /* _template_items_proportional_editing(params, connected=True,
   * toggle_data_path="tool_settings.use_proportional_edit"). */
  template_items_proportional_editing(
      km, params, /*connected=*/true, "tool_settings.use_proportional_edit");

  /* _template_items_context_menu("VIEW3D_MT_edit_metaball_context_menu",
   * params.context_menu_event). */
  template_items_context_menu(km, "VIEW3D_MT_edit_metaball_context_menu", params.context_menu_event);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Modo edicion: lattice
 * \{ */

static void km_edit_lattice(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Lattice", "EMPTY", "WINDOW");

  /* _template_items_transform_actions(params, use_bend=True, use_mirror=True,
   * use_tosphere=True, use_shear=True). */
  template_items_transform_actions(km,
                                   params,
                                   /*use_bend=*/true,
                                   /*use_mirror=*/true,
                                   /*use_tosphere=*/true,
                                   /*use_shear=*/true);

  /* _template_items_select_actions(params, "lattice.select_all"). */
  template_items_select_actions(km, params, "lattice.select_all");

  item(km, "lattice.select_more", ev("NUMPAD_PLUS", "PRESS").ctrl().repeat());
  item(km, "lattice.select_less", ev("NUMPAD_MINUS", "PRESS").ctrl().repeat());
  item(km, "object.vertex_parent_set", ev("P", "PRESS").ctrl());
  item(km, "lattice.flip", ev("F", "PRESS").alt());
  item_menu(km, "VIEW3D_MT_hook", ev("H", "PRESS").ctrl());

  /* _template_items_proportional_editing(params, connected=False,
   * toggle_data_path="tool_settings.use_proportional_edit"). */
  template_items_proportional_editing(
      km, params, /*connected=*/false, "tool_settings.use_proportional_edit");

  /* _template_items_context_menu("VIEW3D_MT_edit_lattice_context_menu",
   * params.context_menu_event). */
  template_items_context_menu(km, "VIEW3D_MT_edit_lattice_context_menu", params.context_menu_event);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Modo edicion: particulas
 * \{ */

static void km_edit_particle(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Particle", "EMPTY", "WINDOW");

  /* _template_items_select_actions(params, "particle.select_all"). */
  template_items_select_actions(km, params, "particle.select_all");

  item(km, "particle.select_more", ev("NUMPAD_PLUS", "PRESS").ctrl().repeat());
  item(km, "particle.select_less", ev("NUMPAD_MINUS", "PRESS").ctrl().repeat());
  item(km, "particle.select_linked_pick", ev("L", "PRESS")).boolean("deselect", false);
  item(km, "particle.select_linked_pick", ev("L", "PRESS").shift()).boolean("deselect", true);
  item(km, "particle.select_linked", ev("L", "PRESS").ctrl());
  item(km, "particle.delete", ev("X", "PRESS"));
  item(km, "particle.delete", ev("DEL", "PRESS"));

  /* _template_items_hide_reveal_actions("particle.hide", "particle.reveal"). */
  template_items_hide_reveal_actions(km, "particle.hide", "particle.reveal");

  item(km, "particle.brush_edit", ev("LEFTMOUSE", "PRESS"));
  item(km, "particle.brush_edit", ev("LEFTMOUSE", "PRESS").shift());
  item(km, "wm.radial_control", ev("F", "PRESS"))
      .string("data_path_primary", "tool_settings.particle_edit.brush.size");
  item(km, "wm.radial_control", ev("F", "PRESS").shift())
      .string("data_path_primary", "tool_settings.particle_edit.brush.strength");
  item(km, "particle.weight_set", ev("K", "PRESS").shift());

  /* El generador `for i, value in enumerate(('PATH', 'POINT', 'TIP'))` sobre NUMBERS_1:
   * las teclas van en el orden FISICO del teclado (1, 2, 3).
   * `value` de `wm.context_set_enum` es una cadena en RNA (el operador la resuelve al
   * ejecutarse), no una enumeracion: por eso `.string()` y no `.enum_()`. */
  {
    const char *const number_keys[3] = {"ONE", "TWO", "THREE"};
    const char *const select_modes[3] = {"PATH", "POINT", "TIP"};
    for (int i = 0; i < 3; i++) {
      item(km, "wm.context_set_enum", ev(number_keys[i], "PRESS"))
          .string("data_path", "tool_settings.particle_edit.select_mode")
          .string("value", select_modes[i]);
    }
  }

  /* _template_items_proportional_editing(params, connected=False,
   * toggle_data_path="tool_settings.use_proportional_edit"). */
  template_items_proportional_editing(
      km, params, /*connected=*/false, "tool_settings.use_proportional_edit");

  /* _template_items_context_menu("VIEW3D_MT_particle_context_menu",
   * params.context_menu_event). */
  template_items_context_menu(km, "VIEW3D_MT_particle_context_menu", params.context_menu_event);

  /* _template_items_transform_actions(params): aqui van al final, despues del menu
   * contextual, igual que en el Python. */
  template_items_transform_actions(km,
                                   params,
                                   /*use_bend=*/false,
                                   /*use_mirror=*/false,
                                   /*use_tosphere=*/false,
                                   /*use_shear=*/false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Modo edicion: texto
 * \{ */

static void km_edit_font(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Font", "EMPTY", "WINDOW");

  item(km, "font.style_toggle", ev("B", "PRESS").ctrl()).enum_("style", "BOLD");
  item(km, "font.style_toggle", ev("I", "PRESS").ctrl()).enum_("style", "ITALIC");
  item(km, "font.style_toggle", ev("U", "PRESS").ctrl()).enum_("style", "UNDERLINE");
  item(km, "font.style_toggle", ev("P", "PRESS").ctrl()).enum_("style", "SMALL_CAPS");
  item(km, "font.delete", ev("DEL", "PRESS").repeat()).enum_("type", "NEXT_OR_SELECTION");
  item(km, "font.delete", ev("DEL", "PRESS").ctrl().repeat()).enum_("type", "NEXT_WORD");
  item(km, "font.delete", ev("BACK_SPACE", "PRESS").repeat())
      .enum_("type", "PREVIOUS_OR_SELECTION");
  /* Shift+Retroceso borra igual que Retroceso a secas: es deliberado en el Python. */
  item(km, "font.delete", ev("BACK_SPACE", "PRESS").shift().repeat())
      .enum_("type", "PREVIOUS_OR_SELECTION");
  item(km, "font.delete", ev("BACK_SPACE", "PRESS").ctrl().repeat()).enum_("type", "PREVIOUS_WORD");
  item(km, "font.move", ev("HOME", "PRESS")).enum_("type", "LINE_BEGIN");
  item(km, "font.move", ev("END", "PRESS")).enum_("type", "LINE_END");
  item(km, "font.move", ev("LEFT_ARROW", "PRESS").repeat()).enum_("type", "PREVIOUS_CHARACTER");
  item(km, "font.move", ev("RIGHT_ARROW", "PRESS").repeat()).enum_("type", "NEXT_CHARACTER");
  item(km, "font.move", ev("LEFT_ARROW", "PRESS").ctrl().repeat()).enum_("type", "PREVIOUS_WORD");
  item(km, "font.move", ev("RIGHT_ARROW", "PRESS").ctrl().repeat()).enum_("type", "NEXT_WORD");
  item(km, "font.move", ev("UP_ARROW", "PRESS").repeat()).enum_("type", "PREVIOUS_LINE");
  item(km, "font.move", ev("DOWN_ARROW", "PRESS").repeat()).enum_("type", "NEXT_LINE");
  item(km, "font.move", ev("PAGE_UP", "PRESS").repeat()).enum_("type", "PREVIOUS_PAGE");
  item(km, "font.move", ev("PAGE_DOWN", "PRESS").repeat()).enum_("type", "NEXT_PAGE");
  item(km, "font.move", ev("HOME", "PRESS").ctrl().repeat()).enum_("type", "TEXT_BEGIN");
  item(km, "font.move", ev("END", "PRESS").ctrl().repeat()).enum_("type", "TEXT_END");
  item(km, "font.move_select", ev("HOME", "PRESS").shift()).enum_("type", "LINE_BEGIN");
  item(km, "font.move_select", ev("END", "PRESS").shift()).enum_("type", "LINE_END");
  item(km, "font.move_select", ev("LEFT_ARROW", "PRESS").shift().repeat())
      .enum_("type", "PREVIOUS_CHARACTER");
  item(km, "font.move_select", ev("RIGHT_ARROW", "PRESS").shift().repeat())
      .enum_("type", "NEXT_CHARACTER");
  item(km, "font.move_select", ev("LEFT_ARROW", "PRESS").shift().ctrl().repeat())
      .enum_("type", "PREVIOUS_WORD");
  item(km, "font.move_select", ev("RIGHT_ARROW", "PRESS").shift().ctrl().repeat())
      .enum_("type", "NEXT_WORD");
  item(km, "font.move_select", ev("UP_ARROW", "PRESS").shift().repeat())
      .enum_("type", "PREVIOUS_LINE");
  item(km, "font.move_select", ev("DOWN_ARROW", "PRESS").shift().repeat())
      .enum_("type", "NEXT_LINE");
  item(km, "font.move_select", ev("PAGE_UP", "PRESS").shift().repeat())
      .enum_("type", "PREVIOUS_PAGE");
  item(km, "font.move_select", ev("PAGE_DOWN", "PRESS").shift().repeat())
      .enum_("type", "NEXT_PAGE");
  item(km, "font.move_select", ev("HOME", "PRESS").shift().ctrl().repeat())
      .enum_("type", "TEXT_BEGIN");
  item(km, "font.move_select", ev("END", "PRESS").shift().ctrl().repeat())
      .enum_("type", "TEXT_END");
  item(km, "font.change_spacing", ev("LEFT_ARROW", "PRESS").alt().repeat())
      .number("delta", -1.0f);
  item(km, "font.change_spacing", ev("RIGHT_ARROW", "PRESS").alt().repeat())
      .number("delta", 1.0f);
  item(km, "font.change_spacing", ev("LEFT_ARROW", "PRESS").shift().alt().repeat())
      .number("delta", -0.1f);
  item(km, "font.change_spacing", ev("RIGHT_ARROW", "PRESS").shift().alt().repeat())
      .number("delta", 0.1f);
  item(km, "font.change_character", ev("UP_ARROW", "PRESS").alt().repeat()).integer("delta", 1);
  item(km, "font.change_character", ev("DOWN_ARROW", "PRESS").alt().repeat()).integer("delta", -1);
  item(km, "font.select_all", ev("A", "PRESS").ctrl());
  item(km, "font.text_copy", ev("C", "PRESS").ctrl());
  item(km, "font.text_cut", ev("X", "PRESS").ctrl());
  item(km, "font.text_paste", ev("V", "PRESS").ctrl().repeat());
  item(km, "font.line_break", ev("RET", "PRESS").repeat());
  item(km, "font.line_break", ev("NUMPAD_ENTER", "PRESS").repeat());
  item(km, "font.text_insert", ev("TEXTINPUT", "ANY").any().repeat());
  item(km, "font.text_insert", ev("BACK_SPACE", "PRESS").alt().repeat()).boolean("accent", true);

  /* _template_items_context_menu("VIEW3D_MT_edit_font_context_menu",
   * params.context_menu_event). */
  template_items_context_menu(km, "VIEW3D_MT_edit_font_context_menu", params.context_menu_event);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Modo edicion: curvas (heredado)
 * \{ */

static void km_edit_curve_legacy(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Curve", "EMPTY", "WINDOW");

  /* _template_items_transform_actions(params, use_bend=True, use_mirror=True). */
  template_items_transform_actions(km,
                                   params,
                                   /*use_bend=*/true,
                                   /*use_mirror=*/true,
                                   /*use_tosphere=*/false,
                                   /*use_shear=*/false);

  item_menu(km, "TOPBAR_MT_edit_curve_add", ev("A", "PRESS").shift());
  item(km, "curve.handle_type_set", ev("V", "PRESS"));
  item(km, "curve.vertex_add", ev(params.action_mouse, "CLICK").ctrl());

  /* _template_items_select_actions(params, "curve.select_all"). */
  template_items_select_actions(km, params, "curve.select_all");

  item(km, "curve.select_row", ev("R", "PRESS").shift().ctrl());
  item(km, "curve.select_more", ev("NUMPAD_PLUS", "PRESS").ctrl().repeat());
  item(km, "curve.select_less", ev("NUMPAD_MINUS", "PRESS").ctrl().repeat());
  item(km, "curve.select_linked", ev("L", "PRESS").ctrl());
  item(km, "curve.select_similar", ev("G", "PRESS").shift());
  item(km, "curve.select_linked_pick", ev("L", "PRESS")).boolean("deselect", false);
  item(km, "curve.select_linked_pick", ev("L", "PRESS").shift()).boolean("deselect", true);
  item(km,
       "curve.shortest_path_pick",
       ev(params.select_mouse, params.select_mouse_value_fallback).ctrl());
  item(km, "curve.separate", ev("P", "PRESS"));
  item(km, "curve.split", ev("Y", "PRESS"));

  /* op_tool_optional(("curve.extrude_move", E), (op_tool_cycle, "builtin.extrude")). */
  if (params.use_key_activate_tools) {
    item_tool(km, "builtin.extrude", ev("E", "PRESS")).boolean("cycle", true);
  }
  else {
    item(km, "curve.extrude_move", ev("E", "PRESS"));
  }

  item(km, "curve.duplicate_move", ev("D", "PRESS").shift());
  item(km, "curve.make_segment", ev("F", "PRESS"));
  item(km, "curve.cyclic_toggle", ev("C", "PRESS").alt());
  item_menu(km, "VIEW3D_MT_edit_curve_delete", ev("X", "PRESS"));
  item_menu(km, "VIEW3D_MT_edit_curve_delete", ev("DEL", "PRESS"));
  item(km, "curve.dissolve_verts", ev("X", "PRESS").ctrl());
  item(km, "curve.dissolve_verts", ev("DEL", "PRESS").ctrl());
  item(km, "curve.tilt_clear", ev("T", "PRESS").alt());

  /* op_tool_optional(("transform.tilt", Ctrl+T), (op_tool_cycle, "builtin.tilt")). */
  if (params.use_key_activate_tools) {
    item_tool(km, "builtin.tilt", ev("T", "PRESS").ctrl()).boolean("cycle", true);
  }
  else {
    item(km, "transform.tilt", ev("T", "PRESS").ctrl());
  }

  item(km, "transform.transform", ev("S", "PRESS").alt()).enum_("mode", "CURVE_SHRINKFATTEN");

  /* _template_items_hide_reveal_actions("curve.hide", "curve.reveal"). */
  template_items_hide_reveal_actions(km, "curve.hide", "curve.reveal");

  /* El Python elige la tecla modificadora dentro del propio diccionario:
   * `"ctrl" if params.legacy else "shift"`. */
  if (params.legacy) {
    item(km, "curve.normals_make_consistent", ev("N", "PRESS").ctrl());
  }
  else {
    item(km, "curve.normals_make_consistent", ev("N", "PRESS").shift());
  }

  item(km, "object.vertex_parent_set", ev("P", "PRESS").ctrl());
  item_menu(km, "VIEW3D_MT_hook", ev("H", "PRESS").ctrl());

  /* _template_items_proportional_editing(params, connected=True,
   * toggle_data_path="tool_settings.use_proportional_edit"). */
  template_items_proportional_editing(
      km, params, /*connected=*/true, "tool_settings.use_proportional_edit");

  /* _template_items_context_menu("VIEW3D_MT_edit_curve_context_menu",
   * params.context_menu_event). */
  template_items_context_menu(km, "VIEW3D_MT_edit_curve_context_menu", params.context_menu_event);
}

/** \} */

void register_group_18(wmKeyConfig *kc, const Params &params)
{
  km_edit_metaball(kc, params);
  km_edit_lattice(kc, params);
  km_edit_particle(kc, params);
  km_edit_font(kc, params);
  km_edit_curve_legacy(kc, params);
}

}  // namespace flipendo::keymap
