/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Herramientas del editor de imagen y de su modo UV. Transliteracion de
 * `_defs_image_generic`, `_defs_image_uv_transform`, `_defs_image_uv_select` y
 * `_defs_image_uv_edit` (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:2425`,
 * `:2469`, `:2518` y `:2588`).
 *
 * Van juntas en un mismo fichero porque las cuatro clases son un unico espacio: el
 * editor de imagen reparte sus herramientas entre los modos VIEW y UV, y las de
 * `_defs_image_generic` salen en los dos. Separarlas por clase obligaria a saltar entre
 * ficheros para revisar un solo modo.
 *
 * Ojo con los nombres de keymap: aqui casi todos llevan el modo dentro ("Image Editor
 * Tool: Uv, ..."), pero `builtin.sample` NO, porque el Python se lo da literal y no
 * generado. Estan copiados de la linea base congelada, nunca sintetizados.
 */

#ifndef __FL_TOOL_DEFS_IMAGE_UV_HH__
#define __FL_TOOL_DEFS_IMAGE_UV_HH__

#include "FL_toolsystem.hpp"

namespace flipendo::toolsystem {

/** `_defs_image_generic` del Python. Comunes al editor de imagen: `sample` sale en el
 * modo VIEW y `cursor` en el modo UV. */
namespace defs_image_generic {

/**
 * `_defs_image_generic.poll_uvedit`: solo hay herramientas de UV si el objeto en
 * edicion tiene capas UV.
 *
 * Se declara sin implementar porque el filtro pertenece a la colocacion, no al
 * catalogo: lo consumira la barra del espacio (`ToolEntry::poll`) en la fase siguiente.
 * Declararlo ya evita que el filtro se pierda al trasladar las tablas.
 */
bool poll_uvedit(const bContext *C);

extern const ToolDecl cursor;
extern const ToolDecl sample;

}  // namespace defs_image_generic

/** `_defs_image_uv_transform`. Las cuatro de transformacion del modo UV; las tres
 * primeras son la version 2D de las de la vista 3D, con gizmos `IMAGE_GGT_*`. */
namespace defs_image_uv_transform {
extern const ToolDecl translate;
extern const ToolDecl rotate;
extern const ToolDecl scale;
extern const ToolDecl transform;
}  // namespace defs_image_uv_transform

/** `_defs_image_uv_select`. */
namespace defs_image_uv_select {
extern const ToolDecl select;
extern const ToolDecl box;
extern const ToolDecl lasso;
extern const ToolDecl circle;
}  // namespace defs_image_uv_select

/** `_defs_image_uv_edit`. */
namespace defs_image_uv_edit {
extern const ToolDecl rip_region;
}  // namespace defs_image_uv_edit

}  // namespace flipendo::toolsystem

#endif /* __FL_TOOL_DEFS_IMAGE_UV_HH__ */
