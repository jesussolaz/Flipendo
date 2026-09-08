/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Mapa de teclado, grupo 22: sistema de herramientas — las de anotacion (validas en
 * cualquier editor), las del editor de imagen y del editor de UV (cursor, seleccion,
 * rip, escultura de UV y transformaciones) y las de seleccion del editor de nodos.
 * Transliterado de blender_default.py.
 */

#include <cstring>

#include "FL_keymap_build.hpp"
#include "FL_keymap_params.hpp"

namespace flipendo::keymap {

/* -------------------------------------------------------------------- */
/** \name Plantillas usadas por este grupo
 *
 * Solo las que se repiten dentro del propio lote; el resto van expandidas en linea
 * donde toca.
 * \{ */

/** `_template_node_select(type=, value=, select_passthrough=)`. */
static void template_node_select(wmKeyMap *km,
                                 const char *type,
                                 const char *value,
                                 const bool select_passthrough)
{
  item(km, "node.select", ev(type, value)).boolean("select_passthrough", select_passthrough);
  item(km, "node.select", ev(type, value).ctrl());
  item(km, "node.select", ev(type, value).alt());
  item(km, "node.select", ev(type, value).ctrl().alt());
  item(km, "node.select", ev(type, value).shift()).boolean("toggle", true);
  item(km, "node.select", ev(type, value).shift().ctrl()).boolean("toggle", true);
  item(km, "node.select", ev(type, value).shift().alt()).boolean("toggle", true);
  item(km, "node.select", ev(type, value).shift().ctrl().alt()).boolean("toggle", true);

  if (select_passthrough && std::strcmp(value, "PRESS") == 0) {
    /* Con paso a traves hace falta un CLICK aparte que si deseleccione lo demas. */
    item(km, "node.select", ev(type, "CLICK")).boolean("deselect_all", true);
  }
}

/**
 * `{**params.tool_maybe_tweak_event, **params.tool_modifier}` del Python.
 *
 * `tool_modifier` solo llega a valer `{"alt": -1}` (Alt en cualquier estado), y solo
 * cuando se selecciona con el boton izquierdo y `use_alt_tool_or_cursor` esta puesto;
 * con los parametros por defecto esta vacio, asi que no afecta al baseline.
 */
static Event tool_maybe_tweak_with_modifier(const Params &params)
{
  Event e = params.tool_maybe_tweak_event;
  if (params.tool_modifier_alt_any) {
    /* TODO(keymap): falta `"alt": -1` (Alt pulsado o no). `Event` solo sabe pedir Alt
     * PULSADO (`.alt()`) o TODOS los modificadores en cualquier estado (`.any()`);
     * el equivalente exacto seria `KMI_PARAMS_MOD_TO_ANY(KM_ALT)` en el andamiaje.
     * No se pone `.alt()` a proposito: exigiria Alt en vez de dejarlo indiferente,
     * que es un atajo distinto. */
  }
  return e;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Herramientas de anotacion (cualquier editor)
 * \{ */

static void km_generic_tool_annotate(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "Generic Tool: Annotate", "EMPTY", "WINDOW");

  item(km, "gpencil.annotate", ev(params.tool_mouse, "PRESS"))
      .enum_("mode", "DRAW")
      .boolean("wait_for_input", false);
  item(km, "gpencil.annotate", ev(params.tool_mouse, "PRESS").ctrl())
      .enum_("mode", "ERASER")
      .boolean("wait_for_input", false);
}

static void km_generic_tool_annotate_line(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "Generic Tool: Annotate Line", "EMPTY", "WINDOW");

  item(km, "gpencil.annotate", params.tool_maybe_tweak_event)
      .enum_("mode", "DRAW_STRAIGHT")
      .boolean("wait_for_input", false);
  item(km, "gpencil.annotate", ev(params.tool_mouse, "PRESS").ctrl())
      .enum_("mode", "ERASER")
      .boolean("wait_for_input", false);
}

static void km_generic_tool_annotate_polygon(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "Generic Tool: Annotate Polygon", "EMPTY", "WINDOW");

