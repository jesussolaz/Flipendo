/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 *
 * Ver FL_view3d_menus.hh.
 *
 * Familia 6: los cuatro menus contextuales grandes de los modos que no son
 * malla — particulas, pose, curva y esqueleto en edicion. Son los que abre el
 * boton derecho (y la tecla `menu`) en cada uno de esos modos.
 */

#include <optional>

#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "FL_ui_registry.hh"

#include "FL_view3d_menus.hh"

namespace blender::ed::view3d {

/** `layout.operator_menu_enum(op, prop)` sin `text=`; ver `fl_view3d_menus_object.cc`. */
static void menu_enum_o(uiLayout *layout,
                        const bContext *C,
                        const char *opname,
                        const char *propname)
{
  wmOperatorType *ot = WM_operatortype_find(opname, false);
  if (ot == nullptr) {
    return;
  }
  PointerRNA opptr;
  uiItemMenuEnumFullO_ptr(layout, C, ot, propname, std::nullopt, ICON_NONE, &opptr);
}

/** `layout.operator(op, text=t).<prop> = <identificador de enumeracion>`. */
static void op_enum_item(
    uiLayout *layout, const char *opname, const char *text, const char *prop, const char *value)
{
  PointerRNA props = layout->op(opname, text ? IFACE_(text) : std::optional<StringRef>(std::nullopt), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, prop, value);
  }
}

/* -------------------------------------------------------------------- */
/** \name VIEW3D_MT_particle_context_menu
 * \{ */

