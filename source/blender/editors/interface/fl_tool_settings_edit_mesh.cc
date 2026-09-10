/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Ajustes de las herramientas de edicion de malla que no caben en filas: `builtin.bevel`
 * y `builtin.knife`. Transliteracion linea a linea de sus `draw_settings` de
 * `_defs_edit_mesh` (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:874` y `:1115`).
 *
 * Las dos reparten sus ajustes entre la cabecera de la herramienta y el popover
 * `TOPBAR_PT_tool_settings_extra` mirando el tipo de region, y fuera de la cabecera
 * (panel lateral "Active Tool") pintan las dos partes seguidas en el mismo `layout`.
 * Las declaraciones de las herramientas estan en
 * `windowmanager/toolsystem/fl_tool_defs_edit_mesh.cc`.
 */

#include "BLI_utildefines.h"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"

#include "FL_tool_settings_ui.hh"

namespace flipendo::ui::settings {

/**
 * `props.<name> == '<identifier>'` para una enumeracion.
 *
 * Desde Python `props.affect` no es un entero sino el IDENTIFICADOR del valor actual
 * (`pyrna_enum_to_py` con `RNA_property_enum_identifier`), y se compara como cadena. Se
 * hace igual aqui en vez de comparar con la constante de C, para que un valor fuera de
 * los elementos se comporte como en Python (cadena vacia: no es igual a nada).
 */
static bool enum_is(const bContext *C, PointerRNA *ptr, const char *name, const char *identifier)
{
  PropertyRNA *property = RNA_struct_find_property(ptr, name);
  if (property == nullptr) {
    return false;
  }
  const char *current = nullptr;
  RNA_property_enum_identifier(const_cast<bContext *>(C),
                               ptr,
                               property,
                               RNA_property_enum_get(ptr, property),
                               &current);
  return current != nullptr && STREQ(current, identifier);
}

/* -------------------------------------------------------------------- */
/** \name Bisel
 * \{ */

/**
 * `_defs_edit_mesh.bevel.draw_settings` (`space_toolsystem_toolbar.py:876`).
 *
 * Rarezas del Python que se conservan:
 * - Fuera de la cabecera se REASIGNA `extra = True` a mitad de funcion, de modo que el
 *   panel lateral pinta la parte normal y a continuacion la parte "extra", en el mismo
 *   `layout` y sin separador entre las dos.
 * - `use_property_split` y `use_property_decorate` se ponen sobre el `layout` que llega,
 *   no sobre uno propio, asi que lo que el llamador pinte despues en ese `layout` los
 *   hereda. Eso tambien pasaba en Python.
 * - `edge_bevel` se lee UNA vez al principio y se usa en la parte "extra", igual que
 *   alli; `miter_inner` y `profile_type` se leen en el momento de usarlos.
 */
void draw_bevel(const bContext *C, uiLayout *layout, bToolRef *tref, bool extra)
{
  PointerRNA props = op_props(tref, "mesh.bevel");
  /* Sin operador el Python lanzaria la excepcion aqui mismo y no pintaria nada. */
  if (props.data == nullptr) {
    return;
  }

  const bool region_is_header = region_is_tool_header(C);

  const bool edge_bevel = enum_is(C, &props, "affect", "EDGES");

  if (!extra) {
    if (region_is_header) {
      prop(layout, &props, "offset_type", Prop().text(""));
    }
    else {
      prop(&row(layout), &props, "affect", Prop().expand());
      layout->separator();
      prop(layout, &props, "offset_type");
    }

    prop(layout, &props, "segments");

    if (region_is_header) {
      prop(layout, &props, "affect", Prop().text(""));
    }

    prop(layout, &props, "profile", Prop().text("Shape").slider());

    if (region_is_header) {
      popover(layout, C, "TOPBAR_PT_tool_settings_extra", "...");
    }
    else {
      extra = true;
    }
  }

  if (extra) {
    uiLayoutSetPropSep(layout, true);
    uiLayoutSetPropDecorate(layout, false);

    prop(layout, &props, "material");

    uiLayout *col = &column(layout);
    prop(col, &props, "harden_normals");
    prop(col, &props, "clamp_overlap");
    prop(col, &props, "loop_slide");

    col = &column(layout, false, "Mark");
    uiLayoutSetActive(col, edge_bevel);
    prop(col, &props, "mark_seam", Prop().text("Seam"));
    prop(col, &props, "mark_sharp", Prop().text("Sharp"));

    col = &column(layout);
    uiLayoutSetActive(col, edge_bevel);
    prop(col, &props, "miter_outer", Prop().text("Miter Outer"));
    prop(col, &props, "miter_inner", Prop().text("Inner"));
    if (enum_is(C, &props, "miter_inner", "ARC")) {
      prop(col, &props, "spread");
    }

    layout->separator();

    col = &column(layout);
    uiLayoutSetActive(col, edge_bevel);
    prop(col, &props, "vmesh_method", Prop().text("Intersections"));

    prop(layout, &props, "face_strength_mode", Prop().text("Face Strength"));

    prop(layout, &props, "profile_type");

    if (enum_is(C, &props, "profile_type", "CUSTOM")) {
      PointerRNA tool_settings_ptr = tool_settings(C);
      /* `template_curveprofile` exige el dato (`PROP_NEVER_NULL`); sin escena Python
       * fallaria en esta linea, que es la ultima. */
      if (tool_settings_ptr.data != nullptr) {
        uiTemplateCurveProfile(layout, &tool_settings_ptr, "custom_bevel_profile_preset");
      }
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cuchillo
 * \{ */

/**
 * `_defs_edit_mesh.knife.draw_settings` (`space_toolsystem_toolbar.py:1117`).
 *
 * Rarezas del Python que se conservan:
 * - Ignora el `context` que recibe (`_context`) y mira `bpy.context.region.type`. Mientras
 *   se dibuja son el mismo contexto, asi que aqui se usa `C` en los dos casos.
 * - A diferencia del bisel, el popover "..." va AL FINAL, despues del bloque `extra`, y
 *   se decide con su propia variable `show_extra`.
 * - Pone `use_property_decorate` ANTES que `use_property_split`, al reves que el bisel.
 *   No cambia el dibujo, pero se respeta el orden.
 * - Igual que el bisel, fuera de la cabecera reasigna `extra = True` y pinta todo seguido
 *   en el `layout` que llega, dejandole puesto `use_property_split`.
 */
void draw_knife(const bContext *C, uiLayout *layout, bToolRef *tref, bool extra)
{
  bool show_extra = false;
  PointerRNA props = op_props(tref, "mesh.knife_tool");
  /* Sin operador el Python lanzaria la excepcion aqui mismo y no pintaria nada. */
  if (props.data == nullptr) {
    return;
  }
  if (!extra) {
    prop(layout, &props, "use_occlude_geometry");
    prop(layout, &props, "only_selected");
    prop(layout, &props, "xray");
    const bool region_is_header = region_is_tool_header(C);
    if (region_is_header) {
      show_extra = true;
    }
    else {
      extra = true;
    }
  }
  if (extra) {
    uiLayoutSetPropDecorate(layout, false);
    uiLayoutSetPropSep(layout, true);

    prop(layout, &props, "visible_measurements");
    prop(layout, &props, "angle_snapping");
    prop(layout, &props, "angle_snapping_increment", Prop().text("Snap Increment"));
  }
  if (show_extra) {
    popover(layout, C, "TOPBAR_PT_tool_settings_extra", "...");
  }
}

/** \} */

}  // namespace flipendo::ui::settings
