/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup splogic
 *
 * Interfaz del editor de logica, en C++ nativo.
 *
 * Sustituye a `scripts/startup/bl_ui/space_logic.py` (145 lineas): el panel de
 * propiedades de juego, el menu de anadir bricks, el menu de vista, el menu de
 * cabecera y la propia cabecera. Mismos `idname`, mismas etiquetas, mismos
 * operadores: para el usuario no cambia nada.
 *
 * Es el piloto de la migracion de `bl_ui` a C++ — cubre los tres mecanismos de
 * registro (panel, menu y cabecera) sobre `FL_ui_registry`, y es dominio propio
 * de Flipendo.
 *
 * Doctrina: politicas/LENGUAJE-CPP.md.
 */

#include "DNA_object_types.h"
#include "DNA_property_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "ED_screen.hh"

#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "FL_ui_registry.hh"

#include "logic_intern.hh"

/* -------------------------------------------------------------------- */
/** \name Panel de propiedades de juego
 * \{ */

static bool logic_panel_properties_poll(const bContext *C, PanelType * /*pt*/)
{
  const Object *ob = CTX_data_active_object(C);
  return ob != nullptr;
}

/* Fila de una propiedad: nombre, tipo, valor, depuracion, mover y borrar. */
static void logic_property_row_draw(uiLayout *layout, PointerRNA *prop_ptr, const int index)
{
  uiLayout *box = &layout->box();
  uiLayout *row = &box->row(false);

  row->prop(prop_ptr, "name", UI_ITEM_NONE, "", ICON_NONE);
  row->prop(prop_ptr, "type", UI_ITEM_NONE, "", ICON_NONE);
  row->prop(prop_ptr, "value", UI_ITEM_NONE, "", ICON_NONE);
  row->prop(prop_ptr, "show_debug", UI_ITEM_R_TOGGLE, "", ICON_INFO);

  uiLayout *sub = &row->row(true);
  PointerRNA op_ptr = sub->op("OBJECT_OT_game_property_move", "", ICON_TRIA_UP);
  if (op_ptr.data) {
    RNA_int_set(&op_ptr, "index", index);
    RNA_enum_set_identifier(nullptr, &op_ptr, "direction", "UP");
  }
  op_ptr = sub->op("OBJECT_OT_game_property_move", "", ICON_TRIA_DOWN);
  if (op_ptr.data) {
    RNA_int_set(&op_ptr, "index", index);
    RNA_enum_set_identifier(nullptr, &op_ptr, "direction", "DOWN");
  }

  op_ptr = row->op("OBJECT_OT_game_property_remove", "", ICON_X, WM_OP_INVOKE_DEFAULT, UI_ITEM_NONE);
  if (op_ptr.data) {
    RNA_int_set(&op_ptr, "index", index);
  }
}

