/* SPDX-FileCopyrightText: 2004 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 */

#include <cmath>

#include "BLI_math_matrix.h"
#include "BLI_math_rotation.h"
#include "BLI_sys_types.h"
#include "BLI_vector.hh"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLT_translation.hh"

#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_editmesh.hh"
#include "BKE_mesh.h"
#include "BKE_mesh.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_mesh.hh"
#include "ED_object.hh"
#include "ED_screen.hh"

#include "mesh_intern.hh" /* own include */

#define MESH_ADD_VERTS_MAXI 10000000

/* ********* add primitive operators ************* */

struct MakePrimitiveData {
  float mat[4][4];
  bool was_editmode;
};

static Object *make_prim_init(bContext *C,
                              const char *idname,
                              const float loc[3],
                              const float rot[3],
                              const float scale[3],
                              ushort local_view_bits,
                              MakePrimitiveData *r_creation_data)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  Object *obedit = CTX_data_edit_object(C);

  r_creation_data->was_editmode = false;
  if (obedit == nullptr || obedit->type != OB_MESH) {
    obedit = blender::ed::object::add_type(C, OB_MESH, idname, loc, rot, false, local_view_bits);
    blender::ed::object::editmode_enter_ex(bmain, scene, obedit, 0);

    r_creation_data->was_editmode = true;
  }

  blender::ed::object::new_primitive_matrix(C, obedit, loc, rot, scale, r_creation_data->mat);

  return obedit;
}

static void make_prim_finish(bContext *C,
                             Object *obedit,
                             const MakePrimitiveData *creation_data,
                             int enter_editmode)
{
  BMEditMesh *em = BKE_editmesh_from_object(obedit);
  const bool exit_editmode = ((creation_data->was_editmode == true) && (enter_editmode == false));

  /* Primitive has all verts selected, use vert select flush
   * to push this up to edges & faces. */
  EDBM_selectmode_flush_ex(em, SCE_SELECT_VERTEX);

  /* Only recalculate edit-mode tessellation if we are staying in edit-mode. */
  EDBMUpdate_Params params{};
  params.calc_looptris = !exit_editmode;
  params.calc_normals = false;
  params.is_destructive = true;
  EDBM_update(static_cast<Mesh *>(obedit->data), &params);

  /* userdef */
  if (exit_editmode) {
    blender::ed::object::editmode_exit_ex(
        CTX_data_main(C), CTX_data_scene(C), obedit, blender::ed::object::EM_FREEDATA);
  }
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, obedit);
}

static wmOperatorStatus add_primitive_plane_exec(bContext *C, wmOperator *op)
{
  MakePrimitiveData creation_data;
  Object *obedit;
  BMEditMesh *em;
  float loc[3], rot[3];
  bool enter_editmode;
  ushort local_view_bits;
  const bool calc_uvs = RNA_boolean_get(op->ptr, "calc_uvs");

  WM_operator_view3d_unit_defaults(C, op);
  blender::ed::object::add_generic_get_opts(
      C, op, 'Z', loc, rot, nullptr, &enter_editmode, &local_view_bits, nullptr);
  obedit = make_prim_init(C,
                          CTX_DATA_(BLT_I18NCONTEXT_ID_MESH, "Plane"),
                          loc,
                          rot,
                          nullptr,
                          local_view_bits,
                          &creation_data);

  em = BKE_editmesh_from_object(obedit);

  if (calc_uvs) {
    ED_mesh_uv_ensure(static_cast<Mesh *>(obedit->data), nullptr);
  }

  if (!EDBM_op_call_and_selectf(
          em,
          op,
          "verts.out",
          false,
          "create_grid x_segments=%i y_segments=%i size=%f matrix=%m4 calc_uvs=%b",
          0,
          0,
          RNA_float_get(op->ptr, "size") / 2.0f,
          creation_data.mat,
          calc_uvs))
  {
    return OPERATOR_CANCELLED;
  }

  make_prim_finish(C, obedit, &creation_data, enter_editmode);

  return OPERATOR_FINISHED;
}

void MESH_OT_primitive_plane_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Plane";
  ot->description = "Construct a filled planar mesh with 4 vertices";
  ot->idname = "MESH_OT_primitive_plane_add";

  /* API callbacks. */
  ot->exec = add_primitive_plane_exec;
  ot->poll = ED_operator_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  blender::ed::object::add_unit_props_size(ot);
  blender::ed::object::add_mesh_props(ot);
  blender::ed::object::add_generic_props(ot, true);
}

