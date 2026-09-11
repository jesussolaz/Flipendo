/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spaction
 *
 * Ver FL_action_menus.hh. Los cinco menus de la hoja de exposicion que el
 * keymap nativo abre por nombre: el contextual de claves, el de canales (que el
 * editor de curvas tambien usa), el de borrar (X) y los dos radiales.
 */

#include <optional>

#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "DNA_action_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "FL_ui_registry.hh"

#include "FL_action_menus.hh"

namespace blender::ed::action {

/** `layout.operator(op, text=t).<prop> = <identificador>`. */
static void op_enum_item(
    uiLayout *layout, const char *opname, const char *text, const char *prop, const char *value)
{
  PointerRNA props = layout->op(
      opname, text ? IFACE_(text) : std::optional<StringRef>(std::nullopt), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, prop, value);
  }
}

/* -------------------------------------------------------------------- */
/** \name Borrar y radiales
 * \{ */

static void delete_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;

  layout->op("ACTION_OT_delete", std::nullopt, ICON_NONE);

  layout->separator();

  PointerRNA props = layout->op("ACTION_OT_clean", std::nullopt, ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "channels", false);
  }
  props = layout->op("ACTION_OT_clean", IFACE_("Clean Channels"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "channels", true);
  }
}

static void snap_pie_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  op_enum_item(&pie, "ACTION_OT_snap", N_("Selection to Current Frame"), "type", "CFRA");
  op_enum_item(&pie, "ACTION_OT_snap", N_("Selection to Nearest Frame"), "type", "NEAREST_FRAME");
  op_enum_item(&pie, "ACTION_OT_snap", N_("Selection to Nearest Second"), "type", "NEAREST_SECOND");
  op_enum_item(&pie, "ACTION_OT_snap", N_("Selection to Nearest Marker"), "type", "NEAREST_MARKER");
}

