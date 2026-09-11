/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spbuttons
 *
 * La pestana de datos del Volumen, en C++ nativo. Sustituye
 * `scripts/startup/bl_ui/properties_data_volume.py` **entera**: sus ocho paneles
 * y su lista `VOLUME_UL_grids`. Por pestanas completas, nunca panel suelto
 * (`UI-A-CPP.md`): un panel aislado cambia de posicion dentro de la lista de su
 * region y el volcado marca diferencia aunque no cambie ningun campo.
 *
 * Como Altavoz, lleva `COMPAT_ENGINES` — los ocho paneles declaran el mismo
 * conjunto (`BLENDER_RENDER`, `BLENDER_EEVEE_NEXT`, `BLENDER_WORKBENCH`)—, asi
 * que basta un `poll`. Ojo a la diferencia con la sonda de luz: aqui el Python
 * mira `context.scene.render.engine`, no `context.engine`.
 */

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_volume_types.h"

#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_main.hh"
#include "BKE_screen.hh"
#include "BKE_volume.hh"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "FL_properties_ui.hpp"
#include "FL_ui_registry.hh"

#include "buttons_intern.hh"

static Volume *context_volume(const bContext *C)
{
  return static_cast<Volume *>(CTX_data_pointer_get_type(C, "volume", &RNA_Volume).data);
}

static PointerRNA volume_ptr(Volume *volume)
{
  return RNA_pointer_create_discrete(&volume->id, &RNA_Volume, volume);
}

/**
 * `volume.grids` es un `VolumeGrids`, y su `RNA_def_struct_sdna` apunta al
 * **propio `Volume`**, no a un sub-struct. Es el mismo patron que `SpaceUVEditor`
 * con `SpaceImage`; construirlo sobre `&volume->active_grid` o similar daria una
 * ruta RNA distinta y el volcado lo cantaria.
 */
static PointerRNA volume_grids_ptr(Volume *volume)
{
  return RNA_pointer_create_discrete(&volume->id, &RNA_VolumeGrids, volume);
}

static PointerRNA volume_display_ptr(Volume *volume)
{
  return RNA_pointer_create_discrete(&volume->id, &RNA_VolumeDisplay, &volume->display);
}

static PointerRNA volume_render_ptr(Volume *volume)
{
  return RNA_pointer_create_discrete(&volume->id, &RNA_VolumeRender, &volume->render);
}

/**
 * `poll` de `DataButtonsPanel`:
 * `context.scene.render.engine in cls.COMPAT_ENGINES and context.volume`.
 *
 * Los ocho paneles declaran el mismo `COMPAT_ENGINES`.
 */
static bool volume_poll(const bContext *C, PanelType * /*pt*/)
{
  if (context_volume(C) == nullptr) {
    return false;
  }
  const Scene *scene = CTX_data_scene(C);
  if (scene == nullptr) {
    return false;
  }
  const char *engine = scene->r.engine;
  return STREQ(engine, "BLENDER_RENDER") || STREQ(engine, "BLENDER_EEVEE_NEXT") ||
         STREQ(engine, "BLENDER_WORKBENCH");
}

/**
 * El `volume.grids.load()` que los dos paneles de fichero y de rejillas llaman
 * antes de dibujar. La funcion RNA `VolumeGrids.load` es exactamente
 * `BKE_volume_load(volume, bmain)`, asi que aqui no hay traduccion que inventar.
 */
static void volume_grids_load(const bContext *C, Volume *volume)
{
  BKE_volume_load(volume, CTX_data_main(C));
}

static void context_volume_draw(const bContext *C, Panel *panel)
{
  Object *ob = CTX_data_active_object(C);
  uiLayout *layout = panel->layout;
  if (ob != nullptr) {
    PointerRNA ob_ptr = RNA_pointer_create_discrete(&ob->id, &RNA_Object, ob);
    uiTemplateID(layout, C, &ob_ptr, "data", nullptr, nullptr, nullptr);
  }
  else if (context_volume(C) != nullptr) {
    SpaceProperties *space = CTX_wm_space_properties(C);
    PointerRNA space_ptr = RNA_pointer_create_discrete(
        reinterpret_cast<ID *>(CTX_wm_screen(C)), &RNA_SpaceProperties, space);
    uiTemplateID(layout, C, &space_ptr, "pin_id", nullptr, nullptr, nullptr);
  }
}

