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
 *   cosas son codigo en el Python y estan explicadas en el `.hh`, junto a las
 *   declaraciones de las dos auxiliares que comparten.
 */

#include "BLT_translation.hh"

#include "fl_tool_defs_view3d_add.hh"

namespace flipendo::toolsystem::defs_view3d_add {

/* Los cinco prefijos de la descripcion calculada quedan anotados con cada herramienta.
 * Son el unico trozo de `description_interactive_add` que SI es dato literal del
 * Python, y perderlos obligaria a volver a abrir el fichero de Python cuando se
 * escriban las siete descripciones calculadas. */

/* Prefijo de la descripcion: "Add cube to mesh interactively". */
const ToolDecl cube_add = {
    /*idname*/ "builtin.primitive_cube_add",
    /*label*/ N_("Add Cube"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
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
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
    /*settings_pending*/ true,
};

/* Prefijo de la descripcion: "Add cone to mesh interactively".
 *
 * Ademas de lo comun, sus ajustes anaden `vertices` y `end_fill_type` de
 * `mesh.primitive_cone_add`, pero solo cuando NO se esta pintando el popover extra.
 * Ese "solo cuando" es flujo de control, asi que la herramienta entera queda
 * pendiente en vez de trasladar las dos filas y perder la condicion. */
const ToolDecl cone_add = {
    /*idname*/ "builtin.primitive_cone_add",
    /*label*/ N_("Add Cone"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
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
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
    /*settings_pending*/ true,
};

/* Prefijo de la descripcion: "Add cylinder to mesh interactively".
 *
 * Sus ajustes propios son `vertices` y `end_fill_type` de
 * `mesh.primitive_cylinder_add`, con la misma condicion que el cono. */
const ToolDecl cylinder_add = {
    /*idname*/ "builtin.primitive_cylinder_add",
    /*label*/ N_("Add Cylinder"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
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
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
    /*settings_pending*/ true,
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
    /*description_fn*/ nullptr,
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
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
    /*settings_pending*/ true,
};

/* Prefijo de la descripcion: "Add sphere to mesh interactively".
 *
 * Su unico ajuste propio es `subdivisions` de `mesh.primitive_ico_sphere_add`. */
const ToolDecl ico_sphere_add = {
    /*idname*/ "builtin.primitive_ico_sphere_add",
    /*label*/ N_("Add Ico Sphere"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
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
    /*draw_settings*/ nullptr,
    /*draw_cursor*/ nullptr,
    /*settings_pending*/ true,
};

}  // namespace flipendo::toolsystem::defs_view3d_add
