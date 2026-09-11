/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spnla
 *
 * Ver FL_nla_menus.hh. Los cuatro menus del editor de NLA que el keymap nativo
 * abre por nombre: el contextual de tiras, el de pistas, y los dos radiales.
 */

#include <optional>

#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "DNA_scene_types.h"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "FL_ui_registry.hh"

#include "FL_nla_menus.hh"

namespace blender::ed::nla {

static void op_enum_item(
    uiLayout *layout, const char *opname, const char *text, const char *prop, const char *value)
{
  PointerRNA props = layout->op(opname, IFACE_(text), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, prop, value);
  }
}

static void snap_pie_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  op_enum_item(&pie, "NLA_OT_snap", N_("Selection to Current Frame"), "type", "CFRA");
  op_enum_item(&pie, "NLA_OT_snap", N_("Selection to Nearest Frame"), "type", "NEAREST_FRAME");
  op_enum_item(&pie, "NLA_OT_snap", N_("Selection to Nearest Second"), "type", "NEAREST_SECOND");
  op_enum_item(&pie, "NLA_OT_snap", N_("Selection to Nearest Marker"), "type", "NEAREST_MARKER");
}

static void view_pie_draw(const bContext *C, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  pie.op("NLA_OT_view_all", std::nullopt, ICON_NONE);
  pie.op("NLA_OT_view_selected", std::nullopt, ICON_ZOOM_SELECTED);
  pie.op("NLA_OT_view_frame", std::nullopt, ICON_NONE);

  const Scene *scene = CTX_data_scene(C);
  const bool preview_range = scene && (scene->r.flag & SCER_PRV_RANGE);
  pie.op("ANIM_OT_scene_range_frame",
         preview_range ? IFACE_("Frame Preview Range") : IFACE_("Frame Scene Range"),
         ICON_NONE);
}

static void channel_context_menu_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  uiItemMenuEnumO(
      layout, C, "ANIM_OT_channels_move", "direction", IFACE_("Track Ordering..."), ICON_NONE);

  layout->separator();

  PointerRNA props = layout->op("NLA_OT_tracks_add", IFACE_("Add Track"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "above_selected", false);
  }
  props = layout->op("NLA_OT_tracks_add", IFACE_("Add Track Above Selected"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "above_selected", true);
  }
  layout->separator();
  layout->op("NLA_OT_tracks_delete", std::nullopt, ICON_NONE);
  layout->op("ANIM_OT_channels_clean_empty", std::nullopt, ICON_NONE);
}

static void context_menu_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  /* `scene.is_nla_tweakmode`. */
  const Scene *scene = CTX_data_scene(C);
  const bool tweakmode = scene != nullptr && (scene->flag & SCE_NLA_EDIT_ON) != 0;

  PointerRNA props;
  if (tweakmode) {
    props = layout->op("NLA_OT_tweakmode_exit", IFACE_("Stop Editing Stashed Action"), ICON_NONE);
    if (props.data) {
      RNA_boolean_set(&props, "isolate_action", true);
    }
    layout->op("NLA_OT_tweakmode_exit", IFACE_("Stop Tweaking Strip Actions"), ICON_NONE);
  }
  else {
    props = layout->op("NLA_OT_tweakmode_enter", IFACE_("Start Editing Stashed Action"), ICON_NONE);
    if (props.data) {
      RNA_boolean_set(&props, "isolate_action", true);
    }
    props = layout->op(
        "NLA_OT_tweakmode_enter", IFACE_("Start Tweaking Strip Actions (Full Stack)"), ICON_NONE);
    if (props.data) {
      RNA_boolean_set(&props, "use_upper_stack_evaluation", true);
    }
    props = layout->op(
        "NLA_OT_tweakmode_enter", IFACE_("Start Tweaking Strip Actions (Lower Stack)"), ICON_NONE);
    if (props.data) {
      RNA_boolean_set(&props, "use_upper_stack_evaluation", false);
    }
  }

  layout->separator();

  props = layout->op("WM_OT_call_panel", IFACE_("Rename..."), ICON_NONE);
  if (props.data) {
    RNA_string_set(&props, "name", "TOPBAR_PT_name");
    RNA_boolean_set(&props, "keep_open", false);
  }
  layout->op("NLA_OT_duplicate_move", std::nullopt, ICON_NONE);
  layout->op("NLA_OT_duplicate_linked_move", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("NLA_OT_split", std::nullopt, ICON_NONE);
  layout->op("NLA_OT_delete", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("NLA_OT_meta_add", std::nullopt, ICON_NONE);
  layout->op("NLA_OT_meta_remove", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("NLA_OT_swap", std::nullopt, ICON_NONE);

  layout->separator();

  uiItemMenuEnumO(layout, C, "NLA_OT_snap", "type", IFACE_("Snap"), ICON_NONE);
}

static const flipendo::MenuDecl nla_menus[] = {
    {
        /*idname*/ "NLA_MT_context_menu",
        /*label*/ N_("NLA"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ context_menu_draw,
    },
    {
        /*idname*/ "NLA_MT_channel_context_menu",
        /*label*/ N_("NLA Tracks"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ channel_context_menu_draw,
    },
    {
        /*idname*/ "NLA_MT_snap_pie",
        /*label*/ N_("Snap"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ snap_pie_draw,
    },
    {
        /*idname*/ "NLA_MT_view_pie",
        /*label*/ N_("View"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ view_pie_draw,
    },
};

void nla_menus_register()
{
  flipendo::menus_register({nla_menus, ARRAY_SIZE(nla_menus)});
}

}  // namespace blender::ed::nla
