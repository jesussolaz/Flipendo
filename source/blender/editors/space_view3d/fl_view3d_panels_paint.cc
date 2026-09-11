/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 *
 * Ver `FL_view3d_panels.hh`.
 *
 * Los cuatro primeros paneles de la region `VIEW_3D WINDOW`: los contextuales de pintado
 * de vertices, de textura, de pesos y de escultura. El keymap nativo los abre con
 * `wm.call_panel` sobre la tecla W de cada uno de esos cuatro modos; mientras solo
 * existieran como clase de Python, esa tecla no habria abierto nada y no habria dado
 * error.
 *
 * La trampa de estos cuatro, y por que el volcado los ve casi vacios
 * ----------------------------------------------------------------
 * Su `draw()` de Python empieza leyendo `context.tool_settings.<modo>.brush`. En la
 * escena de fabrica no hay ningun modo de pintado iniciado: `vertex_paint`, `sculpt` y
 * `weight_paint` son punteros nulos o traen el pincel a `None`, asi que el Python lanza
 * `AttributeError`/`TypeError` y el `draw()` **se corta en esa linea**. La linea base
 * guarda justo eso: el panel a medio dibujar.
 *
 * Aqui NO se ha «codificado el fallo»: se transliteran los accesos en el mismo orden y se
 * corta donde el Python cortaba, de modo que con datos (en un modo de pintado de verdad)
 * los cuatro dibujan el panel entero, y sin datos dibujan lo mismo que dibujaba el
 * Python. La diferencia con el caso de `OUTLINER_MT_context_menu` —que se revirtio— es
 * que alli el corte lo producia una enumeracion dinamica sin items EN EL CONTEXTO DEL
 * VOLCADOR, es decir un artefacto del arnes; aqui lo produce la escena, y en la escena
 * real no se produce.
 *
 * Consecuencia honesta: de estos cuatro, el volcado de dibujo solo prueba la rama vacia.
 * Lo que queda verificado al 100 % es el registro y la posicion en la region.
 */

#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "FL_paint_common.hh"
#include "FL_ui_registry.hh"

#include "FL_view3d_panels.hh"

