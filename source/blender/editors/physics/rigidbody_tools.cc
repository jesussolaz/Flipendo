/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editor_physics
 *
 * Puerto nativo en C++ de `scripts/startup/bl_operators/rigidbody.py` (Flipendo carril C):
 * los tres operadores de cuerpo rigido que vivian en Python y que operaban a base de
 * llamar a otros operadores y de asignar propiedades RNA.
 *
 *   rigidbody.object_settings_copy -> RIGIDBODY_OT_object_settings_copy
 *   rigidbody.bake_to_keyframes    -> RIGIDBODY_OT_bake_to_keyframes
 *   rigidbody.connect              -> RIGIDBODY_OT_connect
 *
 * Se conservan idname, propiedades, textos, flags y `poll`. Donde el Python asignaba una
 * propiedad RNA (`setattr(rb_to, attr, ...)`, `con.type = ...`) se asigna por RNA y se
 * llama a `RNA_property_update()`, igual que hace `pyrna_struct_setattro`: esos setters
 * hacen trabajo de verdad (marcar el cuerpo para revalidar, liberar el objeto de fisica)
 * y saltarselos escribiendo el DNA a pelo cambia el comportamiento.
 */

#include <algorithm>
#include <cmath>

#include "BLI_assert.h"
#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"
#include "BLI_vector.hh"

#include "DNA_anim_types.h"
#include "DNA_collection_types.h"
#include "DNA_object_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"

#include "BLT_translation.hh"

#include "BKE_anim_data.hh"
#include "BKE_collection.hh"
#include "BKE_context.hh"
#include "BKE_fcurve.hh"
#include "BKE_layer.hh"
#include "BKE_main.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "ANIM_action_legacy.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"
#include "DEG_depsgraph_query.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_object.hh"
#include "ED_screen.hh"

#include "physics_intern.hh"

using blender::float3;
using blender::Vector;

/* -------------------------------------------------------------------------- */
/** \name Comun
 * \{ */

/* `poll` de los tres: `obj and obj.rigid_body`. */
static bool rigidbody_active_object_poll(bContext *C)
{
  const Object *ob = blender::ed::object::context_object(C);
  return ob != nullptr && ob->rigidbody_object != nullptr;
}

static void object_select_set(bContext *C, Object *ob, const bool select)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  if (Base *base = BKE_view_layer_base_find(view_layer, ob)) {
    blender::ed::object::base_select(base, select ? blender::ed::object::BA_SELECT :
                                                    blender::ed::object::BA_DESELECT);
  }
}

