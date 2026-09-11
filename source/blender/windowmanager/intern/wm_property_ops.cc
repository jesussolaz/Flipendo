/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Operadores sencillos de propiedades personalizadas que antes vivian en
 * `scripts/startup/bl_operators/wm.py`. Las rutas de contexto se resuelven por
 * RNA y los valores se manipulan como IDProperty, sin `eval()` ni Python.
 */

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"

#include "BLI_listbase.h"
#include "BLI_expr_pylike_eval.h"
#include "BLI_string.h"
#include "BLI_timer.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_animsys.h"
#include "BKE_idprop.hh"
#include "BKE_report.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "FL_operator_dump.hpp"

namespace {

std::string first_component(const char *path)
{
  const char *end = path;
  while (*end != '\0' && *end != '.' && *end != '[') {
    end++;
  }
  return std::string(path, size_t(end - path));
}

bool context_data_pointer_resolve(bContext *C, const char *data_path, PointerRNA *r_ptr)
{
  if (data_path == nullptr || data_path[0] == '\0') {
    return false;
  }

  PointerRNA context_ptr = RNA_pointer_create_discrete(nullptr, &RNA_Context, C);
  if (RNA_path_resolve(&context_ptr, data_path, r_ptr, nullptr)) {
    return r_ptr->data != nullptr;
  }

  const std::string member = first_component(data_path);
  if (member.empty()) {
    return false;
  }
  PointerRNA member_ptr = CTX_data_pointer_get(C, member.c_str());
  if (member_ptr.data == nullptr) {
    return false;
  }

  const char *rest = data_path + member.size();
  if (*rest == '.') {
    rest++;
  }
  if (*rest == '\0') {
    *r_ptr = member_ptr;
    return true;
  }
  return RNA_path_resolve(&member_ptr, rest, r_ptr, nullptr) && r_ptr->data != nullptr;
}

bool is_reference_override(const PointerRNA &ptr)
{
  const ID *id = ptr.owner_id;
  return id != nullptr && id->override_library != nullptr &&
         id->override_library->reference != nullptr;
}

void idproperty_update(bContext *C, PointerRNA *ptr, const char *name)
{
  const std::string path = "[\"" + BLI_str_escape(name) + "\"]";
  PointerRNA property_ptr;
  PropertyRNA *property = nullptr;
  if (RNA_path_resolve_property(ptr, path.c_str(), &property_ptr, &property)) {
    RNA_property_update(C, &property_ptr, property);
  }
}

void def_data_path(wmOperatorType *ot)
{
  PropertyRNA *prop = RNA_def_string(
      ot->srna, "data_path", nullptr, 1025, "Property Edit", "Property data_path edit");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

void def_property_name(wmOperatorType *ot)
{
  RNA_def_string(
      ot->srna, "property_name", nullptr, 64, "Property Name", "Property name edit");
}

enum CustomPropertyType {
  PROP_TYPE_FLOAT = 0,
  PROP_TYPE_FLOAT_ARRAY,
  PROP_TYPE_INT,
  PROP_TYPE_INT_ARRAY,
  PROP_TYPE_BOOL,
  PROP_TYPE_BOOL_ARRAY,
  PROP_TYPE_STRING,
  PROP_TYPE_DATA_BLOCK,
  PROP_TYPE_EXPRESSION,
};

static const EnumPropertyItem custom_property_type_items[] = {
    {PROP_TYPE_FLOAT, "FLOAT", 0, "Float", "A single floating-point value"},
    {PROP_TYPE_FLOAT_ARRAY, "FLOAT_ARRAY", 0, "Float Array", "An array of floating-point values"},
    {PROP_TYPE_INT, "INT", 0, "Integer", "A single integer"},
    {PROP_TYPE_INT_ARRAY, "INT_ARRAY", 0, "Integer Array", "An array of integers"},
    {PROP_TYPE_BOOL, "BOOL", 0, "Boolean", "A true or false value"},
    {PROP_TYPE_BOOL_ARRAY, "BOOL_ARRAY", 0, "Boolean Array", "An array of true or false values"},
    {PROP_TYPE_STRING, "STRING", 0, "String", "A string value"},
    {PROP_TYPE_DATA_BLOCK, "DATA_BLOCK", 0, "Data-Block", "A data-block value"},
    {PROP_TYPE_EXPRESSION,
     "PYTHON",
     0,
     "Python",
     "Edit a Python value directly, for unsupported property types"},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem subtype_none_items[] = {
    {PROP_NONE, "NONE", 0, N_("Plain Data"), N_("Data values without special behavior")},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem subtype_number_items[] = {
    {PROP_NONE, "NONE", 0, N_("Plain Data"), N_("Data values without special behavior")},
    {PROP_PIXEL, "PIXEL", 0, N_("Pixel"), N_("A distance on screen")},
    {PROP_PERCENTAGE, "PERCENTAGE", 0, N_("Percentage"), N_("A percentage between 0 and 100")},
    {PROP_FACTOR, "FACTOR", 0, N_("Factor"), N_("A factor between 0.0 and 1.0")},
    {PROP_ANGLE, "ANGLE", 0, N_("Angle"), N_("A rotational value specified in radians")},
    {PROP_TIME_ABSOLUTE, "TIME_ABSOLUTE", 0, N_("Time"), N_("Time specified in seconds")},
    {PROP_DISTANCE, "DISTANCE", 0, N_("Distance"), N_("A distance between two points")},
    {PROP_POWER, "POWER", 0, N_("Power"), ""},
    {PROP_TEMPERATURE, "TEMPERATURE", 0, N_("Temperature"), ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem subtype_vector_items[] = {
    {PROP_NONE, "NONE", 0, N_("Plain Data"), N_("Data values without special behavior")},
    {PROP_COLOR, "COLOR", 0, N_("Linear Color"), N_("Color in the linear space")},
    {PROP_COLOR_GAMMA, "COLOR_GAMMA", 0, N_("Gamma-Corrected Color"), N_("Color in the gamma corrected space")},
    {PROP_TRANSLATION, "TRANSLATION", 0, N_("Translation"), ""},
    {PROP_DIRECTION, "DIRECTION", 0, N_("Direction"), ""},
    {PROP_VELOCITY, "VELOCITY", 0, N_("Velocity"), ""},
    {PROP_ACCELERATION, "ACCELERATION", 0, N_("Acceleration"), ""},
    {PROP_EULER, "EULER", 0, N_("Euler Angles"), N_("Euler rotation angles in radians")},
    {PROP_QUATERNION, "QUATERNION", 0, N_("Quaternion Rotation"), N_("Quaternion rotation (affects NLA blending)")},
    {PROP_AXISANGLE, "AXISANGLE", 0, N_("Axis-Angle"), N_("Angle and axis to rotate around")},
    {PROP_XYZ, "XYZ", 0, N_("XYZ"), ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem *property_subtype_itemf(bContext * /*C*/,
                                                       PointerRNA *ptr,
                                                       PropertyRNA * /*prop*/,
                                                       bool *r_free)
{
  *r_free = false;
  switch (RNA_enum_get(ptr, "property_type")) {
    case PROP_TYPE_FLOAT:
      return subtype_number_items;
    case PROP_TYPE_FLOAT_ARRAY:
      return subtype_vector_items;
    default:
      return subtype_none_items;
  }
}

static void property_type_update(bContext * /*C*/, PointerRNA *ptr, PropertyRNA * /*prop*/)
{
  RNA_enum_set(ptr, "subtype", PROP_NONE);
}

int custom_property_type(const IDProperty *prop)
{
  if (prop == nullptr) {
    return PROP_TYPE_EXPRESSION;
  }
  switch (prop->type) {
    case IDP_INT:
      return PROP_TYPE_INT;
    case IDP_FLOAT:
    case IDP_DOUBLE:
      return PROP_TYPE_FLOAT;
    case IDP_BOOLEAN:
      return PROP_TYPE_BOOL;
    case IDP_STRING:
      return PROP_TYPE_STRING;
    case IDP_ID:
      return PROP_TYPE_DATA_BLOCK;
    case IDP_ARRAY:
      if (prop->subtype == IDP_INT) {
        return PROP_TYPE_INT_ARRAY;
      }
      if (ELEM(prop->subtype, IDP_FLOAT, IDP_DOUBLE)) {
        return PROP_TYPE_FLOAT_ARRAY;
      }
      if (prop->subtype == IDP_BOOLEAN) {
        return PROP_TYPE_BOOL_ARRAY;
      }
      break;
  }
  return PROP_TYPE_EXPRESSION;
}

double property_number_at(const IDProperty *prop, const int index = 0)
{
  if (prop->type == IDP_ARRAY && prop->len > 0) {
    if (prop->subtype == IDP_INT) {
      return static_cast<const int *>(IDP_Array(prop))[std::min(index, prop->len - 1)];
    }
    if (prop->subtype == IDP_FLOAT) {
      return static_cast<const float *>(IDP_Array(prop))[std::min(index, prop->len - 1)];
    }
    if (prop->subtype == IDP_DOUBLE) {
      return static_cast<const double *>(IDP_Array(prop))[std::min(index, prop->len - 1)];
    }
    if (prop->subtype == IDP_BOOLEAN) {
      return static_cast<const int8_t *>(IDP_Array(prop))[std::min(index, prop->len - 1)];
    }
  }
  if (prop->type == IDP_INT) {
    return IDP_Int(prop);
  }
  if (prop->type == IDP_FLOAT) {
    return IDP_Float(prop);
  }
  if (prop->type == IDP_DOUBLE) {
    return IDP_Double(prop);
  }
  if (prop->type == IDP_BOOLEAN) {
    return IDP_Bool(prop);
  }
  return 0.0;
}

bool eval_number(const char *text, double *r_value)
{
  ExprPyLike_Parsed *expr = BLI_expr_pylike_parse(text, nullptr, 0);
  if (!BLI_expr_pylike_is_valid(expr)) {
    BLI_expr_pylike_free(expr);
    return false;
  }
  const eExprPyLike_EvalStatus status = BLI_expr_pylike_eval(expr, nullptr, 0, r_value);
  BLI_expr_pylike_free(expr);
  return status == EXPR_PYLIKE_SUCCESS;
}

std::string trim(std::string value)
{
  const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) { return std::isspace(c); });
  const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) { return std::isspace(c); }).base();
  return first < last ? std::string(first, last) : std::string();
}

IDProperty *property_from_native_expression(const char *name, const char *expression)
{
  const std::string value = trim(expression ? expression : "");
  if (value == "True") {
    return blender::bke::idprop::create_bool(name, true).release();
  }
  if (value == "False") {
    return blender::bke::idprop::create_bool(name, false).release();
  }
  if (value == "None") {
    return blender::bke::idprop::create(name, static_cast<ID *>(nullptr)).release();
  }
  if (value == "{}") {
    return blender::bke::idprop::create_group(name).release();
  }
  if (value.size() >= 2 && ELEM(value.front(), '\'', '"') && value.back() == value.front()) {
    const std::string literal = value.substr(1, value.size() - 2);
    return blender::bke::idprop::create(name, blender::StringRefNull(literal)).release();
  }
  if (value.size() >= 2 && value.front() == '[' && value.back() == ']') {
    std::vector<double> numbers;
    size_t begin = 1;
    for (size_t i = 1; i <= value.size() - 1; i++) {
      if (i == value.size() - 1 || value[i] == ',') {
        const std::string element = trim(value.substr(begin, i - begin));
        if (!element.empty()) {
          double number;
          if (!eval_number(element.c_str(), &number)) {
            return nullptr;
          }
          numbers.push_back(number);
        }
        begin = i + 1;
      }
    }
    return blender::bke::idprop::create(name, blender::Span(numbers)).release();
  }
  double number;
  if (eval_number(value.c_str(), &number)) {
    return blender::bke::idprop::create(name, number).release();
  }
  return nullptr;
}

std::string property_as_expression(const IDProperty *prop)
{
  if (prop == nullptr) {
    return "";
  }
  if (prop->type == IDP_GROUP) {
    return BLI_listbase_is_empty(&prop->data.group) ? "{}" : "<native group>";
  }
  if (prop->type == IDP_IDPARRAY) {
    return prop->len == 0 ? "[]" : "<native property array>";
  }
  if (prop->type == IDP_STRING) {
    return "'" + std::string(IDP_String(prop)) + "'";
  }
  if (prop->type == IDP_BOOLEAN) {
    return IDP_Bool(prop) ? "True" : "False";
  }
  char buffer[64];
  BLI_snprintf(buffer, sizeof(buffer), "%.17g", property_number_at(prop));
  return buffer;
}

IDProperty *converted_property(const IDProperty *old_prop,
                               const char *name,
                               const int type,
                               PointerRNA *op_ptr)
{
  const int length = RNA_int_get(op_ptr, "array_length");
  switch (type) {
    case PROP_TYPE_INT:
      return blender::bke::idprop::create(name, int(property_number_at(old_prop))).release();
    case PROP_TYPE_FLOAT:
      return blender::bke::idprop::create(name, property_number_at(old_prop)).release();
    case PROP_TYPE_BOOL:
      return blender::bke::idprop::create_bool(name, property_number_at(old_prop) != 0.0).release();
    case PROP_TYPE_STRING:
      if (old_prop->type == IDP_STRING) {
        return blender::bke::idprop::create(name, IDP_String(old_prop)).release();
      }
      return blender::bke::idprop::create(name, property_as_expression(old_prop)).release();
    case PROP_TYPE_INT_ARRAY: {
      std::vector<int32_t> values(length);
      for (int i = 0; i < length; i++) {
        values[i] = int(property_number_at(old_prop, i));
      }
      return blender::bke::idprop::create(name, blender::Span(values)).release();
    }
    case PROP_TYPE_FLOAT_ARRAY: {
      std::vector<double> values(length);
      for (int i = 0; i < length; i++) {
        values[i] = property_number_at(old_prop, i);
      }
      return blender::bke::idprop::create(name, blender::Span(values)).release();
    }
    case PROP_TYPE_BOOL_ARRAY: {
      IDPropertyTemplate value{};
      value.array.len = length;
      value.array.type = IDP_BOOLEAN;
      IDProperty *result = IDP_New(IDP_ARRAY, &value, name);
      int8_t *items = static_cast<int8_t *>(IDP_Array(result));
      for (int i = 0; i < length; i++) {
        items[i] = property_number_at(old_prop, i) != 0.0;
      }
      return result;
    }
    case PROP_TYPE_DATA_BLOCK: {
      ID *id = old_prop->type == IDP_ID ? IDP_Id(old_prop) : nullptr;
      if (id != nullptr && GS(id->name) != RNA_enum_get(op_ptr, "id_type")) {
        id = nullptr;
      }
      return blender::bke::idprop::create(name, id).release();
    }
    case PROP_TYPE_EXPRESSION: {
      char *expression = RNA_string_get_alloc(op_ptr, "eval_string", nullptr, 0, nullptr);
      IDProperty *result = property_from_native_expression(name, expression);
      MEM_freeN(expression);
      return result;
    }
  }
  return nullptr;
}

void set_ui_description(IDPropertyUIData *ui_data, const char *description)
{
  MEM_SAFE_FREE(ui_data->description);
  ui_data->description = description[0] ? BLI_strdup(description) : nullptr;
}

void fill_ui_data(IDProperty *prop, PointerRNA *op_ptr, const int type)
{
  if (type == PROP_TYPE_EXPRESSION) {
    return;
  }
  IDPropertyUIData *base = IDP_ui_data_ensure(prop);
  char *description = RNA_string_get_alloc(op_ptr, "description", nullptr, 0, nullptr);
  set_ui_description(base, description);
  MEM_freeN(description);
  base->rna_subtype = RNA_enum_get(op_ptr, "subtype");

  if (ELEM(type, PROP_TYPE_INT, PROP_TYPE_INT_ARRAY)) {
    auto *ui = reinterpret_cast<IDPropertyUIDataInt *>(base);
    ui->min = RNA_int_get(op_ptr, "min_int");
    ui->max = RNA_int_get(op_ptr, "max_int");
    ui->soft_min = RNA_boolean_get(op_ptr, "use_soft_limits") ? RNA_int_get(op_ptr, "soft_min_int") : ui->min;
    ui->soft_max = RNA_boolean_get(op_ptr, "use_soft_limits") ? RNA_int_get(op_ptr, "soft_max_int") : ui->max;
    ui->step = RNA_int_get(op_ptr, "step_int");
    int defaults[32];
    RNA_int_get_array(op_ptr, "default_int", defaults);
    if (type == PROP_TYPE_INT) {
      ui->default_value = defaults[0];
    }
    else {
      ui->default_array_len = prop->len;
      ui->default_array = static_cast<int *>(MEM_malloc_arrayN(prop->len, sizeof(int), __func__));
      memcpy(ui->default_array, defaults, sizeof(int) * prop->len);
    }
  }
  else if (ELEM(type, PROP_TYPE_FLOAT, PROP_TYPE_FLOAT_ARRAY)) {
    auto *ui = reinterpret_cast<IDPropertyUIDataFloat *>(base);
    ui->min = RNA_float_get(op_ptr, "min_float");
    ui->max = RNA_float_get(op_ptr, "max_float");
    ui->soft_min = RNA_boolean_get(op_ptr, "use_soft_limits") ? RNA_float_get(op_ptr, "soft_min_float") : ui->min;
    ui->soft_max = RNA_boolean_get(op_ptr, "use_soft_limits") ? RNA_float_get(op_ptr, "soft_max_float") : ui->max;
    ui->step = RNA_float_get(op_ptr, "step_float");
    ui->precision = RNA_int_get(op_ptr, "precision");
    float defaults[32];
    RNA_float_get_array(op_ptr, "default_float", defaults);
    if (type == PROP_TYPE_FLOAT) {
      ui->default_value = defaults[0];
    }
    else {
      ui->default_array_len = prop->len;
      ui->default_array = static_cast<double *>(MEM_malloc_arrayN(prop->len, sizeof(double), __func__));
      for (int i = 0; i < prop->len; i++) {
        ui->default_array[i] = defaults[i];
      }
    }
  }
  else if (ELEM(type, PROP_TYPE_BOOL, PROP_TYPE_BOOL_ARRAY)) {
    auto *ui = reinterpret_cast<IDPropertyUIDataBool *>(base);
    bool defaults[32];
    RNA_boolean_get_array(op_ptr, "default_bool", defaults);
    if (type == PROP_TYPE_BOOL) {
      ui->default_value = defaults[0];
    }
    else {
      ui->default_array_len = prop->len;
      ui->default_array = static_cast<int8_t *>(MEM_malloc_arrayN(prop->len, sizeof(int8_t), __func__));
      for (int i = 0; i < prop->len; i++) {
        ui->default_array[i] = defaults[i];
      }
    }
  }
  else if (type == PROP_TYPE_STRING) {
    auto *ui = reinterpret_cast<IDPropertyUIDataString *>(base);
    char *default_value = RNA_string_get_alloc(op_ptr, "default_string", nullptr, 0, nullptr);
    ui->default_value = BLI_strdup(default_value);
    MEM_freeN(default_value);
  }
  else if (type == PROP_TYPE_DATA_BLOCK) {
    reinterpret_cast<IDPropertyUIDataID *>(base)->id_type = RNA_enum_get(op_ptr, "id_type");
  }
}

wmOperatorStatus properties_edit_exec(bContext *C, wmOperator *op)
{
  IDProperty *old_name_prop = IDP_GetPropertyFromGroup(op->properties, "_old_prop_name");
  if (old_name_prop == nullptr || old_name_prop->type != IDP_STRING) {
    BKE_report(op->reports, RPT_ERROR, "Direct execution not supported");
    return OPERATOR_CANCELLED;
  }
  const std::string old_name = IDP_String(old_name_prop);
  char data_path[1025], name[64];
  RNA_string_get(op->ptr, "data_path", data_path);
  RNA_string_get(op->ptr, "property_name", name);
  PointerRNA item;
  if (!context_data_pointer_resolve(C, data_path, &item) || is_reference_override(item)) {
    BKE_report(op->reports, RPT_ERROR, "Cannot edit properties from override data");
    return OPERATOR_CANCELLED;
  }
  IDProperty *group = RNA_struct_idprops(&item, false);
  IDProperty *old_prop = group ? IDP_GetPropertyFromGroup(group, old_name) : nullptr;
  if (old_prop == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Property could not be found");
    return OPERATOR_CANCELLED;
  }
  const int type = RNA_enum_get(op->ptr, "property_type");
  IDProperty *new_prop = converted_property(old_prop, name, type, op->ptr);
  if (new_prop == nullptr) {
    BKE_report(op->reports,
               RPT_WARNING,
               "Native expression rejected; use arithmetic, booleans, None, strings or numeric arrays");
    return OPERATOR_CANCELLED;
  }
  fill_ui_data(new_prop, op->ptr, type);
  if (RNA_boolean_get(op->ptr, "is_overridable_library")) {
    new_prop->flag |= IDP_FLAG_OVERRIDABLE_LIBRARY;
  }
  if (old_name == name) {
    IDP_ReplaceInGroup_ex(group, new_prop, old_prop, 0);
  }
  else {
    IDP_FreeFromGroup(group, old_prop);
    IDP_AddToGroup(group, new_prop);
  }
  idproperty_update(C, &item, name);
  if (old_name != name && item.owner_id != nullptr) {
    BKE_animdata_fix_paths_rename_all_ex(
        CTX_data_main(C), item.owner_id, "", old_name.c_str(), name, 0, 0, true);
  }
  IDP_AssignString(old_name_prop, name);
  return OPERATOR_FINISHED;
}

void property_edit_fill(PointerRNA *op_ptr, IDProperty *prop)
{
  const int type = custom_property_type(prop);
  RNA_enum_set(op_ptr, "property_type", type);
  RNA_int_set(op_ptr, "array_length", prop->type == IDP_ARRAY ? std::clamp(prop->len, 1, 32) : 3);
  RNA_string_set(op_ptr, "eval_string", property_as_expression(prop).c_str());
  RNA_boolean_set(op_ptr, "is_overridable_library", prop->flag & IDP_FLAG_OVERRIDABLE_LIBRARY);
  if (prop->ui_data == nullptr || type == PROP_TYPE_EXPRESSION) {
    return;
  }
  IDPropertyUIData *base = prop->ui_data;
  RNA_string_set(op_ptr, "description", base->description ? base->description : "");
  RNA_enum_set(op_ptr, "subtype", base->rna_subtype);
  if (ELEM(type, PROP_TYPE_INT, PROP_TYPE_INT_ARRAY)) {
    auto *ui = reinterpret_cast<IDPropertyUIDataInt *>(base);
    RNA_int_set(op_ptr, "min_int", ui->min);
    RNA_int_set(op_ptr, "max_int", ui->max);
    RNA_int_set(op_ptr, "soft_min_int", ui->soft_min);
    RNA_int_set(op_ptr, "soft_max_int", ui->soft_max);
    RNA_int_set(op_ptr, "step_int", ui->step);
    RNA_boolean_set(op_ptr, "use_soft_limits", ui->min != ui->soft_min || ui->max != ui->soft_max);
  }
  else if (ELEM(type, PROP_TYPE_FLOAT, PROP_TYPE_FLOAT_ARRAY)) {
    auto *ui = reinterpret_cast<IDPropertyUIDataFloat *>(base);
    RNA_float_set(op_ptr, "min_float", ui->min);
    RNA_float_set(op_ptr, "max_float", ui->max);
    RNA_float_set(op_ptr, "soft_min_float", ui->soft_min);
    RNA_float_set(op_ptr, "soft_max_float", ui->soft_max);
    RNA_float_set(op_ptr, "step_float", ui->step);
    RNA_int_set(op_ptr, "precision", ui->precision);
    RNA_boolean_set(op_ptr, "use_soft_limits", ui->min != ui->soft_min || ui->max != ui->soft_max);
  }
  else if (type == PROP_TYPE_DATA_BLOCK) {
    RNA_enum_set(op_ptr, "id_type", reinterpret_cast<IDPropertyUIDataID *>(base)->id_type);
  }
}

wmOperatorStatus properties_edit_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  char data_path[1025], name[64];
  RNA_string_get(op->ptr, "data_path", data_path);
  RNA_string_get(op->ptr, "property_name", name);
  if (data_path[0] == '\0') {
    BKE_report(op->reports, RPT_ERROR, "Data path not set");
    return OPERATOR_CANCELLED;
  }
  PointerRNA item;
  if (!context_data_pointer_resolve(C, data_path, &item) || is_reference_override(item)) {
    BKE_report(op->reports, RPT_ERROR, "Properties from override data cannot be edited");
    return OPERATOR_CANCELLED;
  }
  IDProperty *group = RNA_struct_idprops(&item, false);
  IDProperty *prop = group ? IDP_GetPropertyFromGroup(group, name) : nullptr;
  if (prop == nullptr) {
    return OPERATOR_CANCELLED;
  }
  IDProperty *old_name_prop = IDP_NewString(name, "_old_prop_name");
  IDP_ReplaceInGroup(op->properties, old_name_prop);
  property_edit_fill(op->ptr, prop);
  return WM_operator_props_dialog_popup(C, op, 400);
}

bool properties_edit_check(bContext * /*C*/, wmOperator *op)
{
  bool changed = false;
  const int type = RNA_enum_get(op->ptr, "property_type");
  if (ELEM(type, PROP_TYPE_INT, PROP_TYPE_INT_ARRAY)) {
    int min = RNA_int_get(op->ptr, "min_int"), max = RNA_int_get(op->ptr, "max_int");
    if (min > max) {
      RNA_int_set(op->ptr, "min_int", max);
      RNA_int_set(op->ptr, "max_int", min);
      changed = true;
    }
  }
  else if (ELEM(type, PROP_TYPE_FLOAT, PROP_TYPE_FLOAT_ARRAY)) {
    float min = RNA_float_get(op->ptr, "min_float"), max = RNA_float_get(op->ptr, "max_float");
    if (min > max) {
      RNA_float_set(op->ptr, "min_float", max);
      RNA_float_set(op->ptr, "max_float", min);
      changed = true;
    }
  }
  return changed;
}

void properties_edit_draw(bContext * /*C*/, wmOperator *op)
{
  uiLayout *layout = op->layout;
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  layout->prop(op->ptr, "property_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->prop(op->ptr, "property_name", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  const int type = RNA_enum_get(op->ptr, "property_type");
  if (ELEM(type, PROP_TYPE_FLOAT_ARRAY, PROP_TYPE_INT_ARRAY, PROP_TYPE_BOOL_ARRAY)) {
    layout->prop(op->ptr, "array_length", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  if (ELEM(type, PROP_TYPE_FLOAT, PROP_TYPE_FLOAT_ARRAY)) {
    layout->prop(op->ptr, "default_float", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    layout->prop(op->ptr, "min_float", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    layout->prop(op->ptr, "max_float", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    layout->prop(op->ptr, "use_soft_limits", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    layout->prop(op->ptr, "soft_min_float", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    layout->prop(op->ptr, "soft_max_float", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    layout->prop(op->ptr, "step_float", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    layout->prop(op->ptr, "precision", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    layout->prop(op->ptr, "subtype", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  else if (ELEM(type, PROP_TYPE_INT, PROP_TYPE_INT_ARRAY)) {
    layout->prop(op->ptr, "default_int", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    layout->prop(op->ptr, "min_int", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    layout->prop(op->ptr, "max_int", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    layout->prop(op->ptr, "use_soft_limits", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    layout->prop(op->ptr, "soft_min_int", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    layout->prop(op->ptr, "soft_max_int", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    layout->prop(op->ptr, "step_int", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  else if (ELEM(type, PROP_TYPE_BOOL, PROP_TYPE_BOOL_ARRAY)) {
    layout->prop(op->ptr, "default_bool", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  else if (type == PROP_TYPE_STRING) {
    layout->prop(op->ptr, "default_string", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  else if (type == PROP_TYPE_DATA_BLOCK) {
    layout->prop(op->ptr, "id_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  layout->prop(op->ptr,
               type == PROP_TYPE_EXPRESSION ? "eval_string" : "description",
               UI_ITEM_NONE,
               std::nullopt,
               ICON_NONE);
  layout->prop(op->ptr, "is_overridable_library", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

wmOperatorStatus properties_edit_value_exec(bContext *C, wmOperator *op)
{
  char *expression = RNA_string_get_alloc(op->ptr, "eval_string", nullptr, 0, nullptr);
  if (expression[0] == '\0') {
    MEM_freeN(expression);
    return OPERATOR_FINISHED;
  }
  char data_path[1025], name[64];
  RNA_string_get(op->ptr, "data_path", data_path);
  RNA_string_get(op->ptr, "property_name", name);
  PointerRNA item;
  if (!context_data_pointer_resolve(C, data_path, &item)) {
    MEM_freeN(expression);
    return OPERATOR_CANCELLED;
  }
  IDProperty *group = RNA_struct_idprops(&item, false);
  IDProperty *old_prop = group ? IDP_GetPropertyFromGroup(group, name) : nullptr;
  IDProperty *new_prop = property_from_native_expression(name, expression);
  MEM_freeN(expression);
  if (old_prop == nullptr || new_prop == nullptr) {
    BKE_report(op->reports, RPT_WARNING, "Native expression evaluation failed");
    return OPERATOR_CANCELLED;
  }
  IDP_ReplaceInGroup_ex(group, new_prop, old_prop, 0);
  idproperty_update(C, &item, name);
  return OPERATOR_FINISHED;
}

wmOperatorStatus properties_edit_value_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  char data_path[1025], name[64];
  RNA_string_get(op->ptr, "data_path", data_path);
  RNA_string_get(op->ptr, "property_name", name);
  PointerRNA item;
  if (!context_data_pointer_resolve(C, data_path, &item)) {
    return OPERATOR_CANCELLED;
  }
  IDProperty *group = RNA_struct_idprops(&item, false);
  IDProperty *prop = group ? IDP_GetPropertyFromGroup(group, name) : nullptr;
  if (custom_property_type(prop) == PROP_TYPE_EXPRESSION) {
    RNA_string_set(op->ptr, "eval_string", property_as_expression(prop).c_str());
  }
  else {
    RNA_string_set(op->ptr, "eval_string", "");
  }
  return WM_operator_props_dialog_popup(C, op, 400);
}

void properties_edit_value_draw(bContext *C, wmOperator *op)
{
  char data_path[1025], name[64];
  RNA_string_get(op->ptr, "data_path", data_path);
  RNA_string_get(op->ptr, "property_name", name);
  PointerRNA item;
  IDProperty *prop = nullptr;
  if (context_data_pointer_resolve(C, data_path, &item)) {
    IDProperty *group = RNA_struct_idprops(&item, false);
    prop = group ? IDP_GetPropertyFromGroup(group, name) : nullptr;
  }
  if (custom_property_type(prop) == PROP_TYPE_EXPRESSION) {
    op->layout->prop(op->ptr, "eval_string", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  else if (prop != nullptr) {
    const std::string path = "[\"" + BLI_str_escape(name) + "\"]";
    op->layout->prop(&item, path.c_str(), UI_ITEM_NONE, "", ICON_NONE);
  }
}

wmOperatorStatus properties_add_exec(bContext *C, wmOperator *op)
{
  char data_path[1025];
  RNA_string_get(op->ptr, "data_path", data_path);

  PointerRNA item;
  if (!context_data_pointer_resolve(C, data_path, &item)) {
    BKE_report(op->reports, RPT_ERROR, "Data path could not be resolved");
    return OPERATOR_CANCELLED;
  }
  if (is_reference_override(item)) {
    BKE_report(op->reports, RPT_ERROR, "Cannot add properties to override data");
    return OPERATOR_CANCELLED;
  }

  IDProperty *group = RNA_struct_idprops(&item, true);
  if (group == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Data does not support custom properties");
    return OPERATOR_CANCELLED;
  }

  std::string name = "prop";
  for (int suffix = 1;
       IDP_GetPropertyFromGroup(group, name.c_str()) != nullptr ||
       RNA_struct_find_property(&item, name.c_str()) != nullptr;
       suffix++)
  {
    name = "prop" + std::to_string(suffix);
  }

  IDProperty *property = blender::bke::idprop::create(name, 1.0).release();
  IDP_AddToGroup(group, property);
  idproperty_update(C, &item, name.c_str());

  auto *ui_data = reinterpret_cast<IDPropertyUIDataFloat *>(IDP_ui_data_ensure(property));
  ui_data->min = 0.0;
  ui_data->max = 1.0;
  ui_data->soft_min = 0.0;
  ui_data->soft_max = 1.0;
  ui_data->step = 0.1f;
  ui_data->precision = 3;
  ui_data->default_value = 1.0;

  return OPERATOR_FINISHED;
}

wmOperatorStatus properties_remove_exec(bContext *C, wmOperator *op)
{
  char data_path[1025];
  char property_name[64];
  RNA_string_get(op->ptr, "data_path", data_path);
  RNA_string_get(op->ptr, "property_name", property_name);

  PointerRNA item;
  if (!context_data_pointer_resolve(C, data_path, &item)) {
    BKE_report(op->reports, RPT_ERROR, "Data path could not be resolved");
    return OPERATOR_CANCELLED;
  }
  if (is_reference_override(item)) {
    BKE_report(op->reports, RPT_ERROR, "Cannot remove properties from override data");
    return OPERATOR_CANCELLED;
  }

  idproperty_update(C, &item, property_name);
  if (!RNA_struct_idprops_unset(&item, property_name)) {
    BKE_report(op->reports, RPT_ERROR, "Property could not be found");
    return OPERATOR_CANCELLED;
  }
  return OPERATOR_FINISHED;
}

wmOperatorStatus properties_context_change_exec(bContext *C, wmOperator *op)
{
  SpaceProperties *space = CTX_wm_space_properties(C);
  if (space == nullptr) {
    return OPERATOR_CANCELLED;
  }

  char context[65];
  RNA_string_get(op->ptr, "context", context);
  PointerRNA space_ptr = RNA_pointer_create_discrete(
      reinterpret_cast<ID *>(CTX_wm_screen(C)), &RNA_SpaceProperties, space);
  RNA_enum_set_identifier(C, &space_ptr, "context", context);
  return OPERATOR_FINISHED;
}

const char *status_string(const wmOperatorStatus status)
{
  return status == OPERATOR_FINISHED ? "['FINISHED']" : "['CANCELLED']";
}

}  // namespace

void WM_OT_properties_add(wmOperatorType *ot)
{
  ot->name = "Add Property";
  ot->idname = "WM_OT_properties_add";
  ot->description = "Add your own property to the data-block";
  ot->exec = properties_add_exec;
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  def_data_path(ot);
}

void WM_OT_properties_context_change(wmOperatorType *ot)
{
  ot->name = "";
  ot->idname = "WM_OT_properties_context_change";
  ot->description = "Jump to a different tab inside the properties editor";
  ot->exec = properties_context_change_exec;
  ot->flag = OPTYPE_INTERNAL;

  RNA_def_string(ot->srna, "context", nullptr, 65, "Context", "");
}

void WM_OT_properties_remove(wmOperatorType *ot)
{
  ot->name = "Remove Property";
  ot->idname = "WM_OT_properties_remove";
  ot->description = "Internal use (edit a property data_path)";
  ot->exec = properties_remove_exec;
  ot->flag = OPTYPE_UNDO | OPTYPE_INTERNAL;

  def_data_path(ot);
  def_property_name(ot);
}

void WM_OT_properties_edit(wmOperatorType *ot)
{
  ot->name = "Edit Property";
  ot->idname = "WM_OT_properties_edit";
  ot->description = "Change a custom property's type, or adjust how it is displayed in the interface";
  ot->invoke = properties_edit_invoke;
  ot->exec = properties_edit_exec;
  ot->check = properties_edit_check;
  ot->ui = properties_edit_draw;
  ot->flag = OPTYPE_REGISTER | OPTYPE_INTERNAL;

  def_data_path(ot);
  def_property_name(ot);
  PropertyRNA *prop = RNA_def_enum(
      ot->srna, "property_type", custom_property_type_items, PROP_TYPE_FLOAT, "Type", "");
  RNA_def_property_update_runtime_with_context_and_property(prop, property_type_update);
  RNA_def_boolean(ot->srna,
                  "is_overridable_library",
                  false,
                  "Library Overridable",
                  "Allow the property to be overridden when the data-block is linked");
  RNA_def_string(ot->srna, "description", nullptr, 0, "Description", "");
  RNA_def_boolean(ot->srna,
                  "use_soft_limits",
                  false,
                  "Soft Limits",
                  "Limits the Property Value slider to a range, values outside the range must be inputted numerically");
  RNA_def_int(ot->srna, "array_length", 3, 1, 32, "Array Length", "", 1, 32);
  RNA_def_int_array(ot->srna,
                     "default_int",
                     32,
                     nullptr,
                     INT_MIN,
                     INT_MAX,
                     "Default Value",
                     "",
                     INT_MIN,
                     INT_MAX);
  RNA_def_int(ot->srna, "min_int", -10000, INT_MIN, INT_MAX, "Min", "", INT_MIN, INT_MAX);
  RNA_def_int(ot->srna, "max_int", 10000, INT_MIN, INT_MAX, "Max", "", INT_MIN, INT_MAX);
  RNA_def_int(
      ot->srna, "soft_min_int", -10000, INT_MIN, INT_MAX, "Soft Min", "", INT_MIN, INT_MAX);
  RNA_def_int(
      ot->srna, "soft_max_int", 10000, INT_MIN, INT_MAX, "Soft Max", "", INT_MIN, INT_MAX);
  RNA_def_int(ot->srna, "step_int", 1, 1, INT_MAX, "Step", "", 1, INT_MAX);
  RNA_def_boolean_array(ot->srna, "default_bool", 32, nullptr, "Default Value", "");
  prop = RNA_def_float_array(ot->srna,
                       "default_float",
                       32,
                       nullptr,
                       -FLT_MAX,
                       FLT_MAX,
                       "Default Value",
                       "",
                       -FLT_MAX,
                       FLT_MAX);
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 3, 2);
  prop = RNA_def_float(
      ot->srna, "min_float", -10000.0f, -FLT_MAX, FLT_MAX, "Min", "", -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 3, 2);
  prop = RNA_def_float(
      ot->srna, "max_float", -10000.0f, -FLT_MAX, FLT_MAX, "Max", "", -FLT_MAX, FLT_MAX);
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 3, 2);
  prop = RNA_def_float(ot->srna,
                "soft_min_float",
                -10000.0f,
                -FLT_MAX,
                FLT_MAX,
                "Soft Min",
                "",
                -FLT_MAX,
                FLT_MAX);
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 3, 2);
  prop = RNA_def_float(ot->srna,
                "soft_max_float",
                -10000.0f,
                -FLT_MAX,
                FLT_MAX,
                "Soft Max",
                "",
                -FLT_MAX,
                FLT_MAX);
  RNA_def_property_ui_range(prop, -FLT_MAX, FLT_MAX, 3, 2);
  RNA_def_int(ot->srna, "precision", 3, 0, 8, "Precision", "", 0, 8);
  prop = RNA_def_float(
      ot->srna, "step_float", 0.1f, 0.001f, FLT_MAX, "Step", "", 0.001f, FLT_MAX);
  RNA_def_property_ui_range(prop, 0.001f, FLT_MAX, 3, 2);
  prop = RNA_def_enum(ot->srna, "subtype", rna_enum_dummy_NULL_items, PROP_NONE, "Subtype", "");
  RNA_def_property_enum_funcs_runtime(prop, nullptr, nullptr, property_subtype_itemf);
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_UNIT);
  RNA_def_string(ot->srna, "default_string", nullptr, 1025, "Default Value", "");
  prop = RNA_def_enum(ot->srna, "id_type", rna_enum_id_type_items, ID_OB, "ID Type", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_ID);
  RNA_def_string(ot->srna,
                 "eval_string",
                 nullptr,
                 0,
                 "Value",
                 "Python value for unsupported custom property types");
}

void WM_OT_properties_edit_value(wmOperatorType *ot)
{
  ot->name = "Edit Property Value";
  ot->idname = "WM_OT_properties_edit_value";
  ot->description = "Edit the value of a custom property";
  ot->invoke = properties_edit_value_invoke;
  ot->exec = properties_edit_value_exec;
  ot->ui = properties_edit_value_draw;
  ot->flag = OPTYPE_REGISTER | OPTYPE_INTERNAL;

  def_data_path(ot);
  def_property_name(ot);
  RNA_def_string(
      ot->srna,
      "eval_string",
      nullptr,
      0,
      "Value",
      "Value for custom property types that can only be edited as a Python expression");
}

bool FL_wm_properties_edit_selftest(bContext *C, const char *filepath)
{
  PointerRNA item = CTX_data_pointer_get(C, "object");
  IDProperty *group = item.data ? RNA_struct_idprops(&item, true) : nullptr;
  if (group == nullptr) {
    return false;
  }
  if (IDProperty *existing = IDP_GetPropertyFromGroup(group, "editable")) {
    IDP_FreeFromGroup(group, existing);
  }
  if (IDProperty *existing = IDP_GetPropertyFromGroup(group, "renamed")) {
    IDP_FreeFromGroup(group, existing);
  }
  IDP_AddToGroup(group, blender::bke::idprop::create("editable", 3.75).release());

  PointerRNA props;
  WM_operator_properties_create(&props, "WM_OT_properties_edit");
  RNA_string_set(&props, "data_path", "object");
  RNA_string_set(&props, "property_name", "renamed");
  RNA_enum_set_identifier(C, &props, "property_type", "INT_ARRAY");
  RNA_int_set(&props, "array_length", 3);
  IDProperty *props_group = RNA_struct_idprops(&props, true);
  IDP_AddToGroup(props_group, IDP_NewString("editable", "_old_prop_name"));
  const wmOperatorStatus convert_status = WM_operator_name_call(
      C, "WM_OT_properties_edit", WM_OP_EXEC_DEFAULT, &props, nullptr);
  WM_operator_properties_free(&props);

  IDProperty *renamed = IDP_GetPropertyFromGroup(group, "renamed");
  int values[3] = {-1, -1, -1};
  if (renamed && renamed->type == IDP_ARRAY && renamed->subtype == IDP_INT && renamed->len == 3) {
    memcpy(values, IDP_Array(renamed), sizeof(values));
  }

  IDP_ReplaceInGroup(group, blender::bke::idprop::create_group("expression").release());
  WM_operator_properties_create(&props, "WM_OT_properties_edit_value");
  RNA_string_set(&props, "data_path", "object");
  RNA_string_set(&props, "property_name", "expression");
  RNA_string_set(&props, "eval_string", "2**3 + 5%2 + 9//2");
  const wmOperatorStatus expression_status = WM_operator_name_call(
      C, "WM_OT_properties_edit_value", WM_OP_EXEC_DEFAULT, &props, nullptr);
  WM_operator_properties_free(&props);
  IDProperty *expression = IDP_GetPropertyFromGroup(group, "expression");

  FILE *fp = std::fopen(filepath, "w");
  if (fp == nullptr) {
    return false;
  }
  fprintf(fp,
          "convert status=%s old=%s type=%d len=%d values=[%d,%d,%d]\n",
          status_string(convert_status),
          IDP_GetPropertyFromGroup(group, "editable") ? "present" : "missing",
          renamed ? int(renamed->type) : -1,
          renamed ? renamed->len : -1,
          values[0],
          values[1],
          values[2]);
  fprintf(fp,
          "expression status=%s type=%d value=%.1f\n",
          status_string(expression_status),
          expression ? int(expression->type) : -1,
          expression ? IDP_Double(expression) : -1.0);
  const bool ok = std::fclose(fp) == 0;

  if (renamed) {
    IDP_FreeFromGroup(group, renamed);
  }
  if (expression) {
    IDP_FreeFromGroup(group, expression);
  }
  return ok;
}

bool FL_wm_property_operators_selftest(bContext *C, const char *filepath)
{
  PointerRNA item = CTX_data_pointer_get(C, "object");
  if (item.data == nullptr) {
    return false;
  }
  IDProperty *group = RNA_struct_idprops(&item, true);
  if (group == nullptr) {
    return false;
  }

  RNA_struct_idprops_unset(&item, "prop");
  RNA_struct_idprops_unset(&item, "prop1");
  IDP_AddToGroup(group, blender::bke::idprop::create("prop", "occupied").release());

  PointerRNA props;
  WM_operator_properties_create(&props, "WM_OT_properties_add");
  RNA_string_set(&props, "data_path", "object");
  const wmOperatorStatus add_status = WM_operator_name_call(
      C, "WM_OT_properties_add", WM_OP_EXEC_DEFAULT, &props, nullptr);
  WM_operator_properties_free(&props);

  IDProperty *added = IDP_GetPropertyFromGroup(group, "prop1");
  const double value = added == nullptr ? 0.0 : IDP_Double(added);

  WM_operator_properties_create(&props, "WM_OT_properties_remove");
  RNA_string_set(&props, "data_path", "object");
  RNA_string_set(&props, "property_name", "prop1");
  const wmOperatorStatus remove_status = WM_operator_name_call(
      C, "WM_OT_properties_remove", WM_OP_EXEC_DEFAULT, &props, nullptr);
  WM_operator_properties_free(&props);
  const bool exists_after_remove = IDP_GetPropertyFromGroup(group, "prop1") != nullptr;

  ScrArea *properties_area = nullptr;
  LISTBASE_FOREACH (ScrArea *, area, &CTX_wm_screen(C)->areabase) {
    if (area->spacetype == SPACE_PROPERTIES) {
      properties_area = area;
      break;
    }
  }
  if (properties_area == nullptr) {
    return false;
  }

  ScrArea *old_area = CTX_wm_area(C);
  CTX_wm_area_set(C, properties_area);
  SpaceProperties *space = CTX_wm_space_properties(C);
  const int old_context = space->mainb;

  WM_operator_properties_create(&props, "WM_OT_properties_context_change");
  RNA_string_set(&props, "context", "WORLD");
  const wmOperatorStatus context_status = WM_operator_name_call(
      C, "WM_OT_properties_context_change", WM_OP_EXEC_DEFAULT, &props, nullptr);
  WM_operator_properties_free(&props);
  const int new_context = space->mainb;
  CTX_wm_area_set(C, old_area);

  PointerRNA space_ptr = RNA_pointer_create_discrete(
      reinterpret_cast<ID *>(CTX_wm_screen(C)), &RNA_SpaceProperties, space);
  PropertyRNA *context_prop = RNA_struct_find_property(&space_ptr, "context");
  const char *old_identifier = nullptr;
  const char *new_identifier = nullptr;
  RNA_property_enum_identifier(C, &space_ptr, context_prop, old_context, &old_identifier);
  RNA_property_enum_identifier(C, &space_ptr, context_prop, new_context, &new_identifier);

  FILE *fp = std::fopen(filepath, "w");
  if (fp == nullptr) {
    return false;
  }
  std::fprintf(fp,
               "add status=%s names=['prop', 'prop1'] value=%.1f\n",
               status_string(add_status),
               value);
  std::fprintf(fp,
               "remove status=%s exists=%s\n",
               status_string(remove_status),
               exists_after_remove ? "True" : "False");
  std::fprintf(fp,
               "context status=%s old=%s requested=WORLD now=%s\n",
               status_string(context_status),
               old_identifier,
               new_identifier);
  const bool ok = std::fclose(fp) == 0;

  RNA_struct_idprops_unset(&item, "prop");
  RNA_struct_idprops_unset(&item, "prop1");
  if (ok) {
    std::printf("Prueba de operadores de propiedades -> %s\n", filepath);
  }
  return ok;
}

namespace {

struct PropertySelftestTimerData {
  bContext *C;
  std::string filepath;
  int attempts = 0;
};

double property_selftest_timer(uintptr_t /*uuid*/, void *user_data)
{
  PropertySelftestTimerData *data = static_cast<PropertySelftestTimerData *>(user_data);
  data->attempts++;

  wmWindow *window = CTX_wm_window(data->C);
  if (window == nullptr || WM_window_get_active_screen(window) == nullptr) {
    if (data->attempts < 100) {
      return 0.05;
    }
    std::fprintf(stderr, "FL_wm_property_operators_selftest: la interfaz no llego a estar lista\n");
    WM_exit(data->C, EXIT_FAILURE);
    return -1.0;
  }

  const bool ok = FL_wm_property_operators_selftest(data->C, data->filepath.c_str());
  WM_exit(data->C, ok ? EXIT_SUCCESS : EXIT_FAILURE);
  return -1.0;
}

void property_selftest_timer_free(uintptr_t /*uuid*/, void *user_data)
{
  delete static_cast<PropertySelftestTimerData *>(user_data);
}

}  // namespace

bool FL_wm_property_operators_selftest_schedule(bContext *C, const char *filepath)
{
  PropertySelftestTimerData *data = new PropertySelftestTimerData{C, filepath};
  BLI_timer_register(uintptr_t(data),
                     property_selftest_timer,
                     data,
                     property_selftest_timer_free,
                     0.05,
                     false);
  return true;
}

namespace {

wmOperatorStatus owner_enable_exec(bContext *C, wmOperator *op)
{
  WorkSpace *workspace = CTX_wm_workspace(C);
  if (workspace == nullptr) {
    return OPERATOR_CANCELLED;
  }

  char *name = RNA_string_get_alloc(op->ptr, "owner_id", nullptr, 0, nullptr);
  wmOwnerID *owner_id = MEM_callocN<wmOwnerID>(__func__);
  STRNCPY(owner_id->name, name);
  MEM_freeN(name);
  BLI_addtail(&workspace->owner_ids, owner_id);
  WM_main_add_notifier(NC_WINDOW, nullptr);
  return OPERATOR_FINISHED;
}

wmOperatorStatus owner_disable_exec(bContext *C, wmOperator *op)
{
  WorkSpace *workspace = CTX_wm_workspace(C);
  if (workspace == nullptr) {
    return OPERATOR_CANCELLED;
  }

  char *name = RNA_string_get_alloc(op->ptr, "owner_id", nullptr, 0, nullptr);
  wmOwnerID *owner_id = static_cast<wmOwnerID *>(
      BLI_findstring(&workspace->owner_ids, name, offsetof(wmOwnerID, name)));
  MEM_freeN(name);
  if (owner_id == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "UI tag is not enabled for this workspace");
    return OPERATOR_CANCELLED;
  }

  BLI_remlink(&workspace->owner_ids, owner_id);
  MEM_freeN(owner_id);
  WM_main_add_notifier(NC_WINDOW, nullptr);
  return OPERATOR_FINISHED;
}

void def_owner_id(wmOperatorType *ot)
{
  RNA_def_string(ot->srna, "owner_id", nullptr, 0, "UI Tag", "");
}

}  // namespace

void WM_OT_owner_enable(wmOperatorType *ot)
{
  ot->name = "Enable Add-on";
  ot->idname = "WM_OT_owner_enable";
  ot->description = "Enable add-on for workspace";
  ot->exec = owner_enable_exec;

  def_owner_id(ot);
}

void WM_OT_owner_disable(wmOperatorType *ot)
{
  ot->name = "Disable Add-on";
  ot->idname = "WM_OT_owner_disable";
  ot->description = "Disable add-on for workspace";
  ot->exec = owner_disable_exec;

  def_owner_id(ot);
}

bool FL_wm_owner_operators_selftest(bContext *C, const char *filepath)
{
  WorkSpace *workspace = CTX_wm_workspace(C);
  if (workspace == nullptr) {
    return false;
  }
  constexpr const char *test_name = "flipendo.selftest";
  if (wmOwnerID *existing = static_cast<wmOwnerID *>(
          BLI_findstring(&workspace->owner_ids, test_name, offsetof(wmOwnerID, name))))
  {
    BLI_freelinkN(&workspace->owner_ids, existing);
  }

  PointerRNA props;
  WM_operator_properties_create(&props, "WM_OT_owner_enable");
  RNA_string_set(&props, "owner_id", test_name);
  const wmOperatorStatus enable_status = WM_operator_name_call(
      C, "WM_OT_owner_enable", WM_OP_EXEC_DEFAULT, &props, nullptr);
  WM_operator_properties_free(&props);
  const bool present_after_enable =
      BLI_findstring(&workspace->owner_ids, test_name, offsetof(wmOwnerID, name)) != nullptr;

  WM_operator_properties_create(&props, "WM_OT_owner_disable");
  RNA_string_set(&props, "owner_id", test_name);
  const wmOperatorStatus disable_status = WM_operator_name_call(
      C, "WM_OT_owner_disable", WM_OP_EXEC_DEFAULT, &props, nullptr);
  WM_operator_properties_free(&props);
  const bool present_after_disable =
      BLI_findstring(&workspace->owner_ids, test_name, offsetof(wmOwnerID, name)) != nullptr;

  FILE *fp = std::fopen(filepath, "w");
  if (fp == nullptr) {
    return false;
  }
  std::fprintf(fp,
               "enable status=%s present=%s\n",
               status_string(enable_status),
               present_after_enable ? "True" : "False");
  std::fprintf(fp,
               "disable status=%s present=%s\n",
               status_string(disable_status),
               present_after_disable ? "True" : "False");
  const bool ok = std::fclose(fp) == 0;
  if (ok) {
    std::printf("Prueba de operadores owner -> %s\n", filepath);
  }
  return ok;
}