static void logic_panel_properties_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return;
  }

  /* En un objeto de texto, la propiedad "Text" es especial: es la que el motor
   * usa como contenido, asi que se muestra aparte y no se puede renombrar. */
  const bool is_font = (ob->type == OB_FONT);
  int text_index = -1;
  if (is_font) {
    int i = 0;
    LISTBASE_FOREACH_INDEX (bProperty *, prop, &ob->prop, i) {
      if (STREQ(prop->name, "Text")) {
        text_index = i;
        break;
      }
    }

    if (text_index != -1) {
      PointerRNA op_ptr = layout->op("OBJECT_OT_game_property_remove",
                                     IFACE_("Remove Text Game Property"),
                                     ICON_X);
      if (op_ptr.data) {
        RNA_int_set(&op_ptr, "index", text_index);
      }

      bProperty *text_prop = static_cast<bProperty *>(BLI_findlink(&ob->prop, text_index));
      PointerRNA prop_ptr = RNA_pointer_create_discrete(
          &ob->id, &RNA_GameProperty, text_prop);

      uiLayout *row = &layout->row(false);
      uiLayout *sub = &row->row(false);
      uiLayoutSetEnabled(sub, false);
      sub->prop(&prop_ptr, "name", UI_ITEM_NONE, "", ICON_NONE);
      row->prop(&prop_ptr, "type", UI_ITEM_NONE, "", ICON_NONE);
      /* El cuerpo del texto no se muestra aqui: puede ser enorme y releerlo por
       * frame de UI es caro. */
      row->label(IFACE_("See Text Object"), ICON_NONE);
    }
    else {
      PointerRNA op_ptr = layout->op("OBJECT_OT_game_property_new",
                                     IFACE_("Add Text Game Property"),
                                     ICON_ADD);
      if (op_ptr.data) {
        RNA_string_set(&op_ptr, "name", "Text");
        RNA_enum_set(&op_ptr, "type", GPROP_STRING);
      }
    }
  }

  PointerRNA op_ptr = layout->op("OBJECT_OT_game_property_new", IFACE_("Add Game Property"), ICON_ADD);
  if (op_ptr.data) {
    RNA_string_set(&op_ptr, "name", "");
  }

  int i = 0;
  LISTBASE_FOREACH_INDEX (bProperty *, prop, &ob->prop, i) {
    if (is_font && i == text_index) {
      continue;
    }
    PointerRNA prop_ptr = RNA_pointer_create_discrete(&ob->id, &RNA_GameProperty, prop);
    logic_property_row_draw(layout, &prop_ptr, i);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Menus
 * \{ */

static void logic_menu_logicbricks_add_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;
  uiItemMenuEnumO(layout, C, "LOGIC_OT_sensor_add", "type", IFACE_("Sensor"), ICON_NONE);
  uiItemMenuEnumO(layout, C, "LOGIC_OT_controller_add", "type", IFACE_("Controller"), ICON_NONE);
  uiItemMenuEnumO(layout, C, "LOGIC_OT_actuator_add", "type", IFACE_("Actuator"), ICON_NONE);
}

static void logic_menu_view_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;

  layout->op("LOGIC_OT_properties", std::nullopt, ICON_MENU_PANEL);

  layout->separator();

  layout->op("SCREEN_OT_area_dupli", std::nullopt, ICON_NONE);
  layout->op("SCREEN_OT_screen_full_area", std::nullopt, ICON_NONE);

  PointerRNA op_ptr = layout->op(
      "SCREEN_OT_screen_full_area", IFACE_("Toggle Fullscreen Area"), ICON_NONE);
  if (op_ptr.data) {
    RNA_boolean_set(&op_ptr, "use_hide_panels", true);
  }
}

static void logic_menu_editor_menus_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;
  layout->menu("LOGIC_MT_view", std::nullopt, ICON_NONE);
  layout->menu("LOGIC_MT_logicbricks_add", std::nullopt, ICON_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cabecera
 * \{ */

static void logic_header_draw(const bContext *C, Header *header)
{
  uiLayout *layout = &header->layout->row(true);
  ScrArea *area = CTX_wm_area(C);

  uiTemplateHeader(layout, const_cast<bContext *>(C));

  /* Equivalente de Menu.draw_collapsible (bpy_types.py:1297): con los menus
   * visibles se despliegan en linea; si no, se recogen en un solo boton. */
  if (area && (area->flag & HEADER_NO_PULLDOWN) == 0) {
    uiLayout *row = &layout->row(true);
    if (MenuType *mt = WM_menutype_find("LOGIC_MT_editor_menus", false)) {
      UI_menutype_draw(const_cast<bContext *>(C), mt, row);
    }
  }
  else {
    layout->menu("LOGIC_MT_editor_menus", "", ICON_COLLAPSEMENU);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registro
 * \{ */

static const flipendo::PanelDecl logic_panels[] = {
    {
        /*idname*/ "LOGIC_PT_properties",
        /*label*/ N_("Properties"),
        /*category*/ "Logic",
        /*context*/ nullptr,
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ logic_panel_properties_draw,
        /*draw_header*/ nullptr,
        /*draw_header_preset*/ nullptr,
        /*poll*/ logic_panel_properties_poll,
    },
};

static const flipendo::MenuDecl logic_menus[] = {
    {
        /*idname*/ "LOGIC_MT_logicbricks_add",
        /*label*/ N_("Add"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ logic_menu_logicbricks_add_draw,
    },
    {
        /*idname*/ "LOGIC_MT_view",
        /*label*/ N_("View"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ logic_menu_view_draw,
    },
    {
        /*idname*/ "LOGIC_MT_editor_menus",
        /*label*/ "",
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ logic_menu_editor_menus_draw,
    },
};

static const flipendo::HeaderDecl logic_headers[] = {
    {
        /*idname*/ "LOGIC_HT_header",
        /*draw*/ logic_header_draw,
    },
};

void logic_buttons_register(ARegionType *art)
{
  flipendo::panels_register(art, SPACE_LOGIC, {logic_panels, ARRAY_SIZE(logic_panels)});
}

void logic_header_register(ARegionType *art)
{
  flipendo::headers_register(
      art, SPACE_LOGIC, RGN_TYPE_HEADER, {logic_headers, ARRAY_SIZE(logic_headers)});
}

void logic_menus_register()
{
  flipendo::menus_register({logic_menus, ARRAY_SIZE(logic_menus)});
}

/** \} */