static void volume_file_draw(const bContext *C, Panel *panel)
{
  Volume *volume = context_volume(C);
  if (volume == nullptr) {
    return;
  }
  volume_grids_load(C, volume);

  PointerRNA ptr = volume_ptr(volume);
  uiLayout *layout = panel->layout;

  layout->prop(&ptr, "filepath", UI_ITEM_NONE, "", ICON_NONE);

  /* La separacion de propiedad se enciende DENTRO del `if`, no antes: con el
   * volumen vacio de fabrica (`filepath` en blanco) el layout raiz se queda con
   * `prop_sep=no`, y encenderla siempre cambiaria el bloque entero. */
  if (volume->filepath[0] != '\0') {
    uiLayoutSetPropSep(layout, true);
    uiLayoutSetPropDecorate(layout, false);

    uiLayout *col = &layout->column(true);
    col->prop(&ptr, "is_sequence", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    if (volume->is_sequence) {
      col->prop(&ptr, "frame_duration", UI_ITEM_NONE, IFACE_("Frames"), ICON_NONE);
      col->prop(&ptr, "frame_start", UI_ITEM_NONE, IFACE_("Start"), ICON_NONE);
      col->prop(&ptr, "frame_offset", UI_ITEM_NONE, IFACE_("Offset"), ICON_NONE);
      col->prop(&ptr, "sequence_mode", UI_ITEM_NONE, IFACE_("Mode"), ICON_NONE);
    }
  }

  const char *error_msg = BKE_volume_grids_error_msg(volume);
  if (error_msg != nullptr && error_msg[0] != '\0') {
    layout->separator();
    uiLayout *col = &layout->column(true);
    col->label(IFACE_("Failed to load volume:"), ICON_NONE);
    /* El Python pasa el mensaje por `layout.label(text=...)`, que traduce; para
     * un texto que viene de OpenVDB en ejecucion la busqueda en el catalogo lo
     * devuelve tal cual, pero se llama igual para no cambiar el contrato. */
    col->label(IFACE_(error_msg), ICON_NONE);
  }
}

/**
 * `VOLUME_UL_grids.draw_item`.
 *
 * Lo que se puede y lo que no se puede verificar de esto, dicho antes de que se
 * lo crea nadie: el volcado de diseno **no ejecuta el `draw_item` de una lista**
 * (deuda del volcador, `UI-A-CPP.md` §D4.2), y ademas un volumen sin fichero VDB
 * no tiene ni una rejilla que dibujar. Lo que si queda verificado por
 * `--fl-dump-ui` es que la lista **existe con sus callbacks**, que es donde
 * estaba el agujero de registro. Esta traduccion es lectura del original, y se
 * dice.
 */
static void volume_grids_draw_item(uiList * /*ui_list*/,
                                   const bContext * /*C*/,
                                   uiLayout *layout,
                                   PointerRNA * /*dataptr*/,
                                   PointerRNA *itemptr,
                                   int /*icon*/,
                                   PointerRNA * /*active_dataptr*/,
                                   const char * /*active_propname*/,
                                   int /*index*/,
                                   int /*flt_flag*/)
{
  char name[256];
  char *name_alloc = RNA_string_get_alloc(itemptr, "name", name, sizeof(name), nullptr);

  /* `grid.bl_rna.properties["data_type"].enum_items[grid.data_type]`, o sea el
   * elemento del enum que corresponde al valor actual, y de el su nombre de
   * interfaz — no el identificador. */
  const char *data_type_name = "";
  PropertyRNA *data_type = RNA_struct_find_property(itemptr, "data_type");
  if (data_type != nullptr) {
    RNA_property_enum_name_gettexted(
        nullptr, itemptr, data_type, RNA_property_enum_get(itemptr, data_type), &data_type_name);
  }

  uiLayoutSetEmboss(layout, blender::ui::EmbossType::None);
  layout->label(IFACE_(name_alloc), ICON_NONE);

  uiLayout *row = &layout->row(false);
  uiLayoutSetAlignment(row, UI_LAYOUT_ALIGN_RIGHT);
  uiLayoutSetActive(row, false);
  row->label(data_type_name, ICON_NONE);

  if (name_alloc != name) {
    MEM_freeN(name_alloc);
  }
}

static void volume_grids_draw(const bContext *C, Panel *panel)
{
  Volume *volume = context_volume(C);
  if (volume == nullptr) {
    return;
  }
  volume_grids_load(C, volume);

  PointerRNA ptr = volume_ptr(volume);
  PointerRNA grids = volume_grids_ptr(volume);

  uiTemplateList(panel->layout,
                 C,
                 "VOLUME_UL_grids",
                 "grids",
                 &ptr,
                 "grids",
                 &grids,
                 "active_index",
                 nullptr,
                 3,
                 5,
                 UILST_LAYOUT_DEFAULT,
                 9,
                 UI_TEMPLATE_LIST_FLAG_NONE);
}

static void volume_render_draw(const bContext *C, Panel *panel)
{
  Volume *volume = context_volume(C);
  if (volume == nullptr) {
    return;
  }
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  PointerRNA ptr = volume_ptr(volume);
  PointerRNA render = volume_render_ptr(volume);

  uiLayout *col = &layout->column(true);
  col->prop(&render, "space", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  const Scene *scene = CTX_data_scene(C);
  if (scene != nullptr && STREQ(scene->r.engine, "CYCLES")) {
    /* Rama de Cycles, copiada tal cual. Hoy no la pisa nadie en Flipendo —
     * `RE_engines` solo trae `BLENDER_EEVEE_NEXT` y `BLENDER_WORKBENCH`—, asi
     * que no esta cubierta por ningun volcado y queda dicho aqui en vez de
     * borrada: quitarla seria perder capacidad por la puerta de atras. */
    col->prop(&render, "step_size", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    col = &layout->column(true);
    col->prop(&render, "clipping", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    col = &layout->column(false);
    col->prop(&render, "precision", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    col = &layout->column(false);
    col->prop(&ptr, "velocity_grid", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(&ptr, "velocity_unit", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(&ptr, "velocity_scale", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
}

static void volume_viewport_display_draw(const bContext *C, Panel *panel)
{
  Volume *volume = context_volume(C);
  if (volume == nullptr) {
    return;
  }
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  PointerRNA display = volume_display_ptr(volume);

  uiLayout *col = &layout->column(true);
  col->prop(&display, "wireframe_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  /* `sub = col.row()` y el `active` va en la FILA, no en la columna: puesto en
   * la columna apagaria tambien el `wireframe_type` de arriba. */
  uiLayout *sub = &col->row(false);
  uiLayoutSetActive(sub,
                    ELEM(volume->display.wireframe_type,
                         VOLUME_WIREFRAME_BOXES,
                         VOLUME_WIREFRAME_POINTS));
  sub->prop(&display, "wireframe_detail", UI_ITEM_NONE, IFACE_("Detail"), ICON_NONE);

  col = &layout->column(false);
  col->prop(&display, "density", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(&display, "interpolation_method", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void volume_viewport_display_slicing_header(const bContext *C, Panel *panel)
{
  Volume *volume = context_volume(C);
  if (volume == nullptr) {
    return;
  }
  PointerRNA display = volume_display_ptr(volume);
  panel->layout->prop(&display, "use_slice", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void volume_viewport_display_slicing_draw(const bContext *C, Panel *panel)
{
  Volume *volume = context_volume(C);
  if (volume == nullptr) {
    return;
  }
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  PointerRNA display = volume_display_ptr(volume);

  /* `layout.active`, sobre la raiz del panel, no sobre la columna. */
  /* `use_slice` no es un booleano propio: su `RNA_def_property_boolean_sdna` lo
   * saca de `axis_slice_method` con la mascara `VOLUME_AXIS_SLICE_SINGLE`, o sea
   * es una prueba de bit, no una comparacion de enum. */
  uiLayoutSetActive(layout,
                    (volume->display.axis_slice_method & VOLUME_AXIS_SLICE_SINGLE) != 0);

  uiLayout *col = &layout->column(false);
  col->prop(&display, "slice_axis", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(&display, "slice_depth", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void volume_animation_draw(const bContext *C, Panel *panel)
{
  Volume *volume = context_volume(C);
  if (volume == nullptr) {
    return;
  }
  flipendo::properties_ui::draw_animation_panel(C, panel->layout, &volume->id);
}

static void volume_custom_props_draw(const bContext *C, Panel *panel)
{
  Volume *volume = context_volume(C);
  if (volume == nullptr) {
    return;
  }
  PointerRNA ptr = volume_ptr(volume);
  flipendo::properties_ui::draw_custom_properties(
      C, panel->layout, &ptr, &volume->id, "object.data");
}

void fl_properties_data_volume_register(ARegionType *art)
{
  /* El orden de este array es el de la tupla `classes` del Python, que es el
   * orden de registro y por tanto el que decide el desempate entre paneles con
   * el mismo `order` dentro de la region. */
  static const flipendo::PanelDecl panels[] = {
      {
          /*idname*/ "DATA_PT_context_volume",
          /*label*/ "",
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ context_volume_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ volume_poll,
          /*flag*/ PANEL_TYPE_NO_HEADER,
      },
      {
          /*idname*/ "DATA_PT_volume_grids",
          /*label*/ N_("Grids"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ volume_grids_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ volume_poll,
      },
      {
          /*idname*/ "DATA_PT_volume_file",
          /*label*/ N_("OpenVDB File"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ volume_file_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ volume_poll,
      },
      {
          /*idname*/ "DATA_PT_volume_viewport_display",
          /*label*/ N_("Viewport Display"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ volume_viewport_display_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ volume_poll,
      },
      {
          /*idname*/ "DATA_PT_volume_viewport_display_slicing",
          /*label*/ "",
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ "DATA_PT_volume_viewport_display",
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ volume_viewport_display_slicing_draw,
          /*draw_header*/ volume_viewport_display_slicing_header,
          /*draw_header_preset*/ nullptr,
          /*poll*/ volume_poll,
      },
      {
          /*idname*/ "DATA_PT_volume_render",
          /*label*/ N_("Render"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ volume_render_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ volume_poll,
      },
      {
          /*idname*/ "DATA_PT_volume_animation",
          /*label*/ N_("Animation"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ volume_animation_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ volume_poll,
          /*flag*/ PANEL_TYPE_DEFAULT_CLOSED,
          /* `PropertiesAnimationMixin.bl_order = PropertyPanel.bl_order - 1`. */
          /*order*/ 999,
      },
      {
          /*idname*/ "DATA_PT_custom_props_volume",
          /*label*/ N_("Custom Properties"),
          /*category*/ nullptr,
          /*context*/ "data",
          /*parent_id*/ nullptr,
          /*description*/ nullptr,
          /*translation_context*/ nullptr,
          /*draw*/ volume_custom_props_draw,
          /*draw_header*/ nullptr,
          /*draw_header_preset*/ nullptr,
          /*poll*/ volume_poll,
          /*flag*/ PANEL_TYPE_DEFAULT_CLOSED,
          /*order*/ 1000,
      },
  };
  flipendo::panels_register(art, SPACE_PROPERTIES, {panels, ARRAY_SIZE(panels)});

  static const flipendo::UIListDecl uilists[] = {
      {
          /*idname*/ "VOLUME_UL_grids",
          /*draw_item*/ volume_grids_draw_item,
      },
  };
  flipendo::uilists_register({uilists, ARRAY_SIZE(uilists)});
}
