/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Mapa de teclado, grupo 03: Outliner, editor de UV y la parte generica de la
 * vista 3D (todas sus regiones).
 * Transliterado de blender_default.py.
 */

#include <cstring>

#include "FL_keymap_build.hpp"
#include "FL_keymap_params.hpp"

namespace flipendo::keymap {

/* Teclado fisico, fila de numeros: `NUMBERS_1` del Python. */
static const char *const NUMBERS_1[10] = {
    "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE", "ZERO"};

/* -------------------------------------------------------------------- */
/** \name Outliner
 * \{ */

static void km_outliner(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Outliner", "OUTLINER", "WINDOW");

  item(km, "outliner.highlight_update", ev("MOUSEMOVE", "ANY").any());
  item(km, "outliner.item_rename", ev("LEFTMOUSE", "DOUBLE_CLICK"));
  item(km, "outliner.item_rename", ev("F2", "PRESS")).boolean("use_active", true);
  item(km, "outliner.item_activate", ev("LEFTMOUSE", "CLICK")).boolean("deselect_all", !params.legacy);
  item(km, "outliner.item_activate", ev("LEFTMOUSE", "CLICK").ctrl())
      .boolean("extend", true)
      .boolean("deselect_all", !params.legacy);
  item(km, "outliner.item_activate", ev("LEFTMOUSE", "CLICK").shift())
      .boolean("extend_range", true)
      .boolean("deselect_all", !params.legacy);
  item(km, "outliner.item_activate", ev("LEFTMOUSE", "CLICK").ctrl().shift())
      .boolean("extend", true)
      .boolean("extend_range", true)
      .boolean("deselect_all", !params.legacy);
  item(km, "outliner.item_activate", ev("LEFTMOUSE", "DOUBLE_CLICK"))
      .boolean("recurse", true)
      .boolean("deselect_all", true);
  item(km, "outliner.item_activate", ev("LEFTMOUSE", "DOUBLE_CLICK").ctrl())
      .boolean("recurse", true)
      .boolean("extend", true)
      .boolean("deselect_all", true);
  item(km, "outliner.item_activate", ev("LEFTMOUSE", "DOUBLE_CLICK").shift())
      .boolean("recurse", true)
      .boolean("extend_range", true)
      .boolean("deselect_all", true);
  item(km, "outliner.item_activate", ev("LEFTMOUSE", "DOUBLE_CLICK").ctrl().shift())
      .boolean("recurse", true)
      .boolean("extend", true)
      .boolean("extend_range", true)
      .boolean("deselect_all", true);
  item(km, "outliner.select_box", ev("B", "PRESS"));
  item(km, "outliner.select_box", ev("LEFTMOUSE", "CLICK_DRAG")).boolean("tweak", true);
  item(km, "outliner.select_box", ev("LEFTMOUSE", "CLICK_DRAG").shift())
      .boolean("tweak", true)
      .enum_("mode", "ADD");
  item(km, "outliner.select_box", ev("LEFTMOUSE", "CLICK_DRAG").ctrl())
      .boolean("tweak", true)
      .enum_("mode", "SUB");
  item(km, "outliner.select_walk", ev("UP_ARROW", "PRESS").repeat()).enum_("direction", "UP");
  item(km, "outliner.select_walk", ev("UP_ARROW", "PRESS").shift().repeat())
      .enum_("direction", "UP")
      .boolean("extend", true);
  item(km, "outliner.select_walk", ev("DOWN_ARROW", "PRESS").repeat()).enum_("direction", "DOWN");
  item(km, "outliner.select_walk", ev("DOWN_ARROW", "PRESS").shift().repeat())
      .enum_("direction", "DOWN")
      .boolean("extend", true);
  item(km, "outliner.select_walk", ev("LEFT_ARROW", "PRESS").repeat()).enum_("direction", "LEFT");
  item(km, "outliner.select_walk", ev("LEFT_ARROW", "PRESS").shift().repeat())
      .enum_("direction", "LEFT")
      .boolean("toggle_all", true);
  item(km, "outliner.select_walk", ev("RIGHT_ARROW", "PRESS").repeat()).enum_("direction", "RIGHT");
  item(km, "outliner.select_walk", ev("RIGHT_ARROW", "PRESS").shift().repeat())
      .enum_("direction", "RIGHT")
      .boolean("toggle_all", true);
  item(km, "outliner.item_openclose", ev("LEFTMOUSE", "CLICK")).boolean("all", false);
  item(km, "outliner.item_openclose", ev("LEFTMOUSE", "CLICK").shift()).boolean("all", true);
  item(km, "outliner.item_openclose", ev("LEFTMOUSE", "CLICK_DRAG")).boolean("all", false);
  /* Cae al menu contextual generico si lo seleccionado no tiene acciones propias. */
  item(km, "outliner.operation", ev("RIGHTMOUSE", "PRESS"));
  item_menu(km, "OUTLINER_MT_context_menu", ev("RIGHTMOUSE", "PRESS"));
  item_menu_pie(km, "OUTLINER_MT_view_pie", ev("ACCENT_GRAVE", "PRESS"));
  item(km, "outliner.item_drag_drop", ev("LEFTMOUSE", "CLICK_DRAG"));
  item(km, "outliner.item_drag_drop", ev("LEFTMOUSE", "CLICK_DRAG").shift());
  item(km, "outliner.show_hierarchy", ev("HOME", "PRESS"));
  item(km, "outliner.show_active", ev("PERIOD", "PRESS"));
  item(km, "outliner.show_active", ev("NUMPAD_PERIOD", "PRESS"));
  item(km, "outliner.scroll_page", ev("PAGE_DOWN", "PRESS").repeat()).boolean("up", false);
  item(km, "outliner.scroll_page", ev("PAGE_UP", "PRESS").repeat()).boolean("up", true);
  item(km, "outliner.show_one_level", ev("NUMPAD_PLUS", "PRESS"));
  item(km, "outliner.show_one_level", ev("NUMPAD_MINUS", "PRESS")).boolean("open", false);

  /* _template_items_select_actions(params, "outliner.select_all") */
  if (!params.use_select_all_toggle) {
    item(km, "outliner.select_all", ev("A", "PRESS")).enum_("action", "SELECT");
    item(km, "outliner.select_all", ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, "outliner.select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
    item(km, "outliner.select_all", ev("A", "DOUBLE_CLICK")).enum_("action", "DESELECT");
  }
  else if (params.legacy) {
    /* En el keymap heredado Alt-A es la reproduccion. */
    item(km, "outliner.select_all", ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, "outliner.select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }
  else {
    item(km, "outliner.select_all", ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, "outliner.select_all", ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, "outliner.select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }

  item(km, "outliner.expanded_toggle", ev("A", "PRESS").shift());
  item(km, "outliner.keyingset_add_selected", ev("K", "PRESS"));
  item(km, "outliner.keyingset_remove_selected", ev("K", "PRESS").alt());
  item(km, "anim.keyframe_insert", ev("I", "PRESS"));
  item(km, "anim.keyframe_delete", ev("I", "PRESS").alt());
  item(km, "outliner.drivers_add_selected", ev("D", "PRESS").ctrl());
  item(km, "outliner.drivers_delete_selected", ev("D", "PRESS").ctrl().alt());
  item(km, "outliner.collection_new", ev("C", "PRESS"));
  item(km, "outliner.delete", ev("X", "PRESS"));
  item(km, "outliner.delete", ev("DEL", "PRESS"));
  item(km, "object.move_to_collection", ev("M", "PRESS"));
  item(km, "object.link_to_collection", ev("M", "PRESS").shift());
  item(km, "outliner.collection_exclude_set", ev("E", "PRESS"));
  item(km, "outliner.collection_exclude_clear", ev("E", "PRESS").alt());
  item(km, "outliner.hide", ev("H", "PRESS"));
  item(km, "outliner.unhide_all", ev("H", "PRESS").alt());
  item(km, "outliner.start_filter", ev("F", "PRESS").ctrl());
  item(km, "outliner.clear_filter", ev("F", "PRESS").alt());
  /* Copiar/pegar. */
  item(km, "outliner.id_copy", ev("C", "PRESS").ctrl());
  item(km, "outliner.id_paste", ev("V", "PRESS").ctrl());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Editor de UV
 * \{ */

static void km_uv_editor(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "UV Editor", "EMPTY", "WINDOW");

  /* --- Modos de seleccion: _template_items_uv_select_mode(params) --- */
  if (params.legacy) {
    item_menu(km, "IMAGE_MT_uvs_select_mode", ev("TAB", "PRESS").ctrl());
  }
  else {
    /* El Python deja aqui un TODO: el menu de modo se colo en el keymap nuevo y se
     * conserva por compatibilidad. */
    item_menu(km, "IMAGE_MT_uvs_select_mode", ev("TAB", "PRESS").ctrl());

    /* _template_items_editmode_mesh_select_mode(params), rama no heredada: el bucle
     * triple del Python (expandir x extender x modo). El orden importa, asi que se
     * respeta el anidamiento original. */
    static const char *const mesh_select_types[3] = {"VERT", "EDGE", "FACE"};
    for (int expand = 0; expand < 2; expand++) {
      for (int extend = 0; extend < 2; extend++) {
        for (int i = 0; i < 3; i++) {
          Event event = ev(NUMBERS_1[i], "PRESS");
          if (expand) {
            event.ctrl();
          }
          if (extend) {
            event.shift();
          }
          Item kmi = item(km, "mesh.select_mode", event);
          if (extend) {
            kmi.boolean("use_extend", true);
          }
          if (expand) {
            kmi.boolean("use_expand", true);
          }
          kmi.enum_("type", mesh_select_types[i]);
        }
      }
    }

    /* Truco del Python para que 4 no caiga al keymap de malla cuando la seleccion
     * sincronizada esta desactivada (y el boton de isla no se ve). */
    item(km, "mesh.select_mode", ev("FOUR", "PRESS"));

    static const char *const uv_select_types[4] = {"VERTEX", "EDGE", "FACE", "ISLAND"};
    for (int i = 0; i < 4; i++) {
      item(km, "uv.select_mode", ev(NUMBERS_1[i], "PRESS")).enum_("type", uv_select_types[i]);
    }
  }

  /* --- _template_uv_select(...) --- */
  {
    bool select_passthrough = params.use_tweak_select_passthrough;
    /* Ver la documentacion de `use_tweak_select_passthrough`: con CLICK o RELEASE no
     * hay nada por lo que dejar pasar el evento. */
    if (select_passthrough && (std::strcmp(params.select_mouse_value_fallback, "CLICK") == 0 ||
                               std::strcmp(params.select_mouse_value_fallback, "RELEASE") == 0))
    {
      select_passthrough = false;
    }

    Item kmi = item(km,
                    "uv.select",
                    ev(params.select_mouse, params.select_mouse_value_fallback));
    if (!params.legacy) {
      kmi.boolean("deselect_all", true);
    }
    if (select_passthrough) {
      kmi.boolean("select_passthrough", true);
    }
    item(km,
         "uv.select",
         ev(params.select_mouse, params.select_mouse_value_fallback).shift())
        .boolean("toggle", true);

    if (select_passthrough) {
      /* Hace falta un CLICK extra para poder deseleccionar el resto cuando el
       * evento se deja pasar. */
      item(km, "uv.select", ev(params.select_mouse, "CLICK")).boolean("deselect_all", true);
    }
  }

  item(km, "uv.mark_seam", ev("E", "PRESS").ctrl());
  item(km, "uv.select_loop", ev(params.select_mouse, params.select_mouse_value).alt());
  item(km, "uv.select_loop", ev(params.select_mouse, params.select_mouse_value).shift().alt())
      .boolean("extend", true);
  item(km, "uv.select_edge_ring", ev(params.select_mouse, params.select_mouse_value).ctrl().alt());
  item(km,
       "uv.select_edge_ring",
       ev(params.select_mouse, params.select_mouse_value).ctrl().shift().alt())
      .boolean("extend", true);
  item(km,
       "uv.shortest_path_pick",
       ev(params.select_mouse, params.select_mouse_value_fallback).ctrl())
      .boolean("use_fill", false);
  item(km,
       "uv.shortest_path_pick",
       ev(params.select_mouse, params.select_mouse_value_fallback).ctrl().shift())
      .boolean("use_fill", true);
  item(km, "uv.select_split", ev("Y", "PRESS"));

  /* op_tool_optional: con `use_key_activate_tools` la tecla activa la herramienta en
   * vez de lanzar el operador. */
  if (params.use_key_activate_tools) {
    item_tool(km, "builtin.select_box", ev("B", "PRESS"));
  }
  else {
    item(km, "uv.select_box", ev("B", "PRESS")).boolean("pinned", false);
  }
  item(km, "uv.select_box", ev("B", "PRESS").ctrl()).boolean("pinned", true);
  if (params.use_key_activate_tools) {
    item_tool(km, "builtin.select_circle", ev("C", "PRESS"));
  }
  else {
    item(km, "uv.select_circle", ev("C", "PRESS"));
  }

  item(km, "uv.select_lasso", ev(params.action_mouse, "CLICK_DRAG").ctrl()).enum_("mode", "ADD");
  item(km, "uv.select_lasso", ev(params.action_mouse, "CLICK_DRAG").shift().ctrl())
      .enum_("mode", "SUB");
  item(km, "uv.select_linked", ev("L", "PRESS").ctrl());
  item(km, "uv.select_linked_pick", ev("L", "PRESS"))
      .boolean("extend", true)
      .boolean("deselect", false);
  item(km, "uv.select_linked_pick", ev("L", "PRESS").shift()).boolean("deselect", true);
  item(km, "uv.select_more", ev("NUMPAD_PLUS", "PRESS").ctrl().repeat());
  item(km, "uv.select_less", ev("NUMPAD_MINUS", "PRESS").ctrl().repeat());
  item(km, "uv.select_similar", ev("G", "PRESS").shift());

  /* _template_items_select_actions(params, "uv.select_all") */
  if (!params.use_select_all_toggle) {
    item(km, "uv.select_all", ev("A", "PRESS")).enum_("action", "SELECT");
    item(km, "uv.select_all", ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, "uv.select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
    item(km, "uv.select_all", ev("A", "DOUBLE_CLICK")).enum_("action", "DESELECT");
  }
  else if (params.legacy) {
    item(km, "uv.select_all", ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, "uv.select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }
  else {
    item(km, "uv.select_all", ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, "uv.select_all", ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, "uv.select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }

  /* _template_items_hide_reveal_actions("uv.hide", "uv.reveal") */
  item(km, "uv.reveal", ev("H", "PRESS").alt());
  item(km, "uv.hide", ev("H", "PRESS")).boolean("unselected", false);
  item(km, "uv.hide", ev("H", "PRESS").shift()).boolean("unselected", true);

  item(km, "uv.select_pinned", ev("P", "PRESS").shift());
  item_menu(km, "IMAGE_MT_uvs_merge", ev("M", "PRESS"));
  item_menu(km, "IMAGE_MT_uvs_split", ev("M", "PRESS").alt());
  item_menu(km, "IMAGE_MT_uvs_align", ev("W", "PRESS").shift());
  item(km, "uv.stitch", ev("V", "PRESS").alt());
  item(km, "uv.rip_move", ev("V", "PRESS"));
  item(km, "uv.pin", ev("P", "PRESS")).boolean("clear", false);
  item(km, "uv.pin", ev("P", "PRESS").alt()).boolean("clear", true);
  item(km, "uv.copy", ev("C", "PRESS").ctrl());
  item(km, "uv.paste", ev("V", "PRESS").ctrl());
  item_menu(km, "IMAGE_MT_uvs_unwrap", ev("U", "PRESS"));
  if (!params.legacy) {
    item_menu_pie(km, "IMAGE_MT_uvs_snap_pie", ev("S", "PRESS").shift());
  }
  else {
    item_menu(km, "IMAGE_MT_uvs_snap", ev("S", "PRESS").shift());
  }

  /* _template_items_proportional_editing(params, connected=False,
   * toggle_data_path="tool_settings.use_proportional_edit") */
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

  /* _template_items_transform_actions(params, use_mirror=True, use_shear=True) */
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
  if (params.use_key_activate_tools) {
    item_tool(km, "builtin.shear", ev("S", "PRESS").shift().ctrl().alt()).boolean("cycle", true);
  }
  else {
    item(km, "transform.shear", ev("S", "PRESS").shift().ctrl().alt());
  }

  item(km, "wm.context_toggle", ev("TAB", "PRESS").shift())
      .string("data_path", "tool_settings.use_snap_uv");
  item(km, "wm.context_menu_enum", ev("TAB", "PRESS").shift().ctrl())
      .string("data_path", "tool_settings.snap_uv_element");
  item(km, "wm.context_toggle", ev("ACCENT_GRAVE", "PRESS").ctrl())
      .string("data_path", "space_data.show_gizmo");
  item(km, "wm.context_toggle", ev("Z", "PRESS").alt().shift())
      .string("data_path", "space_data.overlay.show_overlays");

  /* _template_items_context_menu("IMAGE_MT_uvs_context_menu", params.context_menu_event) */
  item_menu(km, "IMAGE_MT_uvs_context_menu", params.context_menu_event);
  item_menu(km, "IMAGE_MT_uvs_context_menu", ev("APP", "PRESS"));

  /* Reserva para la emulacion del boton central. */
  if (params.use_mouse_emulate_3_button && !params.select_mouse_right) {
    item(km, "uv.select_loop", ev(params.select_mouse, "DOUBLE_CLICK"));
    item(km, "uv.select_loop", ev(params.select_mouse, "DOUBLE_CLICK").alt())
        .boolean("extend", true);
  }

  /* Cursor 2D. */
  if (params.has_cursor_tweak_event) {
    item(km, "uv.cursor_set", params.cursor_set_event);
    item(km, "transform.translate", params.cursor_tweak_event)
        .boolean("release_confirm", true)
        .boolean("cursor_transform", true);
  }
  else {
    item(km, "uv.cursor_set", params.cursor_set_event);
  }

  if (params.legacy) {
    item(km, "uv.minimize_stretch", ev("V", "PRESS").ctrl());
    item(km, "uv.pack_islands", ev("P", "PRESS").ctrl());
    item(km, "uv.average_islands_scale", ev("A", "PRESS").ctrl());
  }

  if (!params.select_mouse_right && !params.legacy) {
    /* Salto rapido a la herramienta de seleccion: con seleccion por el izquierdo no
     * es comodo seleccionar con cualquier otra herramienta activa. */
    item_tool(km, "builtin.select_box", ev("W", "PRESS")).boolean("cycle", true);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vista 3D: todas las regiones
 * \{ */

static void km_view3d_generic(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "3D View Generic", "VIEW_3D", "WINDOW");

  /* _template_space_region_type_toggle(params, toolbar_key={'T'}, sidebar_key={'N'}) */
  if (params.use_region_toggle_pie) {
    /* El Python elige `sidebar_key or sidebar_key or channels_key`; aqui la barra
     * lateral siempre esta dada, asi que la tecla del menu radial es N. */
    item_menu_pie(km, "WM_MT_region_toggle_pie", ev("N", "PRESS"));
  }
  else {
    item(km, "wm.context_toggle", ev("T", "PRESS"))
        .string("data_path", "space_data.show_region_toolbar");
    item(km, "wm.context_toggle", ev("N", "PRESS"))
        .string("data_path", "space_data.show_region_ui");
  }
}

/** \} */

void register_group_03(wmKeyConfig *kc, const Params &params)
{
  km_outliner(kc, params);
  km_uv_editor(kc, params);
  km_view3d_generic(kc, params);
}

}  // namespace flipendo::keymap