static wmOperatorStatus add_primitive_cube_exec(bContext *C, wmOperator *op)
{
  MakePrimitiveData creation_data;
  Object *obedit;
  BMEditMesh *em;
  float loc[3], rot[3], scale[3];
  bool enter_editmode;
  ushort local_view_bits;
  const bool calc_uvs = RNA_boolean_get(op->ptr, "calc_uvs");

  WM_operator_view3d_unit_defaults(C, op);
  blender::ed::object::add_generic_get_opts(
      C, op, 'Z', loc, rot, scale, &enter_editmode, &local_view_bits, nullptr);
  obedit = make_prim_init(C,
                          CTX_DATA_(BLT_I18NCONTEXT_ID_MESH, "Cube"),
                          loc,
                          rot,
                          scale,
                          local_view_bits,
                          &creation_data);

  em = BKE_editmesh_from_object(obedit);

  if (calc_uvs) {
    ED_mesh_uv_ensure(static_cast<Mesh *>(obedit->data), nullptr);
  }

  if (!EDBM_op_call_and_selectf(em,
                                op,
                                "verts.out",
                                false,
                                "create_cube matrix=%m4 size=%f calc_uvs=%b",
                                creation_data.mat,
                                RNA_float_get(op->ptr, "size"),
                                calc_uvs))
  {
    return OPERATOR_CANCELLED;
  }

  /* BMESH_TODO make plane side this: M_SQRT2 - plane (diameter of 1.41 makes it unit size) */
  make_prim_finish(C, obedit, &creation_data, enter_editmode);

  return OPERATOR_FINISHED;
}

void MESH_OT_primitive_cube_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Cube";
  ot->description = "Construct a cube mesh that consists of six square faces";
  ot->idname = "MESH_OT_primitive_cube_add";

  /* API callbacks. */
  ot->exec = add_primitive_cube_exec;
  ot->poll = ED_operator_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  blender::ed::object::add_unit_props_size(ot);
  blender::ed::object::add_mesh_props(ot);
  blender::ed::object::add_generic_props(ot, true);
}

static const EnumPropertyItem fill_type_items[] = {
    {0, "NOTHING", 0, "Nothing", "Don't fill at all"},
    {1, "NGON", 0, "N-Gon", "Use n-gons"},
    {2, "TRIFAN", 0, "Triangle Fan", "Use triangle fans"},
    {0, nullptr, 0, nullptr, nullptr},
};

static wmOperatorStatus add_primitive_circle_exec(bContext *C, wmOperator *op)
{
  MakePrimitiveData creation_data;
  Object *obedit;
  BMEditMesh *em;
  float loc[3], rot[3];
  bool enter_editmode;
  ushort local_view_bits;
  int cap_end, cap_tri;
  const bool calc_uvs = RNA_boolean_get(op->ptr, "calc_uvs");

  cap_end = RNA_enum_get(op->ptr, "fill_type");
  cap_tri = (cap_end == 2);

  WM_operator_view3d_unit_defaults(C, op);
  blender::ed::object::add_generic_get_opts(
      C, op, 'Z', loc, rot, nullptr, &enter_editmode, &local_view_bits, nullptr);
  obedit = make_prim_init(C,
                          CTX_DATA_(BLT_I18NCONTEXT_ID_MESH, "Circle"),
                          loc,
                          rot,
                          nullptr,
                          local_view_bits,
                          &creation_data);

  em = BKE_editmesh_from_object(obedit);

  if (calc_uvs) {
    ED_mesh_uv_ensure(static_cast<Mesh *>(obedit->data), nullptr);
  }

  if (!EDBM_op_call_and_selectf(
          em,
          op,
          "verts.out",
          false,
          "create_circle segments=%i radius=%f cap_ends=%b cap_tris=%b matrix=%m4 calc_uvs=%b",
          RNA_int_get(op->ptr, "vertices"),
          RNA_float_get(op->ptr, "radius"),
          cap_end,
          cap_tri,
          creation_data.mat,
          calc_uvs))
  {
    return OPERATOR_CANCELLED;
  }

  make_prim_finish(C, obedit, &creation_data, enter_editmode);

  return OPERATOR_FINISHED;
}

