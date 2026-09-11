/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spbuttons
 *
 * La pestaña Mundo del editor de Propiedades, en C++ nativo. Sustituye
 * `scripts/startup/bl_ui/properties_world.py` completa: once paneles, incluidos
 * los selectores de animación, nodos y propiedades personalizadas.
 */

#include <algorithm>
#include <string>
#include <vector>

#include "DNA_ID.h"
#include "DNA_anim_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_world_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_anim_data.hh"
#include "BKE_context.hh"
#include "BKE_idprop.hh"
#include "BKE_node_runtime.hh"
#include "BKE_screen.hh"

#include "NOD_shader.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "FL_properties_ui.hpp"
#include "FL_ui_registry.hh"

#include "buttons_intern.hh"

static World *context_world(const bContext *C)
{
  return static_cast<World *>(CTX_data_pointer_get_type(C, "world", &RNA_World).data);
}

static PointerRNA world_ptr(World *world)
{
  return RNA_pointer_create_discrete(&world->id, &RNA_World, world);
}

static bool engine_is(const bContext *C, const char *identifier)
{
  const Scene *scene = CTX_data_scene(C);
  return scene != nullptr && STREQ(scene->r.engine, identifier);
}

static bool world_poll(const bContext *C, PanelType * /*pt*/)
{
  return context_world(C) != nullptr &&
         (engine_is(C, "BLENDER_RENDER") || engine_is(C, "BLENDER_EEVEE_NEXT") ||
          engine_is(C, "BLENDER_WORKBENCH"));
}

static bool world_context_poll(const bContext *C, PanelType * /*pt*/)
{
  return engine_is(C, "BLENDER_RENDER") || engine_is(C, "BLENDER_EEVEE_NEXT") ||
         engine_is(C, "BLENDER_WORKBENCH");
}

static bool eevee_world_poll(const bContext *C, PanelType * /*pt*/)
{
  return context_world(C) != nullptr && engine_is(C, "BLENDER_EEVEE_NEXT");
}

static bool eevee_world_nodes_poll(const bContext *C, PanelType * /*pt*/)
{
  World *world = context_world(C);
  return world != nullptr && world->use_nodes && engine_is(C, "BLENDER_EEVEE_NEXT");
}

static bool world_only_poll(const bContext *C, PanelType * /*pt*/)
{
  return context_world(C) != nullptr;
}

static bNodeSocket *node_input_find(bNode *node, const char *name)
{
  if (node == nullptr) {
    return nullptr;
  }
  LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
    if (STREQ(socket->name, name)) {
      return socket;
    }
  }
  return nullptr;
}

/* La función homónima de `bl_ui/anim.py`, sin cruzar el puente a Python. */
static void draw_action_and_slot_selector(const bContext *C, uiLayout *layout, ID *id)
{
  uiTemplateAction(layout, C, id, "ACTION_OT_new", "ACTION_OT_unlink", std::nullopt);

  AnimData *adt = BKE_animdata_from_id(id);
  if (adt == nullptr || adt->action == nullptr) {
    return;
  }
  PointerRNA action = RNA_id_pointer_create(&adt->action->id);
  if (!RNA_boolean_get(&action, "is_action_layered")) {
    return;
  }

  PointerRNA adt_ptr = RNA_pointer_create_discrete(id, &RNA_AnimData, adt);
  PointerRNA id_ptr = RNA_id_pointer_create(id);
  uiLayoutSetContextPointer(layout, "animated_id", &id_ptr);
  uiTemplateSearch(layout,
                   C,
                   &adt_ptr,
                   "action_slot",
                   &adt_ptr,
                   "action_suitable_slots",
                   "ANIM_OT_slot_new_for_id",
                   "ANIM_OT_slot_unassign_from_id");
}

/* Núcleo de `rna_prop_ui.draw()` para un ID. Mantiene también las ramas que la
 * escena de fábrica no ejercita: valores complejos, punteros ID y edición. */
