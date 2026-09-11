/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spnode
 *
 * La interfaz del editor de nodos, en C++ nativo.
 *
 * Sustituye a `scripts/startup/bl_ui/space_node.py`: la cabecera, los ocho menus
 * que quedaban en Python y los diecisiete paneles propios del editor. Mismos
 * `idname`, mismas etiquetas, mismos operadores con los mismos argumentos: ni un
 * atajo, ni un `.blend`, ni un popover deben notar el cambio.
 *
 * Se verifica con los dos volcados de `politicas/UI-A-CPP.md`
 * (`--fl-dump-ui` y `--fl-dump-ui-layout`) contra las lineas base congeladas con
 * el Python vivo.
 *
 * LO QUE **NO** ESTA AQUI, Y POR QUE
 * ----------------------------------
 * `space_node.py` clonaba siete paneles de las pestanas de Propiedades con su
 * factoria `node_panel()`, mas `NODE_PT_annotation`, que hereda el mixin
 * `AnnotationDataPanel` compartido con la vista 3D, el editor de clips, el de
 * imagen y el de secuencias. La decision del proyecto (REGLAMENTO, 03:40) es
 * **funcion compartida, nunca copia**: un `draw()` que viven en dos sitios se
 * desincroniza.
 *
 * De los ocho, solo uno tiene ya su funcion compartida escrita:
 * `WORLD_PT_viewport_display`, que el carril de Propiedades expuso en
 * `FL_properties_ui.hpp` al migrar la pestana Mundo. Ese si se escribe aqui, y
 * llama a la funcion, no la copia. Los otros siete siguen en Python hasta que
 * sus originales (`properties_material.py`, `properties_data_light.py` y
 * `properties_grease_pencil_common.py`, de otro carril) se migren y expongan su
 * dibujo igual. No colisionan: sus `idname` son `NODE_*` y aqui no se declara
 * ninguno de ellos.
 *
 * LA OTRA COSA QUE SE PIERDE, Y TAMBIEN ES UNA DECISION
 * ----------------------------------------------------
 * Ninguna: el `nodeitems_utils` de `NODE_MT_add` ya se fue con `fl_node_menus.cc`
 * (ver su cabecera). Aqui no queda capacidad perdida.
 *
 * TRAMPAS QUE COSTARON TIEMPO
 * ---------------------------
 * - `SpaceNode::tree_idname` es una **cadena**, no una enumeracion. El
 *   `snode.tree_type` del Python compara contra `"ShaderNodeTree"`,
 *   `"CompositorNodeTree"`, `"TextureNodeTree"` y `"GeometryNodeTree"`.
 * - El conjunto `types_that_support_material` del Python contiene `'GPENCIL'`,
 *   que **ya no es un identificador valido** de `rna_enum_object_type_items` (en
 *   4.5 es `'GREASEPENCIL'`). O sea: hoy los objetos de lapiz de cera NO estan en
 *   ese conjunto. Se reproduce tal cual, comparando identificadores de RNA, para
 *   no cambiar el comportamiento al migrar; arreglarlo es otro cambio, y con su
 *   linea base regenerada.
 * - Tres `draw()` del Python lanzan `AttributeError` sobre la escena de fabrica
 *   (`snode.node_tree` es `None`) y Blender se lo traga: lo dibujado hasta ese
 *   punto se queda. La linea base recoge ese resultado a medias, asi que el C++
 *   tiene que cortar exactamente donde cortaba la excepcion. Va marcado en cada
 *   sitio.
 *
 * Doctrina: politicas/LENGUAJE-CPP.md.
 */

#include <fmt/format.h>

#include <optional>

#include "BLI_listbase.h"
#include "BLI_string_ref.hh"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_freestyle.h"
#include "BKE_node.hh"
#include "BKE_object.hh"
#include "BKE_screen.hh"

#include "DNA_ID.h"
#include "DNA_freestyle_types.h"
#include "DNA_layer_types.h"
#include "DNA_modifier_types.h"
#include "DNA_node_tree_interface_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_enums.h"
#include "DNA_space_types.h"
#include "DNA_world_types.h"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "ED_node.hh"
#include "ED_node_c.hh"
#include "ED_screen.hh"

#include "UI_interface.hh"
#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "preset/FL_preset_ui.hpp"

#include "FL_properties_ui.hpp"
#include "FL_toolbar_ui.hh"
#include "FL_ui_registry.hh"

#include "FL_node_ui.hh"

namespace blender::ed::space_node {

/* -------------------------------------------------------------------- */
/** \name Fuentes de datos
 *
 * Cada una reproduce el descriptor de `bpy.context` que usaba el Python, con el
 * mismo tipo RNA y el mismo `owner_id`: el volcado de diseno serializa
 * `Struct.prop[indice]`, asi que el tipo tiene que coincidir, no solo el dato.
 * \{ */

/** `context.space_data` ya refinado. */
static PointerRNA space_node_ptr(const bContext *C)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  if (snode == nullptr) {
    return PointerRNA_NULL;
  }
  return RNA_pointer_create_discrete(
      reinterpret_cast<ID *>(CTX_wm_screen(C)), &RNA_SpaceNodeEditor, snode);
}

/** `snode.overlay`. `SpaceNodeOverlay` usa el propio `SpaceNode` como dato
 * (`RNA_def_struct_sdna(srna, "SpaceNode")`), no el sub-struct. */
static PointerRNA node_overlay_ptr(const bContext *C)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  if (snode == nullptr) {
    return PointerRNA_NULL;
  }
  return RNA_pointer_create_discrete(
      reinterpret_cast<ID *>(CTX_wm_screen(C)), &RNA_SpaceNodeOverlay, snode);
}

/** `context.tool_settings`. */
static PointerRNA tool_settings_ptr(const bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  ToolSettings *ts = CTX_data_tool_settings(C);
  if (scene == nullptr || ts == nullptr) {
    return PointerRNA_NULL;
  }
  return RNA_pointer_create_id_subdata(scene->id, &RNA_ToolSettings, ts);
}

/** `context.active_node`. */
static PointerRNA active_node_ptr(const bContext *C)
{
  return CTX_data_pointer_get_type(C, "active_node", &RNA_Node);
}

/** El identificador RNA del tipo de un objeto (`ob.type` en Python). */
static const char *object_type_identifier(const Object *ob)
{
  const char *identifier = nullptr;
  RNA_enum_id_from_value(rna_enum_object_type_items, ob->type, &identifier);
  return identifier ? identifier : "";
}

/**
 * El `types_that_support_material` de `NODE_HT_header`, letra por letra.
 *
 * OJO: `'GPENCIL'` ya no existe como identificador (en 4.5 el objeto de lapiz de
 * cera es `'GREASEPENCIL'`), asi que hoy esa entrada no casa con nada. Se copia
 * igual: cambiarla seria cambiar el comportamiento a escondidas de la migracion.
 */
static bool object_type_supports_material(const Object *ob)
{
  static const char *types_that_support_material[] = {
      "MESH",
      "CURVE",
      "SURFACE",
      "FONT",
      "META",
      "GPENCIL",
      "VOLUME",
      "CURVES",
      "POINTCLOUD",
  };
  const char *type = object_type_identifier(ob);
  for (const char *supported : types_that_support_material) {
    if (STREQ(type, supported)) {
      return true;
    }
  }
  return false;
}

/** `snode.tree_type`, que es una CADENA (`SpaceNode::tree_idname`). */
static StringRef tree_type_of(const SpaceNode *snode)
{
  return snode->tree_idname;
}

