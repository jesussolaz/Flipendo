/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Mapa de teclado, grupo 04: la vista 3D (keymap "3D View"): navegacion, vistas del
 * teclado numerico, NDOF, seleccion, bordes, camaras, ajuste (snap) y los menus
 * radiales de pivote/orientacion/sombreado.
 * Transliterado de blender_default.py.
 */

#include <cstring>

#include "FL_keymap_build.hpp"
#include "FL_keymap_params.hpp"

namespace flipendo::keymap {

/* `pi` y `PI_2` de las constantes del Python. Se escriben como literales float para
 * no depender de `M_PI`, que no es estandar. */
static const float PI_F = 3.14159265358979323846f;
static const float PI_2_F = 1.57079632679489661923f;

static bool streq(const char *a, const char *b)
{
  return std::strcmp(a, b) == 0;
}

/* -------------------------------------------------------------------- */
/** \name Vista 3D
 * \{ */

static void km_view3d(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "3D View", "VIEW_3D", "WINDOW");

  /* Cursor 3D. `has_cursor_tweak_event` es el `if params.cursor_tweak_event:` del
   * Python: alli el diccionario vacio es falso. */
  if (params.has_cursor_tweak_event) {
    item(km, "view3d.cursor3d", params.cursor_set_event);
    item(km, "transform.translate", params.cursor_tweak_event)
        .boolean("release_confirm", true)
        .boolean("cursor_transform", true);
  }
  else {
    item(km, "view3d.cursor3d", params.cursor_set_event);
  }

  /* Visibilidad. */
  item(km, "view3d.localview", ev("NUMPAD_SLASH", "PRESS"));
  item(km, "view3d.localview", ev("SLASH", "PRESS"));
  item(km, "view3d.localview", ev("MOUSESMARTZOOM", "ANY"));
  item(km, "view3d.localview_remove_from", ev("NUMPAD_SLASH", "PRESS").alt());
  item(km, "view3d.localview_remove_from", ev("SLASH", "PRESS").alt());

  /* Navegacion. */
  item(km, "view3d.rotate", ev("MOUSEROTATE", "ANY"));
  if (params.use_v3d_mmb_pan) {
    /* Con "MMB desplaza", rotar pasa a Shift-MMB. */
    item(km, "view3d.rotate", ev("MIDDLEMOUSE", "PRESS").shift());
    item(km, "view3d.move", ev("MIDDLEMOUSE", "PRESS"));
    item(km, "view3d.rotate", ev("TRACKPADPAN", "ANY").shift());
    item(km, "view3d.move", ev("TRACKPADPAN", "ANY"));
  }
  else {
    item(km, "view3d.rotate", ev("MIDDLEMOUSE", "PRESS"));
    item(km, "view3d.move", ev("MIDDLEMOUSE", "PRESS").shift());
    item(km, "view3d.rotate", ev("TRACKPADPAN", "ANY"));
    item(km, "view3d.move", ev("TRACKPADPAN", "ANY").shift());
  }
  item(km, "view3d.view_pan", ev("WHEELLEFTMOUSE", "PRESS")).enum_("type", "PANLEFT");
  item(km, "view3d.view_pan", ev("WHEELRIGHTMOUSE", "PRESS")).enum_("type", "PANRIGHT");
  item(km, "view3d.zoom", ev("MIDDLEMOUSE", "PRESS").ctrl());
  item(km, "view3d.dolly", ev("MIDDLEMOUSE", "PRESS").shift().ctrl());
  item(km, "view3d.view_selected", ev("NUMPAD_PERIOD", "PRESS").ctrl())
      .boolean("use_all_regions", true);
  item(km, "view3d.view_selected", ev("NUMPAD_PERIOD", "PRESS"));
  item(km, "view3d.smoothview", ev("TIMER1", "ANY").any());
  item(km, "view3d.zoom", ev("TRACKPADZOOM", "ANY"));
  item(km, "view3d.zoom", ev("TRACKPADPAN", "ANY").ctrl());
  item(km, "view3d.zoom", ev("NUMPAD_PLUS", "PRESS").repeat()).integer("delta", 1);
  item(km, "view3d.zoom", ev("NUMPAD_MINUS", "PRESS").repeat()).integer("delta", -1);
  item(km, "view3d.zoom", ev("EQUAL", "PRESS").ctrl().repeat()).integer("delta", 1);
  item(km, "view3d.zoom", ev("MINUS", "PRESS").ctrl().repeat()).integer("delta", -1);
  item(km, "view3d.zoom", ev("WHEELINMOUSE", "PRESS")).integer("delta", 1);
  item(km, "view3d.zoom", ev("WHEELOUTMOUSE", "PRESS")).integer("delta", -1);
  item(km, "view3d.dolly", ev("NUMPAD_PLUS", "PRESS").shift().repeat()).integer("delta", 1);
  item(km, "view3d.dolly", ev("NUMPAD_MINUS", "PRESS").shift().repeat()).integer("delta", -1);
  item(km, "view3d.dolly", ev("EQUAL", "PRESS").shift().ctrl().repeat()).integer("delta", 1);
  item(km, "view3d.dolly", ev("MINUS", "PRESS").shift().ctrl().repeat()).integer("delta", -1);
  item(km, "view3d.view_center_camera", ev("HOME", "PRESS"));
  item(km, "view3d.view_center_lock", ev("HOME", "PRESS"));
  item(km, "view3d.view_all", ev("HOME", "PRESS")).boolean("center", false);
  item(km, "view3d.view_all", ev("HOME", "PRESS").ctrl())
      .boolean("use_all_regions", true)
      .boolean("center", false);
  item(km, "view3d.view_all", ev("C", "PRESS").shift()).boolean("center", true);
  item_menu_pie(km,
                params.v3d_tilde_action == TildeAction::View ? "VIEW3D_MT_view_pie" :
                                                               "VIEW3D_MT_transform_gizmo_pie",
                ev("ACCENT_GRAVE", params.pie_value));
  if (params.use_pie_click_drag) {
    /* Con el radial en arrastre, el click simple queda libre para la navegacion. */
    item(km, "view3d.navigate", ev("ACCENT_GRAVE", "CLICK"));
  }
  item(km, "view3d.navigate", ev("ACCENT_GRAVE", "PRESS").shift());