static void draw_custom_properties(const bContext *C,
                                   uiLayout *layout,
                                   PointerRNA *ptr,
                                   ID *id,
                                   const char *data_path)
{
  const bool use_edit = !ID_IS_LINKED(id) && !ID_IS_OVERRIDE_LIBRARY_REAL(id);
  if (use_edit) {
    uiLayout *row = &layout->row(false);
    PointerRNA op = row->op("WM_OT_properties_add", IFACE_("New"), ICON_ADD);
    RNA_string_set(&op, "data_path", data_path);
    layout->separator();
  }
  uiLayoutSetPropDecorate(layout, false);

  IDProperty *group = IDP_GetProperties(id);
  if (group == nullptr) {
    return;
  }

  std::vector<IDProperty *> properties;
  LISTBASE_FOREACH (IDProperty *, property, &group->data.group) {
    properties.push_back(property);
  }
  std::sort(properties.begin(), properties.end(), [](const IDProperty *a, const IDProperty *b) {
    return strcmp(a->name, b->name) < 0;
  });

  for (IDProperty *property : properties) {
    const std::string property_path = "[\"" + BLI_str_escape(property->name) + "\"]";
    uiLayout *split = &layout->split(0.4f, true);
    uiLayout *label_row = &split->row(false);
    uiLayoutSetAlignment(label_row, UI_LAYOUT_ALIGN_RIGHT);
    label_row->label(property->name, ICON_NONE);

    uiLayout *value_row = &split->row(true);
    uiLayout *value_column = &value_row->column(true);
    const bool edit_value = property->type == IDP_GROUP ||
                            (property->type == IDP_ARRAY && property->len >= 8);
    if (edit_value) {
      PointerRNA op = value_column->op(
          "WM_OT_properties_edit_value", IFACE_("Edit Value"), ICON_NONE);
      RNA_string_set(&op, "data_path", data_path);
      RNA_string_set(&op, "property_name", property->name);
    }
    else if (property->type == IDP_ID) {
      uiTemplateID(value_column,
                   C,
                   ptr,
                   property_path,
                   nullptr,
                   nullptr,
                   nullptr,
                   UI_TEMPLATE_ID_FILTER_ALL,
                   false,
                   "");
    }
    else {
      value_column->prop(ptr, property_path, UI_ITEM_NONE, "", ICON_NONE);
    }

    if (use_edit) {
      uiLayout *operator_row = &value_row->row(true);
      uiLayoutSetAlignment(operator_row, UI_LAYOUT_ALIGN_RIGHT);
      uiLayoutSetEmboss(operator_row, blender::ui::EmbossType::None);
      PointerRNA edit = operator_row->op("WM_OT_properties_edit", "", ICON_PREFERENCES);
      RNA_string_set(&edit, "data_path", data_path);
      RNA_string_set(&edit, "property_name", property->name);
      PointerRNA remove = operator_row->op("WM_OT_properties_remove", "", ICON_X);
      RNA_string_set(&remove, "data_path", data_path);
      RNA_string_set(&remove, "property_name", property->name);
    }
  }
}

static void world_context_draw(const bContext *C, Panel *panel)
{
  Scene *scene = CTX_data_scene(C);
  World *world = context_world(C);
  if (scene != nullptr) {
    PointerRNA scene_ptr = RNA_pointer_create_discrete(&scene->id, &RNA_Scene, scene);
    uiTemplateID(panel->layout, C, &scene_ptr, "world", "WORLD_OT_new", nullptr, nullptr);
  }
  else if (world != nullptr) {
    SpaceProperties *space = CTX_wm_space_properties(C);
    PointerRNA space_ptr = RNA_pointer_create_discrete(
        reinterpret_cast<ID *>(CTX_wm_screen(C)), &RNA_SpaceProperties, space);
    uiTemplateID(panel->layout, C, &space_ptr, "pin_id", nullptr, nullptr, nullptr);
  }
}

