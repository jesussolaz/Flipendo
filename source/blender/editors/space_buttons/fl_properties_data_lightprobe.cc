/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spbuttons
 *
 * La pestana de datos de la Sonda de Luz, en C++ nativo. Sustituye
 * `scripts/startup/bl_ui/properties_data_lightprobe.py` **entera**: sus trece
 * paneles. Por pestanas completas, nunca panel suelto (`UI-A-CPP.md`).
 *
 * Es la primera de esta familia sin lista ni menu: trece `Panel` y nada mas, con
 * `PropertiesAnimationMixin` como unica dependencia compartida — y ya extraida en
 * `FL_properties_ui.hpp`.
 *
 * DOS COSAS QUE HAY QUE SABER ANTES DE LEER EL CODIGO:
 *
 * 1. **El motor se mira por `context.engine`, no por `scene.render.engine`.** No
 *    es lo mismo: `context.engine` es `CTX_data_engine_type(C)->idname`, o sea el
 *    resultado de `RE_engines_find()`, que **cae a EEVEE** cuando el nombre
 *    guardado en la escena no corresponde a ningun motor registrado. La pestana
 *    del Volumen, al lado, si usa `scene.render.engine` crudo. Copiar aqui aquel
 *    `poll` cambiaria el comportamiento en una escena con un motor desconocido.
 *
 * 2. **Tres de los trece paneles no los dibuja nadie hoy, y no es cosa de esta
 *    migracion.** `DATA_PT_lightprobe`, `DATA_PT_lightprobe_visibility` y
 *    `DATA_PT_lightprobe_display` declaran `COMPAT_ENGINES = {'BLENDER_RENDER'}`,
 *    y `BLENDER_RENDER` **no existe** como motor en 4.5: los unicos registrados
 *    son `BLENDER_EEVEE_NEXT` y `BLENDER_WORKBENCH`. Su `poll` dice que no en
 *    cualquier escena, con Python y con C++. Se migran igualmente y con el mismo
 *    `COMPAT_ENGINES`, porque la doctrina es migrar, no decidir por el original
 *    que su codigo sobra; pero su `draw()` no lo puede cubrir ningun volcado y
 *    eso queda escrito aqui y en el informe en vez de dado por bueno.
 */

#include <optional>

#include "DNA_lightprobe_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "RE_engine.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "FL_properties_ui.hpp"
#include "FL_ui_registry.hh"

#include "buttons_intern.hh"

static LightProbe *context_lightprobe(const bContext *C)
{
  return static_cast<LightProbe *>(
      CTX_data_pointer_get_type(C, "lightprobe", &RNA_LightProbe).data);
}

/**
 * El puntero RNA de la sonda.
 *
 * `RNA_pointer_create_discrete()` pasa por `rna_pointer_refine()`, y `LightProbe`
 * tiene `RNA_def_struct_refine_func`, asi que lo que sale es `LightProbeSphere`,
 * `LightProbePlane` o `LightProbeVolume` segun el tipo — que es exactamente lo
 * que el volcado escribe en `rna=`. Construirlo sin refinar pondria
 * `LightProbe.influence_type[0]` donde el Python pone
 * `LightProbeSphere.influence_type[0]`.
 */
static PointerRNA lightprobe_ptr(LightProbe *probe)
{
  return RNA_pointer_create_discrete(&probe->id, &RNA_LightProbe, probe);
}

/** `context.engine`, o sea `CTX_data_engine_type(C)->idname`. */
static const char *context_engine(const bContext *C)
{
  const RenderEngineType *engine_type = CTX_data_engine_type(C);
  return (engine_type != nullptr) ? engine_type->idname : "";
}

static bool engine_is(const bContext *C, const char *idname)
{
  return STREQ(context_engine(C), idname);
}

/** `poll` de `DataButtonsPanel` con `COMPAT_ENGINES = {'BLENDER_RENDER'}`. */
static bool lightprobe_poll_render(const bContext *C, PanelType * /*pt*/)
{
  return context_lightprobe(C) != nullptr && engine_is(C, "BLENDER_RENDER");
}

/** `COMPAT_ENGINES = {'BLENDER_EEVEE_NEXT'}`. */
static bool lightprobe_poll_eevee(const bContext *C, PanelType * /*pt*/)
{
  return context_lightprobe(C) != nullptr && engine_is(C, "BLENDER_EEVEE_NEXT");
}