/** `Menu.draw_collapsible()` (`bpy_types.py`), igual que en `logic_ui.cc`. */
static void editor_menus_draw_collapsible(const bContext *C, uiLayout *layout)
{
  const ScrArea *area = CTX_wm_area(C);
  if (area && (area->flag & HEADER_NO_PULLDOWN) == 0) {
    uiLayout *row = &layout->row(true);
    if (MenuType *mt = WM_menutype_find("NODE_MT_editor_menus", false)) {
      UI_menutype_draw(const_cast<bContext *>(C), mt, row);
    }
  }
  else {
    layout->menu("NODE_MT_editor_menus", "", ICON_COLLAPSEMENU);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name NODE_HT_header
 *
 * 190 lineas de Python con una rama por tipo de arbol. El orden de los items es
 * el resultado: cualquier cambio sale en el volcado de diseno.
 * \{ */

static void header_shader_tree_draw(const bContext *C,
                                    uiLayout *layout,
                                    SpaceNode *snode,
                                    PointerRNA *snode_ptr,
                                    PointerRNA *snode_id_ptr,
                                    PointerRNA *id_from_ptr)
{
  Scene *scene = CTX_data_scene(C);

  layout->prop(snode_ptr, "shader_type", UI_ITEM_NONE, "", ICON_NONE);

  Object *ob = CTX_data_active_object(C);
  if (snode->shaderfrom == SNODE_SHADER_OBJECT && ob != nullptr) {
    const StringRef ob_type = object_type_identifier(ob);

    editor_menus_draw_collapsible(C, layout);

    if (snode->id != nullptr) {
      uiLayout *row = &layout->row(false);
      row->prop(snode_id_ptr, "use_nodes", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    }

    uiItemSpacer(layout);

    /* Los botones de ranura se apagan al fijar el arbol (#36589) y con los tipos
     * de objeto que no llevan material. */
    const bool has_material_slots = !RNA_boolean_get(snode_ptr, "pin") &&
                                    object_type_supports_material(ob);

    if (ob_type != "LIGHT") {
      uiLayout *row = &layout->row(false);
      uiLayoutSetEnabled(row, has_material_slots);
      uiLayoutSetUnitsX(row, 4.0f);
      uiItemPopoverPanel(row, C, "NODE_PT_material_slots", std::nullopt, ICON_NONE);
    }

    uiLayout *row = &layout->row(false);
    uiLayoutSetEnabled(row, has_material_slots);

    /* `material.new` cuando no hay ID/ranura activa. */
    if (snode->from == nullptr && object_type_supports_material(ob)) {
      PointerRNA ob_ptr = RNA_id_pointer_create(&ob->id);
      uiTemplateID(row, C, &ob_ptr, "active_material", "material.new", nullptr, nullptr);
    }
    /* El ID de material, pero no para las luces. */
    if (snode->from != nullptr && ob_type != "LIGHT") {
      uiTemplateID(row, C, id_from_ptr, "active_material", "material.new", nullptr, nullptr);
    }
  }

  if (snode->shaderfrom == SNODE_SHADER_WORLD) {
    editor_menus_draw_collapsible(C, layout);
    World *world = scene ? scene->world : nullptr;

    if (snode->id != nullptr) {
      uiLayout *row = &layout->row(false);
      row->prop(snode_id_ptr, "use_nodes", UI_ITEM_NONE, std::nullopt, ICON_NONE);

      if (world != nullptr) {
        PointerRNA world_ptr = RNA_id_pointer_create(&world->id);
        if (RNA_boolean_get(&world_ptr, "use_eevee_finite_volume")) {
          row->op("WORLD_OT_convert_volume_to_mesh",
                  IFACE_("Convert Volume"),
                  ICON_WORLD,
                  uiLayoutGetOperatorContext(row),
                  UI_ITEM_R_NO_BG);
        }
      }
    }

    uiItemSpacer(layout);

    uiLayout *row = &layout->row(false);
    uiLayoutSetEnabled(row, !RNA_boolean_get(snode_ptr, "pin"));
    if (scene != nullptr) {
      PointerRNA scene_ptr = RNA_id_pointer_create(&scene->id);
      uiTemplateID(row, C, &scene_ptr, "world", "world.new", nullptr, nullptr);
    }
  }

  if (snode->shaderfrom == SNODE_SHADER_LINESTYLE) {
    ViewLayer *view_layer = CTX_data_view_layer(C);
    FreestyleLineSet *lineset = view_layer ?
                                    BKE_freestyle_lineset_get_active(&view_layer->freestyle_config) :
                                    nullptr;

    if (lineset != nullptr) {
      editor_menus_draw_collapsible(C, layout);

      if (snode->id != nullptr) {
        uiLayout *row = &layout->row(false);
        row->prop(snode_id_ptr, "use_nodes", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      }

      uiItemSpacer(layout);

      uiLayout *row = &layout->row(false);
      uiLayoutSetEnabled(row, !RNA_boolean_get(snode_ptr, "pin"));
      PointerRNA lineset_ptr = RNA_pointer_create_discrete(
          scene ? &scene->id : nullptr, &RNA_FreestyleLineSet, lineset);
      uiTemplateID(
          row, C, &lineset_ptr, "linestyle", "scene.freestyle_linestyle_new", nullptr, nullptr);
    }
  }
}

static void header_geometry_tree_draw(const bContext *C,
                                      uiLayout *layout,
                                      SpaceNode *snode,
                                      PointerRNA *snode_ptr,
                                      bool *r_display_pin)
{
  layout->prop(snode_ptr, "geometry_nodes_type", UI_ITEM_NONE, "", ICON_NONE);
  editor_menus_draw_collapsible(C, layout);
  uiItemSpacer(layout);

  if (snode->geometry_nodes_type == SNODE_GEOMETRY_MODIFIER) {
    Object *ob = CTX_data_active_object(C);

    uiLayout *row = &layout->row(false);
    if (RNA_boolean_get(snode_ptr, "pin")) {
      uiLayoutSetEnabled(row, false);
      uiTemplateID(
          row, C, snode_ptr, "node_tree", "node.new_geometry_node_group_assign", nullptr, nullptr);
    }
    else if (ob != nullptr) {
      ModifierData *md = BKE_object_active_modifier(ob);
      if (md != nullptr && md->type == eModifierType_Nodes) {
        PointerRNA md_ptr = RNA_pointer_create_discrete(&ob->id, &RNA_NodesModifier, md);
        if (reinterpret_cast<NodesModifierData *>(md)->node_group != nullptr) {
          uiTemplateID(row,
                       C,
                       &md_ptr,
                       "node_group",
                       "object.geometry_node_tree_copy_assign",
                       nullptr,
                       nullptr);
        }
        else {
          uiTemplateID(row,
                       C,
                       &md_ptr,
                       "node_group",
                       "node.new_geometry_node_group_assign",
                       nullptr,
                       nullptr);
        }
      }
      else {
        uiTemplateID(
            row, C, snode_ptr, "node_tree", "node.new_geometry_nodes_modifier", nullptr, nullptr);
      }
    }
  }
  else {
    uiTemplateID(layout,
                 C,
                 snode_ptr,
                 "geometry_nodes_tool_tree",
                 "node.new_geometry_node_group_tool",
                 nullptr,
                 nullptr);
    if (snode->nodetree != nullptr) {
      uiItemPopoverPanel(
          layout, C, "NODE_PT_geometry_node_tool_object_types", IFACE_("Types"), ICON_NONE);
      uiItemPopoverPanel(layout, C, "NODE_PT_geometry_node_tool_mode", IFACE_("Modes"), ICON_NONE);
      uiItemPopoverPanel(
          layout, C, "NODE_PT_geometry_node_tool_options", IFACE_("Options"), ICON_NONE);
    }
    *r_display_pin = false;
  }
}

static void node_header_draw(const bContext *C, Header *header)
{
  uiLayout *layout = header->layout;

  SpaceNode *snode = CTX_wm_space_node(C);
  if (snode == nullptr) {
    return;
  }

  PointerRNA snode_ptr = space_node_ptr(C);
  PointerRNA overlay_ptr = node_overlay_ptr(C);
  PointerRNA ts_ptr = tool_settings_ptr(C);
  PointerRNA snode_id_ptr = snode->id ? RNA_id_pointer_create(snode->id) : PointerRNA_NULL;
  PointerRNA id_from_ptr = snode->from ? RNA_id_pointer_create(snode->from) : PointerRNA_NULL;

  const StringRef tree_type = tree_type_of(snode);
  const bool is_compositor = tree_type == "CompositorNodeTree";

  uiTemplateHeader(layout, const_cast<bContext *>(C));

  /* El desplegable de tipo de arbol ya lo da `ui_type`; el Python lo tiene
   * comentado y aqui tampoco se dibuja. */

  bool display_pin = true;
  if (tree_type == "ShaderNodeTree") {
    header_shader_tree_draw(C, layout, snode, &snode_ptr, &snode_id_ptr, &id_from_ptr);
  }
  else if (tree_type == "TextureNodeTree") {
    layout->prop(&snode_ptr, "texture_type", UI_ITEM_NONE, "", ICON_NONE);

    editor_menus_draw_collapsible(C, layout);

    if (snode->id != nullptr) {
      layout->prop(&snode_id_ptr, "use_nodes", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    }

    uiItemSpacer(layout);

    if (snode->from != nullptr) {
      if (snode->texfrom == SNODE_TEX_BRUSH) {
        uiTemplateID(layout, C, &id_from_ptr, "texture", "texture.new", nullptr, nullptr);
      }
      else {
        uiTemplateID(layout, C, &id_from_ptr, "active_texture", "texture.new", nullptr, nullptr);
      }
    }
  }
  else if (tree_type == "CompositorNodeTree") {
    editor_menus_draw_collapsible(C, layout);

    if (snode->id != nullptr) {
      layout->prop(&snode_id_ptr, "use_nodes", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    }
  }
  else if (tree_type == "GeometryNodeTree") {
    header_geometry_tree_draw(C, layout, snode, &snode_ptr, &display_pin);
  }
  else {
    /* Un arbol de nodos propio se edita como un ID independiente. */
    editor_menus_draw_collapsible(C, layout);

    uiItemSpacer(layout);

    uiTemplateID(layout, C, &snode_ptr, "node_tree", "node.new_node_tree", nullptr, nullptr);
  }

  /* El alfiler, junto al bloque de ID. */
  if (!is_compositor && display_pin) {
    layout->prop(&snode_ptr, "pin", UI_ITEM_R_NO_BG, "", ICON_NONE);
  }

  uiItemSpacer(layout);

  /* En composicion, el alfiler va a la derecha. */
  if (is_compositor) {
    layout->prop(&snode_ptr, "pin", UI_ITEM_R_NO_BG, "", ICON_NONE);
  }

  /* `len(snode.path) > 1`. */
  if (BLI_listbase_count_at_most(&snode->treepath, 2) > 1) {
    layout->op("NODE_OT_tree_path_parent", "", ICON_FILE_PARENT);
  }

  const bool has_tree = snode->nodetree != nullptr;

  if (is_compositor) {
    /* Fondo. */
    uiLayout *row = &layout->row(true);
    row->prop(&snode_ptr, "show_backdrop", UI_ITEM_R_TOGGLE, std::nullopt, ICON_NONE);
    uiLayoutSetActive(row, has_tree);
    uiLayout *sub = &row->row(true);
    uiLayoutSetActive(sub, RNA_boolean_get(&snode_ptr, "show_backdrop"));
    sub->prop(&snode_ptr, "backdrop_channels", UI_ITEM_R_ICON_ONLY, "", ICON_NONE);

    /* Interruptor y desplegable de gizmos. */
    row = &layout->row(true);
    row->prop(&snode_ptr, "show_gizmo", UI_ITEM_NONE, "", ICON_GIZMO);
    uiLayoutSetActive(row, has_tree);
    sub = &row->row(true);
    uiLayoutSetActive(sub, RNA_boolean_get(&snode_ptr, "show_gizmo") && uiLayoutGetActive(row));
    uiItemPopoverPanel(sub, C, "NODE_PT_gizmo_display", "", ICON_NONE);
  }

  /* Imantado. */
  uiLayout *row = &layout->row(true);
  row->prop(&ts_ptr, "use_snap_node", UI_ITEM_NONE, "", ICON_NONE);
  uiLayoutSetActive(row, has_tree);

  /* Interruptor y desplegable de superposiciones. */
  row = &layout->row(true);
  row->prop(&overlay_ptr, "show_overlays", UI_ITEM_NONE, "", ICON_OVERLAY);
  uiLayout *sub = &row->row(true);
  uiLayoutSetActive(row, has_tree);
  uiLayoutSetActive(sub, RNA_boolean_get(&overlay_ptr, "show_overlays") && uiLayoutGetActive(row));
  uiItemPopoverPanel(sub, C, "NODE_PT_overlay", "", ICON_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Menus
 * \{ */

static void editor_menus_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;
  layout->menu("NODE_MT_view", std::nullopt, ICON_NONE);
  layout->menu("NODE_MT_select", std::nullopt, ICON_NONE);
  layout->menu("NODE_MT_add", std::nullopt, ICON_NONE);
  layout->menu("NODE_MT_node", std::nullopt, ICON_NONE);
}

static void view_menu_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  PointerRNA snode_ptr = space_node_ptr(C);

  layout->prop(&snode_ptr, "show_region_toolbar", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(&snode_ptr, "show_region_ui", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  layout->separator();

  uiLayout *sub = &layout->column(false);
  uiLayoutSetOperatorContext(sub, WM_OP_EXEC_REGION_WIN);
  sub->op("VIEW2D_OT_zoom_in", std::nullopt, ICON_NONE);
  sub->op("VIEW2D_OT_zoom_out", std::nullopt, ICON_NONE);

  layout->separator();

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);
  layout->op("NODE_OT_view_selected", std::nullopt, ICON_NONE);
  layout->op("NODE_OT_view_all", std::nullopt, ICON_NONE);

  if (RNA_boolean_get(&snode_ptr, "show_backdrop")) {
    layout->separator();

    layout->op("NODE_OT_backimage_move", IFACE_("Backdrop Move"), ICON_NONE);
    PointerRNA props = layout->op("NODE_OT_backimage_zoom", IFACE_("Backdrop Zoom In"), ICON_NONE);
    if (props.data) {
      RNA_float_set(&props, "factor", 1.2f);
    }
    props = layout->op("NODE_OT_backimage_zoom", IFACE_("Backdrop Zoom Out"), ICON_NONE);
    if (props.data) {
      RNA_float_set(&props, "factor", 1.0f / 1.2f);
    }
    layout->op(
        "NODE_OT_backimage_fit", IFACE_("Fit Backdrop to Available Space"), ICON_NONE);
  }

  layout->separator();

  layout->menu("INFO_MT_area", std::nullopt, ICON_NONE);
}

static void select_menu_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  PointerRNA props = layout->op("NODE_OT_select_all", IFACE_("All"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "action", "SELECT");
  }
  props = layout->op("NODE_OT_select_all", IFACE_("None"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "action", "DESELECT");
  }
  props = layout->op("NODE_OT_select_all", IFACE_("Invert"), ICON_NONE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "action", "INVERT");
  }

  layout->separator();

  props = layout->op("NODE_OT_select_box", std::nullopt, ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "tweak", false);
  }
  layout->op("NODE_OT_select_circle", std::nullopt, ICON_NONE);
  /* `operator_menu_enum` sin texto: el nombre lo pone el propio operador, y eso
   * solo lo hace la version que acepta `std::nullopt`. */
  if (wmOperatorType *ot = WM_operatortype_find("NODE_OT_select_lasso", false)) {
    uiItemMenuEnumFullO_ptr(layout, C, ot, "mode", std::nullopt, ICON_NONE, nullptr);
  }

  layout->separator();
  layout->op("NODE_OT_select_linked_from", IFACE_("Linked from"), ICON_NONE);
  layout->op("NODE_OT_select_linked_to", IFACE_("Linked to"), ICON_NONE);

  layout->separator();

  uiItemMenuEnumO(
      layout, C, "NODE_OT_select_grouped", "type", IFACE_("Select Grouped"), ICON_NONE);
  props = layout->op(
      "NODE_OT_select_same_type_step", IFACE_("Activate Same Type Previous"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "prev", true);
  }
  props = layout->op(
      "NODE_OT_select_same_type_step", IFACE_("Activate Same Type Next"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "prev", false);
  }

  layout->separator();

  layout->op("NODE_OT_find_node", IFACE_("Find Node..."), ICON_NONE);
}

/** `props.NODE_OT_translate_attach.TRANSFORM_OT_translate.view2d_edge_pan = True`. */
static void duplicate_move_edge_pan_set(PointerRNA *props)
{
  if (props->data == nullptr) {
    return;
  }
  PointerRNA attach = RNA_pointer_get(props, "NODE_OT_translate_attach");
  if (attach.data == nullptr) {
    return;
  }
  PointerRNA translate = RNA_pointer_get(&attach, "TRANSFORM_OT_translate");
  if (translate.data == nullptr) {
    return;
  }
  RNA_boolean_set(&translate, "view2d_edge_pan", true);
}

static void node_menu_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  const SpaceNode *snode = CTX_wm_space_node(C);
  if (snode == nullptr) {
    return;
  }
  bNodeTree *group = snode->edittree;
  const bool is_compositor = tree_type_of(snode) == "CompositorNodeTree";

  PointerRNA props = layout->op("TRANSFORM_OT_translate", std::nullopt, ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "view2d_edge_pan", true);
  }
  layout->op("TRANSFORM_OT_rotate", std::nullopt, ICON_NONE);
  layout->op("TRANSFORM_OT_resize", std::nullopt, ICON_NONE);

  layout->separator();
  layout->op("NODE_OT_clipboard_copy", IFACE_("Copy"), ICON_COPYDOWN);
  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_DEFAULT);
  layout->op("NODE_OT_clipboard_paste", IFACE_("Paste"), ICON_PASTEDOWN);
  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);
  props = layout->op("NODE_OT_duplicate_move", std::nullopt, ICON_DUPLICATE);
  duplicate_move_edge_pan_set(&props);
  props = layout->op("NODE_OT_duplicate_move_linked", std::nullopt, ICON_NONE);
  duplicate_move_edge_pan_set(&props);

  layout->separator();
  layout->op("NODE_OT_delete", std::nullopt, ICON_X);
  layout->op("NODE_OT_delete_reconnect", std::nullopt, ICON_NONE);

  layout->separator();
  layout->op("NODE_OT_join", IFACE_("Join in New Frame"), ICON_NONE);
  layout->op("NODE_OT_detach", IFACE_("Remove from Frame"), ICON_NONE);

  layout->separator();
  props = layout->op("WM_OT_call_panel", IFACE_("Rename..."), ICON_NONE);
  if (props.data) {
    RNA_string_set(&props, "name", "TOPBAR_PT_name");
    RNA_boolean_set(&props, "keep_open", false);
  }

  layout->separator();
  props = layout->op("NODE_OT_link_make", std::nullopt, ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "replace", false);
  }
  props = layout->op("NODE_OT_link_make", IFACE_("Make and Replace Links"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "replace", true);
  }
  layout->op("NODE_OT_links_cut", std::nullopt, ICON_NONE);
  layout->op("NODE_OT_links_detach", std::nullopt, ICON_NONE);
  layout->op("NODE_OT_links_mute", std::nullopt, ICON_NONE);

  /* `not group or group.bl_use_group_interface`. */
  if (group == nullptr ||
      (group->typeinfo != nullptr && group->typeinfo->no_group_interface == 0))
  {
    layout->separator();
    layout->op("NODE_OT_group_make", std::nullopt, ICON_NODETREE);
    layout->op("NODE_OT_group_insert", IFACE_("Insert Into Group"), ICON_NONE);
    props = layout->op("NODE_OT_group_edit", std::nullopt, ICON_NONE);
    if (props.data) {
      RNA_boolean_set(&props, "exit", false);
    }
    layout->op("NODE_OT_group_ungroup", std::nullopt, ICON_NONE);
  }

  layout->separator();
  layout->menu("NODE_MT_context_menu_show_hide_menu", std::nullopt, ICON_NONE);

  if (is_compositor) {
    layout->separator();
    layout->op("NODE_OT_read_viewlayers", std::nullopt, ICON_RENDERLAYERS);
  }
}

static void node_color_context_menu_draw(const bContext * /*C*/, Menu *menu)
{
  menu->layout->op("NODE_OT_node_copy_color", std::nullopt, ICON_COPY_ID);
}

static void context_menu_show_hide_draw(const bContext *C, Menu *menu)
{
  const SpaceNode *snode = CTX_wm_space_node(C);
  if (snode == nullptr) {
    return;
  }
  const bool is_compositor = tree_type_of(snode) == "CompositorNodeTree";

  uiLayout *layout = menu->layout;

  layout->op("NODE_OT_mute_toggle", IFACE_("Mute"), ICON_NONE);

  /* La vista previa de nodo solo existe en el compositor. */
  if (is_compositor) {
    layout->op("NODE_OT_preview_toggle", IFACE_("Node Preview"), ICON_NONE);
  }

  layout->op("NODE_OT_options_toggle", IFACE_("Node Options"), ICON_NONE);

  layout->separator();

  layout->op("NODE_OT_hide_socket_toggle", IFACE_("Unconnected Sockets"), ICON_NONE);
  layout->op("NODE_OT_hide_toggle", IFACE_("Collapse"), ICON_NONE);
  layout->op("NODE_OT_collapse_hide_unused_toggle", std::nullopt, ICON_NONE);
}

static void context_menu_select_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;

  PointerRNA props = layout->op(
      "NODE_OT_select_grouped", IFACE_("Select Grouped..."), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "extend", false);
  }

  layout->separator();

  layout->op("NODE_OT_select_linked_from", std::nullopt, ICON_NONE);
  layout->op("NODE_OT_select_linked_to", std::nullopt, ICON_NONE);

  layout->separator();

  props = layout->op(
      "NODE_OT_select_same_type_step", IFACE_("Activate Same Type Previous"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "prev", true);
  }
  props = layout->op(
      "NODE_OT_select_same_type_step", IFACE_("Activate Same Type Next"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "prev", false);
  }
}