void MESH_OT_primitive_circle_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Circle";
  ot->description = "Construct a circle mesh";
  ot->idname = "MESH_OT_primitive_circle_add";

  /* API callbacks. */
  ot->exec = add_primitive_circle_exec;
  ot->poll = ED_operator_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_int(ot->srna, "vertices", 32, 3, MESH_ADD_VERTS_MAXI, "Vertices", "", 3, 500);
  blender::ed::object::add_unit_props_radius(ot);
  RNA_def_enum(ot->srna, "fill_type", fill_type_items, 0, "Fill Type", "");

  blender::ed::object::add_mesh_props(ot);
  blender::ed::object::add_generic_props(ot, true);
}

static wmOperatorStatus add_primitive_cylinder_exec(bContext *C, wmOperator *op)
{
  MakePrimitiveData creation_data;
  Object *obedit;
  BMEditMesh *em;
  float loc[3], rot[3], scale[3];
  bool enter_editmode;
  ushort local_view_bits;
  const int end_fill_type = RNA_enum_get(op->ptr, "end_fill_type");
  const bool cap_end = (end_fill_type != 0);
  const bool cap_tri = (end_fill_type == 2);
  const bool calc_uvs = RNA_boolean_get(op->ptr, "calc_uvs");

  WM_operator_view3d_unit_defaults(C, op);
  blender::ed::object::add_generic_get_opts(
      C, op, 'Z', loc, rot, scale, &enter_editmode, &local_view_bits, nullptr);
  obedit = make_prim_init(C,
                          CTX_DATA_(BLT_I18NCONTEXT_ID_MESH, "Cylinder"),
                          loc,
                          rot,
                          scale,
                          local_view_bits,
                          &creation_data);
  em = BKE_editmesh_from_object(obedit);

  if (calc_uvs) {
    ED_mesh_uv_ensure(static_cast<Mesh *>(obedit->data), nullptr);
  }

  if (!EDBM_op_call_and_selectf(em,
                                op,
                                "verts.out",
                                false,
                                "create_cone segments=%i radius1=%f radius2=%f cap_ends=%b "
                                "cap_tris=%b depth=%f matrix=%m4 calc_uvs=%b",
                                RNA_int_get(op->ptr, "vertices"),
                                RNA_float_get(op->ptr, "radius"),
                                RNA_float_get(op->ptr, "radius"),
                                cap_end,
                                cap_tri,
                                RNA_float_get(op->ptr, "depth"),
                                creation_data.mat,
                                calc_uvs))
  {
    return OPERATOR_CANCELLED;
  }

  make_prim_finish(C, obedit, &creation_data, enter_editmode);

  return OPERATOR_FINISHED;
}

void MESH_OT_primitive_cylinder_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Cylinder";
  ot->description = "Construct a cylinder mesh";
  ot->idname = "MESH_OT_primitive_cylinder_add";

  /* API callbacks. */
  ot->exec = add_primitive_cylinder_exec;
  ot->poll = ED_operator_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_int(ot->srna, "vertices", 32, 3, MESH_ADD_VERTS_MAXI, "Vertices", "", 3, 500);
  blender::ed::object::add_unit_props_radius(ot);
  RNA_def_float_distance(
      ot->srna, "depth", 2.0f, 0.0, OBJECT_ADD_SIZE_MAXF, "Depth", "", 0.001, 100.00);
  RNA_def_enum(ot->srna, "end_fill_type", fill_type_items, 1, "Cap Fill Type", "");

  blender::ed::object::add_mesh_props(ot);
  blender::ed::object::add_generic_props(ot, true);
}

static wmOperatorStatus add_primitive_cone_exec(bContext *C, wmOperator *op)
{
  MakePrimitiveData creation_data;
  Object *obedit;
  BMEditMesh *em;
  float loc[3], rot[3], scale[3];
  bool enter_editmode;
  ushort local_view_bits;
  const int end_fill_type = RNA_enum_get(op->ptr, "end_fill_type");
  const bool cap_end = (end_fill_type != 0);
  const bool cap_tri = (end_fill_type == 2);
  const bool calc_uvs = RNA_boolean_get(op->ptr, "calc_uvs");

  WM_operator_view3d_unit_defaults(C, op);
  blender::ed::object::add_generic_get_opts(
      C, op, 'Z', loc, rot, scale, &enter_editmode, &local_view_bits, nullptr);
  obedit = make_prim_init(C,
                          CTX_DATA_(BLT_I18NCONTEXT_ID_MESH, "Cone"),
                          loc,
                          rot,
                          scale,
                          local_view_bits,
                          &creation_data);
  em = BKE_editmesh_from_object(obedit);

  if (calc_uvs) {
    ED_mesh_uv_ensure(static_cast<Mesh *>(obedit->data), nullptr);
  }

  if (!EDBM_op_call_and_selectf(em,
                                op,
                                "verts.out",
                                false,
                                "create_cone segments=%i radius1=%f radius2=%f cap_ends=%b "
                                "cap_tris=%b depth=%f matrix=%m4 calc_uvs=%b",
                                RNA_int_get(op->ptr, "vertices"),
                                RNA_float_get(op->ptr, "radius1"),
                                RNA_float_get(op->ptr, "radius2"),
                                cap_end,
                                cap_tri,
                                RNA_float_get(op->ptr, "depth"),
                                creation_data.mat,
                                calc_uvs))
  {
    return OPERATOR_CANCELLED;
  }

  make_prim_finish(C, obedit, &creation_data, enter_editmode);

  return OPERATOR_FINISHED;
}

