/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spbuttons
 *
 * La pestana de datos del objeto Vacio, en C++ nativo. Sustituye
 * `scripts/startup/bl_ui/properties_data_empty.py` **entera**: sus dos paneles.
 *
 * Se migra la pestana completa y no un panel suelto, por lo que dice
 * politicas/UI-A-CPP.md: un panel aislado cambia de posicion dentro de la region
 * y el volcado marca diferencia aunque no cambie ni un campo.
 */

#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_utildefines.h"

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

static Object *context_empty(const bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  return (ob != nullptr && ob->type == OB_EMPTY) ? ob : nullptr;
}

static PointerRNA object_ptr(Object *ob)
{
  return RNA_pointer_create_discrete(&ob->id, &RNA_Object, ob);
}

/** `poll` de `DataButtonsPanel`: `ob and ob.type == 'EMPTY'`. */
static bool empty_poll(const bContext *C, PanelType * /*pt*/)
{
  return context_empty(C) != nullptr;
}

/** `poll` del panel de imagen: ademas `ob.empty_display_type == 'IMAGE'`. */
static bool empty_image_poll(const bContext *C, PanelType * /*pt*/)
{
  const Object *ob = context_empty(C);
  return ob != nullptr && ob->empty_drawtype == OB_EMPTY_IMAGE;
}

static void empty_draw(const bContext *C, Panel *panel)
{
  Object *ob = context_empty(C);
  if (ob == nullptr) {
    return;
  }
  PointerRNA ptr = object_ptr(ob);
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);

  layout->prop(&ptr, "empty_display_type", UI_ITEM_NONE, IFACE_("Display As"), ICON_NONE);
  layout->prop(&ptr, "empty_display_size", UI_ITEM_NONE, IFACE_("Size"), ICON_NONE);

  if (ob->empty_drawtype != OB_EMPTY_IMAGE) {
    return;
  }

  PropertyRNA *prop_offset = RNA_struct_find_property(&ptr, "empty_image_offset");
  PropertyRNA *prop_color = RNA_struct_find_property(&ptr, "color");

  uiLayout *col = &layout->column(true);
  col->prop(&ptr, prop_offset, 0, 0, UI_ITEM_NONE, IFACE_("Offset X"), ICON_NONE);
  col->prop(&ptr, prop_offset, 1, 0, UI_ITEM_NONE, IFACE_("Y"), ICON_NONE);

  col = &layout->column(false);
  uiLayout *depth_row = &col->row(false);
  /* `depth_row.enabled = not ob.show_in_front`. */
  uiLayoutSetEnabled(depth_row, (ob->dtx & OB_DRAW_IN_FRONT) == 0);
  depth_row->prop(&ptr, "empty_image_depth", UI_ITEM_R_EXPAND, IFACE_("Depth"), ICON_NONE);
  col->row(false).prop(&ptr, "empty_image_side", UI_ITEM_R_EXPAND, IFACE_("Side"), ICON_NONE);

  col = &layout->column(true, IFACE_("Show In"));
  col->prop(
      &ptr, "show_empty_image_orthographic", UI_ITEM_NONE, IFACE_("Orthographic"), ICON_NONE);
  col->prop(&ptr, "show_empty_image_perspective", UI_ITEM_NONE, IFACE_("Perspective"), ICON_NONE);
  col->prop(&ptr,
            "show_empty_image_only_axis_aligned",
            UI_ITEM_NONE,
            IFACE_("Only Axis Aligned"),
            ICON_NONE);

  /* `column(align=False, heading="Opacity")` con decoradores apagados: el
   * decorador lo pone a mano `row.prop_decorator(ob, "color", index=3)`, y si se
   * dejaran encendidos saldrian dos. */
  col = &layout->column(false, IFACE_("Opacity"));
  uiLayoutSetPropDecorate(col, false);
  uiLayout *row = &col->row(true);
  uiLayout *sub = &row->row(true);
  sub->prop(&ptr, "use_empty_image_alpha", UI_ITEM_NONE, "", ICON_NONE);
  sub = &sub->row(true);
  uiLayoutSetActive(sub, (ob->empty_image_flag & OB_EMPTY_IMAGE_USE_ALPHA_BLEND) != 0);
  sub->prop(&ptr, prop_color, 3, 0, UI_ITEM_R_SLIDER, "", ICON_NONE);
  uiItemDecoratorR_prop(row, &ptr, prop_color, 3);
}

static void empty_image_draw(const bContext *C, Panel *panel)
{
  Object *ob = context_empty(C);
  if (ob == nullptr) {
    return;
  }
  PointerRNA ptr = object_ptr(ob);
  uiLayout *layout = panel->layout;

  uiTemplateID(layout, C, &ptr, "data", nullptr, "IMAGE_OT_open", "OBJECT_OT_unlink_data");
  layout->separator();

  PointerRNA iuser_ptr = RNA_pointer_get(&ptr, "image_user");
  uiTemplateImage(layout, const_cast<bContext *>(C), &ptr, "data", &iuser_ptr, true, false);
}

void fl_properties_data_empty_register(ARegionType *art)
{
  static const flipendo::PanelDecl panels[] = {
      {
          /*idname*/ "DATA_PT_empty",
          /*label*/ N_("Empty"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ BLT_I18NCONTEXT_ID_ID,
          /*draw*/ empty_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ empty_poll,
      },
      {
          /*idname*/ "DATA_PT_empty_image",
          /*label*/ N_("Image"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ empty_image_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ empty_image_poll,
      },
  };
  flipendo::panels_register(art, SPACE_PROPERTIES, {panels, ARRAY_SIZE(panels)});
}