  /* Vistas del teclado numerico. */
  item(km, "view3d.view_camera", ev("NUMPAD_0", "PRESS"));
  item(km, "view3d.view_axis", ev("NUMPAD_1", "PRESS")).enum_("type", "FRONT");
  item(km, "view3d.view_orbit", ev("NUMPAD_2", "PRESS").repeat()).enum_("type", "ORBITDOWN");
  item(km, "view3d.view_axis", ev("NUMPAD_3", "PRESS")).enum_("type", "RIGHT");
  item(km, "view3d.view_orbit", ev("NUMPAD_4", "PRESS").repeat()).enum_("type", "ORBITLEFT");
  item(km, "view3d.view_persportho", ev("NUMPAD_5", "PRESS"));
  item(km, "view3d.view_orbit", ev("NUMPAD_6", "PRESS").repeat()).enum_("type", "ORBITRIGHT");
  item(km, "view3d.view_axis", ev("NUMPAD_7", "PRESS")).enum_("type", "TOP");
  item(km, "view3d.view_orbit", ev("NUMPAD_8", "PRESS").repeat()).enum_("type", "ORBITUP");
  item(km, "view3d.view_axis", ev("NUMPAD_1", "PRESS").ctrl()).enum_("type", "BACK");
  item(km, "view3d.view_axis", ev("NUMPAD_3", "PRESS").ctrl()).enum_("type", "LEFT");
  item(km, "view3d.view_axis", ev("NUMPAD_7", "PRESS").ctrl()).enum_("type", "BOTTOM");
  item(km, "view3d.view_pan", ev("NUMPAD_2", "PRESS").ctrl().repeat()).enum_("type", "PANDOWN");
  item(km, "view3d.view_pan", ev("NUMPAD_4", "PRESS").ctrl().repeat()).enum_("type", "PANLEFT");
  item(km, "view3d.view_pan", ev("NUMPAD_6", "PRESS").ctrl().repeat()).enum_("type", "PANRIGHT");
  item(km, "view3d.view_pan", ev("NUMPAD_8", "PRESS").ctrl().repeat()).enum_("type", "PANUP");
  item(km, "view3d.view_roll", ev("NUMPAD_4", "PRESS").shift().repeat()).enum_("type", "LEFT");
  item(km, "view3d.view_roll", ev("NUMPAD_6", "PRESS").shift().repeat()).enum_("type", "RIGHT");
  item(km, "view3d.view_orbit", ev("NUMPAD_9", "PRESS"))
      .number("angle", PI_F)
      .enum_("type", "ORBITRIGHT");
  item(km, "view3d.view_axis", ev("NUMPAD_1", "PRESS").shift())
      .enum_("type", "FRONT")
      .boolean("align_active", true);
  item(km, "view3d.view_axis", ev("NUMPAD_3", "PRESS").shift())
      .enum_("type", "RIGHT")
      .boolean("align_active", true);
  item(km, "view3d.view_axis", ev("NUMPAD_7", "PRESS").shift())
      .enum_("type", "TOP")
      .boolean("align_active", true);
  item(km, "view3d.view_axis", ev("NUMPAD_1", "PRESS").shift().ctrl())
      .enum_("type", "BACK")
      .boolean("align_active", true);
  item(km, "view3d.view_axis", ev("NUMPAD_3", "PRESS").shift().ctrl())
      .enum_("type", "LEFT")
      .boolean("align_active", true);
  item(km, "view3d.view_axis", ev("NUMPAD_7", "PRESS").shift().ctrl())
      .enum_("type", "BOTTOM")
      .boolean("align_active", true);

