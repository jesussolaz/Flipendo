/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spbuttons
 *
 * El panel de Nodos de simulacion de la pestana Fisicas, en C++ nativo.
 * Sustituye `scripts/startup/bl_ui/properties_physics_geometry_nodes.py`
 * **entera**: es la unidad completa, un solo panel y ninguna dependencia
 * compartida.
 */

#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "FL_ui_registry.hh"

#include "buttons_intern.hh"

static bool object_has_nodes_modifier(const Object *ob)
{
  LISTBASE_FOREACH (const ModifierData *, md, &ob->modifiers) {
    if (md->type == eModifierType_Nodes) {
      return true;
    }
  }
  return false;
}

/**
 * `geometry_nodes_objects()` + `poll()` del Python: al menos un objeto
 * seleccionado y editable con un modificador de nodos.
 */
static bool geometry_nodes_poll(const bContext *C, PanelType * /*pt*/)
{
  blender::Vector<PointerRNA> objects;
  CTX_data_selected_editable_objects(C, &objects);
  for (const PointerRNA &ptr : objects) {
    const Object *ob = static_cast<const Object *>(ptr.data);
    if (ob != nullptr && object_has_nodes_modifier(ob)) {
      return true;
    }
  }
  return false;
}

static void geometry_nodes_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  blender::Vector<PointerRNA> objects;
  CTX_data_selected_editable_objects(C, &objects);
  /* El Python mira `len(context.selected_editable_objects) > 1`, no cuantos de
   * ellos tienen nodos: se reproduce tal cual. */
  const bool many = objects.size() > 1;
  const char *calc_text = many ? IFACE_("Calculate Selected to Frame") :
                                 IFACE_("Calculate to Frame");
  const char *bake_text = many ? IFACE_("Bake Selected") : IFACE_("Bake");

  PointerRNA op_ptr = layout->op(
      "OBJECT_OT_simulation_nodes_cache_calculate_to_frame", calc_text, ICON_NONE);
  if (op_ptr.data != nullptr) {
    RNA_boolean_set(&op_ptr, "selected", true);
  }

  uiLayout *row = &layout->row(true);
  op_ptr = row->op("OBJECT_OT_simulation_nodes_cache_bake", bake_text, ICON_NONE);
  if (op_ptr.data != nullptr) {
    RNA_boolean_set(&op_ptr, "selected", true);
  }
  op_ptr = row->op("OBJECT_OT_simulation_nodes_cache_delete", "", ICON_TRASH);
  if (op_ptr.data != nullptr) {
    RNA_boolean_set(&op_ptr, "selected", true);
  }

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  Object *ob = CTX_data_active_object(C);
  if (ob != nullptr) {
    PointerRNA ob_ptr = RNA_pointer_create_discrete(&ob->id, &RNA_Object, ob);
    /* `text_ctxt=i18n_contexts.id_simulation`. */
    layout->prop(&ob_ptr,
                 "use_simulation_cache",
                 UI_ITEM_NONE,
                 CTX_IFACE_(BLT_I18NCONTEXT_ID_SIMULATION, "Cache"),
                 ICON_NONE);
  }
}

void fl_properties_physics_geometry_nodes_register(ARegionType *art)
{
  static const flipendo::PanelDecl panels[] = {
      {
          /*idname*/ "PHYSICS_PT_geometry_nodes",
          /*label*/ N_("Simulation Nodes"),
          /*category*/ nullptr,
          /*context*/ "physics",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ geometry_nodes_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ geometry_nodes_poll,
          /*flag*/ PANEL_TYPE_DEFAULT_CLOSED,
      },
  };
  flipendo::panels_register(art, SPACE_PROPERTIES, {panels, ARRAY_SIZE(panels)});
}
