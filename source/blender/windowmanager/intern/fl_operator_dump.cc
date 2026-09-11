/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Volcado determinista del contrato RNA de los operadores. Ver
 * FL_operator_dump.hpp.
 */

#include "FL_operator_dump.hpp"

#include <algorithm>
#include <cfloat>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_world_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_timer.h"

#include "BKE_context.hh"
#include "BKE_main.hh"
#include "BKE_screen.hh"
#include "BKE_world.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"
#include "RNA_types.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "wm_event_system.hh"

namespace {

struct NamedFlag {
  uint32_t value;
  const char *name;
};

static const NamedFlag operator_flags[] = {
    {OPTYPE_BLOCKING, "BLOCKING"},
    {OPTYPE_DEPENDS_ON_CURSOR, "DEPENDS_ON_CURSOR"},
    {OPTYPE_GRAB_CURSOR_X, "GRAB_CURSOR_X"},
    {OPTYPE_GRAB_CURSOR_XY, "GRAB_CURSOR_XY"},
    {OPTYPE_GRAB_CURSOR_Y, "GRAB_CURSOR_Y"},
    {OPTYPE_INTERNAL, "INTERNAL"},
    {OPTYPE_LOCK_BYPASS, "LOCK_BYPASS"},
    {OPTYPE_MACRO, "MACRO"},
    {OPTYPE_MODAL_PRIORITY, "MODAL_PRIORITY"},
    {OPTYPE_PRESET, "PRESET"},
    {OPTYPE_REGISTER, "REGISTER"},
    {OPTYPE_UNDO, "UNDO"},
    {OPTYPE_UNDO_GROUPED, "UNDO_GROUPED"},
};

static const NamedFlag property_flags[] = {
    {PROP_ANIMATABLE, "ANIMATABLE"},
    {PROP_CONTEXT_UPDATE, "CONTEXT_UPDATE"},
    {PROP_CONTEXT_PROPERTY_UPDATE, "CONTEXT_PROPERTY_UPDATE"},
    {PROP_DEG_SYNC_ONLY, "DEG_SYNC_ONLY"},
    {PROP_DYNAMIC, "DYNAMIC"},
    {PROP_EDITABLE, "EDITABLE"},
    {PROP_ENUM_FLAG, "ENUM_FLAG"},
    {PROP_ENUM_NO_CONTEXT, "ENUM_NO_CONTEXT"},
    {PROP_ENUM_NO_TRANSLATE, "ENUM_NO_TRANSLATE"},
    {PROP_HIDDEN, "HIDDEN"},
    {PROP_ICONS_CONSECUTIVE, "ICONS_CONSECUTIVE"},
    {PROP_ICONS_REVERSE, "ICONS_REVERSE"},
    {PROP_IDPROPERTY, "IDPROPERTY"},
    {PROP_ID_REFCOUNT, "ID_REFCOUNT"},
    {PROP_ID_SELF_CHECK, "ID_SELF_CHECK"},
    {PROP_LIB_EXCEPTION, "LIB_EXCEPTION"},
    {PROP_NEVER_NULL, "NEVER_NULL"},
    {PROP_NEVER_UNLINK, "NEVER_UNLINK"},
    {PROP_NO_DEG_UPDATE, "NO_DEG_UPDATE"},
    {PROP_PATH_OUTPUT, "PATH_OUTPUT"},
    {PROP_PATH_SUPPORTS_BLEND_RELATIVE, "PATH_SUPPORTS_BLEND_RELATIVE"},
    {PROP_PATH_SUPPORTS_TEMPLATES, "PATH_SUPPORTS_TEMPLATES"},
    {PROP_PROPORTIONAL, "PROPORTIONAL"},
    {PROP_PTR_NO_OWNERSHIP, "PTR_NO_OWNERSHIP"},
    {PROP_REGISTER, "REGISTER"},
    {PROP_REGISTER_OPTIONAL, "REGISTER_OPTIONAL"},
    {PROP_SKIP_PRESET, "SKIP_PRESET"},
    {PROP_SKIP_SAVE, "SKIP_SAVE"},
    {PROP_TEXTEDIT_UPDATE, "TEXTEDIT_UPDATE"},
    {PROP_THICK_WRAP, "THICK_WRAP"},
};

std::string quote(const char *value)
{
  std::string out = "\"";
  for (const unsigned char c : std::string(value ? value : "")) {
    switch (c) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (c < 32 || c >= 127) {
          char buf[8];
          std::snprintf(buf, sizeof(buf), "\\x%02x", unsigned(c));
          out += buf;
        }
        else {
          out += char(c);
        }
        break;
    }
  }
  out += '"';
  return out;
}

