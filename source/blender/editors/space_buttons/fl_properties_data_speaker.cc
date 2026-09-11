/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spbuttons
 *
 * La pestana de datos del Altavoz, en C++ nativo. Sustituye
 * `scripts/startup/bl_ui/properties_data_speaker.py` **entera**: sus seis
 * paneles. Por pestanas completas, nunca panel suelto (`UI-A-CPP.md`).
 *
 * A diferencia de Metaball y Rejilla, esta pestana **si** lleva `COMPAT_ENGINES`:
 * los seis paneles solo salen con `BLENDER_RENDER`, `BLENDER_EEVEE_NEXT` o
 * `BLENDER_WORKBENCH`. Se mira donde lo miraba el Python, en
 * `scene.render.engine`, igual que hace `fl_game_buttons.cc`.
 */

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_speaker_types.h"

#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "FL_properties_ui.hpp"
#include "FL_ui_registry.hh"

#include "buttons_intern.hh"

static Speaker *context_speaker(const bContext *C)
{
  return static_cast<Speaker *>(CTX_data_pointer_get_type(C, "speaker", &RNA_Speaker).data);
}

static PointerRNA speaker_ptr(Speaker *speaker)
{
  return RNA_pointer_create_discrete(&speaker->id, &RNA_Speaker, speaker);
}

/**
 * `poll` de `DataButtonsPanel`:
 * `context.speaker and (engine in cls.COMPAT_ENGINES)`.
 *
 * Los seis paneles declaran el mismo `COMPAT_ENGINES`, asi que basta un `poll`.
 */
static bool speaker_poll(const bContext *C, PanelType * /*pt*/)
{
  if (context_speaker(C) == nullptr) {
    return false;
  }
  const Scene *scene = CTX_data_scene(C);
  if (scene == nullptr) {
    return false;
  }
  const char *engine = scene->r.engine;
  return STREQ(engine, "BLENDER_RENDER") || STREQ(engine, "BLENDER_EEVEE_NEXT") ||
         STREQ(engine, "BLENDER_WORKBENCH");
}

/** `not speaker.muted`, o sea el bit `SPK_MUTED` al reves. */
static bool speaker_not_muted(const Speaker *speaker)
{
  return (speaker->flag & SPK_MUTED) == 0;
}

static void context_speaker_draw(const bContext *C, Panel *panel)
{
  Object *ob = CTX_data_active_object(C);
  uiLayout *layout = panel->layout;
  if (ob != nullptr) {
    PointerRNA ob_ptr = RNA_pointer_create_discrete(&ob->id, &RNA_Object, ob);
    uiTemplateID(layout, C, &ob_ptr, "data", nullptr, nullptr, nullptr);
  }
  else if (context_speaker(C) != nullptr) {
    SpaceProperties *space = CTX_wm_space_properties(C);
    PointerRNA space_ptr = RNA_pointer_create_discrete(
        reinterpret_cast<ID *>(CTX_wm_screen(C)), &RNA_SpaceProperties, space);
    uiTemplateID(layout, C, &space_ptr, "pin_id", nullptr, nullptr, nullptr);
  }
}

