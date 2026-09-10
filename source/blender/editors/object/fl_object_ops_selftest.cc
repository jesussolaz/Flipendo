/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 *
 * Implementacion de `--fl-selftest-object-ops`. Ver `FL_object_ops_selftest.hh`.
 */

#include <cstdio>

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_math_vector.h"
#include "BLI_vector.hh"

#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_types.hh"
#include "BKE_object.hh"
#include "BKE_scene.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "FL_object_ops_selftest.hh"

namespace flipendo::object_ops_selftest {

namespace {

/* Crea un cubo (mesh.primitive_cube_add) en la posicion/rotacion/tamano dados;
 * pasa por el operador de verdad (no BKE a pelo) para que la escena de prueba
 * dependa lo menos posible de detalles internos ajenos a lo que se esta probando. */
static Object *make_cube(bContext *C, const float loc[3], const float rot[3], float size)
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

static void set_selection(bContext *C, Object *active, const blender::Vector<Object *> &selected)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  BKE_view_layer_base_deselect_all(scene, view_layer);
  for (Object *ob : selected) {
    Base *base = BKE_view_layer_base_find(view_layer, ob);
    if (base) {
      base->flag |= BASE_SELECTED;
    }
  }
  Base *active_base = BKE_view_layer_base_find(view_layer, active);
  if (active_base) {
    BKE_view_layer_base_select_and_set_active(view_layer, active_base);
  }
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
}

static void dump_object_transform(FILE *f, const char *label, const Object *ob)
{
  fprintf(f,
          "  %-10s loc=(%.9g,%.9g,%.9g) rot=(%.9g,%.9g,%.9g) scale=(%.9g,%.9g,%.9g) "
          "dloc=(%.9g,%.9g,%.9g) drot=(%.9g,%.9g,%.9g) dscale=(%.9g,%.9g,%.9g)\n",
          label,
          double(ob->loc[0]),
          double(ob->loc[1]),
          double(ob->loc[2]),
          double(ob->rot[0]),
          double(ob->rot[1]),
          double(ob->rot[2]),
          double(ob->scale[0]),
          double(ob->scale[1]),
          double(ob->scale[2]),
          double(ob->dloc[0]),
          double(ob->dloc[1]),
          double(ob->dloc[2]),
          double(ob->drot[0]),
          double(ob->drot[1]),
          double(ob->drot[2]),
          double(ob->dscale[0]),
          double(ob->dscale[1]),
          double(ob->dscale[2]));
}

static void dump_mesh_stats(FILE *f, const Object *ob)
{
  const Mesh *me = static_cast<const Mesh *>(ob->data);
  double sum = 0.0, sumsq = 0.0;
  const blender::Span<blender::float3> positions = me->vert_positions();
  for (const blender::float3 &v : positions) {
    sum += double(v.x) + double(v.y) + double(v.z);
    sumsq += double(v.x) * v.x + double(v.y) * v.y + double(v.z) * v.z;
  }
  fprintf(f,
          "  mesh verts=%d edges=%d faces=%d loops=%d sum=%.9g sumsq=%.9g uvs=%d\n",
          me->verts_num,
          me->edges_num,
          me->faces_num,
          me->corners_num,
          sum,
          sumsq,
          me->attributes().contains("UVMap") ? 1 : 0);
}

}  // namespace