  if (params.v3d_alt_mmb_drag_action == AltMmbDragAction::Relative) {
    item(km, "view3d.view_axis", ev("MIDDLEMOUSE", "CLICK_DRAG").direction("NORTH").alt())
        .enum_("type", "TOP")
        .boolean("relative", true);
    item(km, "view3d.view_axis", ev("MIDDLEMOUSE", "CLICK_DRAG").direction("SOUTH").alt())
        .enum_("type", "BOTTOM")
        .boolean("relative", true);
    item(km, "view3d.view_axis", ev("MIDDLEMOUSE", "CLICK_DRAG").direction("EAST").alt())
        .enum_("type", "RIGHT")
        .boolean("relative", true);
    item(km, "view3d.view_axis", ev("MIDDLEMOUSE", "CLICK_DRAG").direction("WEST").alt())
        .enum_("type", "LEFT")
        .boolean("relative", true);
  }
  else {
    item(km, "view3d.view_axis", ev("MIDDLEMOUSE", "CLICK_DRAG").direction("NORTH").alt())
        .enum_("type", "TOP");
    item(km, "view3d.view_axis", ev("MIDDLEMOUSE", "CLICK_DRAG").direction("SOUTH").alt())
        .enum_("type", "BOTTOM");
    item(km, "view3d.view_axis", ev("MIDDLEMOUSE", "CLICK_DRAG").direction("EAST").alt())
        .enum_("type", "RIGHT");
    item(km, "view3d.view_axis", ev("MIDDLEMOUSE", "CLICK_DRAG").direction("WEST").alt())
        .enum_("type", "LEFT");
    /* Las diagonales solo existen en el modo absoluto, para casar con el menu radial. */
    item(km, "view3d.view_axis", ev("MIDDLEMOUSE", "CLICK_DRAG").direction("NORTH_WEST").alt())
        .enum_("type", "FRONT");
    item(km, "view3d.view_axis", ev("MIDDLEMOUSE", "CLICK_DRAG").direction("NORTH_EAST").alt())
        .enum_("type", "BACK");
  }

