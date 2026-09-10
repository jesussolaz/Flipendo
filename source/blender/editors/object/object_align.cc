/* SPDX-FileCopyrightText: 2010-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 *
 * Migracion de `scripts/startup/bl_operators/object_align.py` (Carril C, Flipendo).
 * Mismo idname `object.align`, mismas propiedades (`bb_quality`, `align_mode`,
 * `relative_to`, `align_axis`), mismo algoritmo de calculo de cajas englobantes.
 */

#include <limits>

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_math_vector.h"

#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_mesh.hh"
#include "BKE_object.hh"
#include "BKE_report.hh"
#include "BKE_scene.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_object.hh"
#include "ED_screen.hh"

#include "object_intern.hh"

namespace blender::ed::object {

/* Misma numeracion de cadenas que el Python original (OPT_1..OPT_4): otros
 * scripts/.blend pueden referirse a ellas por su identificador RNA. */
enum {
  ALIGN_MODE_NEGATIVE = 1, /* OPT_1 */
  ALIGN_MODE_CENTERS = 2,  /* OPT_2 */
  ALIGN_MODE_POSITIVE = 3, /* OPT_3 */
};

enum {
  ALIGN_RELATIVE_SCENE = 1,    /* OPT_1 */
  ALIGN_RELATIVE_CURSOR = 2,   /* OPT_2 */
  ALIGN_RELATIVE_SELECTION = 3, /* OPT_3 */
  ALIGN_RELATIVE_ACTIVE = 4,   /* OPT_4 */
};

struct AlignBounds {
  float3 left_front_up;
  float3 right_back_down;
};

/* Equivalente a `worldspace_bounds_from_object_bounds`: min/max de las 8 esquinas
 * del bound_box (ya transformadas a espacio de mundo).
 *
 * Trampa del Python original (preservada aqui a proposito): en Z, `left_front_up`
 * guarda el MAXIMO ("up") y `right_back_down` el MINIMO ("down") -- al reves que en
 * X/Y. Si no se respeta este cruce, `align_mode`/`positive`/`negative` dan resultados
 * distintos en el eje Z (se detecto exactamente asi al comparar contra la linea base
 * de Python: ver commit e informe C1). */
static AlignBounds bounds_from_world_corners(const Span<float3> bb_world)
{
  AlignBounds b;
  b.left_front_up = bb_world[7];
  b.right_back_down = bb_world[7];
  for (const int i : IndexRange(7)) {
    const float3 &v = bb_world[i];
    if (v.x < b.left_front_up.x) {
      b.left_front_up.x = v.x;
    }
    if (v.x > b.right_back_down.x) {
      b.right_back_down.x = v.x;
    }
    if (v.y < b.left_front_up.y) {
      b.left_front_up.y = v.y;
    }
    if (v.y > b.right_back_down.y) {
      b.right_back_down.y = v.y;
    }
    /* Eje Z invertido a proposito: left_front_up.z = maximo, right_back_down.z =
     * minimo (ver comentario de la funcion). */
    if (v.z > b.left_front_up.z) {
      b.left_front_up.z = v.z;
    }
    if (v.z < b.right_back_down.z) {
      b.right_back_down.z = v.z;
    }
  }
  return b;
}

/* Equivalente a `worldspace_bounds_from_object_data`: recorre los vertices de la
 * malla evaluada (con modificadores) en vez del bound_box, para resultados
 * exactos con mallas rotadas/escaladas ("High Quality"). Mismo cruce de Z que
 * `bounds_from_world_corners` (ver comentario alli). */
static AlignBounds bounds_from_evaluated_mesh(Depsgraph *depsgraph, Object *ob)
{
  const float4x4 matrix_world = ob->object_to_world();

  const Object *ob_eval = DEG_get_evaluated(depsgraph, ob);
  const Mesh *me_eval = BKE_object_get_evaluated_mesh(ob_eval);

  AlignBounds b;
  if (!me_eval || me_eval->verts_num == 0) {
    b.left_front_up = b.right_back_down = math::transform_point(matrix_world, float3(0.0f));
    return b;
  }

  const Span<float3> positions = me_eval->vert_positions();
  b.left_front_up = b.right_back_down = math::transform_point(matrix_world,
                                                               positions.last());
  for (const float3 &co : positions) {
    const float3 v = math::transform_point(matrix_world, co);
    if (v.x < b.left_front_up.x) {
      b.left_front_up.x = v.x;
    }
    if (v.x > b.right_back_down.x) {
      b.right_back_down.x = v.x;
    }
    if (v.y < b.left_front_up.y) {
      b.left_front_up.y = v.y;
    }
    if (v.y > b.right_back_down.y) {
      b.right_back_down.y = v.y;
    }
    if (v.z > b.left_front_up.z) {
      b.left_front_up.z = v.z;
    }
    if (v.z < b.right_back_down.z) {
      b.right_back_down.z = v.z;
    }
  }
  return b;
}

static void world_bound_box_corners(Object *ob, float3 r_corners[8])
{
  const float4x4 matrix_world = ob->object_to_world();
  const std::optional<Bounds<float3>> bounds = BKE_object_boundbox_get(ob);
  const float3 min = bounds ? bounds->min : float3(0.0f);
  const float3 max = bounds ? bounds->max : float3(0.0f);
  /* Mismo orden que BKE_boundbox_init_from_minmax / Object.bound_box de Python:
   * no importa para este calculo, que solo busca el minimo y el maximo. */
  const float3 local_corners[8] = {
      {min.x, min.y, min.z},
      {min.x, min.y, max.z},
      {min.x, max.y, max.z},
      {min.x, max.y, min.z},
      {max.x, min.y, min.z},
      {max.x, min.y, max.z},
      {max.x, max.y, max.z},
      {max.x, max.y, min.z},
  };
  for (const int i : IndexRange(8)) {
    r_corners[i] = math::transform_point(matrix_world, local_corners[i]);
  }
}

static AlignBounds bounds_for_object(bContext *C,
                                     Depsgraph *depsgraph,
                                     Object *ob,
                                     const bool bb_quality)
{
  if (bb_quality && ob->type == OB_MESH) {
    return bounds_from_evaluated_mesh(depsgraph, ob);
  }
  float3 corners[8];
  world_bound_box_corners(ob, corners);
  return bounds_from_world_corners(Span<float3>(corners, 8));
}

static bool align_objects_exec_impl(bContext *C,
                                    const bool align_x,
                                    const bool align_y,
                                    const bool align_z,
                                    const int align_mode,
                                    const int relative_to,
                                    const bool bb_quality)
{
  Depsgraph *depsgraph = CTX_data_ensure_evaluated_depsgraph(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  Object *active_object = CTX_data_active_object(C);

  const float3 cursor = scene->cursor.location;

  /* El bounding-box evaluado tiene que estar actualizado (igual que
   * `context.view_layer.update()` en Python). */
  BKE_scene_graph_update_tagged(depsgraph, CTX_data_main(C));

  Vector<Object *> objects;
  CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
    objects.append(ob);
  }
  CTX_DATA_END;

  if (objects.is_empty()) {
    return false;
  }

  float3 left_front_up_sel(0.0f), right_back_down_sel(0.0f);
  float3 center_active(0.0f), size_active(0.0f);
  bool first = true;

  for (Object *ob : objects) {
    const AlignBounds b = bounds_for_object(C, depsgraph, ob, bb_quality);

    if (ob == active_object) {
      center_active = (b.left_front_up + b.right_back_down) * 0.5f;
      size_active.x = (b.right_back_down.x - b.left_front_up.x) * 0.5f;
      size_active.y = (b.right_back_down.y - b.left_front_up.y) * 0.5f;
      size_active.z = (b.left_front_up.z - b.right_back_down.z) * 0.5f;
    }

    if (first) {
      first = false;
      left_front_up_sel = b.left_front_up;
      right_back_down_sel = b.right_back_down;
    }
    else {
      if (b.left_front_up.x < left_front_up_sel.x) {
        left_front_up_sel.x = b.left_front_up.x;
      }
      if (b.left_front_up.y < left_front_up_sel.y) {
        left_front_up_sel.y = b.left_front_up.y;
      }
      if (b.left_front_up.z > left_front_up_sel.z) {
        left_front_up_sel.z = b.left_front_up.z;
      }
      if (b.right_back_down.x > right_back_down_sel.x) {
        right_back_down_sel.x = b.right_back_down.x;
      }
      if (b.right_back_down.y > right_back_down_sel.y) {
        right_back_down_sel.y = b.right_back_down.y;
      }
      if (b.right_back_down.z < right_back_down_sel.z) {
        right_back_down_sel.z = b.right_back_down.z;
      }
    }
  }

  const float3 center_sel = (left_front_up_sel + right_back_down_sel) * 0.5f;

  for (Object *ob : objects) {
    const AlignBounds b = bounds_for_object(C, depsgraph, ob, bb_quality);

    const float3 center = (b.left_front_up + b.right_back_down) * 0.5f;

    const float3 positive(b.right_back_down.x, b.right_back_down.y, b.left_front_up.z);
    const float3 negative(b.left_front_up.x, b.left_front_up.y, b.right_back_down.z);

    const float3 obj_loc = ob->loc;

    for (int axis = 0; axis < 3; axis++) {
      const bool do_axis = (axis == 0) ? align_x : (axis == 1) ? align_y : align_z;
      if (!do_axis) {
        continue;
      }

      float obj_val = 0.0f;
      const bool active_relative = (relative_to == ALIGN_RELATIVE_ACTIVE);

      if (active_relative) {
        if (align_mode == ALIGN_MODE_NEGATIVE) {
          obj_val = obj_loc[axis] - negative[axis] - size_active[axis];
        }
        else if (align_mode == ALIGN_MODE_POSITIVE) {
          obj_val = obj_loc[axis] - positive[axis] + size_active[axis];
        }
      }
      else {
        if (align_mode == ALIGN_MODE_NEGATIVE) {
          obj_val = obj_loc[axis] - negative[axis];
        }
        else if (align_mode == ALIGN_MODE_POSITIVE) {
          obj_val = obj_loc[axis] - positive[axis];
        }
      }
      if (align_mode == ALIGN_MODE_CENTERS) {
        obj_val = obj_loc[axis] - center[axis];
      }

      float loc_val = obj_val;
      if (relative_to == ALIGN_RELATIVE_CURSOR) {
        loc_val = obj_val + cursor[axis];
      }
      else if (relative_to == ALIGN_RELATIVE_SELECTION) {
        loc_val = obj_val + center_sel[axis];
      }
      else if (relative_to == ALIGN_RELATIVE_ACTIVE) {
        loc_val = obj_val + center_active[axis];
      }

      ob->loc[axis] = loc_val;
    }

    DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, nullptr);

  return true;
}

static wmOperatorStatus align_objects_exec(bContext *C, wmOperator *op)
{
  const bool bb_quality = RNA_boolean_get(op->ptr, "bb_quality");
  const int align_mode = RNA_enum_get(op->ptr, "align_mode");
  const int relative_to = RNA_enum_get(op->ptr, "relative_to");
  const int align_axis = RNA_enum_get(op->ptr, "align_axis");

  const bool ok = align_objects_exec_impl(C,
                                          (align_axis & (1 << 0)) != 0,
                                          (align_axis & (1 << 1)) != 0,
                                          (align_axis & (1 << 2)) != 0,
                                          align_mode,
                                          relative_to,
                                          bb_quality);

  if (!ok) {
    BKE_report(op->reports, RPT_WARNING, "No objects with bound-box selected");
    return OPERATOR_CANCELLED;
  }
  return OPERATOR_FINISHED;
}

static bool align_objects_poll(bContext *C)
{
  return CTX_data_mode_enum(C) == CTX_MODE_OBJECT;
}

void OBJECT_OT_align(wmOperatorType *ot)
{
  static const EnumPropertyItem prop_align_mode_items[] = {
      {ALIGN_MODE_NEGATIVE, "OPT_1", 0, "Negative Sides", ""},
      {ALIGN_MODE_CENTERS, "OPT_2", 0, "Centers", ""},
      {ALIGN_MODE_POSITIVE, "OPT_3", 0, "Positive Sides", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };
  static const EnumPropertyItem prop_relative_to_items[] = {
      {ALIGN_RELATIVE_SCENE,
       "OPT_1",
       0,
       "Scene Origin",
       "Use the scene origin as the position for the selected objects to align to"},
      {ALIGN_RELATIVE_CURSOR,
       "OPT_2",
       0,
       "3D Cursor",
       "Use the 3D cursor as the position for the selected objects to align to"},
      {ALIGN_RELATIVE_SELECTION,
       "OPT_3",
       0,
       "Selection",
       "Use the selected objects as the position for the selected objects to align to"},
      {ALIGN_RELATIVE_ACTIVE,
       "OPT_4",
       0,
       "Active",
       "Use the active object as the position for the selected objects to align to"},
      {0, nullptr, 0, nullptr, nullptr},
  };
  static const EnumPropertyItem prop_align_axis_items[] = {
      {1 << 0, "X", 0, "X", ""},
      {1 << 1, "Y", 0, "Y", ""},
      {1 << 2, "Z", 0, "Z", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Align Objects";
  ot->description = "Align objects";
  ot->idname = "OBJECT_OT_align";

  /* API callbacks. */
  ot->exec = align_objects_exec;
  ot->poll = align_objects_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  PropertyRNA *prop;
  prop = RNA_def_boolean(ot->srna,
                         "bb_quality",
                         true,
                         "High Quality",
                         "Enables high quality but slow calculation of the bounding box for "
                         "perfect results on complex shape meshes with rotation/scale");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  RNA_def_enum(ot->srna,
              "align_mode",
              prop_align_mode_items,
              ALIGN_MODE_CENTERS,
              "Align Mode",
              "Side of object to use for alignment");
  RNA_def_enum(ot->srna,
              "relative_to",
              prop_relative_to_items,
              ALIGN_RELATIVE_ACTIVE,
              "Relative To",
              "Reference location to align to");
  prop = RNA_def_enum_flag(
      ot->srna, "align_axis", prop_align_axis_items, 0, "Align", "Align to axis");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

}  // namespace blender::ed::object