bool dump(bContext *C, const char *filepath)
{
  FILE *f = fopen(filepath, "w");
  if (!f) {
    fprintf(stderr, "fl-selftest-object-ops: no puedo escribir '%s'\n", filepath);
    return false;
  }

  fprintf(f, "# FL-OBJECT-OPS-SELFTEST v1\n");

  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);

  /* ---------------------------------------------------------------- */
  /* object.align                                                      */
  /* ---------------------------------------------------------------- */
  {
    const float loc_a[3] = {1.0f, 2.0f, 3.0f};
    const float rot_a[3] = {0.0f, 0.0f, 0.0f};
    const float loc_b[3] = {5.0f, -1.0f, 0.5f};
    const float rot_b[3] = {0.3f, 0.1f, 0.0f};
    const float loc_c[3] = {-3.0f, 4.0f, 1.0f};
    const float rot_c[3] = {0.0f, 0.7f, 0.2f};

    Object *ob_a = make_cube(C, loc_a, rot_a, 2.0f);
    Object *ob_b = make_cube(C, loc_b, rot_b, 1.0f);
    Object *ob_c = make_cube(C, loc_c, rot_c, 3.0f);

    struct AlignCombo {
      const char *align_mode;
      const char *relative_to;
      int align_x, align_y, align_z;
      bool bb_quality;
    };
    const AlignCombo combos[] = {
        {"OPT_2", "OPT_4", 1, 1, 1, true},
        {"OPT_1", "OPT_2", 1, 1, 0, true},
        {"OPT_3", "OPT_3", 0, 1, 1, false},
    };

    int combo_index = 0;
    for (const AlignCombo &combo : combos) {
      copy_v3_v3(ob_a->loc, loc_a);
      copy_v3_v3(ob_a->rot, rot_a);
      copy_v3_v3(ob_b->loc, loc_b);
      copy_v3_v3(ob_b->rot, rot_b);
      copy_v3_v3(ob_c->loc, loc_c);
      copy_v3_v3(ob_c->rot, rot_c);
      DEG_id_tag_update(&ob_a->id, ID_RECALC_TRANSFORM);
      DEG_id_tag_update(&ob_b->id, ID_RECALC_TRANSFORM);
      DEG_id_tag_update(&ob_c->id, ID_RECALC_TRANSFORM);
      BKE_scene_graph_update_tagged(depsgraph, bmain);

      set_selection(C, ob_a, {ob_a, ob_b, ob_c});

      PointerRNA ptr;
      WM_operator_properties_create(&ptr, "object.align");
      RNA_boolean_set(&ptr, "bb_quality", combo.bb_quality);
      RNA_enum_set_identifier(C, &ptr, "align_mode", combo.align_mode);
      RNA_enum_set_identifier(C, &ptr, "relative_to", combo.relative_to);
      int axis_flag = 0;
      if (combo.align_x) {
        axis_flag |= (1 << 0);
      }
      if (combo.align_y) {
        axis_flag |= (1 << 1);
      }
      if (combo.align_z) {
        axis_flag |= (1 << 2);
      }
      RNA_enum_set(&ptr, "align_axis", axis_flag);
      WM_operator_name_call(C, "object.align", WM_OP_EXEC_DEFAULT, &ptr, nullptr);
      WM_operator_properties_free(&ptr);

      BKE_scene_graph_update_tagged(depsgraph, bmain);

      fprintf(f,
              "object.align combo=%d mode=%s rel=%s axis=%d%d%d bbq=%d\n",
              combo_index,
              combo.align_mode,
              combo.relative_to,
              combo.align_x,
              combo.align_y,
              combo.align_z,
              int(combo.bb_quality));
      dump_object_transform(f, "A", ob_a);
      dump_object_transform(f, "B", ob_b);
      dump_object_transform(f, "C", ob_c);
      combo_index++;
    }

    void *data_a = ob_a->data, *data_b = ob_b->data, *data_c = ob_c->data;
    BKE_id_delete(bmain, ob_a);
    BKE_id_delete(bmain, ob_b);
    BKE_id_delete(bmain, ob_c);
    BKE_id_delete(bmain, data_a);
    BKE_id_delete(bmain, data_b);
    BKE_id_delete(bmain, data_c);
  }

  /* ---------------------------------------------------------------- */
  /* object.randomize_transform                                       */
  /* ---------------------------------------------------------------- */
  {
    const float loc0[3] = {0.0f, 0.0f, 0.0f};
    const float rot0[3] = {0.0f, 0.0f, 0.0f};

    struct RandCombo {
      int seed;
      bool delta;
      bool use_loc, use_rot, use_scale, scale_even;
      float loc_range[3], rot_range[3], scale_range[3];
    };
    const RandCombo combos[] = {
        {0, false, true, true, true, false, {1.0f, 1.0f, 1.0f}, {0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}},
        {42, false, true, false, true, true, {2.0f, 0.0f, 3.0f}, {0.0f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.5f}},
        {123, true, true, true, true, false, {0.5f, 0.5f, 0.5f}, {1.0f, 0.2f, 0.0f}, {0.2f, 0.2f, 0.2f}},
    };

    int combo_index = 0;
    for (const RandCombo &combo : combos) {
      Object *ob1 = make_cube(C, loc0, rot0, 2.0f);
      Object *ob2 = make_cube(C, loc0, rot0, 2.0f);
      Object *ob3 = make_cube(C, loc0, rot0, 2.0f);
      set_selection(C, ob1, {ob1, ob2, ob3});

      PointerRNA ptr;
      WM_operator_properties_create(&ptr, "object.randomize_transform");
      RNA_int_set(&ptr, "random_seed", combo.seed);
      RNA_boolean_set(&ptr, "use_delta", combo.delta);
      RNA_boolean_set(&ptr, "use_loc", combo.use_loc);
      RNA_float_set_array(&ptr, "loc", combo.loc_range);
      RNA_boolean_set(&ptr, "use_rot", combo.use_rot);
      RNA_float_set_array(&ptr, "rot", combo.rot_range);
      RNA_boolean_set(&ptr, "use_scale", combo.use_scale);
      RNA_boolean_set(&ptr, "scale_even", combo.scale_even);
      RNA_float_set_array(&ptr, "scale", combo.scale_range);
      WM_operator_name_call(C, "object.randomize_transform", WM_OP_EXEC_DEFAULT, &ptr, nullptr);
      WM_operator_properties_free(&ptr);

      fprintf(f,
              "object.randomize_transform combo=%d seed=%d delta=%d loc=%d rot=%d scale=%d "
              "even=%d\n",
              combo_index,
              combo.seed,
              int(combo.delta),
              int(combo.use_loc),
              int(combo.use_rot),
              int(combo.use_scale),
              int(combo.scale_even));
      dump_object_transform(f, "1", ob1);
      dump_object_transform(f, "2", ob2);
      dump_object_transform(f, "3", ob3);

      void *data1 = ob1->data, *data2 = ob2->data, *data3 = ob3->data;
      BKE_id_delete(bmain, ob1);
      BKE_id_delete(bmain, ob2);
      BKE_id_delete(bmain, ob3);
      BKE_id_delete(bmain, data1);
      BKE_id_delete(bmain, data2);
      BKE_id_delete(bmain, data3);
      combo_index++;
    }
  }

  /* ---------------------------------------------------------------- */
  /* mesh.primitive_torus_add                                         */
  /* ---------------------------------------------------------------- */
  {
    struct TorusCombo {
      int major_segments, minor_segments;
      const char *mode;
      float major_radius, minor_radius;
      float abso_major_rad, abso_minor_rad;
      bool generate_uvs;
    };
    const TorusCombo combos[] = {
        {48, 12, "MAJOR_MINOR", 1.0f, 0.25f, 1.25f, 0.75f, true},
        {8, 5, "MAJOR_MINOR", 2.5f, 0.75f, 3.25f, 1.75f, false},
        {16, 6, "EXT_INT", 1.0f, 0.25f, 4.0f, 1.0f, true},
    };

    int combo_index = 0;
    for (const TorusCombo &combo : combos) {
      PointerRNA ptr;
      WM_operator_properties_create(&ptr, "mesh.primitive_torus_add");
      RNA_int_set(&ptr, "major_segments", combo.major_segments);
      RNA_int_set(&ptr, "minor_segments", combo.minor_segments);
      RNA_enum_set_identifier(C, &ptr, "mode", combo.mode);
      RNA_float_set(&ptr, "major_radius", combo.major_radius);
      RNA_float_set(&ptr, "minor_radius", combo.minor_radius);
      RNA_float_set(&ptr, "abso_major_rad", combo.abso_major_rad);
      RNA_float_set(&ptr, "abso_minor_rad", combo.abso_minor_rad);
      RNA_boolean_set(&ptr, "generate_uvs", combo.generate_uvs);
      const float loc[3] = {0.0f, 0.0f, 0.0f};
      const float rot[3] = {0.0f, 0.0f, 0.0f};
      RNA_float_set_array(&ptr, "location", loc);
      RNA_float_set_array(&ptr, "rotation", rot);
      WM_operator_name_call(C, "mesh.primitive_torus_add", WM_OP_EXEC_DEFAULT, &ptr, nullptr);
      WM_operator_properties_free(&ptr);

      Object *ob = CTX_data_active_object(C);
      fprintf(f,
              "mesh.primitive_torus_add combo=%d major_seg=%d minor_seg=%d mode=%s uvs=%d\n",
              combo_index,
              combo.major_segments,
              combo.minor_segments,
              combo.mode,
              int(combo.generate_uvs));
      dump_mesh_stats(f, ob);

      void *ob_data = ob->data;
      BKE_id_delete(bmain, ob);
      BKE_id_delete(bmain, ob_data);
      combo_index++;
    }
  }

  fclose(f);
  fprintf(stderr, "fl-selftest-object-ops: volcado en '%s'\n", filepath);
  return true;
}

}  // namespace flipendo::object_ops_selftest
