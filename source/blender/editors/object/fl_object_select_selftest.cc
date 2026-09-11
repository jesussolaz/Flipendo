/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 *
 * Implementacion de `--fl-selftest-object-select`. Ver `FL_object_select_selftest.hh`.
 */

#include <cstdio>
#include <functional>

#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_collection_types.h"

#include "BKE_appdir.hh"
#include "BKE_collection.hh"
#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_object.hh"
#include "BKE_scene.hh"
#include "BKE_mesh.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "BLI_math_vector.h"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_object.hh"

#include "FL_selftest_compare.hh"

#include "FL_object_select_selftest.hh"

namespace flipendo::object_select_selftest {

using namespace blender;

namespace {

const char *enum_id(const EnumPropertyItem *items, const int value)
{
  const char *id = "?";
  RNA_enum_id_from_value(items, value, &id);
  return id;
}

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
  CTX_data_scene(C)->camera = nullptr;
  DEG_relations_tag_update(bmain);
}

void add_at(bContext *C, const char *idname, const float x, const std::function<void(PointerRNA *)> &fill)
{
  PointerRNA ptr;
  WM_operator_properties_create(&ptr, idname);
  const float loc[3] = {x, 0.0f, 0.0f};
  const float rot[3] = {0.0f, 0.0f, 0.0f};
  RNA_float_set_array(&ptr, "location", loc);
  RNA_float_set_array(&ptr, "rotation", rot);
  if (fill) {
    fill(&ptr);
  }
  WM_operator_name_call(C, idname, WM_OP_EXEC_DEFAULT, &ptr, nullptr);
  WM_operator_properties_free(&ptr);
}

/* La misma escena que construye el guion de captura: Cube, Cube.001, Sphere, Icosphere,
 * Plane, Empty, Camera y Point, en ese orden de creacion. Los nombres son los que pone
 * Blender por defecto, asi que salen iguales en los dos binarios sin renombrar nada. */
void build_scene(bContext *C)
{
  add_at(C, "mesh.primitive_cube_add", 0.0f, [](PointerRNA *p) { RNA_float_set(p, "size", 1.0f); });
  add_at(C, "mesh.primitive_cube_add", 2.0f, [](PointerRNA *p) { RNA_float_set(p, "size", 1.0f); });
  add_at(C, "mesh.primitive_uv_sphere_add", 4.0f, [](PointerRNA *p) {
    RNA_int_set(p, "segments", 8);
    RNA_int_set(p, "ring_count", 4);
    RNA_float_set(p, "radius", 1.0f);
  });
  add_at(C, "mesh.primitive_ico_sphere_add", 6.0f, [](PointerRNA *p) {
    RNA_int_set(p, "subdivisions", 1);
    RNA_float_set(p, "radius", 1.0f);
  });
  add_at(C, "mesh.primitive_plane_add", 8.0f, [](PointerRNA *p) { RNA_float_set(p, "size", 1.0f); });
  add_at(C, "object.empty_add", 10.0f, [&](PointerRNA *p) {
    RNA_enum_set_identifier(C, p, "type", "PLAIN_AXES");
  });
  add_at(C, "object.camera_add", 12.0f, nullptr);
  add_at(C, "object.light_add", 14.0f, [&](PointerRNA *p) {
    RNA_enum_set_identifier(C, p, "type", "POINT");
  });
}

Object *find(bContext *C, const char *name)
{
  Main *bmain = CTX_data_main(C);
  LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
    if (STREQ(ob->id.name + 2, name)) {
      return ob;
    }
  }
  return nullptr;
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

void set_active(bContext *C, Object *ob)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  view_layer->basact = (ob != nullptr) ? BKE_view_layer_base_find(view_layer, ob) : nullptr;
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
}

/* `ob.hide_set(True)`: pone BASE_HIDDEN y resincroniza la capa de vista. */
void hide_set(bContext *C, Object *ob)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  if (Base *base = BKE_view_layer_base_find(view_layer, ob)) {
    base->flag |= BASE_HIDDEN;
  }
  BKE_layer_collection_sync(scene, view_layer);
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
}

void deselect_all(bContext *C)
{
  call_op(C, "object.select_all", [&](PointerRNA *p) {
    RNA_enum_set_identifier(C, p, "action", "DESELECT");
  });
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
    const int hidden = (base != nullptr && (base->flag & BASE_HIDDEN)) ? 1 : 0;
    fprintf(f,
            "  %d obj=%s sel=%d act=%d hide=%d parent=%s\n",
            n++,
            ob->id.name + 2,
            sel,
            (ob == active) ? 1 : 0,
            hidden,
            (ob->parent != nullptr) ? ob->parent->id.name + 2 : "-");
  }
}

}  // namespace

