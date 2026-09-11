/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editor_physics
 *
 * Implementacion de `--fl-selftest-rigidbody-ops`. Ver `FL_rigidbody_ops_selftest.hh`.
 */

#include <cstdio>
#include <cstring>
#include <functional>

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"

#include "DNA_collection_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_rigidbody_types.h"
#include "DNA_scene_types.h"

#include "BKE_appdir.hh"
#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_object.hh"

#include "FL_selftest_compare.hh"

#include "FL_rigidbody_ops_selftest.hh"

namespace flipendo::rigidbody_ops_selftest {

using namespace blender;

namespace {

/* Las mismas dieciocho propiedades, en el mismo orden, que copiaba el Python y que se
 * vuelcan aqui. Se leen por identificador RNA para que el texto salga identico al del
 * guion de captura, que hacia `getattr(rb, prop)`. */
const char *rb_props[] = {
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

/* -------------------------------------------------------------------------- */
/** \name Utilidades de escena
 * \{ */

void call_op(bContext *C, const char *idname, const std::function<void(PointerRNA *)> &fill)
{
  PointerRNA ptr;
  WM_operator_properties_create(&ptr, idname);
  if (fill) {
    fill(&ptr);
  }
  WM_operator_name_call(C, idname, WM_OP_EXEC_DEFAULT, &ptr, nullptr);
  WM_operator_properties_free(&ptr);
}

/* `purge()` del guion de captura, en el mismo orden: objetos, mallas huerfanas, mundo de
 * cuerpo rigido y colecciones. El orden importa: el mundo tiene una coleccion propia. */
void purge(bContext *C)
{
  Main *bmain = CTX_data_main(C);
  LISTBASE_FOREACH_MUTABLE (Object *, ob, &bmain->objects) {
    BKE_id_delete(bmain, ob);
  }
  LISTBASE_FOREACH_MUTABLE (Mesh *, mesh, &bmain->meshes) {
    if (mesh->id.us == 0) {
      BKE_id_delete(bmain, mesh);
    }
  }
  if (CTX_data_scene(C)->rigidbody_world != nullptr) {
    call_op(C, "rigidbody.world_remove", nullptr);
  }
  LISTBASE_FOREACH_MUTABLE (Collection *, collection, &bmain->collections) {
    BKE_id_delete(bmain, collection);
  }
  DEG_relations_tag_update(bmain);
}

void select_set(bContext *C, Object *ob, const bool select)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  if (Base *base = BKE_view_layer_base_find(view_layer, ob)) {
    ed::object::base_select(base, select ? ed::object::BA_SELECT : ed::object::BA_DESELECT);
  }
}

/* `select(objs, active)` del guion: deselecciona todo con el operador, marca los que
 * toca y pone el activo. */
void select_only(bContext *C, const Span<Object *> objects, Object *active)
{
  call_op(C, "object.select_all", [](PointerRNA *ptr) {
    RNA_enum_set_identifier(nullptr, ptr, "action", "DESELECT");
  });
  for (Object *ob : objects) {
    select_set(C, ob, true);
  }
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  if (Base *base = BKE_view_layer_base_find(view_layer, active)) {
    BKE_view_layer_base_select_and_set_active(view_layer, base);
  }
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
}

Object *add_cube(bContext *C, const float loc[3])
{
  PointerRNA ptr;
  WM_operator_properties_create(&ptr, "mesh.primitive_cube_add");
  RNA_float_set(&ptr, "size", 1.0f);
  const float rot[3] = {0.0f, 0.0f, 0.0f};
  RNA_float_set_array(&ptr, "location", loc);
  RNA_float_set_array(&ptr, "rotation", rot);
  WM_operator_name_call(C, "mesh.primitive_cube_add", WM_OP_EXEC_DEFAULT, &ptr, nullptr);
  WM_operator_properties_free(&ptr);
  return CTX_data_active_object(C);
}

void add_rigid_body(bContext *C, Object *ob, const char *type)
{
  select_only(C, {ob}, ob);
  PointerRNA ptr;
  WM_operator_properties_create(&ptr, "rigidbody.object_add");
  RNA_enum_set_identifier(C, &ptr, "type", type);
  WM_operator_name_call(C, "rigidbody.object_add", WM_OP_EXEC_DEFAULT, &ptr, nullptr);
  WM_operator_properties_free(&ptr);
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name Volcado
 * \{ */

const char *enum_id(const EnumPropertyItem *items, const int value)
{
  const char *id = "?";
  RNA_enum_id_from_value(items, value, &id);
  return id;
}

/* Un valor de propiedad RNA renderizado EXACTAMENTE como lo haria el guion Python:
 * los booleanos como 0/1, los flotantes con `%.9g`, los enums por identificador y
 * `collision_collections` como una cadena de veinte 0/1. */
void append_rna_value(char *out, const size_t out_size, PointerRNA *ptr, const char *identifier)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, identifier);
  if (prop == nullptr) {
    BLI_snprintf(out + strlen(out), out_size - strlen(out), " %s=?", identifier);
    return;
  }
  char *cursor = out + strlen(out);
  const size_t left = out_size - strlen(out);
  const int len = RNA_property_array_length(ptr, prop);

  switch (RNA_property_type(prop)) {
    case PROP_BOOLEAN: {
      if (len > 0) {
        bool values[RNA_MAX_ARRAY_LENGTH];
        RNA_property_boolean_get_array(ptr, prop, values);
        char bits[RNA_MAX_ARRAY_LENGTH + 1];
        for (int i = 0; i < len; i++) {
          bits[i] = values[i] ? '1' : '0';
        }
        bits[len] = '\0';
        BLI_snprintf(cursor, left, " %s=%s", identifier, bits);
      }
      else {
        BLI_snprintf(
            cursor, left, " %s=%d", identifier, RNA_property_boolean_get(ptr, prop) ? 1 : 0);
      }
      break;
    }
    case PROP_FLOAT:
      BLI_snprintf(
          cursor, left, " %s=%.9g", identifier, double(RNA_property_float_get(ptr, prop)));
      break;
    case PROP_INT:
      BLI_snprintf(cursor, left, " %s=%d", identifier, RNA_property_int_get(ptr, prop));
      break;
    case PROP_ENUM: {
      const int value = RNA_property_enum_get(ptr, prop);
      const EnumPropertyItem *items = nullptr;
      bool free_items = false;
      RNA_property_enum_items(nullptr, ptr, prop, &items, nullptr, &free_items);
      BLI_snprintf(cursor, left, " %s=%s", identifier, enum_id(items, value));
      if (free_items) {
        MEM_freeN(items);
      }
      break;
    }
    default:
      BLI_snprintf(cursor, left, " %s=?", identifier);
      break;
  }
}

void dump_case(bContext *C, FILE *f, const int index, const char *label)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  const Object *active = BKE_view_layer_active_object_get(view_layer);

