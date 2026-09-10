/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Ajustes de las cinco herramientas de anadir primitivas de forma interactiva.
 * Transliteracion LINEA A LINEA de los `draw_settings` de `_defs_view3d_add`
 * (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:530-667`).
 *
 * Las cinco envuelven la misma auxiliar, `draw_settings_interactive_add`, y solo
 * cambian los controles propios de la primitiva que anaden detras. Se pintan desde tres
 * sitios, que es lo que decide lo que sale:
 *
 * - La cabecera de herramienta (region `TOOL_HEADER`, `extra` falso): profundidad,
 *   orientacion y ajuste, los controles de la primitiva y, al final, el popover
 *   `TOPBAR_PT_tool_settings_extra` con el texto "...".
 * - Ese popover (`extra` verdadero): ejes y origenes; nada de la primitiva.
 * - El panel lateral "Active Tool" (otra region, `extra` falso): TODO seguido, porque
 *   la auxiliar se pone `extra` a verdadero ella sola cuando no esta en la cabecera.
 */

#include "UI_interface_layout.hh"

#include "FL_tool_settings_ui.hh"

namespace flipendo::ui::settings {

/* En el Python la variable se llama `row`, igual que la funcion `row()` de
 * FL_tool_settings_ui.hh. En C++ el nombre de una variable ya esta en ambito dentro de
 * su propio inicializador, asi que aqui se llama `row_layout`. */

/**
 * `_defs_view3d_add.draw_settings_interactive_add(layout, tool_settings, tool, extra)`
 * (`space_toolsystem_toolbar.py:530`). Recibe tambien `C` porque el Python lee la region
 * de `bpy.context`, que durante el dibujo es el mismo contexto que le llega a la
 * envolvente.
 *
 * Devuelve `show_extra`: si la envolvente debe rematar con el popover extra.
 *
 * El comentario del Python sobre esta funcion dice que los retoques de maqueta de aqui
 * "serian buenos de evitar" y que muestran los limites del motor de maqueta. Se
 * conservan tal cual.
 */
static bool draw_settings_interactive_add(const bContext *C,
                                          uiLayout *layout,
                                          PointerRNA *tool_settings,
                                          bToolRef *tref,
                                          bool extra)
{
  bool show_extra = false;
  if (!extra) {
    uiLayout *row_layout = &row(layout);
    prop(row_layout, tool_settings, "plane_depth", Prop().text("Depth"));
    row_layout = &row(layout);
    prop(row_layout, tool_settings, "plane_orientation", Prop().text("Orientation"));
    row_layout = &row(layout);
    prop(row_layout, tool_settings, "snap_elements_tool");

    /* `bpy.context.region.type == 'TOOL_HEADER'`. Si no hubiera region el Python
     * lanzaria una excepcion; aqui cuenta como "no es la cabecera". Durante el dibujo
     * de un panel siempre hay region, asi que no se ve la diferencia. */
    const bool region_is_header = region_is_tool_header(C);
    if (region_is_header) {
      /* "Don't draw the "extra" popover here as we might have other settings & this
       * should be last." */
      show_extra = true;
    }
    else {
      /* Rareza conservada: fuera de la cabecera la auxiliar se pone `extra` a verdadero
       * y pinta tambien el bloque del popover, pero es su copia LOCAL. La `extra` de la
       * envolvente sigue falsa, asi que el cono, el cilindro y las esferas pintan
       * despues sus propios controles, y lo hacen con `use_property_split` ya puesto
       * aqui abajo. En C++ el paso por valor reproduce lo mismo. */
      extra = true;
    }
  }

  if (extra) {
    PointerRNA props = op_props(tref, "view3d.interactive_add");
    /* `layout.use_property_split = True` (`rna_UILayout_property_split_set`). Rareza
     * conservada: se pone sobre el `layout` que le pasan, no sobre uno propio, asi que
     * sigue puesto para todo lo que el llamador pinte despues en ese mismo `layout`. */
    uiLayoutSetPropSep(layout, true);
    prop(&row(layout), tool_settings, "plane_axis", Prop().expand());
    prop(&row(layout), tool_settings, "plane_axis_auto");

    label(layout, "Base");
    prop(&row(layout), &props, "plane_origin_base", Prop().expand());
    prop(&row(layout), &props, "plane_aspect_base", Prop().expand());
    label(layout, "Height");
    prop(&row(layout), &props, "plane_origin_depth", Prop().expand());
    prop(&row(layout), &props, "plane_aspect_depth", Prop().expand());
  }
  return show_extra;
}

/* -------------------------------------------------------------------- */
/** \name Las cinco envolventes
 * \{ */

/** `_defs_view3d_add.cube_add.draw_settings` (`space_toolsystem_toolbar.py:563`). El
 * cubo no tiene controles propios, y por eso tampoco tiene el `if extra: return`. */
void draw_cube_add(const bContext *C, uiLayout *layout, bToolRef *tref, const bool extra)
{
  PointerRNA ts = tool_settings(C);
  const bool show_extra = draw_settings_interactive_add(C, layout, &ts, tref, extra);
  if (show_extra) {
    popover(layout, C, "TOPBAR_PT_tool_settings_extra", "...");
  }
}

/** `_defs_view3d_add.cone_add.draw_settings` (`space_toolsystem_toolbar.py:582`). */
void draw_cone_add(const bContext *C, uiLayout *layout, bToolRef *tref, const bool extra)
{
  PointerRNA ts = tool_settings(C);
  const bool show_extra = draw_settings_interactive_add(C, layout, &ts, tref, extra);
  if (extra) {
    return;
  }

  PointerRNA props = op_props(tref, "mesh.primitive_cone_add");
  prop(layout, &props, "vertices");
  prop(layout, &props, "end_fill_type");

  if (show_extra) {
    popover(layout, C, "TOPBAR_PT_tool_settings_extra", "...");
  }
}

/** `_defs_view3d_add.cylinder_add.draw_settings` (`space_toolsystem_toolbar.py:608`). */
void draw_cylinder_add(const bContext *C, uiLayout *layout, bToolRef *tref, const bool extra)
{
  PointerRNA ts = tool_settings(C);
  const bool show_extra = draw_settings_interactive_add(C, layout, &ts, tref, extra);
  if (extra) {
    return;
  }

  PointerRNA props = op_props(tref, "mesh.primitive_cylinder_add");
  prop(layout, &props, "vertices");
  prop(layout, &props, "end_fill_type");

  if (show_extra) {
    popover(layout, C, "TOPBAR_PT_tool_settings_extra", "...");
  }
}

/** `_defs_view3d_add.uv_sphere_add.draw_settings` (`space_toolsystem_toolbar.py:633`). */
void draw_uv_sphere_add(const bContext *C, uiLayout *layout, bToolRef *tref, const bool extra)
{
  PointerRNA ts = tool_settings(C);
  const bool show_extra = draw_settings_interactive_add(C, layout, &ts, tref, extra);
  if (extra) {
    return;
  }

  PointerRNA props = op_props(tref, "mesh.primitive_uv_sphere_add");
  prop(layout, &props, "segments");
  prop(layout, &props, "ring_count");

  if (show_extra) {
    popover(layout, C, "TOPBAR_PT_tool_settings_extra", "...");
  }
}

/** `_defs_view3d_add.ico_sphere_add.draw_settings` (`space_toolsystem_toolbar.py:658`). */
void draw_ico_sphere_add(const bContext *C, uiLayout *layout, bToolRef *tref, const bool extra)
{
  PointerRNA ts = tool_settings(C);
  const bool show_extra = draw_settings_interactive_add(C, layout, &ts, tref, extra);
  if (extra) {
    return;
  }

  PointerRNA props = op_props(tref, "mesh.primitive_ico_sphere_add");
  prop(layout, &props, "subdivisions");

  if (show_extra) {
    popover(layout, C, "TOPBAR_PT_tool_settings_extra", "...");
  }
}

/** \} */

}  // namespace flipendo::ui::settings
