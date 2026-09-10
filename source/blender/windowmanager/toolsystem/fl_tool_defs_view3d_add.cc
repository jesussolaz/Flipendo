/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */


/** \file
 * \ingroup wm
 *
 * Herramientas de anadir primitivas de forma interactiva en la vista 3D.
 * Transliteracion de `_defs_view3d_add`
 * (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:495`).
 *
 * Las cinco comparten gizmo, keymap y ajustes; solo cambian el icono, la etiqueta y la
 * primitiva. Dos cosas de este bloque conviene tener presentes al leerlo:
 *
 * - El keymap de las cinco es "3D View Tool: Object, Add Primitive" AUNQUE la barra
 *   las saque tambien en Edit Mesh. No es una errata: el Python guarda el nombre
 *   mutando una lista compartida y gana el primer modo que registra la herramienta,
 *   que aqui es Object. Calcularlo por modo daria "3D View Tool: Edit Mesh, Add
 *   Primitive", un keymap que no existe, y las cinco se quedarian sin atajos en Edit
 *   Mesh sin que nada avisara. Ver la nota de `keymap_name` en FL_toolsystem.hpp.
 *
 * - Ninguna de las cinco tiene descripcion literal ni ajustes declarativos: las dos
 *   cosas son codigo en el Python. La descripcion esta aqui abajo; el dibujo de los
 *   ajustes, en `editors/interface/fl_tool_settings_view3d_add.cc`.
 */

#include <fmt/format.h>

#include "BLT_translation.hh"

#include "fl_tool_description.hh"

#include "fl_tool_defs_view3d_add.hh"

/* Los `draw_settings` de las cinco, transliterados en
 * `editors/interface/fl_tool_settings_view3d_add.cc`. */
namespace flipendo::ui::settings {
void draw_cube_add(const bContext *C, uiLayout *layout, bToolRef *tref, bool extra);
void draw_cone_add(const bContext *C, uiLayout *layout, bToolRef *tref, bool extra);
void draw_cylinder_add(const bContext *C, uiLayout *layout, bToolRef *tref, bool extra);
void draw_uv_sphere_add(const bContext *C, uiLayout *layout, bToolRef *tref, bool extra);
void draw_ico_sphere_add(const bContext *C, uiLayout *layout, bToolRef *tref, bool extra);
}  // namespace flipendo::ui::settings