  fprintf(f, "case=%d %s\n", index, label);
  int n = 0;

  LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
    const Base *base = BKE_view_layer_base_find(view_layer, ob);
    const int sel = (base != nullptr && (base->flag & BASE_SELECTED)) ? 1 : 0;
    fprintf(f,
            "  %d obj=%s type=%s sel=%d act=%d loc=%.9g,%.9g,%.9g empty=%s\n",
            n++,
            ob->id.name + 2,
            enum_id(rna_enum_object_type_items, ob->type),
            sel,
            (ob == active) ? 1 : 0,
            double(ob->loc[0]),
            double(ob->loc[1]),
            double(ob->loc[2]),
            (ob->type == OB_EMPTY) ?
                enum_id(rna_enum_object_empty_drawtype_items, ob->empty_drawtype) :
                "-");

    if (ob->rigidbody_object != nullptr) {
      char line[2048];
      BLI_snprintf(line, sizeof(line), "  %d rb=%s", n++, ob->id.name + 2);
      PointerRNA ptr = RNA_pointer_create_discrete(
          &ob->id, &RNA_RigidBodyObject, ob->rigidbody_object);
      for (const char *identifier : rb_props) {
        append_rna_value(line, sizeof(line), &ptr, identifier);
      }
      fprintf(f, "%s\n", line);
    }