static void world_mist_draw(const bContext *C, Panel *panel)
{
  PointerRNA world = world_ptr(context_world(C));
  PointerRNA mist = RNA_pointer_get(&world, "mist_settings");
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);
  uiLayout *col = &layout->column(true);
  col->prop(&mist, "start", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(&mist, "depth", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col = &layout->column(false);
  col->prop(&mist, "falloff", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void world_animation_draw(const bContext *C, Panel *panel)
{
  World *world = context_world(C);
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiLayout *col = &layout->column(true);
  col->label(IFACE_("World"), ICON_NONE);
  draw_action_and_slot_selector(C, col, &world->id);
  if (world->nodetree != nullptr) {
    col = &layout->column(true);
    col->label(IFACE_("Shader Node Tree"), ICON_NONE);
    draw_action_and_slot_selector(C, col, &world->nodetree->id);
  }
}

static void world_custom_props_draw(const bContext *C, Panel *panel)
{
  World *world = context_world(C);
  PointerRNA ptr = world_ptr(world);
  draw_custom_properties(C, panel->layout, &ptr, &world->id, "world");
}

static void world_surface_draw(const bContext *C, Panel *panel)
{
  World *world = context_world(C);
  PointerRNA ptr = world_ptr(world);
  uiLayout *layout = panel->layout;
  layout->prop(&ptr, "use_nodes", UI_ITEM_NONE, std::nullopt, ICON_NODETREE);
  layout->separator();
  uiLayoutSetPropSep(layout, true);
  if (!world->use_nodes) {
    layout->prop(&ptr, "color", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    return;
  }
  if (world->nodetree == nullptr) {
    layout->label(IFACE_("No output node"), ICON_NONE);
    return;
  }
  bNode *node = ntreeShaderOutputNode(world->nodetree, SHD_OUTPUT_EEVEE);
  bNodeSocket *input = node_input_find(node, "Surface");
  if (node == nullptr) {
    layout->label(IFACE_("No output node"), ICON_NONE);
  }
  else if (input == nullptr) {
    layout->label(IFACE_("Incompatible output node"), ICON_NONE);
  }
  else {
    uiTemplateNodeView(layout, const_cast<bContext *>(C), world->nodetree, node, input);
  }
}

static void world_volume_draw(const bContext *C, Panel *panel)
{
  World *world = context_world(C);
  PointerRNA ptr = world_ptr(world);
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);
  if (RNA_boolean_get(&ptr, "use_eevee_finite_volume")) {
    layout->op("WORLD_OT_convert_volume_to_mesh", IFACE_("Convert Volume"), ICON_WORLD_DATA);
  }
  if (world->nodetree == nullptr) {
    layout->label(IFACE_("No output node"), ICON_NONE);
    return;
  }
  bNode *node = ntreeShaderOutputNode(world->nodetree, SHD_OUTPUT_EEVEE);
  bNodeSocket *input = node_input_find(node, "Volume");
  if (node == nullptr) {
    layout->label(IFACE_("No output node"), ICON_NONE);
  }
  else if (input == nullptr) {
    layout->label(IFACE_("Incompatible output node"), ICON_NONE);
  }
  else {
    uiTemplateNodeView(layout, const_cast<bContext *>(C), world->nodetree, node, input);
  }
}

static void world_settings_draw(const bContext * /*C*/, Panel * /*panel*/) {}

static void world_lightprobe_draw(const bContext *C, Panel *panel)
{
  PointerRNA ptr = world_ptr(context_world(C));
  uiLayoutSetPropSep(panel->layout, true);
  panel->layout->prop(&ptr, "probe_resolution", UI_ITEM_NONE, IFACE_("Resolution"), ICON_NONE);
}

static void world_sun_draw(const bContext *C, Panel *panel)
{
  PointerRNA ptr = world_ptr(context_world(C));
  uiLayoutSetPropSep(panel->layout, true);
  panel->layout->prop(&ptr, "sun_threshold", UI_ITEM_NONE, IFACE_("Threshold"), ICON_NONE);
  panel->layout->prop(&ptr, "sun_angle", UI_ITEM_NONE, IFACE_("Angle"), ICON_NONE);
}

static void world_sun_shadow_header(const bContext *C, Panel *panel)
{
  PointerRNA ptr = world_ptr(context_world(C));
  panel->layout->prop(&ptr, "use_sun_shadow", UI_ITEM_NONE, "", ICON_NONE);
}

static void world_sun_shadow_draw(const bContext *C, Panel *panel)
{
  PointerRNA ptr = world_ptr(context_world(C));
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);
  uiLayout *col = &layout->column(false, IFACE_("Jitter"));
  uiLayout *row = &col->row(true);
  uiLayout *sub = &row->row(true);
  sub->prop(&ptr, "use_sun_shadow_jitter", UI_ITEM_NONE, "", ICON_NONE);
  sub = &sub->row(true);
  uiLayoutSetActive(sub, RNA_boolean_get(&ptr, "use_sun_shadow_jitter"));
  sub->prop(&ptr, "sun_shadow_jitter_overblur", UI_ITEM_NONE, IFACE_("Overblur"), ICON_NONE);
  col->separator();
  col = &layout->column(false);
  col->prop(&ptr, "sun_shadow_filter_radius", UI_ITEM_NONE, IFACE_("Filter"), ICON_NONE);
  col->prop(
      &ptr, "sun_shadow_maximum_resolution", UI_ITEM_NONE, IFACE_("Resolution Limit"), ICON_NONE);
}

void flipendo::properties_ui::world_viewport_display_draw(const bContext *C, Panel *panel)
{
  PointerRNA ptr = world_ptr(context_world(C));
  uiLayoutSetPropSep(panel->layout, true);
  panel->layout->prop(&ptr, "color", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

void fl_world_buttons_register(ARegionType *art)
{
  static const flipendo::PanelDecl panels[] = {
      {"WORLD_PT_context_world", "", nullptr, "world", nullptr, nullptr, nullptr,
       world_context_draw, nullptr, nullptr, world_context_poll, PANEL_TYPE_NO_HEADER},
      {"EEVEE_WORLD_PT_surface", N_("Surface"), nullptr, "world", nullptr, nullptr, nullptr,
       world_surface_draw, nullptr, nullptr, eevee_world_poll},
      {"EEVEE_WORLD_PT_volume", N_("Volume"), nullptr, "world", nullptr, nullptr,
       BLT_I18NCONTEXT_ID_ID, world_volume_draw, nullptr, nullptr, eevee_world_nodes_poll,
       PANEL_TYPE_DEFAULT_CLOSED},
      {"EEVEE_WORLD_PT_mist", N_("Mist Pass"), nullptr, "world", nullptr, nullptr, nullptr,
       world_mist_draw, nullptr, nullptr, eevee_world_poll, PANEL_TYPE_DEFAULT_CLOSED},
      {"EEVEE_WORLD_PT_settings", N_("Settings"), nullptr, "world", nullptr, nullptr, nullptr,
       world_settings_draw, nullptr, nullptr, eevee_world_poll, PANEL_TYPE_DEFAULT_CLOSED},
      {"EEVEE_WORLD_PT_lightprobe", N_("Light Probe"), nullptr, "world",
       "EEVEE_WORLD_PT_settings", nullptr, nullptr, world_lightprobe_draw, nullptr, nullptr,
       eevee_world_poll},
      {"EEVEE_WORLD_PT_sun", N_("Sun"), nullptr, "world", "EEVEE_WORLD_PT_settings", nullptr,
       nullptr, world_sun_draw, nullptr, nullptr, eevee_world_poll},
      {"EEVEE_WORLD_PT_sun_shadow", N_("Shadow"), nullptr, "world", "EEVEE_WORLD_PT_sun",
       nullptr, nullptr, world_sun_shadow_draw, world_sun_shadow_header, nullptr, eevee_world_poll,
       PANEL_TYPE_DEFAULT_CLOSED},
      {"WORLD_PT_viewport_display", N_("Viewport Display"), nullptr, "world", nullptr, nullptr,
       nullptr, flipendo::properties_ui::world_viewport_display_draw, nullptr, nullptr,
       world_only_poll,
       PANEL_TYPE_DEFAULT_CLOSED, 10},
      {"WORLD_PT_animation", N_("Animation"), nullptr, "world", nullptr, nullptr, nullptr,
       world_animation_draw, nullptr, nullptr, world_poll, PANEL_TYPE_DEFAULT_CLOSED, 999},
      {"WORLD_PT_custom_props", N_("Custom Properties"), nullptr, "world", nullptr, nullptr,
       nullptr, world_custom_props_draw, nullptr, nullptr, world_poll,
       PANEL_TYPE_DEFAULT_CLOSED, 1000},
  };
  flipendo::panels_register(art, SPACE_PROPERTIES, {panels, ARRAY_SIZE(panels)});
}