static void node_tree_interface_context_menu_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  const SpaceNode *snode = CTX_wm_space_node(C);
  if (snode == nullptr) {
    return;
  }
  bNodeTree *tree = snode->edittree;
  /* TRAMPA: el Python hacia `tree.interface.active` ANTES de dibujar nada, asi que
   * sin arbol en edicion lanzaba `AttributeError` y el menu salia VACIO. La linea
   * base lo recoge asi; cortar aqui es reproducirlo, no una guarda de mas. */
  if (tree == nullptr) {
    return;
  }
  const bNodeTreeInterfaceItem *active_item = tree->tree_interface.active_item();

  layout->op("NODE_OT_interface_item_duplicate", std::nullopt, ICON_DUPLICATE);
  layout->separator();

  /* Y aqui pasaba lo mismo con `active_item.item_type` si no habia activo. */
  if (active_item == nullptr) {
    return;
  }
  if (active_item->item_type == NODE_INTERFACE_SOCKET) {
    layout->op("NODE_OT_interface_item_make_panel_toggle", std::nullopt, ICON_NONE);
  }
  else if (active_item->item_type == NODE_INTERFACE_PANEL) {
    layout->op("NODE_OT_interface_item_unlink_panel_toggle", std::nullopt, ICON_NONE);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Paneles emergentes de la cabecera
 * \{ */

static void gizmo_display_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  const SpaceNode *snode = CTX_wm_space_node(C);
  if (snode == nullptr) {
    return;
  }
  if (tree_type_of(snode) != "CompositorNodeTree") {
    return;
  }

  PointerRNA snode_ptr = space_node_ptr(C);

  uiLayout *col = &layout->column(false);
  col->label(IFACE_("Viewport Gizmos"), ICON_NONE);
  col->separator();

  uiLayoutSetActive(col, RNA_boolean_get(&snode_ptr, "show_gizmo"));
  uiLayout *colsub = &col->column(false);
  uiLayoutSetActive(colsub, snode->nodetree != nullptr && uiLayoutGetActive(col));
  colsub->prop(&snode_ptr, "show_gizmo_active_node", UI_ITEM_NONE, IFACE_("Active Node"), ICON_NONE);
}

/**
 * `self.bl_label = iface_("Slot {:d}").format(...)`: el `bl_label` dinamico, que en
 * C++ es `UI_panel_drawname_set()`. No dibuja nada, y por eso su bloque `DRAW_HEADER`
 * de la linea base es un `LAYOUT_ROOT` pelado.
 */
static void material_slots_draw_header(const bContext *C, Panel *panel)
{
  const Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return;
  }
  if (ob->totcol > 0) {
    UI_panel_drawname_set(panel,
                          fmt::format(fmt::runtime(IFACE_("Slot {:d}")), ob->actcol));
  }
  else {
    UI_panel_drawname_set(panel, IFACE_("Slot"));
  }
}

