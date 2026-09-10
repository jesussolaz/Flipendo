/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Los dos operadores nativos que hacen el trabajo de los presets: aplicar uno y
 * escribir uno.
 *
 * Son nuevos, no la migracion de un `idname` existente, asi que no heredan
 * nombre de nadie. `script.execute_preset` y la familia `AddPreset*` siguen en
 * `bl_operators/presets.py` porque estan atados a los menus de `bl_ui` (a su
 * `bl_label`, que un operador nativo no puede escribir). Lo que ya no hacen es
 * el trabajo: leen, aplican y escriben desde aqui. Es el mismo puente que
 * bendice TOOLSYSTEM-A-CPP.md -- Python llamando a C++ -- y se va con `bl_ui`.
 */

#include "FL_preset.hpp"

#include <string>

#include "BLI_string.h"

#include "BKE_report.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

namespace {

wmOperatorStatus preset_apply_exec(bContext *C, wmOperator *op)
{
  char filepath[1024];
  RNA_string_get(op->ptr, "filepath", filepath);
  if (filepath[0] == '\0') {
    BKE_report(op->reports, RPT_ERROR, "Falta la ruta del preset");
    return OPERATOR_CANCELLED;
  }
  std::string error;
  if (!flipendo::preset::apply_file(C, filepath, error)) {
    BKE_report(op->reports, RPT_ERROR, error.c_str());
    return OPERATOR_CANCELLED;
  }
  return OPERATOR_FINISHED;
}

wmOperatorStatus preset_write_exec(bContext *C, wmOperator *op)
{
  char filepath[1024];
  char subdir[256];
  RNA_string_get(op->ptr, "filepath", filepath);
  RNA_string_get(op->ptr, "subdir", subdir);
  const bool use_focal_length = RNA_boolean_get(op->ptr, "use_focal_length");
  if (filepath[0] == '\0' || subdir[0] == '\0') {
    BKE_report(op->reports, RPT_ERROR, "Faltan la ruta o la familia del preset");
    return OPERATOR_CANCELLED;
  }
  std::string error;
  if (!flipendo::preset::write_preset(C, subdir, filepath, use_focal_length, error)) {
    BKE_report(op->reports, RPT_ERROR, error.c_str());
    return OPERATOR_CANCELLED;
  }
  return OPERATOR_FINISHED;
}

}  // namespace

void WM_OT_preset_apply(wmOperatorType *ot)
{
  ot->name = "Apply Preset";
  ot->idname = "WM_OT_preset_apply";
  ot->description = "Apply a preset file to the current context";

  ot->exec = preset_apply_exec;
  ot->flag = OPTYPE_INTERNAL;

  RNA_def_string_file_path(
      ot->srna, "filepath", nullptr, 1024, "Path", "Preset file to apply");
}

void WM_OT_preset_write(wmOperatorType *ot)
{
  ot->name = "Write Preset";
  ot->idname = "WM_OT_preset_write";
  ot->description = "Write the current values of a preset family to a preset file";

  ot->exec = preset_write_exec;
  ot->flag = OPTYPE_INTERNAL;

  RNA_def_string_file_path(
      ot->srna, "filepath", nullptr, 1024, "Path", "Preset file to write");
  RNA_def_string(ot->srna, "subdir", nullptr, 256, "Subdirectory", "Preset family");
  RNA_def_boolean(ot->srna,
                  "use_focal_length",
                  false,
                  "Include Focal Length",
                  "Include focal length into the preset");
}
