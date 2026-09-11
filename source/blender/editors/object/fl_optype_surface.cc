/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 *
 * Implementacion de `--fl-dump-optypes`. Ver `FL_optype_surface.hh`.
 */

#include <algorithm>
#include <cstdio>
#include <string>

#include "MEM_guardedalloc.h"

#include "BLI_array.hh"
#include "BLI_utildefines.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "BKE_appdir.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "BLI_span.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "FL_selftest_compare.hh"

#include "FL_optype_surface.hh"

namespace flipendo::optype_surface {

using namespace blender;

namespace {

/* Los operadores que este carril migra de `scripts/startup/bl_operators/` a C++. El
 * orden es el del guion de captura que congelo la linea base contra el binario con
 * Python; anadir uno obliga a recapturarla. */
const char *tracked_idnames[] = {
    "PAINT_OT_vertex_color_dirt",
    "RIGIDBODY_OT_object_settings_copy",
    "RIGIDBODY_OT_bake_to_keyframes",
    "RIGIDBODY_OT_connect",
    "VIEW3D_OT_edit_mesh_extrude_individual_move",
    "VIEW3D_OT_edit_mesh_extrude_move_normal",
    "VIEW3D_OT_edit_mesh_extrude_move_shrink_fatten",
    "VIEW3D_OT_edit_mesh_extrude_manifold_normal",
    "VIEW3D_OT_transform_gizmo_set",
    "OBJECT_OT_select_pattern",
    "OBJECT_OT_select_camera",
    "OBJECT_OT_select_hierarchy",
    "OBJECT_OT_make_dupli_face",
    "OBJECT_OT_isolate_type_render",
    "OBJECT_OT_hide_render_clear_all",
    "OBJECT_OT_instance_offset_from_cursor",
    "OBJECT_OT_instance_offset_to_cursor",
    "OBJECT_OT_instance_offset_from_object",
    "MESH_OT_faces_mirror_uv",
    "MESH_OT_select_next_item",
    "MESH_OT_select_prev_item",
};

/* Los operadores de `scripts/startup/bl_operators/presets.py` (carril OPS-3). Van en
 * una lista aparte, y con su propia linea base, porque la de arriba esta CONGELADA
 * contra un binario que ya no existe: los `.py` de esos operadores se retiraron y no
 * hay forma de volver a capturarla. Anadir un idname alli obligaria a recapturar lo
 * irrepetible.
 *
 * Orden alfabetico, que es el del guion de captura. */
const char *preset_idnames[] = {
    "CAMERA_OT_preset_add",
    "CAMERA_OT_safe_areas_preset_add",
    "CLIP_OT_camera_preset_add",
    "CLIP_OT_track_color_preset_add",
    "CLIP_OT_tracking_settings_preset_add",
    "CLOTH_OT_preset_add",
    "FLUID_OT_preset_add",
    "NODE_OT_node_color_preset_add",
    "PARTICLE_OT_hair_dynamics_preset_add",
    "RENDER_OT_color_management_white_balance_preset_add",
    "RENDER_OT_eevee_raytracing_preset_add",
    "RENDER_OT_preset_add",
    "SCENE_OT_gpencil_brush_preset_add",
    "SCENE_OT_gpencil_material_preset_add",
    "SCRIPT_OT_execute_preset",
    "TEXT_EDITOR_OT_preset_add",
    "WM_OT_interface_theme_preset_add",
    "WM_OT_interface_theme_preset_remove",
    "WM_OT_interface_theme_preset_save",
    "WM_OT_keyconfig_preset_add",
    "WM_OT_keyconfig_preset_remove",
    "WM_OT_operator_preset_add",
    "WM_OT_operator_presets_cleanup",
};

/* Los tres de niveles de detalle de `bl_operators/object.py` (carril OPS-3). Otra lista
 * aparte por el mismo motivo: cada linea base se congela contra el binario que todavia
 * tenia SU Python, y anadir un idname a una lista ya congelada la invalida. */
const char *lod_idnames[] = {
    "OBJECT_OT_lod_by_name",
    "OBJECT_OT_lod_clear_all",
    "OBJECT_OT_lod_generate",
};

/**
 * Las banderas de una propiedad, como las ve Python (`prop.is_hidden`, `is_skip_save`...).
 *
 * Son parte del contrato que exige la doctrina ("mismas propiedades ... y flags") y NO
 * estaban en el volcado v1. Se anaden solo en el modo `with_flags`, que usa la lista de
 * presets: el volcado v1 tiene que seguir saliendo byte a byte igual o su linea base
 * congelada dejaria de valer.
 *
 * `SKIP_PRESET` es derivada, igual que en `rna_Property_is_skip_preset_get`: la encienden
 * tambien `PROP_HIDDEN` y `PROP_SKIP_SAVE`.
 */
std::string fmt_prop_flags(PropertyRNA *prop)
{
  const int flag = int(RNA_property_flag(prop));
  Vector<const char *> names;
  if (flag & PROP_ANIMATABLE) {
    names.append("ANIMATABLE");
  }
  if (flag & PROP_HIDDEN) {
    names.append("HIDDEN");
  }
  if (flag & PROP_LIB_EXCEPTION) {
    names.append("LIBRARY_EDITABLE");
  }
  if (flag & PROP_NEVER_NULL) {
    names.append("NEVER_NONE");
  }
  if (flag & (PROP_SKIP_SAVE | PROP_HIDDEN | PROP_SKIP_PRESET)) {
    names.append("SKIP_PRESET");
  }
  if (flag & PROP_SKIP_SAVE) {
    names.append("SKIP_SAVE");
  }
  if (names.is_empty()) {
    return "-";
  }
  std::string out;
  for (const int i : names.index_range()) {
    out += (i ? "|" : "");
    out += names[i];
  }
  return out;
}

/** `wmOperatorType::flag` con los identificadores de `rna_enum_operator_type_flag_items`. */
std::string fmt_optype_flags(const wmOperatorType *ot)
{
  Vector<std::string> names;
  for (const EnumPropertyItem *item = rna_enum_operator_type_flag_items;
       item != nullptr && item->identifier;
       item++)
  {
    if (item->value != 0 && (ot->flag & item->value) == item->value) {
      names.append(item->identifier);
    }
  }
  std::sort(names.begin(), names.end());
  if (names.is_empty()) {
    return "-";
  }
  std::string out;
  for (const int i : names.index_range()) {
    out += (i ? "|" : "");
    out += names[i];
  }
  return out;
}

/* `%.9g` sobre el double promovido desde float, igual que `"{:.9g}".format()` en Python. */
std::string fmt_float(const float value)
{
  char buf[64];
  BLI_snprintf(buf, sizeof(buf), "%.9g", double(value));
  return buf;
}

const char *enum_id_or_empty(const EnumPropertyItem *items, const int value)
{
  const char *id = "";
  /* Si el valor no esta en la tabla, Python devuelve la cadena vacia (le pasa, por
   * ejemplo, a PROP_ANGLE, que no es un item de `rna_enum_property_subtype_items`). */
  RNA_enum_id_from_value(items, value, &id);
  return id;
}

void dump_property(FILE *f, int n, PointerRNA *ptr, PropertyRNA *prop, const bool with_flags)
{
  const PropertyType type = RNA_property_type(prop);
  const int array_len = RNA_property_array_length(ptr, prop);

  std::string line = std::string("  ") + std::to_string(n) +
                     " prop=" + RNA_property_identifier(prop) +
                     " type=" + enum_id_or_empty(rna_enum_property_type_items, int(type)) +
                     " subtype=" +
                     enum_id_or_empty(rna_enum_property_subtype_items,
                                      int(RNA_property_subtype(prop))) +
                     " array=" + std::to_string(array_len) +
                     " name=" + RNA_property_ui_name(prop) +
                     " desc=" + RNA_property_ui_description(prop);
  if (with_flags) {
    line += " flags=" + fmt_prop_flags(prop);
  }

  switch (type) {
    case PROP_BOOLEAN: {
      line += " default=";
      if (array_len > 0) {
        Array<bool> values(array_len);
        RNA_property_boolean_get_default_array(ptr, prop, values.data());
        for (const int i : values.index_range()) {
          line += (i ? "," : "");
          line += values[i] ? "1" : "0";
        }
      }
      else {
        line += RNA_property_boolean_get_default(ptr, prop) ? "1" : "0";
      }
      break;
    }
    case PROP_INT: {
      line += " default=";
      if (array_len > 0) {
        Array<int> values(array_len);
        RNA_property_int_get_default_array(ptr, prop, values.data());
        for (const int i : values.index_range()) {
          line += (i ? "," : "") + std::to_string(values[i]);
        }
      }
      else {
        line += std::to_string(RNA_property_int_get_default(ptr, prop));
      }
      int hardmin, hardmax, softmin, softmax, step;
      RNA_property_int_range(ptr, prop, &hardmin, &hardmax);
      RNA_property_int_ui_range(ptr, prop, &softmin, &softmax, &step);
      line += " min=" + std::to_string(hardmin) + " max=" + std::to_string(hardmax) +
              " softmin=" + std::to_string(softmin) + " softmax=" + std::to_string(softmax);
      break;
    }
    case PROP_FLOAT: {
      line += " default=";
      if (array_len > 0) {
        Array<float> values(array_len);
        RNA_property_float_get_default_array(ptr, prop, values.data());
        for (const int i : values.index_range()) {
          line += (i ? "," : "") + fmt_float(values[i]);
        }
      }
      else {
        line += fmt_float(RNA_property_float_get_default(ptr, prop));
      }
      float hardmin, hardmax, softmin, softmax, step, precision;
      RNA_property_float_range(ptr, prop, &hardmin, &hardmax);
      RNA_property_float_ui_range(ptr, prop, &softmin, &softmax, &step, &precision);
      line += " min=" + fmt_float(hardmin) + " max=" + fmt_float(hardmax) +
              " softmin=" + fmt_float(softmin) + " softmax=" + fmt_float(softmax);
      break;
    }
    case PROP_ENUM: {
      const bool enum_flag = (RNA_property_flag(prop) & PROP_ENUM_FLAG) != 0;
      line += std::string(" enumflag=") + (enum_flag ? "1" : "0");
      const EnumPropertyItem *items = nullptr;
      bool free_items = false;
      RNA_property_enum_items(nullptr, ptr, prop, &items, nullptr, &free_items);
      const int default_value = RNA_property_enum_get_default(ptr, prop);

      line += " default=";
      if (enum_flag) {
        /* Python entrega `default_flag` como un conjunto; se ordena para que el texto
         * sea estable. */
        Vector<std::string> ids;
        for (const EnumPropertyItem *item = items; item != nullptr && item->identifier;
             item++) {
          if (item->value != 0 && (default_value & item->value) == item->value) {
            ids.append(item->identifier);
          }
        }
        std::sort(ids.begin(), ids.end());
        for (const int i : ids.index_range()) {
          line += (i ? "|" : "") + ids[i];
        }
      }
      else {
        line += enum_id_or_empty(items, default_value);
      }

      line += " items=";
      bool first = true;
      for (const EnumPropertyItem *item = items; item != nullptr && item->identifier; item++) {
        if (!first) {
          line += ";";
        }
        first = false;
        line += std::string(item->identifier) + ":" + (item->name ? item->name : "") + ":" +
                (item->description ? item->description : "") + ":" + std::to_string(item->value);
      }
      if (free_items) {
        MEM_freeN(items);
      }
      break;
    }
    case PROP_STRING: {
      char *value = RNA_property_string_get_default_alloc(ptr, prop, nullptr, 0, nullptr);
      line += std::string(" default=") + (value ? value : "");
      if (value) {
        MEM_freeN(value);
      }
      line += " maxlen=" + std::to_string(RNA_property_string_maxlength(prop));
      break;
    }
    case PROP_POINTER:
    case PROP_COLLECTION: {
      if (with_flags) {
        /* El tipo de los elementos. Lo pide la doctrina y caza un fallo silencioso:
         * `RNA_def_collection()` con el tipo por nombre NO funciona en ejecucion y deja
         * la coleccion sin tipo, o sea sin poder recorrerse. Sin esta linea el volcado
         * daba "identico" con la coleccion rota. */
        StructRNA *item_srna = RNA_property_pointer_type(ptr, prop);
        line += std::string(" srna=") +
                (item_srna ? RNA_struct_identifier(item_srna) : "");
      }
      break;
    }
    default:
      break;
  }

  fprintf(f, "%s\n", line.c_str());
}

/**
 * El volcado. `with_flags` anade `options=` al operador y `flags=` a cada propiedad;
 * lo pide la lista de presets y NO la lista del carril C, cuya linea base esta
 * congelada contra un binario irrepetible.
 */
bool dump_list(const char *filepath,
               const char *header,
               const blender::Span<const char *> idnames,
               const bool with_flags)
{
  FILE *f = fopen(filepath, "w");
  if (f == nullptr) {
    fprintf(stderr, "fl-dump-optypes: no se pudo escribir '%s'\n", filepath);
    return false;
  }
  fprintf(f, "# %s\n", header);

  int index = 0;
  for (const char *idname : idnames) {
    fprintf(f, "case=%d op=%s\n", index++, idname);
    wmOperatorType *ot = WM_operatortype_find(idname, true);
    if (ot == nullptr) {
      fprintf(f, "  0 MISSING\n");
      continue;
    }

    PointerRNA ptr = RNA_pointer_create_discrete(nullptr, ot->srna, nullptr);
    int n = 0;
    std::string head = std::string("  ") + std::to_string(n++) +
                       " name=" + RNA_struct_ui_name(ot->srna) +
                       " desc=" + RNA_struct_ui_description(ot->srna) +
                       " translation_context=" + RNA_struct_translation_context(ot->srna);
    if (with_flags) {
      head += " options=" + fmt_optype_flags(ot);
    }
    fprintf(f, "%s\n", head.c_str());

    RNA_STRUCT_BEGIN (&ptr, prop) {
      if (STREQ(RNA_property_identifier(prop), "rna_type")) {
        continue;
      }
      dump_property(f, n++, &ptr, prop, with_flags);
    }
    RNA_STRUCT_END;
  }

  fclose(f);
  fprintf(stderr, "fl-dump-optypes: volcado en '%s' (%d operadores)\n", filepath, index);
  return true;
}

bool check_list(const char *label,
                const char *baseline_path,
                const char *header,
                const blender::Span<const char *> idnames,
                const bool with_flags)
{
  char actual_path[FILE_MAX];
  BLI_path_join(actual_path, sizeof(actual_path), BKE_tempdir_session(), "fl-optype-surface-actual.txt");
  if (!dump_list(actual_path, header, idnames, with_flags)) {
    return false;
  }
  return flipendo::selftest::compare_to_baseline(label, actual_path, baseline_path);
}

}  // namespace

bool dump(bContext * /*C*/, const char *filepath)
{
  return dump_list(filepath,
                   "FL-OPTYPE-SURFACE v1",
                   Span<const char *>(tracked_idnames, ARRAY_SIZE(tracked_idnames)),
                   false);
}

bool check(bContext * /*C*/, const char *baseline_path)
{
  return check_list("fl-check-optypes",
                    baseline_path,
                    "FL-OPTYPE-SURFACE v1",
                    Span<const char *>(tracked_idnames, ARRAY_SIZE(tracked_idnames)),
                    false);
}

bool dump_presets(bContext * /*C*/, const char *filepath)
{
  return dump_list(filepath,
                   "FL-PRESET-OPTYPE-SURFACE v1",
                   Span<const char *>(preset_idnames, ARRAY_SIZE(preset_idnames)),
                   true);
}

bool check_presets(bContext * /*C*/, const char *baseline_path)
{
  return check_list("fl-check-preset-optypes",
                    baseline_path,
                    "FL-PRESET-OPTYPE-SURFACE v1",
                    Span<const char *>(preset_idnames, ARRAY_SIZE(preset_idnames)),
                    true);
}

bool dump_lod(bContext * /*C*/, const char *filepath)
{
  return dump_list(filepath,
                   "FL-LOD-OPTYPE-SURFACE v1",
                   Span<const char *>(lod_idnames, ARRAY_SIZE(lod_idnames)),
                   true);
}

bool check_lod(bContext * /*C*/, const char *baseline_path)
{
  return check_list("fl-check-lod-optypes",
                    baseline_path,
                    "FL-LOD-OPTYPE-SURFACE v1",
                    Span<const char *>(lod_idnames, ARRAY_SIZE(lod_idnames)),
                    true);
}

}  // namespace flipendo::optype_surface