void MESH_OT_primitive_cone_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Cone";
  ot->description = "Construct a conic mesh";
  ot->idname = "MESH_OT_primitive_cone_add";

  /* API callbacks. */
  ot->exec = add_primitive_cone_exec;
  ot->poll = ED_operator_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_int(ot->srna, "vertices", 32, 3, MESH_ADD_VERTS_MAXI, "Vertices", "", 3, 500);
  RNA_def_float_distance(
      ot->srna, "radius1", 1.0f, 0.0, OBJECT_ADD_SIZE_MAXF, "Radius 1", "", 0.001, 100.00);
  RNA_def_float_distance(
      ot->srna, "radius2", 0.0f, 0.0, OBJECT_ADD_SIZE_MAXF, "Radius 2", "", 0.0, 100.00);
  RNA_def_float_distance(
      ot->srna, "depth", 2.0f, 0.0, OBJECT_ADD_SIZE_MAXF, "Depth", "", 0.001, 100.00);
  RNA_def_enum(ot->srna, "end_fill_type", fill_type_items, 1, "Base Fill Type", "");

  blender::ed::object::add_mesh_props(ot);
  blender::ed::object::add_generic_props(ot, true);
}

static wmOperatorStatus add_primitive_grid_exec(bContext *C, wmOperator *op)
{
  MakePrimitiveData creation_data;
  Object *obedit;
  BMEditMesh *em;
  float loc[3], rot[3];
  bool enter_editmode;
  ushort local_view_bits;
  const bool calc_uvs = RNA_boolean_get(op->ptr, "calc_uvs");

  WM_operator_view3d_unit_defaults(C, op);
  blender::ed::object::add_generic_get_opts(
      C, op, 'Z', loc, rot, nullptr, &enter_editmode, &local_view_bits, nullptr);
  obedit = make_prim_init(C,
                          CTX_DATA_(BLT_I18NCONTEXT_ID_MESH, "Grid"),
                          loc,
                          rot,
                          nullptr,
                          local_view_bits,
                          &creation_data);
  em = BKE_editmesh_from_object(obedit);

  if (calc_uvs) {
    ED_mesh_uv_ensure(static_cast<Mesh *>(obedit->data), nullptr);
  }

  if (!EDBM_op_call_and_selectf(
          em,
          op,
          "verts.out",
          false,
          "create_grid x_segments=%i y_segments=%i size=%f matrix=%m4 calc_uvs=%b",
          RNA_int_get(op->ptr, "x_subdivisions"),
          RNA_int_get(op->ptr, "y_subdivisions"),
          RNA_float_get(op->ptr, "size") / 2.0f,
          creation_data.mat,
          calc_uvs))
  {
    return OPERATOR_CANCELLED;
  }

  make_prim_finish(C, obedit, &creation_data, enter_editmode);

  return OPERATOR_FINISHED;
}

void MESH_OT_primitive_grid_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Grid";
  ot->description = "Construct a subdivided plane mesh";
  ot->idname = "MESH_OT_primitive_grid_add";

  /* API callbacks. */
  ot->exec = add_primitive_grid_exec;
  ot->poll = ED_operator_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  /* Note that if you use MESH_ADD_VERTS_MAXI for both x and y at the same time
   * you will still reach impossible values (10^12 vertices or so...). */
  RNA_def_int(
      ot->srna, "x_subdivisions", 10, 1, MESH_ADD_VERTS_MAXI, "X Subdivisions", "", 1, 1000);
  RNA_def_int(
      ot->srna, "y_subdivisions", 10, 1, MESH_ADD_VERTS_MAXI, "Y Subdivisions", "", 1, 1000);

  blender::ed::object::add_unit_props_size(ot);
  blender::ed::object::add_mesh_props(ot);
  blender::ed::object::add_generic_props(ot, true);
}

