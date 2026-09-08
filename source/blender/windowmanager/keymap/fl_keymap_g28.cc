/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Mapa de teclado, grupo 28: herramientas del previo del editor de secuencias
 * (muestrear, mover, rotar, escalar) y las herramientas de interpolacion de Grease
 * Pencil de la vista 3D (modo edicion y modo pintura).
 * Transliterado de blender_default.py.
 */

#include "FL_keymap_build.hpp"
#include "FL_keymap_params.hpp"

namespace flipendo::keymap {

/* -------------------------------------------------------------------- */
/** \name Herramientas del previo del editor de secuencias
 *
 * Las tres transformaciones (mover, rotar, escalar) usan en el Python
 * `{**params.tool_maybe_tweak_event, **params.tool_modifier}`. `tool_modifier` esta
 * vacio salvo con `use_alt_tool_or_cursor` y seleccion con el boton izquierdo, donde
 * vale `{"alt": -1}` (Alt en KM_ANY). Ver los TODO de cada funcion.
 * \{ */

static void km_sequencer_preview_tool_sample(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Preview Tool: Sample", "SEQUENCE_EDITOR", "WINDOW");

  item(km, "sequencer.sample", ev(params.tool_mouse, "PRESS"));
}

static void km_sequencer_preview_tool_move(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Preview Tool: Move", "SEQUENCE_EDITOR", "WINDOW");

  /* TODO(keymap): falta `params.tool_modifier`. Cuando `params.tool_modifier_alt_any`
   * es true el Python fusiona `{"alt": -1}` en el evento, o sea Alt en estado KM_ANY;
   * Event solo tiene modificadores booleanos y un `.any()` global que afectaria a
   * todos, asi que ese caso no se puede escribir hoy. Con los valores por defecto
   * (tool_modifier_alt_any == false) el atajo resultante es exactamente este. */
  item(km, "transform.translate", params.tool_maybe_tweak_event)
      .boolean("release_confirm", true);
}

static void km_sequencer_preview_tool_rotate(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Preview Tool: Rotate", "SEQUENCE_EDITOR", "WINDOW");

  /* TODO(keymap): falta `params.tool_modifier` (`{"alt": -1}` cuando
   * `params.tool_modifier_alt_any`), igual que en km_sequencer_preview_tool_move. */
  item(km, "transform.rotate", params.tool_maybe_tweak_event)
      .boolean("release_confirm", true);
}

static void km_sequencer_preview_tool_scale(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Preview Tool: Scale", "SEQUENCE_EDITOR", "WINDOW");

  /* TODO(keymap): falta `params.tool_modifier` (`{"alt": -1}` cuando
   * `params.tool_modifier_alt_any`), igual que en km_sequencer_preview_tool_move. */
  item(km, "transform.resize", params.tool_maybe_tweak_event)
      .boolean("release_confirm", true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Interpolacion de Grease Pencil en la vista 3D
 * \{ */

static void km_3d_view_tool_edit_grease_pencil_interpolate(wmKeyConfig *kc,
                                                           const Params &params)
{
  wmKeyMap *km = keymap(
      kc, "3D View Tool: Edit Grease Pencil, Interpolate", "VIEW_3D", "WINDOW");

  item(km, "grease_pencil.interpolate", params.tool_maybe_tweak_event)
      .boolean("use_selection", true);
}

static void km_3d_view_tool_paint_grease_pencil_interpolate(wmKeyConfig *kc,
                                                            const Params &params)
{
  wmKeyMap *km = keymap(
      kc, "3D View Tool: Paint Grease Pencil, Interpolate", "VIEW_3D", "WINDOW");

  item(km, "grease_pencil.interpolate", params.tool_maybe_tweak_event)
      .boolean("use_selection", false);
}

/** \} */

void register_group_28(wmKeyConfig *kc, const Params &params)
{
  km_sequencer_preview_tool_sample(kc, params);
  km_sequencer_preview_tool_move(kc, params);
  km_sequencer_preview_tool_rotate(kc, params);
  km_sequencer_preview_tool_scale(kc, params);
  km_3d_view_tool_edit_grease_pencil_interpolate(kc, params);
  km_3d_view_tool_paint_grease_pencil_interpolate(kc, params);
}

}  // namespace flipendo::keymap
