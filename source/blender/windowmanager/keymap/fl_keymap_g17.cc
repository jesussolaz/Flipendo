/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Mapa de teclado, grupo 17: los modos de edicion de malla ("Mesh") y de esqueleto
 * ("Armature"), ambos keymaps sin espacio propio (space_type 'EMPTY').
 * Transliterado de blender_default.py.
 */

#include "FL_keymap_build.hpp"
#include "FL_keymap_params.hpp"

namespace flipendo::keymap {

/* -------------------------------------------------------------------- */
/** \name Plantillas del Python usadas por este grupo
 * \{ */

/**
 * `op_tool_optional((op, kmi_args, None), (op_tool_cycle, tool), params)`.
 *
 * Con "activar herramientas por tecla" la misma combinacion activa la herramienta
 * (ciclando entre sus variantes) en lugar de lanzar el operador. Se hace helper
 * porque el lote lo usa una docena de veces; solo vale para los casos en los que el
 * operador NO lleva propiedades, que son los mas. Cuando las lleva se escribe el
 * `if` a mano, porque al activar la herramienta esas propiedades se pierden.
 */
static void tool_optional(wmKeyMap *km,
                          const Params &params,
                          const char *op,
                          const char *tool,
                          const Event &event)
{
  if (params.use_key_activate_tools) {
    /* `op_tool_cycle` (no `op_tool`): pone tambien `cycle=True`. */
    item_tool(km, tool, event).boolean("cycle", true);
  }
  else {
    item(km, op, event);
  }
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
    /* En el keymap heredado Alt+A es reproducir, asi que no hay "deseleccionar". */
    item(km, op, ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, op, ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }
  else {
    item(km, op, ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, op, ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, op, ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Modo edicion de malla
 * \{ */

static void km_edit_mesh(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Mesh", "EMPTY", "WINDOW");

  /* `_template_items_transform_actions(params, use_bend=True, use_mirror=True,
   * use_tosphere=True, use_shear=True)`. */
  tool_optional(km, params, "transform.translate", "builtin.move", ev("G", "PRESS"));
  tool_optional(km, params, "transform.rotate", "builtin.rotate", ev("R", "PRESS"));
  tool_optional(km, params, "transform.resize", "builtin.scale", ev("S", "PRESS"));
  item(km, "transform.translate", ev(params.select_mouse, "CLICK_DRAG"));
  item(km, "transform.bend", ev("W", "PRESS").shift());
  item(km, "transform.mirror", ev("M", "PRESS").ctrl());
  tool_optional(
      km, params, "transform.tosphere", "builtin.to_sphere", ev("S", "PRESS").shift().alt());
  tool_optional(
      km, params, "transform.shear", "builtin.shear", ev("S", "PRESS").shift().ctrl().alt());

  item(km, "transform.skin_resize", ev("A", "PRESS").ctrl());

  /* Herramientas. */
  if (params.use_key_activate_tools) {
    item_tool(km, "builtin.loop_cut", ev("R", "PRESS").ctrl()).boolean("cycle", true);
  }
  else {
    item(km, "mesh.loopcut_slide", ev("R", "PRESS").ctrl())
        .sub("TRANSFORM_OT_edge_slide")
        .boolean("release_confirm", false);
  }
  if (params.use_key_activate_tools) {
    item_tool(km, "builtin.offset_edge_loop_cut", ev("R", "PRESS").shift().ctrl())
        .boolean("cycle", true);
  }
  else {
    item(km, "mesh.offset_edge_loops_slide", ev("R", "PRESS").shift().ctrl())
        .sub("TRANSFORM_OT_edge_slide")
        .boolean("release_confirm", false);
  }
  tool_optional(km, params, "mesh.inset", "builtin.inset_faces", ev("I", "PRESS"));
  if (params.use_key_activate_tools) {
    item_tool(km, "builtin.bevel", ev("B", "PRESS").ctrl()).boolean("cycle", true);
  }
  else {
    item(km, "mesh.bevel", ev("B", "PRESS").ctrl()).enum_("affect", "EDGES");
  }
  tool_optional(km,
                params,
                "transform.shrink_fatten",
                "builtin.shrink_fatten",
                ev("S", "PRESS").alt());
  item(km, "mesh.bevel", ev("B", "PRESS").shift().ctrl()).enum_("affect", "VERTICES");

  /* Modos de seleccion: `_template_items_editmode_mesh_select_mode(params)`. */
  if (params.legacy) {
    item_menu(km, "VIEW3D_MT_edit_mesh_select_mode", ev("TAB", "PRESS").ctrl());
  }
  else {
    /* Los tres bucles anidados del Python, en su mismo orden (expandir por fuera,
     * extender en medio, el modo por dentro): el orden es semantico. */
    static const char *const mode_keys[3] = {"ONE", "TWO", "THREE"};
    static const char *const mode_values[3] = {"VERT", "EDGE", "FACE"};
    for (int expand = 0; expand < 2; expand++) {
      for (int extend = 0; extend < 2; extend++) {
        for (int i = 0; i < 3; i++) {
          Event e(mode_keys[i], "PRESS");
          if (expand != 0) {
            e.ctrl();
          }
          if (extend != 0) {
            e.shift();
          }
          Item it = item(km, "mesh.select_mode", e);
          if (extend != 0) {
            it.boolean("use_extend", true);
          }
          if (expand != 0) {
            it.boolean("use_expand", true);
          }
          it.enum_("type", mode_values[i]);
        }
      }
    }
  }

  /* Seleccion de bucle con Alt. El doble clic esta mas abajo, por si la emulacion de
   * tres botones esta activada. */
  item(km,
       "mesh.loop_select",
       ev(params.select_mouse, params.select_mouse_value).alt());
  item(km,
       "mesh.loop_select",
       ev(params.select_mouse, params.select_mouse_value).shift().alt())
      .boolean("toggle", true);
  /* Seleccion. */
  item(km,
       "mesh.edgering_select",
       ev(params.select_mouse, params.select_mouse_value).ctrl().alt());
  item(km,
       "mesh.edgering_select",
       ev(params.select_mouse, params.select_mouse_value).shift().ctrl().alt())
      .boolean("toggle", true);
  item(km,
       "mesh.shortest_path_pick",
       ev(params.select_mouse, params.select_mouse_value_fallback).ctrl())
      .boolean("use_fill", false);
  item(km,
       "mesh.shortest_path_pick",
       ev(params.select_mouse, params.select_mouse_value_fallback).shift().ctrl())
      .boolean("use_fill", true);
  template_items_select_actions(km, params, "mesh.select_all");
  item(km, "mesh.select_more", ev("NUMPAD_PLUS", "PRESS").ctrl().repeat());
  item(km, "mesh.select_less", ev("NUMPAD_MINUS", "PRESS").ctrl().repeat());
  item(km, "mesh.select_next_item", ev("NUMPAD_PLUS", "PRESS").shift().ctrl().repeat());
  item(km, "mesh.select_prev_item", ev("NUMPAD_MINUS", "PRESS").shift().ctrl().repeat());
  item(km, "mesh.select_linked", ev("L", "PRESS").ctrl());
  item(km, "mesh.select_linked_pick", ev("L", "PRESS")).boolean("deselect", false);
  item(km, "mesh.select_linked_pick", ev("L", "PRESS").shift()).boolean("deselect", true);
  item(km, "mesh.select_mirror", ev("M", "PRESS").shift().ctrl());
  item_menu(km, "VIEW3D_MT_edit_mesh_select_similar", ev("G", "PRESS").shift());

  /* `_template_items_hide_reveal_actions("mesh.hide", "mesh.reveal")`. */
  item(km, "mesh.reveal", ev("H", "PRESS").alt());
  item(km, "mesh.hide", ev("H", "PRESS")).boolean("unselected", false);
  item(km, "mesh.hide", ev("H", "PRESS").shift()).boolean("unselected", true);

  /* Herramientas. */
  {
    /* `"ctrl" if params.legacy else "shift"`: el modificador cambia con el keymap
     * heredado, la tecla no. */
    Event e("N", "PRESS");
    if (params.legacy) {
      e.ctrl();
    }
    else {
      e.shift();
    }
    item(km, "mesh.normals_make_consistent", e).boolean("inside", false);
  }
  item(km, "mesh.normals_make_consistent", ev("N", "PRESS").shift().ctrl())
      .boolean("inside", true);
  tool_optional(km,
                params,
                "view3d.edit_mesh_extrude_move_normal",
                "builtin.extrude_region",
                ev("E", "PRESS"));
  item_menu(km, "VIEW3D_MT_edit_mesh_extrude", ev("E", "PRESS").alt());
  item(km, "transform.edge_crease", ev("E", "PRESS").shift());
  item(km, "mesh.fill", ev("F", "PRESS").alt());
  item(km, "mesh.quads_convert_to_tris", ev("T", "PRESS").ctrl())
      .enum_("quad_method", "BEAUTY")
      .enum_("ngon_method", "BEAUTY");
  item(km, "mesh.quads_convert_to_tris", ev("T", "PRESS").shift().ctrl())
      .enum_("quad_method", "FIXED")
      .enum_("ngon_method", "CLIP");
  item(km, "mesh.tris_convert_to_quads", ev("J", "PRESS").alt());
  if (params.use_key_activate_tools) {
    item_tool(km, "builtin.rip_region", ev("V", "PRESS")).boolean("cycle", true);
  }
  else {
    item(km, "mesh.rip_move", ev("V", "PRESS")).sub("MESH_OT_rip").boolean("use_fill", false);
  }
  /* Para esto no hay herramienta equivalente. `use_fill` es justo lo que distingue
   * este atajo del anterior. */
  item(km, "mesh.rip_move", ev("V", "PRESS").alt()).sub("MESH_OT_rip").boolean("use_fill", true);
  item(km, "mesh.rip_edge_move", ev("D", "PRESS").alt());
  item_menu(km, "VIEW3D_MT_edit_mesh_merge", ev("M", "PRESS"));
  item_menu(km, "VIEW3D_MT_edit_mesh_split", ev("M", "PRESS").alt());
  item(km, "mesh.edge_face_add", ev("F", "PRESS").repeat());
  item(km, "mesh.duplicate_move", ev("D", "PRESS").shift());
  item_menu(km, "VIEW3D_MT_mesh_add", ev("A", "PRESS").shift());
  item(km, "mesh.separate", ev("P", "PRESS"));
  item(km, "mesh.split", ev("Y", "PRESS"));
  item(km, "mesh.vert_connect_path", ev("J", "PRESS"));
  item(km, "mesh.point_normals", ev("L", "PRESS").alt());
  tool_optional(
      km, params, "transform.vert_slide", "builtin.vertex_slide", ev("V", "PRESS").shift());
  item(km, "mesh.dupli_extrude_cursor", ev(params.action_mouse, "CLICK").ctrl())
      .boolean("rotate_source", true);
  item(km, "mesh.dupli_extrude_cursor", ev(params.action_mouse, "CLICK").shift().ctrl())
      .boolean("rotate_source", false);
  item_menu(km, "VIEW3D_MT_edit_mesh_delete", ev("X", "PRESS"));
  item_menu(km, "VIEW3D_MT_edit_mesh_delete", ev("DEL", "PRESS"));
  item(km, "mesh.dissolve_mode", ev("X", "PRESS").ctrl());
  item(km, "mesh.dissolve_mode", ev("DEL", "PRESS").ctrl());
  tool_optional(km, params, "mesh.knife_tool", "builtin.knife", ev("K", "PRESS"));
  item(km, "mesh.knife_tool", ev("K", "PRESS").shift())
      .boolean("use_occlude_geometry", false)
      .boolean("only_selected", true);
  item(km, "object.vertex_parent_set", ev("P", "PRESS").ctrl());

  /* Menus. */
  item_menu(km, "VIEW3D_MT_edit_mesh_faces", ev("F", "PRESS").ctrl());
  item_menu(km, "VIEW3D_MT_edit_mesh_edges", ev("E", "PRESS").ctrl());
  item_menu(km, "VIEW3D_MT_edit_mesh_vertices", ev("V", "PRESS").ctrl());
  item_menu(km, "VIEW3D_MT_hook", ev("H", "PRESS").ctrl());
  item_menu(km, "VIEW3D_MT_uv_map", ev("U", "PRESS"));
  item_menu(km, "VIEW3D_MT_vertex_group", ev("G", "PRESS").ctrl());
  item_menu(km, "VIEW3D_MT_edit_mesh_normals", ev("N", "PRESS").alt());
  item(km, "object.vertex_group_remove_from", ev("G", "PRESS").ctrl().alt());

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

  /* `_template_items_context_menu("VIEW3D_MT_edit_mesh_context_menu",
   * params.context_menu_event)`: el evento configurado y la tecla de menu contextual. */
  item_menu(km, "VIEW3D_MT_edit_mesh_context_menu", params.context_menu_event);
  item_menu(km, "VIEW3D_MT_edit_mesh_context_menu", ev("APP", "PRESS"));

  /* `params.select_mouse == 'LEFTMOUSE'` se comprueba con `!select_mouse_right`. Con
   * emulacion de tres botones Alt+LMB es el boton central, asi que la seleccion de
   * bucle necesita ademas el doble clic. */
  if (params.use_mouse_emulate_3_button && !params.select_mouse_right) {
    item(km, "mesh.loop_select", ev(params.select_mouse, "DOUBLE_CLICK"));
    item(km, "mesh.loop_select", ev(params.select_mouse, "DOUBLE_CLICK").shift())
        .boolean("extend", true);
    item(km, "mesh.loop_select", ev(params.select_mouse, "DOUBLE_CLICK").alt())
        .boolean("deselect", true);
    item(km, "mesh.edgering_select", ev(params.select_mouse, "DOUBLE_CLICK").ctrl());
    item(km, "mesh.edgering_select", ev(params.select_mouse, "DOUBLE_CLICK").shift().ctrl())
        .boolean("toggle", true);
  }

  if (params.legacy) {
    item(km, "mesh.poke", ev("P", "PRESS").alt());
    item(km, "mesh.select_non_manifold", ev("M", "PRESS").shift().ctrl().alt());
    item(km, "mesh.faces_select_linked_flat", ev("F", "PRESS").shift().ctrl().alt());
    item(km, "mesh.spin", ev("R", "PRESS").alt());
    item(km, "mesh.beautify_fill", ev("F", "PRESS").shift().alt());
    /* `_template_items_object_subdivision_set()`: NUMBERS_0 (orden numerico), Ctrl. */
    static const char *const number_keys_0[6] = {
        "ZERO", "ONE", "TWO", "THREE", "FOUR", "FIVE"};
    for (int i = 0; i < 6; i++) {
      item(km, "object.subdivision_set", ev(number_keys_0[i], "PRESS").ctrl())
          .integer("level", i)
          .boolean("relative", false);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Modo edicion de esqueleto
 * \{ */

static void km_edit_armature(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Armature", "EMPTY", "WINDOW");

  /* `_template_items_transform_actions(params, use_mirror=True)`. */
  tool_optional(km, params, "transform.translate", "builtin.move", ev("G", "PRESS"));
  tool_optional(km, params, "transform.rotate", "builtin.rotate", ev("R", "PRESS"));
  tool_optional(km, params, "transform.resize", "builtin.scale", ev("S", "PRESS"));
  item(km, "transform.translate", ev(params.select_mouse, "CLICK_DRAG"));
  item(km, "transform.mirror", ev("M", "PRESS").ctrl());

  /* `_template_items_hide_reveal_actions("armature.hide", "armature.reveal")`. */
  item(km, "armature.reveal", ev("H", "PRESS").alt());
  item(km, "armature.hide", ev("H", "PRESS")).boolean("unselected", false);
  item(km, "armature.hide", ev("H", "PRESS").shift()).boolean("unselected", true);

  /* Alinear y giro sobre el eje. */
  item(km, "armature.align", ev("A", "PRESS").ctrl().alt());
  {
    /* `"ctrl" if params.legacy else "shift"`, igual que en la malla. */
    Event e("N", "PRESS");
    if (params.legacy) {
      e.ctrl();
    }
    else {
      e.shift();
    }
    item(km, "armature.calculate_roll", e);
  }
  item(km, "armature.roll_clear", ev("R", "PRESS").alt());
  item(km, "armature.switch_direction", ev("F", "PRESS").alt());
  /* Anadir. */
  item(km, "armature.bone_primitive_add", ev("A", "PRESS").shift());
  /* Emparentar. */
  item(km, "armature.parent_set", ev("P", "PRESS").ctrl());
  item(km, "armature.parent_clear", ev("P", "PRESS").alt());
  /* Seleccion. */
  template_items_select_actions(km, params, "armature.select_all");
  item(km, "armature.select_mirror", ev("M", "PRESS").shift().ctrl())
      .boolean("extend", false);
  item(km, "armature.select_hierarchy", ev("LEFT_BRACKET", "PRESS"))
      .enum_("direction", "PARENT")
      .boolean("extend", false);
  item(km, "armature.select_hierarchy", ev("LEFT_BRACKET", "PRESS").shift())
      .enum_("direction", "PARENT")
      .boolean("extend", true);
  item(km, "armature.select_hierarchy", ev("RIGHT_BRACKET", "PRESS"))
      .enum_("direction", "CHILD")
      .boolean("extend", false);
  item(km, "armature.select_hierarchy", ev("RIGHT_BRACKET", "PRESS").shift())
      .enum_("direction", "CHILD")
      .boolean("extend", true);
  item(km, "armature.select_more", ev("NUMPAD_PLUS", "PRESS").ctrl().repeat());
  item(km, "armature.select_less", ev("NUMPAD_MINUS", "PRESS").ctrl().repeat());
  item(km, "armature.select_similar", ev("G", "PRESS").shift());
  item(km, "armature.select_linked_pick", ev("L", "PRESS")).boolean("deselect", false);
  item(km, "armature.select_linked_pick", ev("L", "PRESS").shift()).boolean("deselect", true);
  item(km, "armature.select_linked", ev("L", "PRESS").ctrl());
  item(km,
       "armature.shortest_path_pick",
       ev(params.select_mouse, params.select_mouse_value_fallback).ctrl());
  /* Edicion. */
  item_menu(km, "VIEW3D_MT_edit_armature_delete", ev("X", "PRESS"));
  item_menu(km, "VIEW3D_MT_edit_armature_delete", ev("DEL", "PRESS"));
  item(km, "armature.duplicate_move", ev("D", "PRESS").shift());
  item(km, "armature.dissolve", ev("X", "PRESS").ctrl());
  item(km, "armature.dissolve", ev("DEL", "PRESS").ctrl());
  tool_optional(km, params, "armature.extrude_move", "builtin.extrude", ev("E", "PRESS"));
  item(km, "armature.extrude_forked", ev("E", "PRESS").shift());
  item(km, "armature.click_extrude", ev(params.action_mouse, "CLICK").ctrl());
  item(km, "armature.fill", ev("F", "PRESS"));
  item(km, "armature.split", ev("Y", "PRESS"));
  item(km, "armature.separate", ev("P", "PRESS"));
  /* Banderas del hueso. */
  item_menu(km, "VIEW3D_MT_bone_options_toggle", ev("W", "PRESS").shift());
  item_menu(km, "VIEW3D_MT_bone_options_enable", ev("W", "PRESS").shift().ctrl());
  item_menu(km, "VIEW3D_MT_bone_options_disable", ev("W", "PRESS").alt());
  /* Colecciones de huesos. */
  item(km, "armature.collection_show_all", ev("ACCENT_GRAVE", "PRESS").ctrl());
  item(km, "armature.assign_to_collection", ev("M", "PRESS").shift());
  item(km, "armature.move_to_collection", ev("M", "PRESS"));
  /* Transformaciones propias del esqueleto. */
  tool_optional(km,
                params,
                "transform.bbone_resize",
                "builtin.bone_size",
                ev("S", "PRESS").shift().ctrl().alt());
  if (params.use_key_activate_tools) {
    item_tool(km, "builtin.bone_envelope", ev("S", "PRESS").alt()).boolean("cycle", true);
  }
  else {
    item(km, "transform.transform", ev("S", "PRESS").alt()).enum_("mode", "BONE_ENVELOPE");
  }
  if (params.use_key_activate_tools) {
    item_tool(km, "builtin.roll", ev("R", "PRESS").ctrl()).boolean("cycle", true);
  }
  else {
    item(km, "transform.transform", ev("R", "PRESS").ctrl()).enum_("mode", "BONE_ROLL");
  }
  /* Menus. `_template_items_context_menu("VIEW3D_MT_armature_context_menu",
   * params.context_menu_event)`. */
  item_menu(km, "VIEW3D_MT_armature_context_menu", params.context_menu_event);
  item_menu(km, "VIEW3D_MT_armature_context_menu", ev("APP", "PRESS"));
}

/** \} */

void register_group_17(wmKeyConfig *kc, const Params &params)
{
  km_edit_mesh(kc, params);
  km_edit_armature(kc, params);
}

}  // namespace flipendo::keymap
