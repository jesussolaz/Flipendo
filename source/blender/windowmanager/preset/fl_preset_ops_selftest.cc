/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * `--fl-selftest-preset-ops` / `--fl-check-preset-ops`: la prueba de CONDUCTA de los
 * operadores de presets migrados en `fl_preset_add_ops.cc`.
 *
 * `--fl-check-preset-optypes` compara la superficie de registro, que es barata y no
 * prueba nada de lo que el operador HACE. Esto si: escribe presets de verdad, los
 * aplica, los borra, y vuelca lo observable — el estado del operador, la etiqueta del
 * menu, si el fichero existe y su contenido byte a byte.
 *
 * La trampa del arnes, y la razon de que se pueda hacer en `--background`: el operador
 * escribe en la carpeta de scripts del USUARIO, que en una maquina de verdad tiene
 * presets del usuario dentro. Se evita con la variable de entorno
 * `BLENDER_USER_SCRIPTS`, que `BKE_appdir` respeta y que apunta a una carpeta temporal:
 * asi el arnes es reproducible y no toca nada de nadie. La linea base se capturo con el
 * binario que todavia tenia las clases de Python, con la misma variable apuntando a una
 * carpeta igual de vacia.
 *
 * Casos, y que prueba cada uno: ver #run_cases.
 */

#include "FL_preset.hpp"
#include "FL_preset_ui.hpp"

#include <cstdio>
#include <string>

#include "DNA_scene_types.h"

#include "BLI_fileops.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "MEM_guardedalloc.h"

#include "BKE_appdir.hh"
#include "BKE_context.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "FL_selftest_compare.hh"

