/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Utilidades del Window Manager que antes dependían de Python: informe del
 * sistema, arranque del Player y listado de operadores.
 */

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <spawn.h>
#include <string>
#include <sys/wait.h>
#include <vector>

#include "DNA_scene_types.h"
#include "DNA_text_types.h"

#include "BLI_fileops.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"

#include "BKE_appdir.hh"
#include "BKE_blender_version.h"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"
#include "BKE_text.h"

#include "GPU_capabilities.hh"
#include "GPU_context.hh"
#include "GPU_platform.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

extern char **environ;
extern "C" char build_date[];
extern "C" char build_time[];
extern "C" char build_commit_date[];
extern "C" char build_commit_time[];
extern "C" char build_hash[];
extern "C" char build_branch[];
extern "C" char build_platform[];
extern "C" char build_type[];
extern "C" char build_cflags[];
extern "C" char build_cxxflags[];
extern "C" char build_linkflags[];
extern "C" char build_system[];

namespace {

/* -------------------------------------------------------------------- */
/** \name wm.sysinfo
 * \{ */

static wmOperatorStatus sysinfo_exec(bContext * /*C*/, wmOperator *op)
{
  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);
  FILE *fp = BLI_fopen(filepath, "w");
  if (fp == nullptr) {
    BKE_reportf(op->reports, RPT_ERROR, "Unable to open '%s': %s", filepath, strerror(errno));
    return OPERATOR_CANCELLED;
  }

  const std::string header = std::string("= Blender ") + BKE_blender_version_string() +
                             " System Information =";
  const std::string rule(header.size(), '=');
  fprintf(fp, "%s\n%s\n%s\n\n", rule.c_str(), header.c_str(), rule.c_str());
  fprintf(fp, "Blender:\n%s\n\n", rule.c_str());
  fprintf(fp,
          "version: %s, branch: %s, commit date: %s %s, hash: %s, type: %s\n",
          BKE_blender_version_string(),
          build_branch,
          build_commit_date,
          build_commit_time,
          build_hash,
          build_type);
  fprintf(fp, "build date: %s, %s\n", build_date, build_time);
  fprintf(fp, "platform: %s\n", build_platform);
  fprintf(fp, "binary path: %s\n", BKE_appdir_program_path());
  fprintf(fp, "build cflags: %s\n", build_cflags);
  fprintf(fp, "build cxxflags: %s\n", build_cxxflags);
  fprintf(fp, "build linkflags: %s\n", build_linkflags);
  fprintf(fp, "build system: %s\n", build_system);
  fprintf(fp, "\nDirectories:\n%s\n\n", rule.c_str());
  fprintf(fp, "tempdir: %s\n", BKE_tempdir_session());

  if (G.background) {
    fprintf(fp, "\nGPU: missing, background mode\n");
  }
  else {
    fprintf(fp, "\nGPU:\n%s\n\n", rule.c_str());
    fprintf(fp, "renderer:\t%s\n", GPU_platform_renderer());
    fprintf(fp, "vendor:\t\t%s\n", GPU_platform_vendor());
    fprintf(fp, "version:\t%s\n", GPU_platform_version());
    fprintf(fp, "device type:\t%s\n", GPU_platform_gpu_name());
    const char *backend = "UNKNOWN";
    switch (GPU_backend_get_type()) {
      case GPU_BACKEND_OPENGL:
        backend = "OPENGL";
        break;
      case GPU_BACKEND_METAL:
        backend = "METAL";
        break;
      case GPU_BACKEND_VULKAN:
        backend = "VULKAN";
        break;
      case GPU_BACKEND_NONE:
        backend = "NONE";
        break;
      default:
        break;
    }
    fprintf(fp, "backend type:\t%s\n", backend);
    fprintf(fp, "\nImplementation Dependent GPU Limits:\n%s\n\n", rule.c_str());
    fprintf(fp, "Maximum Batch Vertices:\t%d\n", GPU_max_batch_vertices());
    fprintf(fp, "Maximum Batch Indices:\t%d\n", GPU_max_batch_indices());
    fprintf(fp, "Maximum Vertex Attributes:\t%d\n", GPU_max_vertex_attribs());
    fprintf(fp, "Maximum Vertex Uniform Components:\t%d\n", GPU_max_uniforms_vert());
    fprintf(fp, "Maximum Fragment Uniform Components:\t%d\n", GPU_max_uniforms_frag());
    fprintf(fp, "Maximum Vertex Image Units:\t%d\n", GPU_max_textures_vert());
    fprintf(fp, "Maximum Fragment Image Units:\t%d\n", GPU_max_textures_frag());
    fprintf(fp, "Maximum Pipeline Image Units:\t%d\n", GPU_max_textures());
    fprintf(fp, "Maximum Image Units:\t%d\n", GPU_max_images());
  }

