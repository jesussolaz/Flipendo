/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Mapa de teclado, grupo 23: herramientas del editor de nodos (lazo, circulo y
 * cortar enlaces) y las herramientas genericas de la vista 3D (cursor, seleccion de
 * texto, tweak, caja/circulo/lazo, transformar, mover, rotar, escalar, inclinar,
 * doblar, medir) mas las de modo pose (breakdowner, empujar, relajar).
 * Transliterado de blender_default.py.
 */

#include <cstring>

#include "FL_keymap_build.hpp"
#include "FL_keymap_params.hpp"

namespace flipendo::keymap {

static bool streq(const char *a, const char *b)
{
  return std::strcmp(a, b) == 0;
}

/**
 * `**params.tool_modifier` del Python.
 *
 * `tool_modifier` solo puede valer `{}` o `{"alt": -1}` (Alt pulsado o no, que es lo
 * que activa `use_alt_tool_or_cursor` con seleccion por boton izquierdo). Se pasa el
 * evento entero para que la linea C++ se lea como el `{**evento, **tool_modifier}`
 * del Python y el condicional no se pierda.
 */
static Event tool_modifier(const Params &params, Event event)
{
  if (params.tool_modifier_alt_any) {
    /* TODO(keymap): falta `{"alt": -1}`: el atajo deberia casar tanto con Alt
     * pulsado como sin pulsar, dejando el resto de modificadores exactos. `Event`
     * solo tiene `.any()`, que pone KM_ANY en TODOS los modificadores, asi que no
     * es equivalente y escribirlo cambiaria otros atajos. Con los parametros por
     * defecto `tool_modifier_alt_any` es false y el baseline no se ve afectado. */
  }
  return event;
}

/**
 * `_template_view3d_paint_mask_select_loop(params)`.
 *
 * Va aparte en el Python para que, con seleccion por boton izquierdo, Alt-LMB pueda
 * usarse para escoger seleccion mientras la herramienta sigue usandolo para el
 * bucle. Se repite en las cuatro herramientas de seleccion de la vista 3D de este
 * grupo, por eso aqui es una funcion.
 */
static void template_view3d_paint_mask_select_loop(wmKeyMap *km, const Params &params)
{
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

/* -------------------------------------------------------------------- */
/** \name Sistema de herramientas (editor de nodos)
 * \{ */

static void km_node_editor_tool_select_lasso(wmKeyConfig *kc,
                                             const Params &params,
                                             const bool fallback)
{
  wmKeyMap *km = keymap(kc,
                        fallback ? "Node Tool: Select Lasso (fallback)" :
                                   "Node Tool: Select Lasso",
                        "NODE_EDITOR",
                        "WINDOW");

  /* El keymap se crea aunque se quede vacio: el Python tambien lo devuelve, solo que
   * sin items. */
  if (fallback && !params.use_fallback_tool) {
    return;
  }

  /* `select_tweak_event` o `tool_tweak_event`: los dos son CLICK_DRAG y solo se
   * diferencian en el boton. */
  const char *type = (fallback && params.use_fallback_tool_select_mouse) ? params.select_mouse :
                                                                          params.tool_mouse;

  /* `_template_items_tool_select_actions_simple`: 'SET' no se define aqui, se toma de
   * las opciones de la herramienta. */
  item(km, "node.select_lasso", ev(type, "CLICK_DRAG")).boolean("tweak", true);
  item(km, "node.select_lasso", ev(type, "CLICK_DRAG").shift())
      .boolean("tweak", true)
      .enum_("mode", "ADD");
  item(km, "node.select_lasso", ev(type, "CLICK_DRAG").ctrl())
      .boolean("tweak", true)
      .enum_("mode", "SUB");
}

static void km_node_editor_tool_select_circle(wmKeyConfig *kc,
                                              const Params &params,
                                              const bool fallback)
{
  wmKeyMap *km = keymap(kc,
                        fallback ? "Node Tool: Select Circle (fallback)" :
                                   "Node Tool: Select Circle",
                        "NODE_EDITOR",
                        "WINDOW");

  if (fallback && !params.use_fallback_tool) {
    return;
  }

  /* El circulo va sobre el arrastre para que RMB o Shift-RMB sigan pudiendo marcar un
   * elemento como activo. */
  const bool on_select_mouse = fallback && params.use_fallback_tool_select_mouse;
  const char *type = on_select_mouse ? params.select_mouse : params.tool_mouse;
  const char *value = on_select_mouse ? "CLICK_DRAG" : "PRESS";

  item(km, "node.select_circle", ev(type, value)).boolean("wait_for_input", false);
  item(km, "node.select_circle", ev(type, value).shift())
      .boolean("wait_for_input", false)
      .enum_("mode", "ADD");
  item(km, "node.select_circle", ev(type, value).ctrl())
      .boolean("wait_for_input", false)
      .enum_("mode", "SUB");
}

static void km_node_editor_tool_links_cut(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Node Tool: Links Cut", "NODE_EDITOR", "WINDOW");

  item(km, "node.links_cut", ev(params.tool_mouse, "PRESS"));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sistema de herramientas (vista 3D, genericas)
 * \{ */

static void km_3d_view_tool_cursor(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "3D View Tool: Cursor", "VIEW_3D", "WINDOW");

  item(km, "view3d.cursor3d", ev(params.tool_mouse, "PRESS"));
  /* No se usa `tool_maybe_tweak_event` porque chocaria con el 'PRESS' que coloca el
   * cursor. */
  item(km, "transform.translate", ev(params.tool_mouse, "CLICK_DRAG"))
      .boolean("release_confirm", true)
      .boolean("cursor_transform", true);
}

static void km_3d_view_tool_text_select(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap(kc, "3D View Tool: Edit Text, Select Text", "VIEW_3D", "WINDOW");

  item(km, "font.selection_set", ev("LEFTMOUSE", "PRESS"));
  item(km, "font.select_word", ev("LEFTMOUSE", "DOUBLE_CLICK"));
}

static void km_3d_view_tool_select(wmKeyConfig *kc, const Params &params, const bool fallback)
{
  wmKeyMap *km = keymap(kc,
                        fallback ? "3D View Tool: Tweak (fallback)" : "3D View Tool: Tweak",
                        "VIEW_3D",
                        "WINDOW");

  /* `_template_items_tool_select(params, "view3d.select", "view3d.cursor3d",
   * fallback=fallback)`, con `cursor_prioritize` a False (su valor por defecto). */
  if (!(fallback && params.select_mouse_right)) {
    bool select_passthrough = false;
    if (!params.legacy) {
      /* Soporte experimental de LMB para la herramienta de tweak, ver #96544. */
      if (!params.select_mouse_right) {
        select_passthrough = params.use_tweak_select_passthrough;
      }
      else {
        select_passthrough = true;
      }
    }

    if (!params.legacy && !fallback && select_passthrough) {
      item(km, "view3d.select", ev("LEFTMOUSE", "PRESS"))
          .boolean("deselect_all", true)
          .boolean("select_passthrough", true);
      item(km, "view3d.select", ev("LEFTMOUSE", "CLICK")).boolean("deselect_all", true);
      item(km, "view3d.select", ev("LEFTMOUSE", "PRESS").shift())
          .boolean("deselect_all", false)
          .boolean("toggle", true);
    }
    else if (!params.select_mouse_right) {
      /* Con seleccion por boton izquierdo se usa 'PRESS', para seleccionar sin
       * retardo. */
      Item it = item(km, "view3d.select", ev("LEFTMOUSE", "PRESS"));
      it.boolean("deselect_all", true);
      if (select_passthrough) {
        /* Sin esto la herramienta de reserva no soporta pasar a traves, ver #115887. */
        it.boolean("select_passthrough", true);
      }
      item(km, "view3d.select", ev("LEFTMOUSE", "PRESS").shift()).boolean("toggle", true);

      if (fallback) {
        /* El keymap de reserva tiene que transformar, porque se espera llegar a la
         * herramienta principal por gizmos, ver #96885. */
        item(km, "transform.translate", ev("LEFTMOUSE", "CLICK_DRAG"))
            .boolean("release_confirm", true);
      }
    }
    else {
      /* Con el boton derecho para seleccionar, el izquierdo coloca el cursor. */
      item(km, "view3d.cursor3d", ev("LEFTMOUSE", "PRESS"));
      item(km, "transform.translate", ev("LEFTMOUSE", "CLICK_DRAG"))
          .boolean("release_confirm", true)
          .boolean("cursor_transform", true);
    }
  }

  /* `_template_view3d_select(type=params.select_mouse, value=params.select_mouse_value,
   * legacy=params.legacy, select_passthrough=params.use_tweak_select_passthrough,
   * exclude_mod="ctrl")`. Se excluyen los items con Ctrl para no comerse las acciones
   * de Ctrl-RMB cuando esto se usa como keymap de herramienta, ver #92467. */
  if (!params.use_fallback_tool_select_handled) {
    const char *sel_type = params.select_mouse;
    const char *sel_value = params.select_mouse_value;
    bool select_passthrough = params.use_tweak_select_passthrough;
    /* Pasar a traves solo tiene sentido mientras el evento aun pueda distinguirse de
     * un arrastre; con CLICK/RELEASE ya esta resuelto. */
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
    item(km, "view3d.select", ev(sel_type, sel_value).alt()).boolean("enumerate", true);
    item(km, "view3d.select", ev(sel_type, sel_value).shift().alt())
        .boolean("toggle", true)
        .boolean("enumerate", true);

    if (select_passthrough) {
      /* Item extra de click: sin el, pasar a traves no podria deseleccionar el resto. */
      item(km, "view3d.select", ev(sel_type, "CLICK")).boolean("deselect_all", true);
    }
  }

  if (!params.select_mouse_right) {
    template_view3d_paint_mask_select_loop(km, params);
  }
}

static void km_3d_view_tool_select_box(wmKeyConfig *kc, const Params &params, const bool fallback)
{
  wmKeyMap *km = keymap(kc,
                        fallback ? "3D View Tool: Select Box (fallback)" :
                                   "3D View Tool: Select Box",
                        "VIEW_3D",
                        "WINDOW");

  if (!(fallback && !params.use_fallback_tool)) {
    /* No se usa `tool_maybe_tweak_event` aqui a proposito: con RMB para seleccionar,
     * Ctrl-LMB lo cazaria la caja de seleccion en vez de anadir/extruir. */
    const char *type = (fallback && params.use_fallback_tool_select_mouse) ? params.select_mouse :
                                                                            params.tool_mouse;

    /* `_template_items_tool_select_actions`. */
    item(km, "view3d.select_box", ev(type, "CLICK_DRAG"));
    item(km, "view3d.select_box", ev(type, "CLICK_DRAG").shift()).enum_("mode", "ADD");
    item(km, "view3d.select_box", ev(type, "CLICK_DRAG").ctrl()).enum_("mode", "SUB");
    item(km, "view3d.select_box", ev(type, "CLICK_DRAG").shift().ctrl()).enum_("mode", "AND");
  }

  if (!params.select_mouse_right) {
    template_view3d_paint_mask_select_loop(km, params);
  }
}

static void km_3d_view_tool_select_circle(wmKeyConfig *kc,
                                          const Params &params,
                                          const bool fallback)
{
  wmKeyMap *km = keymap(kc,
                        fallback ? "3D View Tool: Select Circle (fallback)" :
                                   "3D View Tool: Select Circle",
                        "VIEW_3D",
                        "WINDOW");

  if (!(fallback && !params.use_fallback_tool)) {
    /* El circulo va sobre el arrastre para que RMB o Shift-RMB sigan pudiendo marcar
     * un elemento como activo. */
    const bool on_select_mouse = fallback && params.use_fallback_tool_select_mouse;
    const char *type = on_select_mouse ? params.select_mouse : params.tool_mouse;
    const char *value = on_select_mouse ? "CLICK_DRAG" : "PRESS";

    item(km, "view3d.select_circle", ev(type, value)).boolean("wait_for_input", false);
    item(km, "view3d.select_circle", ev(type, value).shift())
        .boolean("wait_for_input", false)
        .enum_("mode", "ADD");
    item(km, "view3d.select_circle", ev(type, value).ctrl())
        .boolean("wait_for_input", false)
        .enum_("mode", "SUB");
  }

  if (!params.select_mouse_right) {
    template_view3d_paint_mask_select_loop(km, params);
  }
}

static void km_3d_view_tool_select_lasso(wmKeyConfig *kc,
                                         const Params &params,
                                         const bool fallback)
{
  wmKeyMap *km = keymap(kc,
                        fallback ? "3D View Tool: Select Lasso (fallback)" :
                                   "3D View Tool: Select Lasso",
                        "VIEW_3D",
                        "WINDOW");

  if (!(fallback && !params.use_fallback_tool)) {
    const char *type = (fallback && params.use_fallback_tool_select_mouse) ? params.select_mouse :
                                                                            params.tool_mouse;

    /* `_template_items_tool_select_actions`. */
    item(km, "view3d.select_lasso", ev(type, "CLICK_DRAG"));
    item(km, "view3d.select_lasso", ev(type, "CLICK_DRAG").shift()).enum_("mode", "ADD");
    item(km, "view3d.select_lasso", ev(type, "CLICK_DRAG").ctrl()).enum_("mode", "SUB");
    item(km, "view3d.select_lasso", ev(type, "CLICK_DRAG").shift().ctrl()).enum_("mode", "AND");
  }

  if (!params.select_mouse_right) {
    template_view3d_paint_mask_select_loop(km, params);
  }
}

static void km_3d_view_tool_transform(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "3D View Tool: Transform", "VIEW_3D", "WINDOW");

  item(km,
       "transform.from_gizmo",
       tool_modifier(params, ev(params.tool_mouse, params.tool_maybe_tweak_value)));
}

static void km_3d_view_tool_move(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "3D View Tool: Move", "VIEW_3D", "WINDOW");

  item(km,
       "transform.translate",
       tool_modifier(params, ev(params.tool_mouse, params.tool_maybe_tweak_value)))
      .boolean("release_confirm", true);
}

static void km_3d_view_tool_rotate(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "3D View Tool: Rotate", "VIEW_3D", "WINDOW");

