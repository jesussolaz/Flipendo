/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Mapa de teclado, grupo 27: herramientas de primitivas de Grease Pencil en la vista
 * 3D (polilinea, caja, circulo, arco, curva), cuentagotas de Grease Pencil, modal de
 * la herramienta de interpolacion, gradiente de textura, y las herramientas del
 * editor de video (linea de tiempo y vista previa: tweak, caja de seleccion, cursor
 * y cuchilla).
 * Transliterado de blender_default.py.
 */

#include "FL_keymap_build.hpp"
#include "FL_keymap_params.hpp"

namespace flipendo::keymap {

/* -------------------------------------------------------------------- */
/** \name Herramientas de primitivas de Grease Pencil (vista 3D)
 * \{ */

static void km_3d_view_tool_paint_grease_pencil_primitive_polyline(wmKeyConfig *kc,
                                                                   const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Paint Grease Pencil, Polyline", "VIEW_3D", "WINDOW");

  /* El Python trae `{"properties": []}`: lista vacia, o sea ninguna propiedad. */
  item(km, "grease_pencil.primitive_polyline", ev("LEFTMOUSE", "PRESS"));
  item(km, "grease_pencil.primitive_polyline", ev("LEFTMOUSE", "PRESS").shift());
  /* Seleccion con lazo. */
  item(km, "grease_pencil.select_lasso", ev(params.action_mouse, "CLICK_DRAG").ctrl().alt());
}

static void km_3d_view_tool_paint_grease_pencil_primitive_box(wmKeyConfig *kc,
                                                              const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Paint Grease Pencil, Box", "VIEW_3D", "WINDOW");

  item(km, "grease_pencil.primitive_box", ev("LEFTMOUSE", "PRESS"));
  item(km, "grease_pencil.primitive_box", ev("LEFTMOUSE", "PRESS").shift());
  item(km, "grease_pencil.primitive_box", ev("LEFTMOUSE", "PRESS").alt());
  /* Seleccion con lazo. */
  item(km, "grease_pencil.select_lasso", ev(params.action_mouse, "CLICK_DRAG").ctrl().alt());
}

static void km_3d_view_tool_paint_grease_pencil_primitive_circle(wmKeyConfig *kc,
                                                                 const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Paint Grease Pencil, Circle", "VIEW_3D", "WINDOW");

  item(km, "grease_pencil.primitive_circle", ev("LEFTMOUSE", "PRESS"));
  item(km, "grease_pencil.primitive_circle", ev("LEFTMOUSE", "PRESS").shift());
  item(km, "grease_pencil.primitive_circle", ev("LEFTMOUSE", "PRESS").alt());
  /* Seleccion con lazo. */
  item(km, "grease_pencil.select_lasso", ev(params.action_mouse, "CLICK_DRAG").ctrl().alt());
}

static void km_3d_view_tool_paint_grease_pencil_primitive_arc(wmKeyConfig *kc,
                                                              const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Paint Grease Pencil, Arc", "VIEW_3D", "WINDOW");

  item(km, "grease_pencil.primitive_arc", ev("LEFTMOUSE", "PRESS"));
  item(km, "grease_pencil.primitive_arc", ev("LEFTMOUSE", "PRESS").shift());
  item(km, "grease_pencil.primitive_arc", ev("LEFTMOUSE", "PRESS").alt());
  /* Seleccion con lazo. */
  item(km, "grease_pencil.select_lasso", ev(params.action_mouse, "CLICK_DRAG").ctrl().alt());
}

static void km_3d_view_tool_paint_grease_pencil_primitive_curve(wmKeyConfig *kc,
                                                                const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Paint Grease Pencil, Curve", "VIEW_3D", "WINDOW");

  item(km, "grease_pencil.primitive_curve", ev("LEFTMOUSE", "PRESS"));
  /* Seleccion con lazo. */
  item(km, "grease_pencil.select_lasso", ev(params.action_mouse, "CLICK_DRAG").ctrl().alt());
}

static void km_3d_view_tool_paint_grease_pencil_eyedropper(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Paint Grease Pencil, Eyedropper", "VIEW_3D", "WINDOW");

  item(km, "ui.eyedropper_grease_pencil_color", ev(params.tool_mouse, "PRESS"));
  item(km, "ui.eyedropper_grease_pencil_color", ev(params.tool_mouse, "PRESS").shift());
  item(km, "ui.eyedropper_grease_pencil_color", ev(params.tool_mouse, "PRESS").ctrl());
  item(km, "ui.eyedropper_grease_pencil_color", ev(params.tool_mouse, "PRESS").shift().ctrl());
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Grease Pencil: interpolacion y gradiente de textura
 * \{ */

static void km_grease_pencil_interpolate_tool_modal_map(wmKeyConfig *kc,
                                                        const Params & /*params*/)
{
  wmKeyMap *km = keymap_modal(kc, "Interpolate Tool Modal Map");

  item_modal(km, "CANCEL", ev("ESC", "PRESS").any());
  item_modal(km, "CANCEL", ev("RIGHTMOUSE", "PRESS").any());
  item_modal(km, "CONFIRM", ev("RET", "PRESS").any());
  item_modal(km, "CONFIRM", ev("NUMPAD_ENTER", "PRESS").any());
  item_modal(km, "CONFIRM", ev("LEFTMOUSE", "RELEASE").any());
  item_modal(km, "INCREASE", ev("WHEELUPMOUSE", "PRESS"));
  item_modal(km, "DECREASE", ev("WHEELDOWNMOUSE", "PRESS"));
}

static void km_3d_view_tool_edit_grease_pencil_texture_gradient(wmKeyConfig *kc,
                                                                const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Grease Pencil, Gradient", "VIEW_3D", "WINDOW");

  item(km, "grease_pencil.texture_gradient", params.tool_maybe_tweak_event);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Editor de video: herramientas de la linea de tiempo
 * \{ */

/* `km_sequencer_tool_generic_select_rcs()` del Python: no es un keymap, solo la lista
 * de atajos para cuando la seleccion va con el boton DERECHO. Por eso recibe el
 * keymap ya creado en vez del keyconfig. */
static void km_sequencer_tool_generic_select_rcs(wmKeyMap *km, const Params &params)
{
  item(km, "sequencer.select_handle", ev("LEFTMOUSE", "PRESS"));
  item(km, "sequencer.select_handle", ev("LEFTMOUSE", "PRESS").alt())
      .boolean("ignore_connections", true);
  item(km, "anim.change_frame", ev(params.action_mouse, "PRESS"))
      .boolean("seq_solo_preview", true);
  /* El cambio de fotograma tiene prioridad sobre el deslizamiento de la tira: si la
   * pulsacion cae sobre un tirador se cancela y entra el `transform.seq_slide` de
   * debajo. */
  item(km, "transform.seq_slide", ev("LEFTMOUSE", "PRESS"))
      .boolean("view2d_edge_pan", true)
      .boolean("use_restore_handle_selection", true);
}

/* `km_sequencer_tool_generic_select_lcs()`: lo mismo para seleccion con el IZQUIERDO. */
static void km_sequencer_tool_generic_select_lcs(wmKeyMap *km, const Params & /*params*/)
{
  item(km, "sequencer.select", ev("LEFTMOUSE", "PRESS")).boolean("deselect_all", true);
  item(km, "sequencer.select", ev("LEFTMOUSE", "PRESS").shift()).boolean("toggle", true);
  item(km, "anim.change_frame", ev("RIGHTMOUSE", "PRESS").shift())
      .boolean("seq_solo_preview", true);
}

static void km_sequencer_tool_generic_select_box(wmKeyConfig *kc,
                                                 const Params &params,
                                                 const bool fallback)
{
  /* `_fallback_id()`: el keymap de reserva es otro keymap distinto, con " (fallback)"
   * pegado al nombre. */
  wmKeyMap *km = keymap(kc,
                        fallback ? "Sequencer Tool: Select Box (fallback)" :
                                   "Sequencer Tool: Select Box",
                        "SEQUENCE_EDITOR",
                        "WINDOW");

  /* La herramienta de caja incorpora el tweak, para tener una sola herramienta que
   * sirva a la vez para seleccionar y para transformar. */
  if (params.select_mouse_right) {
    km_sequencer_tool_generic_select_rcs(km, params);
  }
  else {
    km_sequencer_tool_generic_select_lcs(km, params);
  }

  /* Aqui NO se usa `tool_maybe_tweak_event`, igual que en el Python. */
  if (!(fallback && !params.use_fallback_tool)) {
    /* `_template_items_tool_select_actions_simple("sequencer.select_box", ...)`,
     * expandida. 'SET' no se define: lo pone la propia herramienta. */
    const Event base = (fallback && params.use_fallback_tool_select_mouse) ?
                           params.select_tweak_event :
                           params.tool_tweak_event;
    const bool tweak = !params.select_mouse_right;

    item(km, "sequencer.select_box", base).boolean("tweak", tweak);
    item(km, "sequencer.select_box", Event(base).shift())
        .boolean("tweak", tweak)
        .enum_("mode", "ADD");
    item(km, "sequencer.select_box", Event(base).ctrl())
        .boolean("tweak", tweak)
        .enum_("mode", "SUB");
  }
}

static void km_sequencer_tool_blade(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap_tool(kc, "Sequencer Tool: Blade", "SEQUENCE_EDITOR", "WINDOW");

  item(km, "sequencer.split", ev("LEFTMOUSE", "PRESS"))
      .enum_("type", "SOFT")
      .enum_("side", "NO_CHANGE")
      .boolean("use_cursor_position", true)
      .boolean("ignore_selection", true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Editor de video: herramientas de la vista previa
 * \{ */

static void km_sequencer_preview_tool_generic_select(wmKeyConfig *kc,
                                                     const Params &params,
                                                     const bool fallback)
{
  wmKeyMap *km = keymap(kc,
                        fallback ? "Preview Tool: Tweak (fallback)" : "Preview Tool: Tweak",
                        "SEQUENCE_EDITOR",
                        "WINDOW");

  item(km, "sequencer.text_cursor_set", ev("LEFTMOUSE", "PRESS"));
  item(km, "sequencer.text_cursor_set", ev("LEFTMOUSE", "CLICK_DRAG"));

  if (!(fallback && params.select_mouse_right)) {
    /* `_template_items_tool_select(params, "sequencer.select", "sequencer.cursor_set",
     * cursor_prioritize=True, fallback=fallback)`, expandida. */
    bool select_passthrough = false;
    bool handled = false;
    if (!params.legacy) {
      if (!params.select_mouse_right) {
        /* Soporte experimental del boton izquierdo para la herramienta de tweak. */
        select_passthrough = params.use_tweak_select_passthrough;
      }
      /* Con `cursor_prioritize` la rama del boton derecho NO activa el passthrough:
       * en las vistas con linea de tiempo siempre hay que poder barrer el tiempo. */

      if (!fallback && select_passthrough) {
        item(km, "sequencer.select", ev("LEFTMOUSE", "PRESS"))
            .boolean("deselect_all", true)
            .boolean("select_passthrough", true);
        item(km, "sequencer.select", ev("LEFTMOUSE", "CLICK")).boolean("deselect_all", true);
        item(km, "sequencer.select", ev("LEFTMOUSE", "PRESS").shift())
            .boolean("deselect_all", false)
            .boolean("toggle", true);
        handled = true;
      }
    }

    if (!handled) {
      if (!params.select_mouse_right) {
        /* 'PRESS' para seleccionar sin retardo. */
        Item it = item(km, "sequencer.select", ev("LEFTMOUSE", "PRESS"))
                      .boolean("deselect_all", true);
        if (select_passthrough) {
          /* Sin esto la herramienta de reserva no deja pasar el evento. */
          it.boolean("select_passthrough", true);
        }
        item(km, "sequencer.select", ev("LEFTMOUSE", "PRESS").shift()).boolean("toggle", true);

        /* El keymap de reserva tiene que transformar: se supone que a la herramienta
         * principal se llega por los gizmos. */
        if (fallback) {
          item(km, "transform.translate", ev("LEFTMOUSE", "CLICK_DRAG"))
              .boolean("release_confirm", true);
        }
      }
      else {
        /* Con el boton derecho seleccionando, el izquierdo coloca el cursor. */
        item(km, "sequencer.cursor_set", ev("LEFTMOUSE", "PRESS"));
        item(km, "transform.translate", ev("LEFTMOUSE", "CLICK_DRAG"))
            .boolean("release_confirm", true)
            .boolean("cursor_transform", true);
      }
    }
  }

  if (!params.use_fallback_tool_select_handled) {
    /* `_template_sequencer_preview_select(type=params.select_mouse,
     * value=params.select_mouse_value, legacy=params.legacy)`: primero la generica y
     * luego las combinaciones propias de la vista previa. */
    Item base = item(km, "sequencer.select", ev(params.select_mouse, params.select_mouse_value));
    if (!params.legacy) {
      base.boolean("deselect_all", true);
    }
    item(km, "sequencer.select", ev(params.select_mouse, params.select_mouse_value).shift())
        .boolean("toggle", true);

    item(km, "sequencer.select", ev(params.select_mouse, params.select_mouse_value).ctrl())
        .boolean("center", true);
    item(km, "sequencer.select", ev(params.select_mouse, params.select_mouse_value).alt())
        .boolean("ignore_connections", true);
    item(km,
         "sequencer.select",
         ev(params.select_mouse, params.select_mouse_value).shift().ctrl())
        .boolean("toggle", true)
        .boolean("center", true);
    item(km, "sequencer.select", ev(params.select_mouse, params.select_mouse_value).shift().alt())
        .boolean("toggle", true)
        .boolean("ignore_connections", true);
  }
}

static void km_sequencer_preview_tool_generic_select_box(wmKeyConfig *kc,
                                                         const Params &params,
                                                         const bool fallback)
{
  wmKeyMap *km = keymap(kc,
                        fallback ? "Preview Tool: Select Box (fallback)" :
                                   "Preview Tool: Select Box",
                        "SEQUENCE_EDITOR",
                        "WINDOW");

  item(km, "sequencer.text_cursor_set", ev("LEFTMOUSE", "PRESS"));
  item(km, "sequencer.text_cursor_set", ev("LEFTMOUSE", "CLICK_DRAG"));

  /* Aqui NO se usa `tool_maybe_tweak_event`, igual que en el Python. */
  if (!(fallback && !params.use_fallback_tool)) {
    /* `_template_items_tool_select_actions_simple("sequencer.select_box", ...)` sin
     * propiedades extra. */
    const Event base = (fallback && params.use_fallback_tool_select_mouse) ?
                           params.select_tweak_event :
                           params.tool_tweak_event;

    item(km, "sequencer.select_box", base);
    item(km, "sequencer.select_box", Event(base).shift()).enum_("mode", "ADD");
    item(km, "sequencer.select_box", Event(base).ctrl()).enum_("mode", "SUB");
  }
}

static void km_sequencer_preview_tool_generic_cursor(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "Preview Tool: Cursor", "SEQUENCE_EDITOR", "WINDOW");

  item(km, "sequencer.cursor_set", ev(params.tool_mouse, "PRESS"));
  /* No se usa `tool_maybe_tweak_event`: chocaria con el 'PRESS' que coloca el cursor. */
  item(km, "transform.translate", params.tool_tweak_event)
      .boolean("release_confirm", true)
      .boolean("cursor_transform", true);
}

/** \} */

void register_group_27(wmKeyConfig *kc, const Params &params)
{
  km_3d_view_tool_paint_grease_pencil_primitive_polyline(kc, params);
  km_3d_view_tool_paint_grease_pencil_primitive_box(kc, params);
  km_3d_view_tool_paint_grease_pencil_primitive_circle(kc, params);
  km_3d_view_tool_paint_grease_pencil_primitive_arc(kc, params);
  km_3d_view_tool_paint_grease_pencil_primitive_curve(kc, params);
  km_3d_view_tool_paint_grease_pencil_eyedropper(kc, params);
  km_grease_pencil_interpolate_tool_modal_map(kc, params);
  km_3d_view_tool_edit_grease_pencil_texture_gradient(kc, params);

  /* `for fallback in (False, True)`: cada una genera dos keymaps distintos. */
  km_sequencer_tool_generic_select_box(kc, params, false);
  km_sequencer_tool_generic_select_box(kc, params, true);
  km_sequencer_preview_tool_generic_select(kc, params, false);
  km_sequencer_preview_tool_generic_select(kc, params, true);
  km_sequencer_preview_tool_generic_select_box(kc, params, false);
  km_sequencer_preview_tool_generic_select_box(kc, params, true);

  km_sequencer_preview_tool_generic_cursor(kc, params);
  km_sequencer_tool_blade(kc, params);
}

}  // namespace flipendo::keymap