std::string flags_to_string(const uint32_t flags, const NamedFlag *items, const int items_num)
{
  std::vector<const NamedFlag *> enabled;
  uint32_t known = 0;
  for (int i = 0; i < items_num; i++) {
    /* REGISTER_OPTIONAL includes REGISTER. Only name the exact composite when
     * both bits are present; the numeric value below still preserves every bit. */
    if ((flags & items[i].value) == items[i].value) {
      enabled.push_back(&items[i]);
      known |= items[i].value;
    }
  }
  std::sort(enabled.begin(), enabled.end(), [](const NamedFlag *a, const NamedFlag *b) {
    return std::strcmp(a->name, b->name) < 0;
  });

  std::string out;
  for (const NamedFlag *item : enabled) {
    if (!out.empty()) {
      out += '|';
    }
    out += item->name;
  }
  if (out.empty()) {
    out = "NONE";
  }
  if (const uint32_t unknown = flags & ~known) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%sUNKNOWN_0x%x", out == "NONE" ? "" : "|", unknown);
    if (out == "NONE") {
      out.clear();
    }
    out += buf;
  }
  return out;
}

const char *property_type_name(const PropertyType type)
{
  switch (type) {
    case PROP_BOOLEAN:
      return "BOOLEAN";
    case PROP_INT:
      return "INT";
    case PROP_FLOAT:
      return "FLOAT";
    case PROP_STRING:
      return "STRING";
    case PROP_ENUM:
      return "ENUM";
    case PROP_POINTER:
      return "POINTER";
    case PROP_COLLECTION:
      return "COLLECTION";
  }
  return "UNKNOWN";
}

