/* SPDX-FileCopyrightText: 2009 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 *
 * Puerto nativo en C++ de los tres operadores de seleccion que vivian en
 * `scripts/startup/bl_operators/object.py` y que bloqueaban la migracion de los menus de
 * la vista 3D (los tres salen en `VIEW3D_MT_select_object`):
 *
 *   object.select_pattern   -> OBJECT_OT_select_pattern
 *   object.select_camera    -> OBJECT_OT_select_camera
 *   object.select_hierarchy -> OBJECT_OT_select_hierarchy
 *
 * Mismos idnames, mismas propiedades, mismos textos, mismos flags y los mismos `poll`.
 */

#include <algorithm>
#include <cstring>

#include "BLI_fnmatch.h"
#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "DNA_ID.h"
#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BLT_translation.hh"

#include "BKE_armature.hh"
#include "BKE_collection.hh"
#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"

#include "DEG_depsgraph.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_object.hh"
#include "ED_screen.hh"

#include "object_intern.hh"

namespace blender::ed::object {

/* -------------------------------------------------------------------------- */
/** \name Comun
 * \{ */

static void call_select_all_deselect(bContext *C, const char *idname)
{
  PointerRNA ptr;
  WM_operator_properties_create(&ptr, idname);
  RNA_enum_set_identifier(C, &ptr, "action", "DESELECT");
  WM_operator_name_call(C, idname, WM_OP_EXEC_DEFAULT, &ptr, nullptr);
  WM_operator_properties_free(&ptr);
}

static void object_select(bContext *C, Object *ob, const bool select)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  if (Base *base = BKE_view_layer_base_find(view_layer, ob)) {
    base_select(base, select ? BA_SELECT : BA_DESELECT);
  }
}

/* `obj.visible_get()` sin argumentos: `BASE_VISIBLE(v3d, base)` con la capa de vista y el
 * espacio 3D del contexto (en segundo plano no hay espacio 3D y `v3d` es nulo, igual que
 * en Python). */
static bool object_visible(bContext *C, Object *ob)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  const Base *base = BKE_view_layer_base_find(view_layer, ob);
  if (base == nullptr) {
    return false;
  }
  return BASE_VISIBLE(CTX_wm_view3d(C), base);
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name object.select_pattern
 * \{ */

/* `fnmatch.fnmatchcase(a, b)` de Python, y su variante en mayusculas cuando no se pide
 * distinguir. `FNM_CASEFOLD` no es exactamente `str.upper()` (uno es ASCII y el otro
 * conoce Unicode), pero coincide en todo lo que sea un nombre de objeto normal. */
static bool pattern_match(const char *name, const char *pattern, const bool case_sensitive)
{
  return fnmatch(pattern, name, case_sensitive ? 0 : FNM_CASEFOLD) == 0;
}

/* `obj.data.bones` recorre el arbol de huesos en preorden; el orden no cambia el
 * resultado (seleccionar es idempotente), pero se recorre igual para no sorprender. */
static void bone_select_by_pattern(ListBase *bonebase, const char *pattern, const bool cs)
{
  LISTBASE_FOREACH (Bone *, bone, bonebase) {
    if (pattern_match(bone->name, pattern, cs)) {
      bone->flag |= BONE_SELECTED;
    }
    bone_select_by_pattern(&bone->childbase, pattern, cs);
  }
}

static wmOperatorStatus object_select_pattern_exec(bContext *C, wmOperator *op)
{
  /* La propiedad declara 65 (ver la nota de abajo): 64 caracteres utiles mas el nulo. */
  char pattern[66];
  RNA_string_get(op->ptr, "pattern", pattern);
  const bool case_sensitive = RNA_boolean_get(op->ptr, "case_sensitive");
  const bool extend = RNA_boolean_get(op->ptr, "extend");

  Object *obj = context_object(C);

  if (obj != nullptr && obj->mode == OB_MODE_POSE) {
    if (!extend) {
      call_select_all_deselect(C, "pose.select_all");
    }
    bArmature *arm = static_cast<bArmature *>(obj->data);
    bone_select_by_pattern(&arm->bonebase, pattern, case_sensitive);
    WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obj);
    return OPERATOR_FINISHED;
  }

  if (obj != nullptr && obj->type == OB_ARMATURE && obj->mode == OB_MODE_EDIT) {
    if (!extend) {
      call_select_all_deselect(C, "armature.select_all");
    }
    bArmature *arm = static_cast<bArmature *>(obj->data);
    if (arm->edbo != nullptr) {
      LISTBASE_FOREACH (EditBone *, ebone, arm->edbo) {
        if (!pattern_match(ebone->name, pattern, case_sensitive)) {
          continue;
        }
        ebone->flag |= BONE_SELECTED | BONE_ROOTSEL | BONE_TIPSEL;
        /* Si el hueso esta conectado a su padre, la punta del padre es la misma
         * articulacion: se selecciona tambien, o la seleccion queda a medias. */
        if ((ebone->flag & BONE_CONNECTED) && ebone->parent != nullptr) {
          ebone->parent->flag |= BONE_TIPSEL;
        }
      }
    }
    WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obj);
    return OPERATOR_FINISHED;
  }

  if (!extend) {
    call_select_all_deselect(C, "object.select_all");
  }
  Vector<Object *> visible;
  CTX_DATA_BEGIN (C, Object *, ob, visible_objects) {
    visible.append(ob);
  }
  CTX_DATA_END;
  for (Object *ob : visible) {
    if (pattern_match(ob->id.name + 2, pattern, case_sensitive)) {
      object_select(C, ob, true);
    }
  }

  Scene *scene = CTX_data_scene(C);
  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
  return OPERATOR_FINISHED;
}

