/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Declaraciones de las herramientas de los modos "menores" de la vista 3D.
 *
 * Transliteracion de `_defs_edit_armature`, `_defs_edit_curve`, `_defs_edit_curves`,
 * `_defs_edit_text`, `_defs_pose` y `_defs_particle`
 * (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:684`, `:1229`, `:1321`, `:1339`,
 * `:1353` y `:1386`).
 *
 * Los seis grupos van en un solo fichero porque suman veinte declaraciones entre
 * todos; partirlos por clase daria seis ficheros de veinte lineas y ninguna ventaja al
 * revisarlos contra el Python, que es para lo que existe la correspondencia de
 * nombres.
 *
 * Cada `namespace defs_X` corresponde uno a uno con la clase `_defs_X` del Python, y
 * el nombre de cada declaracion con el del metodo, NO con su `idname`. Se respeta
 * incluso donde no coinciden — `_defs_edit_armature.extrude_cursor` declara
 * `builtin.extrude_to_cursor` — porque las barras referencian el metodo, y renombrarlo
 * aqui obligaria a traducir mentalmente en cada revision.
 */

#ifndef __FL_TOOL_DEFS_EDIT_MISC_HH__
#define __FL_TOOL_DEFS_EDIT_MISC_HH__

#include "FL_toolsystem.hpp"

namespace flipendo::toolsystem {

/** `_defs_edit_armature`. Solo sale en el modo EDIT_ARMATURE de la vista 3D. */
namespace defs_edit_armature {
extern const ToolDecl roll;
extern const ToolDecl bone_envelope;
extern const ToolDecl bone_size;
extern const ToolDecl extrude;
/** Declara `builtin.extrude_to_cursor`; el nombre sigue al del metodo del Python. */
extern const ToolDecl extrude_cursor;
}  // namespace defs_edit_armature

/**
 * `_defs_edit_curve`.
 *
 * `curve_radius` y `tilt` no son exclusivas de EDIT_CURVE: la barra las reutiliza tal
 * cual en EDIT_CURVES, y `curve_radius` ademas en EDIT_GREASE_PENCIL. Por eso sus
 * keymaps dicen "Edit Curve" en los tres modos.
 */
namespace defs_edit_curve {
extern const ToolDecl draw;
extern const ToolDecl extrude;
/** Declara `builtin.extrude_cursor`, sin el `_to_`, al contrario que la de armadura. */
extern const ToolDecl extrude_cursor;
extern const ToolDecl pen;
extern const ToolDecl tilt;
extern const ToolDecl curve_radius;
extern const ToolDecl curve_vertex_randomize;
}  // namespace defs_edit_curve

/**
 * `_defs_edit_curves`.
 *
 * Una sola herramienta, que comparte `idname` e icono con `defs_edit_curve::draw` pero
 * es una declaracion distinta: su keymap es "Edit Curves, Draw" y el de aquella "Edit
 * Curve, Draw". Fundirlas dejaria uno de los dos modos sin atajos.
 */
namespace defs_edit_curves {
extern const ToolDecl draw;
}  // namespace defs_edit_curves

/** `_defs_edit_text`. */
namespace defs_edit_text {
extern const ToolDecl select_text;
}  // namespace defs_edit_text

/** `_defs_pose`. */
namespace defs_pose {
extern const ToolDecl breakdown;
extern const ToolDecl push;
extern const ToolDecl relax;
}  // namespace defs_pose

/**
 * `_defs_particle`.
 *
 * La unica clase del catalogo que no declara ni una `ToolDef`: su unico miembro es
 * `generate_from_brushes`, que llama a `generate_from_enum_ex` sobre `ParticleEdit.tool`.
 * Se conserva como generacion en vez de copiarse a siete tablas para que siga a la
 * enumeracion si esta cambia; una copia se desincronizaria en silencio.
 */
namespace defs_particle {
extern const EnumToolsDecl generate_from_brushes;
}  // namespace defs_particle

}  // namespace flipendo::toolsystem

#endif /* __FL_TOOL_DEFS_EDIT_MISC_HH__ */