  item(km, "gpencil.annotate", ev(params.tool_mouse, "PRESS"))
      .enum_("mode", "DRAW_POLY")
      .boolean("wait_for_input", false);
  item(km, "gpencil.annotate", ev(params.tool_mouse, "PRESS").ctrl())
      .enum_("mode", "ERASER")
      .boolean("wait_for_input", false);
}

static void km_generic_tool_annotate_eraser(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "Generic Tool: Annotate Eraser", "EMPTY", "WINDOW");

  item(km, "gpencil.annotate", ev(params.tool_mouse, "PRESS"))
      .enum_("mode", "ERASER")
      .boolean("wait_for_input", false);
  item(km, "gpencil.annotate", ev(params.tool_mouse, "PRESS").ctrl())
      .enum_("mode", "ERASER")
      .boolean("wait_for_input", false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Herramientas del editor de imagen
 * \{ */

static void km_image_editor_tool_generic_sample(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "Image Editor Tool: Sample", "IMAGE_EDITOR", "WINDOW");

  item(km, "image.sample", ev(params.tool_mouse, "PRESS"));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Herramientas del editor de UV
 * \{ */

static void km_image_editor_tool_uv_cursor(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "Image Editor Tool: Uv, Cursor", "IMAGE_EDITOR", "WINDOW");

  item(km, "uv.cursor_set", ev(params.tool_mouse, "PRESS"));
  /* No se usa `tool_maybe_tweak_event`: chocaria con el PRESS que coloca el cursor. */
  item(km, "transform.translate", params.tool_tweak_event)
      .boolean("release_confirm", true)
      .boolean("cursor_transform", true);
}

static void km_image_editor_tool_uv_select(wmKeyConfig *kc,
                                           const Params &params,
                                           const bool fallback)
{
  wmKeyMap *km = keymap(kc,
                        fallback ? "Image Editor Tool: Uv, Tweak (fallback)" :
                                   "Image Editor Tool: Uv, Tweak",
                        "IMAGE_EDITOR",
                        "WINDOW");

  /* `_template_items_tool_select(params, "uv.select", "uv.cursor_set", fallback=fallback)`,
   * expandida en linea. `cursor_prioritize` es False para esta herramienta. */
  if (!(fallback && params.select_mouse_right)) {
    bool select_passthrough = false;
    bool emitted = false;

    if (!params.legacy) {
      /* Soporte experimental de interaccion con el boton izquierdo, ver #96544. */
      if (!params.select_mouse_right) {
        select_passthrough = params.use_tweak_select_passthrough;
      }
      else {
        select_passthrough = true;
      }

      if (!fallback && select_passthrough) {
        item(km, "uv.select", ev("LEFTMOUSE", "PRESS"))
            .boolean("deselect_all", true)
            .boolean("select_passthrough", true);
        item(km, "uv.select", ev("LEFTMOUSE", "CLICK")).boolean("deselect_all", true);
        item(km, "uv.select", ev("LEFTMOUSE", "PRESS").shift())
            .boolean("deselect_all", false)
            .boolean("toggle", true);
        emitted = true;
      }
    }

    if (!emitted) {
      if (!params.select_mouse_right) {
        /* PRESS para seleccionar sin retardo. */
        Item kmi = item(km, "uv.select", ev("LEFTMOUSE", "PRESS"));
        kmi.boolean("deselect_all", true);
        if (select_passthrough) {
          /* Sin esto la herramienta de reserva no deja pasar el evento, ver #115887. */
          kmi.boolean("select_passthrough", true);
        }
        item(km, "uv.select", ev("LEFTMOUSE", "PRESS").shift()).boolean("toggle", true);

        /* El keymap de reserva tiene que transformar, porque en ese caso se espera
         * llegar a la herramienta principal por los gizmos. Ver #96885. */
        if (fallback) {
          item(km, "transform.translate", ev("LEFTMOUSE", "CLICK_DRAG"))
              .boolean("release_confirm", true);
        }
      }
      else {
        /* Con el boton derecho seleccionando, el izquierdo coloca el cursor. */
        item(km, "uv.cursor_set", ev("LEFTMOUSE", "PRESS"));
        item(km, "transform.translate", ev("LEFTMOUSE", "CLICK_DRAG"))
            .boolean("release_confirm", true)
            .boolean("cursor_transform", true);
      }
    }
  }

  /* `_template_uv_select(type=params.select_mouse, value=params.select_mouse_value,
   * select_passthrough=params.use_tweak_select_passthrough, legacy=params.legacy)`. */
  if (!params.use_fallback_tool_select_handled) {
    bool select_passthrough = params.use_tweak_select_passthrough;
    /* Ver la documentacion de `use_tweak_select_passthrough`: con CLICK o RELEASE no
     * hay nada por lo que dejar pasar el evento. */
    if (select_passthrough && (std::strcmp(params.select_mouse_value, "CLICK") == 0 ||
                               std::strcmp(params.select_mouse_value, "RELEASE") == 0))
    {
      select_passthrough = false;
    }

    Item kmi = item(km, "uv.select", ev(params.select_mouse, params.select_mouse_value));
    if (!params.legacy) {
      kmi.boolean("deselect_all", true);
    }
    if (select_passthrough) {
      kmi.boolean("select_passthrough", true);
    }
    item(km, "uv.select", ev(params.select_mouse, params.select_mouse_value).shift())
        .boolean("toggle", true);

    if (select_passthrough) {
      /* CLICK extra para poder deseleccionar el resto cuando el evento se deja pasar. */
      item(km, "uv.select", ev(params.select_mouse, "CLICK")).boolean("deselect_all", true);
    }
  }
}

static void km_image_editor_tool_uv_select_box(wmKeyConfig *kc,
                                               const Params &params,
                                               const bool fallback)
{
  wmKeyMap *km = keymap(kc,
                        fallback ? "Image Editor Tool: Uv, Select Box (fallback)" :
                                   "Image Editor Tool: Uv, Select Box",
                        "IMAGE_EDITOR",
                        "WINDOW");

  if (!(fallback && !params.use_fallback_tool)) {
    /* No se usa `tool_maybe_tweak_event`, ver el comentario de esta ranura. */
    const Event base = (fallback && params.use_fallback_tool_select_mouse) ?
                           params.select_tweak_event :
                           params.tool_tweak_event;

    /* `_template_items_tool_select_actions_simple`: 'SET' no se define aqui, sale de
     * las opciones de la herramienta. */
    item(km, "uv.select_box", base);
    item(km, "uv.select_box", Event(base).shift()).enum_("mode", "ADD");
    item(km, "uv.select_box", Event(base).ctrl()).enum_("mode", "SUB");
  }
}

static void km_image_editor_tool_uv_select_circle(wmKeyConfig *kc,
                                                  const Params &params,
                                                  const bool fallback)
{
  wmKeyMap *km = keymap(kc,
                        fallback ? "Image Editor Tool: Uv, Select Circle (fallback)" :
                                   "Image Editor Tool: Uv, Select Circle",
                        "IMAGE_EDITOR",
                        "WINDOW");

  if (!(fallback && !params.use_fallback_tool)) {
    const Event base = (fallback && params.use_fallback_tool_select_mouse) ?
                           params.select_tweak_event :
                           ev(params.tool_mouse, "PRESS");

    item(km, "uv.select_circle", base).boolean("wait_for_input", false);
    item(km, "uv.select_circle", Event(base).shift())
        .boolean("wait_for_input", false)
        .enum_("mode", "ADD");
    item(km, "uv.select_circle", Event(base).ctrl())
        .boolean("wait_for_input", false)
        .enum_("mode", "SUB");
  }
  /* Sin seleccion de reserva: esta herramienta actua ya en el PRESS. */
}

static void km_image_editor_tool_uv_select_lasso(wmKeyConfig *kc,
                                                 const Params &params,
                                                 const bool fallback)
{
  wmKeyMap *km = keymap(kc,
                        fallback ? "Image Editor Tool: Uv, Select Lasso (fallback)" :
                                   "Image Editor Tool: Uv, Select Lasso",
                        "IMAGE_EDITOR",
                        "WINDOW");

  if (!(fallback && !params.use_fallback_tool)) {
    const Event base = (fallback && params.use_fallback_tool_select_mouse) ?
                           params.select_tweak_event :
                           params.tool_tweak_event;

    item(km, "uv.select_lasso", base);
    item(km, "uv.select_lasso", Event(base).shift()).enum_("mode", "ADD");
    item(km, "uv.select_lasso", Event(base).ctrl()).enum_("mode", "SUB");
  }
}

static void km_image_editor_tool_uv_rip_region(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "Image Editor Tool: Uv, Rip Region", "IMAGE_EDITOR", "WINDOW");

  Item kmi = item(km, "uv.rip_move", tool_maybe_tweak_with_modifier(params));
  kmi.sub("TRANSFORM_OT_translate").boolean("release_confirm", true);
}

static void km_image_editor_tool_uv_grab(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "Image Editor Tool: Uv, Grab", "IMAGE_EDITOR", "WINDOW");

  item(km, "sculpt.uv_sculpt_grab", ev(params.tool_mouse, "PRESS"));
  item(km, "sculpt.uv_sculpt_grab", ev(params.tool_mouse, "PRESS").ctrl())
      .boolean("use_invert", true);
  item(km, "sculpt.uv_sculpt_relax", ev(params.tool_mouse, "PRESS").shift());
  item(km, "wm.radial_control", ev("F", "PRESS"))
      .string("data_path_primary", "tool_settings.uv_sculpt.size");
  item(km, "wm.radial_control", ev("F", "PRESS").shift())
      .string("data_path_primary", "tool_settings.uv_sculpt.strength");
}