static Vector<Object *> selected_objects_get(bContext *C)
{
  Vector<Object *> objects;
  CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
    objects.append(ob);
  }
  CTX_DATA_END;
  return objects;
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name rigidbody.object_settings_copy
 * \{ */

/* Las dieciocho propiedades que copiaba `CopyRigidbodySettings._attrs`, en el mismo
 * orden. Se copian por identificador RNA a proposito: varias tienen setter propio
 * (`type` y `collision_shape` liberan el objeto de fisica y marcan para revalidar). */
static const char *rigidbody_copy_attrs[] = {
    "type",
    "kinematic",
    "mass",
    "collision_shape",
    "use_margin",
    "collision_margin",
    "friction",
    "restitution",
    "use_deactivation",
    "use_start_deactivated",
    "deactivate_linear_velocity",
    "deactivate_angular_velocity",
    "linear_damping",
    "angular_damping",
    "collision_collections",
    "mesh_source",
    "use_deform",
    "enabled",
};

/* Copia el valor de una propiedad de `from` a `to` y lanza su callback de
 * actualizacion, que es exactamente lo que hace `setattr()` de bpy
 * (`pyrna_struct_setattro` -> `pyrna_py_to_prop` -> `RNA_property_update`). No se usa
 * `RNA_property_copy()` porque ese pasa por la maquinaria de liboverride: aqui interesa
 * el camino llano, el mismo que recorria el Python. */
static void rigidbody_copy_property(bContext *C,
                                    PointerRNA *to,
                                    PointerRNA *from,
                                    PropertyRNA *prop)
{
  const int len = RNA_property_array_length(to, prop);

  switch (RNA_property_type(prop)) {
    case PROP_BOOLEAN: {
      if (len > 0) {
        bool values[RNA_MAX_ARRAY_LENGTH];
        RNA_property_boolean_get_array(from, prop, values);
        RNA_property_boolean_set_array(to, prop, values);
      }
      else {
        RNA_property_boolean_set(to, prop, RNA_property_boolean_get(from, prop));
      }
      break;
    }
    case PROP_INT: {
      if (len > 0) {
        int values[RNA_MAX_ARRAY_LENGTH];
        RNA_property_int_get_array(from, prop, values);
        RNA_property_int_set_array(to, prop, values);
      }
      else {
        RNA_property_int_set(to, prop, RNA_property_int_get(from, prop));
      }
      break;
    }
    case PROP_FLOAT: {
      if (len > 0) {
        float values[RNA_MAX_ARRAY_LENGTH];
        RNA_property_float_get_array(from, prop, values);
        RNA_property_float_set_array(to, prop, values);
      }
      else {
        RNA_property_float_set(to, prop, RNA_property_float_get(from, prop));
      }
      break;
    }
    case PROP_ENUM:
      RNA_property_enum_set(to, prop, RNA_property_enum_get(from, prop));
      break;
    default:
      BLI_assert_unreachable();
      break;
  }

  RNA_property_update(C, to, prop);
}

static wmOperatorStatus rigidbody_object_settings_copy_exec(bContext *C, wmOperator *op)
{
  Object *obj_act = blender::ed::object::context_object(C);
  if (obj_act == nullptr || obj_act->rigidbody_object == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* Se deseleccionan los objetos que no son malla y los que ya tienen cuerpo rigido,
   * para que `rigidbody.objects_add` no los toque; los segundos se vuelven a
   * seleccionar despues. Literalmente lo que hacia el Python. */
  Vector<Object *> rb_objects;
  for (Object *ob : selected_objects_get(C)) {
    if (ob->type != OB_MESH || ob->rigidbody_object != nullptr) {
      object_select_set(C, ob, false);
      if (ob->rigidbody_object != nullptr) {
        rb_objects.append(ob);
      }
    }
  }

  /* `bpy.ops.rigidbody.objects_add()`. Se pasan las propiedades explicitas: al llamar
   * por idname desde C++, `WM_operator_last_properties_init()` rellenaria las que no se
   * pongan con las de la ultima ejecucion (es OPTYPE_REGISTER), y `bpy.ops` no hace eso. */
  {
    PointerRNA ptr;
    WM_operator_properties_create(&ptr, "rigidbody.objects_add");
    RNA_enum_set(&ptr, "type", RBO_TYPE_ACTIVE);
    WM_operator_name_call(C, "rigidbody.objects_add", WM_OP_EXEC_DEFAULT, &ptr, nullptr);
    WM_operator_properties_free(&ptr);
  }

  for (Object *ob : rb_objects) {
    object_select_set(C, ob, true);
  }

  const Vector<Object *> objects = selected_objects_get(C);
  if (objects.is_empty()) {
    return OPERATOR_FINISHED;
  }

  PointerRNA ptr_from = RNA_pointer_create_discrete(
      &obj_act->id, &RNA_RigidBodyObject, obj_act->rigidbody_object);

  for (Object *ob : objects) {
    if (ob == obj_act) {
      continue;
    }
    if (ob->rigidbody_object == nullptr) {
      /* En Python esto seria un AttributeError sobre `None`. Aqui se ignora el objeto:
       * solo ocurre si `rigidbody.objects_add` no pudo anadirlo. */
      continue;
    }
    PointerRNA ptr_to = RNA_pointer_create_discrete(
        &ob->id, &RNA_RigidBodyObject, ob->rigidbody_object);

    for (const char *attr : rigidbody_copy_attrs) {
      PropertyRNA *prop = RNA_struct_find_property(&ptr_to, attr);
      if (prop == nullptr) {
        BKE_reportf(op->reports, RPT_WARNING, "Unknown rigid body property '%s'", attr);
        continue;
      }
      rigidbody_copy_property(C, &ptr_to, &ptr_from, prop);
    }
  }

  return OPERATOR_FINISHED;
}

void RIGIDBODY_OT_object_settings_copy(wmOperatorType *ot)
{
  /* identifiers */
  ot->idname = "RIGIDBODY_OT_object_settings_copy";
  ot->name = "Copy Rigid Body Settings";
  ot->description = "Copy Rigid Body settings from active object to selected";

  /* callbacks */
  ot->exec = rigidbody_object_settings_copy_exec;
  ot->poll = rigidbody_active_object_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name rigidbody.bake_to_keyframes
 * \{ */

/* `scene.frame_set(f)` de RNA: fija el fotograma y recorre TODAS las capas de vista
 * actualizando su grafo de dependencias, que es lo que hace avanzar la simulacion. */
static void scene_frame_set(bContext *C, Main *bmain, Scene *scene, const int frame)
{
  double cfra = double(frame);
  CLAMP(cfra, MINAFRAME, MAXFRAME);
  BKE_scene_frame_set(scene, cfra);

  LISTBASE_FOREACH (ViewLayer *, view_layer, &scene->view_layers) {
    Depsgraph *depsgraph = BKE_scene_ensure_depsgraph(bmain, scene, view_layer);
    BKE_scene_graph_update_for_newframe(depsgraph);
  }
  BKE_scene_camera_switch_update(scene);
  UNUSED_VARS(C);
}

static wmOperatorStatus rigidbody_bake_to_keyframes_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);

  const int frame_start = RNA_int_get(op->ptr, "frame_start");
  const int frame_end = RNA_int_get(op->ptr, "frame_end");
  const int step = RNA_int_get(op->ptr, "step");
  const int frame_orig = scene->r.cfra;

  /* Solo se hornean los cuerpos rigidos ACTIVE; el resto se deselecciona, igual que en
   * el Python (y la deseleccion es visible al terminar, no se restaura). */
  for (Object *ob : selected_objects_get(C)) {
    if (ob->rigidbody_object == nullptr || ob->rigidbody_object->type != RBO_TYPE_ACTIVE) {
      object_select_set(C, ob, false);
    }
  }

  const Vector<Object *> objects = selected_objects_get(C);
  if (objects.is_empty()) {
    return OPERATOR_FINISHED;
  }

  /* Fotogramas en los que se pone clave: `range(start, end + 1, step)`. */
  Vector<int> frames_step;
  for (int f = frame_start; f <= frame_end; f += step) {
    frames_step.append(f);
  }

  /* Se recorre la linea de tiempo COMPLETA aunque solo se guarde cada `step`: la
   * simulacion tiene que correr desde el principio o el resultado no es el mismo. */
  Vector<Vector<blender::float4x4>> bake;
  {
    int next_step = 0;
    for (int f = frame_start; f <= frame_end; f++) {
      scene_frame_set(C, bmain, scene, f);
      if (next_step < frames_step.size() && frames_step[next_step] == f) {
        Vector<blender::float4x4> mats;
        for (Object *ob : objects) {
          mats.append(blender::float4x4(ob->object_to_world()));
        }
        bake.append(std::move(mats));
        next_step++;
      }
    }
  }

  for (const int i : frames_step.index_range()) {
    scene_frame_set(C, bmain, scene, frames_step[i]);

    for (const int j : objects.index_range()) {
      Object *ob = objects[j];
      blender::float4x4 mat = bake[i][j];

      /* El transform del mundo se pasa al espacio del padre, para que los objetos
       * emparentados no se desplacen al hornear. */
      if (ob->parent != nullptr) {
        const blender::float4x4 parent_inv = blender::math::invert(
            blender::float4x4(ob->parentinv));
        const blender::float4x4 parent_world_inv = blender::math::invert(
            blender::float4x4(ob->parent->object_to_world()));
        mat = parent_inv * parent_world_inv * mat;
      }

      /* `mat.to_translation()` es la cuarta columna tal cual, y `mat.to_quaternion()`
       * es `mat4_to_quat()` (normaliza la parte 3x3 antes de convertir). */
      copy_v3_v3(ob->loc, mat.location());
      float rot_quat[4];
      mat4_to_quat(rot_quat, mat.ptr());

      if (ob->rotmode == ROT_MODE_QUAT) {
        /* Se elige el signo del cuaternion compatible con el del fotograma anterior:
         * q y -q son la misma rotacion, pero interpolan por caminos opuestos. */
        if (dot_v4v4(ob->quat, rot_quat) < 0.0f) {
          negate_v4(rot_quat);
        }
        copy_v4_v4(ob->quat, rot_quat);
      }
      else if (ob->rotmode == ROT_MODE_AXISANGLE) {
        float axis[3], angle;
        quat_to_axis_angle(axis, &angle, rot_quat);
        copy_v3_v3(ob->rotAxis, axis);
        ob->rotAngle = angle;
      }
      else {
        /* Euler compatible con el del fotograma anterior, como `mat.to_euler(mode, prev)`:
         * de los ocho eulers que representan la misma rotacion se elige el mas cercano al
         * anterior, o la curva pega saltos de 2*pi al interpolar. */
        float eul_compat[3];
        copy_v3_v3(eul_compat, ob->rot);
        mat4_to_compatible_eulO(ob->rot, eul_compat, short(ob->rotmode), mat.ptr());
      }

      DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
    }

    /* `bpy.ops.anim.keyframe_insert_by_name(type='BUILTIN_KSI_LocRot')`. */
    PointerRNA ptr;
    WM_operator_properties_create(&ptr, "anim.keyframe_insert_by_name");
    RNA_string_set(&ptr, "type", "BUILTIN_KSI_LocRot");
    WM_operator_name_call(C, "anim.keyframe_insert_by_name", WM_OP_EXEC_DEFAULT, &ptr, nullptr);
    WM_operator_properties_free(&ptr);
  }

  /* `bpy.ops.rigidbody.objects_remove()`: los objetos horneados salen de la simulacion. */
  WM_operator_name_call(C, "rigidbody.objects_remove", WM_OP_EXEC_DEFAULT, nullptr, nullptr);

  /* Limpieza de claves: se quitan las intermedias que no aportan nada (las que estan
   * practicamente en la recta entre su vecina anterior y la siguiente) y se pasa todo a
   * interpolacion lineal, que da mejor resultado visual en una simulacion horneada. */
  for (Object *ob : objects) {
    AnimData *adt = BKE_animdata_from_id(&ob->id);
    if (adt == nullptr || adt->action == nullptr) {
      continue;
    }
    for (FCurve *fcu : blender::animrig::legacy::fcurves_for_assigned_action(adt)) {
      int i = 1;
      while (i < fcu->totvert - 1) {
        const float val_prev = fcu->bezt[i - 1].vec[1][1];
        const float val_next = fcu->bezt[i + 1].vec[1][1];
        const float val = fcu->bezt[i].vec[1][1];

        if (fabsf(val - val_prev) + fabsf(val - val_next) < 0.0001f) {
          BKE_fcurve_delete_key(fcu, i);
          BKE_fcurve_handles_recalc(fcu);
        }
        else {
          i++;
        }
      }
      for (int k = 0; k < fcu->totvert; k++) {
        fcu->bezt[k].ipo = BEZT_IPO_LIN;
      }
    }
    DEG_id_tag_update(&ob->id, ID_RECALC_ANIMATION);
  }

  scene_frame_set(C, bmain, scene, frame_orig);

  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, nullptr);
  WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus rigidbody_bake_to_keyframes_invoke(bContext *C,
                                                           wmOperator *op,
                                                           const wmEvent * /*event*/)
{
  const Scene *scene = CTX_data_scene(C);
  RNA_int_set(op->ptr, "frame_start", scene->r.sfra);
  RNA_int_set(op->ptr, "frame_end", scene->r.efra);
  return WM_operator_props_dialog_popup(C, op, 200);
}

void RIGIDBODY_OT_bake_to_keyframes(wmOperatorType *ot)
{
  /* identifiers */
  ot->idname = "RIGIDBODY_OT_bake_to_keyframes";
  ot->name = "Bake to Keyframes";
  ot->description = "Bake rigid body transformations of selected objects to keyframes";

  /* callbacks */
  ot->invoke = rigidbody_bake_to_keyframes_invoke;
  ot->exec = rigidbody_bake_to_keyframes_exec;
  ot->poll = rigidbody_active_object_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_int(
      ot->srna, "frame_start", 1, 0, 300000, "Start Frame", "Start frame for baking", 0, 300000);
  RNA_def_int(
      ot->srna, "frame_end", 250, 1, 300000, "End Frame", "End frame for baking", 1, 300000);
  RNA_def_int(ot->srna, "step", 1, 1, 120, "Frame Step", "Frame Step", 1, 120);
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name rigidbody.connect
 * \{ */

enum {
  RB_PIVOT_CENTER = 0,
  RB_PIVOT_ACTIVE = 1,
  RB_PIVOT_SELECTED = 2,
};

enum {
  RB_PATTERN_SELECTED_TO_ACTIVE = 0,
  RB_PATTERN_CHAIN_DISTANCE = 1,
};

static const EnumPropertyItem rigidbody_pivot_type_items[] = {
    {RB_PIVOT_CENTER,
     "CENTER",
     0,
     "Center",
     "Pivot location is between the constrained rigid bodies"},
    {RB_PIVOT_ACTIVE, "ACTIVE", 0, "Active", "Pivot location is at the active object position"},
    {RB_PIVOT_SELECTED,
     "SELECTED",
     0,
     "Selected",
     "Pivot location is at the selected object position"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem rigidbody_connection_pattern_items[] = {
    {RB_PATTERN_SELECTED_TO_ACTIVE,
     "SELECTED_TO_ACTIVE",
     0,
     "Selected to Active",
     "Connect selected objects to the active object"},
    {RB_PATTERN_CHAIN_DISTANCE,
     "CHAIN_DISTANCE",
     0,
     "Chain by Distance",
     "Connect objects as a chain based on distance, starting at the active object"},
    {0, nullptr, 0, nullptr, nullptr},
};

static void rigidbody_connect_add_constraint(bContext *C,
                                             wmOperator *op,
                                             Object *object1,
                                             Object *object2)
{
  if (object1 == object2) {
    return;
  }

  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const int pivot_type = RNA_enum_get(op->ptr, "pivot_type");
  const int con_type = RNA_enum_get(op->ptr, "con_type");

  float loc[3];
  if (pivot_type == RB_PIVOT_ACTIVE) {
    copy_v3_v3(loc, object1->loc);
  }
  else if (pivot_type == RB_PIVOT_SELECTED) {
    copy_v3_v3(loc, object2->loc);
  }
  else {
    add_v3_v3v3(loc, object1->loc, object2->loc);
    mul_v3_fl(loc, 0.5f);
  }

  /* `bpy.data.objects.new("Constraint", object_data=None)` + link a la coleccion maestra
   * de la escena, que es lo que hace `context.scene.collection.objects.link()`. */
  Object *ob = BKE_object_add_only_object(bmain, OB_EMPTY, "Constraint");
  ob->data = nullptr;
  copy_v3_v3(ob->loc, loc);
  BKE_collection_object_add(bmain, scene->master_collection, ob);

  BKE_view_layer_synced_ensure(scene, view_layer);
  if (Base *base = BKE_view_layer_base_find(view_layer, ob)) {
    BKE_view_layer_base_select_and_set_active(view_layer, base);
  }
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
  DEG_relations_tag_update(bmain);

  /* `bpy.ops.rigidbody.constraint_add()`: propiedades explicitas, ver la nota de
   * `object_settings_copy` sobre `WM_operator_last_properties_init()`. */
  {
    PointerRNA ptr;
    WM_operator_properties_create(&ptr, "rigidbody.constraint_add");
    RNA_enum_set(&ptr, "type", RBC_TYPE_FIXED);
    WM_operator_name_call(C, "rigidbody.constraint_add", WM_OP_EXEC_DEFAULT, &ptr, nullptr);
    WM_operator_properties_free(&ptr);
  }

  Object *con_obj = blender::ed::object::context_active_object(C);
  if (con_obj == nullptr || con_obj->rigidbody_constraint == nullptr) {
    return;
  }
  con_obj->empty_drawtype = OB_ARROWS;

  /* `con.type` tiene setter propio (marca el constraint para revalidar), asi que se
   * asigna por RNA y no escribiendo `rbc->type`. */
  PointerRNA con_ptr = RNA_pointer_create_discrete(
      &con_obj->id, &RNA_RigidBodyConstraint, con_obj->rigidbody_constraint);
  RNA_enum_set(&con_ptr, "type", con_type);
  if (PropertyRNA *prop = RNA_struct_find_property(&con_ptr, "type")) {
    RNA_property_update(C, &con_ptr, prop);
  }

  con_obj->rigidbody_constraint->ob1 = object1;
  con_obj->rigidbody_constraint->ob2 = object2;
  DEG_id_tag_update(&con_obj->id, ID_RECALC_TRANSFORM);
  DEG_relations_tag_update(bmain);
}

static wmOperatorStatus rigidbody_connect_exec(bContext *C, wmOperator *op)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  const Vector<Object *> objects = selected_objects_get(C);
  Object *obj_act = blender::ed::object::context_active_object(C);
  const int pattern = RNA_enum_get(op->ptr, "connection_pattern");
  bool changed = false;

  if (pattern == RB_PATTERN_CHAIN_DISTANCE) {
    Vector<Object *> objs_sorted;
    objs_sorted.append(obj_act);

    Vector<Object *> objects_tmp = objects;
    const int64_t act_index = objects_tmp.first_index_of_try(obj_act);
    if (act_index >= 0) {
      objects_tmp.remove(act_index);
    }

    Object *last_obj = obj_act;
    while (!objects_tmp.is_empty()) {
      /* `list.sort(key=...)` de Python es estable: ante distancias iguales gana el que
       * ya iba antes. `std::stable_sort` es el equivalente exacto. */
      std::stable_sort(objects_tmp.begin(), objects_tmp.end(), [&](Object *a, Object *b) {
        return len_v3v3(last_obj->loc, a->loc) < len_v3v3(last_obj->loc, b->loc);
      });
      last_obj = objects_tmp[0];
      objects_tmp.remove(0);
      objs_sorted.append(last_obj);
    }

    for (int i = 1; i < objs_sorted.size(); i++) {
      rigidbody_connect_add_constraint(C, op, objs_sorted[i - 1], objs_sorted[i]);
      changed = true;
    }
  }
  else {
    for (Object *ob : objects) {
      rigidbody_connect_add_constraint(C, op, obj_act, ob);
      changed = true;
    }
  }

  if (!changed) {
    BKE_report(op->reports, RPT_WARNING, "No other objects selected");
    return OPERATOR_CANCELLED;
  }

  /* Se restaura la seleccion que habia al empezar. */
  BKE_view_layer_synced_ensure(scene, view_layer);
  BKE_view_layer_base_deselect_all(scene, view_layer);
  for (Object *ob : objects) {
    object_select_set(C, ob, true);
  }
  if (Base *base = BKE_view_layer_base_find(view_layer, obj_act)) {
    BKE_view_layer_base_select_and_set_active(view_layer, base);
  }
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);

  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, nullptr);
  return OPERATOR_FINISHED;
}

void RIGIDBODY_OT_connect(wmOperatorType *ot)
{
  /* identifiers */
  ot->idname = "RIGIDBODY_OT_connect";
  ot->name = "Connect Rigid Bodies";
  ot->description = "Create rigid body constraints between selected rigid bodies";

  /* callbacks */
  ot->exec = rigidbody_connect_exec;
  ot->poll = rigidbody_active_object_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties. `con_type` tomaba sus items del propio enum de RigidBodyConstraint.type
   * (`bpy.types.RigidBodyConstraint.bl_rna.properties["type"].enum_items`); aqui se usa
   * directamente ese mismo array, que es de donde salian. */
  RNA_def_enum(ot->srna,
               "con_type",
               rna_enum_rigidbody_constraint_type_items,
               RBC_TYPE_FIXED,
               "Type",
               "Type of generated constraint");
  RNA_def_enum(ot->srna,
               "pivot_type",
               rigidbody_pivot_type_items,
               RB_PIVOT_CENTER,
               "Location",
               "Constraint pivot location");
  RNA_def_enum(ot->srna,
               "connection_pattern",
               rigidbody_connection_pattern_items,
               RB_PATTERN_SELECTED_TO_ACTIVE,
               "Connection Pattern",
               "Pattern used to connect objects");
}

/** \} */
