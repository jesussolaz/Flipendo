/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 *
 * Ver `FL_view3d_panels.hh`.
 *
 * Paneles 5 a 11 de la region `VIEW_3D WINDOW`: los tres emergentes de la escultura de
 * curvas y los cuatro contextuales del lapiz de cera. Con estos y los cuatro de
 * `fl_view3d_panels_paint.cc` la region queda **entera en C++**, que es lo que permite
 * dejar de preocuparse por el orden: ya no queda ningun `PanelType` de Python detras.
 *
 * Los siete se abren con `wm.call_panel` desde el keymap nativo, asi que sin ellos la
 * tecla W de esos modos no habria abierto nada y no habria dado error.
 *
 * Igual que en los cuatro anteriores, en la escena de fabrica el `draw()` del Python se
 * corta al leer el pincel (`AttributeError` / `TypeError`), asi que la linea base tiene
 * los siete bloques a medio dibujar. Se transliteran los accesos EN EL MISMO ORDEN y se
 * corta donde cortaba el Python: con datos de verdad los siete dibujan entero.
 */

#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "RNA_access.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_types.hh"

#include "FL_paint_common.hh"
#include "FL_ui_registry.hh"

#include "FL_view3d_panels.hh"

namespace blender::ed::view3d {

using flipendo::paint_common::paint_settings;
using flipendo::paint_common::unified_paint_settings;

/** `context.tool_settings`. */
static PointerRNA tool_settings_ptr(const bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  if (scene == nullptr || scene->toolsettings == nullptr) {
    return PointerRNA_NULL;
  }
  return RNA_pointer_create_discrete(&scene->id, &RNA_ToolSettings, scene->toolsettings);
}

/** `getattr(ptr, name)` cuando el atributo es un puntero; nulo si `ptr` ya era nulo. */
static PointerRNA sub_ptr(PointerRNA *ptr, const char *name)
{
  if (ptr == nullptr || ptr->data == nullptr) {
    return PointerRNA_NULL;
  }
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);
  if (prop == nullptr || RNA_property_type(prop) != PROP_POINTER) {
    return PointerRNA_NULL;
  }
  return RNA_property_pointer_get(ptr, prop);
}

/** `brush.curves_sculpt_settings`, o nulo si el Python habria lanzado la excepcion. */
static PointerRNA curves_sculpt_settings(PointerRNA *brush)
{
  return sub_ptr(brush, "curves_sculpt_settings");
}

/* -------------------------------------------------------------------- */
/** \name El bloque de la capa activa, que repiten tres de los cuatro del lapiz
 * \{ */

/**
 * ```
 * layer = context.object.data.layers.active
 * if layer:
 *     layout.label(text="Active Layer")
 *     row = layout.row(align=True)
 *     row.operator_context = 'EXEC_REGION_WIN'
 *     row.menu("GREASE_PENCIL_MT_Layers", text="", icon='OUTLINER_DATA_GP_LAYER')
 *     row.prop(layer, "name", text="")
 *     row.operator("grease_pencil.layer_remove", text="", icon='X')
 * ```
 *
 * Hay DOS resultados distintos y hay que distinguirlos, porque el Python los distingue:
 * si el objeto no es un lapiz de cera, `.layers` no existe y el `draw()` **revienta ahi**
 * (`AttributeError: 'Mesh' object has no attribute 'layers'`, que es justo lo que sale en
 * la linea base); si existe pero no hay capa activa, el `if` es falso y el dibujo
 * **continua**.
 *
 * \return `false` solo en el primer caso.
 */