namespace flipendo::toolsystem::defs_view3d_add {

/* -------------------------------------------------------------------- */
/** \name Las descripciones calculadas de las cinco
 *
 * `description_interactive_add` (`space_toolsystem_toolbar.py:498`).
 *
 * Las cinco son la misma plantilla con distinto prefijo, y NINGUNA es un literal: la
 * funcion abre el keymap modal del usuario y mete dentro los tres atajos que esa
 * persona tenga puestos. Por eso `description` va a `nullptr` y `description_fn` no.
 *
 * Aqui no vale dejarlo sin hacer y confiar en que el tooltip caiga en el del operador,
 * que es lo que hace el motor cuando no hay descripcion: estas cinco tienen `op` a
 * `nullptr`, asi que no hay operador del que heredar nada. Sin esto el usuario se queda
 * con la etiqueta a secas y sin ninguna pista de los tres modificadores, que son la
 * mitad del manejo de la herramienta.
 * \{ */

static std::string description_interactive_add(const bContext *C, const char *prefix)
{
  const wmKeyMap *km = user_modal_keymap(C, "View3D Placement Modal");

  return fmt::format(fmt::runtime(TIP_("{:s}\n"
                                       " \u2022 {:s} toggles snap while dragging\n"
                                       " \u2022 {:s} toggles dragging from the center\n"
                                       " \u2022 {:s} toggles fixed aspect")),
                     prefix,
                     kmi_to_string_or_none(modal_kmi_from_identifier(km, "SNAP_ON")),
                     kmi_to_string_or_none(modal_kmi_from_identifier(km, "PIVOT_CENTER_ON")),
                     kmi_to_string_or_none(modal_kmi_from_identifier(km, "FIXED_ASPECT_ON")));
}

/* El keymap que llega es el de la herramienta; estas cinco no lo usan, porque los
 * atajos que enseñan viven en el modal de colocacion, que es comun a las cinco. */
static std::string description_cube_add(const bContext *C, const wmKeyMap * /*km*/)
{
  return description_interactive_add(C, TIP_("Add cube to mesh interactively"));
}
static std::string description_cone_add(const bContext *C, const wmKeyMap * /*km*/)
{
  return description_interactive_add(C, TIP_("Add cone to mesh interactively"));
}
static std::string description_cylinder_add(const bContext *C, const wmKeyMap * /*km*/)
{
  return description_interactive_add(C, TIP_("Add cylinder to mesh interactively"));
}
/* Las dos esferas comparten texto en el Python; no es un descuido al copiar. */
static std::string description_uv_sphere_add(const bContext *C, const wmKeyMap * /*km*/)
{
  return description_interactive_add(C, TIP_("Add sphere to mesh interactively"));
}
static std::string description_ico_sphere_add(const bContext *C, const wmKeyMap * /*km*/)
{
  return description_interactive_add(C, TIP_("Add sphere to mesh interactively"));
}

/** \} */


/* Los cinco prefijos de la descripcion calculada quedan anotados con cada herramienta.
 * Son el unico trozo de `description_interactive_add` que SI es dato literal del
 * Python, y perderlos obligaria a volver a abrir el fichero de Python cuando se
 * escriban las siete descripciones calculadas. */

/* Prefijo de la descripcion: "Add cube to mesh interactively".
 *
 * Sus ajustes son solo los comunes a las cinco, sin controles propios. */
const ToolDecl cube_add = {
    /*idname*/ "builtin.primitive_cube_add",
    /*label*/ N_("Add Cube"),
    /*description*/ nullptr,
    /*description_fn*/ description_cube_add,
    /*icon*/ "ops.mesh.primitive_cube_add_gizmo",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_placement",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Object, Add Primitive",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ {},
    /*draw_settings*/ flipendo::ui::settings::draw_cube_add,
    /*draw_cursor*/ nullptr,
    /*pending*/ TOOL_PENDING_NONE,
};

/* Prefijo de la descripcion: "Add cone to mesh interactively".
 *
 * Ademas de lo comun, sus ajustes anaden `vertices` y `end_fill_type` de
 * `mesh.primitive_cone_add`, pero solo cuando NO se esta pintando el popover extra.
 * Ese "solo cuando" es flujo de control, y por eso va como codigo (`draw_settings`) y
 * no como dos filas que perderian la condicion. */
const ToolDecl cone_add = {
    /*idname*/ "builtin.primitive_cone_add",
    /*label*/ N_("Add Cone"),
    /*description*/ nullptr,
    /*description_fn*/ description_cone_add,
    /*icon*/ "ops.mesh.primitive_cone_add_gizmo",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_placement",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Object, Add Primitive",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ {},
    /*draw_settings*/ flipendo::ui::settings::draw_cone_add,
    /*draw_cursor*/ nullptr,
    /*pending*/ TOOL_PENDING_NONE,
};

/* Prefijo de la descripcion: "Add cylinder to mesh interactively".
 *
 * Sus ajustes propios son `vertices` y `end_fill_type` de
 * `mesh.primitive_cylinder_add`, con la misma condicion que el cono. */
const ToolDecl cylinder_add = {
    /*idname*/ "builtin.primitive_cylinder_add",
    /*label*/ N_("Add Cylinder"),
    /*description*/ nullptr,
    /*description_fn*/ description_cylinder_add,
    /*icon*/ "ops.mesh.primitive_cylinder_add_gizmo",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_placement",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Object, Add Primitive",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ {},
    /*draw_settings*/ flipendo::ui::settings::draw_cylinder_add,
    /*draw_cursor*/ nullptr,
    /*pending*/ TOOL_PENDING_NONE,
};

/* Prefijo de la descripcion: "Add sphere to mesh interactively". Es el mismo texto que
 * el de la ico-esfera, y por eso las dos comparten tambien icono: en el Python son la
 * misma cadena escrita dos veces, no un descuido de esta transliteracion.
 *
 * Sus ajustes propios son `segments` y `ring_count` de `mesh.primitive_uv_sphere_add`. */
const ToolDecl uv_sphere_add = {
    /*idname*/ "builtin.primitive_uv_sphere_add",
    /*label*/ N_("Add UV Sphere"),
    /*description*/ nullptr,
    /*description_fn*/ description_uv_sphere_add,
    /*icon*/ "ops.mesh.primitive_sphere_add_gizmo",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_placement",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Object, Add Primitive",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ {},
    /*draw_settings*/ flipendo::ui::settings::draw_uv_sphere_add,
    /*draw_cursor*/ nullptr,
    /*pending*/ TOOL_PENDING_NONE,
};

/* Prefijo de la descripcion: "Add sphere to mesh interactively".
 *
 * Su unico ajuste propio es `subdivisions` de `mesh.primitive_ico_sphere_add`. */
const ToolDecl ico_sphere_add = {
    /*idname*/ "builtin.primitive_ico_sphere_add",
    /*label*/ N_("Add Ico Sphere"),
    /*description*/ nullptr,
    /*description_fn*/ description_ico_sphere_add,
    /*icon*/ "ops.mesh.primitive_sphere_add_gizmo",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_placement",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Object, Add Primitive",
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ {},
    /*draw_settings*/ flipendo::ui::settings::draw_ico_sphere_add,
    /*draw_cursor*/ nullptr,
    /*pending*/ TOOL_PENDING_NONE,
};

}  // namespace flipendo::toolsystem::defs_view3d_add