  item(km,
       "transform.rotate",
       tool_modifier(params, ev(params.tool_mouse, params.tool_maybe_tweak_value)))
      .boolean("release_confirm", true);
}

static void km_3d_view_tool_scale(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "3D View Tool: Scale", "VIEW_3D", "WINDOW");

  item(km,
       "transform.resize",
       tool_modifier(params, ev(params.tool_mouse, params.tool_maybe_tweak_value)))
      .boolean("release_confirm", true);
}

static void km_3d_view_tool_shear(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "3D View Tool: Shear", "VIEW_3D", "WINDOW");

  /* No se usa `tool_maybe_tweak_value` porque se perderia el soporte de direccion del
   * arrastre. */
  item(km,
       "transform.shear",
       tool_modifier(params, ev(params.tool_mouse, "CLICK_DRAG").direction("NORTH")))
      .boolean("release_confirm", true)
      .enum_("orient_axis_ortho", "Y");
  item(km,
       "transform.shear",
       tool_modifier(params, ev(params.tool_mouse, "CLICK_DRAG").direction("SOUTH")))
      .boolean("release_confirm", true)
      .enum_("orient_axis_ortho", "Y");

  /* De reserva, para cazar tambien las diagonales. */
  item(km, "transform.shear", tool_modifier(params, ev(params.tool_mouse, "CLICK_DRAG")))
      .boolean("release_confirm", true)
      .enum_("orient_axis_ortho", "X");
}

