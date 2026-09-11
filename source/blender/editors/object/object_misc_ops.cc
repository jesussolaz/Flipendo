/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 *
 * Puerto nativo en C++ de cinco operadores pequenos de
 * `scripts/startup/bl_operators/object.py` (Flipendo carril C):
 *
 *   object.isolate_type_render          -> OBJECT_OT_isolate_type_render
 *   object.hide_render_clear_all        -> OBJECT_OT_hide_render_clear_all
 *   object.instance_offset_from_cursor  -> OBJECT_OT_instance_offset_from_cursor
 *   object.instance_offset_to_cursor    -> OBJECT_OT_instance_offset_to_cursor
 *   object.instance_offset_from_object  -> OBJECT_OT_instance_offset_from_object
 *
 * Mismos idnames, mismos textos, mismos flags (los tres de desplazamiento son INTERNAL |
 * UNDO, sin REGISTER) y los mismos `poll`.
 */

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_vector.hh"

#include "DNA_collection_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_scene.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_object.hh"

#include "object_intern.hh"

namespace blender::ed::object {

/* -------------------------------------------------------------------------- */
/** \name object.isolate_type_render
 * \{ */

static wmOperatorStatus isolate_type_render_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const Object *obact = context_object(C);
  const short act_type = obact->type;

  BKE_view_layer_synced_ensure(scene, view_layer);

  /* `context.visible_objects`, y dentro `obj.select_get()`: los seleccionados se
   * muestran y los NO seleccionados del mismo tipo que el activo se ocultan. Los de otro
   * tipo se quedan como estaban. */
  Vector<Object *> visible;
  CTX_DATA_BEGIN (C, Object *, ob, visible_objects) {
    visible.append(ob);
  }
  CTX_DATA_END;

  for (Object *ob : visible) {
    const Base *base = BKE_view_layer_base_find(view_layer, ob);
    const bool selected = (base != nullptr) && (base->flag & BASE_SELECTED);
    if (selected) {
      ob->visibility_flag &= ~OB_HIDE_RENDER;
    }
    else if (ob->type == act_type) {
      ob->visibility_flag |= OB_HIDE_RENDER;
    }
    else {
      continue;
    }
    DEG_id_tag_update(&ob->id, ID_RECALC_SYNC_TO_EVAL);
  }

  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, nullptr);
  return OPERATOR_FINISHED;
}

static bool isolate_type_render_poll(bContext *C)
{
  /* `return (ob is not None)`, literal. */
  return context_object(C) != nullptr;
}

void OBJECT_OT_isolate_type_render(wmOperatorType *ot)
{
  ot->name = "Restrict Render Unselected";
  ot->idname = "OBJECT_OT_isolate_type_render";
  ot->description =
      "Hide unselected render objects of same type as active by setting the hide render flag";

  ot->exec = isolate_type_render_exec;
  ot->poll = isolate_type_render_poll;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name object.hide_render_clear_all
 * \{ */

static wmOperatorStatus hide_render_clear_all_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);

  /* `context.scene.objects` recorre TODOS los objetos de la escena, no solo los de la
   * capa de vista ni solo los visibles. */
  FOREACH_SCENE_OBJECT_BEGIN (scene, ob) {
    ob->visibility_flag &= ~OB_HIDE_RENDER;
    DEG_id_tag_update(&ob->id, ID_RECALC_SYNC_TO_EVAL);
  }
  FOREACH_SCENE_OBJECT_END;

  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, nullptr);
  return OPERATOR_FINISHED;
}

void OBJECT_OT_hide_render_clear_all(wmOperatorType *ot)
{
  ot->name = "Clear All Restrict Render";
  ot->idname = "OBJECT_OT_hide_render_clear_all";
  ot->description = "Reveal all render objects by setting the hide render flag";

  ot->exec = hide_render_clear_all_exec;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name Desplazamiento de instancia de coleccion
 * \{ */

static wmOperatorStatus instance_offset_from_cursor_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  Collection *collection = CTX_data_collection(C);
  if (collection == nullptr) {
    return OPERATOR_CANCELLED;
  }
  copy_v3_v3(collection->instance_offset, scene->cursor.location);
  DEG_id_tag_update(&collection->id, ID_RECALC_SYNC_TO_EVAL);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, nullptr);
  return OPERATOR_FINISHED;
}

void OBJECT_OT_instance_offset_from_cursor(wmOperatorType *ot)
{
  ot->name = "Set Offset from Cursor";
  ot->idname = "OBJECT_OT_instance_offset_from_cursor";
  ot->description = "Set offset used for collection instances based on cursor position";

  ot->exec = instance_offset_from_cursor_exec;

  /* `bl_options = {'INTERNAL', 'UNDO'}`: sin REGISTER, no sale en el buscador. */
  ot->flag = OPTYPE_INTERNAL | OPTYPE_UNDO;
}

static wmOperatorStatus instance_offset_to_cursor_exec(bContext *C, wmOperator * /*op*/)
{
  Scene *scene = CTX_data_scene(C);
  Collection *collection = CTX_data_collection(C);
  if (collection == nullptr) {
    return OPERATOR_CANCELLED;
  }
  copy_v3_v3(scene->cursor.location, collection->instance_offset);
  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
  WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, nullptr);
  return OPERATOR_FINISHED;
}

void OBJECT_OT_instance_offset_to_cursor(wmOperatorType *ot)
{
  ot->name = "Set Cursor to Offset";
  ot->idname = "OBJECT_OT_instance_offset_to_cursor";
  ot->description = "Set cursor position to the offset used for collection instances";

  ot->exec = instance_offset_to_cursor_exec;

  ot->flag = OPTYPE_INTERNAL | OPTYPE_UNDO;
}

static wmOperatorStatus instance_offset_from_object_exec(bContext *C, wmOperator * /*op*/)
{
  Collection *collection = CTX_data_collection(C);
  Object *ob = CTX_data_active_object(C);
  if (collection == nullptr || ob == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* `context.active_object.evaluated_get(view_layer.depsgraph).matrix_world` — el objeto
   * EVALUADO, no el original: si tiene restricciones o padre animado, la posicion buena
   * es la del grafo de dependencias. */
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  const Object *ob_eval = DEG_get_evaluated(depsgraph, ob);
  copy_v3_v3(collection->instance_offset, ob_eval->object_to_world().location());

  DEG_id_tag_update(&collection->id, ID_RECALC_SYNC_TO_EVAL);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, nullptr);
  return OPERATOR_FINISHED;
}

static bool instance_offset_from_object_poll(bContext *C)
{
  /* `return (context.active_object is not None)`, literal. */
  return CTX_data_active_object(C) != nullptr;
}

void OBJECT_OT_instance_offset_from_object(wmOperatorType *ot)
{
  ot->name = "Set Offset from Object";
  ot->idname = "OBJECT_OT_instance_offset_from_object";
  ot->description = "Set offset used for collection instances based on the active object position";

  ot->exec = instance_offset_from_object_exec;
  ot->poll = instance_offset_from_object_poll;

  ot->flag = OPTYPE_INTERNAL | OPTYPE_UNDO;
}

/** \} */

}  // namespace blender::ed::object