  item(km, "view3d.view_center_pick", ev("MIDDLEMOUSE", "CLICK").alt());
  item(km, "view3d.ndof_orbit_zoom", ev("NDOF_MOTION", "ANY"));
  item(km, "view3d.ndof_orbit", ev("NDOF_MOTION", "ANY").ctrl());
  item(km, "view3d.ndof_pan", ev("NDOF_MOTION", "ANY").shift());
  item(km, "view3d.ndof_all", ev("NDOF_MOTION", "ANY").shift().ctrl());
  item(km, "view3d.view_selected", ev("NDOF_BUTTON_FIT", "PRESS"));
  item(km, "view3d.view_roll", ev("NDOF_BUTTON_ROLL_CW", "PRESS")).number("angle", PI_2_F);
  item(km, "view3d.view_roll", ev("NDOF_BUTTON_ROLL_CCW", "PRESS")).number("angle", -PI_2_F);
  item(km, "view3d.view_axis", ev("NDOF_BUTTON_FRONT", "PRESS")).enum_("type", "FRONT");
  item(km, "view3d.view_axis", ev("NDOF_BUTTON_BACK", "PRESS")).enum_("type", "BACK");
  item(km, "view3d.view_axis", ev("NDOF_BUTTON_LEFT", "PRESS")).enum_("type", "LEFT");
  item(km, "view3d.view_axis", ev("NDOF_BUTTON_RIGHT", "PRESS")).enum_("type", "RIGHT");
  item(km, "view3d.view_axis", ev("NDOF_BUTTON_TOP", "PRESS")).enum_("type", "TOP");
  item(km, "view3d.view_axis", ev("NDOF_BUTTON_BOTTOM", "PRESS")).enum_("type", "BOTTOM");
  item(km, "view3d.view_axis", ev("NDOF_BUTTON_FRONT", "PRESS").shift())
      .enum_("type", "FRONT")
      .boolean("align_active", true);
  item(km, "view3d.view_axis", ev("NDOF_BUTTON_RIGHT", "PRESS").shift())
      .enum_("type", "RIGHT")
      .boolean("align_active", true);
  item(km, "view3d.view_axis", ev("NDOF_BUTTON_TOP", "PRESS").shift())
      .enum_("type", "TOP")
      .boolean("align_active", true);

  /* Seleccion: `_template_view3d_select` expandido con
   * type=params.select_mouse, value=params.select_mouse_value_fallback,
   * legacy=params.legacy, select_passthrough=params.use_tweak_select_passthrough. */
  {
    const char *sel_type = params.select_mouse;
    const char *sel_value = params.select_mouse_value_fallback;
    bool select_passthrough = params.use_tweak_select_passthrough;
    /* El "pasar a traves" solo tiene sentido cuando el propio evento aun puede
     * distinguirse de un arrastre; con CLICK/RELEASE ya se ha resuelto. */
    if (select_passthrough && (streq(sel_value, "CLICK") || streq(sel_value, "RELEASE"))) {
      select_passthrough = false;
    }

    if (!params.legacy) {
      Item it = item(km, "view3d.select", ev(sel_type, sel_value));
      it.boolean("deselect_all", true);
      if (select_passthrough) {
        it.boolean("select_passthrough", true);
      }
    }
    else {
      item(km, "view3d.select", ev(sel_type, sel_value));
    }
    item(km, "view3d.select", ev(sel_type, sel_value).shift()).boolean("toggle", true);
    item(km, "view3d.select", ev(sel_type, sel_value).ctrl())
        .boolean("center", true)
        .boolean("object", true);
    item(km, "view3d.select", ev(sel_type, sel_value).alt()).boolean("enumerate", true);
    item(km, "view3d.select", ev(sel_type, sel_value).shift().ctrl())
        .boolean("toggle", true)
        .boolean("center", true);
    item(km, "view3d.select", ev(sel_type, sel_value).ctrl().alt())
        .boolean("center", true)
        .boolean("enumerate", true);
    item(km, "view3d.select", ev(sel_type, sel_value).shift().alt())
        .boolean("toggle", true)
        .boolean("enumerate", true);
    item(km, "view3d.select", ev(sel_type, sel_value).shift().ctrl().alt())
        .boolean("toggle", true)
        .boolean("center", true)
        .boolean("enumerate", true);

    if (select_passthrough) {
      /* Item extra de click: sin el, pasar a traves no podria deseleccionar el resto. */
      item(km, "view3d.select", ev(sel_type, "CLICK")).boolean("deselect_all", true);
    }
  }

