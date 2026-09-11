/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spgraph
 *
 * Menus nativos del editor de curvas: sustituyen a las clases `Menu` de
 * `scripts/startup/bl_ui/space_graph.py`.
 *
 * Son los siete que el keymap nativo abre por nombre en este editor — el
 * contextual (boton derecho), los tres radiales (punto de pivote, ajustar,
 * vista), el de borrar (X) y los dos de suavizado y mezcla de claves. Ver
 * `politicas/MENUS-DEL-KEYMAP-A-CPP.md`.
 */

#include <optional>

#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_types.hh"

#include "FL_ui_registry.hh"

#include "FL_graph_menus.hh"
#include "graph_intern.hh"

namespace blender::ed::graph {

/** `layout.operator(op, text=t).<prop> = <identificador>`. */
static void op_enum_item(
    uiLayout *layout, const char *opname, const char *text, const char *prop, const char *value)
{
  PointerRNA props = layout->op(opname, text ? IFACE_(text) : std::optional<StringRef>(std::nullopt), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, prop, value);
  }
}

/* -------------------------------------------------------------------- */
/** \name GRAPH_MT_context_menu
 * \{ */

static void context_menu_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);

  layout->op("GRAPH_OT_copy", IFACE_("Copy"), ICON_COPYDOWN);
  layout->op("GRAPH_OT_paste", IFACE_("Paste"), ICON_PASTEDOWN);
  PointerRNA props = layout->op("GRAPH_OT_paste", IFACE_("Paste Flipped"), ICON_PASTEFLIPDOWN);
  if (props.data) {
    RNA_boolean_set(&props, "flipped", true);
  }

  layout->separator();

  uiItemMenuEnumO(layout, C, "GRAPH_OT_handle_type", "type", IFACE_("Handle Type"), ICON_NONE);
  uiItemMenuEnumO(
      layout, C, "GRAPH_OT_interpolation_type", "type", IFACE_("Interpolation Mode"), ICON_NONE);
  uiItemMenuEnumO(layout, C, "GRAPH_OT_easing_type", "type", IFACE_("Easing Type"), ICON_NONE);

  layout->separator();

  op_enum_item(layout, "GRAPH_OT_keyframe_insert", nullptr, "type", "SEL");
  layout->op("GRAPH_OT_duplicate_move", std::nullopt, ICON_NONE);
  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_REGION_WIN);
  layout->op("GRAPH_OT_delete", std::nullopt, ICON_NONE);

  layout->separator();

  uiItemMenuEnumO(layout, C, "GRAPH_OT_mirror", "type", IFACE_("Mirror"), ICON_NONE);
  uiItemMenuEnumO(layout, C, "GRAPH_OT_snap", "type", IFACE_("Snap"), ICON_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Radiales
 * \{ */

static void pivot_pie_draw(const bContext *C, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  SpaceGraph *sipo = CTX_wm_space_graph(C);
  if (sipo == nullptr) {
    return;
  }
  PointerRNA space = RNA_pointer_create_discrete(
      reinterpret_cast<ID *>(CTX_wm_screen(C)), &RNA_SpaceGraphEditor, sipo);

  uiItemEnumR_string(&pie, &space, "pivot_point", "BOUNDING_BOX_CENTER", std::nullopt, ICON_NONE);
  uiItemEnumR_string(&pie, &space, "pivot_point", "CURSOR", std::nullopt, ICON_NONE);
  uiItemEnumR_string(&pie, &space, "pivot_point", "INDIVIDUAL_ORIGINS", std::nullopt, ICON_NONE);
}

static void snap_pie_item(uiLayout &pie, const char *text, const char *type)
{
  PointerRNA props = pie.op("GRAPH_OT_snap", IFACE_(text), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "type", type);
  }
}

static void snap_pie_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  snap_pie_item(pie, N_("Selection to Current Frame"), "CFRA");
  snap_pie_item(pie, N_("Selection to Cursor Value"), "VALUE");
  snap_pie_item(pie, N_("Selection to Nearest Frame"), "NEAREST_FRAME");
  snap_pie_item(pie, N_("Selection to Nearest Second"), "NEAREST_SECOND");
  snap_pie_item(pie, N_("Selection to Nearest Marker"), "NEAREST_MARKER");
  snap_pie_item(pie, N_("Flatten Handles"), "HORIZONTAL");
  pie.op("GRAPH_OT_frame_jump", IFACE_("Cursor to Selection"), ICON_NONE);
  pie.op("GRAPH_OT_snap_cursor_value", IFACE_("Cursor Value to Selection"), ICON_NONE);
}