static wmOperatorStatus add_primitive_monkey_exec(bContext *C, wmOperator *op)
{
  MakePrimitiveData creation_data;
  Object *obedit;
  BMEditMesh *em;
  float loc[3], rot[3];
  float dia;
  bool enter_editmode;
  ushort local_view_bits;
  const bool calc_uvs = RNA_boolean_get(op->ptr, "calc_uvs");

  WM_operator_view3d_unit_defaults(C, op);
  blender::ed::object::add_generic_get_opts(
      C, op, 'Y', loc, rot, nullptr, &enter_editmode, &local_view_bits, nullptr);

  obedit = make_prim_init(C,
                          CTX_DATA_(BLT_I18NCONTEXT_ID_MESH, "Suzanne"),
                          loc,
                          rot,
                          nullptr,
                          local_view_bits,
                          &creation_data);
  dia = RNA_float_get(op->ptr, "size") / 2.0f;
  mul_mat3_m4_fl(creation_data.mat, dia);

  em = BKE_editmesh_from_object(obedit);

  if (calc_uvs) {
    ED_mesh_uv_ensure(static_cast<Mesh *>(obedit->data), nullptr);
  }

  if (!EDBM_op_call_and_selectf(em,
                                op,
                                "verts.out",
                                false,
                                "create_monkey matrix=%m4 calc_uvs=%b",
                                creation_data.mat,
                                calc_uvs))
  {
    return OPERATOR_CANCELLED;
  }

  make_prim_finish(C, obedit, &creation_data, enter_editmode);

  return OPERATOR_FINISHED;
}

void MESH_OT_primitive_monkey_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Monkey";
  ot->description = "Construct a Suzanne mesh";
  ot->idname = "MESH_OT_primitive_monkey_add";

  /* API callbacks. */
  ot->exec = add_primitive_monkey_exec;
  ot->poll = ED_operator_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  blender::ed::object::add_unit_props_size(ot);
  blender::ed::object::add_mesh_props(ot);
  blender::ed::object::add_generic_props(ot, true);
}

static wmOperatorStatus add_primitive_uvsphere_exec(bContext *C, wmOperator *op)
{
  MakePrimitiveData creation_data;
  Object *obedit;
  BMEditMesh *em;
  float loc[3], rot[3], scale[3];
  bool enter_editmode;
  ushort local_view_bits;
  const bool calc_uvs = RNA_boolean_get(op->ptr, "calc_uvs");

  WM_operator_view3d_unit_defaults(C, op);
  blender::ed::object::add_generic_get_opts(
      C, op, 'Z', loc, rot, scale, &enter_editmode, &local_view_bits, nullptr);
  obedit = make_prim_init(C,
                          CTX_DATA_(BLT_I18NCONTEXT_ID_MESH, "Sphere"),
                          loc,
                          rot,
                          scale,
                          local_view_bits,
                          &creation_data);
  em = BKE_editmesh_from_object(obedit);

  if (calc_uvs) {
    ED_mesh_uv_ensure(static_cast<Mesh *>(obedit->data), nullptr);
  }

  if (!EDBM_op_call_and_selectf(
          em,
          op,
          "verts.out",
          false,
          "create_uvsphere u_segments=%i v_segments=%i radius=%f matrix=%m4 calc_uvs=%b",
          RNA_int_get(op->ptr, "segments"),
          RNA_int_get(op->ptr, "ring_count"),
          RNA_float_get(op->ptr, "radius"),
          creation_data.mat,
          calc_uvs))
  {
    return OPERATOR_CANCELLED;
  }

  make_prim_finish(C, obedit, &creation_data, enter_editmode);

  return OPERATOR_FINISHED;
}

void MESH_OT_primitive_uv_sphere_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add UV Sphere";
  ot->description =
      "Construct a spherical mesh with quad faces, except for triangle faces at the top and "
      "bottom";
  ot->idname = "MESH_OT_primitive_uv_sphere_add";

  /* API callbacks. */
  ot->exec = add_primitive_uvsphere_exec;
  ot->poll = ED_operator_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_int(ot->srna, "segments", 32, 3, MESH_ADD_VERTS_MAXI / 100, "Segments", "", 3, 500);
  RNA_def_int(ot->srna, "ring_count", 16, 3, MESH_ADD_VERTS_MAXI / 100, "Rings", "", 3, 500);

  blender::ed::object::add_unit_props_radius(ot);
  blender::ed::object::add_mesh_props(ot);
  blender::ed::object::add_generic_props(ot, true);
}

