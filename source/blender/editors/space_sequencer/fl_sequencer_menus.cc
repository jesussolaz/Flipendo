/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spseq
 *
 * Ver FL_sequencer_menus.hh. Seis de los ocho menus que el keymap nativo abre
 * por nombre en el secuenciador: los tres radiales, el contextual de la
 * previsualizacion, el de cambiar y el de retemporizacion.
 *
 * Los dos que faltan — `SEQUENCER_MT_add` (63 lineas) y
 * `SEQUENCER_MT_context_menu` (121) — van en su propia tanda: son tablas largas
 * con muchas ramas por tipo de tira, y mezclarlas aqui haria el commit
 * imposible de revisar.
 */

#include <optional>

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_main.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "FL_ui_registry.hh"

#include "FL_sequencer_menus.hh"

namespace blender::ed::sequencer {

/** `context.tool_settings.sequencer_tool_settings`. */
static PointerRNA sequencer_tool_settings_ptr(const bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  if (scene == nullptr || ts == nullptr) {
    return PointerRNA_NULL;
  }
  PointerRNA ts_ptr = RNA_pointer_create_id_subdata(scene->id, &RNA_ToolSettings, ts);
  return RNA_pointer_get(&ts_ptr, "sequencer_tool_settings");
}

/* -------------------------------------------------------------------- */
/** \name Radiales
 * \{ */

static void pivot_pie_draw(const bContext *C, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  PointerRNA sts = sequencer_tool_settings_ptr(C);
  if (sts.data == nullptr) {
    return;
  }
  uiItemEnumR_string(&pie, &sts, "pivot_point", "CENTER", std::nullopt, ICON_NONE);
  uiItemEnumR_string(&pie, &sts, "pivot_point", "CURSOR", std::nullopt, ICON_NONE);
  uiItemEnumR_string(&pie, &sts, "pivot_point", "INDIVIDUAL_ORIGINS", std::nullopt, ICON_NONE);
  uiItemEnumR_string(&pie, &sts, "pivot_point", "MEDIAN", std::nullopt, ICON_NONE);
}

static void view_pie_draw(const bContext *C, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  pie.op("SEQUENCER_OT_view_all", std::nullopt, ICON_NONE);
  pie.op("SEQUENCER_OT_view_selected", IFACE_("Frame Selected"), ICON_ZOOM_SELECTED);
  pie.separator();

  const Scene *scene = CTX_data_scene(C);
  const bool preview_range = scene && (scene->r.flag & SCER_PRV_RANGE);
  pie.op("ANIM_OT_scene_range_frame",
         preview_range ? IFACE_("Frame Preview Range") : IFACE_("Frame Scene Range"),
         ICON_NONE);
}

static void preview_view_pie_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  uiLayoutSetOperatorContext(&pie, WM_OP_INVOKE_REGION_PREVIEW);
  pie.op("SEQUENCER_OT_view_all_preview", std::nullopt, ICON_NONE);
  pie.op("SEQUENCER_OT_view_selected", IFACE_("Frame Selected"), ICON_ZOOM_SELECTED);
  pie.separator();
  PointerRNA props = pie.op("SEQUENCER_OT_view_zoom_ratio", IFACE_("Zoom 1:1"), ICON_NONE);
  if (props.data) {
    RNA_float_set(&props, "ratio", 1.0f);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Contextual de la previsualizacion y retemporizacion
 * \{ */

static void preview_context_menu_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);

  PointerRNA props = layout->op("WM_OT_call_panel", IFACE_("Rename..."), ICON_NONE);
  if (props.data) {
    RNA_string_set(&props, "name", "TOPBAR_PT_name");
    RNA_boolean_set(&props, "keep_open", false);
  }
}

static void retiming_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;
  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);

  layout->op("SEQUENCER_OT_retiming_key_add", std::nullopt, ICON_NONE);
  layout->op("SEQUENCER_OT_retiming_add_freeze_frame_slide", std::nullopt, ICON_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name SEQUENCER_MT_change
 * \{ */

static void change_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  /* `context.active_strip`. */
  PointerRNA strip = CTX_data_pointer_get_type(C, "active_strip", &RNA_Strip);

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);

  const bool is_scene_strip = strip.data && RNA_enum_get(&strip, "type") == STRIP_TYPE_SCENE;
  if (is_scene_strip) {
    const Main *bmain = CTX_data_main(C);
    const int scenes_num = bmain ? BLI_listbase_count(&bmain->scenes) : 0;
    if (scenes_num > 10) {
      uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);
      layout->op("SEQUENCER_OT_change_scene", IFACE_("Change Scene..."), ICON_NONE);
    }
    else if (scenes_num > 1) {
      uiItemMenuEnumO(
          layout, C, "SEQUENCER_OT_change_scene", "scene", IFACE_("Change Scene"), ICON_NONE);
    }
  }

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);
  layout->menu("SEQUENCER_MT_strip_effect_change", std::nullopt, ICON_NONE);
  layout->op("SEQUENCER_OT_swap_inputs", std::nullopt, ICON_NONE);
  PointerRNA props = layout->op("SEQUENCER_OT_change_path", IFACE_("Path/Files"), ICON_NONE);

  if (strip.data && props.data) {
    switch (RNA_enum_get(&strip, "type")) {
      case STRIP_TYPE_IMAGE:
        RNA_boolean_set(&props, "filter_image", true);
        break;
      case STRIP_TYPE_MOVIE:
        RNA_boolean_set(&props, "filter_movie", true);
        break;
      case STRIP_TYPE_SOUND_RAM:
        RNA_boolean_set(&props, "filter_sound", true);
        break;
      default:
        break;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registro
 * \{ */

static const flipendo::MenuDecl sequencer_menus[] = {
    {
        /*idname*/ "SEQUENCER_MT_change",
        /*label*/ N_("Change"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ change_draw,
    },
    {
        /*idname*/ "SEQUENCER_MT_retiming",
        /*label*/ N_("Retiming"),
        /*description*/ nullptr,
        /*translation_context*/ BLT_I18NCONTEXT_OPERATOR_DEFAULT,
        /*draw*/ retiming_draw,
    },
    {
        /*idname*/ "SEQUENCER_MT_preview_context_menu",
        /*label*/ N_("Sequencer Preview"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ preview_context_menu_draw,
    },
    {
        /*idname*/ "SEQUENCER_MT_pivot_pie",
        /*label*/ N_("Pivot Point"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ pivot_pie_draw,
    },
    {
        /*idname*/ "SEQUENCER_MT_view_pie",
        /*label*/ N_("View"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ view_pie_draw,
    },
    {
        /*idname*/ "SEQUENCER_MT_preview_view_pie",
        /*label*/ N_("View"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ preview_view_pie_draw,
    },
};

void sequencer_menus_register()
{
  flipendo::menus_register({sequencer_menus, ARRAY_SIZE(sequencer_menus)});
}

/** \} */

}  // namespace blender::ed::sequencer
