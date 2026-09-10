/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * La barra de herramientas, dibujada en C++. Ver FL_toolbar_ui.hh.
 *
 * Transliteracion de `ToolSelectPanelHelper.draw_cls`, de sus generadores de columnas y
 * de `WM_MT_toolsystem_submenu` (`scripts/startup/bl_ui/space_toolsystem_common.py`). El
 * objetivo es el mismo dibujo pixel a pixel, asi que se conservan las rarezas del
 * Python que afectan a la maqueta, y cada una esta senalada donde aparece.
 */

#include <cstdio>
#include <memory>
#include <string>

#include "BKE_appdir.hh"
#include "BKE_context.hh"
#include "BKE_icons.h"
#include "BKE_screen.hh"

#include "BLI_fileops.h"
#include "BLI_map.hh"
#include "BLI_path_utils.hh"
#include "BLI_string.h"

#include "BLT_translation.hh"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_workspace_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "WM_api.hh"

#include "FL_toolbar_ui.hh"
#include "FL_ui_registry.hh"
#include "toolsystem/FL_toolsystem.hpp"

namespace flipendo::ui {

namespace ts = flipendo::toolsystem;

/* -------------------------------------------------------------------- */
/** \name Iconos
 *
 * `_icon_value_from_icon_handle` del Python. Lo que alli era
 * `bpy.app.icons.new_triangles_from_file` es solo un envoltorio de estas dos llamadas
 * (`bpy_app_icons.cc`).
 * \{ */

/** -1 si no se pudo cargar. */
static int tool_icon_load(const char *icon_name)
{
  /* El Python usa el directorio del SISTEMA; esto mira antes el del usuario, que en una
   * instalacion normal no tiene `datafiles/icons`, asi que acaba en el mismo sitio. */
  const std::optional<std::string> dir = BKE_appdir_folder_id(BLENDER_DATAFILES, "icons");
  if (!dir) {
    fprintf(stderr, "Missing icons: no datafiles/icons directory\n");
    return -1;
  }
  const std::string filename = std::string(icon_name) + ".dat";
  char filepath[FILE_MAX];
  BLI_path_join(filepath, sizeof(filepath), dir->c_str(), filename.c_str());
  Icon_Geom *geom = BKE_icon_geom_from_file(filepath);
  if (geom == nullptr) {
    fprintf(stderr, "%s: %s\n", BLI_exists(filepath) ? "Corrupt icon" : "Missing icons", filepath);
    return -1;
  }
  return BKE_icon_geom_ensure(geom);
}

int tool_icon_value(const char *icon_name)
{
  if (icon_name == nullptr) {
    return 0;
  }
  static blender::Map<std::string, int> cache;
  if (const int *value = cache.lookup_ptr(icon_name)) {
    return *value;
  }
  int value = tool_icon_load(icon_name);
  if (value < 0) {
    /* Como el Python: 'none' en su lugar, para no descuadrar la maqueta. */
    value = STREQ(icon_name, "none") ? 0 : tool_icon_value("none");
  }
  cache.add(icon_name, value);
  return value;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generadores de columnas
 *
 * En el Python son corrutinas (`yield`/`send`), no bucles, y el resultado depende del
 * punto exacto donde se reanudan. Se reproducen como maquinas de estado: `send(false)`
 * es `ui_gen.send(False)` (una herramienta), `send(true)` un separador y `finish()` el
 * `ui_gen.send(None)` final.
 * \{ */

class LayoutGen {
 public:
  virtual ~LayoutGen() = default;
  /** Donde va la herramienta (o, tras un separador, la columna nueva). */
  virtual uiLayout *send(bool is_sep) = 0;
  virtual void finish() = 0;
};

/** `_layout_generator_single_column`. */
class SingleColumn : public LayoutGen {
 public:
  SingleColumn(uiLayout *layout, const float scale_y) : layout_(layout), scale_y_(scale_y)
  {
    new_column();
  }

  uiLayout *send(const bool is_sep) override
  {
    if (is_sep) {
      new_column();
    }
    return col_;
  }

  void finish() override {}

 private:
  void new_column()
  {
    col_ = &layout_->column(true);
    uiLayoutSetScaleY(col_, scale_y_);
  }

  uiLayout *layout_;
  float scale_y_;
  uiLayout *col_ = nullptr;
};

/** `_layout_generator_multi_columns`. */
class MultiColumns : public LayoutGen {
 public:
  MultiColumns(uiLayout *layout, const int column_count, const float scale_y)
      : layout_(layout),
        column_count_(column_count),
        column_last_(column_count - 1),
        scale_x_(scale_y * 1.1f),
        scale_y_(scale_y)
  {
    /* Aqui la columna NO lleva escala; solo las filas. */
    col_ = &layout_->column(true);
    new_row();
  }

  uiLayout *send(const bool is_sep) override
  {
    /* Se reanuda tras `is_sep = yield row`: primero el salto de fila... */
    if (column_index_ == column_count_) {
      column_index_ = 0;
      new_row();
    }
    column_index_ += 1;
    /* ...y en la vuelta siguiente del bucle, el separador. */
    if (is_sep) {
      if (column_index_ != column_last_) {
        row_->label("", ICON_NONE);
      }
      col_ = &layout_->column(true);
      new_row();
      column_index_ = 0;
    }
    return row_;
  }

  void finish() override
  {
    if (column_index_ == column_last_) {
      row_->label("", ICON_NONE);
      return;
    }
    /* Rareza conservada: si no cae en la ultima columna, el generador del Python no
     * termina; da una vuelta mas y se queda parado, y esa vuelta puede abrir una fila
     * vacia. Una fila de mas o de menos cambia pixeles. */
    if (column_index_ == column_count_) {
      column_index_ = 0;
      new_row();
    }
    column_index_ += 1;
  }

 private:
  void new_row()
  {
    row_ = &col_->row(true);
    uiLayoutSetScaleX(row_, scale_x_);
    uiLayoutSetScaleY(row_, scale_y_);
  }

  uiLayout *layout_;
  int column_count_;
  int column_last_;
  float scale_x_;
  float scale_y_;
  uiLayout *col_ = nullptr;
  uiLayout *row_ = nullptr;
  int column_index_ = 0;
};

/**
 * `_layout_generator_detect_from_region`: una o dos columnas, y si hay texto, segun el
 * ancho de la region en unidades de interfaz.
 */
static std::unique_ptr<LayoutGen> layout_gen_detect(const bContext *C,
                                                    uiLayout *layout,
                                                    const float scale_y,
                                                    bool *r_show_text)
{
  const ARegion *region = CTX_wm_region(C);
  float x0, x1, unused;
  UI_view2d_region_to_view(&region->v2d, 1.0f, 0.0f, &x1, &unused);
  UI_view2d_region_to_view(&region->v2d, 0.0f, 0.0f, &x0, &unused);

  /* La misma propiedad que lee el Python (`preferences.system.ui_scale`), por RNA: asi
   * es fiel por construccion, sin depender de en que campo de `U` vive. */
  PointerRNA system_ptr = RNA_pointer_create_discrete(nullptr, &RNA_PreferencesSystem, &U);
  const float ui_scale = RNA_float_get(&system_ptr, "ui_scale");

  /* En `double`, como el Python: con `float` un ancho en el limite podria caer al otro
   * lado de 80 o 120. */
  const double width_scale = double(region->winx) * (double(x1) - double(x0)) / double(ui_scale);

  int column_count = 1;
  if (width_scale > 120.0) {
    *r_show_text = true;
  }
  else {
    *r_show_text = false;
    /* El comentario del Python dice "2 column layout, disabled", pero el codigo lo usa. */
    column_count = (width_scale > 80.0) ? 2 : 1;
  }
  if (column_count == 1) {
    return std::make_unique<SingleColumn>(layout, scale_y);
  }
  return std::make_unique<MultiColumns>(layout, column_count, scale_y);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Botones
 * \{ */

/** Lo que hace `rna_translate_ui_text` con la etiqueta de un boton de operador. */
static const char *operator_button_text(const wmOperatorType *ot, const char *text)
{
  if (text[0] == '\0' || !BLT_translate_iface()) {
    return text;
  }
  return BLT_pgettext(RNA_struct_translation_context(ot->srna), text);
}

/** `layout.operator("wm.tool_set_by_id", ...).name = idname`, o su version con menu al
 * mantener pulsado. El texto vacio se pasa como cadena vacia, NO como ausente: ausente,
 * `op()` pondria el nombre del operador. */
static void tool_button(uiLayout *layout,
                        wmOperatorType *ot,
                        const ts::ToolDecl &tool,
                        const char *text,
                        const bool depress,
                        const char *menu_hold)
{
  const blender::StringRef label = operator_button_text(ot, text);
  const int icon = tool_icon_value(tool.icon);
  const eUI_Item_Flag flag = depress ? UI_ITEM_O_DEPRESS : UI_ITEM_NONE;
  const wmOperatorCallContext context = uiLayoutGetOperatorContext(layout);
  PointerRNA opptr;
  if (menu_hold != nullptr) {
    uiItemFullOMenuHold_ptr(layout, ot, label, icon, context, flag, menu_hold, &opptr);
  }
  else {
    opptr = layout->op(ot, label, icon, context, flag);
  }
  if (opptr.data != nullptr) {
    RNA_string_set(&opptr, "name", tool.idname);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name La barra
 * \{ */

void toolbar_draw(const bContext *C, uiLayout *layout, const bool detect_layout, const float scale_y)
{
  const SpaceLink *sl = CTX_wm_space_data(C);
  if (sl == nullptr) {
    return;
  }
  const int space_type = sl->spacetype;
  const ts::ToolbarDecl *toolbar = ts::toolbar_for_space(space_type);
  wmOperatorType *ot = WM_operatortype_find("WM_OT_tool_set_by_id", false);
  if (toolbar == nullptr || ot == nullptr) {
    return;
  }

  const bToolRef *tref = ts::tool_active_ref(C, space_type, false);
  const char *tool_active_id = tref != nullptr ? tref->idname : nullptr;

  bool show_text = true;
  std::unique_ptr<LayoutGen> gen = detect_layout ?
                                       layout_gen_detect(C, layout, scale_y, &show_text) :
                                       std::make_unique<SingleColumn>(layout, scale_y);

  const char *mode = toolbar->mode_from_context != nullptr ? toolbar->mode_from_context(C) :
                                                             nullptr;
  for (const ts::ToolGroupView &entry : ts::toolbar_entries_for_space_mode(C, *toolbar, mode)) {
    if (entry.is_separator()) {
      gen->send(true);
      continue;
    }

    const ts::ToolDecl *item = entry.tools[0];
    bool use_menu = false;
    if (entry.is_group()) {
      int index = -1;
      for (const int i : entry.tools.index_range()) {
        if (tool_active_id != nullptr && STREQ(entry.tools[i]->idname, tool_active_id)) {
          index = i;
          break;
        }
      }
      if (index != -1) {
        /* "not ideal, write this every time", dice el Python: el grupo recuerda la
         * herramienta activa cada vez que se dibuja. */
        ts::group_active_set(space_type, entry.tools[0]->idname, index);
      }
      else {
        index = ts::group_active_get(space_type, entry.tools[0]->idname);
        if (index < 0 || index >= entry.tools.size()) {
          index = 0;
        }
      }
      item = entry.tools[index];
      use_menu = true;
    }

    const bool is_active = tool_active_id != nullptr && STREQ(item->idname, tool_active_id);
    uiLayout *sub = gen->send(false);
    tool_button(sub,
                ot,
                *item,
                show_text ? item->label : "",
                is_active,
                use_menu ? "WM_MT_toolsystem_submenu" : nullptr);
  }
  gen->finish();
}

static void toolbar_panel_draw(const bContext *C, Panel *panel)
{
  /* `ToolSelectPanelHelper.draw`: `draw_cls(layout, context)` con sus valores por
   * defecto. */
  toolbar_draw(C, panel->layout, true, 1.75f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Menu de grupo
 *
 * El que se abre al mantener pulsado el boton de un grupo. El boton deja su operador en
 * el contexto (`button_operator`), y de su propiedad `name` sale el grupo.
 * \{ */

static void toolsystem_submenu_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;
  uiLayoutSetScaleY(layout, 2.0f);

  const ts::ToolGroupView *group = nullptr;
  blender::Vector<ts::ToolGroupView> entries;

  const SpaceLink *sl = CTX_wm_space_data(C);
  const ts::ToolbarDecl *toolbar = sl != nullptr ? ts::toolbar_for_space(sl->spacetype) : nullptr;
  wmOperatorType *ot = WM_operatortype_find("WM_OT_tool_set_by_id", false);
  PointerRNA button_ptr = CTX_data_pointer_get_type(C, "button_operator", &RNA_OperatorProperties);
  if (toolbar != nullptr && ot != nullptr && button_ptr.data != nullptr) {
    char *button_id = RNA_string_get_alloc(&button_ptr, "name", nullptr, 0, nullptr);
    const char *mode = toolbar->mode_from_context != nullptr ? toolbar->mode_from_context(C) :
                                                               nullptr;
    entries = ts::toolbar_entries_for_space_mode(C, *toolbar, mode);
    /* Solo grupos: en el Python se buscan tuplas. */
    for (const ts::ToolGroupView &entry : entries) {
      if (!entry.is_group()) {
        continue;
      }
      for (const ts::ToolDecl *tool : entry.tools) {
        if (STREQ(tool->idname, button_id)) {
          group = &entry;
          break;
        }
      }
      if (group != nullptr) {
        break;
      }
    }
    MEM_freeN(button_id);
  }

  if (group == nullptr) {
    /* "Should never happen, just in case", dice el Python. */
    layout->label(IFACE_("Unable to find toolbar group"), ICON_NONE);
    return;
  }
  for (const ts::ToolDecl *tool : group->tools) {
    tool_button(layout, ot, *tool, tool->label, false, nullptr);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registro
 * \{ */

void toolbar_panels_register(ARegionType *art, const int space_type)
{
  const char *idname = nullptr;
  switch (space_type) {
    case SPACE_VIEW3D:
      idname = "VIEW3D_PT_tools_active";
      break;
    case SPACE_IMAGE:
      idname = "IMAGE_PT_tools_active";
      break;
    case SPACE_NODE:
      idname = "NODE_PT_tools_active";
      break;
    case SPACE_SEQ:
      idname = "SEQUENCER_PT_tools_active";
      break;
    default:
      return;
  }

  /* Los mismos datos que declaraban las clases de Python: etiqueta "Tools" (no se ve) y
   * sin cabecera. */
  PanelDecl panel{};
  panel.idname = idname;
  panel.label = N_("Tools");
  panel.draw = toolbar_panel_draw;
  panel.flag = PANEL_TYPE_NO_HEADER;
  panels_register(art, space_type, blender::Span<const PanelDecl>(&panel, 1));

  static bool menu_registered = false;
  if (!menu_registered) {
    MenuDecl submenu{};
    submenu.idname = "WM_MT_toolsystem_submenu";
    submenu.label = "";
    submenu.draw = toolsystem_submenu_draw;
    menus_register(blender::Span<const MenuDecl>(&submenu, 1));
    menu_registered = true;
  }
}

/** \} */

}  // namespace flipendo::ui