std::string callbacks_to_string(const wmOperatorType &ot)
{
  std::vector<const char *> callbacks;
#define CALLBACK_IF_SET(member) \
  if (ot.member != nullptr) { \
    callbacks.push_back(#member); \
  }
  CALLBACK_IF_SET(cancel);
  CALLBACK_IF_SET(check);
  CALLBACK_IF_SET(depends_on_cursor);
  CALLBACK_IF_SET(exec);
  CALLBACK_IF_SET(get_description);
  CALLBACK_IF_SET(get_name);
  CALLBACK_IF_SET(invoke);
  CALLBACK_IF_SET(modal);
  CALLBACK_IF_SET(poll);
  CALLBACK_IF_SET(poll_property);
  CALLBACK_IF_SET(ui);
  CALLBACK_IF_SET(ui_poll);
#undef CALLBACK_IF_SET
  std::sort(callbacks.begin(), callbacks.end(), [](const char *a, const char *b) {
    return std::strcmp(a, b) < 0;
  });
  std::string out;
  for (const char *callback : callbacks) {
    if (!out.empty()) {
      out += '|';
    }
    out += callback;
  }
  return out.empty() ? "NONE" : out;
}

template<typename T> std::string number_array_to_string(const std::vector<T> &values)
{
  std::string out = "[";
  char buf[64];
  for (size_t i = 0; i < values.size(); i++) {
    if (i != 0) {
      out += ',';
    }
    if constexpr (std::is_same_v<T, float>) {
      std::snprintf(buf, sizeof(buf), "%.9g", double(values[i]));
    }
    else {
      std::snprintf(buf, sizeof(buf), "%d", int(values[i]));
    }
    out += buf;
  }
  out += ']';
  return out;
}

struct EnumItemDump {
  std::string line;
};

void write_property(FILE *fp,
                    bContext *C,
                    PointerRNA *ptr,
                    PropertyRNA *prop,
                    const std::string &path,
                    std::vector<const StructRNA *> &ancestors)
{
  const char *identifier = RNA_property_identifier(prop);
  const PropertyType type = RNA_property_type(prop);
  const int flags = RNA_property_flag(prop);
  const int array_len = RNA_property_array_length(ptr, prop);

  std::vector<std::pair<std::string, std::string>> fields = {
      {"array_length", std::to_string(array_len)},
      {"description", quote(RNA_property_ui_description_raw(prop))},
      {"flags",
       flags_to_string(flags, property_flags, int(ARRAY_SIZE(property_flags))) + " (" +
           std::to_string(uint32_t(flags)) + ")"},
      {"identifier", quote(identifier)},
      {"name", quote(RNA_property_ui_name_raw(prop))},
      {"subtype", std::to_string(int(RNA_property_subtype(prop)))},
      {"translation_context", quote(RNA_property_translation_context(prop))},
      {"type", std::string(property_type_name(type)) + " (" + std::to_string(int(type)) + ")"},
  };
  std::vector<EnumItemDump> enum_items;
  std::vector<PropertyRNA *> nested_properties;
  PointerRNA nested_ptr = {};
  StructRNA *fixed_type = nullptr;
  bool recursive_reference = false;
  char number[64];

  switch (type) {
    case PROP_BOOLEAN: {
      if (array_len != 0) {
        std::vector<int> defaults(size_t(array_len), 0);
        for (int i = 0; i < array_len; i++) {
          defaults[size_t(i)] = RNA_property_boolean_get_default_index(ptr, prop, i) ? 1 : 0;
        }
        fields.emplace_back("default", number_array_to_string(defaults));
      }
      else {
        fields.emplace_back("default",
                            RNA_property_boolean_get_default(ptr, prop) ? "true" : "false");
      }
      break;
    }
    case PROP_INT: {
      int hard_min, hard_max, soft_min, soft_max, step;
      RNA_property_int_range(ptr, prop, &hard_min, &hard_max);
      RNA_property_int_ui_range(ptr, prop, &soft_min, &soft_max, &step);
      if (array_len != 0) {
        std::vector<int> defaults(size_t(array_len), 0);
        RNA_property_int_get_default_array(ptr, prop, defaults.data());
        fields.emplace_back("default", number_array_to_string(defaults));
      }
      else {
        fields.emplace_back("default", std::to_string(RNA_property_int_get_default(ptr, prop)));
      }
      fields.emplace_back("hard_max", std::to_string(hard_max));
      fields.emplace_back("hard_min", std::to_string(hard_min));
      fields.emplace_back("soft_max", std::to_string(soft_max));
      fields.emplace_back("soft_min", std::to_string(soft_min));
      fields.emplace_back("step", std::to_string(step));
      break;
    }
    case PROP_FLOAT: {
      float hard_min, hard_max, soft_min, soft_max, step, precision;
      RNA_property_float_range(ptr, prop, &hard_min, &hard_max);
      RNA_property_float_ui_range(ptr, prop, &soft_min, &soft_max, &step, &precision);
      if (array_len != 0) {
        std::vector<float> defaults(size_t(array_len), 0.0f);
        RNA_property_float_get_default_array(ptr, prop, defaults.data());
        fields.emplace_back("default", number_array_to_string(defaults));
      }
      else {
        std::snprintf(
            number, sizeof(number), "%.9g", double(RNA_property_float_get_default(ptr, prop)));
        fields.emplace_back("default", number);
      }
#define ADD_FLOAT_FIELD(key, value) \
  std::snprintf(number, sizeof(number), "%.9g", double(value)); \
  fields.emplace_back(key, number)
      ADD_FLOAT_FIELD("hard_max", hard_max);
      ADD_FLOAT_FIELD("hard_min", hard_min);
      ADD_FLOAT_FIELD("precision", precision);
      ADD_FLOAT_FIELD("soft_max", soft_max);
      ADD_FLOAT_FIELD("soft_min", soft_min);
      ADD_FLOAT_FIELD("step", step);
#undef ADD_FLOAT_FIELD
      break;
    }
    case PROP_STRING: {
      char *value = RNA_property_string_get_default_alloc(ptr, prop, nullptr, 0, nullptr);
      fields.emplace_back("default", quote(value));
      fields.emplace_back("max_length", std::to_string(RNA_property_string_maxlength(prop)));
      MEM_SAFE_FREE(value);
      break;
    }
    case PROP_ENUM: {
      fields.emplace_back("default", std::to_string(RNA_property_enum_get_default(ptr, prop)));
      const EnumPropertyItem *items = nullptr;
      int items_num = 0;
      bool free_items = false;
      /* Los callbacks dinamicos necesitan contextos distintos segun el operador
       * (nodos, assets, modos de pintura...). El contrato estable es su lista
       * estatica; invocarlos todos con un unico contexto produciria excepciones y
       * haria que el volcado dependiera del editor que estuviera activo. */
      RNA_property_enum_items_ex(C, ptr, prop, true, &items, &items_num, &free_items);
      enum_items.reserve(size_t(items_num));
      for (int i = 0; i < items_num; i++) {
        const EnumPropertyItem &item = items[i];
        std::string line = "      ITEM " + std::to_string(i) +
                           " description=" + quote(item.description) +
                           " icon=" + std::to_string(item.icon) +
                           " identifier=" + quote(item.identifier) + " name=" + quote(item.name) +
                           " value=" + std::to_string(item.value);
        enum_items.push_back({std::move(line)});
      }
      fields.emplace_back("enum_items", std::to_string(items_num));
      if (free_items) {
        MEM_freeN(items);
      }
      break;
    }
    case PROP_POINTER:
    case PROP_COLLECTION: {
      fixed_type = RNA_property_pointer_type(ptr, prop);
      fields.emplace_back("default", "null");
      fields.emplace_back(
          "fixed_type", quote(fixed_type ? RNA_struct_identifier(fixed_type) : ""));

      /* Las macros guardan las propiedades de sus sub-operadores en punteros a
       * OperatorProperties. Esos contratos tambien son parte del operador y se
       * recorren; los punteros de datos normales solo declaran su tipo fijo. */
      if (fixed_type != nullptr && RNA_struct_is_a(fixed_type, &RNA_OperatorProperties)) {
        nested_ptr = RNA_pointer_create_discrete(ptr->owner_id, fixed_type, nullptr);
        if (type == PROP_POINTER) {
          const PointerRNA value_ptr = RNA_property_pointer_get(ptr, prop);
          if (value_ptr.type != nullptr) {
            nested_ptr = value_ptr;
          }
        }

        RNA_STRUCT_BEGIN_SKIP_RNA_TYPE (&nested_ptr, nested_prop) {
          nested_properties.push_back(nested_prop);
        }
        RNA_STRUCT_END;
        std::sort(nested_properties.begin(),
                  nested_properties.end(),
                  [](const PropertyRNA *a, const PropertyRNA *b) {
                    return std::strcmp(RNA_property_identifier(a), RNA_property_identifier(b)) < 0;
                  });
        fields.emplace_back("nested_properties", std::to_string(nested_properties.size()));

        if (std::find(ancestors.begin(), ancestors.end(), fixed_type) != ancestors.end()) {
          recursive_reference = true;
          fields.emplace_back("recursive_reference", "true");
        }
      }
      break;
    }
  }

  std::sort(fields.begin(), fields.end(), [](const auto &a, const auto &b) {
    return a.first < b.first;
  });
  std::fprintf(fp, "  PROPERTY %s\n", path.c_str());
  for (const auto &[key, value] : fields) {
    std::fprintf(fp, "    %s=%s\n", key.c_str(), value.c_str());
  }
  for (const EnumItemDump &item : enum_items) {
    std::fprintf(fp, "%s\n", item.line.c_str());
  }

  if (!nested_properties.empty() && !recursive_reference) {
    ancestors.push_back(fixed_type);
    for (PropertyRNA *nested_prop : nested_properties) {
      write_property(fp,
                     C,
                     &nested_ptr,
                     nested_prop,
                     path + "." + RNA_property_identifier(nested_prop),
                     ancestors);
    }
    ancestors.pop_back();
  }
}

void write_operator(FILE *fp, bContext *C, wmOperatorType *ot)
{
  PointerRNA *ptr = nullptr;
  IDProperty *id_properties = nullptr;
  WM_operator_properties_alloc(&ptr, &id_properties, ot->idname);
  WM_operator_properties_default(ptr, false);

  std::vector<PropertyRNA *> properties;
  RNA_STRUCT_BEGIN_SKIP_RNA_TYPE (ptr, prop) {
    properties.push_back(prop);
  }
  RNA_STRUCT_END;
  std::sort(properties.begin(), properties.end(), [](const PropertyRNA *a, const PropertyRNA *b) {
    return std::strcmp(RNA_property_identifier(a), RNA_property_identifier(b)) < 0;
  });

  std::fprintf(fp, "OPERATOR %s\n", ot->idname);
  std::fprintf(fp, "  callbacks=%s\n", callbacks_to_string(*ot).c_str());
  std::fprintf(fp, "  description=%s\n", quote(ot->description).c_str());
  std::fprintf(fp,
               "  flags=%s (%d)\n",
               flags_to_string(
                   ot->flag, operator_flags, int(ARRAY_SIZE(operator_flags))).c_str(),
               int(ot->flag));
  std::fprintf(fp, "  idname=%s\n", quote(ot->idname).c_str());
  std::fprintf(fp, "  name=%s\n", quote(ot->name).c_str());
  std::fprintf(fp, "  properties=%zu\n", properties.size());
  std::fprintf(fp, "  translation_context=%s\n", quote(ot->translation_context).c_str());
  std::vector<const StructRNA *> ancestors = {ot->srna};
  for (PropertyRNA *prop : properties) {
    write_property(fp, C, ptr, prop, RNA_property_identifier(prop), ancestors);
  }

  WM_operator_properties_free(ptr);
  MEM_delete(ptr);
}

}  // namespace