static void view_pie_draw(const bContext *C, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  pie.op("GRAPH_OT_view_all", std::nullopt, ICON_NONE);
  pie.op("GRAPH_OT_view_selected", std::nullopt, ICON_ZOOM_SELECTED);
  pie.op("GRAPH_OT_view_frame", std::nullopt, ICON_NONE);

  /* `context.scene.use_preview_range`. */
  const Scene *scene = CTX_data_scene(C);
  const bool preview_range = scene && (scene->r.flag & SCER_PRV_RANGE);
  pie.op("ANIM_OT_scene_range_frame",
         preview_range ? IFACE_("Frame Preview Range") : IFACE_("Frame Scene Range"),
         ICON_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Borrar, suavizar y mezclar
 * \{ */

static void delete_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;

  layout->op("GRAPH_OT_delete", std::nullopt, ICON_NONE);

  layout->separator();

  PointerRNA props = layout->op("GRAPH_OT_clean", std::nullopt, ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "channels", false);
  }
  props = layout->op("GRAPH_OT_clean", IFACE_("Clean Channels"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "channels", true);
  }
}

static void key_smoothing_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;
  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);

  layout->op("GRAPH_OT_gaussian_smooth", IFACE_("Smooth (Gaussian)"), ICON_NONE);
  layout->op("GRAPH_OT_smooth", IFACE_("Smooth (Legacy)"), ICON_NONE);
  layout->op("GRAPH_OT_butterworth_smooth", std::nullopt, ICON_NONE);
}

static void key_blending_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;
  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);

  layout->op("GRAPH_OT_breakdown", IFACE_("Breakdown"), ICON_NONE);
  layout->op("GRAPH_OT_blend_to_neighbor", IFACE_("Blend to Neighbor"), ICON_NONE);
  layout->op("GRAPH_OT_blend_to_default", IFACE_("Blend to Default Value"), ICON_NONE);
  layout->op("GRAPH_OT_ease", IFACE_("Ease"), ICON_NONE);
  layout->op("GRAPH_OT_blend_offset", IFACE_("Blend Offset"), ICON_NONE);
  layout->op("GRAPH_OT_blend_to_ease", IFACE_("Blend to Ease"), ICON_NONE);
  layout->op("GRAPH_OT_match_slope", IFACE_("Match Slope"), ICON_NONE);
  layout->op("GRAPH_OT_push_pull", IFACE_("Push Pull"), ICON_NONE);
  layout->op("GRAPH_OT_shear", IFACE_("Shear Keys"), ICON_NONE);
  layout->op("GRAPH_OT_scale_average", IFACE_("Scale Average"), ICON_NONE);
  layout->op("GRAPH_OT_scale_from_neighbor", IFACE_("Scale from Neighbor"), ICON_NONE);
  layout->op("GRAPH_OT_time_offset", IFACE_("Time Offset"), ICON_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registro
 * \{ */

static const flipendo::MenuDecl graph_menus[] = {
    {
        /*idname*/ "GRAPH_MT_context_menu",
        /*label*/ N_("F-Curve"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ context_menu_draw,
    },
    {
        /*idname*/ "GRAPH_MT_pivot_pie",
        /*label*/ N_("Pivot Point"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ pivot_pie_draw,
    },
    {
        /*idname*/ "GRAPH_MT_snap_pie",
        /*label*/ N_("Snap"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ snap_pie_draw,
    },
    {
        /*idname*/ "GRAPH_MT_view_pie",
        /*label*/ N_("View"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ view_pie_draw,
    },
    {
        /*idname*/ "GRAPH_MT_delete",
        /*label*/ N_("Delete"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ delete_draw,
    },
    {
        /*idname*/ "GRAPH_MT_key_smoothing",
        /*label*/ N_("Smooth"),
        /*description*/ nullptr,
        /*translation_context*/ BLT_I18NCONTEXT_OPERATOR_DEFAULT,
        /*draw*/ key_smoothing_draw,
    },
    {
        /*idname*/ "GRAPH_MT_key_blending",
        /*label*/ N_("Blend"),
        /*description*/ nullptr,
        /*translation_context*/ BLT_I18NCONTEXT_OPERATOR_DEFAULT,
        /*draw*/ key_blending_draw,
    },
};

void graph_menus_register()
{
  flipendo::menus_register({graph_menus, ARRAY_SIZE(graph_menus)});
}

/** \} */

}  // namespace blender::ed::graph