static void km_image_editor_tool_uv_relax(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "Image Editor Tool: Uv, Relax", "IMAGE_EDITOR", "WINDOW");

  item(km, "sculpt.uv_sculpt_relax", ev(params.tool_mouse, "PRESS"));
  item(km, "sculpt.uv_sculpt_relax", ev(params.tool_mouse, "PRESS").ctrl())
      .boolean("use_invert", true);
  item(km, "sculpt.uv_sculpt_relax", ev(params.tool_mouse, "PRESS").shift());
  item(km, "wm.radial_control", ev("F", "PRESS"))
      .string("data_path_primary", "tool_settings.uv_sculpt.size");
  item(km, "wm.radial_control", ev("F", "PRESS").shift())
      .string("data_path_primary", "tool_settings.uv_sculpt.strength");
}

static void km_image_editor_tool_uv_pinch(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "Image Editor Tool: Uv, Pinch", "IMAGE_EDITOR", "WINDOW");

  item(km, "sculpt.uv_sculpt_pinch", ev(params.tool_mouse, "PRESS"));
  item(km, "sculpt.uv_sculpt_pinch", ev(params.tool_mouse, "PRESS").ctrl())
      .boolean("use_invert", true);
  item(km, "sculpt.uv_sculpt_relax", ev(params.tool_mouse, "PRESS").shift());
  item(km, "wm.radial_control", ev("F", "PRESS"))
      .string("data_path_primary", "tool_settings.uv_sculpt.size");
  item(km, "wm.radial_control", ev("F", "PRESS").shift())
      .string("data_path_primary", "tool_settings.uv_sculpt.strength");
}