bool FL_operators_dump(bContext *C, const char *filepath)
{
  FILE *fp = std::fopen(filepath, "w");
  if (fp == nullptr) {
    std::fprintf(stderr, "FL_operators_dump: no se pudo escribir %s\n", filepath);
    return false;
  }

  std::vector<wmOperatorType *> operators(WM_operatortypes_registered_get().begin(),
                                           WM_operatortypes_registered_get().end());
  std::sort(operators.begin(), operators.end(), [](const wmOperatorType *a, const wmOperatorType *b) {
    return std::strcmp(a->idname, b->idname) < 0;
  });

  std::fprintf(fp, "OPERATORS %zu\n", operators.size());
  for (wmOperatorType *ot : operators) {
    write_operator(fp, C, ot);
  }

  const bool ok = std::fclose(fp) == 0;
  if (ok) {
    std::printf("Operadores volcados: %zu tipos -> %s\n", operators.size(), filepath);
  }
  else {
    std::fprintf(stderr, "FL_operators_dump: error al cerrar %s\n", filepath);
  }
  return ok;
}

namespace {

class OperatorProperties {
 public:
  explicit OperatorProperties(const char *idname)
  {
    WM_operator_properties_alloc(&ptr_, &properties_, idname);
  }

  ~OperatorProperties()
  {
    WM_operator_properties_free(ptr_);
    MEM_delete(ptr_);
  }

