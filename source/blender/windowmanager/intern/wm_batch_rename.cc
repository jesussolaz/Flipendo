/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Renombrado por lotes nativo. La selección se convierte en PointerRNA y las
 * acciones trabajan sobre `name`, de modo que IDs y elementos embebidos usan
 * el mismo camino de actualización.
 */

#include <algorithm>
#include <cctype>
#include <regex>
#include <string>
#include <vector>

#include "MEM_guardedalloc.h"

#include "DNA_ID.h"
#include "DNA_ID_enums.h"
#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_collection_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.h"
#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_material.hh"
#include "BKE_report.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

namespace {

enum DataSource { SOURCE_SELECTED = 0, SOURCE_ALL };
enum DataType {
  DATA_OBJECT = 0,
  DATA_COLLECTION,
  DATA_MATERIAL,
  DATA_MESH,
  DATA_CURVE,
  DATA_META,
  DATA_VOLUME,
  DATA_GREASE_PENCIL,
  DATA_ARMATURE,
  DATA_LATTICE,
  DATA_LIGHT,
  DATA_LIGHT_PROBE,
  DATA_CAMERA,
  DATA_SPEAKER,
  DATA_BONE,
  DATA_NODE,
  DATA_SEQUENCE_STRIP,
  DATA_ACTION,
  DATA_SCENE,
  DATA_BRUSH,
};

static const EnumPropertyItem data_type_items[] = {
    {DATA_OBJECT, "OBJECT", ICON_OBJECT_DATA, "Objects", ""},
    {DATA_COLLECTION, "COLLECTION", ICON_OUTLINER_COLLECTION, "Collections", ""},
    {DATA_MATERIAL, "MATERIAL", ICON_MATERIAL_DATA, "Materials", ""},
    {0, "", 0, "", ""},
    {DATA_MESH, "MESH", ICON_MESH_DATA, "Meshes", ""},
    {DATA_CURVE, "CURVE", ICON_CURVE_DATA, "Curves", ""},
    {DATA_META, "META", ICON_META_DATA, "Metaballs", ""},
    {DATA_VOLUME, "VOLUME", ICON_VOLUME_DATA, "Volumes", ""},
    {DATA_GREASE_PENCIL, "GPENCIL", ICON_OUTLINER_DATA_GREASEPENCIL, "Grease Pencils", ""},
    {DATA_ARMATURE, "ARMATURE", ICON_ARMATURE_DATA, "Armatures", ""},
    {DATA_LATTICE, "LATTICE", ICON_LATTICE_DATA, "Lattices", ""},
    {DATA_LIGHT, "LIGHT", ICON_LIGHT_DATA, "Lights", ""},
    {DATA_LIGHT_PROBE, "LIGHT_PROBE", ICON_OUTLINER_DATA_LIGHTPROBE, "Light Probes", ""},
    {DATA_CAMERA, "CAMERA", ICON_CAMERA_DATA, "Cameras", ""},
    {DATA_SPEAKER, "SPEAKER", ICON_OUTLINER_DATA_SPEAKER, "Speakers", ""},
    {0, "", 0, "", ""},
    {DATA_BONE, "BONE", ICON_BONE_DATA, "Bones", ""},
    {DATA_NODE, "NODE", ICON_NODETREE, "Nodes", ""},
    {DATA_SEQUENCE_STRIP, "SEQUENCE_STRIP", ICON_SEQ_SEQUENCER, "Sequence Strips", ""},
    {DATA_ACTION, "ACTION_CLIP", ICON_ACTION, "Action Clips", ""},
    {0, "", 0, "", ""},
    {DATA_SCENE, "SCENE", ICON_SCENE_DATA, "Scenes", ""},
    {DATA_BRUSH, "BRUSH", ICON_BRUSH_DATA, "Brushes", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem data_source_items[] = {
    {SOURCE_SELECTED, "SELECT", 0, "Selected", ""},
    {SOURCE_ALL, "ALL", 0, "All", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

enum ActionType { ACTION_REPLACE = 0, ACTION_SET, ACTION_STRIP, ACTION_CASE };
enum SetMethod { SET_NEW = 0, SET_PREFIX, SET_SUFFIX };
enum StripFlag { STRIP_SPACE = 1, STRIP_DIGIT = 2, STRIP_PUNCT = 4 };
enum StripPart { STRIP_START = 1, STRIP_END = 2 };
enum CaseMethod { CASE_UPPER = 0, CASE_LOWER, CASE_TITLE };

static const EnumPropertyItem action_type_items[] = {
    {ACTION_REPLACE, "REPLACE", 0, "Find/Replace", "Replace text in the name"},
    {ACTION_SET, "SET", 0, "Set Name", "Set a new name or prefix/suffix the existing one"},
    {ACTION_STRIP, "STRIP", 0, "Strip Characters", "Strip leading/trailing text from the name"},
    {ACTION_CASE, "CASE", 0, "Change Case", "Change case of each name"},
    {0, nullptr, 0, nullptr, nullptr},
};
static const EnumPropertyItem set_method_items[] = {
    {SET_NEW, "NEW", 0, "New", ""},
    {SET_PREFIX, "PREFIX", 0, "Prefix", ""},
    {SET_SUFFIX, "SUFFIX", 0, "Suffix", ""},
    {0, nullptr, 0, nullptr, nullptr},
};
static const EnumPropertyItem strip_chars_items[] = {
    {STRIP_SPACE, "SPACE", 0, "Spaces", ""},
    {STRIP_DIGIT, "DIGIT", 0, "Digits", ""},
    {STRIP_PUNCT, "PUNCT", 0, "Punctuation", ""},
    {0, nullptr, 0, nullptr, nullptr},
};
static const EnumPropertyItem strip_part_items[] = {
    {STRIP_START, "START", 0, "Start", ""},
    {STRIP_END, "END", 0, "End", ""},
    {0, nullptr, 0, nullptr, nullptr},
};
static const EnumPropertyItem case_method_items[] = {
    {CASE_UPPER, "UPPER", 0, "Upper Case", ""},
    {CASE_LOWER, "LOWER", 0, "Lower Case", ""},
    {CASE_TITLE, "TITLE", 0, "Title Case", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

StructRNA *batch_action_srna()
{
  static StructRNA *srna = nullptr;
  if (srna != nullptr) {
    return srna;
  }
  srna = RNA_def_struct_ptr(&BLENDER_RNA, "BatchRenameAction", &RNA_PropertyGroup);
  RNA_def_struct_ui_text(srna, "Batch Rename Action", "One operation in a batch rename");
  RNA_def_enum(srna, "type", action_type_items, ACTION_REPLACE, "Operation", "");
  RNA_def_string(srna, "set_name", nullptr, 0, "Name", "");
  RNA_def_enum(srna, "set_method", set_method_items, SET_SUFFIX, "Method", "");
  PropertyRNA *prop = RNA_def_enum_flag(
      srna, "strip_chars", strip_chars_items, 0, "Strip Characters", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_TEXT);
  RNA_def_enum_flag(srna, "strip_part", strip_part_items, 0, "Strip Part", "");
  RNA_def_string(srna, "replace_src", nullptr, 0, "Find", "");
  RNA_def_string(srna, "replace_dst", nullptr, 0, "Replace", "");
  RNA_def_boolean(srna, "replace_match_case", false, "Case Sensitive", "");
  RNA_def_boolean(srna,
                  "use_replace_regex_src",
                  false,
                  "Regular Expression Find",
                  "Use regular expressions to match text in the 'Find' field");
  RNA_def_boolean(srna,
                  "use_replace_regex_dst",
                  false,
                  "Regular Expression Replace",
                  "Use regular expression for the replacement text (supporting groups)");
  RNA_def_enum(srna, "case_method", case_method_items, CASE_UPPER, "Case", "");
  prop = RNA_def_boolean(srna, "op_add", false, "Add", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_OPERATOR_DEFAULT);
  prop = RNA_def_boolean(srna, "op_remove", false, "Remove", "");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_OPERATOR_DEFAULT);
  return srna;
}

bool editable_pointer(const PointerRNA &ptr)
{
  return ptr.data != nullptr &&
         (ptr.owner_id == nullptr ||
          (ptr.owner_id->lib == nullptr && ptr.owner_id->override_library == nullptr));
}

void append_unique(blender::Vector<PointerRNA> &targets,
                   blender::Set<const void *> &seen,
                   const PointerRNA &ptr)
{
  if (editable_pointer(ptr) && seen.add(ptr.data)) {
    targets.append(ptr);
  }
}

short id_code_for_data_type(const int data_type)
{
  switch (data_type) {
    case DATA_OBJECT: return ID_OB;
    case DATA_COLLECTION: return ID_GR;
    case DATA_MATERIAL: return ID_MA;
    case DATA_MESH: return ID_ME;
    case DATA_CURVE: return ID_CU_LEGACY;
    case DATA_META: return ID_MB;
    case DATA_VOLUME: return ID_VO;
    case DATA_GREASE_PENCIL: return ID_GP;
    case DATA_ARMATURE: return ID_AR;
    case DATA_LATTICE: return ID_LT;
    case DATA_LIGHT: return ID_LA;
    case DATA_LIGHT_PROBE: return ID_LP;
    case DATA_CAMERA: return ID_CA;
    case DATA_SPEAKER: return ID_SPK;
    case DATA_ACTION: return ID_AC;
    case DATA_SCENE: return ID_SCE;
    case DATA_BRUSH: return ID_BR;
    default: return 0;
  }
}

blender::Vector<PointerRNA> targets_from_context(bContext *C, const int data_type, const int source)
{
  blender::Vector<PointerRNA> targets;
  blender::Set<const void *> seen;
  if (source == SOURCE_ALL) {
    const short id_code = id_code_for_data_type(data_type);
    if (id_code != 0) {
      ListBase *list = which_libbase(CTX_data_main(C), id_code);
      LISTBASE_FOREACH (ID *, id, list) {
        append_unique(targets, seen, RNA_id_pointer_create(id));
      }
    }
    return targets;
  }

  blender::Vector<PointerRNA> selected;
  if (data_type == DATA_OBJECT) {
    CTX_data_selected_editable_objects(C, &selected);
    for (PointerRNA &ptr : selected) {
      append_unique(targets, seen, ptr);
    }
    return targets;
  }
  if (data_type == DATA_NODE) {
    CTX_data_selected_nodes(C, &selected);
    for (PointerRNA &ptr : selected) {
      append_unique(targets, seen, ptr);
    }
    return targets;
  }
  if (data_type == DATA_BONE) {
    CTX_data_selected_pose_bones(C, &selected);
    for (PointerRNA &ptr : selected) {
      PointerRNA bone;
      if (RNA_pointer_get(&ptr, "bone").data != nullptr) {
        bone = RNA_pointer_get(&ptr, "bone");
        append_unique(targets, seen, bone);
      }
      else {
        append_unique(targets, seen, ptr);
      }
    }
    return targets;
  }
  if (data_type == DATA_SEQUENCE_STRIP) {
    selected = CTX_data_collection_get(C, "selected_strips");
    for (const PointerRNA &ptr : selected) {
      append_unique(targets, seen, ptr);
    }
    return targets;
  }

  CTX_data_selected_objects(C, &selected);
  const short wanted_code = id_code_for_data_type(data_type);
  for (const PointerRNA &ptr : selected) {
    Object *ob = static_cast<Object *>(ptr.data);
    if (data_type == DATA_COLLECTION && ob->instance_collection != nullptr) {
      append_unique(targets, seen, RNA_id_pointer_create(&ob->instance_collection->id));
    }
    else if (data_type == DATA_MATERIAL) {
      for (int i = 0; i < ob->totcol; i++) {
        Material *material = BKE_object_material_get(ob, i + 1);
        if (material != nullptr) {
          append_unique(targets, seen, RNA_id_pointer_create(&material->id));
        }
      }
    }
    else if (data_type == DATA_ACTION && ob->adt && ob->adt->action) {
      append_unique(targets, seen, RNA_id_pointer_create(&ob->adt->action->id));
    }
    else if (ob->data != nullptr) {
      ID *data = static_cast<ID *>(ob->data);
      if (GS(data->name) == wanted_code) {
        append_unique(targets, seen, RNA_id_pointer_create(data));
      }
    }
  }
  if (data_type == DATA_SCENE) {
    append_unique(targets, seen, RNA_id_pointer_create(&CTX_data_scene(C)->id));
  }
  return targets;
}

std::string lower_ascii(std::string value)
{
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return char(std::tolower(c));
  });
  return value;
}

std::string replace_plain(std::string name,
                          const std::string &needle,
                          const std::string &replacement,
                          const bool match_case)
{
  if (needle.empty()) {
    return name;
  }
  size_t pos = 0;
  while (pos <= name.size()) {
    const std::string haystack = match_case ? name : lower_ascii(name);
    const std::string find = match_case ? needle : lower_ascii(needle);
    pos = haystack.find(find, pos);
    if (pos == std::string::npos) {
      break;
    }
    name.replace(pos, needle.size(), replacement);
    pos += replacement.size();
  }
  return name;
}

std::string regex_replacement_to_cpp(const std::string &replacement)
{
  std::string result;
  for (size_t i = 0; i < replacement.size(); i++) {
    if (replacement[i] == '\\' && i + 1 < replacement.size() &&
        std::isdigit(static_cast<unsigned char>(replacement[i + 1])))
    {
      result += '$';
      result += replacement[++i];
    }
    else {
      result += replacement[i];
    }
  }
  return result;
}

std::string apply_actions(PointerRNA *actions_ptr, std::string name)
{
  RNA_BEGIN (actions_ptr, action, "actions") {
    const int type = RNA_enum_get(&action, "type");
    if (type == ACTION_SET) {
      char *text = RNA_string_get_alloc(&action, "set_name", nullptr, 0, nullptr);
      const int method = RNA_enum_get(&action, "set_method");
      if (method == SET_NEW) name = text;
      else if (method == SET_PREFIX) name = std::string(text) + name;
      else name += text;
      MEM_freeN(text);
    }
    else if (type == ACTION_STRIP) {
      const int chars = RNA_enum_get(&action, "strip_chars");
      const int part = RNA_enum_get(&action, "strip_part");
      auto strip_char = [&](unsigned char c) {
        return ((chars & STRIP_SPACE) && c == ' ') ||
               ((chars & STRIP_DIGIT) && std::isdigit(c)) ||
               ((chars & STRIP_PUNCT) && std::ispunct(c));
      };
      if (part & STRIP_START) {
        name.erase(name.begin(), std::find_if_not(name.begin(), name.end(), strip_char));
      }
      if (part & STRIP_END) {
        name.erase(std::find_if_not(name.rbegin(), name.rend(), strip_char).base(), name.end());
      }
    }
    else if (type == ACTION_REPLACE) {
      char *src = RNA_string_get_alloc(&action, "replace_src", nullptr, 0, nullptr);
      char *dst = RNA_string_get_alloc(&action, "replace_dst", nullptr, 0, nullptr);
      if (RNA_boolean_get(&action, "use_replace_regex_src")) {
        const std::regex::flag_type flags = std::regex::ECMAScript |
            (RNA_boolean_get(&action, "replace_match_case") ? std::regex::flag_type{} : std::regex::icase);
        const std::regex pattern(src, flags);
        const std::string replacement = RNA_boolean_get(&action, "use_replace_regex_dst") ?
                                            regex_replacement_to_cpp(dst) :
                                            std::regex_replace(std::string(dst), std::regex(R"([\$])"), R"(\$&)");
        name = std::regex_replace(name, pattern, replacement);
      }
      else {
        name = replace_plain(name, src, dst, RNA_boolean_get(&action, "replace_match_case"));
      }
      MEM_freeN(src);
      MEM_freeN(dst);
    }
    else if (type == ACTION_CASE) {
      const int method = RNA_enum_get(&action, "case_method");
      bool word_start = true;
      for (char &c : name) {
        const unsigned char byte = c;
        if (method == CASE_UPPER) c = char(std::toupper(byte));
        else if (method == CASE_LOWER) c = char(std::tolower(byte));
        else {
          c = word_start ? char(std::toupper(byte)) : char(std::tolower(byte));
          word_start = !std::isalnum(byte);
        }
      }
    }
  }
  RNA_END;
  return name;
}

wmOperatorStatus batch_rename_exec(bContext *C, wmOperator *op)
{
  const int data_type = RNA_enum_get(op->ptr, "data_type");
  blender::Vector<PointerRNA> targets = targets_from_context(
      C, data_type, RNA_enum_get(op->ptr, "data_source"));
  int changed = 0;
  for (PointerRNA &target : targets) {
    PropertyRNA *name_prop = RNA_struct_find_property(&target, "name");
    if (name_prop == nullptr || RNA_property_type(name_prop) != PROP_STRING ||
        !RNA_property_editable(&target, name_prop))
    {
      continue;
    }
    char *src = RNA_property_string_get_alloc(&target, name_prop, nullptr, 0, nullptr);
    const std::string dst = apply_actions(op->ptr, src);
    if (dst != src) {
      RNA_property_string_set(&target, name_prop, dst.c_str());
      RNA_property_update(C, &target, name_prop);
      changed++;
    }
    MEM_freeN(src);
  }
  BKE_reportf(op->reports,
              RPT_INFO,
              "Renamed %d of %d item(s)",
              changed,
              int(targets.size()));
  return OPERATOR_FINISHED;
}

wmOperatorStatus batch_rename_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  if (RNA_collection_length(op->ptr, "actions") == 0) {
    PointerRNA action;
    RNA_collection_add(op->ptr, "actions", &action);
  }
  return WM_operator_props_dialog_popup(C, op, 400);
}

bool batch_rename_check(bContext * /*C*/, wmOperator *op)
{
  bool changed = false;
  int index = 0;
  RNA_BEGIN (op->ptr, action, "actions") {
    if (RNA_boolean_get(&action, "op_add")) {
      RNA_boolean_set(&action, "op_add", false);
      PointerRNA added;
      RNA_collection_add(op->ptr, "actions", &added);
      PropertyRNA *actions_prop = RNA_struct_find_property(op->ptr, "actions");
      RNA_property_collection_move(
          op->ptr, actions_prop, RNA_collection_length(op->ptr, "actions") - 1, index + 1);
      changed = true;
      break;
    }
    if (RNA_boolean_get(&action, "op_remove")) {
      RNA_boolean_set(&action, "op_remove", false);
      if (RNA_collection_length(op->ptr, "actions") > 1) {
        PropertyRNA *actions_prop = RNA_struct_find_property(op->ptr, "actions");
        RNA_property_collection_remove(op->ptr, actions_prop, index);
      }
      changed = true;
      break;
    }
    index++;
  }
  RNA_END;
  return changed;
}

void batch_rename_draw(bContext *C, wmOperator *op)
{
  uiLayout *layout = op->layout;
  uiLayout *top = &layout->row(true);
  top->prop(op->ptr, "data_source", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
  top->prop(op->ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
  RNA_BEGIN (op->ptr, action, "actions") {
    uiLayout *box = &layout->box();
    box->prop(&action, "type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    const int type = RNA_enum_get(&action, "type");
    if (type == ACTION_SET) {
      box->prop(&action, "set_method", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
      box->prop(&action, "set_name", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    }
    else if (type == ACTION_STRIP) {
      box->prop(&action, "strip_chars", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      box->prop(&action, "strip_part", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    }
    else if (type == ACTION_REPLACE) {
      box->prop(&action, "replace_src", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      box->prop(&action, "replace_dst", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      box->prop(&action, "replace_match_case", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      box->prop(&action, "use_replace_regex_src", UI_ITEM_NONE, std::nullopt, ICON_NONE);
      box->prop(&action, "use_replace_regex_dst", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    }
    else {
      box->prop(&action, "case_method", UI_ITEM_R_EXPAND, std::nullopt, ICON_NONE);
    }
    uiLayout *buttons = &box->row(true);
    buttons->prop(&action, "op_remove", UI_ITEM_NONE, "", ICON_REMOVE);
    buttons->prop(&action, "op_add", UI_ITEM_NONE, "", ICON_ADD);
  }
  RNA_END;
  const int count = targets_from_context(
                        C, RNA_enum_get(op->ptr, "data_type"), RNA_enum_get(op->ptr, "data_source"))
                        .size();
  layout->label("Rename " + std::to_string(count) + " item(s)", ICON_NONE);
}

}  // namespace

void WM_OT_batch_rename(wmOperatorType *ot)
{
  ot->name = "Batch Rename";
  ot->idname = "WM_OT_batch_rename";
  ot->description = "Rename multiple items at once";
  ot->invoke = batch_rename_invoke;
  ot->exec = batch_rename_exec;
  ot->check = batch_rename_check;
  ot->ui = batch_rename_draw;
  ot->flag = OPTYPE_UNDO;

  PropertyRNA *prop = RNA_def_enum(
      ot->srna, "data_type", data_type_items, DATA_OBJECT, "Type", "Type of data to rename");
  RNA_def_property_translation_context(prop, BLT_I18NCONTEXT_ID_ID);
  RNA_def_property_flag(prop, PROP_ENUM_NO_CONTEXT);
  prop = RNA_def_enum(ot->srna, "data_source", data_source_items, SOURCE_SELECTED, "Source", "");
  RNA_def_property_flag(prop, PROP_ENUM_NO_CONTEXT);
  RNA_def_collection_runtime(ot->srna, "actions", batch_action_srna(), "actions", "");
}

bool FL_wm_batch_rename_selftest(bContext *C, const char *filepath)
{
  blender::Vector<PointerRNA> targets = targets_from_context(C, DATA_OBJECT, SOURCE_ALL);
  std::vector<std::string> original_names;
  for (PointerRNA &target : targets) {
    char *name = RNA_string_get_alloc(&target, "name", nullptr, 0, nullptr);
    original_names.emplace_back(name);
    MEM_freeN(name);
  }

  PointerRNA props;
  WM_operator_properties_create(&props, "WM_OT_batch_rename");
  RNA_enum_set_identifier(C, &props, "data_type", "OBJECT");
  RNA_enum_set_identifier(C, &props, "data_source", "ALL");
  PointerRNA action;
  RNA_collection_add(&props, "actions", &action);
  RNA_enum_set_identifier(C, &action, "type", "SET");
  RNA_enum_set_identifier(C, &action, "set_method", "PREFIX");
  RNA_string_set(&action, "set_name", "pre-");
  RNA_collection_add(&props, "actions", &action);
  RNA_enum_set_identifier(C, &action, "type", "REPLACE");
  RNA_string_set(&action, "replace_src", "a");
  RNA_string_set(&action, "replace_dst", "_");
  RNA_collection_add(&props, "actions", &action);
  RNA_enum_set_identifier(C, &action, "type", "CASE");
  RNA_enum_set_identifier(C, &action, "case_method", "UPPER");
  const wmOperatorStatus status = WM_operator_name_call(
      C, "WM_OT_batch_rename", WM_OP_EXEC_DEFAULT, &props, nullptr);
  WM_operator_properties_free(&props);

  std::vector<std::string> renamed;
  for (PointerRNA &target : targets) {
    char *name = RNA_string_get_alloc(&target, "name", nullptr, 0, nullptr);
    renamed.emplace_back(name);
    MEM_freeN(name);
  }
  std::sort(renamed.begin(), renamed.end());

  FILE *fp = std::fopen(filepath, "w");
  if (fp == nullptr) {
    return false;
  }
  fprintf(fp, "status=%d count=%zu names=[", int(status), renamed.size());
  for (size_t i = 0; i < renamed.size(); i++) {
    fprintf(fp, "%s%s", i ? "," : "", renamed[i].c_str());
  }
  fprintf(fp, "]\n");
  const bool ok = std::fclose(fp) == 0;

  for (const int i : targets.index_range()) {
    RNA_string_set(&targets[i], "name", original_names[i].c_str());
  }
  return ok;
}
