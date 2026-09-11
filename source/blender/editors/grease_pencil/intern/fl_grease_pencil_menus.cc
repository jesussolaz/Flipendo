/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgreasepencil
 *
 * Menus nativos del lapiz de cera que el keymap nativo abre por nombre y que
 * solo existian como clase de Python en
 * `scripts/startup/bl_ui/properties_grease_pencil_common.py`:
 * `GREASE_PENCIL_MT_layer_active` (Y), `GREASE_PENCIL_MT_move_to_layer` (M) y
 * `GREASE_PENCIL_MT_snap_pie` (Shift-S). Ver
 * `politicas/MENUS-DEL-KEYMAP-A-CPP.md`.
 *
 * Los dos primeros son los primeros menus **generados a partir de datos** de
 * toda esta migracion: una fila por capa del lapiz, en orden inverso, con el
 * icono puesto solo en la activa. Se recorren por RNA (`layers`, `layers.active`)
 * y no por el DNA, que es el mismo camino que recorria el Python.
 *
 * No tienen editor propio, asi que el registro cuelga de
 * `ED_operatortypes_grease_pencil()`, que es lo que corre al arrancar.
 */

#include <optional>
#include <string>

#include "BLI_utildefines.h"

#include "MEM_guardedalloc.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "DNA_grease_pencil_types.h"
#include "DNA_object_types.h"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_types.hh"

#include "FL_ui_registry.hh"

#include "ED_grease_pencil.hh"

#include "FL_grease_pencil_menus.hh"

namespace blender::ed::greasepencil {

/**
 * `context.active_object.data`, SEA DEL TIPO QUE SEA.
 *
 * Trampa: el Python no comprueba el tipo. Con el cubo de fabrica seleccionado,
 * `obd` es una malla, dibuja igualmente el boton «New Layer» y solo DESPUES
 * revienta al pedir `obd.layers`. Filtrar aqui por `OB_GREASE_PENCIL` deja el
 * menu vacio, y el volcado de dibujo lo canto: dos botones de menos.
 */
static PointerRNA active_object_data(const bContext *C)
{
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr || ob->data == nullptr) {
    return PointerRNA_NULL;
  }
  return RNA_id_pointer_create(static_cast<ID *>(ob->data));
}

/**
 * Recorre `data.layers` al reves y llama a `fn(indice, capa, es_la_activa)`.
 *
 * El Python hace `for i in range(len(layers) - 1, -1, -1)`, asi que el orden
 * importa: la capa de arriba sale la primera.
 */
template<typename Fn>
static void foreach_layer_reversed(PointerRNA &data, Fn &&fn)
{
  PropertyRNA *layers_prop = RNA_struct_find_property(&data, "layers");
  if (layers_prop == nullptr) {
    return;
  }
  const int layers_num = RNA_property_collection_length(&data, layers_prop);
  if (layers_num == 0) {
    return;
  }
  /* `layers.active`: en RNA cuelga de la coleccion `GreasePencilLayers`, y su
   * getter (`rna_GreasePencil_active_layer_get`) devuelve
   * `grease_pencil->get_active_layer()`. Se va por el dato directamente, que es
   * lo mismo y no obliga a fabricar el puntero de la coleccion. */
  const GreasePencil *grease_pencil = static_cast<const GreasePencil *>(data.data);
  const void *active_layer_data = grease_pencil->has_active_layer() ?
                                      static_cast<const void *>(grease_pencil->get_active_layer()) :
                                      nullptr;

  for (int i = layers_num - 1; i >= 0; i--) {
    PointerRNA layer;
    if (!RNA_property_collection_lookup_int(&data, layers_prop, i, &layer)) {
      continue;
    }
    const bool is_active = active_layer_data != nullptr && layer.data == active_layer_data;
    fn(i, layer, is_active);
  }
}

/** Nombre de la capa, tal cual lo pone el Python (`layer.name`). */
static std::string layer_name(PointerRNA &layer)
{
  char *name = RNA_string_get_alloc(&layer, "name", nullptr, 0, nullptr);
  std::string out = name ? name : "";
  if (name) {
    MEM_freeN(name);
  }
  return out;
}

/* -------------------------------------------------------------------- */
/** \name GREASE_PENCIL_MT_layer_active
 * \{ */

