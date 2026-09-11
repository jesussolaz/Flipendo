/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 *
 * Puerto nativo en C++ de los cuatro operadores de extrusion de
 * `scripts/startup/bl_operators/view3d.py` (Flipendo carril C):
 *
 *   view3d.edit_mesh_extrude_individual_move -> VIEW3D_OT_edit_mesh_extrude_individual_move
 *   view3d.edit_mesh_extrude_move_normal     -> VIEW3D_OT_edit_mesh_extrude_move
 *   view3d.edit_mesh_extrude_move_shrink_fatten -> VIEW3D_OT_edit_mesh_extrude_move_shrink_fatten
 *   view3d.edit_mesh_extrude_manifold_normal -> VIEW3D_OT_edit_mesh_extrude_manifold_normal
 *
 * No extruyen nada por si mismos: miran el modo de seleccion y cuanto hay seleccionado y
 * despachan a la macro `MESH_OT_extrude_*` que toque, en modo INVOKE_REGION_WIN para que
 * el transform arranque modal bajo el raton. Por eso viven en `editors/mesh` y no en
 * `editors/space_view3d`: lo unico que hacen es elegir entre macros que se definen aqui
 * al lado, en `ED_operatormacros_mesh()` de `mesh_ops.cc`. (El idname sigue siendo
 * `view3d.*`, sin tocar: hay atajos de teclado y menus que dependen de el.)
 */

#include <functional>

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.hh"
#include "BKE_editmesh.hh"
#include "BKE_mesh_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_object.hh"

#include "mesh_intern.hh" /* own include */

/* -------------------------------------------------------------------------- */
/** \name Comun
 * \{ */

/* `context.mode == 'EDIT_MESH'`, literal. */
static bool view3d_edit_mesh_extrude_poll(bContext *C)
{
  return CTX_data_mode_enum(C) == CTX_MODE_EDIT_MESH;
}

/* `mesh.total_face_sel` / `mesh.total_edge_sel`, que en RNA son los contadores del
 * BMesh en edicion (0 si no hay malla en edicion). */
static void edit_mesh_selection_counts(const Object *ob, int *r_totface, int *r_totedge)
{
  *r_totface = 0;
  *r_totedge = 0;
  if (ob == nullptr || ob->type != OB_MESH) {
    return;
  }
  const Mesh *mesh = static_cast<const Mesh *>(ob->data);
  if (mesh->runtime == nullptr) {
    return;
  }
  if (const BMEditMesh *em = mesh->runtime->edit_mesh.get()) {
    *r_totface = em->bm->totfacesel;
    *r_totedge = em->bm->totedgesel;
  }
}

/* Llama a una macro `MESH_OT_extrude_*` en INVOKE_REGION_WIN, dejando que `fill` ponga
 * las propiedades de los sub-operadores. Se devuelve siempre FINISHED sin mirar lo que
 * conteste la macro: la macro se queda en RUNNING_MODAL y, si se propagara, este
 * operador no llegaria a liberarse (el mismo motivo que comentaba el Python, #24671). */
static void extrude_macro_invoke(bContext *C,
                                 const char *idname,
                                 const std::function<void(PointerRNA *)> &fill)
{
  PointerRNA ptr;
  WM_operator_properties_create(&ptr, idname);
  if (fill) {
    fill(&ptr);
  }
  WM_operator_name_call(C, idname, WM_OP_INVOKE_REGION_WIN, &ptr, nullptr);
  WM_operator_properties_free(&ptr);
}