/** `COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_EEVEE_NEXT'}`. */
static bool lightprobe_poll_render_or_eevee(const bContext *C, PanelType * /*pt*/)
{
  return context_lightprobe(C) != nullptr &&
         (engine_is(C, "BLENDER_RENDER") || engine_is(C, "BLENDER_EEVEE_NEXT"));
}

/** `DATA_PT_lightprobe_capture`: ademas, `type in {'SPHERE', 'PLANE'}`. */
static bool lightprobe_poll_capture(const bContext *C, PanelType * /*pt*/)
{
  const LightProbe *probe = context_lightprobe(C);
  return probe != nullptr && ELEM(probe->type, LIGHTPROBE_TYPE_SPHERE, LIGHTPROBE_TYPE_PLANE) &&
         engine_is(C, "BLENDER_EEVEE_NEXT");
}

/** `DATA_PT_lightprobe_bake`: ademas, `type == 'VOLUME'`. */
static bool lightprobe_poll_bake(const bContext *C, PanelType * /*pt*/)
{
  const LightProbe *probe = context_lightprobe(C);
  return probe != nullptr && probe->type == LIGHTPROBE_TYPE_VOLUME &&
         engine_is(C, "BLENDER_EEVEE_NEXT");
}

/** `DATA_PT_lightprobe_parallax`: ademas, `type == 'SPHERE'`. */
static bool lightprobe_poll_parallax(const bContext *C, PanelType * /*pt*/)
{
  const LightProbe *probe = context_lightprobe(C);
  return probe != nullptr && probe->type == LIGHTPROBE_TYPE_SPHERE &&
         (engine_is(C, "BLENDER_RENDER") || engine_is(C, "BLENDER_EEVEE_NEXT"));
}

/**
 * `layout.prop(...)` con la semantica EXACTA de la del Python, que NO es la de
 * `uiLayout::prop()`.
 *
 * Cuando la propiedad no existe en el struct:
 * - el Python (`rna_uiItemR`) avisa por consola y **no dibuja nada**;
 * - el C++ (`uiLayout::prop`) dibuja una **etiqueta deshabilitada** con el
 *   identificador crudo (`ui_item_disabled`).
 *
 * Y aqui eso pasa de verdad, no es teoria. `LightProbe` se **refina** a
 * `LightProbeSphere`, `LightProbePlane` o `LightProbeVolume`, y los cuatro
 * subpaneles de horneado piden propiedades que solo existen en el de volumen
 * (`capture_distance`, `resolution_x`, `clamp_direct`, `surface_bias`...)
 * mientras su `poll` **no mira el tipo**. Con una sonda de esfera delante, el
 * Python dibuja las columnas y ni un boton; el primer intento en C++ metia trece
 * etiquetas deshabilitadas de mas, y el volcado de diseno las canto una a una.
 *
 * Es la misma familia de trampa que «hay `draw()` de Python que revientan a
 * medias» (`UI-A-CPP.md`, editor de nodos): el C++ tiene que cortar donde corta
 * el original, ni antes ni despues.
 */
static void prop_py(uiLayout *layout,
                    PointerRNA *ptr,
                    const char *propname,
                    const eUI_Item_Flag flag,
                    const std::optional<blender::StringRef> name,
                    const int icon)
{
  if (RNA_struct_find_property(ptr, propname) == nullptr) {
    return;
  }
  layout->prop(ptr, propname, flag, name, icon);
}

static void context_lightprobe_draw(const bContext *C, Panel *panel)
{
  Object *ob = CTX_data_active_object(C);
  uiLayout *layout = panel->layout;
  if (ob != nullptr) {
    PointerRNA ob_ptr = RNA_pointer_create_discrete(&ob->id, &RNA_Object, ob);
    uiTemplateID(layout, C, &ob_ptr, "data", nullptr, nullptr, nullptr);
  }
  else if (context_lightprobe(C) != nullptr) {
    SpaceProperties *space = CTX_wm_space_properties(C);
    PointerRNA space_ptr = RNA_pointer_create_discrete(
        reinterpret_cast<ID *>(CTX_wm_screen(C)), &RNA_SpaceProperties, space);
    uiTemplateID(layout, C, &space_ptr, "pin_id", nullptr, nullptr, nullptr);
  }
}

