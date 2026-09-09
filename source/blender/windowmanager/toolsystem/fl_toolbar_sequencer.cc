/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Barra de herramientas del editor de secuencias.
 *
 * Transliteracion de `SEQUENCER_PT_tools_active`
 * (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:3670`).
 *
 * Lo que este espacio llama "modo" no es un modo de objeto sino la VISTA abierta
 * (`space_data.view_type`): la linea de tiempo, la previsualizacion, o las dos a la vez.
 * Cambiar de vista cambia la barra entera, y por eso las mismas herramientas aparecen en
 * listas distintas con keymaps distintos. Ver `fl_tool_defs_sequencer.cc`.
 */

#include "BKE_context.hh"

#include "DNA_space_types.h"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "fl_tool_defs.hh"
#include "fl_tool_defs_sequencer.hh"
#include "fl_toolbar_sequencer.hh"

namespace flipendo::toolsystem {

/* Las cuatro de anotacion van bajo un mismo boton con ciclo entre ellas, igual que en
 * los otros tres espacios: en el Python son `_tools_annotate`, una tupla suelta que cada
 * clase desempaqueta en su lista. */
static const ToolDecl *sequencer_annotate_group[] = {
    &defs_annotate::scribble,
    &defs_annotate::line,
    &defs_annotate::poly,
    &defs_annotate::eraser,
};

/* -------------------------------------------------------------------- */
/** \name Vista de previsualizacion
 * \{ */

/* La seleccion de la previsualizacion es un grupo de dos: el ajuste fino y la caja. En
 * la linea de tiempo no hay grupo porque alli solo existe la caja. */
static const ToolDecl *sequencer_preview_select_group[] = {
    &defs_sequencer_select::select_preview,
    &defs_sequencer_select::box_preview,
};

static const ToolDecl *sequencer_preview_cursor[] = {&defs_sequencer_generic::cursor};
static const ToolDecl *sequencer_preview_translate[] = {&defs_sequencer_generic::translate};
static const ToolDecl *sequencer_preview_rotate[] = {&defs_sequencer_generic::rotate};
static const ToolDecl *sequencer_preview_scale[] = {&defs_sequencer_generic::scale};
static const ToolDecl *sequencer_preview_transform[] = {&defs_sequencer_generic::transform};
static const ToolDecl *sequencer_preview_sample[] = {&defs_sequencer_generic::sample};

/* Las cuatro de transformacion van SUELTAS, cada una con su boton, y no agrupadas como
 * en la vista 3D: aqui cada una lleva su propio gizmo 2D y se alternan a golpe de vista,
 * no ciclando. */
static const ToolEntry sequencer_preview_entries[] = {
    {span(sequencer_preview_select_group)},
    {span(sequencer_preview_cursor)},
    /* Separador: entrada vacia, el `None` del Python. */
    {},
    {span(sequencer_preview_translate)},
    {span(sequencer_preview_rotate)},
    {span(sequencer_preview_scale)},
    {span(sequencer_preview_transform)},
    {},
    {span(sequencer_preview_sample)},
    {span(sequencer_annotate_group)},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vista de linea de tiempo
 * \{ */

static const ToolDecl *sequencer_timeline_box[] = {&defs_sequencer_select::box_timeline};
static const ToolDecl *sequencer_timeline_blade[] = {&defs_sequencer_generic::blade};

/* La linea de tiempo sola no tiene anotaciones: se dibuja sobre la imagen, y aqui no hay
 * imagen. Por eso solo son dos herramientas, la lista mas corta del catalogo. */
static const ToolEntry sequencer_timeline_entries[] = {
    {span(sequencer_timeline_box)},
    {span(sequencer_timeline_blade)},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vista partida
 * \{ */

/* La vista partida NO es la suma de las otras dos: se queda con la caja y la cuchilla de
 * la linea de tiempo y anade las anotaciones, pero pierde el cursor y las cuatro de
 * transformacion de la previsualizacion. Reproducirla como una union daria cinco
 * herramientas de mas. Ojo tambien al orden, que mete las anotaciones EN MEDIO, entre la
 * caja y la cuchilla. */
static const ToolEntry sequencer_split_entries[] = {
    {span(sequencer_timeline_box)},
    {span(sequencer_annotate_group)},
    {},
    {span(sequencer_timeline_blade)},
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name La barra
 * \{ */

/* El Python declara ademas una lista comun (`_tools[None]`) VACIA. Aqui no se declara
 * ninguna entrada de modo comun, que es lo mismo con menos: un array de cero elementos
 * no es valido en C++, y el registro recorre las comunes solo si existen. */
static const ModeTools sequencer_modes[] = {
    {"PREVIEW", span(sequencer_preview_entries)},
    {"SEQUENCER", span(sequencer_timeline_entries)},
    {"SEQUENCER_PREVIEW", span(sequencer_split_entries)},
};

/**
 * El "modo" de este espacio es `space_data.view_type`.
 *
 * Se resuelve a traves de la enumeracion RNA y no con un `switch` sobre `SEQ_VIEW_*`
 * porque el nombre del modo que hay que devolver es el IDENTIFICADOR de la enumeracion
 * ('SEQUENCER', 'PREVIEW', 'SEQUENCER_PREVIEW'), que es lo que el Python leia y lo que
 * usa la linea base como clave. Escribir esas tres cadenas a mano las duplicaria, y si
 * la enumeracion cambiara la barra se quedaria vacia sin que nada avisara.
 *
 * Sin espacio se devuelve `nullptr`: el Python deja el modo en `None` en ese caso — no
 * cae a ninguna vista, como si hace el editor de imagen — y entonces solo saldrian las
 * herramientas comunes, que aqui no hay.
 */
static const char *sequencer_mode_from_context(const bContext *C)
{
  const SpaceSeq *sseq = CTX_wm_space_seq(C);
  if (sseq == nullptr) {
    return nullptr;
  }
  const char *identifier = nullptr;
  if (!RNA_enum_identifier(rna_enum_space_sequencer_view_type_items, sseq->view, &identifier)) {
    return nullptr;
  }
  return identifier;
}

/* Ojo con el prefijo de keymap: es el de la clase del Python, y NINGUNA de las diez
 * herramientas del espacio lo usa. Todas llevan "Preview Tool: ..." o "Sequencer
 * Tool: ..." literal. Sigue aqui porque es lo que declara la clase y lo que necesita el
 * registro de keymaps, no porque de el salga ningun nombre. */
const ToolbarDecl toolbar_sequencer = {
    /*space_type*/ SPACE_SEQ,
    /*keymap_prefix*/ "Sequence Editor Tool:",
    /*tool_fallback_id*/ "builtin.select",
    /*mode_from_context*/ sequencer_mode_from_context,
    /*modes*/ span(sequencer_modes),
};

/** \} */

}  // namespace flipendo::toolsystem
