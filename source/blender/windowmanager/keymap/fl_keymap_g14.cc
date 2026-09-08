/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Mapa de teclado, grupo 14: modal de la herramienta de relleno de grease pencil y
 * los modos de objeto (Object Mode, Object Non-modal y Pose).
 * Transliterado de blender_default.py.
 */

#include "FL_keymap_build.hpp"
#include "FL_keymap_params.hpp"

namespace flipendo::keymap {

/* `NUMBERS_1` del Python: la fila de numeros en su orden FISICO (el cero al final). */
static const char *const NUMBERS_1[10] = {
    "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE", "ZERO"};

/* `NUMBERS_0` del Python: la misma fila en orden NUMERICO (el cero primero). */
static const char *const NUMBERS_0[10] = {
    "ZERO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE"};

/* -------------------------------------------------------------------- */
/** \name Modal de la herramienta de relleno (grease pencil)
 * \{ */

static void km_grease_pencil_fill_tool_modal_map(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap_modal(kc, "Fill Tool Modal Map");

  item_modal(km, "CANCEL", ev("ESC", "PRESS").any());
  item_modal(km, "CANCEL", ev("RIGHTMOUSE", "PRESS"));
  item_modal(km, "CONFIRM", ev("LEFTMOUSE", "PRESS").any());
  item_modal(km, "EXTENSION_MODE_TOGGLE", ev("S", "PRESS"));
  item_modal(km, "EXTENSION_LENGTHEN", ev("PAGE_UP", "PRESS").repeat());
  item_modal(km, "EXTENSION_LENGTHEN", ev("WHEELUPMOUSE", "PRESS"));
  item_modal(km, "EXTENSION_SHORTEN", ev("PAGE_DOWN", "PRESS").repeat());
  item_modal(km, "EXTENSION_SHORTEN", ev("WHEELDOWNMOUSE", "PRESS"));
  item_modal(km, "EXTENSION_DRAG", ev("MIDDLEMOUSE", "PRESS"));
  item_modal(km, "EXTENSION_COLLIDE", ev("D", "PRESS"));
  item_modal(km, "INVERT", ev("LEFT_CTRL", "ANY").any());
  item_modal(km, "INVERT", ev("RIGHT_CTRL", "ANY").any());
  item_modal(km, "PRECISION", ev("LEFT_SHIFT", "ANY").any());
  item_modal(km, "PRECISION", ev("RIGHT_SHIFT", "ANY").any());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Modo objeto y modo pose
 * \{ */

static void km_object_mode(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Object Mode", "EMPTY", "WINDOW");

  /* _template_items_proportional_editing(params, connected=False,
   * toggle_data_path="tool_settings.use_proportional_edit_objects") */
  if (!params.legacy) {
    item_menu_pie(km, "VIEW3D_MT_proportional_editing_falloff_pie", ev("O", "PRESS").shift());
  }
  else {
    item(km, "wm.context_cycle_enum", ev("O", "PRESS").shift())
        .string("data_path", "tool_settings.proportional_edit_falloff")
        .boolean("wrap", true);
  }
  item(km, "wm.context_toggle", ev("O", "PRESS"))
      .string("data_path", "tool_settings.use_proportional_edit_objects");

  /* _template_items_select_actions(params, "object.select_all") */
  if (!params.use_select_all_toggle) {
    item(km, "object.select_all", ev("A", "PRESS")).enum_("action", "SELECT");
    item(km, "object.select_all", ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, "object.select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
    item(km, "object.select_all", ev("A", "DOUBLE_CLICK")).enum_("action", "DESELECT");
  }
  else if (params.legacy) {
    /* En el keymap heredado Alt-A es reproducir, asi que ahi no hay "deseleccionar". */
    item(km, "object.select_all", ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, "object.select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }
  else {
    item(km, "object.select_all", ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, "object.select_all", ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, "object.select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }

  item(km, "object.select_more", ev("NUMPAD_PLUS", "PRESS").ctrl().repeat());
  item(km, "object.select_less", ev("NUMPAD_MINUS", "PRESS").ctrl().repeat());
  item(km, "object.select_linked", ev("L", "PRESS").shift());
  item(km, "object.select_grouped", ev("G", "PRESS").shift());
  item(km, "object.select_hierarchy", ev("LEFT_BRACKET", "PRESS").repeat())
      .enum_("direction", "PARENT")
      .boolean("extend", false);
  item(km, "object.select_hierarchy", ev("LEFT_BRACKET", "PRESS").shift().repeat())
      .enum_("direction", "PARENT")
      .boolean("extend", true);
  item(km, "object.select_hierarchy", ev("RIGHT_BRACKET", "PRESS").repeat())
      .enum_("direction", "CHILD")
      .boolean("extend", false);
  item(km, "object.select_hierarchy", ev("RIGHT_BRACKET", "PRESS").shift().repeat())
      .enum_("direction", "CHILD")
      .boolean("extend", true);
  item(km, "object.parent_set", ev("P", "PRESS").ctrl());
  item(km, "object.parent_clear", ev("P", "PRESS").alt());

  /* Transform Actions.
   * _template_items_transform_actions(params, use_mirror=True). El `op_tool_optional`
   * elige entre lanzar el operador o activar la herramienta con la misma tecla. */
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
  item(km, "transform.mirror", ev("M", "PRESS").ctrl());

  item(km, "object.transform_axis_target", ev("T", "PRESS").shift());
  item(km, "object.location_clear", ev("G", "PRESS").alt()).boolean("clear_delta", false);
  item(km, "object.rotation_clear", ev("R", "PRESS").alt()).boolean("clear_delta", false);
  item(km, "object.scale_clear", ev("S", "PRESS").alt()).boolean("clear_delta", false);
  item(km, "object.delete", ev("X", "PRESS")).boolean("use_global", false);
  item(km, "object.delete", ev("X", "PRESS").shift()).boolean("use_global", true);
  item(km, "object.delete", ev("DEL", "PRESS"))
      .boolean("use_global", false)
      .boolean("confirm", false);
  item(km, "object.delete", ev("DEL", "PRESS").shift())
      .boolean("use_global", true)
      .boolean("confirm", false);
  item_menu(km, "VIEW3D_MT_add", ev("A", "PRESS").shift());
  item_menu(km, "VIEW3D_MT_object_apply", ev("A", "PRESS").ctrl());
  item_menu(km, "VIEW3D_MT_make_links", ev("L", "PRESS").ctrl());
  item(km, "object.duplicate_move", ev("D", "PRESS").shift());
  item(km, "object.duplicate_move_linked", ev("D", "PRESS").alt());
  item(km, "object.join", ev("J", "PRESS").ctrl());
  item(km, "wm.context_toggle", ev("PERIOD", "PRESS").ctrl())
      .string("data_path", "tool_settings.use_transform_data_origin");
  item(km, "anim.keyframe_insert_menu", ev("K", "PRESS")).boolean("always_prompt", true);
  item(km, "anim.keyframe_delete_v3d", ev("I", "PRESS").alt());
  item(km, "anim.keying_set_active_set", ev("K", "PRESS").shift());
  item(km, "collection.create", ev("G", "PRESS").ctrl());
  item(km, "collection.objects_remove", ev("G", "PRESS").ctrl().alt());
  item(km, "collection.objects_remove_all", ev("G", "PRESS").shift().ctrl().alt());
  item(km, "collection.objects_add_active", ev("G", "PRESS").shift().ctrl());
  item(km, "collection.objects_remove_active", ev("G", "PRESS").shift().alt());

  /* _template_items_object_subdivision_set(): Ctrl-0 .. Ctrl-5, en orden numerico. */
  for (int i = 0; i < 6; i++) {
    item(km, "object.subdivision_set", ev(NUMBERS_0[i], "PRESS").ctrl())
        .integer("level", i)
        .boolean("relative", false);
  }

  item(km, "object.move_to_collection", ev("M", "PRESS"));
  item(km, "object.link_to_collection", ev("M", "PRESS").shift());

  /* _template_items_hide_reveal_actions("object.hide_view_set", "object.hide_view_clear") */
  item(km, "object.hide_view_clear", ev("H", "PRESS").alt());
  item(km, "object.hide_view_set", ev("H", "PRESS")).boolean("unselected", false);
  item(km, "object.hide_view_set", ev("H", "PRESS").shift()).boolean("unselected", true);

  item(km, "object.hide_collection", ev("H", "PRESS").ctrl());

  /* _template_object_hide_collection_from_number_keys(): el triple bucle del Python.
   * El anidamiento se respeta porque fija el orden de los 40 atajos. */
  for (int extend = 0; extend < 2; extend++) {
    for (int add_10 = 0; add_10 < 2; add_10++) {
      for (int i = 0; i < 10; i++) {
        Event event = ev(NUMBERS_1[i], "PRESS");
        if (extend) {
          event.shift();
        }
        if (add_10) {
          event.alt();
        }
        item(km, "object.hide_collection", event)
            .integer("collection_index", i + (add_10 ? 11 : 1))
            .boolean("extend", extend != 0);
      }
    }
  }

  /* _template_items_context_menu("VIEW3D_MT_object_context_menu", params.context_menu_event) */
  item_menu(km, "VIEW3D_MT_object_context_menu", params.context_menu_event);
  item_menu(km, "VIEW3D_MT_object_context_menu", ev("APP", "PRESS"));

  if (params.use_pie_click_drag) {
    item(km, "anim.keyframe_insert", ev("I", "CLICK"));
    item_menu_pie(km, "ANIM_MT_keyframe_insert_pie", ev("I", "CLICK_DRAG"));
  }
  else {
    item(km, "anim.keyframe_insert", ev("I", "PRESS"));
  }

  if (params.legacy) {
    item(km, "object.select_mirror", ev("M", "PRESS").shift().ctrl());
    item(km, "object.parent_no_inverse_set", ev("P", "PRESS").shift().ctrl());
    item(km, "object.track_set", ev("T", "PRESS").ctrl());
    item(km, "object.track_clear", ev("T", "PRESS").alt());
    item(km, "object.constraint_add_with_targets", ev("C", "PRESS").shift().ctrl());
    item(km, "object.constraints_clear", ev("C", "PRESS").ctrl().alt());
    item(km, "object.origin_clear", ev("O", "PRESS").alt());
    item(km, "object.duplicates_make_real", ev("A", "PRESS").shift().ctrl());
    item_menu(km, "VIEW3D_MT_make_single_user", ev("U", "PRESS"));
    item(km, "object.convert", ev("C", "PRESS").alt());
    item(km, "object.make_local", ev("L", "PRESS"));
    item(km, "object.data_transfer", ev("T", "PRESS").shift().ctrl());
  }
}

static void km_object_non_modal(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Object Non-modal", "EMPTY", "WINDOW");

  if (params.legacy) {
    item(km, "object.mode_set", ev("TAB", "PRESS")).enum_("mode", "EDIT").boolean("toggle", true);
    item(km, "object.mode_set", ev("TAB", "PRESS").ctrl())
        .enum_("mode", "POSE")
        .boolean("toggle", true);
    item(km, "object.mode_set", ev("V", "PRESS"))
        .enum_("mode", "VERTEX_PAINT")
        .boolean("toggle", true);
    /* Repite Ctrl-Tab a proposito, igual que el Python: el segundo no llega a casar. */
    item(km, "object.mode_set", ev("TAB", "PRESS").ctrl())
        .enum_("mode", "WEIGHT_PAINT")
        .boolean("toggle", true);

    item(km, "object.origin_set", ev("C", "PRESS").shift().ctrl().alt());
  }
  else {
    /* NOTA: este atajo (aunque no sea temporal) no es el ideal, ver: #89757. */
    item(km, "object.transfer_mode", ev("Q", "PRESS").alt());

    if (params.use_pie_click_drag) {
      item(km, "object.mode_set", ev("TAB", "CLICK")).enum_("mode", "EDIT").boolean("toggle", true);
      item_menu_pie(km, "VIEW3D_MT_object_mode_pie", ev("TAB", "CLICK_DRAG"));
      item(km, "view3d.object_mode_pie_or_toggle", ev("TAB", "PRESS").ctrl());
    }
    else if (params.use_v3d_tab_menu) {
      /* Intercambia Tab y Ctrl-Tab. */
      item(km, "object.mode_set", ev("TAB", "PRESS").ctrl())
          .enum_("mode", "EDIT")
          .boolean("toggle", true);
      item_menu_pie(km, "VIEW3D_MT_object_mode_pie", ev("TAB", "PRESS"));
    }
    else {
      item(km, "object.mode_set", ev("TAB", "PRESS")).enum_("mode", "EDIT").boolean("toggle", true);
      item(km, "view3d.object_mode_pie_or_toggle", ev("TAB", "PRESS").ctrl());
    }
  }
}

static void km_pose(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Pose", "EMPTY", "WINDOW");

  /* Transform Actions.
   * _template_items_transform_actions(params, use_mirror=True). */
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
  item(km, "transform.mirror", ev("M", "PRESS").ctrl());

  item(km, "object.parent_set", ev("P", "PRESS").ctrl());

  /* _template_items_hide_reveal_actions("pose.hide", "pose.reveal") */
  item(km, "pose.reveal", ev("H", "PRESS").alt());
  item(km, "pose.hide", ev("H", "PRESS")).boolean("unselected", false);
  item(km, "pose.hide", ev("H", "PRESS").shift()).boolean("unselected", true);

  item_menu(km, "VIEW3D_MT_pose_apply", ev("A", "PRESS").ctrl());
  item(km, "pose.rot_clear", ev("R", "PRESS").alt());
  item(km, "pose.loc_clear", ev("G", "PRESS").alt());
  item(km, "pose.scale_clear", ev("S", "PRESS").alt());
  item(km, "pose.quaternions_flip", ev("F", "PRESS").alt());
  item(km, "pose.rotation_mode_set", ev("R", "PRESS").ctrl());
  item(km, "pose.copy", ev("C", "PRESS").ctrl());
  item(km, "pose.paste", ev("V", "PRESS").ctrl()).boolean("flipped", false);
  item(km, "pose.paste", ev("V", "PRESS").shift().ctrl()).boolean("flipped", true);

  /* _template_items_select_actions(params, "pose.select_all") */
  if (!params.use_select_all_toggle) {
    item(km, "pose.select_all", ev("A", "PRESS")).enum_("action", "SELECT");
    item(km, "pose.select_all", ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, "pose.select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
    item(km, "pose.select_all", ev("A", "DOUBLE_CLICK")).enum_("action", "DESELECT");
  }
  else if (params.legacy) {
    item(km, "pose.select_all", ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, "pose.select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }
  else {
    item(km, "pose.select_all", ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, "pose.select_all", ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, "pose.select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }

  item(km, "pose.select_parent", ev("P", "PRESS").shift());
  item(km, "pose.select_hierarchy", ev("LEFT_BRACKET", "PRESS").repeat())
      .enum_("direction", "PARENT")
      .boolean("extend", false);
  item(km, "pose.select_hierarchy", ev("LEFT_BRACKET", "PRESS").shift().repeat())
      .enum_("direction", "PARENT")
      .boolean("extend", true);
  item(km, "pose.select_hierarchy", ev("RIGHT_BRACKET", "PRESS").repeat())
      .enum_("direction", "CHILD")
      .boolean("extend", false);
  item(km, "pose.select_hierarchy", ev("RIGHT_BRACKET", "PRESS").shift().repeat())
      .enum_("direction", "CHILD")
      .boolean("extend", true);
  item(km, "pose.select_linked", ev("L", "PRESS").ctrl());
  item(km, "pose.select_linked_pick", ev("L", "PRESS"));
  item(km, "pose.select_grouped", ev("G", "PRESS").shift());
  item(km, "pose.select_mirror", ev("M", "PRESS").shift().ctrl());
  item(km, "pose.constraint_add_with_targets", ev("C", "PRESS").shift().ctrl());
  item(km, "pose.constraints_clear", ev("C", "PRESS").ctrl().alt());
  item(km, "pose.ik_add", ev("I", "PRESS").shift());
  item(km, "pose.ik_clear", ev("I", "PRESS").ctrl().alt());
  item_menu(km, "VIEW3D_MT_bone_options_toggle", ev("W", "PRESS").shift());
  item_menu(km, "VIEW3D_MT_bone_options_enable", ev("W", "PRESS").shift().ctrl());
  item_menu(km, "VIEW3D_MT_bone_options_disable", ev("W", "PRESS").alt());
  item(km, "armature.collection_show_all", ev("ACCENT_GRAVE", "PRESS").ctrl());
  item(km, "armature.assign_to_collection", ev("M", "PRESS").shift());
  item(km, "armature.move_to_collection", ev("M", "PRESS"));
  item(km, "transform.bbone_resize", ev("S", "PRESS").shift().ctrl().alt());
  item(km, "anim.keyframe_insert_menu", ev("K", "PRESS")).boolean("always_prompt", true);
  item(km, "anim.keyframe_delete_v3d", ev("I", "PRESS").alt());
  item(km, "anim.keying_set_active_set", ev("K", "PRESS").shift());
  item(km, "pose.push", ev("E", "PRESS").ctrl());
  item(km, "pose.relax", ev("E", "PRESS").alt());
  item(km, "pose.breakdown", ev("E", "PRESS").shift());
  item(km, "pose.blend_to_neighbor", ev("E", "PRESS").shift().alt());
  item_menu(km, "VIEW3D_MT_pose_propagate", ev("P", "PRESS").alt());

  /* _template_items_context_menu("VIEW3D_MT_pose_context_menu", params.context_menu_event) */
  item_menu(km, "VIEW3D_MT_pose_context_menu", params.context_menu_event);
  item_menu(km, "VIEW3D_MT_pose_context_menu", ev("APP", "PRESS"));

  item_menu(km, "POSE_MT_selection_sets_select", ev("W", "PRESS").shift().alt());

  if (params.use_pie_click_drag) {
    item(km, "anim.keyframe_insert", ev("I", "CLICK"));
    item_menu_pie(km, "ANIM_MT_keyframe_insert_pie", ev("I", "CLICK_DRAG"));
  }
  else {
    item(km, "anim.keyframe_insert", ev("I", "PRESS"));
  }
}

/** \} */

void register_group_14(wmKeyConfig *kc, const Params &params)
{
  km_grease_pencil_fill_tool_modal_map(kc, params);
  km_object_mode(kc, params);
  km_object_non_modal(kc, params);
  km_pose(kc, params);
}

}  // namespace flipendo::keymap