  /* `op_tool_optional`: con "activar herramientas por tecla", la tecla activa la
   * herramienta en vez de lanzar el operador. */
  if (params.use_key_activate_tools) {
    item_tool(km, "builtin.select_box", ev("B", "PRESS"));
  }
  else {
    item(km, "view3d.select_box", ev("B", "PRESS"));
  }
  item(km, "view3d.select_lasso", ev(params.action_mouse, "CLICK_DRAG").ctrl())
      .enum_("mode", "ADD");
  item(km, "view3d.select_lasso", ev(params.action_mouse, "CLICK_DRAG").shift().ctrl())
      .enum_("mode", "SUB");
  if (params.use_key_activate_tools) {
    item_tool(km, "builtin.select_circle", ev("C", "PRESS"));
  }
  else {
    item(km, "view3d.select_circle", ev("C", "PRESS"));
  }

  /* Bordes. */
  item(km, "view3d.clip_border", ev("B", "PRESS").alt());
  item(km, "view3d.zoom_border", ev("B", "PRESS").shift());
  item(km, "view3d.render_border", ev("B", "PRESS").ctrl());
  item(km, "view3d.clear_render_border", ev("B", "PRESS").ctrl().alt());

  /* Camaras. */
  item(km, "view3d.camera_to_view", ev("NUMPAD_0", "PRESS").ctrl().alt());
  item(km, "view3d.object_as_camera", ev("NUMPAD_0", "PRESS").ctrl());

  /* Copiar / pegar. */
  item(km, "view3d.copybuffer", ev("C", "PRESS").ctrl());
  item(km, "view3d.pastebuffer", ev("V", "PRESS").ctrl());

  /* Transformar: lo pone `_template_items_transform_actions`. */

  /* Ajuste (snap). */
  item(km, "wm.context_toggle", ev("TAB", "PRESS").shift())
      .string("data_path", "tool_settings.use_snap");
  item_panel(km, "VIEW3D_PT_snapping", ev("TAB", "PRESS").shift().ctrl())
      .boolean("keep_open", true);
  if (!params.legacy) {
    item_menu_pie(km, "VIEW3D_MT_snap_pie", ev("S", "PRESS").shift());
  }
  else {
    item_menu(km, "VIEW3D_MT_snap", ev("S", "PRESS").shift());
  }

  /* Arranque del juego. */
  item(km, "view3d.game_start", ev("P", "PRESS"));

