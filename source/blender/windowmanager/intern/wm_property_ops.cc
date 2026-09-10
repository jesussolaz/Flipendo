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

#include <cstdio>
#include <string>

#include "DNA_ID.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_timer.h"

#include "BKE_context.hh"
#include "BKE_idprop.hh"
#include "BKE_report.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_path.hh"
#include "RNA_prototypes.hh"

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