static wmOperatorStatus add_primitive_icosphere_exec(bContext *C, wmOperator *op)
{
  MakePrimitiveData creation_data;
  Object *obedit;
  BMEditMesh *em;
  float loc[3], rot[3], scale[3];
  bool enter_editmode;
  ushort local_view_bits;
  const bool calc_uvs = RNA_boolean_get(op->ptr, "calc_uvs");

  WM_operator_view3d_unit_defaults(C, op);
  blender::ed::object::add_generic_get_opts(
      C, op, 'Z', loc, rot, scale, &enter_editmode, &local_view_bits, nullptr);
  obedit = make_prim_init(C,
                          CTX_DATA_(BLT_I18NCONTEXT_ID_MESH, "Icosphere"),
                          loc,
                          rot,
                          scale,
                          local_view_bits,
                          &creation_data);
  em = BKE_editmesh_from_object(obedit);

  if (calc_uvs) {
    ED_mesh_uv_ensure(static_cast<Mesh *>(obedit->data), nullptr);
  }

  if (!EDBM_op_call_and_selectf(
          em,
          op,
          "verts.out",
          false,
          "create_icosphere subdivisions=%i radius=%f matrix=%m4 calc_uvs=%b",
          RNA_int_get(op->ptr, "subdivisions"),
          RNA_float_get(op->ptr, "radius"),
          creation_data.mat,
          calc_uvs))
  {
    return OPERATOR_CANCELLED;
  }

  make_prim_finish(C, obedit, &creation_data, enter_editmode);

  return OPERATOR_FINISHED;
}

void MESH_OT_primitive_ico_sphere_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Ico Sphere";
  ot->description = "Construct a spherical mesh that consists of equally sized triangles";
  ot->idname = "MESH_OT_primitive_ico_sphere_add";

  /* API callbacks. */
  ot->exec = add_primitive_icosphere_exec;
  ot->poll = ED_operator_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* props */
  RNA_def_int(ot->srna, "subdivisions", 2, 1, 10, "Subdivisions", "", 1, 8);

  blender::ed::object::add_unit_props_radius(ot);
  blender::ed::object::add_mesh_props(ot);
  blender::ed::object::add_generic_props(ot, true);
}

/* -------------------------------------------------------------------- */
/** \name Add Torus Operator
 *
 * Migracion de `scripts/startup/bl_operators/add_mesh_torus.py` (Carril C, Flipendo).
 * Mismo idname `mesh.primitive_torus_add`, mismas propiedades. A diferencia de los
 * primitivos de arriba, el Python original NO construye la malla dentro de un objeto
 * en modo edicion (no usa `bmesh`): crea una `Mesh` de datos sueltos con
 * vertices/loops/poligonos ya calculados y la asigna a un objeto nuevo via
 * `object_data_add`. Aqui se reproduce lo mismo con la API de bajo nivel de `Mesh`
 * (`BKE_mesh_new_nomain` + `mesh_calc_edges`) y `add_type_with_obdata`, que es el
 * equivalente en C++ de `object_data_add` ya usado por el resto de operadores
 * "add object" nativos (`OBJECT_OT_empty_add` y hermanos en `object_add.cc`).
 * \{ */

enum {
  MESH_TORUS_MAJOR_MINOR = 0,
  MESH_TORUS_EXT_INT = 1,
};