bool dump(bContext *C, const char *filepath)
{
  FILE *f = fopen(filepath, "w");
  if (f == nullptr) {
    fprintf(stderr, "fl-selftest-object-select: no se pudo escribir '%s'\n", filepath);
    return false;
  }
  fprintf(f, "# FL-OBJECT-SELECT-SELFTEST v1\n");
  int index = 0;

  /* --- object.select_pattern --- */
  struct PatternCase {
    const char *pattern;
    bool case_sensitive;
    bool extend;
  };
  const PatternCase pattern_cases[] = {
      {"Cube*", false, false},
      {"Cube*", false, true},
      {"*phere", false, false},
      {"?lane", false, false},
      {"[CP]*", false, false},
      {"cube*", true, false},
      {"cube*", false, false},
      {"*.001", false, false},
      {"Camera", false, false},
      {"*", false, false},
  };
  for (const PatternCase &pc : pattern_cases) {
    purge(C);
    build_scene(C);
    hide_set(C, find(C, "Plane")); /* uno oculto: solo se miran los objetos visibles. */
    deselect_all(C);
    select_set(C, find(C, "Icosphere"), true);
    set_active(C, find(C, "Icosphere"));

    PointerRNA ptr;
    WM_operator_properties_create(&ptr, "object.select_pattern");
    RNA_string_set(&ptr, "pattern", pc.pattern);
    RNA_boolean_set(&ptr, "case_sensitive", pc.case_sensitive);
    RNA_boolean_set(&ptr, "extend", pc.extend);
    WM_operator_name_call(C, "object.select_pattern", WM_OP_EXEC_DEFAULT, &ptr, nullptr);
    WM_operator_properties_free(&ptr);

    char label[256];
    BLI_snprintf(label,
                 sizeof(label),
                 "op=object.select_pattern pattern=%s cs=%d ext=%d",
                 pc.pattern,
                 int(pc.case_sensitive),
                 int(pc.extend));
    dump_case(C, f, index++, label);
  }

  /* --- object.select_camera --- */
  struct CameraCase {
    bool scene_cam;
    bool extend;
  };
  const CameraCase camera_cases[] = {{true, false}, {true, true}, {false, false}};
  for (const CameraCase &cc : camera_cases) {
    purge(C);
    build_scene(C);
    if (cc.scene_cam) {
      CTX_data_scene(C)->camera = find(C, "Camera");
    }
    deselect_all(C);
    select_set(C, find(C, "Cube"), true);
    set_active(C, find(C, "Cube"));

    PointerRNA ptr;
    WM_operator_properties_create(&ptr, "object.select_camera");
    RNA_boolean_set(&ptr, "extend", cc.extend);
    WM_operator_name_call(C, "object.select_camera", WM_OP_EXEC_DEFAULT, &ptr, nullptr);
    WM_operator_properties_free(&ptr);

    char label[256];
    BLI_snprintf(label,
                 sizeof(label),
                 "op=object.select_camera scene_cam=%d ext=%d",
                 int(cc.scene_cam),
                 int(cc.extend));
    dump_case(C, f, index++, label);
  }

  /* --- object.select_hierarchy --- */
  struct HierarchyCase {
    const char *direction;
    bool extend;
    const char *start;
  };
  const HierarchyCase hierarchy_cases[] = {
      {"PARENT", false, "Sphere"},
      {"PARENT", true, "Sphere"},
      {"CHILD", false, "Cube"},
      {"CHILD", true, "Cube"},
      {"PARENT", false, "Cube"},
      {"CHILD", false, "Plane"},
  };
  for (const HierarchyCase &hc : hierarchy_cases) {
    purge(C);
    build_scene(C);
    /* Jerarquia: Cube -> (Cube.001, Sphere); Cube.001 -> Icosphere. */
    const char *pairs[3][2] = {
        {"Cube.001", "Cube"}, {"Sphere", "Cube"}, {"Icosphere", "Cube.001"}};
    for (const auto &pair : pairs) {
      deselect_all(C);
      select_set(C, find(C, pair[0]), true);
      select_set(C, find(C, pair[1]), true);
      set_active(C, find(C, pair[1]));
      call_op(C, "object.parent_set", [&](PointerRNA *p) {
        RNA_enum_set_identifier(C, p, "type", "OBJECT");
        RNA_boolean_set(p, "keep_transform", false);
      });
    }
    deselect_all(C);
    select_set(C, find(C, hc.start), true);
    set_active(C, find(C, hc.start));

    PointerRNA ptr;
    WM_operator_properties_create(&ptr, "object.select_hierarchy");
    RNA_enum_set_identifier(C, &ptr, "direction", hc.direction);
    RNA_boolean_set(&ptr, "extend", hc.extend);
    const wmOperatorStatus status = WM_operator_name_call(
        C, "object.select_hierarchy", WM_OP_EXEC_DEFAULT, &ptr, nullptr);
    WM_operator_properties_free(&ptr);
    fprintf(f,
            "# select_hierarchy ret=['%s']\n",
            (status & OPERATOR_FINISHED) ? "FINISHED" : "CANCELLED");

    char label[256];
    BLI_snprintf(label,
                 sizeof(label),
                 "op=object.select_hierarchy dir=%s ext=%d start=%s",
                 hc.direction,
                 int(hc.extend),
                 hc.start);
    dump_case(C, f, index++, label);
  }

  fclose(f);
  fprintf(stderr, "fl-selftest-object-select: volcado en '%s' (%d casos)\n", filepath, index);
  return true;
}