static wmOperatorStatus object_select_pattern_invoke(bContext *C,
                                                     wmOperator *op,
                                                     const wmEvent *event)
{
  return WM_operator_props_popup(C, op, event);
}

/* El `draw()` del Python: el patron en su fila y las dos casillas juntas debajo. */
static void object_select_pattern_ui(bContext * /*C*/, wmOperator *op)
{
  uiLayout *layout = op->layout;
  layout->prop(op->ptr, "pattern", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  uiLayout *row = &layout->row(false);
  row->prop(op->ptr, "case_sensitive", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  row->prop(op->ptr, "extend", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static bool object_select_pattern_poll(bContext *C)
{
  /* `(not obj) or (obj.mode == 'OBJECT') or (obj.type == 'ARMATURE')`, literal. */
  const Object *obj = context_object(C);
  return (obj == nullptr) || (obj->mode == OB_MODE_OBJECT) || (obj->type == OB_ARMATURE);
}

void OBJECT_OT_select_pattern(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Pattern";
  ot->idname = "OBJECT_OT_select_pattern";
  ot->description = "Select objects matching a naming pattern";

  /* API callbacks. */
  ot->exec = object_select_pattern_exec;
  ot->invoke = object_select_pattern_invoke;
  ot->ui = object_select_pattern_ui;
  ot->poll = object_select_pattern_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  /* Ojo con el 65: en Python `maxlen=64` NO llega a RNA como 64. `bpy_props` guarda
   * `maxlen + 1` para dejar sitio al nulo, asi que la propiedad registrada declara 65. Si
   * aqui se pone 64, el contrato deja de ser el mismo. Lo cazo `--fl-check-optypes`. */
  PropertyRNA *prop = RNA_def_string(ot->srna,
                                     "pattern",
                                     "*",
                                     65,
                                     "Pattern",
                                     "Name filter using '*', '?' and '[abc]' unix style wildcards");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_TEXT);
  /* `bl_property = "pattern"`: es la que se enfoca al abrir el dialogo. */
  ot->prop = prop;

  RNA_def_boolean(
      ot->srna, "case_sensitive", false, "Case Sensitive", "Do a case sensitive compare");
  RNA_def_boolean(ot->srna, "extend", true, "Extend", "Extend the existing selection");
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name object.select_camera
 * \{ */

static wmOperatorStatus object_select_camera_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const bool extend = RNA_boolean_get(op->ptr, "extend");

  /* `view.use_local_camera` es `!v3d->scenelock`. Fuera de una vista 3D (por ejemplo en
   * segundo plano) `context.space_data` no es una View3D y manda la camara de escena. */
  const View3D *v3d = CTX_wm_view3d(C);
  Object *camera = (v3d != nullptr && (v3d->scenelock == 0)) ? v3d->camera : scene->camera;

  if (camera == nullptr) {
    BKE_report(op->reports, RPT_WARNING, "No camera found");
    return OPERATOR_CANCELLED;
  }
  if (!BKE_collection_has_object_recursive(scene->master_collection, camera)) {
    BKE_report(op->reports, RPT_WARNING, "Active camera is not in this scene");
    return OPERATOR_CANCELLED;
  }

  if (!extend) {
    call_select_all_deselect(C, "object.select_all");
  }

  BKE_view_layer_synced_ensure(scene, view_layer);
  if (Base *base = BKE_view_layer_base_find(view_layer, camera)) {
    BKE_view_layer_base_select_and_set_active(view_layer, base);
  }
  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
  return OPERATOR_FINISHED;
}

void OBJECT_OT_select_camera(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Camera";
  ot->idname = "OBJECT_OT_select_camera";
  ot->description = "Select the active camera";

  /* API callbacks. */
  ot->exec = object_select_camera_exec;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name object.select_hierarchy
 * \{ */

enum {
  HIERARCHY_PARENT = 0,
  HIERARCHY_CHILD = 1,
};

static const EnumPropertyItem select_hierarchy_direction_items[] = {
    {HIERARCHY_PARENT, "PARENT", 0, "Parent", ""},
    {HIERARCHY_CHILD, "CHILD", 0, "Child", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static wmOperatorStatus object_select_hierarchy_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const int direction = RNA_enum_get(op->ptr, "direction");
  const bool extend = RNA_boolean_get(op->ptr, "extend");

  Vector<Object *> selected_objects;
  CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
    selected_objects.append(ob);
  }
  CTX_DATA_END;

  Object *obj_act = context_object(C);
  /* El Python anade el objeto activo a la lista si no estaba seleccionado. */
  if (obj_act != nullptr && !selected_objects.contains(obj_act)) {
    selected_objects.append(obj_act);
  }

  Vector<Object *> select_new;
  Object *act_new = nullptr;

  if (direction == HIERARCHY_PARENT) {
    for (Object *ob : selected_objects) {
      Object *parent = ob->parent;
      if (parent != nullptr && object_visible(C, parent)) {
        if (obj_act == ob) {
          act_new = parent;
        }
        select_new.append(parent);
      }
    }
  }
  else {
    /* `obj.children` no es RNA: esta en bpy_types.py y recorre `bpy.data.objects`
     * filtrando por padre, o sea el orden de `bmain->objects`. */
    for (Object *ob : selected_objects) {
      LISTBASE_FOREACH (Object *, child, &bmain->objects) {
        if (child->parent == ob && object_visible(C, child)) {
          select_new.append(child);
        }
      }
    }
    if (!select_new.is_empty()) {
      std::stable_sort(select_new.begin(), select_new.end(), [](Object *a, Object *b) {
        /* `select_new.sort(key=lambda o: o.name)` de Python es orden de cadena a secas,
         * no "natural": "Cube.10" va antes que "Cube.2". */
        return strcmp(a->id.name + 2, b->id.name + 2) < 0;
      });
      act_new = select_new[0];
    }
  }

  if (select_new.is_empty()) {
    return OPERATOR_CANCELLED;
  }

  if (!extend) {
    call_select_all_deselect(C, "object.select_all");
  }
  for (Object *ob : select_new) {
    object_select(C, ob, true);
  }

  /* `view_layer.objects.active = act_new`. Puede ser nulo: si el padre del objeto activo
   * no era visible, `act_new` se queda sin asignar y la capa de vista se queda SIN objeto
   * activo. Es raro, pero es lo que hace el Python y hay que conservarlo. */
  BKE_view_layer_synced_ensure(scene, view_layer);
  view_layer->basact = (act_new != nullptr) ? BKE_view_layer_base_find(view_layer, act_new) :
                                              nullptr;

  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
  return OPERATOR_FINISHED;
}

static bool object_select_hierarchy_poll(bContext *C)
{
  /* `return context.object`, literal. */
  return context_object(C) != nullptr;
}

void OBJECT_OT_select_hierarchy(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Hierarchy";
  ot->idname = "OBJECT_OT_select_hierarchy";
  ot->description = "Select object relative to the active object's position in the hierarchy";

  /* API callbacks. */
  ot->exec = object_select_hierarchy_exec;
  ot->poll = object_select_hierarchy_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_enum(ot->srna,
               "direction",
               select_hierarchy_direction_items,
               HIERARCHY_PARENT,
               "Direction",
               "Direction to select in the hierarchy");
  RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the existing selection");
}

/** \} */

}  // namespace blender::ed::object