static void speaker_draw(const bContext *C, Panel *panel)
{
  Speaker *speaker = context_speaker(C);
  if (speaker == nullptr) {
    return;
  }
  PointerRNA ptr = speaker_ptr(speaker);
  uiLayout *layout = panel->layout;

  /* Ojo al orden: el `template_ID` va ANTES de `use_property_split = True`. Si se
   * pone la separacion primero, el selector de sonido se dibuja partido y el
   * volcado lo canta. `open="sound.open_mono"` es el operador de ABRIR, que en
   * la firma C++ es el tercer parametro, no el segundo (que es el de NUEVO). */
  uiTemplateID(layout, C, &ptr, "sound", nullptr, "SOUND_OT_open_mono", nullptr);

  uiLayoutSetPropSep(layout, true);

  layout->prop(&ptr, "muted", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  uiLayout *col = &layout->column(false);
  uiLayoutSetActive(col, speaker_not_muted(speaker));
  col->prop(&ptr, "volume", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);
  col->prop(&ptr, "pitch", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void distance_draw(const bContext *C, Panel *panel)
{
  Speaker *speaker = context_speaker(C);
  if (speaker == nullptr) {
    return;
  }
  PointerRNA ptr = speaker_ptr(speaker);
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);

  /* `layout.active`, sobre el layout raiz del panel y no sobre la columna. */
  uiLayoutSetActive(layout, speaker_not_muted(speaker));

  uiLayout *col = &layout->column(false);
  uiLayout *sub = &col->column(true);
  sub->prop(&ptr, "volume_min", UI_ITEM_R_SLIDER, IFACE_("Volume Min"), ICON_NONE);
  sub->prop(&ptr, "volume_max", UI_ITEM_R_SLIDER, IFACE_("Max"), ICON_NONE);
  col->prop(&ptr, "attenuation", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  col->separator();
  col->prop(&ptr, "distance_max", UI_ITEM_NONE, IFACE_("Max Distance"), ICON_NONE);
  col->prop(&ptr, "distance_reference", UI_ITEM_NONE, IFACE_("Distance Reference"), ICON_NONE);
}

static void cone_draw(const bContext *C, Panel *panel)
{
  Speaker *speaker = context_speaker(C);
  if (speaker == nullptr) {
    return;
  }
  PointerRNA ptr = speaker_ptr(speaker);
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);

  uiLayoutSetActive(layout, speaker_not_muted(speaker));

  uiLayout *col = &layout->column(false);

  uiLayout *sub = &col->column(true);
  sub->prop(&ptr, "cone_angle_outer", UI_ITEM_NONE, IFACE_("Angle Outer"), ICON_NONE);
  sub->prop(&ptr, "cone_angle_inner", UI_ITEM_NONE, IFACE_("Inner"), ICON_NONE);

  col->separator();

  col->prop(&ptr, "cone_volume_outer", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);
}

static void speaker_animation_draw(const bContext *C, Panel *panel)
{
  Speaker *speaker = context_speaker(C);
  if (speaker == nullptr) {
    return;
  }
  flipendo::properties_ui::draw_animation_panel(C, panel->layout, &speaker->id);
}

static void speaker_custom_props_draw(const bContext *C, Panel *panel)
{
  Speaker *speaker = context_speaker(C);
  if (speaker == nullptr) {
    return;
  }
  PointerRNA ptr = speaker_ptr(speaker);
  flipendo::properties_ui::draw_custom_properties(
      C, panel->layout, &ptr, &speaker->id, "object.data");
}

void fl_properties_data_speaker_register(ARegionType *art)
{
  static const flipendo::PanelDecl panels[] = {
      {
          /*idname*/ "DATA_PT_context_speaker",
          /*label*/ "",
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ context_speaker_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ speaker_poll,
          /*flag*/ PANEL_TYPE_NO_HEADER,
      },
      {
          /*idname*/ "DATA_PT_speaker",
          /*label*/ N_("Sound"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ speaker_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ speaker_poll,
      },
      {
          /*idname*/ "DATA_PT_distance",
          /*label*/ N_("Distance"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ distance_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ speaker_poll,
          /*flag*/ PANEL_TYPE_DEFAULT_CLOSED,
      },
      {
          /*idname*/ "DATA_PT_cone",
          /*label*/ N_("Cone"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ cone_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ speaker_poll,
          /*flag*/ PANEL_TYPE_DEFAULT_CLOSED,
      },
      {
          /*idname*/ "DATA_PT_speaker_animation",
          /*label*/ N_("Animation"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ speaker_animation_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ speaker_poll,
          /*flag*/ PANEL_TYPE_DEFAULT_CLOSED,
          /* `PropertiesAnimationMixin.bl_order = PropertyPanel.bl_order - 1`. */
          /*order*/ 999,
      },
      {
          /*idname*/ "DATA_PT_custom_props_speaker",
          /*label*/ N_("Custom Properties"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ speaker_custom_props_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ speaker_poll,
          /*flag*/ PANEL_TYPE_DEFAULT_CLOSED,
          /*order*/ 1000,
      },
  };
  flipendo::panels_register(art, SPACE_PROPERTIES, {panels, ARRAY_SIZE(panels)});
}