  PointerRNA *ptr() const
  {
    return ptr_;
  }

 private:
  PointerRNA *ptr_ = nullptr;
  IDProperty *properties_ = nullptr;
};

wmEventHandler_Op *find_modal_operator(wmWindow *win, const char *idname)
{
  LISTBASE_FOREACH (wmEventHandler *, handler_base, &win->modalhandlers) {
    if (handler_base->type == WM_HANDLER_TYPE_OP) {
      wmEventHandler_Op *handler = reinterpret_cast<wmEventHandler_Op *>(handler_base);
      if (handler->op != nullptr && STREQ(handler->op->type->idname, idname)) {
        return handler;
      }
    }
  }
  return nullptr;
}

ARegion *visible_header_region(ScrArea *area)
{
  ARegion *header = nullptr;
  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    if (region->runtime == nullptr || !region->runtime->visible) {
      continue;
    }
    if (region->regiontype == RGN_TYPE_HEADER) {
      header = region;
    }
    else if (region->regiontype == RGN_TYPE_TOOL_HEADER) {
      return region;
    }
  }
  return header;
}

bool selftest_set_view3d_context(bContext *C)
{
  wmWindow *win = CTX_wm_window(C);
  if (win == nullptr) {
    return false;
  }
  bScreen *screen = WM_window_get_active_screen(win);
  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    if (area->spacetype != SPACE_VIEW3D) {
      continue;
    }
    CTX_wm_area_set(C, area);
    LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
      if (region->regiontype == RGN_TYPE_WINDOW) {
        CTX_wm_region_set(C, region);
        return true;
      }
    }
  }
  return false;
}

}  // namespace