static void translate_props_set(bContext *C,
                                PointerRNA *macro_ptr,
                                const char *orient_type,
                                const bool *constraint_axis)
{
  PointerRNA sub = RNA_pointer_get(macro_ptr, "TRANSFORM_OT_translate");
  if (orient_type != nullptr) {
    RNA_enum_set_identifier(C, &sub, "orient_type", orient_type);
  }
  if (constraint_axis != nullptr) {
    RNA_boolean_set_array(&sub, "constraint_axis", constraint_axis);
  }
  RNA_boolean_set(&sub, "release_confirm", false);
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name view3d.edit_mesh_extrude_individual_move
 * \{ */

static wmOperatorStatus edit_mesh_extrude_individual_move_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  if (blender::ed::object::shape_key_report_if_active_locked(ob, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  const Scene *scene = CTX_data_scene(C);
  const short selectmode = scene->toolsettings->selectmode;
  int totface, totedge;
  edit_mesh_selection_counts(ob, &totface, &totedge);

  const bool axis_z[3] = {false, false, true};

  if ((selectmode & SCE_SELECT_FACE) && totface == 1) {
    extrude_macro_invoke(C, "MESH_OT_extrude_region_move", [&](PointerRNA *ptr) {
      translate_props_set(C, ptr, "NORMAL", axis_z);
    });
  }
  else if ((selectmode & SCE_SELECT_FACE) && totface > 1) {
    extrude_macro_invoke(C, "MESH_OT_extrude_faces_move", [&](PointerRNA *ptr) {
      PointerRNA sub = RNA_pointer_get(ptr, "TRANSFORM_OT_shrink_fatten");
      RNA_boolean_set(&sub, "release_confirm", false);
    });
  }
  else if ((selectmode & SCE_SELECT_EDGE) && totedge >= 1) {
    extrude_macro_invoke(C, "MESH_OT_extrude_edges_move", [&](PointerRNA *ptr) {
      translate_props_set(C, ptr, nullptr, nullptr);
    });
  }
  else {
    extrude_macro_invoke(C, "MESH_OT_extrude_vertices_move", [&](PointerRNA *ptr) {
      translate_props_set(C, ptr, nullptr, nullptr);
    });
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus edit_mesh_extrude_individual_move_invoke(bContext *C,
                                                                 wmOperator *op,
                                                                 const wmEvent * /*event*/)
{
  return edit_mesh_extrude_individual_move_exec(C, op);
}

void VIEW3D_OT_edit_mesh_extrude_individual_move(wmOperatorType *ot)
{
  ot->name = "Extrude Individual and Move";
  ot->idname = "VIEW3D_OT_edit_mesh_extrude_individual_move";
  ot->description = "Extrude each individual face separately along local normals";

  ot->exec = edit_mesh_extrude_individual_move_exec;
  ot->invoke = edit_mesh_extrude_individual_move_invoke;
  ot->poll = view3d_edit_mesh_extrude_poll;

  /* El Python no declaraba bl_options: sin REGISTER ni UNDO (los pone la macro). */
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name view3d.edit_mesh_extrude_move_normal / _shrink_fatten
 * \{ */

/* `VIEW3D_OT_edit_mesh_extrude_move.extrude_region()`, compartido por los dos. */
static wmOperatorStatus edit_mesh_extrude_region(bContext *C,
                                                 wmOperator *op,
                                                 const bool use_vert_normals,
                                                 const bool dissolve_and_intersect)
{
  Object *ob = CTX_data_active_object(C);
  if (blender::ed::object::shape_key_report_if_active_locked(ob, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  int totface, totedge;
  edit_mesh_selection_counts(ob, &totface, &totedge);

  const bool axis_z[3] = {false, false, true};
  const bool axis_none[3] = {false, false, false};

  if (totface >= 1) {
    if (use_vert_normals) {
      extrude_macro_invoke(C, "MESH_OT_extrude_region_shrink_fatten", [&](PointerRNA *ptr) {
        PointerRNA sub = RNA_pointer_get(ptr, "TRANSFORM_OT_shrink_fatten");
        RNA_boolean_set(&sub, "release_confirm", false);
      });
    }
    else if (dissolve_and_intersect) {
      extrude_macro_invoke(C, "MESH_OT_extrude_manifold", [&](PointerRNA *ptr) {
        PointerRNA region = RNA_pointer_get(ptr, "MESH_OT_extrude_region");
        RNA_boolean_set(&region, "use_dissolve_ortho_edges", true);
        translate_props_set(C, ptr, "NORMAL", axis_z);
      });
    }
    else {
      extrude_macro_invoke(C, "MESH_OT_extrude_region_move", [&](PointerRNA *ptr) {
        translate_props_set(C, ptr, "NORMAL", axis_z);
      });
    }
  }
  else if (totedge == 1) {
    /* Con una sola arista NO se fija `orient_type`: el usuario espera que el boton
     * central del raton use su preferencia de orientacion (#61637), y tampoco se
     * restringe el eje, que resulta demasiado rigido para retopologia. */
    extrude_macro_invoke(C, "MESH_OT_extrude_region_move", [&](PointerRNA *ptr) {
      translate_props_set(C, ptr, nullptr, axis_none);
    });
  }
  else {
    extrude_macro_invoke(C, "MESH_OT_extrude_region_move", [&](PointerRNA *ptr) {
      translate_props_set(C, ptr, nullptr, nullptr);
    });
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus edit_mesh_extrude_move_exec(bContext *C, wmOperator *op)
{
  const bool dissolve_and_intersect = RNA_boolean_get(op->ptr, "dissolve_and_intersect");
  return edit_mesh_extrude_region(C, op, false, dissolve_and_intersect);
}

static wmOperatorStatus edit_mesh_extrude_move_invoke(bContext *C,
                                                      wmOperator *op,
                                                      const wmEvent * /*event*/)
{
  return edit_mesh_extrude_move_exec(C, op);
}

void VIEW3D_OT_edit_mesh_extrude_move_normal(wmOperatorType *ot)
{
  ot->name = "Extrude and Move on Normals";
  ot->idname = "VIEW3D_OT_edit_mesh_extrude_move_normal";
  ot->description = "Extrude region together along the average normal";

  ot->exec = edit_mesh_extrude_move_exec;
  ot->invoke = edit_mesh_extrude_move_invoke;
  ot->poll = view3d_edit_mesh_extrude_poll;
  ot->flag = 0;

  RNA_def_boolean(ot->srna,
                  "dissolve_and_intersect",
                  false,
                  "dissolve_and_intersect",
                  "Dissolves adjacent faces and intersects new geometry");
}

static wmOperatorStatus edit_mesh_extrude_shrink_fatten_exec(bContext *C, wmOperator *op)
{
  return edit_mesh_extrude_region(C, op, true, false);
}

static wmOperatorStatus edit_mesh_extrude_shrink_fatten_invoke(bContext *C,
                                                               wmOperator *op,
                                                               const wmEvent * /*event*/)
{
  return edit_mesh_extrude_shrink_fatten_exec(C, op);
}

void VIEW3D_OT_edit_mesh_extrude_move_shrink_fatten(wmOperatorType *ot)
{
  ot->name = "Extrude and Move on Individual Normals";
  ot->idname = "VIEW3D_OT_edit_mesh_extrude_move_shrink_fatten";
  ot->description = "Extrude region together along local normals";

  ot->exec = edit_mesh_extrude_shrink_fatten_exec;
  ot->invoke = edit_mesh_extrude_shrink_fatten_invoke;
  ot->poll = view3d_edit_mesh_extrude_poll;
  ot->flag = 0;
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name view3d.edit_mesh_extrude_manifold_normal
 * \{ */

static wmOperatorStatus edit_mesh_extrude_manifold_normal_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  if (blender::ed::object::shape_key_report_if_active_locked(ob, op->reports)) {
    return OPERATOR_CANCELLED;
  }

  const bool axis_z[3] = {false, false, true};
  extrude_macro_invoke(C, "MESH_OT_extrude_manifold", [&](PointerRNA *ptr) {
    PointerRNA region = RNA_pointer_get(ptr, "MESH_OT_extrude_region");
    RNA_boolean_set(&region, "use_dissolve_ortho_edges", true);
    translate_props_set(C, ptr, "NORMAL", axis_z);
  });

  return OPERATOR_FINISHED;
}

static wmOperatorStatus edit_mesh_extrude_manifold_normal_invoke(bContext *C,
                                                                 wmOperator *op,
                                                                 const wmEvent * /*event*/)
{
  return edit_mesh_extrude_manifold_normal_exec(C, op);
}

void VIEW3D_OT_edit_mesh_extrude_manifold_normal(wmOperatorType *ot)
{
  ot->name = "Extrude Manifold Along Normals";
  ot->idname = "VIEW3D_OT_edit_mesh_extrude_manifold_normal";
  ot->description = "Extrude manifold region along normals";

  ot->exec = edit_mesh_extrude_manifold_normal_exec;
  ot->invoke = edit_mesh_extrude_manifold_normal_invoke;
  ot->poll = view3d_edit_mesh_extrude_poll;
  ot->flag = 0;
}

/** \} */