static bool draw_active_layer_block(uiLayout *layout, const bContext *C)
{
  /* `context.object`, NO `context.active_object`: son dos miembros distintos del
   * contexto y el Python usa este aqui (y `active_object` unos renglones mas abajo,
   * para el material). En la vista 3D coinciden, pero la diferencia es real —el editor
   * de Propiedades fijado devuelve otro— y copiar el nombre equivocado es justo la
   * clase de desliz que esta migracion persigue. */
  PointerRNA ob_ptr = CTX_data_pointer_get(C, "object");
  Object *ob = static_cast<Object *>(ob_ptr.data);
  if (ob == nullptr || ob->data == nullptr) {
    /* `None.data` / `None.layers`: AttributeError. */
    return false;
  }
  PointerRNA data_ptr = RNA_id_pointer_create(static_cast<ID *>(ob->data));
  if (RNA_struct_find_property(&data_ptr, "layers") == nullptr) {
    /* El objeto no tiene capas: AttributeError, el `draw()` se corta. */
    return false;
  }

  PointerRNA layer;
  if (!RNA_path_resolve(&data_ptr, "layers.active", &layer, nullptr) || layer.data == nullptr) {
    /* Hay capas pero ninguna activa: `if layer:` es falso y se sigue dibujando. */
    return true;
  }

  layout->label(IFACE_("Active Layer"), ICON_NONE);
  uiLayout *row = &layout->row(true);
  uiLayoutSetOperatorContext(row, WM_OP_EXEC_REGION_WIN);
  row->menu("GREASE_PENCIL_MT_Layers", "", ICON_OUTLINER_DATA_GP_LAYER);
  row->prop(&layer, "name", UI_ITEM_NONE, "", ICON_NONE);
  row->op("GREASE_PENCIL_OT_layer_remove", "", ICON_X);
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_PT_curves_sculpt_add_shape
 * \{ */

static void curves_sculpt_add_shape_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false); /* Sin animacion. */

  /* `settings = UnifiedPaintPanel.paint_settings(context)` y acto seguido
   * `settings.brush`: con `settings` a `None` el Python revienta aqui. */
  PointerRNA settings = paint_settings(C);
  if (settings.data == nullptr) {
    return;
  }
  PointerRNA brush = sub_ptr(&settings, "brush");
  PointerRNA cs = curves_sculpt_settings(&brush);
  if (cs.data == nullptr) {
    return;
  }

  uiLayout *col = &layout->column(true, IFACE_("Interpolate"));
  col->prop(&cs, "use_length_interpolate", UI_ITEM_NONE, IFACE_("Length"), ICON_NONE);
  col->prop(&cs, "use_radius_interpolate", UI_ITEM_NONE, IFACE_("Radius"), ICON_NONE);
  col->prop(&cs, "use_shape_interpolate", UI_ITEM_NONE, IFACE_("Shape"), ICON_NONE);
  col->prop(&cs, "use_point_count_interpolate", UI_ITEM_NONE, IFACE_("Point Count"), ICON_NONE);

  col = &layout->column(false);
  uiLayoutSetActive(col, !RNA_boolean_get(&cs, "use_length_interpolate"));
  col->prop(&cs, "curve_length", UI_ITEM_NONE, IFACE_("Length"), ICON_NONE);

  col = &layout->column(false);
  uiLayoutSetActive(col, !RNA_boolean_get(&cs, "use_radius_interpolate"));
  col->prop(&cs, "curve_radius", UI_ITEM_NONE, IFACE_("Radius"), ICON_NONE);

  col = &layout->column(false);
  uiLayoutSetActive(col, !RNA_boolean_get(&cs, "use_point_count_interpolate"));
  col->prop(&cs, "points_per_curve", UI_ITEM_NONE, IFACE_("Points"), ICON_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_PT_curves_sculpt_parameter_falloff
 * \{ */

static void curves_sculpt_parameter_falloff_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA settings = paint_settings(C);
  if (settings.data == nullptr) {
    return;
  }
  PointerRNA brush = sub_ptr(&settings, "brush");
  PointerRNA cs = curves_sculpt_settings(&brush);
  if (cs.data == nullptr) {
    return;
  }

  /* `template_curve_mapping(..., type=0, levels=False, brush=False,
   * use_negative_slope=False, show_tone=False)`: todos los valores por defecto. */
  uiTemplateCurveMapping(layout, &cs, "curve_parameter_falloff", 0, false, false, false, false);

  uiLayout *row = &layout->row(true);
  static const struct {
    int icon;
    const char *shape;
  } presets[] = {
      {ICON_SMOOTHCURVE, "SMOOTH"},
      {ICON_SPHERECURVE, "ROUND"},
      {ICON_ROOTCURVE, "ROOT"},
      {ICON_SHARPCURVE, "SHARP"},
      {ICON_LINCURVE, "LINE"},
      {ICON_NOCURVE, "MAX"},
  };
  for (const auto &preset : presets) {
    PointerRNA props = row->op("BRUSH_OT_sculpt_curves_falloff_preset", "", preset.icon);
    if (props.data != nullptr) {
      /* `shape` es una enumeracion: se asigna por identificador, nunca con
       * `RNA_string_set`. */
      RNA_enum_set_identifier(const_cast<bContext *>(C), &props, "shape", preset.shape);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_PT_curves_sculpt_grow_shrink_scaling
 * \{ */

static void curves_sculpt_grow_shrink_scaling_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false); /* Sin animacion. */

  PointerRNA settings = paint_settings(C);
  if (settings.data == nullptr) {
    return;
  }
  PointerRNA brush = sub_ptr(&settings, "brush");
  PointerRNA cs = curves_sculpt_settings(&brush);
  if (cs.data == nullptr) {
    return;
  }

  layout->prop(&cs, "use_uniform_scale", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(&cs, "minimum_length", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_PT_greasepencil_draw_context_menu
 * \{ */

static void greasepencil_draw_context_menu_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ts = tool_settings_ptr(C);
  PointerRNA settings = sub_ptr(&ts, "gpencil_paint");
  if (settings.data == nullptr) {
    return;
  }
  PointerRNA brush = sub_ptr(&settings, "brush");
  PointerRNA gp_settings = sub_ptr(&brush, "gpencil_settings");
  if (gp_settings.data == nullptr) {
    /* `brush.gpencil_settings` con el pincel a `None`: AttributeError. */
    return;
  }

  bContext *C_mut = const_cast<bContext *>(C);
  const bool is_pin_vertex = RNA_enum_is_equal(
      C_mut, &gp_settings, "brush_draw_mode", "VERTEXCOLOR");
  const bool is_vertex = RNA_enum_is_equal(C_mut, &settings, "color_mode", "VERTEXCOLOR") ||
                         RNA_enum_is_equal(C_mut, &brush, "gpencil_tool", "TINT") || is_pin_vertex;

  const bool is_erase = RNA_enum_is_equal(C_mut, &brush, "gpencil_tool", "ERASE");
  const bool is_cutter = RNA_enum_is_equal(C_mut, &brush, "gpencil_tool", "CUTTER");
  const bool is_fill = RNA_enum_is_equal(C_mut, &brush, "gpencil_tool", "FILL");
  const bool is_eyedropper = RNA_enum_is_equal(C_mut, &brush, "gpencil_tool", "EYEDROPPER");

  if (!(is_erase || is_cutter || is_eyedropper) && is_vertex) {
    uiLayout *split = &layout->split(0.1f, false);
    split->prop(&brush, "color", UI_ITEM_NONE, "", ICON_NONE);
    uiTemplateColorPicker(split, &brush, "color", true, false, false, false);

    uiLayout *col = &layout->column(false);
    col->separator();
    /* `col.prop_menu_enum(gp_settings, "vertex_mode", text="Mode")`. `rna_uiItemMenuEnumR`
     * avisa y no dibuja si la propiedad no existe; `uiItemMenuEnumR_prop` la
     * desreferencia sin mirar, asi que la comprobacion va aqui. */
    if (PropertyRNA *vertex_mode = RNA_struct_find_property(&gp_settings, "vertex_mode")) {
      uiItemMenuEnumR_prop(col, &gp_settings, vertex_mode, IFACE_("Mode"), ICON_NONE);
    }
    col->separator();
  }

  if (!(is_fill || is_cutter || is_erase)) {
    /* `radius = "size" if (brush.use_locked_size == 'VIEW') else "unprojected_radius"`. */
    const char *radius = RNA_enum_is_equal(C_mut, &brush, "use_locked_size", "VIEW") ?
                             "size" :
                             "unprojected_radius";
    layout->prop(&brush, radius, UI_ITEM_R_SLIDER, IFACE_("Radius"), ICON_NONE);
  }
  if (is_erase) {
    layout->prop(&brush, "size", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);
  }
  if (!(is_erase || is_fill || is_cutter)) {
    layout->prop(&gp_settings, "pen_strength", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  if (!draw_active_layer_block(layout, C)) {
    return;
  }

  layout->label(IFACE_("Active Material"), ICON_NONE);
  uiLayout *row = &layout->row(true);
  row->menu("VIEW3D_MT_greasepencil_material_active", "", ICON_MATERIAL);
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    /* `ob.active_material` con `ob` a `None`: AttributeError. */
    return;
  }
  PointerRNA ob_ptr = RNA_id_pointer_create(&ob->id);
  PointerRNA mat = sub_ptr(&ob_ptr, "active_material");
  if (mat.data != nullptr) {
    row->prop(&mat, "name", UI_ITEM_NONE, "", ICON_NONE);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_PT_greasepencil_sculpt_context_menu
 * \{ */

static void greasepencil_sculpt_context_menu_draw(const bContext *C, Panel *panel)
{
  PointerRNA ts = tool_settings_ptr(C);
  PointerRNA gsp = sub_ptr(&ts, "gpencil_sculpt_paint");
  if (gsp.data == nullptr) {
    return;
  }
  PointerRNA brush = sub_ptr(&gsp, "brush");
  uiLayout *layout = panel->layout;

  PointerRNA ups = unified_paint_settings(C);
  PointerRNA *size_owner = (ups.data != nullptr && RNA_boolean_get(&ups, "use_unified_size")) ?
                               &ups :
                               &brush;
  PointerRNA *strength_owner = (ups.data != nullptr &&
                                RNA_boolean_get(&ups, "use_unified_strength")) ?
                                   &ups :
                                   &brush;
  if (size_owner->data == nullptr) {
    return;
  }
  layout->prop(size_owner, "size", UI_ITEM_NONE, "", ICON_NONE);
  if (strength_owner->data == nullptr) {
    return;
  }
  layout->prop(strength_owner, "strength", UI_ITEM_NONE, "", ICON_NONE);

  draw_active_layer_block(layout, C);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_PT_greasepencil_vertex_paint_context_menu
 * \{ */

static void greasepencil_vertex_paint_context_menu_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ts = tool_settings_ptr(C);
  PointerRNA settings = sub_ptr(&ts, "gpencil_vertex_paint");
  if (settings.data == nullptr) {
    return;
  }
  PointerRNA brush = sub_ptr(&settings, "brush");
  PointerRNA gp_settings = sub_ptr(&brush, "gpencil_settings");
  if (gp_settings.data == nullptr) {
    return;
  }
  PointerRNA ups = unified_paint_settings(C);

  /* `col = layout.column()` se crea ANTES del `if`, y dentro se reasigna a otra. */
  uiLayout *col = &layout->column(false);

  bContext *C_mut = const_cast<bContext *>(C);
  const bool draw_or_replace = RNA_enum_is_equal(
                                   C_mut, &brush, "gpencil_vertex_tool", "DRAW") ||
                               RNA_enum_is_equal(C_mut, &brush, "gpencil_vertex_tool", "REPLACE");
  if (draw_or_replace) {
    /* El `split` se crea ANTES de tocar `ups`, como en el Python: si `ups` no estuviera
     * (imposible hoy, es un struct dentro de `ToolSettings`) la fila ya se habria
     * creado cuando salta la excepcion. */
    uiLayout *split = &layout->split(0.1f, false);
    if (ups.data == nullptr) {
      return;
    }
    split->prop(&ups, "color", UI_ITEM_NONE, "", ICON_NONE);
    uiTemplateColorPicker(split, &ups, "color", true, false, false, false);

    col = &layout->column(false);
    col->separator();
    col->prop(&gp_settings, "vertex_mode", UI_ITEM_NONE, "", ICON_NONE);
    col->separator();
  }

  uiLayout *row = &col->row(true);
  if (ups.data == nullptr) {
    return;
  }
  row->prop(&ups, "size", UI_ITEM_NONE, IFACE_("Radius"), ICON_NONE);
  row->prop(&brush, "use_pressure_size", UI_ITEM_NONE, "", ICON_STYLUS_PRESSURE);

  const bool draw_blur_smear = RNA_enum_is_equal(C_mut, &brush, "gpencil_vertex_tool", "DRAW") ||
                               RNA_enum_is_equal(C_mut, &brush, "gpencil_vertex_tool", "BLUR") ||
                               RNA_enum_is_equal(C_mut, &brush, "gpencil_vertex_tool", "SMEAR");
  if (draw_blur_smear) {
    row = &layout->row(true);
    row->prop(&brush, "strength", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);
    row->prop(&brush, "use_pressure_strength", UI_ITEM_NONE, "", ICON_STYLUS_PRESSURE);
  }

  draw_active_layer_block(layout, C);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_PT_greasepencil_weight_context_menu
 * \{ */

static void greasepencil_weight_context_menu_draw(const bContext *C, Panel *panel)
{
  PointerRNA ts = tool_settings_ptr(C);
  PointerRNA settings = sub_ptr(&ts, "gpencil_weight_paint");
  if (settings.data == nullptr) {
    return;
  }
  PointerRNA brush = sub_ptr(&settings, "brush");
  uiLayout *layout = panel->layout;

  flipendo::paint_common::brush_basic_grease_pencil_weight_settings(layout, C, &brush, false);
}

/** \} */

/* El ORDEN de esta tabla es el de la lista de la region: van detras de los cuatro de
 * `fl_view3d_panels_paint.cc`, que se dan de alta antes. */
static const flipendo::PanelDecl view3d_curves_gp_panels[] = {
    {
        /*idname*/ "VIEW3D_PT_curves_sculpt_add_shape",
        /*label*/ N_("Curves Sculpt Add Curve Options"),
        /*category*/ nullptr,
        /*context*/ nullptr,
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ curves_sculpt_add_shape_draw,
    },
    {
        /*idname*/ "VIEW3D_PT_curves_sculpt_parameter_falloff",
        /*label*/ N_("Curves Sculpt Parameter Falloff"),
        /*category*/ nullptr,
        /*context*/ nullptr,
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ curves_sculpt_parameter_falloff_draw,
    },
    {
        /*idname*/ "VIEW3D_PT_curves_sculpt_grow_shrink_scaling",
        /*label*/ N_("Curves Grow/Shrink Scaling"),
        /*category*/ nullptr,
        /*context*/ nullptr,
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ curves_sculpt_grow_shrink_scaling_draw,
        /*draw_header*/ nullptr,
        /*draw_header_preset*/ nullptr,
        /*poll*/ nullptr,
        /*flag*/ 0,
        /*order*/ 0,
        /*ui_units_x*/ 12,
    },
    {
        /*idname*/ "VIEW3D_PT_greasepencil_draw_context_menu",
        /*label*/ N_("Draw"),
        /*category*/ nullptr,
        /*context*/ nullptr,
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ greasepencil_draw_context_menu_draw,
        /*draw_header*/ nullptr,
        /*draw_header_preset*/ nullptr,
        /*poll*/ nullptr,
        /*flag*/ 0,
        /*order*/ 0,
        /*ui_units_x*/ 12,
    },
    {
        /*idname*/ "VIEW3D_PT_greasepencil_sculpt_context_menu",
        /*label*/ N_("Sculpt"),
        /*category*/ nullptr,
        /*context*/ nullptr,
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ greasepencil_sculpt_context_menu_draw,
        /*draw_header*/ nullptr,
        /*draw_header_preset*/ nullptr,
        /*poll*/ nullptr,
        /*flag*/ 0,
        /*order*/ 0,
        /*ui_units_x*/ 12,
    },
    {
        /*idname*/ "VIEW3D_PT_greasepencil_vertex_paint_context_menu",
        /*label*/ N_("Vertex Paint"),
        /*category*/ nullptr,
        /*context*/ nullptr,
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ greasepencil_vertex_paint_context_menu_draw,
        /*draw_header*/ nullptr,
        /*draw_header_preset*/ nullptr,
        /*poll*/ nullptr,
        /*flag*/ 0,
        /*order*/ 0,
        /*ui_units_x*/ 12,
    },
    {
        /*idname*/ "VIEW3D_PT_greasepencil_weight_context_menu",
        /*label*/ N_("Weight Paint"),
        /*category*/ nullptr,
        /*context*/ nullptr,
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ greasepencil_weight_context_menu_draw,
        /*draw_header*/ nullptr,
        /*draw_header_preset*/ nullptr,
        /*poll*/ nullptr,
        /*flag*/ 0,
        /*order*/ 0,
        /*ui_units_x*/ 12,
    },
};

void view3d_curves_gp_panels_register(ARegionType *art)
{
  flipendo::panels_register(
      art, SPACE_VIEW3D, {view3d_curves_gp_panels, ARRAY_SIZE(view3d_curves_gp_panels)});
}

}  // namespace blender::ed::view3d