namespace flipendo::preset::selftest {

namespace {

const char *status_name(const wmOperatorStatus status)
{
  if (status & OPERATOR_FINISHED) {
    return "FINISHED";
  }
  if (status & OPERATOR_CANCELLED) {
    return "CANCELLED";
  }
  if (status & OPERATOR_RUNNING_MODAL) {
    return "RUNNING_MODAL";
  }
  if (status & OPERATOR_INTERFACE) {
    return "INTERFACE";
  }
  return "PASS_THROUGH";
}

/** La carpeta donde el operador escribe: `<BLENDER_USER_SCRIPTS>/presets/render`. */
std::string user_render_dir()
{
  const std::optional<std::string> base = BKE_appdir_folder_id_user_notest(BLENDER_USER_SCRIPTS,
                                                                          "presets/render");
  return base ? *base : std::string();
}

/** Vuelca el contenido de un `.fpreset` linea a linea, numerado. */
void dump_file(FILE *f, int &n, const std::string &filepath)
{
  fprintf(f, "  %d exists=%d\n", n++, BLI_exists(filepath.c_str()) ? 1 : 0);
  size_t size = 0;
  void *buffer = BLI_file_read_text_as_mem(filepath.c_str(), 0, &size);
  if (buffer == nullptr) {
    return;
  }
  const std::string text(static_cast<const char *>(buffer), size);
  MEM_freeN(buffer);
  size_t start = 0;
  while (start < text.size()) {
    const size_t end = text.find('\n', start);
    const size_t stop = (end == std::string::npos) ? text.size() : end;
    fprintf(f, "  %d linea=%s\n", n++, text.substr(start, stop - start).c_str());
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
}

/** Llama a `render.preset_add` con TODAS sus propiedades puestas (trampa 1 de RIGIDBODY). */
wmOperatorStatus call_render_preset_add(bContext *C,
                                        const char *name,
                                        const bool remove_name,
                                        const bool remove_active)
{
  PointerRNA props;
  WM_operator_properties_create(&props, "RENDER_OT_preset_add");
  RNA_string_set(&props, "name", name);
  RNA_boolean_set(&props, "remove_name", remove_name);
  RNA_boolean_set(&props, "remove_active", remove_active);
  const wmOperatorStatus status = WM_operator_name_call(
      C, "RENDER_OT_preset_add", WM_OP_EXEC_DEFAULT, &props, nullptr);
  WM_operator_properties_free(&props);
  return status;
}

void set_render_values(bContext *C, const int fps, const int resolution_x)
{
  Scene *scene = CTX_data_scene(C);
  PointerRNA scene_ptr = RNA_id_pointer_create(&scene->id);
  PointerRNA render_ptr = RNA_pointer_get(&scene_ptr, "render");
  RNA_int_set(&render_ptr, "fps", fps);
  RNA_int_set(&render_ptr, "resolution_x", resolution_x);
}

void dump_render_values(FILE *f, int &n, bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  PointerRNA scene_ptr = RNA_id_pointer_create(&scene->id);
  PointerRNA render_ptr = RNA_pointer_get(&scene_ptr, "render");
  fprintf(f, "  %d fps=%d\n", n++, RNA_int_get(&render_ptr, "fps"));
  fprintf(f, "  %d resolution_x=%d\n", n++, RNA_int_get(&render_ptr, "resolution_x"));
}

/**
 * Los nombres que prueban `AddPresetBase.as_filename`, que es donde estaba la unica
 * transformacion de texto del operador: recorte, literales de caracteres que no caben
 * en un nombre de fichero, lista negra de puntuacion y recorte de guiones bajos.
 */
struct NameCase {
  const char *name;
  const char *expected_file;
};
const NameCase name_cases[] = {
    {"Mi Preset", "Mi_Preset.fpreset"},
    {"   espacios   ", "espacios.fpreset"},
    {"a:b+c/d", "a_colon_b_plus_c_slash_d.fpreset"},
    {"_raro_", "raro.fpreset"},
    {"x!y@z#w", "x_y_z_w.fpreset"},
    /* `;` SI esta en la lista negra (va detras de `"`), asi que cae y luego lo
     * recorta el `strip("_")`. */
    {"punto.y,coma;", "punto_y_coma.fpreset"},
    /* Nombre vacio: el original devuelve FINISHED y no escribe nada. */
    {"   ", ""},
};

void run_cases(bContext *C, FILE *f)
{
  const std::string dir = user_render_dir();
  int case_index = 0;

  /* --- Escribir: un preset por cada forma de nombre. ----------------------- */
  for (const NameCase &name_case : name_cases) {
    set_render_values(C, 48, 1234);
    const wmOperatorStatus status = call_render_preset_add(C, name_case.name, false, false);
    fprintf(f, "case=%d add name=[%s]\n", case_index++, name_case.name);
    int n = 0;
    fprintf(f, "  %d status=%s\n", n++, status_name(status));
    fprintf(f,
            "  %d label=%s\n",
            n++,
            ui::menu_label_get("RENDER_PT_format_presets").c_str());
    if (name_case.expected_file[0] == '\0') {
      /* Solo se comprueba que la carpeta no gane un fichero; el nombre no existe. */
      fprintf(f, "  %d sin_fichero\n", n++);
      continue;
    }
    char filepath[FILE_MAX];
    BLI_path_join(filepath, sizeof(filepath), dir.c_str(), name_case.expected_file);
    dump_file(f, n, filepath);
  }

  /* --- Aplicar: el preset escrito tiene que devolver los valores. ---------- */
  {
    char filepath[FILE_MAX];
    BLI_path_join(filepath, sizeof(filepath), dir.c_str(), "Mi_Preset.fpreset");
    set_render_values(C, 12, 99);
    PointerRNA props;
    WM_operator_properties_create(&props, "WM_OT_preset_apply");
    RNA_string_set(&props, "filepath", filepath);
    const wmOperatorStatus status = WM_operator_name_call(
        C, "WM_OT_preset_apply", WM_OP_EXEC_DEFAULT, &props, nullptr);
    WM_operator_properties_free(&props);

    fprintf(f, "case=%d apply Mi_Preset\n", case_index++);
    int n = 0;
    fprintf(f, "  %d status=%s\n", n++, status_name(status));
    dump_render_values(f, n, C);
  }

  /* --- Quitar por nombre. -------------------------------------------------- */
  {
    const wmOperatorStatus status = call_render_preset_add(C, "Mi Preset", true, false);
    fprintf(f, "case=%d remove_name Mi Preset\n", case_index++);
    int n = 0;
    fprintf(f, "  %d status=%s\n", n++, status_name(status));
    fprintf(f,
            "  %d label=%s\n",
            n++,
            ui::menu_label_get("RENDER_PT_format_presets").c_str());
    char filepath[FILE_MAX];
    BLI_path_join(filepath, sizeof(filepath), dir.c_str(), "Mi_Preset.fpreset");
    fprintf(f, "  %d exists=%d\n", n++, BLI_exists(filepath) ? 1 : 0);
  }

  /* --- Quitar por nombre VISIBLE (el que se ve en el menu). ---------------- */
  {
    /* `Mi_Preset` ya no esta; `a_colon_b_plus_c_slash_d` se ve como `a:b+c/d`. */
    const wmOperatorStatus status = call_render_preset_add(C, "a:b+c/d", true, false);
    fprintf(f, "case=%d remove_name por nombre visible\n", case_index++);
    int n = 0;
    fprintf(f, "  %d status=%s\n", n++, status_name(status));
    char filepath[FILE_MAX];
    BLI_path_join(filepath, sizeof(filepath), dir.c_str(), "a_colon_b_plus_c_slash_d.fpreset");
    fprintf(f, "  %d exists=%d\n", n++, BLI_exists(filepath) ? 1 : 0);
  }

  /* --- Quitar uno que no existe: CANCELLED y nada mas. --------------------- */
  {
    const wmOperatorStatus status = call_render_preset_add(C, "No Existe", true, false);
    fprintf(f, "case=%d remove_name inexistente\n", case_index++);
    int n = 0;
    fprintf(f, "  %d status=%s\n", n++, status_name(status));
  }

  /* --- Quitar uno de los que vienen con Flipendo: se tiene que negar. ------ */
  {
    const wmOperatorStatus status = call_render_preset_add(C, "HDTV 1080p", true, false);
    fprintf(f, "case=%d remove_name de un preset de fabrica\n", case_index++);
    int n = 0;
    fprintf(f, "  %d status=%s\n", n++, status_name(status));
  }

  /* --- `remove_active`: usa la etiqueta del menu como nombre. -------------- */
  {
    set_render_values(C, 30, 640);
    call_render_preset_add(C, "Activo", false, false);
    const std::string label_after_add = ui::menu_label_get("RENDER_PT_format_presets");
    const wmOperatorStatus status = call_render_preset_add(C, "", false, true);
    fprintf(f, "case=%d remove_active\n", case_index++);
    int n = 0;
    fprintf(f, "  %d label_tras_anadir=%s\n", n++, label_after_add.c_str());
    fprintf(f, "  %d status=%s\n", n++, status_name(status));
    fprintf(f,
            "  %d label=%s\n",
            n++,
            ui::menu_label_get("RENDER_PT_format_presets").c_str());
    char filepath[FILE_MAX];
    BLI_path_join(filepath, sizeof(filepath), dir.c_str(), "Activo.fpreset");
    fprintf(f, "  %d exists=%d\n", n++, BLI_exists(filepath) ? 1 : 0);
  }

  /* --- Limpiar lo que quede, para que la carpeta acabe como empezo. -------- */
  for (const NameCase &name_case : name_cases) {
    if (name_case.expected_file[0] == '\0') {
      continue;
    }
    char filepath[FILE_MAX];
    BLI_path_join(filepath, sizeof(filepath), dir.c_str(), name_case.expected_file);
    if (BLI_exists(filepath)) {
      BLI_delete(filepath, false, false);
    }
  }
}

}  // namespace

bool dump(bContext *C, const char *filepath)
{
  FILE *f = fopen(filepath, "w");
  if (f == nullptr) {
    fprintf(stderr, "fl-selftest-preset-ops: no se pudo escribir '%s'\n", filepath);
    return false;
  }
  fprintf(f, "# FL-PRESET-OPS v1\n");
  run_cases(C, f);
  fclose(f);
  fprintf(stderr, "fl-selftest-preset-ops: volcado en '%s'\n", filepath);
  return true;
}

bool check(bContext *C, const char *baseline_path)
{
  char actual_path[FILE_MAX];
  BLI_path_join(
      actual_path, sizeof(actual_path), BKE_tempdir_session(), "fl-preset-ops-actual.txt");
  if (!dump(C, actual_path)) {
    return false;
  }
  return flipendo::selftest::compare_to_baseline(
      "fl-check-preset-ops", actual_path, baseline_path);
}

}  // namespace flipendo::preset::selftest