bool check(bContext *C, const char *baseline_path)
{
  char actual_path[FILE_MAX];
  BLI_path_join(actual_path,
                sizeof(actual_path),
                BKE_tempdir_session(),
                "fl-selftest-object-select-actual.txt");
  if (!dump(C, actual_path)) {
    return false;
  }
  return flipendo::selftest::compare_to_baseline(
      "fl-check-object-select", actual_path, baseline_path);
}

/* -------------------------------------------------------------------------- */
/** \name object.make_dupli_face
 * \{ */

namespace {

void select_only(bContext *C, const Span<Object *> objects, Object *active)
{
  deselect_all(C);
  for (Object *ob : objects) {
    select_set(C, ob, true);
  }
  set_active(C, active);
}

const char *instance_type_name(const Object *ob)
{
  if (ob->transflag & OB_DUPLIFACES) {
    return "FACES";
  }
  if (ob->transflag & OB_DUPLIVERTS) {
    return "VERTS";
  }
  if (ob->transflag & OB_DUPLICOLLECTION) {
    return "COLLECTION";
  }
  return "NONE";
}

void dump_dupliface_case(bContext *C, FILE *f, const int index, const char *label)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);

  fprintf(f, "case=%d %s\n", index, label);
  int n = 0;
  LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
    const Base *base = BKE_view_layer_base_find(view_layer, ob);
    fprintf(f,
            "  %d obj=%s type=%s sel=%d parent=%s inst=%s ifs=%d scale=%.9g\n",
            n++,
            ob->id.name + 2,
            enum_id(rna_enum_object_type_items, ob->type),
            (base != nullptr && (base->flag & BASE_SELECTED)) ? 1 : 0,
            (ob->parent != nullptr) ? ob->parent->id.name + 2 : "-",
            instance_type_name(ob),
            (ob->transflag & OB_DUPLIFACES_SCALE) ? 1 : 0,
            double(ob->instance_faces_scale));
  }
  LISTBASE_FOREACH (Mesh *, mesh, &bmain->meshes) {
    double sum = 0.0;
    for (const float3 &co : mesh->vert_positions()) {
      sum += double(co.x) * 3.0 + double(co.y) * 5.0 + double(co.z) * 7.0;
    }
    fprintf(f,
            "  %d mesh=%s verts=%d edges=%d faces=%d loops=%d users=%d sum=%.9g\n",
            n++,
            mesh->id.name + 2,
            mesh->verts_num,
            mesh->edges_num,
            mesh->faces_num,
            mesh->corners_num,
            mesh->id.us,
            sum);
  }
}

Object *dupliface_add_cube(bContext *C, const float loc[3], const float rot[3], const float size)
{
  PointerRNA ptr;
  WM_operator_properties_create(&ptr, "mesh.primitive_cube_add");
  RNA_float_set(&ptr, "size", size);
  RNA_float_set_array(&ptr, "location", loc);
  RNA_float_set_array(&ptr, "rotation", rot);
  WM_operator_name_call(C, "mesh.primitive_cube_add", WM_OP_EXEC_DEFAULT, &ptr, nullptr);
  WM_operator_properties_free(&ptr);
  return CTX_data_active_object(C);
}

void dupliface_run(bContext *C, FILE *f, int *index, const char *label)
{
  const wmOperatorStatus status = WM_operator_name_call(
      C, "object.make_dupli_face", WM_OP_EXEC_DEFAULT, nullptr, nullptr);
  fprintf(f, "# ret=['%s']\n", (status & OPERATOR_FINISHED) ? "FINISHED" : "CANCELLED");
  dump_dupliface_case(C, f, (*index)++, label);
}

}  // namespace

