/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Ajustes de las herramientas de transformar de la vista 3D: los `draw_settings` de
 * `_defs_transform` (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:301`) y lo que
 * usan de `_template_widget` (`:89`). Transliteracion linea a linea; las declaraciones de
 * las herramientas estan en `windowmanager/toolsystem/fl_tool_defs_transform.cc`.
 *
 * Las seis acaban en `_template_widget.VIEW3D_GGT_xform_gizmo.draw_settings_with_index`,
 * que pinta la orientacion de UNA ranura de la escena elegida por indice. El indice es el
 * de la ranura del gesto: 1 mover, 2 girar, 3 escalar (la 0 es la de por defecto). Las
 * que no son ni mover, ni girar, ni escalar reutilizan la ranura del gesto parecido: la
 * jaula la de escalar y el sesgado la de girar.
 *
 * Los nombres llevan el prefijo `draw_transform_` y el nombre de la funcion del Python
 * (`translate`, no `move`), porque el editor de imagen y el de secuencias tienen tambien
 * un `builtin.move`, un `builtin.rotate`... y todas comparten este espacio de nombres.
 */

#include "BKE_context.hh"

#include "DNA_scene_types.h"

#include "RNA_access.hh"

#include "UI_interface_layout.hh"

#include "FL_tool_settings_ui.hh"

namespace flipendo::ui::settings {

/* -------------------------------------------------------------------- */
/** \name Ayudantes compartidos
 *
 * En el Python son funciones de clase que llaman los `draw_settings`; aqui son `static`
 * porque solo las usa este fichero. El sesgado del lapiz de cera (`builtin.shear` de
 * `_defs_grease_pencil_edit`) tambien llama a `draw_settings_with_index`, pero es de otra
 * familia y lleva su propia copia.
 * \{ */

/**
 * `_template_widget.VIEW3D_GGT_xform_gizmo.draw_settings_with_index`
 * (`space_toolsystem_toolbar.py:99`).
 *
 * Sustituye a la declaracion sin cuerpo de `fl_tool_defs_transform.hh`: aquella estaba en
 * el modulo `wm` y con otra firma, y el dibujo vive en `editors/interface`.
 */
static void xform_gizmo_draw_settings_with_index(const bContext *C,
                                                 uiLayout *layout,
                                                 const int index)
{
  /* scene = context.scene */
  PointerRNA scene_ptr = scene(C);
  if (scene_ptr.data == nullptr) {
    return;
  }
  /* orient_slot = scene.transform_orientation_slots[index]
   *
   * `pyrna_prop_collection_subscript_int`: la coleccion no tiene `lookupint`, asi que el
   * Python la recorre hasta el elemento `index`, que es lo mismo que hace
   * `RNA_property_collection_lookup_int` sin la funcion. Fuera de rango el Python lanzaria
   * `IndexError` y no pintaria nada; aqui tampoco. Con las cuatro ranuras de la escena y los
   * indices 1, 2 y 3 no pasa. */
  PropertyRNA *slots_prop = RNA_struct_find_property(&scene_ptr, "transform_orientation_slots");
  if (slots_prop == nullptr) {
    return;
  }
  PointerRNA orient_slot;
  if (!RNA_property_collection_lookup_int(&scene_ptr, slots_prop, index, &orient_slot)) {
    return;
  }
  /* layout.prop(orient_slot, "type") */
  prop(layout, &orient_slot, "type");
}

/**
 * `_defs_transform.draw_transform_sculpt_tool_settings` (`space_toolsystem_toolbar.py:303`).
 *
 * Sustituye a la declaracion sin cuerpo de `fl_tool_defs_transform.hh`, por lo mismo que
 * la anterior.
 */
static void draw_transform_sculpt_tool_settings(const bContext *C, uiLayout *layout)
{
  /* if context.mode != 'SCULPT': return
   *
   * `context.mode` es `rna_Context_mode_get`, o sea `CTX_data_mode_enum`, y 'SCULPT' es
   * `CTX_MODE_SCULPT` en `rna_enum_context_mode_items`. Solo el escultor de mallas: el de
   * curvas y el de lapiz de cera son otros modos y no pintan nada. */
  if (CTX_data_mode_enum(C) != CTX_MODE_SCULPT) {
    return;
  }
  /* layout.prop(context.tool_settings.sculpt, "transform_mode")
   *
   * Con `sculpt` a `None` el Python lanzaria `TypeError` (el `data` de `prop` es
   * `PROP_NEVER_NULL`) y el `draw_settings` que llama se cortaria sin pintar la
   * orientacion; aqui `prop()` no pinta nada y la orientacion sigue. No se reproduce porque
   * no puede pasar: al entrar en escultura `BKE_paint_init(..., PaintMode::Sculpt, ...)`
   * (`sculpt_ops.cc`) crea `tool_settings.sculpt`, y este `if` ya exige ese modo. */
  PointerRNA ts = tool_settings(C);
  PointerRNA sculpt = pointer_get(&ts, "sculpt");
  prop(layout, &sculpt, "transform_mode");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name `_defs_transform`
 *
 * Ninguno de estos `draw_settings` acepta `extra`: en el Python su firma es
 * `(context, layout, _tool)`, y llamarlos con `extra=True` (lo que hace el popover
 * `TOPBAR_PT_tool_settings_extra`) lanzaria `TypeError` antes de pintar nada. Ese popover
 * solo se abre desde herramientas que lo piden con su propio boton "...", y ninguna de
 * estas lo tiene, asi que no deberia llegar; pero si llega, se reproduce lo mismo que el
 * Python, que es no pintar nada.
 * \{ */

/* `_defs_transform.translate` (`space_toolsystem_toolbar.py:309`), `builtin.move`. */
void draw_transform_translate(const bContext *C,
                              uiLayout *layout,
                              bToolRef * /*tref*/,
                              const bool extra)
{
  if (extra) {
    return;
  }
  /* _defs_transform.draw_transform_sculpt_tool_settings(context, layout) */
  draw_transform_sculpt_tool_settings(C, layout);
  /* _template_widget.VIEW3D_GGT_xform_gizmo.draw_settings_with_index(context, layout, 1) */
  xform_gizmo_draw_settings_with_index(C, layout, 1);
}

/* `_defs_transform.rotate` (`space_toolsystem_toolbar.py:325`), `builtin.rotate`. */
void draw_transform_rotate(const bContext *C,
                           uiLayout *layout,
                           bToolRef * /*tref*/,
                           const bool extra)
{
  if (extra) {
    return;
  }
  /* _defs_transform.draw_transform_sculpt_tool_settings(context, layout) */
  draw_transform_sculpt_tool_settings(C, layout);
  /* _template_widget.VIEW3D_GGT_xform_gizmo.draw_settings_with_index(context, layout, 2) */
  xform_gizmo_draw_settings_with_index(C, layout, 2);
}

/* `_defs_transform.scale` (`space_toolsystem_toolbar.py:341`), `builtin.scale`. */
void draw_transform_scale(const bContext *C,
                          uiLayout *layout,
                          bToolRef * /*tref*/,
                          const bool extra)
{
  if (extra) {
    return;
  }
  /* _defs_transform.draw_transform_sculpt_tool_settings(context, layout) */
  draw_transform_sculpt_tool_settings(C, layout);
  /* _template_widget.VIEW3D_GGT_xform_gizmo.draw_settings_with_index(context, layout, 3) */
  xform_gizmo_draw_settings_with_index(C, layout, 3);
}

/* `_defs_transform.scale_cage` (`space_toolsystem_toolbar.py:357`), `builtin.scale_cage`.
 *
 * Rareza conservada: a diferencia de mover, girar y escalar, NO llama a
 * `draw_transform_sculpt_tool_settings`. Hoy da igual, porque la jaula no sale en el modo
 * de escultura, pero si alguien la anadiera alli no pintaria el modo de transformacion. */
void draw_transform_scale_cage(const bContext *C,
                               uiLayout *layout,
                               bToolRef * /*tref*/,
                               const bool extra)
{
  if (extra) {
    return;
  }
  /* _template_widget.VIEW3D_GGT_xform_gizmo.draw_settings_with_index(context, layout, 3) */
  xform_gizmo_draw_settings_with_index(C, layout, 3);
}

/* `_defs_transform.shear` (`space_toolsystem_toolbar.py:371`), `builtin.shear`.
 *
 * Rarezas conservadas: pinta la ranura 2, la de GIRAR, no una propia; y el Python tiene
 * comentada la linea `props = tool.operator_properties("transform.shear")`. Se deja sin
 * llamar a proposito: `operator_properties` no solo lee, tambien crea las propiedades del
 * operador en el `bToolRef` si no existen, y el Python no lo hace. */
void draw_transform_shear(const bContext *C,
                          uiLayout *layout,
                          bToolRef * /*tref*/,
                          const bool extra)
{
  if (extra) {
    return;
  }
  /* # props = tool.operator_properties("transform.shear") */
  /* _template_widget.VIEW3D_GGT_xform_gizmo.draw_settings_with_index(context, layout, 2) */
  xform_gizmo_draw_settings_with_index(C, layout, 2);
}

/* `_defs_transform.transform` (`space_toolsystem_toolbar.py:395`), `builtin.transform`.
 *
 * Rarezas conservadas:
 * - El rotulo "Gizmos:" solo sale con `use_property_split`, o sea en el panel lateral y no
 *   en la cabecera; y sale aunque luego no se pinte `drag_action`, que es a lo que parece
 *   rotular. Con la reserva activa el panel muestra "Gizmos:" seguido directamente de la
 *   orientacion.
 * - Usa la ranura 1, la de MOVER, aunque la herramienta tambien gira y escala. */
void draw_transform_transform(const bContext *C,
                              uiLayout *layout,
                              bToolRef *tref,
                              const bool extra)
{
  if (extra) {
    return;
  }
  /* if layout.use_property_split:
   *     layout.label(text="Gizmos:")
   * `rna_UILayout_property_split_get` es `uiLayoutGetPropSep`. */
  if (uiLayoutGetPropSep(layout)) {
    label(layout, "Gizmos:");
  }

  /* show_drag = True
   * tool_settings = context.tool_settings
   * if tool_settings.workspace_tool_type == 'FALLBACK':
   *     show_drag = False
   *
   * 'FALLBACK' es `SCE_WORKSPACE_TOOL_FALLBACK` en `workspace_tool_items` de
   * `rna_scene.cc`. Sin `tool_settings` (sin escena) el Python lanza `AttributeError` al
   * leer el atributo de `None` y no pinta nada mas: ni `drag_action` ni la orientacion.
   * Aqui se sale igual, despues del rotulo, que ya se habia pintado. En la vista 3D no
   * pasa, porque siempre hay escena. */
  bool show_drag = true;
  PointerRNA ts = tool_settings(C);
  if (ts.data == nullptr) {
    return;
  }
  if (RNA_enum_get(&ts, "workspace_tool_type") == SCE_WORKSPACE_TOOL_FALLBACK) {
    show_drag = false;
  }

  /* if show_drag:
   *     props = tool.gizmo_group_properties("VIEW3D_GGT_xform_gizmo")
   *     layout.prop(props, "drag_action") */
  if (show_drag) {
    PointerRNA props = gizmo_props(tref, "VIEW3D_GGT_xform_gizmo");
    prop(layout, &props, "drag_action");
  }

  /* _defs_transform.draw_transform_sculpt_tool_settings(context, layout) */
  draw_transform_sculpt_tool_settings(C, layout);
  /* _template_widget.VIEW3D_GGT_xform_gizmo.draw_settings_with_index(context, layout, 1) */
  xform_gizmo_draw_settings_with_index(C, layout, 1);
}

/** \} */

}  // namespace flipendo::ui::settings
