/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Herramientas de transformar de la vista 3D. Transliteracion de `_template_widget`
 * (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:89`) y de `_defs_transform`
 * (`:301`).
 *
 * `_template_widget` no declara ninguna herramienta: son los ajustes COMPARTIDOS de dos
 * grupos de gizmos. En el Python son clases sueltas justamente porque los usan varios
 * grupos `_defs_*` a la vez, asi que se declaran aqui, con los mismos nombres, para que
 * quien traslade la malla, la curva y la armadura los referencie en vez de copiarlos.
 */

#ifndef __FL_TOOL_DEFS_TRANSFORM_HH__
#define __FL_TOOL_DEFS_TRANSFORM_HH__

#include "FL_toolsystem.hpp"

namespace flipendo::toolsystem {

/** `_template_widget` del Python. */
namespace template_widget {

/** `_template_widget.VIEW3D_GGT_xform_extrude`. */
namespace view3d_ggt_xform_extrude {

/**
 * El `draw_settings` de la clase, que es TODO el `draw_settings` de las tres
 * herramientas de extrusion (armadura, malla y curva): alli se pasa entera como
 * `draw_settings=`, no se llama desde dentro de otra.
 *
 * Es una sola `layout.prop` sin condiciones, asi que cabe como tabla; y se exporta como
 * tabla, en vez de dejarla privada, porque los tres ficheros que la van a usar todavia
 * no existen y repetir las filas es como se desincronizan.
 *
 * El Python la mete en un `layout.row(align=True)`: con una unica propiedad expandida no
 * hay nada que alinear, asi que la fila no aporta nada que reproducir.
 */
extern const blender::Span<PropRow> settings;

}  // namespace view3d_ggt_xform_extrude

/** `_template_widget.VIEW3D_GGT_xform_gizmo`. */
namespace view3d_ggt_xform_gizmo {

/**
 * Pinta el tipo de orientacion de la ranura `index` de la escena, o sea
 * `scene.transform_orientation_slots[index].type`.
 *
 * No puede ser una fila `PropRow` por dos motivos independientes: la propiedad no sale
 * de la herramienta ni de `tool_settings`, sino de un elemento de una coleccion de la
 * escena elegido por indice, y ademas el indice cambia en cada llamada. Tampoco puede
 * ser un `DrawSettingsFn`, porque ese contrato no lleva indice.
 *
 * Queda solo DECLARADA a proposito: las seis de este fichero que la usan van con
 * `settings_pending` (y la septima, el sesgado del lapiz de cera, va en otro fichero),
 * asi que hoy no la llama nadie e implementarla ahora seria escribir codigo de dibujo sin
 * nada que lo ejercite. Se implementa en la fase de dibujado.
 */
void draw_settings_with_index(const bContext *C, uiLayout *layout, int index);

}  // namespace view3d_ggt_xform_gizmo

}  // namespace template_widget

/** `_defs_transform`. Las siete de transformar, comunes a casi todos los modos de la
 * vista 3D. */
namespace defs_transform {

/**
 * `draw_transform_sculpt_tool_settings`: el modo de transformacion del escultor, que
 * solo se pinta cuando el modo es 'SCULPT'.
 *
 * Ese `if` sobre el modo es flujo de control, asi que no es una fila. Se declara para
 * que conste que existe y que cuatro herramientas la comparten; se implementa en la fase
 * de dibujado, junto con las que hoy llevan `settings_pending`.
 */
void draw_transform_sculpt_tool_settings(const bContext *C, uiLayout *layout);

extern const ToolDecl translate;
extern const ToolDecl rotate;
extern const ToolDecl scale;
extern const ToolDecl scale_cage;
extern const ToolDecl shear;
extern const ToolDecl bend;
extern const ToolDecl transform;

}  // namespace defs_transform

}  // namespace flipendo::toolsystem

#endif /* __FL_TOOL_DEFS_TRANSFORM_HH__ */
