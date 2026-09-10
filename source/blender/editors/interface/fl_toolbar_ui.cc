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

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_workspace_types.h"

#include "RNA_access.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "WM_api.hh"
#include "WM_toolsystem.hh"

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
/** \name Ajustes de la herramienta
 *
 * Las filas declarativas (`PropRow`) reproducen `layout.prop(...)` sobre su fuente; lo
 * que no cabe en filas es codigo (`ToolDecl::draw_settings`).
 * \{ */

/** La fuente de una fila: `tool.operator_properties(op)`,
 * `tool.gizmo_group_properties(g)`, `context.tool_settings[.ruta]` o
 * `context.preferences.edit`. */
static PointerRNA prop_row_source(const bContext *C, bToolRef *tref, const ts::PropRow &row)
{
  switch (row.source) {
    case ts::PropSource::Operator: {
      wmOperatorType *ot = WM_operatortype_find(row.source_id, true);
      if (ot == nullptr) {
        return PointerRNA_NULL;
      }
      PointerRNA ptr;
      WM_toolsystem_ref_properties_ensure_from_operator(tref, ot, &ptr);
      return ptr;
    }
    case ts::PropSource::GizmoGroup: {
      wmGizmoGroupType *gzgt = WM_gizmogrouptype_find(row.source_id, false);
      if (gzgt == nullptr) {
        return PointerRNA_NULL;
      }
      PointerRNA ptr;
      WM_toolsystem_ref_properties_ensure_from_gizmo_group(tref, gzgt, &ptr);
      return ptr;
    }
    case ts::PropSource::ToolSettings:
    case ts::PropSource::ToolSettingsSub: {
      Scene *scene = CTX_data_scene(C);
      if (scene == nullptr || scene->toolsettings == nullptr) {
        return PointerRNA_NULL;
      }
      PointerRNA ts_ptr = RNA_pointer_create_discrete(
          &scene->id, &RNA_ToolSettings, scene->toolsettings);
      if (row.source == ts::PropSource::ToolSettings || row.source_id == nullptr) {
        return ts_ptr;
      }
      /* Rutas con puntos (`gpencil_paint.brush.gpencil_settings`). Si algo de la cadena no
       * existe, sin pincel por ejemplo, no hay fila: el Python tampoco podria pintarla. */
      PointerRNA sub;
      PropertyRNA *prop = nullptr;
      if (!RNA_path_resolve(&ts_ptr, row.source_id, &sub, &prop)) {
        return PointerRNA_NULL;
      }
      if (prop != nullptr) {
        if (RNA_property_type(prop) != PROP_POINTER) {
          return PointerRNA_NULL;
        }
        sub = RNA_property_pointer_get(&sub, prop);
      }
      return sub;
    }
    case ts::PropSource::PreferencesEdit:
      return RNA_pointer_create_discrete(nullptr, &RNA_PreferencesEdit, &U);
  }
  return PointerRNA_NULL;
}