bool FL_context_operators_selftest(bContext *C, const char *filepath)
{
  if (!selftest_set_view3d_context(C)) {
    std::fprintf(stderr, "FL_context_operators_selftest: no hay una vista 3D activa\n");
    return false;
  }

  Object *object = CTX_data_active_object(C);
  Scene *scene = CTX_data_scene(C);
  wmWindow *win = CTX_wm_window(C);
  ScrArea *area = CTX_wm_area(C);
  Main *bmain = CTX_data_main(C);
  if (object == nullptr || scene == nullptr || win == nullptr || area == nullptr || bmain == nullptr) {
    std::fprintf(stderr, "FL_context_operators_selftest: falta contexto de escena\n");
    return false;
  }

  FILE *fp = std::fopen(filepath, "w");
  if (fp == nullptr) {
    std::fprintf(stderr, "FL_context_operators_selftest: no se pudo escribir %s\n", filepath);
    return false;
  }

  wmOperatorStatus set_value_status;
  {
    OperatorProperties props("WM_OT_context_set_value");
    RNA_string_set(props.ptr(), "data_path", "tool_settings.mesh_select_mode");
    RNA_string_set(props.ptr(), "value", "(False, True, False)");
    set_value_status = WM_operator_name_call(
        C, "WM_OT_context_set_value", WM_OP_EXEC_DEFAULT, props.ptr(), nullptr);
  }
  std::fprintf(fp,
               "context_set_value status=%d mesh_select_mode=%d\n",
               int(set_value_status),
               int(scene->toolsettings->selectmode));

  scene->cursor.location[0] = 1.0f;
  scene->cursor.location[1] = 2.0f;
  scene->cursor.location[2] = 3.0f;
  wmOperatorStatus cycle_array_status;
  {
    OperatorProperties props("WM_OT_context_cycle_array");
    RNA_string_set(props.ptr(), "data_path", "scene.cursor.location");
    RNA_boolean_set(props.ptr(), "reverse", false);
    cycle_array_status = WM_operator_name_call(
        C, "WM_OT_context_cycle_array", WM_OP_EXEC_DEFAULT, props.ptr(), nullptr);
  }
  std::fprintf(fp,
               "context_cycle_array status=%d cursor=[%.9g,%.9g,%.9g]\n",
               int(cycle_array_status),
               double(scene->cursor.location[0]),
               double(scene->cursor.location[1]),
               double(scene->cursor.location[2]));

  World *world = BKE_world_add(bmain, "FL Operator Test World");
  scene->world = nullptr;
  wmOperatorStatus set_id_status;
  {
    OperatorProperties props("WM_OT_context_set_id");
    RNA_string_set(props.ptr(), "data_path", "scene.world");
    RNA_string_set(props.ptr(), "value", world->id.name + 2);
    set_id_status = WM_operator_name_call(
        C, "WM_OT_context_set_id", WM_OP_EXEC_DEFAULT, props.ptr(), nullptr);
  }
  std::fprintf(fp,
               "context_set_id status=%d world=%s\n",
               int(set_id_status),
               scene->world ? scene->world->id.name + 2 : "<null>");

  object->dtx &= ~OB_DRAWWIRE;
  wmOperatorStatus collection_status;
  {
    OperatorProperties props("WM_OT_context_collection_boolean_set");
    RNA_string_set(props.ptr(), "data_path_iter", "selected_editable_objects");
    RNA_string_set(props.ptr(), "data_path_item", "show_wire");
    RNA_enum_set_identifier(C, props.ptr(), "type", "ENABLE");
    collection_status = WM_operator_name_call(C,
                                              "WM_OT_context_collection_boolean_set",
                                              WM_OP_EXEC_DEFAULT,
                                              props.ptr(),
                                              nullptr);
  }
  std::fprintf(fp,
               "context_collection_boolean_set status=%d show_wire=%d\n",
               int(collection_status),
               (object->dtx & OB_DRAWWIRE) != 0);

  object->empty_drawsize = 2.0f;
  wmOperatorStatus modal_invoke_status;
  {
    OperatorProperties props("WM_OT_context_modal_mouse");
    RNA_string_set(props.ptr(), "data_path_iter", "selected_editable_objects");
    RNA_string_set(props.ptr(), "data_path_item", "empty_display_size");
    RNA_string_set(props.ptr(), "header_text", "Empty Display Size: %.3f");
    RNA_float_set(props.ptr(), "input_scale", 0.01f);
    wmEvent invoke_event = {};
    invoke_event.type = LEFTMOUSE;
    invoke_event.val = KM_PRESS;
    invoke_event.xy[0] = 100;
    modal_invoke_status = WM_operator_name_call(C,
                                                "WM_OT_context_modal_mouse",
                                                WM_OP_INVOKE_DEFAULT,
                                                props.ptr(),
                                                &invoke_event);
  }

  wmOperatorStatus modal_move_status = wmOperatorStatus(0);
  wmOperatorStatus modal_finish_status = wmOperatorStatus(0);
  std::string modal_header;
  if (wmEventHandler_Op *handler = find_modal_operator(win, "WM_OT_context_modal_mouse")) {
    wmEvent move_event = {};
    move_event.type = MOUSEMOVE;
    move_event.xy[0] = 125;
    modal_move_status = handler->op->type->modal(C, handler->op, &move_event);
    if (ARegion *header = visible_header_region(area)) {
      modal_header = header->runtime->headerstr ? header->runtime->headerstr : "";
    }

    wmEvent finish_event = {};
    finish_event.type = LEFTMOUSE;
    finish_event.val = KM_PRESS;
    finish_event.xy[0] = 125;
    modal_finish_status = handler->op->type->modal(C, handler->op, &finish_event);
    WM_event_remove_model_handler(&win->modalhandlers, handler->op, false);
  }
  std::fprintf(fp,
               "context_modal_mouse invoke=%d move=%d finish=%d value=%.9g header=%s\n",
               int(modal_invoke_status),
               int(modal_move_status),
               int(modal_finish_status),
               double(object->empty_drawsize),
               quote(modal_header.c_str()).c_str());

  wmOperatorStatus menu_status;
  {
    OperatorProperties props("WM_OT_context_menu_enum");
    RNA_string_set(props.ptr(), "data_path", "tool_settings.uv_select_mode");
    menu_status = WM_operator_name_call(
        C, "WM_OT_context_menu_enum", WM_OP_EXEC_DEFAULT, props.ptr(), nullptr);
  }
  std::fprintf(fp, "context_menu_enum status=%d\n", int(menu_status));

  wmOperatorStatus pie_status;
  {
    OperatorProperties props("WM_OT_context_pie_enum");
    RNA_string_set(props.ptr(), "data_path", "tool_settings.uv_select_mode");
    wmEvent event = {};
    event.type = EVT_ZKEY;
    event.val = KM_PRESS;
    pie_status = WM_operator_name_call(
        C, "WM_OT_context_pie_enum", WM_OP_INVOKE_DEFAULT, props.ptr(), &event);
  }
  std::fprintf(fp, "context_pie_enum status=%d\n", int(pie_status));

  const bool ok = std::fclose(fp) == 0;
  if (ok) {
    std::printf("Prueba real de context ops: 7 operadores -> %s\n", filepath);
  }
  return ok;
}