/* Duplica parte de `EEVEE_MATERIAL_PT_context_material`, igual que hacia el Python. */
static void material_slots_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return;
  }
  PointerRNA ob_ptr = RNA_id_pointer_create(&ob->id);

  uiLayout *row = &layout->row(false);
  uiLayout *col = &row->column(false);

  uiTemplateList(col,
                 C,
                 "MATERIAL_UL_matslots",
                 "",
                 &ob_ptr,
                 "material_slots",
                 &ob_ptr,
                 "active_material_index",
                 nullptr,
                 5,
                 5,
                 UILST_LAYOUT_DEFAULT,
                 9,
                 UI_TEMPLATE_LIST_FLAG_NONE);

  col = &row->column(true);
  col->op("OBJECT_OT_material_slot_add", "", ICON_ADD);
  col->op("OBJECT_OT_material_slot_remove", "", ICON_REMOVE);

  col->separator();

  col->menu("MATERIAL_MT_context_menu", "", ICON_DOWNARROW_HLT);

  if (ob->totcol > 1) {
    col->separator();

    PointerRNA props = col->op("OBJECT_OT_material_slot_move", "", ICON_TRIA_UP);
    if (props.data) {
      RNA_enum_set_identifier(nullptr, &props, "direction", "UP");
    }
    props = col->op("OBJECT_OT_material_slot_move", "", ICON_TRIA_DOWN);
    if (props.data) {
      RNA_enum_set_identifier(nullptr, &props, "direction", "DOWN");
    }
  }

  if (ob->mode == OB_MODE_EDIT) {
    uiLayout *edit_row = &layout->row(true);
    edit_row->op("OBJECT_OT_material_slot_assign", IFACE_("Assign"), ICON_NONE);
    edit_row->op("OBJECT_OT_material_slot_select", IFACE_("Select"), ICON_NONE);
    edit_row->op("OBJECT_OT_material_slot_deselect", IFACE_("Deselect"), ICON_NONE);
  }
}