static void km_image_editor_tool_uv_move(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "Image Editor Tool: Uv, Move", "IMAGE_EDITOR", "WINDOW");

  item(km, "transform.translate", tool_maybe_tweak_with_modifier(params))
      .boolean("release_confirm", true);
}

static void km_image_editor_tool_uv_rotate(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "Image Editor Tool: Uv, Rotate", "IMAGE_EDITOR", "WINDOW");

  item(km, "transform.rotate", tool_maybe_tweak_with_modifier(params))
      .boolean("release_confirm", true);
}

static void km_image_editor_tool_uv_scale(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "Image Editor Tool: Uv, Scale", "IMAGE_EDITOR", "WINDOW");

  item(km, "transform.resize", tool_maybe_tweak_with_modifier(params))
      .boolean("release_confirm", true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Herramientas del editor de nodos
 * \{ */

static void km_node_editor_tool_select(wmKeyConfig *kc,
                                       const Params &params,
                                       const bool fallback)
{
  wmKeyMap *km = keymap(kc,
                        fallback ? "Node Tool: Tweak (fallback)" : "Node Tool: Tweak",
                        "NODE_EDITOR",
                        "WINDOW");

  /* El keymap del editor de nodos ya selecciona, aqui no hace falta nada.
   * OJO: a proposito no se mira `fallback` (al contrario que en las demas
   * herramientas de tipo "tweak"): esto solo vale para seleccion con el izquierdo,
   * que si no se activaria en el CLICK y no en el PRESS. */
  if (!params.select_mouse_right) {
    template_node_select(km, params.select_mouse, "PRESS", true);
  }
}

static void km_node_editor_tool_select_box(wmKeyConfig *kc,
                                           const Params &params,
                                           const bool fallback)
{
  wmKeyMap *km = keymap(kc,
                        fallback ? "Node Tool: Select Box (fallback)" : "Node Tool: Select Box",
                        "NODE_EDITOR",
                        "WINDOW");

  if (!(fallback && !params.use_fallback_tool)) {
    /* No se usa `tool_maybe_tweak_event`, ver el comentario de esta ranura. */
    const Event base = (fallback && params.use_fallback_tool_select_mouse) ?
                           params.select_tweak_event :
                           params.tool_tweak_event;

    item(km, "node.select_box", base).boolean("tweak", true);
    item(km, "node.select_box", Event(base).shift())
        .boolean("tweak", true)
        .enum_("mode", "ADD");
    item(km, "node.select_box", Event(base).ctrl())
        .boolean("tweak", true)
        .enum_("mode", "SUB");
  }

  if (!params.select_mouse_right) {
    template_node_select(km, "LEFTMOUSE", "PRESS", true);
  }
}

/** \} */

void register_group_22(wmKeyConfig *kc, const Params &params)
{
  km_generic_tool_annotate(kc, params);
  km_generic_tool_annotate_line(kc, params);
  km_generic_tool_annotate_polygon(kc, params);
  km_generic_tool_annotate_eraser(kc, params);

  km_image_editor_tool_generic_sample(kc, params);
  km_image_editor_tool_uv_cursor(kc, params);
  /* Las herramientas de seleccion se registran dos veces, la segunda como keymap de
   * reserva ("(fallback)"), igual que el bucle `for fallback in (False, True)`. */
  km_image_editor_tool_uv_select(kc, params, false);
  km_image_editor_tool_uv_select(kc, params, true);
  km_image_editor_tool_uv_select_box(kc, params, false);
  km_image_editor_tool_uv_select_box(kc, params, true);
  km_image_editor_tool_uv_select_circle(kc, params, false);
  km_image_editor_tool_uv_select_circle(kc, params, true);
  km_image_editor_tool_uv_select_lasso(kc, params, false);
  km_image_editor_tool_uv_select_lasso(kc, params, true);
  km_image_editor_tool_uv_rip_region(kc, params);
  km_image_editor_tool_uv_grab(kc, params);
  km_image_editor_tool_uv_relax(kc, params);
  km_image_editor_tool_uv_pinch(kc, params);
  km_image_editor_tool_uv_move(kc, params);
  km_image_editor_tool_uv_rotate(kc, params);
  km_image_editor_tool_uv_scale(kc, params);

  km_node_editor_tool_select(kc, params, false);
  km_node_editor_tool_select(kc, params, true);
  km_node_editor_tool_select_box(kc, params, false);
  km_node_editor_tool_select_box(kc, params, true);
}

}  // namespace flipendo::keymap