  if (fclose(fp) != 0) {
    BKE_reportf(op->reports, RPT_ERROR, "Unable to finish writing '%s'", filepath);
    return OPERATOR_CANCELLED;
  }
  return OPERATOR_FINISHED;
}

static wmOperatorStatus sysinfo_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);
  if (filepath[0] == '\0') {
    BLI_path_join(filepath, sizeof(filepath), BLI_dir_home(), "system-info.txt");
    RNA_string_set(op->ptr, "filepath", filepath);
  }
  WM_event_add_fileselect(C, op);
  return OPERATOR_RUNNING_MODAL;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name wm.blenderplayer_start
 * \{ */

static bool wait_for_process(const std::vector<std::string> &args)
{
  std::vector<char *> argv;
  argv.reserve(args.size() + 1);
  for (const std::string &arg : args) {
    argv.push_back(const_cast<char *>(arg.c_str()));
  }
  argv.push_back(nullptr);

  pid_t pid = 0;
  const int spawn_result = posix_spawn(&pid, argv[0], nullptr, nullptr, argv.data(), environ);
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
}

static std::string player_path()
{
  char path[FILE_MAX];
  STRNCPY(path, BKE_appdir_program_path());
#ifdef __APPLE__
  for (int i = 0; i < 4; i++) {
    BLI_path_parent_dir(path);
  }
  BLI_path_append(path, sizeof(path), "Blenderplayer.app/Contents/MacOS/Blenderplayer");
  if (!BLI_exists(path)) {
    STRNCPY(path, "/Applications/UPBGE-0.3-Alpha/Blenderplayer.app/Contents/MacOS/Blenderplayer");
  }
#else
  const char *extension = BLI_path_extension(path);
  BLI_path_parent_dir(path);
  BLI_path_append(path, sizeof(path), extension ? "blenderplayer" : "blenderplayer");
  if (extension) {
    BLI_path_extension_ensure(path, sizeof(path), extension);
  }
#endif
  return path;
}

static wmOperatorStatus blenderplayer_start_exec(bContext *C, wmOperator *op)
{
  const std::string player = player_path();
  if (!BLI_exists(player.c_str())) {
    BKE_reportf(op->reports, RPT_ERROR, "Player path: '%s' not found", player.c_str());
    return OPERATOR_CANCELLED;
  }

  Main *bmain = CTX_data_main(C);
  char filepath[FILE_MAX];
  if (bmain->filepath[0] != '\0') {
    BLI_snprintf(filepath, sizeof(filepath), "%s~", bmain->filepath);
  }
  else {
    BLI_path_join(filepath, sizeof(filepath), BKE_tempdir_session(), "game.blend");
  }

  PointerRNA props;
  WM_operator_properties_create(&props, "WM_OT_save_as_mainfile");
  RNA_string_set(&props, "filepath", filepath);
  RNA_boolean_set(&props, "copy", true);
  const wmOperatorStatus saved = WM_operator_name_call(
      C, "WM_OT_save_as_mainfile", WM_OP_EXEC_DEFAULT, &props, nullptr);
  WM_operator_properties_free(&props);
  if (!(saved & OPERATOR_FINISHED)) {
    return OPERATOR_CANCELLED;
  }

  const GameData &game = CTX_data_scene(C)->gm;
  auto enabled = [&](const int flag) { return (game.flag & flag) ? "1" : "0"; };
  const std::vector<std::string> args = {
      player,
      "-g", "show_framerate", "=", enabled(GAME_SHOW_FRAMERATE),
      "-g", "show_profile", "=", enabled(GAME_SHOW_FRAMERATE),
      "-g", "show_properties", "=", enabled(GAME_SHOW_DEBUG_PROPS),
      "-g", "ignore_deprecation_warnings", "=", enabled(GAME_IGNORE_DEPRECATION_WARNINGS),
      filepath,
  };
  const bool ran = wait_for_process(args);
  BLI_delete(filepath, false, false);
  if (!ran) {
    BKE_reportf(op->reports, RPT_ERROR, "Player failed: %s", strerror(errno));
    return OPERATOR_CANCELLED;
  }
  return OPERATOR_FINISHED;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name wm.operator_cheat_sheet
 * \{ */

static wmOperatorStatus operator_cheat_sheet_exec(bContext *C, wmOperator *op)
{
  std::vector<wmOperatorType *> operators(WM_operatortypes_registered_get().begin(),
                                           WM_operatortypes_registered_get().end());
  std::sort(operators.begin(), operators.end(), [](const wmOperatorType *a, const wmOperatorType *b) {
    return strcmp(a->idname, b->idname) < 0;
  });

  std::string output = "# " + std::to_string(operators.size()) + " Operators\n\n";
  for (const wmOperatorType *ot : operators) {
    char py_idname[OP_MAX_TYPENAME];
    WM_operator_py_idname(py_idname, ot->idname);
    output += "bpy.ops.";
    output += py_idname;
    output += '(';
    PointerRNA ptr = RNA_pointer_create_discrete(nullptr, ot->srna, nullptr);
    bool first = true;
    RNA_STRUCT_BEGIN_SKIP_RNA_TYPE (&ptr, prop) {
      if (!first) {
        output += ", ";
      }
      output += RNA_property_identifier(prop);
      output += "=...";
      first = false;
    }
    RNA_STRUCT_END;
    output += ")\n";
    if (ot->description != nullptr) {
      output += "    ";
      output += ot->description;
      output += '\n';
    }
    output += '\n';
  }

  Text *text = BKE_text_add(CTX_data_main(C), "OperatorList.txt");
  BKE_text_write(text, output.c_str(), int(output.size()));
  BKE_report(op->reports, RPT_INFO, "See OperatorList.txt text block");
  return OPERATOR_FINISHED;
}

/** \} */

}  // namespace

void WM_OT_sysinfo(wmOperatorType *ot)
{
  ot->name = "Save System Info";
  ot->idname = "WM_OT_sysinfo";
  ot->description = "Generate system information, saved into a text file";
  ot->exec = sysinfo_exec;
  ot->invoke = sysinfo_invoke;

  PropertyRNA *prop = RNA_def_string(ot->srna, "filepath", nullptr, 0, "filepath", "");
  RNA_def_property_subtype(prop, PROP_FILEPATH);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

void WM_OT_blenderplayer_start(wmOperatorType *ot)
{
  ot->name = "Start Game In Player";
  ot->idname = "WM_OT_blenderplayer_start";
  ot->description = "Launch the blender-player with the current blend-file";
  ot->exec = blenderplayer_start_exec;
}

void WM_OT_operator_cheat_sheet(wmOperatorType *ot)
{
  ot->name = "Operator Cheat Sheet";
  ot->idname = "WM_OT_operator_cheat_sheet";
  ot->description = "List all the operators in a text-block, useful for scripting";
  ot->exec = operator_cheat_sheet_exec;
}
