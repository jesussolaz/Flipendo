/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Operadores pequeños de integración con el sistema que antes vivían en
 * `scripts/startup/bl_operators/wm.py`.
 */

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <spawn.h>
#include <string>
#include <sys/sysctl.h>
#include <sys/wait.h>
#include <utility>
#include <vector>

#include "BLI_fileops.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"

#include "BLT_lang.hh"
#include "BLT_translation.hh"

#include "BKE_blender_version.h"
#include "BKE_context.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"

#include "GPU_context.hh"
#include "GPU_platform.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "FL_operator_dump.hpp"

extern char **environ;
extern "C" char build_commit_date[];
extern "C" char build_commit_time[];
extern "C" char build_hash[];
extern "C" char build_branch[];

namespace {

bool open_with_default_application(const char *target)
{
#ifdef __APPLE__
  pid_t pid = 0;
  char *const argv[] = {const_cast<char *>("/usr/bin/open"), const_cast<char *>(target), nullptr};
  const int spawn_result = posix_spawn(&pid, argv[0], nullptr, nullptr, argv, environ);
  if (spawn_result != 0) {
    errno = spawn_result;
    return false;
  }

  int status = 0;
  while (waitpid(pid, &status, 0) == -1) {
    if (errno != EINTR) {
      return false;
    }
  }
  return WIFEXITED(status) && WEXITSTATUS(status) == 0;
#else
  UNUSED_VARS(target);
  return false;
#endif
}

bool url_has_scheme(const std::string &url)
{
  if (url.empty() || !std::isalpha(static_cast<unsigned char>(url[0]))) {
    return false;
  }
  for (size_t i = 1; i < url.size(); i++) {
    const unsigned char c = url[i];
    if (c == ':') {
      return true;
    }
    if (!(std::isalnum(c) || c == '+' || c == '-' || c == '.')) {
      return false;
    }
  }
  return false;
}

std::string url_authority(const std::string &url)
{
  const size_t scheme = url.find(':');
  if (scheme == std::string::npos || url.compare(scheme + 1, 2, "//") != 0) {
    return {};
  }
  const size_t begin = scheme + 3;
  const size_t end = url.find_first_of("/?#", begin);
  return url.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
}

bool blender_domain(const std::string &authority)
{
  if (authority == "blender.org") {
    return true;
  }
  constexpr const char suffix[] = ".blender.org";
  return authority.size() > sizeof(suffix) - 1 &&
         authority.compare(authority.size() - (sizeof(suffix) - 1), sizeof(suffix) - 1, suffix) ==
             0;
}

int hex_value(const char c)
{
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

std::string url_decode(const std::string &text)
{
  std::string result;
  result.reserve(text.size());
  for (size_t i = 0; i < text.size(); i++) {
    if (text[i] == '+') {
      result += ' ';
    }
    else if (text[i] == '%' && i + 2 < text.size()) {
      const int high = hex_value(text[i + 1]);
      const int low = hex_value(text[i + 2]);
      if (high >= 0 && low >= 0) {
        result += char((high << 4) | low);
        i += 2;
      }
      else {
        result += text[i];
      }
    }
    else {
      result += text[i];
    }
  }
  return result;
}

bool url_unreserved(const unsigned char c)
{
  return std::isalnum(c) || ELEM(c, '-', '_', '.', '~');
}

std::string url_encode(const std::string &text)
{
  static constexpr char digits[] = "0123456789ABCDEF";
  std::string result;
  for (const unsigned char c : text) {
    if (url_unreserved(c)) {
      result += char(c);
    }
    else if (c == ' ') {
      result += '+';
    }
    else {
      result += '%';
      result += digits[c >> 4];
      result += digits[c & 15];
    }
  }
  return result;
}

struct QueryValue {
  std::string key;
  std::vector<std::string> values;
};

std::vector<QueryValue> parse_query(const std::string &query)
{
  std::vector<QueryValue> result;
  size_t begin = 0;
  while (begin <= query.size()) {
    const size_t end = query.find('&', begin);
    const std::string field = query.substr(
        begin, end == std::string::npos ? std::string::npos : end - begin);
    const size_t equals = field.find('=');
    const std::string key = url_decode(field.substr(0, equals));
    const std::string value = equals == std::string::npos ? "" : url_decode(field.substr(equals + 1));

    /* urllib.parse.parse_qs usa keep_blank_values=False por defecto. */
    if (!value.empty()) {
      auto existing = std::find_if(result.begin(), result.end(), [&](const QueryValue &item) {
        return item.key == key;
      });
      if (existing == result.end()) {
        result.push_back({key, {value}});
      }
      else {
        existing->values.push_back(value);
      }
    }

    if (end == std::string::npos) {
      break;
    }
    begin = end + 1;
  }
  return result;
}

std::string build_query(const std::vector<QueryValue> &query)
{
  std::string result;
  for (const QueryValue &item : query) {
    for (const std::string &value : item.values) {
      if (!result.empty()) {
        result += '&';
      }
      result += url_encode(item.key);
      result += '=';
      result += url_encode(value);
    }
  }
  return result;
}

std::string utm_source()
{
  std::string source = "blender-";
  source += BKE_blender_version_string();
  for (char &c : source) {
    if (std::isspace(static_cast<unsigned char>(c))) {
      c = '-';
    }
    else {
      c = char(std::tolower(static_cast<unsigned char>(c)));
    }
  }
  return source;
}

std::string complete_url(std::string url)
{
  if (!url_has_scheme(url)) {
    url = "https://" + url;
  }
  if (!blender_domain(url_authority(url))) {
    return url;
  }

  const size_t fragment_pos = url.find('#');
  const std::string fragment = fragment_pos == std::string::npos ? "" : url.substr(fragment_pos);
  const std::string without_fragment = url.substr(0, fragment_pos);
  const size_t query_pos = without_fragment.find('?');
  const std::string base = without_fragment.substr(0, query_pos);
  std::vector<QueryValue> query = parse_query(
      query_pos == std::string::npos ? "" : without_fragment.substr(query_pos + 1));

  auto utm = std::find_if(query.begin(), query.end(), [](const QueryValue &item) {
    return item.key == "utm_source";
  });
  if (utm == query.end()) {
    query.push_back({"utm_source", {utm_source()}});
  }
  else {
    utm->values = {utm_source()};
  }
  return base + '?' + build_query(query) + fragment;
}

std::string manual_language_code()
{
  static const std::pair<const char *, const char *> languages[] = {
      {"ar_EG", "ar"}, {"ca_AD", "ca"}, {"de_DE", "de"}, {"el_GR", "el"},
      {"es", "es"},    {"fi_FI", "fi"}, {"fr_FR", "fr"}, {"id_ID", "id"},
      {"it_IT", "it"}, {"ja_JP", "ja"}, {"ko_KR", "ko"}, {"nl_NL", "nl"},
      {"pt_PT", "pt"}, {"pt_BR", "pt"}, {"ru_RU", "ru"}, {"sk_SK", "sk"},
      {"sr_RS", "sr"}, {"th_TH", "th"}, {"uk_UA", "uk"}, {"vi_VN", "vi"},
      {"zh_HANS", "zh-hans"},
      {"zh_HANT", "zh-hant"},
  };
  const char *locale = BLT_lang_get();
  for (const auto &[identifier, manual_code] : languages) {
    if (STREQ(locale, identifier)) {
      return manual_code;
    }
  }
  return "en";
}

std::string macos_platform_string()
{
  char version[128] = "unknown";
  size_t version_size = sizeof(version);
  if (sysctlbyname("kern.osproductversion", version, &version_size, nullptr, 0) != 0) {
    STRNCPY(version, "unknown");
  }
  return "macOS-" + std::string(version) + "-x86_64-i386-64bit";
}

std::string bug_report_url()
{
  std::vector<QueryValue> query = {
      {"type", {"bug_report"}},
      {"project", {"blender"}},
      {"os", {macos_platform_string() + " 64 Bits"}},
      {"gpu",
       {std::string(GPU_platform_renderer()) + " " + GPU_platform_vendor() + " " +
        GPU_platform_version()}},
      {"broken_version",
       {std::string(BKE_blender_version_string()) + ", branch: " + build_branch +
        ", commit date: " + build_commit_date + " " + build_commit_time + ", hash: `" +
        build_hash + "`"}},
  };
  return "https://redirect.blender.org/?" + build_query(query);
}

enum UrlPreset {
  URL_PRESET_BUG = 0,
  URL_PRESET_RELEASE_NOTES,
  URL_PRESET_MANUAL,
  URL_PRESET_API,
  URL_PRESET_FUND,
  URL_PRESET_BLENDER,
  URL_PRESET_CREDITS,
  URL_PRESET_EXTENSIONS,
};

static const EnumPropertyItem url_preset_items[] = {
    {URL_PRESET_BUG, "BUG", 0, N_("Bug"), N_("Report a bug with pre-filled version information")},
    {URL_PRESET_RELEASE_NOTES,
     "RELEASE_NOTES",
     0,
     N_("Release Notes"),
     N_("Read about what's new in this version of Blender")},
    {URL_PRESET_MANUAL,
     "MANUAL",
     0,
     N_("User Manual"),
     N_("The reference manual for this version of Blender")},
    {URL_PRESET_API,
     "API",
     0,
     N_("Python API Reference"),
     N_("The API reference manual for this version of Blender")},
    {URL_PRESET_FUND,
     "FUND",
     0,
     N_("Development Fund"),
     N_("The donation program to support maintenance and improvements")},
    {URL_PRESET_BLENDER,
     "BLENDER",
     0,
     "blender.org",
     N_("Blender's official web-site")},
    {URL_PRESET_CREDITS,
     "CREDITS",
     0,
     N_("Credits"),
     N_("Lists committers to Blender's source code")},
    {URL_PRESET_EXTENSIONS,
     "EXTENSIONS",
     0,
     N_("Extensions Platform"),
     N_("Online directory of free and open source extensions")},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem *url_preset_itemf(bContext * /*C*/,
                                                PointerRNA * /*ptr*/,
                                                PropertyRNA * /*prop*/,
                                                bool *r_free)
{
  *r_free = false;
  return url_preset_items;
}

std::string url_from_preset(const int preset)
{
  const int major = BLENDER_VERSION / 100;
  const int minor = BLENDER_VERSION % 100;
  switch (preset) {
    case URL_PRESET_BUG:
      return bug_report_url();
    case URL_PRESET_RELEASE_NOTES:
      return "https://www.blender.org/download/releases/" + std::to_string(major) + '-' +
             std::to_string(minor) + '/';
    case URL_PRESET_MANUAL:
      return "https://docs.blender.org/manual/" + manual_language_code() + '/' +
             std::to_string(major) + '.' + std::to_string(minor) + '/';
    case URL_PRESET_API:
      return "https://docs.blender.org/api/" + std::to_string(major) + '.' +
             std::to_string(minor) + '/';
    case URL_PRESET_FUND:
      return "https://fund.blender.org";
    case URL_PRESET_BLENDER:
      return "https://www.blender.org";
    case URL_PRESET_CREDITS:
      return "https://www.blender.org/about/credits/";
    case URL_PRESET_EXTENSIONS:
      return "https://extensions.blender.org/";
    default:
      return {};
  }
}

std::optional<std::string> documentation_url(const char *doc_id, ReportList *reports)
{
  const std::string id = doc_id ? doc_id : "";
  const size_t dot = id.find('.');
  const std::string prefix = "https://docs.blender.org/api/" +
                             std::to_string(BLENDER_VERSION / 100) + "." +
                             std::to_string(BLENDER_VERSION % 100);
  if (dot == std::string::npos) {
    return prefix + "/bpy.types." + id + ".html";
  }
  if (id.find('.', dot + 1) != std::string::npos) {
    return std::nullopt;
  }

  std::string class_name = id.substr(0, dot);
  const std::string property_name = id.substr(dot + 1);
  if (WM_operatortype_find(id.c_str(), true) != nullptr) {
    return prefix + "/bpy.ops." + class_name + ".html#bpy.ops." + class_name + "." +
           property_name;
  }
  if (WM_operatortype_find(class_name.c_str(), true) != nullptr) {
    char python_id[OP_MAX_TYPENAME];
    WM_operator_py_idname(python_id, class_name.c_str());
    const std::string python_name = python_id;
    const size_t python_dot = python_name.find('.');
    if (python_dot != std::string::npos) {
      const std::string module = python_name.substr(0, python_dot);
      const std::string op_name = python_name.substr(python_dot + 1);
      return prefix + "/bpy.ops." + module + ".html#bpy.ops." + module + "." + op_name;
    }
  }

  StructRNA *srna = RNA_struct_find(class_name.c_str());
  if (srna == nullptr) {
    if (reports != nullptr) {
      BKE_reportf(reports, RPT_ERROR, "Type \"%s\" cannot be found", class_name.c_str());
    }
    return std::nullopt;
  }

  PropertyRNA *property = RNA_struct_type_find_property(srna, property_name.c_str());
  if (property == nullptr) {
    return prefix + "/bpy.types.bpy_struct.html#bpy.types.bpy_struct.items";
  }
  for (StructRNA *base = RNA_struct_base(srna); base != nullptr; base = RNA_struct_base(base)) {
    if (RNA_struct_type_find_property(base, property_name.c_str()) != property) {
      break;
    }
    class_name = RNA_struct_identifier(base);
  }
  return prefix + "/bpy.types." + class_name + ".html#bpy.types." + class_name + "." +
         property_name;
}

}  // namespace

static wmOperatorStatus url_open_exec(bContext * /*C*/, wmOperator *op)
{
  char url[4096];
  RNA_string_get(op->ptr, "url", url);
  const std::string completed = complete_url(url);
  /* webbrowser.open tambien devolvia FINISHED aunque el lanzador no encontrase un
   * navegador. Se intenta abrir y se conserva ese contrato exterior. */
  open_with_default_application(completed.c_str());
  return OPERATOR_FINISHED;
}

void WM_OT_url_open(wmOperatorType *ot)
{
  ot->name = "";
  ot->idname = "WM_OT_url_open";
  ot->description = "Open a website in the web browser";
  ot->exec = url_open_exec;
  ot->flag = OPTYPE_INTERNAL;

  RNA_def_string(ot->srna, "url", nullptr, 0, "URL", "URL to open");
}

static wmOperatorStatus url_open_preset_exec(bContext * /*C*/, wmOperator *op)
{
  const std::string url = url_from_preset(RNA_enum_get(op->ptr, "type"));
  if (url.empty()) {
    return OPERATOR_CANCELLED;
  }
  open_with_default_application(complete_url(url).c_str());
  return OPERATOR_FINISHED;
}

void WM_OT_url_open_preset(wmOperatorType *ot)
{
  ot->name = "Open Preset Website";
  ot->idname = "WM_OT_url_open_preset";
  ot->description = "Open a preset website in the web browser";
  ot->exec = url_open_preset_exec;
  ot->flag = OPTYPE_INTERNAL;

  ot->prop = RNA_def_enum(
      ot->srna, "type", rna_enum_dummy_NULL_items, URL_PRESET_BUG, "Site", "");
  RNA_def_property_enum_funcs_runtime(ot->prop, nullptr, nullptr, url_preset_itemf);
}

static wmOperatorStatus path_open_exec(bContext *C, wmOperator *op)
{
  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);
  if (filepath[0] == '\0') {
    BKE_report(op->reports, RPT_ERROR, "File path was not set");
    return OPERATOR_CANCELLED;
  }

  BLI_path_abs(filepath, BKE_main_blendfile_path(CTX_data_main(C)));
  BLI_path_normalize(filepath);
  if (!BLI_exists(filepath)) {
    BKE_reportf(op->reports, RPT_ERROR, "File '%s' not found", filepath);
    return OPERATOR_CANCELLED;
  }

  /* subprocess.check_call(["open", filepath]) era el camino Python en macOS. */
  open_with_default_application(filepath);
  return OPERATOR_FINISHED;
}

void WM_OT_path_open(wmOperatorType *ot)
{
  ot->name = "";
  ot->idname = "WM_OT_path_open";
  ot->description = "Open a path in a file browser";
  ot->exec = path_open_exec;
  ot->flag = OPTYPE_INTERNAL;

  PropertyRNA *prop = RNA_def_string(
      ot->srna, "filepath", nullptr, 0, "filepath", "");
  RNA_def_property_subtype(prop, PROP_FILEPATH);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static wmOperatorStatus doc_view_exec(bContext * /*C*/, wmOperator *op)
{
  char doc_id[1025];
  RNA_string_get(op->ptr, "doc_id", doc_id);
  const std::optional<std::string> url = documentation_url(doc_id, op->reports);
  if (!url) {
    return OPERATOR_CANCELLED;
  }
  open_with_default_application(complete_url(*url).c_str());
  return OPERATOR_FINISHED;
}

void WM_OT_doc_view(wmOperatorType *ot)
{
  ot->name = "View Documentation";
  ot->idname = "WM_OT_doc_view";
  ot->description = "Open online reference docs in a web browser";
  ot->exec = doc_view_exec;

  PropertyRNA *prop = RNA_def_string(ot->srna, "doc_id", nullptr, 1025, "Doc ID", "");
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

bool FL_wm_system_operators_selftest(bContext *C, const char *filepath)
{
  FILE *fp = std::fopen(filepath, "w");
  if (fp == nullptr) {
    return false;
  }

  const char *url_cases[] = {
      "blender.org",
      "https://blender.org/path",
      "https://www.blender.org/path?a=1&utm_source=old&b=two%20words#frag",
      "https://example.com/?x=1",
  };
  for (const char *url : url_cases) {
    std::fprintf(fp, "url=%s -> %s\n", url, complete_url(url).c_str());
  }
  for (int preset = URL_PRESET_RELEASE_NOTES; preset <= URL_PRESET_EXTENSIONS; preset++) {
    std::fprintf(fp,
                 "preset=%d -> %s\n",
                 preset,
                 complete_url(url_from_preset(preset)).c_str());
  }

  const char *doc_cases[] = {
      "Object",
      "Object.location",
      "Object.name",
      "Object.definitely_custom",
      "wm.open_mainfile",
      "WM_OT_open_mainfile.anything",
      "DefinitelyMissingType.value",
      "too.many.parts",
  };
  for (const char *doc_id : doc_cases) {
    const std::optional<std::string> url = documentation_url(doc_id, nullptr);
    std::fprintf(fp, "doc=%s -> %s\n", doc_id, url ? url->c_str() : "<null>");
  }

  wmOperatorStatus empty_status;
  wmOperatorStatus missing_status;
  {
    PointerRNA props;
    WM_operator_properties_create(&props, "WM_OT_path_open");
    RNA_string_set(&props, "filepath", "");
    empty_status = WM_operator_name_call(
        C, "WM_OT_path_open", WM_OP_EXEC_DEFAULT, &props, nullptr);
    WM_operator_properties_free(&props);
  }
  {
    PointerRNA props;
    WM_operator_properties_create(&props, "WM_OT_path_open");
    RNA_string_set(&props, "filepath", "/flipendo-selftest/path-that-does-not-exist");
    missing_status = WM_operator_name_call(
        C, "WM_OT_path_open", WM_OP_EXEC_DEFAULT, &props, nullptr);
    WM_operator_properties_free(&props);
  }
  std::fprintf(
      fp, "path_open empty=%d missing=%d\n", int(empty_status), int(missing_status));

  const bool ok = std::fclose(fp) == 0;
  if (ok) {
    std::printf("Prueba de operadores de sistema -> %s\n", filepath);
  }
  return ok;
}