/**
 * Los tres paneles de herramienta de geometria leen `snode.node_tree`, que en la
 * escena de fabrica es `None`. El Python reventaba con `AttributeError` en el primer
 * acceso y Blender se lo tragaba, dejando dibujado SOLO lo anterior. Se reproduce
 * exactamente: la columna ya existe, el resto no.
 */
static void geometry_node_tool_object_types_draw(const bContext *C, Panel *panel)
{
  struct TypeRow {
    const char *prop;
    const char *name;
    int icon;
  };
  static const TypeRow types[] = {
      {"is_type_mesh", N_("Mesh"), ICON_MESH_DATA},
      {"is_type_curve", N_("Hair Curves"), ICON_CURVES_DATA},
      {"is_type_grease_pencil", N_("Grease Pencil"), ICON_OUTLINER_OB_GREASEPENCIL},
      {"is_type_pointcloud", N_("Point Cloud"), ICON_POINTCLOUD_DATA},
  };

  uiLayout *layout = panel->layout;
  const SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *group = snode ? snode->nodetree : nullptr;

  uiLayout *col = &layout->column(false);
  if (group == nullptr) {
    return;
  }
  PointerRNA group_ptr = RNA_id_pointer_create(&group->id);
  uiLayoutSetActive(col, RNA_boolean_get(&group_ptr, "is_tool"));

  for (const TypeRow &type : types) {
    uiLayout *row = &col->row(true);
    row->label(IFACE_(type.name), type.icon);
    row->prop(&group_ptr, type.prop, UI_ITEM_NONE, "", ICON_NONE);
  }
}

static void geometry_node_tool_mode_draw(const bContext *C, Panel *panel)
{
  struct ModeRow {
    const char *prop;
    const char *name;
    int icon;
  };
  static const ModeRow modes[] = {
      {"is_mode_object", N_("Object Mode"), ICON_OBJECT_DATAMODE},
      {"is_mode_edit", N_("Edit Mode"), ICON_EDITMODE_HLT},
      {"is_mode_sculpt", N_("Sculpt Mode"), ICON_SCULPTMODE_HLT},
  };

  uiLayout *layout = panel->layout;
  const SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *group = snode ? snode->nodetree : nullptr;

  uiLayout *col = &layout->column(false);
  if (group == nullptr) {
    return;
  }
  PointerRNA group_ptr = RNA_id_pointer_create(&group->id);
  uiLayoutSetActive(col, RNA_boolean_get(&group_ptr, "is_tool"));

  for (const ModeRow &mode : modes) {
    uiLayout *row = &col->row(true);
    row->label(IFACE_(mode.name), mode.icon);
    row->prop(&group_ptr, mode.prop, UI_ITEM_NONE, "", ICON_NONE);
  }

  if (RNA_boolean_get(&group_ptr, "is_type_grease_pencil")) {
    uiLayout *row = &col->row(true);
    row->label(IFACE_("Draw Mode"), ICON_GREASEPENCIL);
    row->prop(&group_ptr, "is_mode_paint", UI_ITEM_NONE, "", ICON_NONE);
  }
}

