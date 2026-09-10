/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Ajustes de las herramientas de pintura de lapiz de cera. Transliteracion linea a linea
 * de los `draw_settings` de `_defs_grease_pencil_paint`
 * (`space_toolsystem_toolbar.py:2149`) y de las funciones de
 * `bl_ui/properties_paint_common.py` a las que llaman.
 *
 * Las seis primitivas (linea, polilinea, arco, curva, caja y circulo) comparten
 * `grease_pencil_primitive_toolbar`; el cuentagotas va aparte.
 *
 * Una nota sobre las excepciones. Varias lineas del Python lanzan AttributeError si falta
 * algo (sin objeto activo, sin `gpencil_settings`...). Desde la cabecera eso corta el
 * dibujo justo en esa linea: lo ya pintado se queda y lo demas no sale. Aqui se
 * reproduce igual, volviendo en el mismo punto, y cada vuelta lo dice en su comentario.
 */

#include <cstdio>
#include <optional>
#include <string>

#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_icons.h"
#include "BKE_preview_image.hh"

#include "BLT_translation.hh"

#include "DNA_ID.h"
#include "DNA_space_types.h"
#include "DNA_workspace_types.h"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_toolsystem.hh"

#include "FL_tool_settings_ui.hh"

namespace flipendo::ui::settings {

/* -------------------------------------------------------------------- */
/** \name Utilidades
 * \{ */

/** `layout.row(align=...)` como puntero, para poder llamar `row` a la variable igual que
 * en el Python sin tapar la funcion. */
static uiLayout *ui_row(uiLayout *layout, const bool align = false)
{
  return &row(layout, align);
}

/** `layout.column(align=...)`, por lo mismo. */
static uiLayout *ui_column(uiLayout *layout, const bool align = false)
{
  return &column(layout, align);
}

/** `ptr.<name>` de una enumeracion, como identificador: lo que ve el Python al compararla
 * con una cadena. "" si no hay dato o no es una enumeracion. */
static const char *enum_id(const bContext *C, PointerRNA *ptr, const char *name)
{
  if (ptr == nullptr || ptr->data == nullptr) {
    return "";
  }
  PropertyRNA *property = RNA_struct_find_property(ptr, name);
  if (property == nullptr || RNA_property_type(property) != PROP_ENUM) {
    return "";
  }
  const char *identifier = nullptr;
  RNA_property_enum_identifier(const_cast<bContext *>(C),
                               ptr,
                               property,
                               RNA_property_enum_get(ptr, property),
                               &identifier);
  return identifier != nullptr ? identifier : "";
}

static bool enum_is(const bContext *C, PointerRNA *ptr, const char *name, const char *value)
{
  return STREQ(enum_id(C, ptr, name), value);
}

/** `context.mode`, como identificador de `rna_enum_context_mode_items`. */
static const char *context_mode(const bContext *C)
{
  const char *identifier = nullptr;
  RNA_enum_identifier(rna_enum_context_mode_items, CTX_data_mode_enum(C), &identifier);
  return identifier != nullptr ? identifier : "";
}

/** `context.workspace.tools.from_space_view3d_mode(context.mode, create=False)`, que es
 * `rna_WorkSpace_tools_from_space_view3d_mode`. Nulo si no hay herramienta. */
static bToolRef *tool_from_space_view3d_mode(const bContext *C)
{
  WorkSpace *workspace = CTX_wm_workspace(C);
  if (workspace == nullptr) {
    return nullptr;
  }
  bToolKey key{};
  key.space_type = SPACE_VIEW3D;
  key.mode = CTX_data_mode_enum(C);
  return WM_toolsystem_ref_find(workspace, &key);
}

/** `tool.idname in {"builtin.arc", ...}`: el conjunto que se repite tres veces en
 * `brush_basic_grease_pencil_paint_settings`. */
static bool tool_is_primitive(const bToolRef *tool)
{
  return STR_ELEM(tool->idname,
                  "builtin.arc",
                  "builtin.curve",
                  "builtin.line",
                  "builtin.box",
                  "builtin.circle",
                  "builtin.polyline");
}

/** `layout.prop_with_popover(ptr, name, text=..., panel=...)`: `rna_uiItemR_with_popover`,
 * con `text_ctxt`, `translate`, `icon` e `icon_only` por defecto. */
static void prop_with_popover(
    uiLayout *layout, PointerRNA *ptr, const char *name, const char *text, const char *panel)
{
  PropertyRNA *property = RNA_struct_find_property(ptr, name);
  if (property == nullptr) {
    fprintf(stderr,
            "Herramientas: propiedad no encontrada: %s.%s\n",
            RNA_struct_identifier(ptr->type),
            name);
    return;
  }
  if ((RNA_property_type(property) != PROP_ENUM) &&
      !ELEM(RNA_property_subtype(property), PROP_COLOR, PROP_COLOR_GAMMA))
  {
    fprintf(stderr,
            "Herramientas: la propiedad no es enumeracion ni color: %s.%s\n",
            RNA_struct_identifier(ptr->type),
            name);
    return;
  }
  /* `rna_translate_ui_text` sin tipo: el texto vacio va tal cual, el resto con el contexto
   * por defecto. */
  std::optional<blender::StringRefNull> t;
  if (text != nullptr) {
    t = (text[0] != '\0' && BLT_translate_iface()) ?
            blender::StringRefNull(BLT_pgettext(BLT_I18NCONTEXT_DEFAULT, text)) :
            blender::StringRefNull(text);
  }
  uiItemFullR_with_popover(layout, ptr, property, -1, 0, UI_ITEM_NONE, t, ICON_NONE, panel);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name De `space_toolsystem_common.py`
 * \{ */

/**
 * `ToolSelectPanelHelper.tool_active_from_context` (`space_toolsystem_common.py:784`)
 * junto con `_tool_active_from_context` (`:463`).
 *
 * Solo la rama de `'VIEW_3D'` y `'PROPERTIES'`: estas herramientas solo se pintan en la
 * vista 3D y en la pestana de herramienta del editor de propiedades, asi que las ramas del
 * editor de imagen, de nodos y del secuenciador no se alcanzan desde aqui. Se conserva el
 * `refresh_from_context()`, que el Python llama al dibujar.
 *
 * Diferencia sin alcance practico: sin `space_data` el Python lanza AttributeError en
 * `context.space_data.type`; aqui sale nulo, y `draw_popup_selector` no pinta nada. Al
 * pintar una cabecera o un panel siempre hay editor.
 */
static bToolRef *tool_active_from_context(const bContext *C)
{
  const int space = space_type(C);
  if (ELEM(space, SPACE_VIEW3D, SPACE_PROPERTIES)) {
    bToolRef *tool = tool_from_space_view3d_mode(C);
    if (tool != nullptr) {
      WM_toolsystem_ref_sync_from_context(CTX_data_main(C), CTX_wm_workspace(C), tool);
      return tool;
    }
  }
  return nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name De `properties_paint_common.py`
 * \{ */

/**
 * `UnifiedPaintPanel.get_brush_mode` (`properties_paint_common.py:181`). Devuelve el
 * identificador del modo, o nulo donde el Python devuelve `None`.
 */
static const char *get_brush_mode(const bContext *C)
{
  const char *mode = context_mode(C);

  if (STREQ(mode, "PARTICLE")) {
    /* Particle brush settings currently completely do their own thing. */
    return nullptr;
  }

  bToolRef *tool = tool_active_from_context(C);

  if (tool == nullptr) {
    /* If there is no active tool, then there can't be an active brush. */
    return nullptr;
  }

  /* `tool.use_brushes`: `rna_WorkSpaceTool_use_brushes_get`. */
  if (!(tool->runtime != nullptr && (tool->runtime->flag & TOOLREF_FLAG_USE_BRUSHES))) {
    return nullptr;
  }

  const int space = space_type(C);
  PointerRNA tool_settings_ptr = tool_settings(C);

  if (space != -1) {
    if (space == SPACE_IMAGE) {
      return "PAINT_2D";
    }
    if (ELEM(space, SPACE_VIEW3D, SPACE_PROPERTIES)) {
      if (STREQ(mode, "PAINT_TEXTURE")) {
        if (pointer_get(&tool_settings_ptr, "image_paint").data != nullptr) {
          return mode;
        }
        return nullptr;
      }
      return mode;
    }
  }
  return nullptr;
}

/**
 * `BrushAssetShelf.get_shelf_name_from_context` (`properties_paint_common.py:117`).
 *
 * Un modo con pincel que no este en la tabla da KeyError en el Python; aqui da nulo y no
 * se pinta el selector. Desde el modo de pintura de lapiz no se llega nunca a ese caso.
 */
static const char *get_shelf_name_from_context(const bContext *C)
{
  static const struct {
    const char *mode;
    const char *shelf;
  } mode_map[] = {
      {"SCULPT", "VIEW3D_AST_brush_sculpt"},
      {"PAINT_VERTEX", "VIEW3D_AST_brush_vertex_paint"},
      {"PAINT_WEIGHT", "VIEW3D_AST_brush_weight_paint"},
      {"PAINT_TEXTURE", "VIEW3D_AST_brush_texture_paint"},
      {"PAINT_2D", "IMAGE_AST_brush_paint"},
      {"PAINT_GREASE_PENCIL", "VIEW3D_AST_brush_gpencil_paint"},
      {"SCULPT_GREASE_PENCIL", "VIEW3D_AST_brush_gpencil_sculpt"},
      {"WEIGHT_GREASE_PENCIL", "VIEW3D_AST_brush_gpencil_weight"},
      {"VERTEX_GREASE_PENCIL", "VIEW3D_AST_brush_gpencil_vertex"},
      {"SCULPT_CURVES", "VIEW3D_AST_brush_sculpt_curves"},
  };
  const char *mode = get_brush_mode(C);
  if (mode == nullptr || mode[0] == '\0') {
    return nullptr;
  }
  for (const auto &item : mode_map) {
    if (STREQ(item.mode, mode)) {
      return item.shelf;
    }
  }
  return nullptr;
}

/**
 * `BrushAssetShelf.draw_popup_selector(layout, context, brush)`
 * (`properties_paint_common.py:137`), con `show_name=True`, el unico uso de aqui.
 * `brush` nunca es nulo: quien llama ya ha vuelto antes en ese caso.
 */
static void draw_popup_selector(uiLayout *layout, const bContext *C, PointerRNA *brush)
{
  ID *brush_id = brush->owner_id;

  /* `brush.preview.icon_id if brush and brush.preview else 0`: `rna_IDPreview_get` y
   * `rna_ImagePreview_icon_id_get`, que crea el icono si no lo habia. */
  int preview_icon_id = 0;
  if (PreviewImage *preview = BKE_previewimg_id_get(brush_id)) {
    preview_icon_id = BKE_icon_preview_ensure(brush_id, preview);
  }

  const char *shelf_name = get_shelf_name_from_context(C);
  if (shelf_name == nullptr) {
    return;
  }

  std::string display_name = brush_id->name + 2;
  if (!display_name.empty() && RNA_boolean_get(brush, "has_unsaved_changes")) {
    display_name = display_name + "*";
  }

  /* `rna_uiTemplateAssetShelfPopover`: `icon_value` solo cuenta si `icon` es 'NONE', y
   * el nombre no se traduce. */
  int icon = preview_icon_id == 0 ? ICON_BRUSH_DATA : ICON_NONE;
  if (preview_icon_id && !icon) {
    icon = preview_icon_id;
  }
  blender::ui::template_asset_shelf_popover(*layout, *C, shelf_name, display_name, icon);
}

/**
 * `brush_basic__draw_color_selector(context, layout, brush, gp_settings)`
 * (`properties_paint_common.py:1586`). Devuelve falso donde el Python lanzaria una
 * excepcion: el dibujo se corta ahi y quien llama no debe seguir.
 */
static bool brush_basic__draw_color_selector(const bContext *C,
                                             uiLayout *layout,
                                             PointerRNA *brush,
                                             PointerRNA *gp_settings)
{
  PointerRNA scene_ptr = scene(C);
  PointerRNA tool_settings_ptr = pointer_get(&scene_ptr, "tool_settings");
  PointerRNA settings = pointer_get(&tool_settings_ptr, "gpencil_paint");
  if (gp_settings->data == nullptr) {
    /* `gp_settings.material` con `gp_settings` a `None`: AttributeError antes de pintar. */
    return false;
  }
  PointerRNA ma = pointer_get(gp_settings, "material");

  uiLayout *row = ui_row(layout, true);
  if (!RNA_boolean_get(gp_settings, "use_material_pin")) {
    PointerRNA ob = context_pointer(C, "object");
    if (ob.data == nullptr) {
      /* `context.object.active_material` sin objeto activo: AttributeError con la fila
       * ya creada y vacia. */
      return false;
    }
    ma = pointer_get(&ob, "active_material");
  }
  int icon_id = 0;
  std::string txt_ma;
  if (ma.data != nullptr) {
    /* `ma.id_data` es el propio material. */
    ID *ma_id = ma.owner_id;
    BKE_previewimg_id_ensure(ma_id);
    if (PreviewImage *preview = BKE_previewimg_id_get(ma_id)) {
      icon_id = BKE_icon_preview_ensure(ma_id, preview);
      txt_ma = ma_id->name + 2;
      const int maxw = 25;
      /* `len()` y los cortes del Python cuentan caracteres, no bytes: un nombre con
       * acentos se recorta por el mismo sitio que alli. */
      const size_t len_bytes = txt_ma.size();
      const int len = int(BLI_strlen_utf8(txt_ma.c_str()));
      if (len > maxw) {
        const int head = BLI_str_utf8_offset_from_index(txt_ma.c_str(), len_bytes, maxw - 5);
        const int tail = BLI_str_utf8_offset_from_index(txt_ma.c_str(), len_bytes, len - 3);
        txt_ma = txt_ma.substr(0, size_t(head)) + ".." + txt_ma.substr(size_t(tail));
      }
    }
  }

  uiLayout *sub = ui_row(row, true);
  uiLayoutSetEnabled(sub, !RNA_boolean_get(gp_settings, "use_material_pin"));
  uiLayoutSetUnitsX(sub, 8.0f);
  /* `sub.popover(..., text=txt_ma, translate=False, icon_value=icon_id)`: el texto va sin
   * traducir, y por eso no se usa `popover()`, que traduce. En `rna_uiItemPopoverPanel`
   * `icon_value` gana porque `icon` se queda en 'NONE'. */
  int icon = ICON_NONE;
  if (icon_id && !icon) {
    icon = icon_id;
  }
  uiItemPopoverPanel(
      sub, C, "TOPBAR_PT_grease_pencil_materials", blender::StringRef(txt_ma), icon);

  prop(row, gp_settings, "use_material_pin", Prop().text(""));

  if (enum_is(C, brush, "gpencil_tool", "DRAW") || enum_is(C, brush, "gpencil_tool", "FILL")) {
    row->separator(1.0f);
    uiLayout *sub_row = ui_row(row, true);
    const bool pin_draw_mode = RNA_boolean_get(gp_settings, "pin_draw_mode");
    uiLayoutSetEnabled(sub_row, !pin_draw_mode);
    if (pin_draw_mode) {
      prop_enum(sub_row, gp_settings, "brush_draw_mode", "MATERIAL", "", ICON_MATERIAL);
      prop_enum(sub_row, gp_settings, "brush_draw_mode", "VERTEXCOLOR", "", ICON_VPAINT_HLT);
    }
    else {
      prop_enum(sub_row, &settings, "color_mode", "MATERIAL", "", ICON_MATERIAL);
      prop_enum(sub_row, &settings, "color_mode", "VERTEXCOLOR", "", ICON_VPAINT_HLT);
    }

    const bool show_vertex_color =
        ((!pin_draw_mode) && enum_is(C, &settings, "color_mode", "VERTEXCOLOR")) ||
        (pin_draw_mode && enum_is(C, gp_settings, "brush_draw_mode", "VERTEXCOLOR"));

    if (show_vertex_color) {
      sub_row = ui_row(row, true);
      /* Rareza del Python: `enabled` se pone a `show_vertex_color` dentro del `if` que ya
       * lo exige, asi que siempre es verdadero. Se conserva. */
      uiLayoutSetEnabled(sub_row, show_vertex_color);
      uiLayoutSetScaleX(sub_row, 0.8f);
      prop_with_popover(sub_row, brush, "color", "", "TOPBAR_PT_grease_pencil_vertex_color");
    }
    prop(row, gp_settings, "pin_draw_mode", Prop().text(""));
  }
  return true;
}

/**
 * `brush_basic_grease_pencil_paint_settings(layout, context, brush, props, compact=True)`
 * (`properties_paint_common.py:1740`).
 *
 * Aqui siempre se llama con `compact=True`, asi que las ramas `not compact` (las tres
 * `template_curve_mapping` y la etiqueta "Use Thickness Profile") no se alcanzan y no se
 * trasladan. Las ramas por tipo de pincel si, aunque con una primitiva activa lo normal es
 * entrar por la de las primitivas: dependen de datos, no de un literal.
 *
 * `tool` puede ser nulo en teoria; el Python lanza AttributeError al leer `tool.idname`,
 * y como algunas lecturas estan tras un `or` o un `and` que corta, lo hace en sitios
 * distintos segun el pincel. Cada vuelta anticipada de abajo es uno de esos sitios.
 */
static void brush_basic_grease_pencil_paint_settings(uiLayout *layout,
                                                     const bContext *C,
                                                     PointerRNA *brush,
                                                     PointerRNA *props)
{
  PointerRNA gp_settings = pointer_get(brush, "gpencil_settings");
  bToolRef *tool = tool_from_space_view3d_mode(C);
  if (gp_settings.data == nullptr) {
    return;
  }

  const char *grease_pencil_tool = enum_id(C, brush, "gpencil_tool");

  const bool draw_erase_tint = STR_ELEM(grease_pencil_tool, "DRAW", "ERASE", "TINT");
  if (!draw_erase_tint && tool == nullptr) {
    return;
  }
  if (draw_erase_tint || tool_is_primitive(tool)) {
    const char *size = "size";
    if (enum_is(C, brush, "use_locked_size", "SCENE")) {
      if (!STREQ(grease_pencil_tool, "DRAW") && tool == nullptr) {
        return;
      }
      if (STREQ(grease_pencil_tool, "DRAW") || tool_is_primitive(tool)) {
        size = "unprojected_radius";
      }
    }
    uiLayout *row = ui_row(layout, true);
    prop(row, brush, size, Prop().slider().text("Radius"));
    prop(row, brush, "use_pressure_size", Prop().text(""));

    /* `if brush.use_pressure_size and not compact:` nunca se cumple aqui. */

    row = ui_row(layout, true);
    prop(row, brush, "strength", Prop().slider().text("Strength"));
    prop(row, brush, "use_pressure_strength", Prop().text(""));

    /* `if brush.use_pressure_strength and not compact:` tampoco. */
  }

  /* `if props:`: un `bpy_struct` siempre es verdadero; solo falla sin dato. */
  if (props->data != nullptr) {
    prop(layout, props, "subdivision");
  }

  /* Brush details */
  if (tool == nullptr) {
    /* Aqui el Python lee `tool.idname` sin nada que corte antes. */
    return;
  }
  if (tool_is_primitive(tool)) {
    uiLayout *row = ui_row(layout, true);
    if (region_is_tool_header(C)) {
      prop(row, &gp_settings, "caps_type", Prop().text("").expand());
    }
    else {
      prop(row, &gp_settings, "caps_type", Prop().text("Caps Type"));
    }

    PointerRNA tool_settings_ptr = tool_settings(C);
    PointerRNA settings = pointer_get(&tool_settings_ptr, "gpencil_sculpt");
    /* compact */
    row = ui_row(layout, true);
    prop(row, &settings, "use_thickness_curve", Prop().text("").icon(ICON_SPHERECURVE));
    uiLayout *sub = ui_row(row, true);
    uiLayoutSetActive(sub,
                      settings.data != nullptr &&
                          RNA_boolean_get(&settings, "use_thickness_curve"));
    popover(sub, C, "TOPBAR_PT_gpencil_primitive", "Thickness Profile");
  }
  else if (STREQ(grease_pencil_tool, "DRAW")) {
    uiLayout *row = ui_row(layout, true);
    /* compact */
    prop(row, &gp_settings, "caps_type", Prop().text("").expand());
  }
  else if (enum_is(C, brush, "gpencil_tool", "FILL")) {
    const bool use_property_split_prev = uiLayoutGetPropSep(layout);
    /* compact */
    uiLayout *row = ui_row(layout, true);
    prop(row, &gp_settings, "fill_direction", Prop().text("").expand());

    row = ui_row(layout, true);
    prop(row, &gp_settings, "fill_factor");
    row = ui_row(layout, true);
    prop(row, &gp_settings, "dilate");
    row = ui_row(layout, true);
    prop(row, brush, "size", Prop().text("Thickness"));
    /* Rareza del Python: con `compact` nadie toca `use_property_split`, pero se
     * restaura igual. Se conserva. */
    uiLayoutSetPropSep(layout, use_property_split_prev);
  }
  else if (STREQ(grease_pencil_tool, "ERASE")) {
    prop(layout, &gp_settings, "eraser_mode", Prop().expand());
    prop(layout, &gp_settings, "use_active_layer_only");
    if (enum_is(C, &gp_settings, "eraser_mode", "HARD") ||
        enum_is(C, &gp_settings, "eraser_mode", "SOFT"))
    {
      prop(layout, &gp_settings, "use_keep_caps_eraser");
    }
  }
  else if (STREQ(grease_pencil_tool, "TINT")) {
    prop(layout, &gp_settings, "vertex_mode", Prop().text("Mode"));
    popover(layout, C, "VIEW3D_PT_tools_brush_falloff");
    prop(layout, &gp_settings, "use_active_layer_only");
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Primitivas de trazo
 * \{ */

/**
 * `_defs_grease_pencil_paint.grease_pencil_primitive_toolbar(context, layout, _tool,
 * props)` (`space_toolsystem_toolbar.py:2191`). El valor devuelto no lo usa nadie; se
 * conserva por fidelidad.
 */
static bool grease_pencil_primitive_toolbar(const bContext *C,
                                            uiLayout *layout,
                                            bToolRef * /*tool*/,
                                            PointerRNA *props)
{
  PointerRNA tool_settings_ptr = tool_settings(C);
  PointerRNA paint = pointer_get(&tool_settings_ptr, "gpencil_paint");
  PointerRNA brush = pointer_get(&paint, "brush");

  if (brush.data == nullptr) {
    return false;
  }

  PointerRNA gp_settings = pointer_get(&brush, "gpencil_settings");

  uiLayout *row = ui_row(layout, true);

  draw_popup_selector(row, C, &brush);

  if (!brush_basic__draw_color_selector(C, layout, &brush, &gp_settings)) {
    /* Excepcion en el Python: lo que sigue no llega a pintarse. */
    return false;
  }
  brush_basic_grease_pencil_paint_settings(layout, C, &brush, props);
  return true;
}

/* Los `draw_settings` del Python no aceptan `extra`: llamarlos con `extra=True` daria
 * TypeError antes de pintar nada. Por eso con `extra` no se pinta nada. */

void draw_grease_pencil_paint_line(const bContext *C,
                                   uiLayout *layout,
                                   bToolRef *tref,
                                   const bool extra)
{
  if (extra) {
    return;
  }
  PointerRNA props = op_props(tref, "grease_pencil.primitive_line");
  grease_pencil_primitive_toolbar(C, layout, tref, &props);
}

void draw_grease_pencil_paint_polyline(const bContext *C,
                                       uiLayout *layout,
                                       bToolRef *tref,
                                       const bool extra)
{
  if (extra) {
    return;
  }
  PointerRNA props = op_props(tref, "grease_pencil.primitive_polyline");
  grease_pencil_primitive_toolbar(C, layout, tref, &props);
}

void draw_grease_pencil_paint_arc(const bContext *C,
                                  uiLayout *layout,
                                  bToolRef *tref,
                                  const bool extra)
{
  if (extra) {
    return;
  }
  PointerRNA props = op_props(tref, "grease_pencil.primitive_arc");
  grease_pencil_primitive_toolbar(C, layout, tref, &props);
}

void draw_grease_pencil_paint_curve(const bContext *C,
                                    uiLayout *layout,
                                    bToolRef *tref,
                                    const bool extra)
{
  if (extra) {
    return;
  }
  PointerRNA props = op_props(tref, "grease_pencil.primitive_curve");
  grease_pencil_primitive_toolbar(C, layout, tref, &props);
}

void draw_grease_pencil_paint_box(const bContext *C,
                                  uiLayout *layout,
                                  bToolRef *tref,
                                  const bool extra)
{
  if (extra) {
    return;
  }
  PointerRNA props = op_props(tref, "grease_pencil.primitive_box");
  grease_pencil_primitive_toolbar(C, layout, tref, &props);
}

void draw_grease_pencil_paint_circle(const bContext *C,
                                     uiLayout *layout,
                                     bToolRef *tref,
                                     const bool extra)
{
  if (extra) {
    return;
  }
  PointerRNA props = op_props(tref, "grease_pencil.primitive_circle");
  grease_pencil_primitive_toolbar(C, layout, tref, &props);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cuentagotas
 * \{ */

/** `_defs_grease_pencil_paint.eyedropper` (`space_toolsystem_toolbar.py:2348`). */
void draw_grease_pencil_paint_eyedropper(const bContext *C,
                                         uiLayout *layout,
                                         bToolRef *tref,
                                         const bool extra)
{
  if (extra) {
    return;
  }
  PointerRNA props = op_props(tref, "ui.eyedropper_grease_pencil_color");
  uiLayout *row = ui_row(layout);
  uiLayoutSetPropSep(row, false);
  prop(row, &props, "mode", Prop().expand());

  if (enum_is(C, &props, "mode", "MATERIAL")) {
    uiLayout *col = ui_column(layout);
    prop(col, &props, "material_mode");
  }
  else if (enum_is(C, &props, "mode", "PALETTE")) {
    PointerRNA tool_settings_ptr = tool_settings(C);
    PointerRNA settings = pointer_get(&tool_settings_ptr, "gpencil_paint");

    uiLayout *col = ui_column(layout);

    row = ui_row(col, true);
    /* `row.template_ID(settings, "palette", new="palette.new")`: `rna_uiTemplateID`, sin
     * `open` ni `unlink`, filtro 'ALL', sin icono vivo y sin texto. Con `settings` a
     * `None` el Python da TypeError en esta linea, con la columna y la fila ya creadas. */
    if (settings.data == nullptr) {
      return;
    }
    if (RNA_struct_find_property(&settings, "palette") == nullptr) {
      fprintf(stderr,
              "Herramientas: propiedad no encontrada: %s.palette\n",
              RNA_struct_identifier(settings.type));
      return;
    }
    uiTemplateID(row,
                 C,
                 &settings,
                 "palette",
                 "palette.new",
                 nullptr,
                 nullptr,
                 UI_TEMPLATE_ID_FILTER_ALL,
                 false,
                 std::nullopt);
    if (pointer_get(&settings, "palette").data != nullptr) {
      uiTemplatePalette(col, &settings, "palette", true);
    }
  }
}

/** \} */

}  // namespace flipendo::ui::settings