bool dump_dupli_face(bContext *C, const char *filepath)
{
  FILE *f = fopen(filepath, "w");
  if (f == nullptr) {
    fprintf(stderr, "fl-selftest-dupli-face: no se pudo escribir '%s'\n", filepath);
    return false;
  }
  fprintf(f, "# FL-DUPLIFACE-SELFTEST v1\n");
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  int index = 0;

  /* Tres cubos con datos DISTINTOS: tres grupos, tres mallas de instancia. */
  {
    purge(C);
    const float r0[3] = {0.0f, 0.0f, 0.0f};
    const float l0[3] = {0.0f, 0.0f, 0.0f}, l1[3] = {3.0f, 1.0f, 0.0f},
                l2[3] = {-2.0f, 4.0f, 1.0f};
    const float r1[3] = {0.3f, 0.2f, 0.1f}, r2[3] = {0.0f, 0.7f, 0.0f};
    Object *a = dupliface_add_cube(C, l0, r0, 1.0f);
    Object *b = dupliface_add_cube(C, l1, r1, 2.0f);
    Object *c = dupliface_add_cube(C, l2, r2, 0.5f);
    select_only(C, {a, b, c}, a);
    dupliface_run(C, f, &index, "op=object.make_dupli_face tres-datos-distintos");
  }

  /* Tres objetos que COMPARTEN malla: un solo grupo con tres caras. */
  {
    purge(C);
    const float r0[3] = {0.0f, 0.0f, 0.0f};
    const float l0[3] = {0.0f, 0.0f, 0.0f};
    Object *a = dupliface_add_cube(C, l0, r0, 1.0f);
    Object *shared[2];
    const char *names[2] = {"Shared1", "Shared2"};
    const float locs[2][3] = {{2.0f, 0.0f, 0.0f}, {0.0f, 3.0f, 1.0f}};
    for (int i = 0; i < 2; i++) {
      Object *ob = BKE_object_add_only_object(bmain, OB_MESH, names[i]);
      ob->data = a->data;
      id_us_plus(static_cast<ID *>(a->data));
      BKE_collection_object_add(bmain, scene->master_collection, ob);
      copy_v3_v3(ob->loc, locs[i]);
      shared[i] = ob;
    }
    DEG_relations_tag_update(bmain);
    BKE_scene_graph_update_tagged(CTX_data_ensure_evaluated_depsgraph(C), bmain);
    select_only(C, {a, shared[0], shared[1]}, a);
    dupliface_run(C, f, &index, "op=object.make_dupli_face malla-compartida");
  }

  /* Un solo objeto. */
  {
    purge(C);
    const float loc[3] = {1.0f, 2.0f, 3.0f};
    const float rot[3] = {0.1f, 0.2f, 0.3f};
    Object *a = dupliface_add_cube(C, loc, rot, 1.5f);
    select_only(C, {a}, a);
    dupliface_run(C, f, &index, "op=object.make_dupli_face uno-solo");
  }

  /* Nada seleccionado que valga: una camara. */
  {
    purge(C);
    PointerRNA ptr;
    WM_operator_properties_create(&ptr, "object.camera_add");
    const float loc[3] = {0.0f, 0.0f, 0.0f};
    const float rot[3] = {0.0f, 0.0f, 0.0f};
    RNA_float_set_array(&ptr, "location", loc);
    RNA_float_set_array(&ptr, "rotation", rot);
    WM_operator_name_call(C, "object.camera_add", WM_OP_EXEC_DEFAULT, &ptr, nullptr);
    WM_operator_properties_free(&ptr);
    Object *cam = CTX_data_active_object(C);
    select_only(C, {cam}, cam);
    dupliface_run(C, f, &index, "op=object.make_dupli_face sin-candidatos");
  }

  fclose(f);
  fprintf(stderr, "fl-selftest-dupli-face: volcado en '%s' (%d casos)\n", filepath, index);
  return true;
}

bool check_dupli_face(bContext *C, const char *baseline_path)
{
  char actual_path[FILE_MAX];
  BLI_path_join(
      actual_path, sizeof(actual_path), BKE_tempdir_session(), "fl-selftest-dupli-face-actual.txt");
  if (!dump_dupli_face(C, actual_path)) {
    return false;
  }
  return flipendo::selftest::compare_to_baseline(
      "fl-check-dupli-face", actual_path, baseline_path);
}

/** \} */

}  // namespace flipendo::object_select_selftest