namespace blender::ed::view3d {

using flipendo::paint_common::prop_unified;
using flipendo::paint_common::prop_unified_color;
using flipendo::paint_common::prop_unified_color_picker;

/** `context.tool_settings`. */
static PointerRNA tool_settings_ptr(const bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  if (scene == nullptr || scene->toolsettings == nullptr) {
    return PointerRNA_NULL;
  }
  return RNA_pointer_create_discrete(&scene->id, &RNA_ToolSettings, scene->toolsettings);
}

/** `getattr(ptr, name)` cuando el atributo es un puntero. Nulo si `ptr` ya era nulo: es
 * el `AttributeError` del Python sobre `None`. */
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

/* -------------------------------------------------------------------- */
/** \name El bloque de color que comparten tres de los cuatro
 * \{ */

/**
 * ```
 * if capabilities.has_color:
 *     split = layout.split(factor=0.1)
 *     UnifiedPaintPanel.prop_unified_color(split, context, brush, "color", text="")
 *     UnifiedPaintPanel.prop_unified_color_picker(split, context, brush, "color", value_slider=True)
 *     layout.prop(brush, "blend", text="")
 * ```
 *
 * Identico en `paint_vertex`, `paint_texture` y `sculpt`. Devuelve `false` si el Python
 * se habria cortado dentro del bloque.
 */
static bool draw_color_block(uiLayout *layout, const bContext *C, PointerRNA *brush)
{
  uiLayout *split = &layout->split(0.1f, false);
  if (!prop_unified_color(split, C, brush, "color", "")) {
    return false;
  }
  if (!prop_unified_color_picker(split, C, brush, "color", true)) {
    return false;
  }
  if (brush->data == nullptr) {
    return false;
  }
  layout->prop(brush, "blend", UI_ITEM_NONE, "", ICON_NONE);
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_PT_paint_vertex_context_menu
 * \{ */

static void paint_vertex_context_menu_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ts = tool_settings_ptr(C);
  /* `context.tool_settings.vertex_paint.brush`: sin `vertex_paint` el Python revienta
   * ANTES de dibujar nada. */
  PointerRNA vpaint = sub_ptr(&ts, "vertex_paint");
  if (vpaint.data == nullptr) {
    return;
  }
  PointerRNA brush = sub_ptr(&vpaint, "brush");
  /* `brush.vertex_paint_capabilities` con el pincel a `None`: AttributeError. */
  PointerRNA caps = sub_ptr(&brush, "vertex_paint_capabilities");
  if (caps.data == nullptr) {
    return;
  }

  if (RNA_boolean_get(&caps, "has_color")) {
    if (!draw_color_block(layout, C, &brush)) {
      return;
    }
  }

  if (prop_unified(layout,
                   C,
                   &brush,
                   "size",
                   "use_unified_size",
                   "use_pressure_size",
                   ICON_NONE,
                   std::nullopt,
                   true,
                   false) == nullptr)
  {
    return;
  }
  prop_unified(layout,
               C,
               &brush,
               "strength",
               "use_unified_strength",
               "use_pressure_strength",
               ICON_NONE,
               std::nullopt,
               true,
               false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_PT_paint_texture_context_menu
 * \{ */

static void paint_texture_context_menu_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ts = tool_settings_ptr(C);
  PointerRNA imapaint = sub_ptr(&ts, "image_paint");
  if (imapaint.data == nullptr) {
    return;
  }
  PointerRNA brush = sub_ptr(&imapaint, "brush");
  PointerRNA caps = sub_ptr(&brush, "image_paint_capabilities");
  if (caps.data == nullptr) {
    return;
  }

  if (RNA_boolean_get(&caps, "has_color")) {
    if (!draw_color_block(layout, C, &brush)) {
      return;
    }
  }

  /* A diferencia del panel de vertices, aqui radio y fuerza van dentro de `has_radius`. */
  if (RNA_boolean_get(&caps, "has_radius")) {
    if (prop_unified(layout,
                     C,
                     &brush,
                     "size",
                     "use_unified_size",
                     "use_pressure_size",
                     ICON_NONE,
                     std::nullopt,
                     true,
                     false) == nullptr)
    {
      return;
    }
    prop_unified(layout,
                 C,
                 &brush,
                 "strength",
                 "use_unified_strength",
                 "use_pressure_strength",
                 ICON_NONE,
                 std::nullopt,
                 true,
                 false);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_PT_paint_weight_context_menu
 * \{ */

static void paint_weight_context_menu_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ts = tool_settings_ptr(C);
  PointerRNA wpaint = sub_ptr(&ts, "weight_paint");
  if (wpaint.data == nullptr) {
    return;
  }
  PointerRNA brush = sub_ptr(&wpaint, "brush");

  /* El peso NO lleva `pressure_name`; el radio y la fuerza si. */
  if (prop_unified(layout,
                   C,
                   &brush,
                   "weight",
                   "use_unified_weight",
                   nullptr,
                   ICON_NONE,
                   std::nullopt,
                   true,
                   false) == nullptr)
  {
    return;
  }
  if (prop_unified(layout,
                   C,
                   &brush,
                   "size",
                   "use_unified_size",
                   "use_pressure_size",
                   ICON_NONE,
                   std::nullopt,
                   true,
                   false) == nullptr)
  {
    return;
  }
  prop_unified(layout,
               C,
               &brush,
               "strength",
               "use_unified_strength",
               "use_pressure_strength",
               ICON_NONE,
               std::nullopt,
               true,
               false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name VIEW3D_PT_sculpt_context_menu
 * \{ */

static void sculpt_context_menu_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ts = tool_settings_ptr(C);
  PointerRNA sculpt = sub_ptr(&ts, "sculpt");
  if (sculpt.data == nullptr) {
    return;
  }
  PointerRNA brush = sub_ptr(&sculpt, "brush");
  PointerRNA caps = sub_ptr(&brush, "sculpt_capabilities");
  if (caps.data == nullptr) {
    return;
  }

  if (RNA_boolean_get(&caps, "has_color")) {
    if (!draw_color_block(layout, C, &brush)) {
      return;
    }
  }

  /* ```
   * ups = context.tool_settings.unified_paint_settings
   * size = "size"
   * size_owner = ups if ups.use_unified_size else brush
   * if size_owner.use_locked_size == 'SCENE':
   *     size = "unprojected_radius"
   * ```
   * `use_locked_size` es una enumeracion, asi que se compara por identificador y no por
   * el valor crudo del DNA. */
  PointerRNA ups = flipendo::paint_common::unified_paint_settings(C);
  const char *size = "size";
  PointerRNA *size_owner = (ups.data != nullptr && RNA_boolean_get(&ups, "use_unified_size")) ?
                               &ups :
                               &brush;
  if (size_owner->data == nullptr) {
    /* `None.use_locked_size`: AttributeError. */
    return;
  }
  if (RNA_enum_is_equal(
          const_cast<bContext *>(C), size_owner, "use_locked_size", "SCENE"))
  {
    size = "unprojected_radius";
  }

  if (prop_unified(layout,
                   C,
                   &brush,
                   size,
                   "use_unified_size",
                   "use_pressure_size",
                   ICON_NONE,
                   IFACE_("Radius"),
                   true,
                   false) == nullptr)
  {
    return;
  }
  if (prop_unified(layout,
                   C,
                   &brush,
                   "strength",
                   "use_unified_strength",
                   "use_pressure_strength",
                   ICON_NONE,
                   std::nullopt,
                   true,
                   false) == nullptr)
  {
    return;
  }

  /* A partir de aqui el Python solo toca `brush`, que ya sabemos que no es nulo: si lo
   * fuera, `prop_unified` habria cortado antes. */
  if (RNA_boolean_get(&caps, "has_auto_smooth")) {
    layout->prop(&brush, "auto_smooth_factor", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);
  }

  if (RNA_boolean_get(&caps, "has_normal_weight")) {
    layout->prop(&brush, "normal_weight", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);
  }

  if (RNA_boolean_get(&caps, "has_pinch_factor")) {
    /* `text = "Magnify"` para BLOB y SNAKE_HOOK, `"Pinch"` para el resto. */
    const bool magnify = RNA_enum_is_equal(
                             const_cast<bContext *>(C), &brush, "sculpt_tool", "BLOB") ||
                         RNA_enum_is_equal(
                             const_cast<bContext *>(C), &brush, "sculpt_tool", "SNAKE_HOOK");
    layout->prop(&brush,
                 "crease_pinch_factor",
                 UI_ITEM_R_SLIDER,
                 magnify ? IFACE_("Magnify") : IFACE_("Pinch"),
                 ICON_NONE);
  }

  if (RNA_boolean_get(&caps, "has_rake_factor")) {
    layout->prop(&brush, "rake_factor", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);
  }

  if (RNA_boolean_get(&caps, "has_plane_offset")) {
    layout->prop(&brush, "plane_offset", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);
    layout->prop(&brush, "plane_trim", UI_ITEM_R_SLIDER, IFACE_("Distance"), ICON_NONE);
  }

  if (RNA_boolean_get(&caps, "has_height")) {
    layout->prop(&brush, "height", UI_ITEM_R_SLIDER, IFACE_("Height"), ICON_NONE);
  }
}

/** \} */

/* El ORDEN de esta tabla es el de la lista de la region: son los cuatro primeros de
 * `VIEW_3D WINDOW` en la linea base, y por eso se pueden migrar sin tocar los otros
 * siete. Ver la cabecera de `FL_view3d_panels.hh`. */
static const flipendo::PanelDecl view3d_paint_panels[] = {
    {
        /*idname*/ "VIEW3D_PT_paint_vertex_context_menu",
        /*label*/ N_("Vertex Paint"),
        /*category*/ nullptr,
        /*context*/ nullptr,
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ paint_vertex_context_menu_draw,
    },
    {
        /*idname*/ "VIEW3D_PT_paint_texture_context_menu",
        /*label*/ N_("Texture Paint"),
        /*category*/ nullptr,
        /*context*/ nullptr,
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ paint_texture_context_menu_draw,
    },
    {
        /*idname*/ "VIEW3D_PT_paint_weight_context_menu",
        /*label*/ N_("Weights"),
        /*category*/ nullptr,
        /*context*/ nullptr,
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ paint_weight_context_menu_draw,
    },
    {
        /*idname*/ "VIEW3D_PT_sculpt_context_menu",
        /*label*/ N_("Sculpt"),
        /*category*/ nullptr,
        /*context*/ nullptr,
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ sculpt_context_menu_draw,
    },
};

void view3d_paint_panels_register(ARegionType *art)
{
  flipendo::panels_register(art, SPACE_VIEW3D, {view3d_paint_panels, ARRAY_SIZE(view3d_paint_panels)});
}

}  // namespace blender::ed::view3d