/**
 * `DATA_PT_lightprobe` — el panel «Probe» del motor `BLENDER_RENDER`.
 *
 * No lo dibuja nadie hoy (ver la cabecera del fichero). Se traduce linea a linea
 * de todos modos, incluido el `layout.prop(probe, "type")` que el original tiene
 * comentado: comentado se queda.
 *
 * La trampa del original, que hay que conservar: el ultimo `sub = col.column()`
 * esta FUERA del `if`, asi que cuelga de la columna que haya creado la rama que
 * se tomo. Sacarlo de una de las ramas cambiaria el arbol.
 */
static void lightprobe_draw(const bContext *C, Panel *panel)
{
  LightProbe *probe = context_lightprobe(C);
  if (probe == nullptr) {
    return;
  }
  PointerRNA ptr = lightprobe_ptr(probe);
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);

  uiLayout *col = nullptr;
  if (probe->type == LIGHTPROBE_TYPE_VOLUME) {
    col = &layout->column(false);
    prop_py(col, &ptr, "influence_distance", UI_ITEM_NONE, IFACE_("Distance"), ICON_NONE);
    prop_py(col, &ptr, "falloff", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    prop_py(col, &ptr, "intensity", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    uiLayout *sub = &col->column(true);
    prop_py(sub, &ptr, "resolution_x", UI_ITEM_NONE, IFACE_("Resolution X"), ICON_NONE);
    prop_py(sub, &ptr, "resolution_y", UI_ITEM_NONE, IFACE_("Y"), ICON_NONE);
    prop_py(sub, &ptr, "resolution_z", UI_ITEM_NONE, IFACE_("Z"), ICON_NONE);
  }
  else if (probe->type == LIGHTPROBE_TYPE_PLANE) {
    col = &layout->column(false);
    prop_py(col, &ptr, "influence_distance", UI_ITEM_NONE, IFACE_("Distance"), ICON_NONE);
    prop_py(col, &ptr, "falloff", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  else {
    col = &layout->column(false);
    prop_py(col, &ptr, "influence_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    if (probe->attenuation_type == LIGHTPROBE_SHAPE_ELIPSOID) {
      prop_py(col, &ptr, "influence_distance", UI_ITEM_NONE, IFACE_("Radius"), ICON_NONE);
    }
    else {
      prop_py(col, &ptr, "influence_distance", UI_ITEM_NONE, IFACE_("Size"), ICON_NONE);
    }

    prop_py(col, &ptr, "falloff", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    prop_py(col, &ptr, "intensity", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  uiLayout *sub = &col->column(true);
  if (probe->type == LIGHTPROBE_TYPE_PLANE) {
    prop_py(sub, &ptr, "clip_start", UI_ITEM_NONE, IFACE_("Clipping Offset"), ICON_NONE);
  }
  else {
    prop_py(sub, &ptr, "clip_start", UI_ITEM_NONE, IFACE_("Clipping Start"), ICON_NONE);
    /* `text_ctxt=i18n_contexts.id_camera`: el «End» del recorte se traduce con el
     * contexto de camara, no con el general. */
    prop_py(sub, &ptr,
              "clip_end",
              UI_ITEM_NONE,
              CTX_IFACE_(BLT_I18NCONTEXT_ID_CAMERA, "End"),
              ICON_NONE);
  }
}

static void lightprobe_eevee_next_draw(const bContext *C, Panel *panel)
{
  LightProbe *probe = context_lightprobe(C);
  if (probe == nullptr) {
    return;
  }
  PointerRNA ptr = lightprobe_ptr(probe);
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);

  if (probe->type == LIGHTPROBE_TYPE_VOLUME) {
    uiLayout *col = &layout->column(false);

    prop_py(col, &ptr, "intensity", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->separator();

    uiLayout *sub = &col->column(true);
    prop_py(sub, &ptr, "normal_bias", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    prop_py(sub, &ptr, "view_bias", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    prop_py(sub, &ptr, "facing_bias", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    col->separator();

    prop_py(col, &ptr, "validity_threshold", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    sub = &col->column(true);
    prop_py(sub, &ptr, "dilation_threshold", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    prop_py(sub, &ptr, "dilation_radius", UI_ITEM_NONE, IFACE_("Radius"), ICON_NONE);

    /* El original cierra con un `col.separator()` suelto. Se copia: un separador
     * es un item del arbol y el volcado lo cuenta. */
    col->separator();
  }
  else if (probe->type == LIGHTPROBE_TYPE_SPHERE) {
    uiLayout *col = &layout->column(false);
    prop_py(col, &ptr, "influence_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    const char *influence_text = (probe->attenuation_type == LIGHTPROBE_SHAPE_ELIPSOID) ?
                                     IFACE_("Radius") :
                                     IFACE_("Size");
    prop_py(col, &ptr, "influence_distance", UI_ITEM_NONE, influence_text, ICON_NONE);
    prop_py(col, &ptr, "falloff", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  else if (probe->type == LIGHTPROBE_TYPE_PLANE) {
    uiLayout *col = &layout->column(false);
    prop_py(col, &ptr, "influence_distance", UI_ITEM_NONE, IFACE_("Distance"), ICON_NONE);
  }
  /* El `else` del original es «Currently unsupported» y no dibuja nada. */
}

/** `DATA_PT_lightprobe_visibility` — tampoco lo dibuja nadie (`BLENDER_RENDER`). */
static void lightprobe_visibility_draw(const bContext *C, Panel *panel)
{
  LightProbe *probe = context_lightprobe(C);
  if (probe == nullptr) {
    return;
  }
  PointerRNA ptr = lightprobe_ptr(probe);
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);

  uiLayout *col = &layout->column(false);

  if (probe->type == LIGHTPROBE_TYPE_VOLUME) {
    prop_py(col, &ptr, "visibility_buffer_bias", UI_ITEM_NONE, IFACE_("Bias"), ICON_NONE);
    prop_py(col, &ptr, "visibility_bleed_bias", UI_ITEM_NONE, IFACE_("Bleed Bias"), ICON_NONE);
    prop_py(col, &ptr, "visibility_blur", UI_ITEM_NONE, IFACE_("Blur"), ICON_NONE);
  }

  uiLayout *row = &col->row(true);
  prop_py(row, &ptr, "visibility_collection", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  prop_py(row, &ptr, "invert_visibility_collection", UI_ITEM_NONE, "", ICON_ARROW_LEFTRIGHT);
}

static void lightprobe_capture_draw(const bContext *C, Panel *panel)
{
  LightProbe *probe = context_lightprobe(C);
  if (probe == nullptr) {
    return;
  }
  PointerRNA ptr = lightprobe_ptr(probe);
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);

  uiLayout *col = &layout->column(false);

  if (probe->type == LIGHTPROBE_TYPE_SPHERE) {
    uiLayout *sub = &col->column(true);
    prop_py(sub, &ptr, "clip_start", UI_ITEM_NONE, IFACE_("Clipping Start"), ICON_NONE);
    prop_py(sub, &ptr,
              "clip_end",
              UI_ITEM_NONE,
              CTX_IFACE_(BLT_I18NCONTEXT_ID_CAMERA, "End"),
              ICON_NONE);
  }
  else if (probe->type == LIGHTPROBE_TYPE_PLANE) {
    prop_py(col, &ptr, "clip_start", UI_ITEM_NONE, IFACE_("Clipping Offset"), ICON_NONE);
  }
}

static void lightprobe_bake_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  uiLayout *col = &layout->column(false);
  uiLayout *row = &col->row(true);

  /* `row.operator("object.lightprobe_cache_bake").subset = 'ACTIVE'`: el
   * operador se anade y **despues** se le escribe la propiedad en el puntero que
   * devuelve. El volcado serializa esos argumentos, asi que olvidarlos sale como
   * `op='bpy.ops.object.lightprobe_cache_bake()'` sin el `subset`.
   *
   * Se pone por **identificador**, no por numero: `LIGHTCACHE_SUBSET_ACTIVE` vale
   * 2, no 0 (`ALL`=0, `SELECTED`=1), y escribir el numero a ojo es la forma
   * clasica de dejar aqui un `subset='ALL'` que arrasa el horneado de toda la
   * escena. Los dos enums son estaticos, asi que la busqueda no necesita
   * contexto. */
  PointerRNA op_ptr = row->op("OBJECT_OT_lightprobe_cache_bake", std::nullopt, ICON_NONE);
  RNA_enum_set_identifier(nullptr, &op_ptr, "subset", "ACTIVE");

  op_ptr = row->op("OBJECT_OT_lightprobe_cache_free", "", ICON_TRASH);
  RNA_enum_set_identifier(nullptr, &op_ptr, "subset", "ACTIVE");
}

static void lightprobe_bake_resolution_draw(const bContext *C, Panel *panel)
{
  LightProbe *probe = context_lightprobe(C);
  if (probe == nullptr) {
    return;
  }
  PointerRNA ptr = lightprobe_ptr(probe);
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  uiLayout *col = &layout->column(false);

  uiLayout *sub = &col->column(true);
  prop_py(sub, &ptr, "resolution_x", UI_ITEM_NONE, IFACE_("Resolution X"), ICON_NONE);
  prop_py(sub, &ptr, "resolution_y", UI_ITEM_NONE, IFACE_("Y"), ICON_NONE);
  prop_py(sub, &ptr, "resolution_z", UI_ITEM_NONE, IFACE_("Z"), ICON_NONE);

  prop_py(col, &ptr, "bake_samples", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  prop_py(col, &ptr, "surfel_density", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void lightprobe_bake_capture_draw(const bContext *C, Panel *panel)
{
  LightProbe *probe = context_lightprobe(C);
  if (probe == nullptr) {
    return;
  }
  PointerRNA ptr = lightprobe_ptr(probe);
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  uiLayout *col = &layout->column(false);
  prop_py(col, &ptr, "capture_distance", UI_ITEM_NONE, IFACE_("Distance"), ICON_NONE);

  /* `layout.column(heading="Contributions", align=True)`: el encabezado es el
   * primer argumento de la sobrecarga con `heading`, y va **traducido**. */
  col = &layout->column(true, IFACE_("Contributions"));
  prop_py(col, &ptr, "capture_world", UI_ITEM_NONE, IFACE_("World"), ICON_NONE);
  prop_py(col, &ptr, "capture_indirect", UI_ITEM_NONE, IFACE_("Indirect Light"), ICON_NONE);
  prop_py(col, &ptr, "capture_emission", UI_ITEM_NONE, IFACE_("Emission"), ICON_NONE);
}

static void lightprobe_bake_offset_draw(const bContext *C, Panel *panel)
{
  LightProbe *probe = context_lightprobe(C);
  if (probe == nullptr) {
    return;
  }
  PointerRNA ptr = lightprobe_ptr(probe);
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  uiLayout *col = &layout->column(true);
  prop_py(col, &ptr, "surface_bias", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  prop_py(col, &ptr, "escape_bias", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void lightprobe_bake_clamping_draw(const bContext *C, Panel *panel)
{
  LightProbe *probe = context_lightprobe(C);
  if (probe == nullptr) {
    return;
  }
  PointerRNA ptr = lightprobe_ptr(probe);
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  uiLayout *col = &layout->column(true);
  prop_py(col, &ptr, "clamp_direct", UI_ITEM_NONE, IFACE_("Direct Light"), ICON_NONE);
  prop_py(col, &ptr, "clamp_indirect", UI_ITEM_NONE, IFACE_("Indirect Light"), ICON_NONE);
}

static void lightprobe_parallax_header(const bContext *C, Panel *panel)
{
  LightProbe *probe = context_lightprobe(C);
  if (probe == nullptr) {
    return;
  }
  PointerRNA ptr = lightprobe_ptr(probe);
  prop_py(panel->layout, &ptr, "use_custom_parallax", UI_ITEM_NONE, "", ICON_NONE);
}

static void lightprobe_parallax_draw(const bContext *C, Panel *panel)
{
  LightProbe *probe = context_lightprobe(C);
  if (probe == nullptr) {
    return;
  }
  PointerRNA ptr = lightprobe_ptr(probe);
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);

  uiLayout *col = &layout->column(false);
  /* `col.active`, no `enabled`: son dos aspectos distintos en el volcado. */
  uiLayoutSetActive(col, (probe->flag & LIGHTPROBE_FLAG_CUSTOM_PARALLAX) != 0);

  prop_py(col, &ptr, "parallax_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  if (probe->parallax_type == LIGHTPROBE_SHAPE_ELIPSOID) {
    prop_py(col, &ptr, "parallax_distance", UI_ITEM_NONE, IFACE_("Radius"), ICON_NONE);
  }
  else {
    prop_py(col, &ptr, "parallax_distance", UI_ITEM_NONE, IFACE_("Size"), ICON_NONE);
  }
}

/** `DATA_PT_lightprobe_display` — tampoco lo dibuja nadie (`BLENDER_RENDER`). */
static void lightprobe_display_draw(const bContext *C, Panel *panel)
{
  LightProbe *probe = context_lightprobe(C);
  if (probe == nullptr) {
    return;
  }
  Object *ob = CTX_data_active_object(C);
  PointerRNA ptr = lightprobe_ptr(probe);
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  uiLayout *col = &layout->column(false);

  if (probe->type == LIGHTPROBE_TYPE_PLANE) {
    if (ob != nullptr) {
      PointerRNA ob_ptr = RNA_pointer_create_discrete(&ob->id, &RNA_Object, ob);
      prop_py(col, &ob_ptr, "empty_display_size", UI_ITEM_NONE, IFACE_("Arrow Size"), ICON_NONE);
    }
    prop_py(col, &ptr, "show_influence", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  if (ELEM(probe->type, LIGHTPROBE_TYPE_VOLUME, LIGHTPROBE_TYPE_SPHERE)) {
    prop_py(col, &ptr, "show_influence", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    prop_py(col, &ptr, "show_clip", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  if (probe->type == LIGHTPROBE_TYPE_SPHERE) {
    uiLayout *sub = &col->column(false);
    uiLayoutSetActive(sub, (probe->flag & LIGHTPROBE_FLAG_CUSTOM_PARALLAX) != 0);
    prop_py(sub, &ptr, "show_parallax", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
}

static void lightprobe_display_eevee_next_draw(const bContext *C, Panel *panel)
{
  LightProbe *probe = context_lightprobe(C);
  if (probe == nullptr) {
    return;
  }
  Object *ob = CTX_data_active_object(C);
  PointerRNA ptr = lightprobe_ptr(probe);
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  uiLayout *col = &layout->column(false);

  if (ELEM(probe->type, LIGHTPROBE_TYPE_VOLUME, LIGHTPROBE_TYPE_SPHERE)) {
    uiLayout *row = &col->row(false, IFACE_("Data"));
    prop_py(row, &ptr, "use_data_display", UI_ITEM_NONE, "", ICON_NONE);
    uiLayout *subrow = &row->row(false);
    uiLayoutSetActive(subrow, (probe->flag & LIGHTPROBE_FLAG_SHOW_DATA) != 0);
    prop_py(subrow, &ptr, "data_display_size", UI_ITEM_R_SLIDER, IFACE_("Size"), ICON_NONE);
    prop_py(col, &ptr, "show_clip", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    prop_py(col, &ptr, "show_influence", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  if (probe->type == LIGHTPROBE_TYPE_SPHERE) {
    uiLayout *sub = &col->column(false);
    uiLayoutSetActive(sub, (probe->flag & LIGHTPROBE_FLAG_CUSTOM_PARALLAX) != 0);
    prop_py(sub, &ptr, "show_parallax", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  if (probe->type == LIGHTPROBE_TYPE_PLANE) {
    if (ob != nullptr) {
      PointerRNA ob_ptr = RNA_pointer_create_discrete(&ob->id, &RNA_Object, ob);
      prop_py(col, &ob_ptr, "empty_display_size", UI_ITEM_NONE, IFACE_("Arrow Size"), ICON_NONE);
    }
    prop_py(col, &ptr, "use_data_display", UI_ITEM_NONE, IFACE_("Capture"), ICON_NONE);
    prop_py(col, &ptr, "show_influence", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
}

static void lightprobe_animation_draw(const bContext *C, Panel *panel)
{
  LightProbe *probe = context_lightprobe(C);
  if (probe == nullptr) {
    return;
  }
  flipendo::properties_ui::draw_animation_panel(C, panel->layout, &probe->id);
}

void fl_properties_data_lightprobe_register(ARegionType *art)
{
  /* El orden de este array es el de la tupla `classes` del Python, que es el
   * orden de registro y por tanto el que desempata entre paneles con el mismo
   * `order` dentro de la lista de la region. Ojo: en esa tupla `_bake_clamping`
   * va **antes** que `_bake_offset`, al reves de como estan escritos en el
   * fichero. */
  static const flipendo::PanelDecl panels[] = {
      {
          /*idname*/ "DATA_PT_context_lightprobe",
          /*label*/ "",
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ context_lightprobe_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ lightprobe_poll_render_or_eevee,
          /*flag*/ PANEL_TYPE_NO_HEADER,
      },
      {
          /*idname*/ "DATA_PT_lightprobe",
          /*label*/ N_("Probe"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ lightprobe_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ lightprobe_poll_render,
      },
      {
          /*idname*/ "DATA_PT_lightprobe_eevee_next",
          /*label*/ N_("Probe"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ lightprobe_eevee_next_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ lightprobe_poll_eevee,
      },
      {
          /*idname*/ "DATA_PT_lightprobe_capture",
          /*label*/ N_("Capture"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ lightprobe_capture_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ lightprobe_poll_capture,
      },
      {
          /*idname*/ "DATA_PT_lightprobe_bake",
          /*label*/ N_("Bake"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ lightprobe_bake_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ lightprobe_poll_bake,
      },
      {
          /*idname*/ "DATA_PT_lightprobe_bake_resolution",
          /*label*/ N_("Resolution"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ "DATA_PT_lightprobe_bake",
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ lightprobe_bake_resolution_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ lightprobe_poll_eevee,
      },
      {
          /*idname*/ "DATA_PT_lightprobe_bake_capture",
          /*label*/ N_("Capture"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ "DATA_PT_lightprobe_bake",
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ lightprobe_bake_capture_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ lightprobe_poll_eevee,
      },
      {
          /*idname*/ "DATA_PT_lightprobe_bake_clamping",
          /*label*/ N_("Clamping"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ "DATA_PT_lightprobe_bake_capture",
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ lightprobe_bake_clamping_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ lightprobe_poll_eevee,
          /*flag*/ PANEL_TYPE_DEFAULT_CLOSED,
      },
      {
          /*idname*/ "DATA_PT_lightprobe_bake_offset",
          /*label*/ N_("Offset"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ "DATA_PT_lightprobe_bake_capture",
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ lightprobe_bake_offset_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ lightprobe_poll_eevee,
          /*flag*/ PANEL_TYPE_DEFAULT_CLOSED,
      },
      {
          /*idname*/ "DATA_PT_lightprobe_visibility",
          /*label*/ N_("Visibility"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ "DATA_PT_lightprobe",
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ lightprobe_visibility_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ lightprobe_poll_render,
      },
      {
          /*idname*/ "DATA_PT_lightprobe_parallax",
          /*label*/ N_("Custom Parallax"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ lightprobe_parallax_draw,
          /*draw_header*/ lightprobe_parallax_header,
          /*draw_header_preset*/ nullptr,
          /*poll*/ lightprobe_poll_parallax,
          /*flag*/ PANEL_TYPE_DEFAULT_CLOSED,
      },
      {
          /*idname*/ "DATA_PT_lightprobe_display",
          /*label*/ N_("Viewport Display"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ lightprobe_display_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ lightprobe_poll_render,
          /*flag*/ PANEL_TYPE_DEFAULT_CLOSED,
      },
      {
          /*idname*/ "DATA_PT_lightprobe_display_eevee_next",
          /*label*/ N_("Viewport Display"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ lightprobe_display_eevee_next_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ lightprobe_poll_eevee,
          /*flag*/ PANEL_TYPE_DEFAULT_CLOSED,
      },
      {
          /*idname*/ "DATA_PT_lightprobe_animation",
          /*label*/ N_("Animation"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ lightprobe_animation_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ lightprobe_poll_eevee,
          /*flag*/ PANEL_TYPE_DEFAULT_CLOSED,
          /* `PropertiesAnimationMixin.bl_order = PropertyPanel.bl_order - 1`. */
          /*order*/ 999,
      },
  };
  flipendo::panels_register(art, SPACE_PROPERTIES, {panels, ARRAY_SIZE(panels)});
}
