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
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "BKE_appdir.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

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
    "MESH_OT_faces_mirror_uv",
    "MESH_OT_select_next_item",
    "MESH_OT_select_prev_item",
};

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

void dump_property(FILE *f, int n, PointerRNA *ptr, PropertyRNA *prop)
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
    default:
      break;
  }

  fprintf(f, "%s\n", line.c_str());
}

}  // namespace

bool dump(bContext * /*C*/, const char *filepath)
{
  FILE *f = fopen(filepath, "w");
  if (f == nullptr) {
    fprintf(stderr, "fl-dump-optypes: no se pudo escribir '%s'\n", filepath);
    return false;
  }
  fprintf(f, "# FL-OPTYPE-SURFACE v1\n");

  int index = 0;
  for (const char *idname : tracked_idnames) {
    fprintf(f, "case=%d op=%s\n", index++, idname);
    wmOperatorType *ot = WM_operatortype_find(idname, true);
    if (ot == nullptr) {
      fprintf(f, "  0 MISSING\n");
      continue;
    }

    PointerRNA ptr = RNA_pointer_create_discrete(nullptr, ot->srna, nullptr);
    int n = 0;
    fprintf(f,
            "  %d name=%s desc=%s translation_context=%s\n",
            n++,
            RNA_struct_ui_name(ot->srna),
            RNA_struct_ui_description(ot->srna),
            RNA_struct_translation_context(ot->srna));

    RNA_STRUCT_BEGIN (&ptr, prop) {
      if (STREQ(RNA_property_identifier(prop), "rna_type")) {
        continue;
      }
      dump_property(f, n++, &ptr, prop);
    }
    RNA_STRUCT_END;
  }

  fclose(f);
  fprintf(stderr, "fl-dump-optypes: volcado en '%s' (%d operadores)\n", filepath, index);
  return true;
}

bool check(bContext *C, const char *baseline_path)
{
  char actual_path[FILE_MAX];
  BLI_path_join(
      actual_path, sizeof(actual_path), BKE_tempdir_session(), "fl-optype-surface-actual.txt");
  if (!dump(C, actual_path)) {
    return false;
  }
  return flipendo::selftest::compare_to_baseline("fl-check-optypes", actual_path, baseline_path);
}

}  // namespace flipendo::optype_surface
