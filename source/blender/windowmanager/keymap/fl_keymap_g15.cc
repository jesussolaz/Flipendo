/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Mapa de teclado, grupo 15: modos de pintura (curva de pincel, pintura de textura,
 * pintura de vertices, pintura de pesos) y la mascara de caras que comparten.
 * Transliterado de blender_default.py.
 */

#include <string>

#include "FL_keymap_build.hpp"
#include "FL_keymap_params.hpp"

namespace flipendo::keymap {

/* -------------------------------------------------------------------- */
/** \name Plantillas que este grupo repite
 *
 * Son `radial_control_properties()`, `_template_paint_radial_control()`,
 * `_template_asset_shelf_popup()` y `_template_items_legacy_tools_from_numbers()`
 * del Python. Se quedan aqui como `static` porque los cuatro modos de pintura de
 * este fichero las usan una y otra vez; expandirlas a mano seria copiar once
 * propiedades por atajo.
 * \{ */

/**
 * `radial_control_properties()`: las once propiedades de `wm.radial_control`.
 *
 * Las rutas se componen en tiempo de ejecucion porque dependen del modo de pintura;
 * `RNA_string_set` copia la cadena, asi que basta con que el `std::string` viva
 * hasta la llamada.
 */
static void radial_control_properties(Item &kmi,
                                      const char *paint,
                                      const char *prop,
                                      const char *secondary_prop,
                                      const bool secondary_rotation = false,
                                      const bool color = false,
                                      const bool zoom = false)
{
  const std::string brush_path = std::string("tool_settings.") + paint + ".brush";
  const std::string unified_path = "tool_settings.unified_paint_settings";
  const char *rotation = secondary_rotation ? "mask_texture_slot.angle" : "texture_slot.angle";

  /* El Python pone cadena vacia, no omite la propiedad, cuando la opcion no aplica. */
  const std::string data_path_primary = brush_path + "." + prop;
  const std::string data_path_secondary = (secondary_prop != nullptr) ?
                                              unified_path + "." + prop :
                                              std::string();
  const std::string use_secondary = (secondary_prop != nullptr) ?
                                        unified_path + "." + secondary_prop :
                                        std::string();
  const std::string rotation_path = brush_path + "." + rotation;
  const std::string color_path = brush_path + ".cursor_color_add";
  const std::string fill_color_path = color ? brush_path + ".color" : std::string();
  const std::string fill_color_override_path = color ? unified_path + ".color" : std::string();
  const std::string fill_color_override_test_path = color ?
                                                        unified_path + ".use_unified_color" :
                                                        std::string();

  kmi.string("data_path_primary", data_path_primary.c_str());
  kmi.string("data_path_secondary", data_path_secondary.c_str());
  kmi.string("use_secondary", use_secondary.c_str());
  kmi.string("rotation_path", rotation_path.c_str());
  kmi.string("color_path", color_path.c_str());
  kmi.string("fill_color_path", fill_color_path.c_str());
  kmi.string("fill_color_override_path", fill_color_override_path.c_str());
  kmi.string("fill_color_override_test_path", fill_color_override_test_path.c_str());
  kmi.string("zoom_path", zoom ? "space_data.zoom" : "");
  kmi.string("image_id", brush_path.c_str());
  kmi.boolean("secondary_tex", secondary_rotation);
}

/** `_template_paint_radial_control()`. */
static void template_paint_radial_control(wmKeyMap *km,
                                          const char *paint,
                                          const bool rotation = false,
                                          const bool secondary_rotation = false,
                                          const bool color = false,
                                          const bool zoom = false)
{
  Item kmi_size = item(km, "wm.radial_control", ev("F", "PRESS"));
  radial_control_properties(
      kmi_size, paint, "size", "use_unified_size", secondary_rotation, color, zoom);

  Item kmi_strength = item(km, "wm.radial_control", ev("F", "PRESS").shift());
  radial_control_properties(
      kmi_strength, paint, "strength", "use_unified_strength", secondary_rotation, color, false);

  if (rotation) {
    Item kmi = item(km, "wm.radial_control", ev("F", "PRESS").ctrl());
    radial_control_properties(kmi, paint, "texture_slot.angle", nullptr, false, color, false);
  }

  if (secondary_rotation) {
    Item kmi = item(km, "wm.radial_control", ev("F", "PRESS").ctrl().alt());
    radial_control_properties(
        kmi, paint, "mask_texture_slot.angle", nullptr, secondary_rotation, color, false);
  }
}

/** `_template_asset_shelf_popup()`: con 'SEARCH' la barra espaciadora no se toca. */
static void template_asset_shelf_popup(wmKeyMap *km,
                                       const char *asset_shelf,
                                       const SpacebarAction spacebar_action)
{
  if (spacebar_action == SpacebarAction::Search) {
    return;
  }

  Event event = ev("SPACE", "PRESS");
  if (spacebar_action == SpacebarAction::Play) {
    event.shift();
  }

  item(km, "wm.call_asset_shelf_popover", event).string("name", asset_shelf);
}

/** `_template_items_legacy_tools_from_numbers()`: 20 herramientas, 10 teclas + Shift. */
static void template_items_legacy_tools_from_numbers(wmKeyMap *km)
{
  /* `NUMBERS_1`: orden fisico del teclado, el cero al final. */
  static const char *numbers_1[10] = {
      "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN", "EIGHT", "NINE", "ZERO"};

  for (int i = 0; i < 20; i++) {
    Event event = ev(numbers_1[i % 10], "PRESS");
    if (i >= 10) {
      event.shift();
    }
    item(km, "wm.tool_set_by_index", event).integer("index", i);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Curva de pincel
 * \{ */

static void km_paint_curve(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Paint Curve", "EMPTY", "WINDOW");

  item(km, "paintcurve.add_point_slide", ev(params.action_mouse, "PRESS").ctrl());
  item(km, "paintcurve.select", ev(params.select_mouse, "PRESS"));
  item(km, "paintcurve.select", ev(params.select_mouse, "PRESS").shift())
      .boolean("extend", true);
  item(km, "paintcurve.slide", ev(params.action_mouse, "PRESS")).boolean("align", false);
  item(km, "paintcurve.slide", ev(params.action_mouse, "PRESS").shift()).boolean("align", true);
  item(km, "paintcurve.select", ev("A", "PRESS")).boolean("toggle", true);
  item(km, "paintcurve.cursor", ev(params.action_mouse, "PRESS").shift().ctrl());
  item(km, "paintcurve.delete_point", ev("X", "PRESS"));
  item(km, "paintcurve.delete_point", ev("DEL", "PRESS"));
  item(km, "paintcurve.draw", ev("RET", "PRESS"));
  item(km, "paintcurve.draw", ev("NUMPAD_ENTER", "PRESS"));
  item(km, "transform.translate", ev("G", "PRESS"));
  item(km, "transform.translate", ev(params.select_mouse, "CLICK_DRAG"));
  item(km, "transform.rotate", ev("R", "PRESS"));
  item(km, "transform.resize", ev("S", "PRESS"));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pintura de textura
 * \{ */

static void km_image_paint(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Image Paint", "EMPTY", "WINDOW");

  item(km, "paint.image_paint", ev("LEFTMOUSE", "PRESS"));
  item(km, "paint.image_paint", ev("LEFTMOUSE", "PRESS").ctrl()).enum_("mode", "INVERT");
  item(km, "paint.image_paint", ev("LEFTMOUSE", "PRESS").shift()).enum_("mode", "SMOOTH");
  item(km, "paint.brush_colors_flip", ev("X", "PRESS"));
  item(km, "paint.grab_clone", ev("RIGHTMOUSE", "PRESS"));
  item(km, "paint.sample_color", ev("X", "PRESS").shift()).boolean("merged", false);
  item(km, "paint.sample_color", ev("X", "PRESS").shift().ctrl()).boolean("merged", true);
  item(km, "brush.scale_size", ev("LEFT_BRACKET", "PRESS").repeat()).number("scalar", 0.9f);
  item(km, "brush.scale_size", ev("RIGHT_BRACKET", "PRESS").repeat())
      .number("scalar", static_cast<float>(1.0 / 0.9));

  /* `_template_paint_radial_control("image_paint", color, zoom, rotation, secondary_rotation)`. */
  template_paint_radial_control(km, "image_paint", true, true, true, true);

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
  item(km, "wm.context_toggle", ev("ONE", "PRESS"))
      .string("data_path", "image_paint_object.data.use_paint_mask");
  item(km, "wm.context_toggle", ev("S", "PRESS").shift())
      .string("data_path", "tool_settings.image_paint.brush.use_smooth_stroke");
  item(km, "wm.context_menu_enum", ev("E", "PRESS").alt())
      .string("data_path", "tool_settings.image_paint.brush.stroke_method");

  /* `_template_items_context_panel()`: el evento del usuario y ademas la tecla de menu. */
  item_panel(km, "VIEW3D_PT_paint_texture_context_menu", params.context_menu_event);
  item_panel(km, "VIEW3D_PT_paint_texture_context_menu", ev("APP", "PRESS"));

  template_asset_shelf_popup(km, "VIEW3D_AST_brush_texture_paint", params.spacebar_action);
  template_asset_shelf_popup(km, "IMAGE_AST_brush_paint", params.spacebar_action);

  if (params.legacy) {
    template_items_legacy_tools_from_numbers(km);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pintura de vertices
 * \{ */

static void km_vertex_paint(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Vertex Paint", "EMPTY", "WINDOW");

  item(km, "paint.vertex_paint", ev("LEFTMOUSE", "PRESS"));
  item(km, "paint.vertex_paint", ev("LEFTMOUSE", "PRESS").ctrl()).enum_("mode", "INVERT");
  item(km, "paint.vertex_paint", ev("LEFTMOUSE", "PRESS").shift()).enum_("mode", "SMOOTH");
  item(km, "paint.brush_colors_flip", ev("X", "PRESS"));
  item(km, "paint.sample_color", ev("X", "PRESS").shift()).boolean("merged", false);
  item(km, "paint.vertex_color_set", ev("X", "PRESS").ctrl());
  item(km, "brush.scale_size", ev("LEFT_BRACKET", "PRESS").repeat()).number("scalar", 0.9f);
  item(km, "brush.scale_size", ev("RIGHT_BRACKET", "PRESS").repeat())
      .number("scalar", static_cast<float>(1.0 / 0.9));

  /* `_template_paint_radial_control("vertex_paint", color=True, rotation=True)`. */
  template_paint_radial_control(km, "vertex_paint", true, false, true, false);

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
  item(km, "wm.context_toggle", ev("ONE", "PRESS"))
      .string("data_path", "vertex_paint_object.data.use_paint_mask");
  item(km, "wm.context_toggle", ev("S", "PRESS").shift())
      .string("data_path", "tool_settings.vertex_paint.brush.use_smooth_stroke");
  item(km, "wm.context_menu_enum", ev("E", "PRESS").alt())
      .string("data_path", "tool_settings.vertex_paint.brush.stroke_method");
  item(km, "paint.face_vert_reveal", ev("H", "PRESS").alt());

  item_panel(km, "VIEW3D_PT_paint_vertex_context_menu", params.context_menu_event);
  item_panel(km, "VIEW3D_PT_paint_vertex_context_menu", ev("APP", "PRESS"));

  template_asset_shelf_popup(km, "VIEW3D_AST_brush_vertex_paint", params.spacebar_action);

  if (params.legacy) {
    template_items_legacy_tools_from_numbers(km);
  }
  else {
    item(km, "wm.context_toggle", ev("TWO", "PRESS"))
        .string("data_path", "vertex_paint_object.data.use_paint_mask_vertex");
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pintura de pesos
 *
 * Nota del Python: este keymap cae a "Pose" cuando la armadura que deforma la malla
 * esta seleccionada en modo pintura de pesos, asi que al tocarlo hay que cuidar de no
 * pisar operaciones de pose (transformar huesos, por ejemplo).
 * \{ */

static void km_weight_paint(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Weight Paint", "EMPTY", "WINDOW");

  item(km, "paint.weight_paint", ev("LEFTMOUSE", "PRESS"));
  item(km, "paint.weight_paint", ev("LEFTMOUSE", "PRESS").ctrl()).enum_("mode", "INVERT");
  item(km, "paint.weight_paint", ev("LEFTMOUSE", "PRESS").shift()).enum_("mode", "SMOOTH");
  item(km, "paint.weight_sample", ev("X", "PRESS").shift());
  item(km, "paint.weight_sample_group", ev("X", "PRESS").ctrl().shift());
  item(km, "paint.weight_gradient", ev("A", "PRESS").shift().alt()).enum_("type", "RADIAL");
  item(km, "paint.weight_gradient", ev("A", "PRESS").shift()).enum_("type", "LINEAR");
  item(km, "paint.weight_set", ev("X", "PRESS").ctrl());
  item(km, "brush.scale_size", ev("LEFT_BRACKET", "PRESS").repeat()).number("scalar", 0.9f);
  item(km, "brush.scale_size", ev("RIGHT_BRACKET", "PRESS").repeat())
      .number("scalar", static_cast<float>(1.0 / 0.9));

  /* `_template_paint_radial_control("weight_paint")`: sin rotacion ni color ni zoom. */
  template_paint_radial_control(km, "weight_paint", false, false, false, false);

  Item kmi_weight = item(km, "wm.radial_control", ev("F", "PRESS").ctrl());
  radial_control_properties(kmi_weight, "weight_paint", "weight", "use_unified_weight");

  /* Sic: el Python apunta a `vertex_paint` aqui, no a `weight_paint`. Se conserva. */
  item(km, "wm.context_menu_enum", ev("E", "PRESS").alt())
      .string("data_path", "tool_settings.vertex_paint.brush.stroke_method");
  item(km, "wm.context_toggle", ev("ONE", "PRESS"))
      .string("data_path", "weight_paint_object.data.use_paint_mask");
  item(km, "wm.context_toggle", ev("TWO", "PRESS"))
      .string("data_path", "weight_paint_object.data.use_paint_mask_vertex");
  item(km, "wm.context_toggle", ev("THREE", "PRESS"))
      .string("data_path", "weight_paint_object.data.use_paint_bone_selection");
  item(km, "wm.context_toggle", ev("S", "PRESS").shift())
      .string("data_path", "tool_settings.weight_paint.brush.use_smooth_stroke");
  item_menu_pie(km, "VIEW3D_MT_wpaint_vgroup_lock_pie", ev("K", "PRESS"));

  item_panel(km, "VIEW3D_PT_paint_weight_context_menu", params.context_menu_event);
  item_panel(km, "VIEW3D_PT_paint_weight_context_menu", ev("APP", "PRESS"));

  template_asset_shelf_popup(km, "VIEW3D_AST_brush_weight_paint", params.spacebar_action);

  if (!params.select_mouse_right) {
    /* Seleccion de huesos con Alt para el modo combinado pintura de pesos + pose. */
    item(km, "view3d.select", ev("LEFTMOUSE", "PRESS").alt());
    item(km, "view3d.select", ev("LEFTMOUSE", "PRESS").shift().alt()).boolean("toggle", true);

    /* Ctrl-Shift-LMB hace falta para la emulacion del boton central, que choca con Alt.
     * Va bien en modo pose, donde basta con seleccionar un hueso; para caras o vertices
     * sirve de poco y hay que recurrir a las herramientas de seleccion. */
    item(km, "view3d.select", ev("LEFTMOUSE", "PRESS").ctrl().shift());
  }

  if (params.legacy) {
    template_items_legacy_tools_from_numbers(km);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mascara de caras (pesos, vertices, textura)
 * \{ */

static void km_paint_face_mask(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Paint Face Mask (Weight, Vertex, Texture)", "EMPTY", "WINDOW");

  /* `_template_items_select_actions(params, "paint.face_select_all")`. */
  if (!params.use_select_all_toggle) {
    item(km, "paint.face_select_all", ev("A", "PRESS")).enum_("action", "SELECT");
    item(km, "paint.face_select_all", ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, "paint.face_select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
    item(km, "paint.face_select_all", ev("A", "DOUBLE_CLICK")).enum_("action", "DESELECT");
  }
  else if (params.legacy) {
    /* En el keymap heredado Alt-A es la reproduccion, asi que no se usa aqui. */
    item(km, "paint.face_select_all", ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, "paint.face_select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }
  else {
    item(km, "paint.face_select_all", ev("A", "PRESS")).enum_("action", "TOGGLE");
    item(km, "paint.face_select_all", ev("A", "PRESS").alt()).enum_("action", "DESELECT");
    item(km, "paint.face_select_all", ev("I", "PRESS").ctrl()).enum_("action", "INVERT");
  }

  /* `_template_items_select_lasso(params, "view3d.select_lasso")`: con seleccion por
   * boton derecho, Ctrl-LMB ya esta cogido por los modos de pincel, asi que para
   * desenmascarar se usan todos los modificadores juntos. */
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

  /* `_template_items_hide_reveal_actions("paint.face_select_hide", "paint.face_vert_reveal")`. */
  item(km, "paint.face_vert_reveal", ev("H", "PRESS").alt());
  item(km, "paint.face_select_hide", ev("H", "PRESS")).boolean("unselected", false);
  item(km, "paint.face_select_hide", ev("H", "PRESS").shift()).boolean("unselected", true);

  item(km, "paint.face_select_linked", ev("L", "PRESS").ctrl());
  item(km, "paint.face_select_linked_pick", ev("L", "PRESS")).boolean("deselect", false);
  item(km, "paint.face_select_linked_pick", ev("L", "PRESS").shift()).boolean("deselect", true);
  item(km, "paint.face_select_more", ev("NUMPAD_PLUS", "PRESS").ctrl());
  item(km, "paint.face_select_less", ev("NUMPAD_MINUS", "PRESS").ctrl());

  /* Con el boton izquierdo la seleccion en bucle vive en el keymap de la herramienta,
   * porque aqui chocaria con Alt-LMB (seleccion normal). */
  if (params.select_mouse_right) {
    item(km, "paint.face_select_loop", ev(params.select_mouse, "PRESS").alt())
        .boolean("extend", false)
        .boolean("select", true);
    item(km, "paint.face_select_loop", ev(params.select_mouse, "PRESS").alt().shift())
        .boolean("extend", true)
        .boolean("select", true);
    item(km, "paint.face_select_loop", ev(params.select_mouse, "PRESS").alt().shift().ctrl())
        .boolean("extend", true)
        .boolean("select", false);
  }
}

/** \} */

void register_group_15(wmKeyConfig *kc, const Params &params)
{
  km_paint_curve(kc, params);
  km_image_paint(kc, params);
  km_vertex_paint(kc, params);
  km_weight_paint(kc, params);
  km_paint_face_mask(kc, params);
}

}  // namespace flipendo::keymap