  if (!params.legacy) {
    /* Menus radiales nuevos. */
    item(km, "wm.context_toggle", ev("ACCENT_GRAVE", "PRESS").ctrl())
        .string("data_path", "space_data.show_gizmo");
    item_menu_pie(km, "VIEW3D_MT_pivot_pie", ev("PERIOD", "PRESS"));
    item_menu_pie(km, "VIEW3D_MT_orientations_pie", ev("COMMA", "PRESS"));
    item_menu_pie(km,
                  !params.use_v3d_shade_ex_pie ? "VIEW3D_MT_shading_pie" :
                                                 "VIEW3D_MT_shading_ex_pie",
                  ev("Z", params.pie_value));
    if (params.use_pie_click_drag) {
      item(km, "view3d.toggle_shading", ev("Z", "CLICK")).enum_("type", "WIREFRAME");
    }
    item(km, "view3d.toggle_shading", ev("Z", "PRESS").shift()).enum_("type", "WIREFRAME");
    item(km, "view3d.toggle_xray", ev("Z", "PRESS").alt());
    item(km, "wm.context_toggle", ev("Z", "PRESS").alt().shift())
        .string("data_path", "space_data.overlay.show_overlays");
  }
  else {
    /* Navegacion antigua. */
    item(km, "view3d.view_lock_to_active", ev("NUMPAD_PERIOD", "PRESS").shift());
    item(km, "view3d.view_lock_clear", ev("NUMPAD_PERIOD", "PRESS").alt());
    item(km, "view3d.navigate", ev("F", "PRESS").shift());
    item(km, "view3d.zoom_camera_1_to_1", ev("NUMPAD_ENTER", "PRESS").shift());
    item(km, "view3d.view_center_cursor", ev("HOME", "PRESS").alt());
    item(km, "view3d.view_center_pick", ev("F", "PRESS").alt());
    item(km, "view3d.view_pan", ev("WHEELUPMOUSE", "PRESS").ctrl()).enum_("type", "PANRIGHT");
    item(km, "view3d.view_pan", ev("WHEELDOWNMOUSE", "PRESS").ctrl()).enum_("type", "PANLEFT");
    item(km, "view3d.view_pan", ev("WHEELUPMOUSE", "PRESS").shift()).enum_("type", "PANUP");
    item(km, "view3d.view_pan", ev("WHEELDOWNMOUSE", "PRESS").shift()).enum_("type", "PANDOWN");
    item(km, "view3d.view_orbit", ev("WHEELUPMOUSE", "PRESS").ctrl().alt())
        .enum_("type", "ORBITLEFT");
    item(km, "view3d.view_orbit", ev("WHEELDOWNMOUSE", "PRESS").ctrl().alt())
        .enum_("type", "ORBITRIGHT");
    item(km, "view3d.view_orbit", ev("WHEELUPMOUSE", "PRESS").shift().alt())
        .enum_("type", "ORBITUP");
    item(km, "view3d.view_orbit", ev("WHEELDOWNMOUSE", "PRESS").shift().alt())
        .enum_("type", "ORBITDOWN");
    item(km, "view3d.view_roll", ev("WHEELUPMOUSE", "PRESS").shift().ctrl()).enum_("type", "LEFT");
    item(km, "view3d.view_roll", ev("WHEELDOWNMOUSE", "PRESS").shift().ctrl())
        .enum_("type", "RIGHT");
    item(km, "transform.create_orientation", ev("SPACE", "PRESS").ctrl().alt())
        .boolean("use", true);
    item(km, "transform.translate", ev("T", "PRESS").shift()).boolean("texture_space", true);
    item(km, "transform.resize", ev("T", "PRESS").shift().alt()).boolean("texture_space", true);

    /* Pivote antiguo. */
    item(km, "wm.context_set_enum", ev("COMMA", "PRESS"))
        .string("data_path", "tool_settings.transform_pivot_point")
        .string("value", "BOUNDING_BOX_CENTER");
    item(km, "wm.context_set_enum", ev("COMMA", "PRESS").ctrl())
        .string("data_path", "tool_settings.transform_pivot_point")
        .string("value", "MEDIAN_POINT");
    item(km, "wm.context_toggle", ev("COMMA", "PRESS").alt())
        .string("data_path", "tool_settings.use_transform_pivot_point_align");
    item(km, "wm.context_toggle", ev("SPACE", "PRESS").ctrl())
        .string("data_path", "space_data.show_gizmo_context");
    item(km, "wm.context_set_enum", ev("PERIOD", "PRESS"))
        .string("data_path", "tool_settings.transform_pivot_point")
        .string("value", "CURSOR");
    item(km, "wm.context_set_enum", ev("PERIOD", "PRESS").ctrl())
        .string("data_path", "tool_settings.transform_pivot_point")
        .string("value", "INDIVIDUAL_ORIGINS");
    item(km, "wm.context_set_enum", ev("PERIOD", "PRESS").alt())
        .string("data_path", "tool_settings.transform_pivot_point")
        .string("value", "ACTIVE_ELEMENT");

    /* Sombreado antiguo. */
    item(km, "wm.context_toggle_enum", ev("Z", "PRESS"))
        .string("data_path", "space_data.shading.type")
        .string("value_1", "WIREFRAME")
        .string("value_2", "SOLID");
    item(km, "wm.context_toggle_enum", ev("Z", "PRESS").shift())
        .string("data_path", "space_data.shading.type")
        .string("value_1", "RENDERED")
        .string("value_2", "SOLID");
    item(km, "wm.context_toggle_enum", ev("Z", "PRESS").alt())
        .string("data_path", "space_data.shading.type")
        .string("value_1", "MATERIAL")
        .string("value_2", "SOLID");
  }

  /* `params.select_mouse == 'LEFTMOUSE'` se comprueba con `!select_mouse_right`.
   * Atajo rapido a la herramienta de seleccion: con seleccion por el izquierdo no se
   * puede seleccionar comodamente con cualquier herramienta activa. */
  if (!params.select_mouse_right && !params.legacy) {
    /* `op_tool_cycle`: `item_tool` pone el nombre; `cycle` va aparte. */
    item_tool(km, "builtin.select_box", ev("W", "PRESS")).boolean("cycle", true);
  }
}

/** \} */

void register_group_04(wmKeyConfig *kc, const Params &params)
{
  km_view3d(kc, params);
}

}  // namespace flipendo::keymap