namespace {

struct ContextSelftestTimerData {
  bContext *C;
  std::string filepath;
  int attempts = 0;
};

double context_selftest_timer(uintptr_t /*uuid*/, void *user_data)
{
  ContextSelftestTimerData *data = static_cast<ContextSelftestTimerData *>(user_data);
  data->attempts++;

  wmWindow *win = CTX_wm_window(data->C);
  if (win == nullptr || WM_window_get_active_screen(win) == nullptr) {
    if (data->attempts < 100) {
      return 0.05;
    }
    std::fprintf(stderr, "FL_context_operators_selftest: la interfaz no llego a estar lista\n");
    WM_exit(data->C, EXIT_FAILURE);
    return -1.0;
  }

  const bool ok = FL_context_operators_selftest(data->C, data->filepath.c_str());
  WM_exit(data->C, ok ? EXIT_SUCCESS : EXIT_FAILURE);
  return -1.0;
}

void context_selftest_timer_free(uintptr_t /*uuid*/, void *user_data)
{
  delete static_cast<ContextSelftestTimerData *>(user_data);
}

}  // namespace

bool FL_context_operators_selftest_schedule(bContext *C, const char *filepath)
{
  ContextSelftestTimerData *data = new ContextSelftestTimerData{C, filepath};
  BLI_timer_register(uintptr_t(data),
                     context_selftest_timer,
                     data,
                     context_selftest_timer_free,
                     0.05,
                     false);
  return true;
}
