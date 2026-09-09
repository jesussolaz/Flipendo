/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Barra de herramientas del editor de imagen.
 *
 * Transliteracion de `IMAGE_PT_tools_active`
 * (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:3066`).
 *
 * Es el primer espacio del catalogo CON modos, y ensena por que el modo no puede
 * deducirse de nada global: sale de `SpaceImage::mode`, o sea del editor que se este
 * mirando, y dos editores de imagen abiertos a la vez pueden estar en modos distintos.
 *
 * Ninguna de sus entradas lleva filtro. A diferencia de la vista 3D, aqui el Python no
 * mete ningun `lambda context: ...` en las listas de modo, asi que no hay ningun `poll`
 * que trasladar: lo que decide que se ve es el modo y nada mas.
 */

#include "DNA_space_types.h"

#include "BKE_context.hh"

#include "BLT_translation.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "fl_tool_defs.hh"
#include "fl_tool_defs_image_uv.hh"
#include "fl_tool_defs_paint.hh"
#include "fl_tool_defs_uv_curves_sculpt.hh"

#include "fl_toolbar_image.hh"

namespace flipendo::toolsystem {

/* -------------------------------------------------------------------- */
/** \name El pincel generico de la barra
 * \{ */

/**
 * `IMAGE_PT_tools_active._brush_tool`.
 *
 * Es de la BARRA y no de ninguna clase `_defs_*`: en el Python cada espacio con
 * pintado declara su propia copia identica, y esa copia es la que llega a la linea
 * base. No confundir con `defs_texture_paint::brush`, que comparte el idname
 * `builtin.brush` pero lleva otra etiqueta y otro icono y no la enlaza ninguna barra;
 * usar aquella daria un boton distinto del que ve hoy el usuario.
 */
static const ToolDecl brush_tool = {
    /*idname*/ "builtin.brush",
    /*label*/ N_("Brush"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "brush.generic",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
    /*keymap_fallback*/ nullptr,
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name El modo activo
 * \{ */

/**
 * El modo del editor de imagen con el nombre que usa el catalogo: 'VIEW', 'UV',
 * 'PAINT' o 'MASK'.
 *
 * Sin editor de imagen en el contexto el Python cae a 'VIEW', que NO es lo mismo que
 * quedarse sin modo: 'VIEW' tiene cinco herramientas y "sin modo" no tendria ninguna.
 * Es el caso del volcado y de cualquier consulta hecha desde fuera del espacio.
 *
 * El nombre sale de la enumeracion RNA y no de un `switch` escrito a mano porque es el
 * mismo que el usuario elige en la interfaz: copiarlo aqui seria una segunda lista que
 * se desincronizaria en silencio el dia que se anada un modo.
 */
static const char *image_mode_from_context(const bContext *C)
{
  const SpaceImage *sima = CTX_wm_space_image(C);
  if (sima == nullptr) {
    return "VIEW";
  }
  const char *identifier = nullptr;
  if (!RNA_enum_identifier(rna_enum_space_image_mode_all_items, sima->mode, &identifier)) {
    /* Un valor fuera de la enumeracion no es ningun modo del catalogo, y ese caso no
     * existe en el Python porque alli RNA no lo dejaria salir. Sin modo, la barra se
     * queda con las comunes. */
    return nullptr;
  }
  return identifier;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Grupos con ciclo
 * \{ */

/* `_tools_select`: los cuatro de seleccion comparten boton y se cicla entre ellos. En
 * el Python son una tupla DENTRO de la lista del modo; el orden es el de la linea
 * base: tweak, caja, circulo y lazo. */
static const ToolDecl *image_select_group[] = {
    &defs_image_uv_select::select,
    &defs_image_uv_select::box,
    &defs_image_uv_select::circle,
    &defs_image_uv_select::lasso,
};

/* `_tools_annotate`: las mismas cuatro, en el mismo grupo, en tres de los cuatro
 * modos. Una sola tabla referenciada tres veces, que es justo lo que hace el Python
 * con su tupla privada. */
static const ToolDecl *image_annotate_group[] = {
    &defs_annotate::scribble,
    &defs_annotate::line,
    &defs_annotate::poly,
    &defs_annotate::eraser,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Herramientas sueltas
 *
 * Una tabla de uno por cada una: `ToolEntry::tools` es un `Span`, y un `Span` no puede
 * apuntar a un array temporal escrito dentro de la lista de entradas.
 * \{ */

static const ToolDecl *image_sample[] = {&defs_image_generic::sample};
static const ToolDecl *image_cursor[] = {&defs_image_generic::cursor};

static const ToolDecl *image_uv_translate[] = {&defs_image_uv_transform::translate};
static const ToolDecl *image_uv_rotate[] = {&defs_image_uv_transform::rotate};
static const ToolDecl *image_uv_scale[] = {&defs_image_uv_transform::scale};
static const ToolDecl *image_uv_transform[] = {&defs_image_uv_transform::transform};

static const ToolDecl *image_uv_rip_region[] = {&defs_image_uv_edit::rip_region};

static const ToolDecl *image_uv_sculpt_grab[] = {&defs_image_uv_sculpt::grab};
static const ToolDecl *image_uv_sculpt_relax[] = {&defs_image_uv_sculpt::relax};
static const ToolDecl *image_uv_sculpt_pinch[] = {&defs_image_uv_sculpt::pinch};

static const ToolDecl *image_paint_brush[] = {&brush_tool};
static const ToolDecl *image_paint_blur[] = {&defs_texture_paint::blur};
static const ToolDecl *image_paint_smear[] = {&defs_texture_paint::smear};
static const ToolDecl *image_paint_clone[] = {&defs_texture_paint::clone};
static const ToolDecl *image_paint_fill[] = {&defs_texture_paint::fill};
static const ToolDecl *image_paint_mask[] = {&defs_texture_paint::mask};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Las entradas de cada modo
 * \{ */

/* 'VIEW': el editor mirando una imagen. Lo unico que se puede hacer es medir un pixel
 * y anotar encima. */
static const ToolEntry image_view_entries[] = {
    {span(image_sample)},
    {span(image_annotate_group)},
};

/* 'UV': el modo con todo. Las cuatro de transformar van SUELTAS, cada una con su
 * boton, no agrupadas: en el Python `_tools_transform` es una tupla plana que se
 * desempaqueta en la lista, no una tupla anidada como `_tools_select`. */
static const ToolEntry image_uv_entries[] = {
    {span(image_select_group)},
    {span(image_cursor)},
    /* Separador: entrada vacia, el `None` del Python. */
    {},
    {span(image_uv_translate)},
    {span(image_uv_rotate)},
    {span(image_uv_scale)},
    {span(image_uv_transform)},
    {},
    {span(image_annotate_group)},
    {},
    {span(image_uv_rip_region)},
    {},
    {span(image_uv_sculpt_grab)},
    {span(image_uv_sculpt_relax)},
    {span(image_uv_sculpt_pinch)},
};

/* 'MASK': una sola entrada, y es un separador. El Python declara `'MASK': [None]`, o
 * sea cero herramientas. Se declara igualmente en vez de omitir el modo, porque un
 * modo que existe y esta vacio no es lo mismo que un modo que no esta trasladado. */
static const ToolEntry image_mask_entries[] = {
    {},
};

/* 'PAINT': el pincel generico primero y detras los cinco tipos concretos, que lo unico
 * que hacen es fijar el `brush_type`. */
static const ToolEntry image_paint_entries[] = {
    {span(image_paint_brush)},
    {span(image_paint_blur)},
    {span(image_paint_smear)},
    {span(image_paint_clone)},
    {span(image_paint_fill)},
    {span(image_paint_mask)},
    {},
    {span(image_annotate_group)},
};

/* La entrada de modo `nullptr` son las COMUNES del espacio, y el editor de imagen no
 * tiene ninguna: en el Python es `_tools[None] = []`, una lista vacia con un comentario
 * dentro. Se declara vacia en vez de omitirse para que se lea que esta mirado, que es
 * distinto de que se haya olvidado. */
static const ModeTools image_modes[] = {
    {nullptr, {}},
    {"VIEW", span(image_view_entries)},
    {"UV", span(image_uv_entries)},
    {"MASK", span(image_mask_entries)},
    {"PAINT", span(image_paint_entries)},
};

const ToolbarDecl toolbar_image = {
    /*space_type*/ SPACE_IMAGE,
    /*keymap_prefix*/ "Image Editor Tool:",
    /*tool_fallback_id*/ "builtin.select",
    /*mode_from_context*/ image_mode_from_context,
    /*modes*/ span(image_modes),
};

/** \} */

}  // namespace flipendo::toolsystem