    if (ob->rigidbody_constraint != nullptr) {
      const RigidBodyCon *rbc = ob->rigidbody_constraint;
      fprintf(f,
              "  %d rbc=%s type=%s ob1=%s ob2=%s\n",
              n++,
              ob->id.name + 2,
              enum_id(rna_enum_rigidbody_constraint_type_items, rbc->type),
              (rbc->ob1 != nullptr) ? rbc->ob1->id.name + 2 : "-",
              (rbc->ob2 != nullptr) ? rbc->ob2->id.name + 2 : "-");
    }
  }
}

/** \} */

}  // namespace

/* -------------------------------------------------------------------------- */
/** \name Bateria de casos
 * \{ */

bool dump(bContext *C, const char *filepath)
{
  FILE *f = fopen(filepath, "w");
  if (f == nullptr) {
    fprintf(stderr, "fl-selftest-rigidbody-ops: no se pudo escribir '%s'\n", filepath);
    return false;
  }
  fprintf(f, "# FL-RIGIDBODY-OPS-SELFTEST v1\n");

  int index = 0;

  /* --- rigidbody.object_settings_copy ---
   * Escena a proposito heterogenea: el activo con cuerpo rigido y ajustes raros, uno con
   * cuerpo rigido distinto (se sobrescribe), uno de malla sin cuerpo rigido (se lo pone
   * `rigidbody.objects_add`) y un objeto que no es malla (se deselecciona y se queda
   * fuera). */
  {
    purge(C);
    const float loc_a[3] = {0.0f, 0.0f, 0.0f};
    const float loc_b[3] = {3.0f, 0.0f, 0.0f};
    const float loc_c[3] = {6.0f, 0.0f, 0.0f};
    Object *a = add_cube(C, loc_a);
    Object *b = add_cube(C, loc_b);
    Object *c = add_cube(C, loc_c);
    {
      PointerRNA ptr;
      WM_operator_properties_create(&ptr, "object.empty_add");
      RNA_enum_set_identifier(C, &ptr, "type", "PLAIN_AXES");
      const float loc_d[3] = {9.0f, 0.0f, 0.0f};
      const float rot_d[3] = {0.0f, 0.0f, 0.0f};
      RNA_float_set_array(&ptr, "location", loc_d);
      RNA_float_set_array(&ptr, "rotation", rot_d);
      WM_operator_name_call(C, "object.empty_add", WM_OP_EXEC_DEFAULT, &ptr, nullptr);
      WM_operator_properties_free(&ptr);
    }
    Object *d = CTX_data_active_object(C);

    add_rigid_body(C, a, "ACTIVE");
    {
      PointerRNA ptr = RNA_pointer_create_discrete(
          &a->id, &RNA_RigidBodyObject, a->rigidbody_object);
      RNA_boolean_set(&ptr, "kinematic", true);
      RNA_float_set(&ptr, "mass", 7.25f);
      RNA_enum_set_identifier(C, &ptr, "collision_shape", "CAPSULE");
      RNA_boolean_set(&ptr, "use_margin", true);
      RNA_float_set(&ptr, "collision_margin", 0.123f);
      RNA_float_set(&ptr, "friction", 0.321f);
      RNA_float_set(&ptr, "restitution", 0.654f);
      RNA_boolean_set(&ptr, "use_deactivation", false);
      RNA_boolean_set(&ptr, "use_start_deactivated", true);
      RNA_float_set(&ptr, "deactivate_linear_velocity", 0.55f);
      RNA_float_set(&ptr, "deactivate_angular_velocity", 0.66f);
      RNA_float_set(&ptr, "linear_damping", 0.125f);
      RNA_float_set(&ptr, "angular_damping", 0.456f);
      bool cols[20];
      for (int i = 0; i < 20; i++) {
        cols[i] = (i % 3 == 0);
      }
      RNA_boolean_set_array(&ptr, "collision_collections", cols);
      RNA_enum_set_identifier(C, &ptr, "mesh_source", "FINAL");
      RNA_boolean_set(&ptr, "use_deform", true);
      RNA_boolean_set(&ptr, "enabled", false);
    }

    add_rigid_body(C, b, "PASSIVE");
    {
      PointerRNA ptr = RNA_pointer_create_discrete(
          &b->id, &RNA_RigidBodyObject, b->rigidbody_object);
      RNA_float_set(&ptr, "mass", 99.0f);
      RNA_float_set(&ptr, "friction", 0.05f);
    }

    select_only(C, {a, b, c, d}, a);
    call_op(C, "rigidbody.object_settings_copy", nullptr);
    dump_case(C, f, index++, "op=rigidbody.object_settings_copy mixto");
  }

  /* --- rigidbody.connect --- */
  struct ConnectCase {
    const char *label;
    const char *con_type;
    const char *pivot_type;
    const char *pattern;
  };
  const ConnectCase connect_cases[] = {
      {"selected_to_active center POINT", "POINT", "CENTER", "SELECTED_TO_ACTIVE"},
      {"selected_to_active active HINGE", "HINGE", "ACTIVE", "SELECTED_TO_ACTIVE"},
      {"chain_distance selected SLIDER", "SLIDER", "SELECTED", "CHAIN_DISTANCE"},
      {"chain_distance center GENERIC_SPRING",
       "GENERIC_SPRING",
       "CENTER",
       "CHAIN_DISTANCE"},
  };

  for (const ConnectCase &cc : connect_cases) {
    purge(C);
    const float locs[4][3] = {
        {0.0f, 0.0f, 0.0f}, {2.0f, 0.0f, 0.0f}, {0.0f, 3.0f, 0.0f}, {5.0f, 5.0f, 0.0f}};
    Vector<Object *> obs;
    for (const float(&loc)[3] : locs) {
      obs.append(add_cube(C, loc));
    }
    for (Object *ob : obs) {
      add_rigid_body(C, ob, "ACTIVE");
    }
    select_only(C, obs, obs[0]);

    PointerRNA ptr;
    WM_operator_properties_create(&ptr, "rigidbody.connect");
    RNA_enum_set_identifier(C, &ptr, "con_type", cc.con_type);
    RNA_enum_set_identifier(C, &ptr, "pivot_type", cc.pivot_type);
    RNA_enum_set_identifier(C, &ptr, "connection_pattern", cc.pattern);
    WM_operator_name_call(C, "rigidbody.connect", WM_OP_EXEC_DEFAULT, &ptr, nullptr);
    WM_operator_properties_free(&ptr);

    char label[256];
    BLI_snprintf(label, sizeof(label), "op=rigidbody.connect %s", cc.label);
    dump_case(C, f, index++, label);
  }

  fclose(f);
  fprintf(stderr, "fl-selftest-rigidbody-ops: volcado en '%s' (%d casos)\n", filepath, index);
  return true;
}

bool check(bContext *C, const char *baseline_path)
{
  char actual_path[FILE_MAX];
  BLI_path_join(actual_path,
                sizeof(actual_path),
                BKE_tempdir_session(),
                "fl-selftest-rigidbody-ops-actual.txt");
  if (!dump(C, actual_path)) {
    return false;
  }
  return flipendo::selftest::compare_to_baseline(
      "fl-check-rigidbody-ops", actual_path, baseline_path);
}

/** \} */

}  // namespace flipendo::rigidbody_ops_selftest