static void view_pie_draw(const bContext *C, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  pie.op("ACTION_OT_view_all", std::nullopt, ICON_NONE);
  pie.op("ACTION_OT_view_selected", std::nullopt, ICON_ZOOM_SELECTED);
  pie.op("ACTION_OT_view_frame", std::nullopt, ICON_NONE);

  const Scene *scene = CTX_data_scene(C);
  const bool preview_range = scene && (scene->r.flag & SCER_PRV_RANGE);
  pie.op("ANIM_OT_scene_range_frame",
         preview_range ? IFACE_("Frame Preview Range") : IFACE_("Frame Scene Range"),
         ICON_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name DOPESHEET_MT_context_menu
 * \{ */

static void context_menu_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  const SpaceAction *saction = CTX_wm_space_action(C);
  const bool is_gpencil = saction != nullptr && saction->mode == SACTCONT_GPENCIL;

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);

  layout->op("ACTION_OT_copy", IFACE_("Copy"), ICON_COPYDOWN);
  layout->op("ACTION_OT_paste", IFACE_("Paste"), ICON_PASTEDOWN);
  PointerRNA props = layout->op("ACTION_OT_paste", IFACE_("Paste Flipped"), ICON_PASTEFLIPDOWN);
  if (props.data) {
    RNA_boolean_set(&props, "flipped", true);
  }

  layout->separator();

  uiItemMenuEnumO(layout, C, "ACTION_OT_keyframe_type", "type", IFACE_("Keyframe Type"), ICON_NONE);

  if (!is_gpencil) {
    uiItemMenuEnumO(layout, C, "ACTION_OT_handle_type", "type", IFACE_("Handle Type"), ICON_NONE);
    uiItemMenuEnumO(
        layout, C, "ACTION_OT_interpolation_type", "type", IFACE_("Interpolation Mode"), ICON_NONE);
    uiItemMenuEnumO(layout, C, "ACTION_OT_easing_type", "type", IFACE_("Easing Mode"), ICON_NONE);
  }

  layout->separator();

  op_enum_item(layout, "ACTION_OT_keyframe_insert", nullptr, "type", "SEL");
  layout->op("ACTION_OT_duplicate_move", std::nullopt, ICON_NONE);

  if (is_gpencil) {
    layout->separator();
    layout->op("GREASE_PENCIL_OT_delete_breakdown", std::nullopt, ICON_NONE);
  }

  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_REGION_WIN);
  layout->op("ACTION_OT_delete", std::nullopt, ICON_NONE);

  layout->separator();

  uiItemMenuEnumO(layout, C, "ACTION_OT_mirror", "type", IFACE_("Mirror"), ICON_NONE);
  uiItemMenuEnumO(layout, C, "ACTION_OT_snap", "type", IFACE_("Snap"), ICON_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name DOPESHEET_MT_channel_context_menu
 *
 * Este menu lo usa tambien el editor de curvas; de ahi el `is_graph_editor`.
 * \{ */

static void channels_setting_item(
    uiLayout *layout, const char *opname, const char *text, const char *type)
{
  PointerRNA props = layout->op(opname, IFACE_(text), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "type", type);
  }
}

static void channel_context_menu_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  /* `context.area.type == 'GRAPH_EDITOR'`. */
  const ScrArea *area = CTX_wm_area(C);
  const bool is_graph_editor = area != nullptr && area->spacetype == SPACE_GRAPH;

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_CHANNELS);

  layout->separator();
  layout->op("ANIM_OT_channels_view_selected", std::nullopt, ICON_NONE);

  channels_setting_item(layout, "ANIM_OT_channels_setting_enable", N_("Mute Channels"), "MUTE");
  channels_setting_item(layout, "ANIM_OT_channels_setting_disable", N_("Unmute Channels"), "MUTE");
  layout->separator();
  channels_setting_item(
      layout, "ANIM_OT_channels_setting_enable", N_("Protect Channels"), "PROTECT");
  channels_setting_item(
      layout, "ANIM_OT_channels_setting_disable", N_("Unprotect Channels"), "PROTECT");

  layout->separator();
  layout->op("ANIM_OT_channels_group", std::nullopt, ICON_NONE);
  layout->op("ANIM_OT_channels_ungroup", std::nullopt, ICON_NONE);

  layout->separator();
  layout->op("ANIM_OT_channels_editable_toggle", std::nullopt, ICON_NONE);

  uiItemMenuEnumO(layout,
                  C,
                  is_graph_editor ? "GRAPH_OT_extrapolation_type" : "ACTION_OT_extrapolation_type",
                  "type",
                  IFACE_("Extrapolation Mode"),
                  ICON_NONE);

  if (is_graph_editor) {
    if (wmOperatorType *ot = WM_operatortype_find("GRAPH_OT_fmodifier_add", false)) {
      PointerRNA props;
      uiItemMenuEnumFullO_ptr(
          layout, C, ot, "type", IFACE_("Add F-Curve Modifier"), ICON_NONE, &props);
      if (props.data) {
        RNA_boolean_set(&props, "only_active", false);
      }
    }
    layout->separator();
    PointerRNA props = layout->op("GRAPH_OT_hide", IFACE_("Hide Selected Curves"), ICON_NONE);
    if (props.data) {
      RNA_boolean_set(&props, "unselected", false);
    }
    props = layout->op("GRAPH_OT_hide", IFACE_("Hide Unselected Curves"), ICON_NONE);
    if (props.data) {
      RNA_boolean_set(&props, "unselected", true);
    }
    layout->op("GRAPH_OT_reveal", std::nullopt, ICON_NONE);
  }

  layout->separator();
  layout->op("ANIM_OT_channels_expand", std::nullopt, ICON_NONE);
  layout->op("ANIM_OT_channels_collapse", std::nullopt, ICON_NONE);

  layout->separator();
  uiItemMenuEnumO(layout, C, "ANIM_OT_channels_move", "direction", IFACE_("Move..."), ICON_NONE);

  layout->separator();

  layout->op("ANIM_OT_channels_delete", std::nullopt, ICON_NONE);

  if (is_graph_editor) {
    const SpaceGraph *sipo = CTX_wm_space_graph(C);
    if (sipo != nullptr && sipo->mode == SIPO_MODE_DRIVERS) {
      layout->op("GRAPH_OT_driver_delete_invalid", std::nullopt, ICON_NONE);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registro
 * \{ */

static const flipendo::MenuDecl action_menus[] = {
    {
        /*idname*/ "DOPESHEET_MT_context_menu",
        /*label*/ N_("Dope Sheet"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ context_menu_draw,
    },
    {
        /*idname*/ "DOPESHEET_MT_channel_context_menu",
        /*label*/ N_("Channel"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ channel_context_menu_draw,
    },
    {
        /*idname*/ "DOPESHEET_MT_delete",
        /*label*/ N_("Delete"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ delete_draw,
    },
    {
        /*idname*/ "DOPESHEET_MT_snap_pie",
        /*label*/ N_("Snap"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ snap_pie_draw,
    },
    {
        /*idname*/ "DOPESHEET_MT_view_pie",
        /*label*/ N_("View"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ view_pie_draw,
    },
};

void action_menus_register()
{
  flipendo::menus_register({action_menus, ARRAY_SIZE(action_menus)});
}

/** \} */

}  // namespace blender::ed::action
