/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 *
 * Ver FL_view3d_menus.hh.
 *
 * Familia 3: los submenus de la edicion de malla que el keymap abre con
 * Ctrl-V, Ctrl-E, Ctrl-F, Alt-E, Alt-N, Shift-G y la tecla de modo de
 * seleccion. Son los siete menus que un modelador abre mas veces al dia.
 *
 * Dos dependencias que NO son nativas todavia y quedan anotadas como deuda en
 * `politicas/MENUS-DEL-KEYMAP-A-CPP.md`:
 * - `view3d.edit_mesh_extrude_move_normal`, `…_move_shrink_fatten` y
 *   `…_manifold_normal` siguen siendo operadores de Python
 *   (`scripts/startup/bl_operators/view3d.py`). El menu se migra igual: el
 *   nombre se resuelve en tiempo de dibujo, asi que en cuanto ese operador sea
 *   C++ el menu funciona sin tocarlo.
 * - Los submenus que estos abren (`VIEW3D_MT_vertex_group`, `VIEW3D_MT_hook`,
 *   `VIEW3D_MT_edit_mesh_faces_data`, los tres de fuerza de normales…) siguen
 *   en Python; se llaman por cadena, asi que conviven sin problema.
 */

#include <optional>

#include "BLI_math_base.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_editmesh.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.hh"

#include "ED_geometry.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_types.hh"

#include "FL_ui_registry.hh"

#include "FL_view3d_menus.hh"

namespace blender::ed::view3d {

/* -------------------------------------------------------------------- */
/** \name Modo de seleccion
 * \{ */

static void select_mode_item(uiLayout *layout, const char *text, int icon, const char *type)
{
  PointerRNA props = layout->op("MESH_OT_select_mode", IFACE_(text), icon);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "type", type);
  }
}