static void particle_context_menu_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  const ToolSettings *ts = CTX_data_tool_settings(C);
  if (ts == nullptr) {
    return;
  }
  /* `tool_settings.particle_edit.select_mode == 'POINT'`. */
  const bool point_mode = ts->particle.selectmode == SCE_SELECT_POINT;

  layout->op("PARTICLE_OT_rekey", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("PARTICLE_OT_delete", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("PARTICLE_OT_remove_doubles", std::nullopt, ICON_NONE);
  layout->op("PARTICLE_OT_unify_length", std::nullopt, ICON_NONE);

  if (point_mode) {
    layout->op("PARTICLE_OT_subdivide", std::nullopt, ICON_NONE);
  }

  layout->op("PARTICLE_OT_weight_set", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("PARTICLE_OT_mirror", std::nullopt, ICON_NONE);

  if (point_mode) {
    layout->separator();

    op_enum_item(layout, "PARTICLE_OT_select_all", N_("All"), "action", "SELECT");
    op_enum_item(layout, "PARTICLE_OT_select_all", N_("None"), "action", "DESELECT");
    op_enum_item(layout, "PARTICLE_OT_select_all", N_("Invert"), "action", "INVERT");

    layout->separator();

    layout->op("PARTICLE_OT_select_roots", std::nullopt, ICON_NONE);
    layout->op("PARTICLE_OT_select_tips", std::nullopt, ICON_NONE);

    layout->separator();

    layout->op("PARTICLE_OT_select_random", std::nullopt, ICON_NONE);

    layout->separator();

    layout->op("PARTICLE_OT_select_more", std::nullopt, ICON_NONE);
    layout->op("PARTICLE_OT_select_less", std::nullopt, ICON_NONE);

    layout->separator();

    layout->op("PARTICLE_OT_select_linked", IFACE_("Select Linked"), ICON_NONE);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_MT_pose_context_menu
 * \{ */

static void pose_context_menu_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);

  layout->op("ANIM_OT_keyframe_insert", IFACE_("Insert Keyframe"), ICON_NONE);
  PointerRNA props = layout->op(
      "ANIM_OT_keyframe_insert_menu", IFACE_("Insert Keyframe with Keying Set"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "always_prompt", true);
  }

  layout->separator();

  layout->op("POSE_OT_copy", std::nullopt, ICON_COPYDOWN);
  props = layout->op("POSE_OT_paste", std::nullopt, ICON_PASTEDOWN);
  if (props.data) {
    RNA_boolean_set(&props, "flipped", false);
  }
  props = layout->op("POSE_OT_paste", IFACE_("Paste X-Flipped Pose"), ICON_PASTEFLIPDOWN);
  if (props.data) {
    RNA_boolean_set(&props, "flipped", true);
  }

  layout->separator();

  props = layout->op("WM_OT_call_panel", IFACE_("Rename Active Bone..."), ICON_NONE);
  if (props.data) {
    RNA_string_set(&props, "name", "TOPBAR_PT_name");
    RNA_boolean_set(&props, "keep_open", false);
  }

  layout->separator();

  layout->op("POSE_OT_push", std::nullopt, ICON_NONE);
  layout->op("POSE_OT_relax", std::nullopt, ICON_NONE);
  layout->op("POSE_OT_breakdown", std::nullopt, ICON_NONE);
  layout->op("POSE_OT_blend_to_neighbor", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("POSE_OT_paths_calculate", IFACE_("Calculate Motion Paths"), ICON_NONE);
  layout->op("POSE_OT_paths_clear", IFACE_("Clear Motion Paths"), ICON_NONE);
  layout->op("POSE_OT_paths_update", IFACE_("Update Armature Motion Paths"), ICON_NONE);
  layout->op("OBJECT_OT_paths_update_visible", IFACE_("Update All Motion Paths"), ICON_NONE);

  layout->separator();

  props = layout->op("POSE_OT_hide", std::nullopt, ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "unselected", false);
  }
  layout->op("POSE_OT_reveal", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("POSE_OT_user_transforms_clear", std::nullopt, ICON_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_MT_edit_curve_context_menu
 * \{ */

static void edit_curve_context_menu_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);

  /* Anadir. */
  layout->op("CURVE_OT_subdivide", std::nullopt, ICON_NONE);
  layout->op("CURVE_OT_extrude_move", std::nullopt, ICON_NONE);
  layout->op("CURVE_OT_make_segment", std::nullopt, ICON_NONE);
  layout->op("CURVE_OT_duplicate_move", std::nullopt, ICON_NONE);

  layout->separator();

  /* Transformar. */
  op_enum_item(layout, "TRANSFORM_OT_transform", N_("Radius"), "mode", "CURVE_SHRINKFATTEN");
  layout->op("TRANSFORM_OT_tilt", std::nullopt, ICON_NONE);
  layout->op("CURVE_OT_tilt_clear", std::nullopt, ICON_NONE);
  layout->op("CURVE_OT_smooth", std::nullopt, ICON_NONE);
  layout->op("CURVE_OT_smooth_tilt", std::nullopt, ICON_NONE);
  layout->op("CURVE_OT_smooth_radius", std::nullopt, ICON_NONE);

  layout->separator();

  layout->menu("VIEW3D_MT_mirror", std::nullopt, ICON_NONE);
  layout->menu("VIEW3D_MT_snap", std::nullopt, ICON_NONE);

  layout->separator();

  /* Modificar. */
  menu_enum_o(layout, C, "CURVE_OT_spline_type_set", "type");
  menu_enum_o(layout, C, "CURVE_OT_handle_type_set", "type");
  layout->op("CURVE_OT_cyclic_toggle", std::nullopt, ICON_NONE);
  layout->op("CURVE_OT_switch_direction", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("CURVE_OT_normals_make_consistent", std::nullopt, ICON_NONE);
  layout->op("CURVE_OT_spline_weight_set", std::nullopt, ICON_NONE);
  layout->op("CURVE_OT_radius_set", std::nullopt, ICON_NONE);

  layout->separator();

  /* Quitar. */
  layout->op("CURVE_OT_split", std::nullopt, ICON_NONE);
  layout->op("CURVE_OT_decimate", std::nullopt, ICON_NONE);
  layout->op("CURVE_OT_separate", std::nullopt, ICON_NONE);
  layout->op("CURVE_OT_dissolve_verts", std::nullopt, ICON_NONE);
  op_enum_item(layout, "CURVE_OT_delete", N_("Delete Segment"), "type", "SEGMENT");
  op_enum_item(layout, "CURVE_OT_delete", N_("Delete Point"), "type", "VERT");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_MT_armature_context_menu
 * \{ */

static void armature_context_menu_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  /* `context.edit_object.data.use_mirror_x`. */
  const Object *edit_object = CTX_data_edit_object(C);
  if (edit_object == nullptr || edit_object->data == nullptr) {
    return;
  }
  const bArmature *arm = static_cast<const bArmature *>(edit_object->data);

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);

  /* Anadir. */
  layout->op("ARMATURE_OT_subdivide", IFACE_("Subdivide"), ICON_NONE);
  layout->op("ARMATURE_OT_duplicate_move", IFACE_("Duplicate"), ICON_NONE);
  layout->op("ARMATURE_OT_extrude_move", std::nullopt, ICON_NONE);
  if (arm->flag & ARM_MIRROR_EDIT) {
    layout->op("ARMATURE_OT_extrude_forked", std::nullopt, ICON_NONE);
  }

  layout->separator();

  layout->op("ARMATURE_OT_fill", std::nullopt, ICON_NONE);

  layout->separator();

  /* Modificar. */
  layout->menu("VIEW3D_MT_mirror", std::nullopt, ICON_NONE);
  layout->menu("VIEW3D_MT_snap", std::nullopt, ICON_NONE);
  layout->op("ARMATURE_OT_symmetrize", std::nullopt, ICON_NONE);
  layout->op("ARMATURE_OT_switch_direction", IFACE_("Switch Direction"), ICON_NONE);
  layout->menu("VIEW3D_MT_edit_armature_names", std::nullopt, ICON_NONE);

  layout->separator();

  layout->menu("VIEW3D_MT_edit_armature_parent", std::nullopt, ICON_NONE);

  layout->separator();

  /* Quitar. */
  layout->op("ARMATURE_OT_split", std::nullopt, ICON_NONE);
  layout->op("ARMATURE_OT_separate", std::nullopt, ICON_NONE);
  layout->op("ARMATURE_OT_dissolve", std::nullopt, ICON_NONE);
  layout->op("ARMATURE_OT_delete", std::nullopt, ICON_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registro
 * \{ */

static const flipendo::MenuDecl view3d_ctxmode_menus[] = {
    {
        /*idname*/ "VIEW3D_MT_particle_context_menu",
        /*label*/ N_("Particle"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ particle_context_menu_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_pose_context_menu",
        /*label*/ N_("Pose"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ pose_context_menu_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_edit_curve_context_menu",
        /*label*/ N_("Curve"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ edit_curve_context_menu_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_armature_context_menu",
        /*label*/ N_("Armature"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ armature_context_menu_draw,
    },
};

void view3d_ctxmode_menus_register()
{
  flipendo::menus_register({view3d_ctxmode_menus, ARRAY_SIZE(view3d_ctxmode_menus)});
}

/** \} */

}  // namespace blender::ed::view3d
