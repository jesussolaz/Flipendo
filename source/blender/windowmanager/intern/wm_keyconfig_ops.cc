/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Activacion nativa de configuraciones de teclado. La configuracion integrada
 * "Blender" se reconstruye siempre desde flipendo::keymap; los scripts Python
 * quedan exclusivamente como compatibilidad para configuraciones externas del
 * usuario, que no forman parte del codigo distribuido de Flipendo.
 */

#include <cstddef>
#include <string>

#include "DNA_windowmanager_types.h"

#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"

#include "BKE_context.hh"
#include "BKE_report.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_keymap.hh"
#include "WM_types.hh"

#ifdef WITH_PYTHON
#  include "BPY_extern_run.hh"
#endif

namespace {

std::string keyconfig_name_from_path(const char *filepath)
{
  const char *basename = BLI_path_basename(filepath);
  std::string name = basename ? basename : "";
  const size_t extension = name.find_last_of('.');
  if (extension != std::string::npos) {
    name.resize(extension);
  }
  return name;
}

wmKeyConfig *keyconfig_find(wmWindowManager *wm, const char *name)
{
  return static_cast<wmKeyConfig *>(
      BLI_findstring(&wm->keyconfigs, name, offsetof(wmKeyConfig, idname)));
}

wmOperatorStatus keyconfig_activate_exec(bContext *C, wmOperator *op)
{
  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);

  wmWindowManager *wm = CTX_wm_manager(C);
  if (wm == nullptr || wm->defaultconf == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "Window manager has no default key configuration");
    return OPERATOR_CANCELLED;
  }

  const std::string name = keyconfig_name_from_path(filepath);
  if (name.empty()) {
    BKE_report(op->reports, RPT_ERROR, "Key configuration path has no name");
    return OPERATOR_CANCELLED;
  }

  if (name == WM_KEYCONFIG_STR_DEFAULT) {
    /* Esta es la ruta importante: el desplegable ya no ejecuta Blender.py. Las
     * preferencias se leen de UserDef dentro de register_default(). */
    WM_keyconfig_reload(C);
    WM_keyconfig_set_active(wm, WM_KEYCONFIG_STR_DEFAULT);
    return OPERATOR_FINISHED;
  }

  /* Una configuracion personalizada importada en esta sesion ya esta en memoria;
   * cambiar entre ella y Blender tampoco necesita volver a ejecutar su fichero. */
  if (keyconfig_find(wm, name.c_str()) != nullptr) {
    WM_keyconfig_set_active(wm, name.c_str());
    return OPERATOR_FINISHED;
  }

#ifdef WITH_PYTHON
  /* Compatibilidad con configuraciones .py creadas por versiones anteriores. No
   * es codigo distribuido por Flipendo y no se usa para la configuracion integrada.
   * Se conserva para no inutilizar los ficheros del usuario durante la migracion a
   * un formato nativo de intercambio. */
  wmKeyConfig *old = keyconfig_find(wm, name.c_str());
  if (BLI_path_extension_check(filepath, ".py") && BPY_run_filepath(C, filepath, op->reports)) {
    wmKeyConfig *loaded = keyconfig_find(wm, name.c_str());
    if (loaded != nullptr) {
      WM_keyconfig_set_active(wm, name.c_str());
      return OPERATOR_FINISHED;
    }
  }
  wmKeyConfig *loaded = keyconfig_find(wm, name.c_str());
  if (loaded != nullptr && loaded != old) {
    WM_keyconfig_remove(wm, loaded);
  }
#endif

  BKE_reportf(op->reports,
              RPT_ERROR,
              "Key configuration '%s' is neither native nor loaded",
              name.c_str());
  return OPERATOR_CANCELLED;
}

}  // namespace

void PREFERENCES_OT_keyconfig_activate(wmOperatorType *ot)
{
  ot->name = "Activate Keyconfig";
  ot->idname = "PREFERENCES_OT_keyconfig_activate";
  ot->exec = keyconfig_activate_exec;

  /* Contrato identico al StringProperty(subtype='FILE_PATH') del operador Python. */
  RNA_def_string_file_path(ot->srna, "filepath", nullptr, 0, "filepath", "");
}