static void torus_verts_and_faces(const float major_rad,
                                  const float minor_rad,
                                  const int major_seg,
                                  const int minor_seg,
                                  blender::Vector<blender::float3> &r_verts,
                                  blender::Vector<int> &r_faces)
{
  using namespace blender;

  const int tot_verts = major_seg * minor_seg;
  r_verts.reserve(tot_verts);
  r_faces.reserve(tot_verts * 4);

  int i1 = 0;
  for (int major_index = 0; major_index < major_seg; major_index++) {
    float mat[3][3];
    axis_angle_to_mat3_single(mat, 'Z', (float(major_index) / float(major_seg)) * float(2.0 * M_PI));

    for (int minor_index = 0; minor_index < minor_seg; minor_index++) {
      const float angle = float(2.0 * M_PI) * float(minor_index) / float(minor_seg);

      const float3 local(major_rad + cosf(angle) * minor_rad, 0.0f, sinf(angle) * minor_rad);
      float3 vec;
      mul_v3_m3v3(vec, mat, local);
      r_verts.append(vec);

      int i2, i3, i4;
      if (minor_index + 1 == minor_seg) {
        i2 = major_index * minor_seg;
        i3 = i1 + minor_seg;
        i4 = i2 + minor_seg;
      }
      else {
        i2 = i1 + 1;
        i3 = i1 + minor_seg;
        i4 = i3 + 1;
      }

      if (i2 >= tot_verts) {
        i2 -= tot_verts;
      }
      if (i3 >= tot_verts) {
        i3 -= tot_verts;
      }
      if (i4 >= tot_verts) {
        i4 -= tot_verts;
      }

      r_faces.append(i1);
      r_faces.append(i3);
      r_faces.append(i4);
      r_faces.append(i2);

      i1++;
    }
  }
}

static void torus_add_uvs(Mesh *mesh, const int minor_seg, const int major_seg)
{
  using namespace blender;

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  bke::SpanAttributeWriter<float2> uv_map = attributes.lookup_or_add_for_write_only_span<float2>(
      "UVMap", bke::AttrDomain::Corner);
  const Span<int> face_offsets = mesh->face_offsets();

  const float u_step = 1.0f / float(major_seg);
  const float v_step = 1.0f / float(minor_seg);

  const float u_init = 0.5f + fmodf(0.5f, u_step);
  const float v_init = 0.5f + fmodf(0.5f, v_step);

  const float u_wrap = 1.0f - (u_step / 2.0f);
  const float v_wrap = 1.0f - (v_step / 2.0f);

  int face_index = 0;
  float u_prev = u_init;
  float u_next = u_prev + u_step;
  for (int major_index = 0; major_index < major_seg; major_index++) {
    float v_prev = v_init;
    float v_next = v_prev + v_step;
    for (int minor_index = 0; minor_index < minor_seg; minor_index++) {
      const int loop_start = face_offsets[face_index];
      /* Mismo orden de esquinas que el Python (create_grid: i1,i3,i4,i2 -> loops
       * 0,1,2,3 == i1,i3,i4,i2). */
      uv_map.span[loop_start + 0] = float2(u_prev, v_prev);
      uv_map.span[loop_start + 1] = float2(u_next, v_prev);
      uv_map.span[loop_start + 3] = float2(u_prev, v_next);
      uv_map.span[loop_start + 2] = float2(u_next, v_next);

      v_prev = (v_next > v_wrap) ? (v_next - 1.0f) : v_next;
      v_next = v_prev + v_step;

      face_index++;
    }
    u_prev = (u_next > u_wrap) ? (u_next - 1.0f) : u_next;
    u_next = u_prev + u_step;
  }

  uv_map.finish();
}

static wmOperatorStatus add_primitive_torus_exec(bContext *C, wmOperator *op)
{
  float major_radius = RNA_float_get(op->ptr, "major_radius");
  float minor_radius = RNA_float_get(op->ptr, "minor_radius");
  const int major_segments = RNA_int_get(op->ptr, "major_segments");
  const int minor_segments = RNA_int_get(op->ptr, "minor_segments");
  const bool generate_uvs = RNA_boolean_get(op->ptr, "generate_uvs");

  if (RNA_enum_get(op->ptr, "mode") == MESH_TORUS_EXT_INT) {
    const float abso_major_rad = RNA_float_get(op->ptr, "abso_major_rad");
    const float abso_minor_rad = RNA_float_get(op->ptr, "abso_minor_rad");
    const float extra_helper = (abso_major_rad - abso_minor_rad) * 0.5f;
    major_radius = abso_minor_rad + extra_helper;
    minor_radius = extra_helper;
    RNA_float_set(op->ptr, "major_radius", major_radius);
    RNA_float_set(op->ptr, "minor_radius", minor_radius);
  }

  blender::Vector<blender::float3> verts;
  blender::Vector<int> faces;
  torus_verts_and_faces(major_radius, minor_radius, major_segments, minor_segments, verts, faces);

  const int verts_num = verts.size();
  const int faces_num = int(faces.size()) / 4;
  const int corners_num = int(faces.size());

  Mesh *mesh = BKE_mesh_new_nomain(verts_num, 0, faces_num, corners_num);
  mesh->vert_positions_for_write().copy_from(verts);
  blender::MutableSpan<int> face_offsets = mesh->face_offsets_for_write();
  blender::MutableSpan<int> corner_verts = mesh->corner_verts_for_write();
  for (int i = 0; i < faces_num; i++) {
    face_offsets[i] = i * 4;
  }
  corner_verts.copy_from(faces);

  blender::bke::mesh_calc_edges(*mesh, false, false);
  blender::bke::mesh_smooth_set(*mesh, false);

  if (generate_uvs) {
    torus_add_uvs(mesh, minor_segments, major_segments);
  }

  float loc[3], rot[3];
  bool enter_editmode;
  ushort local_view_bits;
  blender::ed::object::add_generic_get_opts(
      C, op, 'Z', loc, rot, nullptr, &enter_editmode, &local_view_bits, nullptr);

  Object *ob = blender::ed::object::add_type_with_obdata(C,
                                                         OB_MESH,
                                                         CTX_DATA_(BLT_I18NCONTEXT_ID_MESH, "Torus"),
                                                         loc,
                                                         rot,
                                                         enter_editmode,
                                                         local_view_bits,
                                                         &mesh->id);
  BLI_assert(ob->data == mesh);

  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, ob);

  return OPERATOR_FINISHED;
}