static void km_3d_view_tool_bend(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "3D View Tool: Bend", "VIEW_3D", "WINDOW");

  /* Sin `tool_modifier`: esta herramienta se queda con toda la entrada. */
  item(km, "transform.bend", ev(params.tool_mouse, params.tool_maybe_tweak_value))
      .boolean("release_confirm", true);
}

static void km_3d_view_tool_measure(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "3D View Tool: Measure", "VIEW_3D", "WINDOW");

  item(km, "view3d.ruler_add", ev(params.tool_mouse, params.tool_maybe_tweak_value));
  item(km, "view3d.ruler_remove", ev("X", "PRESS"));
  item(km, "view3d.ruler_remove", ev("DEL", "PRESS"));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Sistema de herramientas (vista 3D, modo pose)
 * \{ */

static void km_3d_view_tool_pose_breakdowner(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "3D View Tool: Pose, Breakdowner", "VIEW_3D", "WINDOW");

  item(km,
       "pose.breakdown",
       tool_modifier(params, ev(params.tool_mouse, params.tool_maybe_tweak_value)));
}

static void km_3d_view_tool_pose_push(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "3D View Tool: Pose, Push", "VIEW_3D", "WINDOW");

  item(km,
       "pose.push",
       tool_modifier(params, ev(params.tool_mouse, params.tool_maybe_tweak_value)));
}

