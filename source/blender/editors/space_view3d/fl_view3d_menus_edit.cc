/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 *
 * Ver FL_view3d_menus.hh.
 *
 * Familia 2: los menus de BORRADO de los modos de edicion — la tecla X, que es
 * la misma en malla, curva, esqueleto y lapiz — mas los dos vecinos que el
 * keymap abre con M (fusionar) e Y (separar) en la edicion de malla.
 *
 * Son los menus mas usados de la edicion y los que menos dependen de nada:
 * todos sus operadores son ya nativos (`MESH_OT_*`, `CURVE_OT_*`,
 * `ARMATURE_OT_*`, `GREASE_PENCIL_OT_*`), asi que no dejan deuda detras.
 */

#include <optional>

#include "BLI_utildefines.h"

#include "BKE_screen.hh"

#include "BLT_translation.hh"

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
/** \name Malla: borrar, fusionar, separar
 * \{ */

static void edit_mesh_delete_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  uiItemsEnumO(layout, "MESH_OT_delete", "type");

  layout->separator();

  layout->op("MESH_OT_dissolve_verts", std::nullopt, ICON_NONE);
  layout->op("MESH_OT_dissolve_edges", std::nullopt, ICON_NONE);
  layout->op("MESH_OT_dissolve_faces", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("MESH_OT_dissolve_limited", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("MESH_OT_edge_collapse", std::nullopt, ICON_NONE);
  layout->op("MESH_OT_delete_edgeloop", IFACE_("Edge Loops"), ICON_NONE);

  geometry::ui_template_node_operator_asset_menu_items(
      *layout, *const_cast<bContext *>(C), "Mesh/Delete");
}

static void edit_mesh_merge_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  uiItemsEnumO(layout, "MESH_OT_merge", "type");

  layout->separator();

  layout->op("MESH_OT_remove_doubles", IFACE_("By Distance"), ICON_NONE);

  geometry::ui_template_node_operator_asset_menu_items(
      *layout, *const_cast<bContext *>(C), "Mesh/Merge");
}

static void edit_mesh_split_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  layout->op("MESH_OT_split", IFACE_("Selection"), ICON_NONE);

  layout->separator();

  uiItemsEnumO(layout, "MESH_OT_edge_split", "type");

  geometry::ui_template_node_operator_asset_menu_items(
      *layout, *const_cast<bContext *>(C), "Mesh/Split");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lapiz de cera, curva y esqueleto: borrar
 * \{ */

static void edit_greasepencil_delete_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;

  layout->op("GREASE_PENCIL_OT_delete", std::nullopt, ICON_NONE);

  layout->separator();

  uiItemsEnumO(layout, "GREASE_PENCIL_OT_dissolve", "type");

  layout->separator();

  PointerRNA props = layout->op(
      "GREASE_PENCIL_OT_delete_frame", IFACE_("Delete Active Keyframe (Active Layer)"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "type", "ACTIVE_FRAME");
  }
  props = layout->op(
      "GREASE_PENCIL_OT_delete_frame", IFACE_("Delete Active Keyframes (All Layers)"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "type", "ALL_FRAMES");
  }
}

/** `GREASE_PENCIL_MT_draw_delete`, de `properties_grease_pencil_common.py`. */
static void greasepencil_draw_delete_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;
  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);

  PointerRNA props = layout->op(
      "GREASE_PENCIL_OT_delete_frame", IFACE_("Delete Active Keyframe (Active Layer)"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "type", "ACTIVE_FRAME");
  }
  props = layout->op(
      "GREASE_PENCIL_OT_delete_frame", IFACE_("Delete Active Keyframes (All Layers)"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "type", "ALL_FRAMES");
  }
}

static void edit_curve_delete_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;

  uiItemsEnumO(layout, "CURVE_OT_delete", "type");

  layout->separator();

  layout->op("CURVE_OT_dissolve_verts", std::nullopt, ICON_NONE);
}

static void edit_armature_delete_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;
  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_AREA);

  layout->op("ARMATURE_OT_delete", IFACE_("Bones"), ICON_NONE);

  layout->separator();

  layout->op("ARMATURE_OT_dissolve", IFACE_("Dissolve Bones"), ICON_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registro
 * \{ */

static const flipendo::MenuDecl view3d_edit_menus[] = {
    {
        /*idname*/ "VIEW3D_MT_edit_mesh_delete",
        /*label*/ N_("Delete"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ edit_mesh_delete_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_edit_mesh_merge",
        /*label*/ N_("Merge"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ edit_mesh_merge_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_edit_mesh_split",
        /*label*/ N_("Split"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ edit_mesh_split_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_edit_greasepencil_delete",
        /*label*/ N_("Delete"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ edit_greasepencil_delete_draw,
    },
    {
        /*idname*/ "GREASE_PENCIL_MT_draw_delete",
        /*label*/ N_("Delete"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ greasepencil_draw_delete_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_edit_curve_delete",
        /*label*/ N_("Delete"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ edit_curve_delete_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_edit_armature_delete",
        /*label*/ N_("Delete"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ edit_armature_delete_draw,
    },
};

void view3d_edit_menus_register()
{
  flipendo::menus_register({view3d_edit_menus, ARRAY_SIZE(view3d_edit_menus)});
}

/** \} */

}  // namespace blender::ed::view3d