static void edit_mesh_select_mode_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);
  select_mode_item(layout, N_("Vertex"), ICON_VERTEXSEL, "VERT");
  select_mode_item(layout, N_("Edge"), ICON_EDGESEL, "EDGE");
  select_mode_item(layout, N_("Face"), ICON_FACESEL, "FACE");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Extruir
 * \{ */

static void edit_mesh_extrude_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;
  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);

  const ToolSettings *ts = CTX_data_tool_settings(C);
  Object *ob = CTX_data_active_object(C);
  if (ts == nullptr || ob == nullptr || ob->type != OB_MESH) {
    return;
  }
  /* `mesh.total_*_sel` (`rna_Mesh_tot_*_get`): cuentas de la malla en edicion. */
  const BMEditMesh *em = BKE_editmesh_from_object(ob);
  const int total_vert_sel = em ? em->bm->totvertsel : 0;
  const int total_edge_sel = em ? em->bm->totedgesel : 0;
  const int total_face_sel = em ? em->bm->totfacesel : 0;
  const bool select_vert = (ts->selectmode & SCE_SELECT_VERTEX) != 0;
  const bool select_edge = (ts->selectmode & SCE_SELECT_EDGE) != 0;

  if (total_face_sel) {
    layout->op("VIEW3D_OT_edit_mesh_extrude_move_normal", IFACE_("Extrude Faces"), ICON_NONE);
    layout->op("VIEW3D_OT_edit_mesh_extrude_move_shrink_fatten",
               IFACE_("Extrude Faces Along Normals"),
               ICON_NONE);
    layout->op("MESH_OT_extrude_faces_move", IFACE_("Extrude Individual Faces"), ICON_NONE);
    layout->op("VIEW3D_OT_edit_mesh_extrude_manifold_normal",
               IFACE_("Extrude Manifold"),
               ICON_NONE);
  }

  if (total_edge_sel && (select_vert || select_edge)) {
    layout->op("MESH_OT_extrude_edges_move", IFACE_("Extrude Edges"), ICON_NONE);
  }

  if (total_vert_sel && select_vert) {
    layout->op("MESH_OT_extrude_vertices_move", IFACE_("Extrude Vertices"), ICON_NONE);
  }

  layout->separator();

  layout->op("MESH_OT_extrude_repeat", std::nullopt, ICON_NONE);
  PointerRNA props = layout->op("MESH_OT_spin", std::nullopt, ICON_NONE);
  if (props.data) {
    RNA_float_set(&props, "angle", M_PI * 2.0f);
  }
  geometry::ui_template_node_operator_asset_menu_items(
      *layout, *const_cast<bContext *>(C), "Mesh/Extrude");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Vertice
 * \{ */

static void edit_mesh_vertices_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;
  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);

  layout->op("MESH_OT_extrude_vertices_move", IFACE_("Extrude Vertices"), ICON_NONE);
  PointerRNA props = layout->op("MESH_OT_dupli_extrude_cursor", std::nullopt, ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "rotate_source", true);
  }
  props = layout->op("MESH_OT_bevel", IFACE_("Bevel Vertices"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "affect", "VERTICES");
  }

  layout->separator();

  layout->op("MESH_OT_edge_face_add", IFACE_("New Edge/Face from Vertices"), ICON_NONE);
  layout->op("MESH_OT_vert_connect_path", IFACE_("Connect Vertex Path"), ICON_NONE);
  layout->op("MESH_OT_vert_connect", IFACE_("Connect Vertex Pairs"), ICON_NONE);

  layout->separator();

  /* `props.MESH_OT_rip.use_fill`: `mesh.rip_move` es una macro, y sus
   * propiedades cuelgan de un puntero por cada operador de la cadena. */
  props = layout->op("MESH_OT_rip_move", IFACE_("Rip Vertices"), ICON_NONE);
  if (props.data) {
    PointerRNA rip = RNA_pointer_get(&props, "MESH_OT_rip");
    RNA_boolean_set(&rip, "use_fill", false);
  }
  props = layout->op("MESH_OT_rip_move", IFACE_("Rip Vertices and Fill"), ICON_NONE);
  if (props.data) {
    PointerRNA rip = RNA_pointer_get(&props, "MESH_OT_rip");
    RNA_boolean_set(&rip, "use_fill", true);
  }
  layout->op("MESH_OT_rip_edge_move", IFACE_("Rip Vertices and Extend"), ICON_NONE);

  layout->separator();

  layout->op("TRANSFORM_OT_vert_slide", IFACE_("Slide Vertices"), ICON_NONE);
  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_REGION_WIN);
  props = layout->op("MESH_OT_vertices_smooth", IFACE_("Smooth Vertices"), ICON_NONE);
  if (props.data) {
    RNA_float_set(&props, "factor", 0.5f);
  }
  layout->op(
      "MESH_OT_vertices_smooth_laplacian", IFACE_("Smooth Vertices (Laplacian)"), ICON_NONE);
  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);

  layout->separator();

  layout->op("TRANSFORM_OT_vert_crease", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("MESH_OT_blend_from_shape", std::nullopt, ICON_NONE);
  layout->op("MESH_OT_shape_propagate_to_all", IFACE_("Propagate to Shapes"), ICON_NONE);

  layout->separator();

  layout->menu("VIEW3D_MT_vertex_group", std::nullopt, ICON_NONE);
  layout->menu("VIEW3D_MT_hook", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("OBJECT_OT_vertex_parent_set", std::nullopt, ICON_NONE);

  geometry::ui_template_node_operator_asset_menu_items(
      *layout, *const_cast<bContext *>(C), "Vertex");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Arista
 * \{ */

static void edit_mesh_edges_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  /* `bpy.app.build_options.freestyle`. */
#ifdef WITH_FREESTYLE
  const bool with_freestyle = true;
#else
  const bool with_freestyle = false;
#endif

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);

  layout->op("MESH_OT_extrude_edges_move", IFACE_("Extrude Edges"), ICON_NONE);
  PointerRNA props = layout->op("MESH_OT_bevel", IFACE_("Bevel Edges"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "affect", "EDGES");
  }
  layout->op("MESH_OT_bridge_edge_loops", std::nullopt, ICON_NONE);
  layout->op("MESH_OT_screw", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("MESH_OT_subdivide", std::nullopt, ICON_NONE);
  layout->op("MESH_OT_subdivide_edgering", std::nullopt, ICON_NONE);
  layout->op("MESH_OT_unsubdivide", std::nullopt, ICON_NONE);

  layout->separator();

  props = layout->op("MESH_OT_edge_rotate", IFACE_("Rotate Edge CW"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "use_ccw", false);
  }
  props = layout->op("MESH_OT_edge_rotate", IFACE_("Rotate Edge CCW"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "use_ccw", true);
  }

  layout->separator();

  layout->op("TRANSFORM_OT_edge_slide", std::nullopt, ICON_NONE);
  props = layout->op("MESH_OT_loopcut_slide", std::nullopt, ICON_NONE);
  if (props.data) {
    PointerRNA slide = RNA_pointer_get(&props, "TRANSFORM_OT_edge_slide");
    RNA_boolean_set(&slide, "release_confirm", false);
  }
  layout->op("MESH_OT_offset_edge_loops_slide", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("TRANSFORM_OT_edge_crease", std::nullopt, ICON_NONE);
  layout->op("TRANSFORM_OT_edge_bevelweight", std::nullopt, ICON_NONE);

  layout->separator();

  props = layout->op("MESH_OT_mark_seam", std::nullopt, ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "clear", false);
  }
  props = layout->op("MESH_OT_mark_seam", IFACE_("Clear Seam"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "clear", true);
  }

  layout->separator();

  layout->op("MESH_OT_mark_sharp", std::nullopt, ICON_NONE);
  props = layout->op("MESH_OT_mark_sharp", IFACE_("Clear Sharp"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "clear", true);
  }

  props = layout->op("MESH_OT_mark_sharp", IFACE_("Mark Sharp from Vertices"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "use_verts", true);
  }
  props = layout->op("MESH_OT_mark_sharp", IFACE_("Clear Sharp from Vertices"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "use_verts", true);
    RNA_boolean_set(&props, "clear", true);
  }

  layout->op("MESH_OT_set_sharpness_by_angle", std::nullopt, ICON_NONE);

  if (with_freestyle) {
    layout->separator();

    props = layout->op("MESH_OT_mark_freestyle_edge", std::nullopt, ICON_NONE);
    if (props.data) {
      RNA_boolean_set(&props, "clear", false);
    }
    props = layout->op("MESH_OT_mark_freestyle_edge", IFACE_("Clear Freestyle Edge"), ICON_NONE);
    if (props.data) {
      RNA_boolean_set(&props, "clear", true);
    }
  }

  geometry::ui_template_node_operator_asset_menu_items(
      *layout, *const_cast<bContext *>(C), "Edge");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cara
 * \{ */

static void edit_mesh_faces_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);

  layout->op("VIEW3D_OT_edit_mesh_extrude_move_normal", IFACE_("Extrude Faces"), ICON_NONE);
  layout->op("VIEW3D_OT_edit_mesh_extrude_move_shrink_fatten",
             IFACE_("Extrude Faces Along Normals"),
             ICON_NONE);
  layout->op("MESH_OT_extrude_faces_move", IFACE_("Extrude Individual Faces"), ICON_NONE);

  layout->separator();

  layout->op("MESH_OT_inset", std::nullopt, ICON_NONE);
  layout->op("MESH_OT_poke", std::nullopt, ICON_NONE);
  PointerRNA props = layout->op("MESH_OT_quads_convert_to_tris", std::nullopt, ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "quad_method", "BEAUTY");
    RNA_enum_set_identifier(nullptr, &props, "ngon_method", "BEAUTY");
  }
  layout->op("MESH_OT_tris_convert_to_quads", std::nullopt, ICON_NONE);
  layout->op("MESH_OT_solidify", IFACE_("Solidify Faces"), ICON_NONE);
  layout->op("MESH_OT_wireframe", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("MESH_OT_fill", std::nullopt, ICON_NONE);
  layout->op("MESH_OT_fill_grid", std::nullopt, ICON_NONE);
  layout->op("MESH_OT_beautify_fill", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("MESH_OT_intersect", std::nullopt, ICON_NONE);
  layout->op("MESH_OT_intersect_boolean", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("MESH_OT_face_split_by_edges", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("MESH_OT_faces_shade_smooth", std::nullopt, ICON_NONE);
  layout->op("MESH_OT_faces_shade_flat", std::nullopt, ICON_NONE);

  layout->separator();

  layout->menu("VIEW3D_MT_edit_mesh_faces_data", std::nullopt, ICON_NONE);

  geometry::ui_template_node_operator_asset_menu_items(
      *layout, *const_cast<bContext *>(C), "Face");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Normales
 * \{ */

static void edit_mesh_normals_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  layout->op("MESH_OT_flip_normals", IFACE_("Flip"), ICON_NONE);
  PointerRNA props = layout->op(
      "MESH_OT_normals_make_consistent", IFACE_("Recalculate Outside"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "inside", false);
  }
  props = layout->op("MESH_OT_normals_make_consistent", IFACE_("Recalculate Inside"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "inside", true);
  }

  layout->separator();

  layout->op("MESH_OT_set_normals_from_faces", IFACE_("Set from Faces"), ICON_NONE);

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);
  layout->op("TRANSFORM_OT_rotate_normal", IFACE_("Rotate..."), ICON_NONE);
  layout->op("MESH_OT_point_normals", IFACE_("Point to Target..."), ICON_NONE);
  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_REGION_WIN);

  layout->op("MESH_OT_merge_normals", IFACE_("Merge"), ICON_NONE);
  layout->op("MESH_OT_split_normals", IFACE_("Split"), ICON_NONE);
  layout->menu("VIEW3D_MT_edit_mesh_normals_average",
               CTX_IFACE_(BLT_I18NCONTEXT_ID_MESH, "Average"),
               ICON_NONE);

  layout->separator();

  props = layout->op("MESH_OT_normals_tools", IFACE_("Copy Vector"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "mode", "COPY");
  }
  props = layout->op("MESH_OT_normals_tools", IFACE_("Paste Vector"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "mode", "PASTE");
  }

  layout->op("MESH_OT_smooth_normals", IFACE_("Smooth Vectors"), ICON_NONE);
  props = layout->op("MESH_OT_normals_tools", IFACE_("Reset Vectors"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "mode", "RESET");
  }

  layout->separator();

  layout->menu("VIEW3D_MT_edit_mesh_normals_select_strength", std::nullopt, ICON_NONE);
  layout->menu("VIEW3D_MT_edit_mesh_normals_set_strength", std::nullopt, ICON_NONE);
  geometry::ui_template_node_operator_asset_menu_items(
      *layout, *const_cast<bContext *>(C), "Mesh/Normals");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Seleccionar similar
 * \{ */

static void edit_mesh_select_similar_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;

  uiItemsEnumO(layout, "MESH_OT_select_similar", "type");

  layout->separator();

  layout->op("MESH_OT_select_similar_region", IFACE_("Face Regions"), ICON_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registro
 * \{ */

static const flipendo::MenuDecl view3d_mesh_menus[] = {
    {
        /*idname*/ "VIEW3D_MT_edit_mesh_select_mode",
        /*label*/ N_("Mesh Select Mode"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ edit_mesh_select_mode_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_edit_mesh_extrude",
        /*label*/ N_("Extrude"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ edit_mesh_extrude_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_edit_mesh_vertices",
        /*label*/ N_("Vertex"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ edit_mesh_vertices_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_edit_mesh_edges",
        /*label*/ N_("Edge"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ edit_mesh_edges_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_edit_mesh_faces",
        /*label*/ N_("Face"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ edit_mesh_faces_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_edit_mesh_normals",
        /*label*/ N_("Normals"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ edit_mesh_normals_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_edit_mesh_select_similar",
        /*label*/ N_("Select Similar"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ edit_mesh_select_similar_draw,
    },
};

void view3d_mesh_menus_register()
{
  flipendo::menus_register({view3d_mesh_menus, ARRAY_SIZE(view3d_mesh_menus)});
}

/** \} */

}  // namespace blender::ed::view3d
