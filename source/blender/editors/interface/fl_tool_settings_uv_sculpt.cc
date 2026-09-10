/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * `draw_settings` de las herramientas de escultura de UV. Transliteracion linea a
 * linea de `_defs_image_uv_sculpt` (`scripts/startup/bl_ui/space_toolsystem_toolbar.py`,
 * `grab`, `relax` y `pinch`, hacia la linea 2603).
 *
 * En el Python son tres funciones copiadas. Aqui se mantienen las tres, cada una con
 * su cuerpo completo y en el mismo orden, para poder revisarlas con el Python al lado;
 * `grab` y `pinch` son identicas, y `relax` anade su fila de metodo DETRAS de los dos
 * popovers, no delante.
 *
 * Los popovers solo ponen el boton: el contenido de `IMAGE_PT_uv_sculpt_curve` e
 * `IMAGE_PT_uv_sculpt_options` lo pinta su panel (`space_image.py`) al abrirse, y se
 * busca por nombre en `uiItemPopoverPanel`, lo mismo que hace `rna_uiItemPopoverPanel`.
 *
 * El Python no acepta `extra` ni, salvo `relax`, usa `tool`; aqui se reciben y se
 * ignoran. `extra` solo llegaria a verdadero desde el popover
 * `TOPBAR_PT_tool_settings_extra`, que estas herramientas no ponen (en el Python esa
 * llamada lanzaria TypeError).
 */

#include "FL_tool_settings_ui.hh"

namespace flipendo::ui::settings {

/* `context.scene.tool_settings.uv_sculpt`. Se recorre el mismo camino que el Python,
 * pasando por la escena. `uv_sculpt` es un struct embebido en `ToolSettings`
 * (`UvSculpt uvsculpt`), asi que nunca sale nulo con escena. */
static PointerRNA uv_sculpt_get(const bContext *C)
{
  PointerRNA scene_ptr = scene(C);
  PointerRNA tool_settings_ptr = pointer_get(&scene_ptr, "tool_settings");
  return pointer_get(&tool_settings_ptr, "uv_sculpt");
}

void draw_uv_sculpt_grab(const bContext *C,
                         uiLayout *layout,
                         bToolRef * /*tref*/,
                         const bool /*extra*/)
{
  PointerRNA uv_sculpt = uv_sculpt_get(C);
  prop(layout, &uv_sculpt, "size");
  prop(layout, &uv_sculpt, "strength");
  popover(layout, C, "IMAGE_PT_uv_sculpt_curve");
  popover(layout, C, "IMAGE_PT_uv_sculpt_options");
}

void draw_uv_sculpt_relax(const bContext *C,
                          uiLayout *layout,
                          bToolRef *tref,
                          const bool /*extra*/)
{
  PointerRNA uv_sculpt = uv_sculpt_get(C);
  prop(layout, &uv_sculpt, "size");
  prop(layout, &uv_sculpt, "strength");
  popover(layout, C, "IMAGE_PT_uv_sculpt_curve");
  popover(layout, C, "IMAGE_PT_uv_sculpt_options");

  /* Rareza conservada: la fila del metodo va DETRAS de los dos popovers, no junto a
   * tamano y fuerza. En la cabecera queda a la derecha de los botones desplegables. */
  PointerRNA props = op_props(tref, "sculpt.uv_sculpt_relax");
  prop(layout, &props, "relax_method", Prop().text("Method"));
}

void draw_uv_sculpt_pinch(const bContext *C,
                          uiLayout *layout,
                          bToolRef * /*tref*/,
                          const bool /*extra*/)
{
  PointerRNA uv_sculpt = uv_sculpt_get(C);
  prop(layout, &uv_sculpt, "size");
  prop(layout, &uv_sculpt, "strength");
  popover(layout, C, "IMAGE_PT_uv_sculpt_curve");
  popover(layout, C, "IMAGE_PT_uv_sculpt_options");
}

}  // namespace flipendo::ui::settings