static void geometry_node_tool_options_draw(const bContext *C, Panel *panel)
{
  const SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *group = snode ? snode->nodetree : nullptr;
  if (group == nullptr) {
    return;
  }
  PointerRNA group_ptr = RNA_id_pointer_create(&group->id);
  panel->layout->prop(&group_ptr, "use_wait_for_click", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void overlay_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  layout->label(IFACE_("Node Editor Overlays"), ICON_NONE);

  const SpaceNode *snode = CTX_wm_space_node(C);
  if (snode == nullptr) {
    return;
  }
  PointerRNA snode_ptr = space_node_ptr(C);
  PointerRNA overlay_ptr = node_overlay_ptr(C);

  uiLayoutSetActive(layout, RNA_boolean_get(&overlay_ptr, "show_overlays"));

  uiLayout *col = &layout->column(false);
  col->prop(&overlay_ptr, "show_wire_color", UI_ITEM_NONE, IFACE_("Wire Colors"), ICON_NONE);
  col->prop(&overlay_ptr,
            "show_reroute_auto_labels",
            UI_ITEM_NONE,
            IFACE_("Reroute Auto Labels"),
            ICON_NONE);

  col->separator();

  col->prop(&overlay_ptr, "show_context_path", UI_ITEM_NONE, IFACE_("Context Path"), ICON_NONE);
  col->prop(&snode_ptr, "show_annotation", UI_ITEM_NONE, IFACE_("Annotations"), ICON_NONE);

  const StringRef tree_type = tree_type_of(snode);

  if (RNA_boolean_get(&snode_ptr, "supports_previews")) {
    col->separator();
    col->prop(&overlay_ptr, "show_previews", UI_ITEM_NONE, IFACE_("Previews"), ICON_NONE);
    if (tree_type == "ShaderNodeTree") {
      uiLayout *row = &col->row(false);
      row->prop(&overlay_ptr, "preview_shape", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
      uiLayoutSetActive(row, RNA_boolean_get(&overlay_ptr, "show_previews"));
    }
  }

  if (tree_type == "GeometryNodeTree") {
    col->separator();
    col->prop(&overlay_ptr, "show_timing", UI_ITEM_NONE, IFACE_("Timings"), ICON_NONE);
    col->prop(&overlay_ptr,
              "show_named_attributes",
              UI_ITEM_NONE,
              IFACE_("Named Attributes"),
              ICON_NONE);
  }

  if (tree_type == "CompositorNodeTree") {
    col->prop(&overlay_ptr, "show_timing", UI_ITEM_NONE, IFACE_("Timings"), ICON_NONE);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Paneles de la barra lateral
 * \{ */

static bool active_node_poll(const bContext *C, PanelType * /*pt*/)
{
  return active_node_ptr(C).data != nullptr;
}

static bool active_node_color_poll(const bContext *C, PanelType * /*pt*/)
{
  PointerRNA node_ptr = active_node_ptr(C);
  const bNode *node = static_cast<const bNode *>(node_ptr.data);
  if (node == nullptr) {
    return false;
  }
  /* `node.bl_idname == "NodeReroute"`. */
  return !STREQ(node->idname, "NodeReroute");
}

static bool texture_mapping_poll(const bContext *C, PanelType * /*pt*/)
{
  PointerRNA node_ptr = active_node_ptr(C);
  if (node_ptr.data == nullptr) {
    return false;
  }
  if (RNA_struct_find_property(&node_ptr, "texture_mapping") == nullptr) {
    return false;
  }
  /* `COMPAT_ENGINES = {'BLENDER_RENDER', 'BLENDER_WORKBENCH'}`. */
  const Scene *scene = CTX_data_scene(C);
  return scene != nullptr &&
         (STREQ(scene->r.engine, "BLENDER_RENDER") || STREQ(scene->r.engine, "BLENDER_WORKBENCH"));
}

static bool backdrop_poll(const bContext *C, PanelType * /*pt*/)
{
  const SpaceNode *snode = CTX_wm_space_node(C);
  return snode != nullptr && tree_type_of(snode) == "CompositorNodeTree";
}

static bool quality_poll(const bContext *C, PanelType * /*pt*/)
{
  const SpaceNode *snode = CTX_wm_space_node(C);
  return snode != nullptr && tree_type_of(snode) == "CompositorNodeTree" &&
         snode->nodetree != nullptr;
}

static bool node_tree_properties_poll(const bContext *C, PanelType * /*pt*/)
{
  const SpaceNode *snode = CTX_wm_space_node(C);
  if (snode == nullptr) {
    return false;
  }
  const bNodeTree *group = snode->edittree;
  if (group == nullptr) {
    return false;
  }
  return (group->id.flag & ID_FLAG_EMBEDDED_DATA) == 0;
}

static bool node_tree_interface_poll(const bContext *C, PanelType * /*pt*/)
{
  const SpaceNode *snode = CTX_wm_space_node(C);
  if (snode == nullptr) {
    return false;
  }
  const bNodeTree *tree = snode->edittree;
  if (tree == nullptr) {
    return false;
  }
  if (tree->id.flag & ID_FLAG_EMBEDDED_DATA) {
    return false;
  }
  /* `tree.bl_use_group_interface`. */
  return tree->typeinfo != nullptr && tree->typeinfo->no_group_interface == 0;
}

/** El primer elemento del panel activo, si es el socket que hace de interruptor. */
static bNodeTreeInterfaceSocket *panel_toggle_socket_get(const bContext *C)
{
  const SpaceNode *snode = CTX_wm_space_node(C);
  if (snode == nullptr) {
    return nullptr;
  }
  bNodeTree *tree = snode->edittree;
  if (tree == nullptr) {
    return nullptr;
  }
  bNodeTreeInterfaceItem *active_item = tree->tree_interface.active_item();
  if (active_item == nullptr || active_item->item_type != NODE_INTERFACE_PANEL) {
    return nullptr;
  }
  bNodeTreeInterfacePanel *active_panel = reinterpret_cast<bNodeTreeInterfacePanel *>(active_item);
  if (active_panel->items_num == 0) {
    return nullptr;
  }
  bNodeTreeInterfaceItem *first_item = active_panel->items_array[0];
  if (first_item == nullptr || first_item->item_type != NODE_INTERFACE_SOCKET) {
    /* `getattr(first_item, "is_panel_toggle", False)`: un panel no tiene esa
     * propiedad, asi que el `getattr` devolvia `False`. */
    return nullptr;
  }
  bNodeTreeInterfaceSocket *socket = reinterpret_cast<bNodeTreeInterfaceSocket *>(first_item);
  if ((socket->flag & NODE_INTERFACE_SOCKET_PANEL_TOGGLE) == 0) {
    return nullptr;
  }
  return socket;
}

static bool node_tree_interface_panel_toggle_poll(const bContext *C, PanelType * /*pt*/)
{
  return panel_toggle_socket_get(C) != nullptr;
}

static void active_node_generic_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA node_ptr = active_node_ptr(C);
  if (node_ptr.data == nullptr) {
    return;
  }
  /* `node.id_data`: el arbol al que pertenece el nodo. */
  const bNodeTree *tree = reinterpret_cast<const bNodeTree *>(node_ptr.owner_id);

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  layout->prop(&node_ptr, "name", UI_ITEM_NONE, std::nullopt, ICON_NODE);
  layout->prop(&node_ptr, "label", UI_ITEM_NONE, std::nullopt, ICON_NODE);

  if (tree != nullptr && tree->type == NTREE_GEOMETRY) {
    layout->prop(&node_ptr, "warning_propagation", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
}

static void active_node_color_draw_header(const bContext *C, Panel *panel)
{
  PointerRNA node_ptr = active_node_ptr(C);
  if (node_ptr.data == nullptr) {
    return;
  }
  panel->layout->prop(&node_ptr, "use_custom_color", UI_ITEM_NONE, "", ICON_NONE);
}

static void active_node_color_draw_header_preset(const bContext *C, Panel *panel)
{
  flipendo::preset::ui::draw_panel_header(C, panel->layout, "NODE_PT_node_color_presets");
}

static void active_node_color_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  PointerRNA node_ptr = active_node_ptr(C);
  if (node_ptr.data == nullptr) {
    return;
  }

  uiLayoutSetEnabled(layout, RNA_boolean_get(&node_ptr, "use_custom_color"));

  uiLayout *row = &layout->row(false);
  row->prop(&node_ptr, "color", UI_ITEM_NONE, "", ICON_NONE);
  row->menu("NODE_MT_node_color_context_menu", "", ICON_DOWNARROW_HLT);
}

static void active_node_properties_draw(const bContext *C, Panel *panel)
{
  PointerRNA node_ptr = active_node_ptr(C);
  if (node_ptr.data == nullptr) {
    return;
  }
  uiTemplateNodeInputs(panel->layout, const_cast<bContext *>(C), &node_ptr);
}

static void texture_mapping_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false); /* Sin animacion. */

  PointerRNA node_ptr = active_node_ptr(C);
  if (node_ptr.data == nullptr) {
    return;
  }
  PointerRNA mapping = RNA_pointer_get(&node_ptr, "texture_mapping");

  layout->prop(&mapping, "vector_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  layout->separator();

  uiLayout *col = &layout->column(true);
  col->prop(&mapping, "mapping_x", UI_ITEM_NONE, IFACE_("Projection X"), ICON_NONE);
  col->prop(&mapping, "mapping_y", UI_ITEM_NONE, IFACE_("Y"), ICON_NONE);
  col->prop(&mapping, "mapping_z", UI_ITEM_NONE, IFACE_("Z"), ICON_NONE);

  layout->separator();

  layout->prop(&mapping, "translation", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(&mapping, "rotation", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(&mapping, "scale", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void active_tool_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  /* La cabecera de la herramienta activa: dibujo nativo (`FL_toolbar_ui.hh`), el
   * mismo al que llamaba `template_tool_header` desde el Python. */
  flipendo::ui::tool_header_draw(C, &layout->column(false), true, SPACE_NODE, nullptr);
}

static void backdrop_draw_header(const bContext *C, Panel *panel)
{
  PointerRNA snode_ptr = space_node_ptr(C);
  if (snode_ptr.data == nullptr) {
    return;
  }
  panel->layout->prop(&snode_ptr, "show_backdrop", UI_ITEM_NONE, "", ICON_NONE);
}

static void backdrop_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  PointerRNA snode_ptr = space_node_ptr(C);
  if (snode_ptr.data == nullptr) {
    return;
  }
  uiLayoutSetActive(layout, RNA_boolean_get(&snode_ptr, "show_backdrop"));

  uiLayout *col = &layout->column(false);

  col->prop(&snode_ptr, "backdrop_channels", UI_ITEM_NONE, IFACE_("Channels"), ICON_NONE);
  col->prop(&snode_ptr, "backdrop_zoom", UI_ITEM_NONE, IFACE_("Zoom"), ICON_NONE);

  col->prop(&snode_ptr, "backdrop_offset", UI_ITEM_NONE, IFACE_("Offset"), ICON_NONE);

  col->separator();

  col->op("NODE_OT_backimage_move", IFACE_("Move"), ICON_NONE);
  col->op("NODE_OT_backimage_fit", IFACE_("Fit"), ICON_NONE);
}

static void quality_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  Scene *scene = CTX_data_scene(C);
  const SpaceNode *snode = CTX_wm_space_node(C);
  if (scene == nullptr || snode == nullptr || snode->nodetree == nullptr) {
    return;
  }
  PointerRNA rd_ptr = RNA_pointer_create_id_subdata(scene->id, &RNA_RenderSettings, &scene->r);
  PointerRNA tree_ptr = RNA_id_pointer_create(&snode->nodetree->id);

  uiLayout *col = &layout->column(false);
  col->prop(&rd_ptr, "compositor_device", UI_ITEM_NONE, IFACE_("Device"), ICON_NONE);
  if (RNA_enum_get(&rd_ptr, "compositor_device") == SCE_COMPOSITOR_DEVICE_GPU) {
    col->prop(&rd_ptr, "compositor_precision", UI_ITEM_NONE, IFACE_("Precision"), ICON_NONE);
  }

  col = &layout->column(false);
  col->prop(&tree_ptr, "use_viewer_border", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void node_tree_properties_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  const SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *group = snode ? snode->edittree : nullptr;
  if (group == nullptr) {
    return;
  }
  PointerRNA group_ptr = RNA_id_pointer_create(&group->id);

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  layout->prop(&group_ptr, "name", UI_ITEM_NONE, IFACE_("Name"), ICON_NONE);

  if (group->id.asset_data != nullptr) {
    PointerRNA asset_ptr = RNA_pointer_create_discrete(
        &group->id, &RNA_AssetMetaData, group->id.asset_data);
    layout->prop(&asset_ptr, "description", UI_ITEM_NONE, IFACE_("Description"), ICON_NONE);
  }
  else {
    layout->prop(&group_ptr, "description", UI_ITEM_NONE, IFACE_("Description"), ICON_NONE);
  }

  /* `group.bl_use_group_interface`. */
  if (group->typeinfo == nullptr || group->typeinfo->no_group_interface != 0) {
    return;
  }

  layout->prop(&group_ptr, "color_tag", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  uiLayout *row = &layout->row(true);
  row->prop(&group_ptr, "default_group_node_width", UI_ITEM_NONE, IFACE_("Node Width"), ICON_NONE);
  row->op("NODE_OT_default_group_width_set", "", ICON_NODE);

  if (StringRef(group->idname) == "GeometryNodeTree") {
    PanelLayout usage = layout->panel(C, "group_usage", false);
    usage.header->label(IFACE_("Usage"), ICON_NONE);
    if (usage.body) {
      uiLayout *col = &usage.body->column(true);
      col->prop(&group_ptr, "is_modifier", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      col->prop(&group_ptr, "is_tool", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    }
  }
}

static void node_tree_interface_draw(const bContext *C, Panel *panel)
{
  static const char *field_socket_types[] = {
      "NodeSocketInt",
      "NodeSocketColor",
      "NodeSocketVector",
      "NodeSocketBool",
      "NodeSocketFloat",
  };

  uiLayout *layout = panel->layout;

  const SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *tree = snode ? snode->edittree : nullptr;
  if (tree == nullptr) {
    return;
  }
  PointerRNA interface_ptr = RNA_pointer_create_discrete(
      &tree->id, &RNA_NodeTreeInterface, &tree->tree_interface);

  uiLayout *split = &layout->row(false);

  uiTemplateNodeTreeInterface(split, const_cast<bContext *>(C), &interface_ptr);

  uiLayout *ops_col = &split->column(true);
  uiLayoutSetEnabled(ops_col, tree->id.lib == nullptr);
  uiItemMenuEnumO(ops_col, C, "NODE_OT_interface_item_new", "item_type", "", ICON_ADD);
  ops_col->op("NODE_OT_interface_item_remove", "", ICON_REMOVE);
  ops_col->separator();
  ops_col->menu("NODE_MT_node_tree_interface_context_menu", "", ICON_DOWNARROW_HLT);

  ops_col->separator();

  bNodeTreeInterfaceItem *active_item = tree->tree_interface.active_item();
  if (active_item == nullptr) {
    return;
  }

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  if (active_item->item_type == NODE_INTERFACE_SOCKET) {
    bNodeTreeInterfaceSocket *socket = reinterpret_cast<bNodeTreeInterfaceSocket *>(active_item);
    PointerRNA item_ptr = RNA_pointer_create_discrete(
        &tree->id, &RNA_NodeTreeInterfaceSocket, socket);

    layout->prop(&item_ptr, "socket_type", UI_ITEM_NONE, IFACE_("Type"), ICON_NONE);
    layout->prop(&item_ptr, "description", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    /* Las descripciones solo se muestran en nodos de geometria: es lo unico que las
     * usa, en el panel del modificador. */
    if (tree->type == NTREE_GEOMETRY) {
      bool is_field_socket = false;
      for (const char *type : field_socket_types) {
        if (socket->socket_type != nullptr && STREQ(socket->socket_type, type)) {
          is_field_socket = true;
          break;
        }
      }
      if (is_field_socket) {
        if (socket->flag & NODE_INTERFACE_SOCKET_OUTPUT) {
          layout->prop(&item_ptr, "attribute_domain", UI_ITEM_NONE, std::nullopt, ICON_NONE);
        }
        layout->prop(&item_ptr, "default_attribute_name", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      }
    }

    /* `if hasattr(active_item, "draw"): active_item.draw(context, layout)`.
     *
     * `draw` es una funcion RNA declarada SIEMPRE en `NodeTreeInterfaceSocket`, asi
     * que el `hasattr` era siempre cierto; lo que hacia la llamada era el
     * `interface_draw` del tipo de socket, que es lo que se llama aqui
     * (`rna_NodeTreeInterfaceSocket_draw_builtin`). Los tipos de socket registrados
     * desde Python no existen sin interprete, asi que no hay nada mas que llamar. */
    if (bNodeSocketTypeHandle *stype = socket->socket_typeinfo()) {
      if (stype->interface_draw) {
        stype->interface_draw(&tree->id, socket, const_cast<bContext *>(C), layout);
      }
    }
  }

  if (active_item->item_type == NODE_INTERFACE_PANEL) {
    PointerRNA item_ptr = RNA_pointer_create_discrete(
        &tree->id, &RNA_NodeTreeInterfacePanel, active_item);
    layout->prop(&item_ptr, "description", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    layout->prop(&item_ptr, "default_closed", UI_ITEM_NONE, IFACE_("Closed by Default"), ICON_NONE);
  }

  uiLayoutSetPropSep(layout, false);
}

static void node_tree_interface_panel_toggle_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;

  const SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *tree = snode ? snode->edittree : nullptr;
  bNodeTreeInterfaceSocket *socket = panel_toggle_socket_get(C);
  if (tree == nullptr || socket == nullptr) {
    return;
  }
  PointerRNA item_ptr = RNA_pointer_create_discrete(
      &tree->id, &RNA_NodeTreeInterfaceSocket, socket);

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  layout->prop(&item_ptr, "default_value", UI_ITEM_NONE, IFACE_("Default"), ICON_NONE);

  uiLayout *col = &layout->column(false);
  col->prop(&item_ptr, "hide_in_modifier", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(&item_ptr, "force_non_field", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  uiLayoutSetPropSep(layout, false);
}

/**
 * El clon `NODE_WORLD_PT_viewport_display` que `node_panel()` sacaba de la pestana
 * Mundo. NO se reimplementa: se llama a la funcion compartida que dejo escrita el
 * carril de Propiedades (`FL_properties_ui.hpp`, commit «la pestana Mundo pasa a
 * C++»). Es la decision del proyecto: funcion compartida, nunca copia.
 */
static bool world_viewport_display_poll(const bContext *C, PanelType * /*pt*/)
{
  return CTX_data_pointer_get_type(C, "world", &RNA_World).data != nullptr;
}

/** `NODE_PT_node_color_presets`, con el dibujo de presets nativo (D4.1). */
static void node_color_presets_draw(const bContext *C, Panel *panel)
{
  static const flipendo::preset::ui::MenuSpec node_color = {
      /*subdir*/ "node_color",
      /*op*/ "SCRIPT_OT_execute_preset",
      /*menu_idname*/ "NODE_PT_node_color_presets",
      /*add_op*/ "NODE_OT_node_color_preset_add",
  };
  flipendo::preset::ui::draw_panel(C, panel->layout, node_color);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registro
 *
 * El ORDEN de estas tablas es el orden en que los paneles salen en su region: el
 * bloque `REGION ...` del volcado es literalmente la lista, no un conjunto. Es el
 * mismo orden que tenia la tupla `classes` de `space_node.py`.
 * \{ */

static const flipendo::HeaderDecl node_headers[] = {
    {
        /*idname*/ "NODE_HT_header",
        /*draw*/ node_header_draw,
    },
};

static const flipendo::MenuDecl node_ui_menus[] = {
    {
        /*idname*/ "NODE_MT_editor_menus",
        /*label*/ "",
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ editor_menus_draw,
    },
    {
        /*idname*/ "NODE_MT_view",
        /*label*/ N_("View"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ view_menu_draw,
    },
    {
        /*idname*/ "NODE_MT_select",
        /*label*/ N_("Select"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ select_menu_draw,
    },
    {
        /*idname*/ "NODE_MT_node",
        /*label*/ N_("Node"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ node_menu_draw,
    },
    {
        /*idname*/ "NODE_MT_node_color_context_menu",
        /*label*/ N_("Node Color Specials"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ node_color_context_menu_draw,
    },
    {
        /*idname*/ "NODE_MT_context_menu_show_hide_menu",
        /*label*/ N_("Show/Hide"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ context_menu_show_hide_draw,
    },
    {
        /*idname*/ "NODE_MT_context_menu_select_menu",
        /*label*/ N_("Select"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ context_menu_select_draw,
    },
    {
        /*idname*/ "NODE_MT_node_tree_interface_context_menu",
        /*label*/ N_("Node Tree Interface Specials"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ node_tree_interface_context_menu_draw,
    },
};

static const flipendo::PanelDecl node_header_panels[] = {
    {
        /*idname*/ "NODE_PT_material_slots",
        /*label*/ N_("Slot"),
        /*category*/ nullptr,
        /*context*/ nullptr,
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ material_slots_draw,
        /*draw_header*/ material_slots_draw_header,
        /*draw_header_preset*/ nullptr,
        /*poll*/ nullptr,
        /*flag*/ 0,
        /*order*/ 0,
        /*ui_units_x*/ 12,
    },
    {
        /*idname*/ "NODE_PT_geometry_node_tool_object_types",
        /*label*/ N_("Object Types"),
        /*category*/ nullptr,
        /*context*/ nullptr,
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ geometry_node_tool_object_types_draw,
        /*draw_header*/ nullptr,
        /*draw_header_preset*/ nullptr,
        /*poll*/ nullptr,
        /*flag*/ 0,
        /*order*/ 0,
        /*ui_units_x*/ 8,
    },
    {
        /*idname*/ "NODE_PT_geometry_node_tool_mode",
        /*label*/ N_("Modes"),
        /*category*/ nullptr,
        /*context*/ nullptr,
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ geometry_node_tool_mode_draw,
        /*draw_header*/ nullptr,
        /*draw_header_preset*/ nullptr,
        /*poll*/ nullptr,
        /*flag*/ 0,
        /*order*/ 0,
        /*ui_units_x*/ 8,
    },
    {
        /*idname*/ "NODE_PT_geometry_node_tool_options",
        /*label*/ N_("Options"),
        /*category*/ nullptr,
        /*context*/ nullptr,
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ geometry_node_tool_options_draw,
        /*draw_header*/ nullptr,
        /*draw_header_preset*/ nullptr,
        /*poll*/ nullptr,
        /*flag*/ 0,
        /*order*/ 0,
        /*ui_units_x*/ 8,
    },
    {
        /*idname*/ "NODE_PT_overlay",
        /*label*/ N_("Overlays"),
        /*category*/ nullptr,
        /*context*/ nullptr,
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ overlay_draw,
        /*draw_header*/ nullptr,
        /*draw_header_preset*/ nullptr,
        /*poll*/ nullptr,
        /*flag*/ 0,
        /*order*/ 0,
        /*ui_units_x*/ 7,
    },
    {
        /*idname*/ "NODE_PT_gizmo_display",
        /*label*/ N_("Gizmos"),
        /*category*/ nullptr,
        /*context*/ nullptr,
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ gizmo_display_draw,
        /*draw_header*/ nullptr,
        /*draw_header_preset*/ nullptr,
        /*poll*/ nullptr,
        /*flag*/ 0,
        /*order*/ 0,
        /*ui_units_x*/ 8,
    },
};

static const flipendo::PanelDecl node_sidebar_panels[] = {
    {
        /*idname*/ "NODE_PT_node_tree_properties",
        /*label*/ N_("Group"),
        /*category*/ "Group",
        /*context*/ nullptr,
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ node_tree_properties_draw,
        /*draw_header*/ nullptr,
        /*draw_header_preset*/ nullptr,
        /*poll*/ node_tree_properties_poll,
    },
    {
        /*idname*/ "NODE_PT_node_tree_interface",
        /*label*/ N_("Group Sockets"),
        /*category*/ "Group",
        /*context*/ nullptr,
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ node_tree_interface_draw,
        /*draw_header*/ nullptr,
        /*draw_header_preset*/ nullptr,
        /*poll*/ node_tree_interface_poll,
    },
    {
        /*idname*/ "NODE_PT_node_tree_interface_panel_toggle",
        /*label*/ N_("Panel Toggle"),
        /*category*/ "Group",
        /*context*/ nullptr,
        /*parent_id*/ "NODE_PT_node_tree_interface",
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ node_tree_interface_panel_toggle_draw,
        /*draw_header*/ nullptr,
        /*draw_header_preset*/ nullptr,
        /*poll*/ node_tree_interface_panel_toggle_poll,
    },
    {
        /*idname*/ "NODE_PT_active_node_generic",
        /*label*/ N_("Node"),
        /*category*/ "Node",
        /*context*/ nullptr,
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ active_node_generic_draw,
        /*draw_header*/ nullptr,
        /*draw_header_preset*/ nullptr,
        /*poll*/ active_node_poll,
    },
    {
        /*idname*/ "NODE_PT_active_node_color",
        /*label*/ N_("Color"),
        /*category*/ "Node",
        /*context*/ nullptr,
        /*parent_id*/ "NODE_PT_active_node_generic",
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ active_node_color_draw,
        /*draw_header*/ active_node_color_draw_header,
        /*draw_header_preset*/ active_node_color_draw_header_preset,
        /*poll*/ active_node_color_poll,
        /*flag*/ PANEL_TYPE_DEFAULT_CLOSED,
    },
    {
        /*idname*/ "NODE_PT_texture_mapping",
        /*label*/ N_("Texture Mapping"),
        /*category*/ "Node",
        /*context*/ nullptr,
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ texture_mapping_draw,
        /*draw_header*/ nullptr,
        /*draw_header_preset*/ nullptr,
        /*poll*/ texture_mapping_poll,
        /*flag*/ PANEL_TYPE_DEFAULT_CLOSED,
    },
    {
        /*idname*/ "NODE_PT_active_tool",
        /*label*/ N_("Active Tool"),
        /*category*/ "Tool",
        /*context*/ nullptr,
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ active_tool_draw,
    },
    {
        /*idname*/ "NODE_PT_backdrop",
        /*label*/ N_("Backdrop"),
        /*category*/ "View",
        /*context*/ nullptr,
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ backdrop_draw,
        /*draw_header*/ backdrop_draw_header,
        /*draw_header_preset*/ nullptr,
        /*poll*/ backdrop_poll,
    },
    {
        /*idname*/ "NODE_PT_quality",
        /*label*/ N_("Performance"),
        /*category*/ "Options",
        /*context*/ nullptr,
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ quality_draw,
        /*draw_header*/ nullptr,
        /*draw_header_preset*/ nullptr,
        /*poll*/ quality_poll,
    },
    {
        /*idname*/ "NODE_PT_active_node_properties",
        /*label*/ N_("Properties"),
        /*category*/ "Node",
        /*context*/ nullptr,
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ active_node_properties_draw,
        /*draw_header*/ nullptr,
        /*draw_header_preset*/ nullptr,
        /*poll*/ active_node_poll,
    },
    {
        /*idname*/ "NODE_WORLD_PT_viewport_display",
        /*label*/ N_("Viewport Display"),
        /*category*/ "Options",
        /*context*/ "world",
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ flipendo::properties_ui::world_viewport_display_draw,
        /*draw_header*/ nullptr,
        /*draw_header_preset*/ nullptr,
        /*poll*/ world_viewport_display_poll,
        /*flag*/ PANEL_TYPE_DEFAULT_CLOSED,
        /*order*/ 10,
    },
};

/**
 * `NODE_PT_node_color_presets` hereda de `PresetPanel`, cuyo `bl_space_type` es
 * `'PROPERTIES'` y su `bl_region_type` `'HEADER'`. O sea: el panel de presets del
 * editor de nodos vive, desde siempre, en la cabecera del editor de Propiedades.
 * Se copia tal cual; cambiarlo de sitio seria cambiar la interfaz.
 */
static const flipendo::PanelDecl node_preset_panels[] = {
    {
        /*idname*/ "NODE_PT_node_color_presets",
        /*label*/ N_("Color Presets"),
        /*category*/ nullptr,
        /*context*/ nullptr,
        /*parent_id*/ nullptr,
        /*description*/ N_("Predefined node color"),
        /*translation_context*/ nullptr,
        /*draw*/ node_color_presets_draw,
    },
};

void node_ui_menus_register()
{
  flipendo::menus_register({node_ui_menus, ARRAY_SIZE(node_ui_menus)});
}

void node_ui_header_register(ARegionType *art)
{
  flipendo::headers_register(
      art, SPACE_NODE, RGN_TYPE_HEADER, {node_headers, ARRAY_SIZE(node_headers)});
}

void node_ui_header_panels_register(ARegionType *art)
{
  flipendo::panels_register(
      art, SPACE_NODE, {node_header_panels, ARRAY_SIZE(node_header_panels)});
}

void node_ui_panels_register(ARegionType *art)
{
  flipendo::panels_register(
      art, SPACE_NODE, {node_sidebar_panels, ARRAY_SIZE(node_sidebar_panels)});
}

void node_preset_panels_register()
{
  /* La region de la cabecera de Propiedades no existe cuando se registra el editor
   * de nodos (`ED_spacetype_node()` va ANTES que `ED_spacetype_buttons()`), asi que
   * esto lo llama `ED_spacetypes_init()` despues del segundo. */
  SpaceType *st = BKE_spacetype_from_id(SPACE_PROPERTIES);
  if (st == nullptr) {
    BLI_assert_unreachable();
    return;
  }
  ARegionType *art = BKE_regiontype_from_id(st, RGN_TYPE_HEADER);
  if (art == nullptr) {
    BLI_assert_unreachable();
    return;
  }
  flipendo::panels_register(
      art, SPACE_PROPERTIES, {node_preset_panels, ARRAY_SIZE(node_preset_panels)});
}

/** \} */

}  // namespace blender::ed::space_node
