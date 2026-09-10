/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * `draw_settings` de `_defs_grease_pencil_edit`
 * (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:2380`), transliterados linea a
 * linea. De las tres herramientas del grupo solo `shear` pinta a mano: `interpolate` se
 * declara con filas y `texture_gradient` no tiene ajustes.
 */

#include "RNA_access.hh"

#include "UI_interface_layout.hh"

#include "FL_tool_settings_ui.hh"

namespace flipendo::ui::settings {

/* -------------------------------------------------------------------- */
/** \name Ayudantes de otros modulos
 * \{ */

/**
 * `_template_widget.VIEW3D_GGT_xform_gizmo.draw_settings_with_index`
 * (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:99`):
 *
 *     scene = context.scene
 *     orient_slot = scene.transform_orientation_slots[index]
 *     layout.prop(orient_slot, "type")
 *
 * El subindice de Python llega a `pyrna_prop_collection_subscript_int`, y como la
 * coleccion no define `lookupint` recorre el iterador hasta el elemento `index`.
 * `RNA_property_collection_lookup_int` hace exactamente ese mismo recorrido, asi que
 * el puntero que sale (dueno la escena, datos `&scene->orientation_slots[index]`) es el
 * mismo que veia el Python.
 *
 * Si el indice se sale de rango el Python lanza `IndexError` y no pinta nada; aqui el
 * puntero queda nulo y `prop()` tampoco pinta nada. Con los indices que se usan (1, 2
 * y 3 de cuatro ranuras) no pasa.
 */
static void xform_gizmo_draw_settings_with_index(const bContext *C,
                                                 uiLayout *layout,
                                                 const int index)
{
  PointerRNA scene_ptr = scene(C);
  PointerRNA orient_slot = PointerRNA_NULL;
  if (scene_ptr.data != nullptr) {
    PropertyRNA *slots = RNA_struct_find_property(&scene_ptr, "transform_orientation_slots");
    RNA_property_collection_lookup_int(&scene_ptr, slots, index, &orient_slot);
  }
  prop(layout, &orient_slot, "type");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name `_defs_grease_pencil_edit`
 * \{ */

/**
 * `_defs_grease_pencil_edit.shear`:
 *
 *     def draw_settings(context, layout, _tool):
 *         _template_widget.VIEW3D_GGT_xform_gizmo.draw_settings_with_index(context, layout, 2)
 *
 * El Python no recibe `extra` ni usa `tool`: se ignoran los dos. El indice 2 es la
 * ranura de ROTACION (`SCE_ORIENT_ROTATE`), no la de por defecto (0) ni la de escala
 * (3), aunque la herramienta cizalla. Es una rareza del Python y se conserva: el `shear`
 * de `_defs_transform` usa ese mismo 2.
 */
void draw_shear(const bContext *C, uiLayout *layout, bToolRef * /*tref*/, bool extra)
{
  /* El Python tiene la firma (context, layout, tool), sin `extra`: pedirle los ajustes
   * "extra" lanzaria TypeError sin pintar nada. Lo encontro la revision de la familia de
   * transformacion, que ya lo hacia asi. */
  if (extra) {
    return;
  }
  xform_gizmo_draw_settings_with_index(C, layout, 2);
}

/** \} */

}  // namespace flipendo::ui::settings