static void layer_active_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;
  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);

  PointerRNA data = active_object_data(C);
  if (data.data == nullptr) {
    return;
  }

  PointerRNA props = layout->op("GREASE_PENCIL_OT_layer_add", IFACE_("New Layer"), ICON_ADD);
  if (props.data) {
    RNA_string_set(&props, "new_layer_name", "Layer");
  }

  /* Aqui es donde el Python revienta si el dato no es lapiz de cera. */
  PropertyRNA *layers_prop = RNA_struct_find_property(&data, "layers");
  if (layers_prop == nullptr || RNA_property_collection_length(&data, layers_prop) == 0) {
    return;
  }

  layout->separator();

  foreach_layer_reversed(data, [&](int i, PointerRNA &layer, bool is_active) {
    const std::string name = layer_name(layer);
    PointerRNA op = layout->op(
        "GREASE_PENCIL_OT_layer_active", name, is_active ? ICON_GREASEPENCIL : ICON_NONE);
    if (op.data) {
      RNA_int_set(&op, "layer", i);
    }
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GREASE_PENCIL_MT_move_to_layer
 * \{ */

static void move_to_layer_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;
  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);

  PointerRNA data = active_object_data(C);
  if (data.data == nullptr) {
    return;
  }

  PointerRNA props = layout->op("GREASE_PENCIL_OT_move_to_layer", IFACE_("New Layer"), ICON_ADD);
  if (props.data) {
    RNA_boolean_set(&props, "add_new_layer", true);
  }

  /* Aqui es donde el Python revienta si el dato no es lapiz de cera. */
  PropertyRNA *layers_prop = RNA_struct_find_property(&data, "layers");
  if (layers_prop == nullptr || RNA_property_collection_length(&data, layers_prop) == 0) {
    return;
  }

  layout->separator();

  foreach_layer_reversed(data, [&](int /*i*/, PointerRNA &layer, bool is_active) {
    const std::string name = layer_name(layer);
    PointerRNA op = layout->op(
        "GREASE_PENCIL_OT_move_to_layer", name, is_active ? ICON_GREASEPENCIL : ICON_NONE);
    if (op.data) {
      RNA_string_set(&op, "target_layer_name", name.c_str());
    }
  });
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name GREASE_PENCIL_MT_snap_pie
 * \{ */

static void snap_pie_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  pie.op("VIEW3D_OT_snap_cursor_to_grid", IFACE_("Cursor to Grid"), ICON_CURSOR);
  pie.op("GREASE_PENCIL_OT_snap_to_grid",
         IFACE_("Selection to Grid"),
         ICON_RESTRICT_SELECT_OFF);
  pie.op("GREASE_PENCIL_OT_snap_cursor_to_selected",
         IFACE_("Cursor to Selected"),
         ICON_CURSOR);
  PointerRNA props = pie.op(
      "GREASE_PENCIL_OT_snap_to_cursor", IFACE_("Selection to Cursor"), ICON_RESTRICT_SELECT_OFF);
  if (props.data) {
    RNA_boolean_set(&props, "use_offset", false);
  }
  props = pie.op("GREASE_PENCIL_OT_snap_to_cursor",
                 IFACE_("Selection to Cursor (Keep Offset)"),
                 ICON_RESTRICT_SELECT_OFF);
  if (props.data) {
    RNA_boolean_set(&props, "use_offset", true);
  }
  pie.separator();
  pie.op("VIEW3D_OT_snap_cursor_to_center", IFACE_("Cursor to World Origin"), ICON_CURSOR);
  pie.separator();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registro
 * \{ */

static const flipendo::MenuDecl grease_pencil_menus[] = {
    {
        /*idname*/ "GREASE_PENCIL_MT_layer_active",
        /*label*/ N_("Change Active Layer"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ layer_active_draw,
    },
    {
        /*idname*/ "GREASE_PENCIL_MT_move_to_layer",
        /*label*/ N_("Move to Layer"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ move_to_layer_draw,
    },
    {
        /*idname*/ "GREASE_PENCIL_MT_snap_pie",
        /*label*/ N_("Snap"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ snap_pie_draw,
    },
};

void menus_register()
{
  flipendo::menus_register({grease_pencil_menus, ARRAY_SIZE(grease_pencil_menus)});
}

/** \} */

}  // namespace blender::ed::greasepencil