static void prop_rows_draw(const bContext *C,
                           uiLayout *layout,
                           bToolRef *tref,
                           const blender::Span<ts::PropRow> rows,
                           const bool extra)
{
  for (const ts::PropRow &row : rows) {
    if (bool(row.flags & ts::PROP_ROW_EXTRA_ONLY) != extra) {
      continue;
    }
    PointerRNA ptr = prop_row_source(C, tref, row);
    if (ptr.data == nullptr) {
      continue;
    }
    PropertyRNA *prop = RNA_struct_find_property(&ptr, row.prop);
    if (prop == nullptr) {
      fprintf(stderr,
              "Herramientas: '%s' no tiene la propiedad '%s'.\n",
              RNA_struct_identifier(ptr.type),
              row.prop);
      continue;
    }
    uiLayout *target = layout;
    if (row.flags & ts::PROP_ROW_OWN_ROW) {
      /* `layout.row()` desde Python: variante con encabezado vacio. */
      target = &layout->row(false, "");
      if (row.flags & ts::PROP_ROW_NO_SPLIT) {
        uiLayoutSetPropSep(target, false);
      }
    }
    eUI_Item_Flag flag = UI_ITEM_NONE;
    if (row.flags & ts::PROP_ROW_EXPAND) {
      flag |= UI_ITEM_R_EXPAND;
    }
    if (row.flags & ts::PROP_ROW_SLIDER) {
      flag |= UI_ITEM_R_SLIDER;
    }
    if (row.flags & ts::PROP_ROW_TOGGLE) {
      flag |= UI_ITEM_R_TOGGLE;
    }
    if (row.flags & ts::PROP_ROW_ICON_ONLY) {
      flag |= UI_ITEM_R_ICON_ONLY;
    }
    /* `rna_translate_ui_text` con propiedad y sin tipo: contexto por defecto. Sin texto,
     * la etiqueta de la propiedad; con texto vacio, ninguna. */
    std::optional<blender::StringRef> text;
    if (row.flags & ts::PROP_ROW_NO_TEXT) {
      text = "";
    }
    else if (row.text != nullptr) {
      text = (row.text[0] != '\0' && BLT_translate_iface()) ?
                 BLT_pgettext(BLT_I18NCONTEXT_DEFAULT, row.text) :
                 row.text;
    }
    target->prop(&ptr, prop, -1, 0, flag, text, ICON_NONE);
  }
}

static void tool_settings_draw(const bContext *C,
                               uiLayout *layout,
                               bToolRef *tref,
                               const ts::ToolDecl &item,
                               const bool extra)
{
  if (item.draw_settings != nullptr) {
    item.draw_settings(C, layout, tref, extra);
    return;
  }
  prop_rows_draw(C, layout, tref, item.settings, extra);
}

/** `tool_settings.workspace_tool_type == identifier`, leido por RNA como el Python. */
static bool workspace_tool_type_is(const bContext *C, const char *identifier)
{
  Scene *scene = CTX_data_scene(C);
  if (scene == nullptr || scene->toolsettings == nullptr) {
    return false;
  }
  PointerRNA ptr = RNA_pointer_create_discrete(&scene->id, &RNA_ToolSettings, scene->toolsettings);
  PropertyRNA *prop = RNA_struct_find_property(&ptr, "workspace_tool_type");
  if (prop == nullptr) {
    return false;
  }
  const char *current = nullptr;
  RNA_property_enum_identifier(
      const_cast<bContext *>(C), &ptr, prop, RNA_property_enum_get(&ptr, prop), &current);
  return current != nullptr && STREQ(current, identifier);
}

/** `context.space_data.show_region_toolbar`. Sin esa propiedad, como si se viera: asi
 * no se pinta el icono de sustitucion. */