static void km_3d_view_tool_pose_relax(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "3D View Tool: Pose, Relax", "VIEW_3D", "WINDOW");

  item(km,
       "pose.relax",
       tool_modifier(params, ev(params.tool_mouse, params.tool_maybe_tweak_value)));
}

/** \} */

void register_group_23(wmKeyConfig *kc, const Params &params)
{
  /* Las herramientas de seleccion se registran dos veces, igual que en el Python:
   * `for fallback in (False, True)`. La segunda pasada es el keymap "(fallback)". */
  km_node_editor_tool_select_lasso(kc, params, false);
  km_node_editor_tool_select_lasso(kc, params, true);
  km_node_editor_tool_select_circle(kc, params, false);
  km_node_editor_tool_select_circle(kc, params, true);
  km_node_editor_tool_links_cut(kc, params);

  km_3d_view_tool_cursor(kc, params);
  km_3d_view_tool_text_select(kc, params);
  km_3d_view_tool_select(kc, params, false);
  km_3d_view_tool_select(kc, params, true);
  km_3d_view_tool_select_box(kc, params, false);
  km_3d_view_tool_select_box(kc, params, true);
  km_3d_view_tool_select_circle(kc, params, false);
  km_3d_view_tool_select_circle(kc, params, true);
  km_3d_view_tool_select_lasso(kc, params, false);
  km_3d_view_tool_select_lasso(kc, params, true);
  km_3d_view_tool_transform(kc, params);
  km_3d_view_tool_move(kc, params);
  km_3d_view_tool_rotate(kc, params);
  km_3d_view_tool_scale(kc, params);
  km_3d_view_tool_shear(kc, params);
  km_3d_view_tool_bend(kc, params);
  km_3d_view_tool_measure(kc, params);

  km_3d_view_tool_pose_breakdowner(kc, params);
  km_3d_view_tool_pose_push(kc, params);
  km_3d_view_tool_pose_relax(kc, params);
}

}  // namespace flipendo::keymap