static void mesh_torus_mode_update(Main * /*bmain*/, Scene * /*scene*/, PointerRNA *ptr)
{
  if (RNA_enum_get(ptr, "mode") == MESH_TORUS_EXT_INT) {
    const float major_radius = RNA_float_get(ptr, "major_radius");
    const float minor_radius = RNA_float_get(ptr, "minor_radius");
    RNA_float_set(ptr, "abso_major_rad", major_radius + minor_radius);
    RNA_float_set(ptr, "abso_minor_rad", major_radius - minor_radius);
  }
}

void MESH_OT_primitive_torus_add(wmOperatorType *ot)
{
  static const EnumPropertyItem prop_torus_mode_items[] = {
      {MESH_TORUS_MAJOR_MINOR,
       "MAJOR_MINOR",
       0,
       "Major/Minor",
       "Use the major/minor radii for torus dimensions"},
      {MESH_TORUS_EXT_INT,
       "EXT_INT",
       0,
       "Exterior/Interior",
       "Use the exterior/interior radii for torus dimensions"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Add Torus";
  ot->description = "Construct a torus mesh";
  ot->idname = "MESH_OT_primitive_torus_add";

  /* API callbacks. */
  ot->exec = add_primitive_torus_exec;
  ot->poll = ED_operator_scene_editable;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_PRESET;

  /* properties */
  PropertyRNA *prop;
  RNA_def_int(ot->srna,
             "major_segments",
             48,
             3,
             256,
             "Major Segments",
             "Number of segments for the main ring of the torus",
             3,
             256);
  RNA_def_int(ot->srna,
             "minor_segments",
             12,
             3,
             256,
             "Minor Segments",
             "Number of segments for the minor ring of the torus",
             3,
             256);
  prop = RNA_def_enum(
      ot->srna, "mode", prop_torus_mode_items, MESH_TORUS_MAJOR_MINOR, "Dimensions Mode", "");
  RNA_def_property_update_runtime(prop, mesh_torus_mode_update);

  RNA_def_float_distance(ot->srna,
                        "major_radius",
                        1.0f,
                        0.0f,
                        10000.0f,
                        "Major Radius",
                        "Radius from the origin to the center of the cross sections",
                        0.0f,
                        100.0f);
  RNA_def_float_distance(ot->srna,
                        "minor_radius",
                        0.25f,
                        0.0f,
                        10000.0f,
                        "Minor Radius",
                        "Radius of the torus' cross section",
                        0.0f,
                        100.0f);
  RNA_def_float_distance(ot->srna,
                        "abso_major_rad",
                        1.25f,
                        0.0f,
                        10000.0f,
                        "Exterior Radius",
                        "Total Exterior Radius of the torus",
                        0.0f,
                        100.0f);
  RNA_def_float_distance(ot->srna,
                        "abso_minor_rad",
                        0.75f,
                        0.0f,
                        10000.0f,
                        "Interior Radius",
                        "Total Interior Radius of the torus",
                        0.0f,
                        100.0f);
  RNA_def_boolean(ot->srna, "generate_uvs", true, "Generate UVs", "Generate a default UV map");

  blender::ed::object::add_generic_props(ot, false);
}

/** \} */