static bool space_shows_toolbar(const bContext *C)
{
  SpaceLink *sl = CTX_wm_space_data(C);
  if (sl == nullptr) {
    return true;
  }
  bScreen *screen = CTX_wm_screen(C);
  PointerRNA ptr = RNA_pointer_create_discrete(screen ? &screen->id : nullptr, &RNA_Space, sl);
  PropertyRNA *prop = RNA_struct_find_property(&ptr, "show_region_toolbar");
  return prop == nullptr || RNA_property_boolean_get(&ptr, prop);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name La cabecera
 * \{ */

bToolRef *tool_header_draw(const bContext *C,
                           uiLayout *layout,
                           const bool show_tool_icon_always,
                           int space_type,
                           const char *mode)
{
  if (space_type < 0) {
    const SpaceLink *sl = CTX_wm_space_data(C);
    if (sl == nullptr) {
      return nullptr;
    }
    space_type = sl->spacetype;
    if (mode == nullptr) {
      const ts::ToolbarDecl *tb = ts::toolbar_for_space(space_type);
      if (tb != nullptr && tb->mode_from_context != nullptr) {
        mode = tb->mode_from_context(C);
      }
    }
  }
  const ts::ToolbarDecl *toolbar = ts::toolbar_for_space(space_type);
  if (toolbar == nullptr) {
    return nullptr;
  }

  bToolRef *tref = ts::tool_ref_for_mode(C, space_type, mode, false);
  if (tref == nullptr) {
    return nullptr;
  }
  const ts::ToolDecl *item = ts::tool_find_in(C, *toolbar, mode, tref->idname);
  if (item == nullptr) {
    return nullptr;
  }

  const int icon_value = tool_icon_value(item->icon);
  if (show_tool_icon_always) {
    /* "Add some spacing since the icon is currently assuming regular small icon size." */
    const std::string label = std::string("    ") +
                              CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, item->label);
    layout->label(label, icon_value);
    layout->separator();
  }
  else if (!space_shows_toolbar(C)) {
    /* El Python pide `template_icon(scale=0.5)`, pero la propiedad RNA tiene minimo
     * duro 1.0 y Python recorta los argumentos a su rango: el valor efectivo es 1.0. */
    uiTemplateIcon(layout, icon_value, 1.0f);
    layout->separator();
  }

  tool_settings_draw(C, layout, tref, *item, false);

  const char *idname_fallback = tref->idname_fallback;
  if (idname_fallback[0] != '\0' && !STREQ(idname_fallback, item->idname)) {
    /* Parece un enum y no lo es: un popover con la reserva elegida. */
    const char *label = "Active Tool";
    if (workspace_tool_type_is(C, "FALLBACK")) {
      if (const ts::ToolDecl *fallback = ts::tool_find_by_id_active(
              C, *toolbar, mode, toolbar->tool_fallback_id))
      {
        label = fallback->label;
      }
    }
    uiLayout &row = layout->row(false, CTX_IFACE_(BLT_I18NCONTEXT_EDITOR_VIEW3D, "Drag"));
    WorkSpace *workspace = CTX_wm_workspace(C);
    PointerRNA tool_ptr = RNA_pointer_create_discrete(
        workspace ? &workspace->id : nullptr, &RNA_WorkSpaceTool, tref);
    uiLayoutSetContextPointer(&row, "tool", &tool_ptr);
    uiItemPopoverPanel(&row,
                       C,
                       "TOPBAR_PT_tool_fallback",
                       CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, label),
                       ICON_NONE);
  }
  return tref;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name La reserva
 * \{ */

void tool_fallback_items_draw(const bContext *C, uiLayout *layout, const bool pie)
{
  const SpaceLink *sl = CTX_wm_space_data(C);
  if (sl == nullptr) {
    return;
  }
  int space_type = sl->spacetype;
  if (space_type == SPACE_PROPERTIES) {
    space_type = SPACE_VIEW3D;
  }
  const ts::ToolbarDecl *toolbar = ts::toolbar_for_space(space_type);
  wmOperatorType *ot = WM_operatortype_find("WM_OT_tool_set_by_id", false);
  Scene *scene = CTX_data_scene(C);
  if (toolbar == nullptr || ot == nullptr || scene == nullptr) {
    return;
  }
  const char *mode = toolbar->mode_from_context != nullptr ? toolbar->mode_from_context(C) :
                                                             nullptr;
  const blender::Vector<const ts::ToolDecl *> group = ts::fallback_group_tools(
      C, *toolbar, mode);
  if (group.is_empty()) {
    /* El Python lanza "Fallback tool doesn't exist". */
    return;
  }

  PointerRNA ts_ptr = RNA_pointer_create_discrete(&scene->id, &RNA_ToolSettings, scene->toolsettings);
  PropertyRNA *type_prop = RNA_struct_find_property(&ts_ptr, "workspace_tool_type");
  const bool is_active_tool = workspace_tool_type_is(C, "DEFAULT");
  int index_current = -1;
  if (!is_active_tool) {
    index_current = ts::group_active_get(space_type, group[0]->idname);
    if (index_current < 0 || index_current >= group.size()) {
      index_current = 0;
    }
  }
  const char *active_tool_text = IFACE_("Active Tool");

  if (!pie) {
    uiLayout &col = layout->column(true);
    uiItemEnumR_string_prop(&col, &ts_ptr, type_prop, "DEFAULT", active_tool_text, ICON_NONE);
    uiLayout &items = layout->column(true);
    for (const int i : group.index_range()) {
      PointerRNA props = items.op(ot,
                                  operator_button_text(ot, group[i]->label),
                                  ICON_NONE,
                                  uiLayoutGetOperatorContext(&items),
                                  (i == index_current) ? UI_ITEM_O_DEPRESS : UI_ITEM_NONE);
      RNA_string_set(&props, "name", group[i]->idname);
      RNA_boolean_set(&props, "as_fallback", true);
      RNA_enum_set(&props, "space_type", space_type);
    }
    return;
  }

  /* En la tarta tambien se deja cambiar la herramienta activa: confunde menos. */
  const bToolRef *active = ts::tool_active_ref(C, space_type, false);
  bool is_fallback_group_active = false;
  for (const ts::ToolDecl *tool : group) {
    if (active != nullptr && STREQ(active->idname, tool->idname)) {
      is_fallback_group_active = true;
    }
  }
  uiLayout *pie_layout = &layout->menu_pie();
  uiItemEnumR_string_prop(
      pie_layout, &ts_ptr, type_prop, "DEFAULT", active_tool_text, ICON_TOOL_SETTINGS);
  for (const int i : group.index_range()) {
    PointerRNA props = pie_layout->op(ot,
                                      operator_button_text(ot, group[i]->label),
                                      tool_icon_value(group[i]->icon),
                                      uiLayoutGetOperatorContext(pie_layout),
                                      (i == index_current) ? UI_ITEM_O_DEPRESS : UI_ITEM_NONE);
    RNA_string_set(&props, "name", group[i]->idname);
    RNA_enum_set(&props, "space_type", space_type);
    if (!is_fallback_group_active) {
      RNA_boolean_set(&props, "as_fallback", true);
    }
  }
}

void tool_fallback_settings_draw(const bContext *C,
                                 uiLayout *layout,
                                 bToolRef *tref,
                                 const bool is_horizontal_layout)
{
  if (tref == nullptr) {
    return;
  }
  const ts::ToolbarDecl *toolbar = ts::toolbar_for_space(tref->space_type);
  if (toolbar == nullptr) {
    return;
  }
  const char *mode = toolbar->mode_from_context != nullptr ? toolbar->mode_from_context(C) :
                                                             nullptr;
  const ts::ToolDecl *fallback = ts::tool_find_in(C, *toolbar, mode, tref->idname_fallback);
  if (fallback == nullptr || (fallback->draw_settings == nullptr && fallback->settings.is_empty()))
  {
    return;
  }
  if (!is_horizontal_layout) {
    layout->separator();
  }
  /* Los ajustes de la RESERVA, sobre el `bToolRef` de la herramienta activa. */
  tool_settings_draw(C, layout, tref, *fallback, false);
}

void tool_settings_extra_draw(const bContext *C, uiLayout *layout)
{
  const SpaceLink *sl = CTX_wm_space_data(C);
  const ts::ToolbarDecl *toolbar = sl != nullptr ? ts::toolbar_for_space(sl->spacetype) : nullptr;
  if (toolbar == nullptr) {
    return;
  }
  const char *mode = toolbar->mode_from_context != nullptr ? toolbar->mode_from_context(C) :
                                                             nullptr;
  bToolRef *tref = ts::tool_ref_for_mode(C, sl->spacetype, mode, false);
  const ts::ToolDecl *item = tref != nullptr ? ts::tool_find_in(C, *toolbar, mode, tref->idname) :
                                               nullptr;
  if (item != nullptr) {
    tool_settings_draw(C, layout, tref, *item, true);
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
