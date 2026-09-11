/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * `--fl-selftest-theme-xml` / `--fl-check-theme-xml`: la prueba del `rna_xml` nativo.
 *
 * La idea es que el volcado de cada caso ES el XML del tema. Así una sola comparación
 * cubre las dos mitades a la vez:
 *
 * - el **escritor** se compara byte a byte (caso 0: escribir el tema de fábrica);
 * - el **lector** se compara por su efecto (casos 1 a 3: leer un tema y volver a
 *   escribirlo; si el lector se dejara un valor, el XML de salida lo cantaría).
 *
 * La línea base se captura con el mismo guion pero llamando a `rna_xml.py`, contra el
 * binario que todavía lo tiene.
 */

#include "FL_rna_xml.hpp"

#include <cstdio>
#include <string>

#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "BKE_appdir.hh"
#include "BKE_context.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "FL_selftest_compare.hh"

namespace flipendo::rna_xml::selftest {

namespace {

const MapEntry theme_map[] = {
    {"preferences.themes[0]", "Theme"},
    {"preferences.ui_styles[0]", "ThemeStyle"},
};

/**
 * `USERPREF_MT_interface_theme_presets.preset_xml_secure_types`, los 33 nombres, en el
 * mismo orden en que los declara `space_userpref.py`. Es el salvavidas que impide que un
 * XML ajeno se salga del tema hacia las preferencias.
 */
const char *secure_types[] = {
    "Theme",
    "ThemeAssetShelf",
    "ThemeBoneColorSet",
    "ThemeClipEditor",
    "ThemeCollectionColor",
    "ThemeConsole",
    "ThemeDopeSheet",
    "ThemeFileBrowser",
    "ThemeFontStyle",
    "ThemeGradientColors",
    "ThemeGraphEditor",
    "ThemeImageEditor",
    "ThemeInfo",
    "ThemeNLAEditor",
    "ThemeNodeEditor",
    "ThemeOutliner",
    "ThemePanelColors",
    "ThemePreferences",
    "ThemeProperties",
    "ThemeSequenceEditor",
    "ThemeSpaceGeneric",
    "ThemeSpaceGradient",
    "ThemeSpaceListGeneric",
    "ThemeSpreadsheet",
    "ThemeStatusBar",
    "ThemeStripColor",
    "ThemeStyle",
    "ThemeTextEditor",
    "ThemeTopBar",
    "ThemeUserInterface",
    "ThemeView3D",
    "ThemeWidgetColors",
    "ThemeWidgetStateColors",
};

void reset_theme(bContext *C)
{
  PointerRNA props;
  WM_operator_properties_create(&props, "PREFERENCES_OT_reset_default_theme");
  WM_operator_name_call(
      C, "PREFERENCES_OT_reset_default_theme", WM_OP_EXEC_DEFAULT, &props, nullptr);
  WM_operator_properties_free(&props);
}

/** Vuelca el XML del tema actual, línea a línea y numerado. */
void dump_theme(FILE *f, int &n, bContext *C)
{
  std::string text;
  std::string error;
  if (!write_string(C, blender::Span<MapEntry>(theme_map, ARRAY_SIZE(theme_map)), text, error)) {
    fprintf(f, "  %d ERROR=%s\n", n++, error.c_str());
    return;
  }
  size_t start = 0;
  while (start < text.size()) {
    const size_t end = text.find('\n', start);
    const size_t stop = (end == std::string::npos) ? text.size() : end;
    fprintf(f, "  %d %s\n", n++, text.substr(start, stop - start).c_str());
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  /* Se vacía el búfer en cada caso: si un caso posterior se cae, lo ya volcado tiene que
   * estar en el disco. Sin esto, un fallo en el caso 1 truncaba el caso 0 y parecía que
   * el escritor estaba mal. */
  fflush(f);
}

/** La ruta de un tema distribuido: `<scripts del sistema>/presets/interface_theme/X`. */
std::string bundled_theme(const char *filename)
{
  const std::optional<std::string> base = BKE_appdir_folder_id(BLENDER_SYSTEM_SCRIPTS,
                                                               "presets/interface_theme");
  if (!base) {
    return "";
  }
  char path[FILE_MAX];
  BLI_path_join(path, sizeof(path), base->c_str(), filename);
  return path;
}

void run_cases(bContext *C, FILE *f)
{
  const blender::Span<MapEntry> map(theme_map, ARRAY_SIZE(theme_map));
  const blender::Span<const char *> secure(secure_types, ARRAY_SIZE(secure_types));

  /* --- 0: escribir el tema de fábrica. Prueba el ESCRITOR, byte a byte. --------- */
  {
    reset_theme(C);
    fprintf(f, "case=0 escribir el tema de fabrica\n");
    int n = 0;
    dump_theme(f, n, C);
  }

  /* --- 1: leer el tema claro y volver a escribirlo. Prueba el LECTOR. ----------- */
  {
    reset_theme(C);
    const std::string path = bundled_theme("Blender_Light.xml");
    std::string error;
    const bool ok = run_file(C, path, map, secure, error);
    fprintf(f, "case=1 leer Blender_Light.xml y reescribir\n");
    int n = 0;
    fprintf(f, "  %d ok=%d\n", n++, ok ? 1 : 0);
    dump_theme(f, n, C);
  }

  /* --- 2: el tema oscuro, que es un XML VACÍO a propósito. ---------------------- */
  {
    reset_theme(C);
    const std::string path = bundled_theme("Blender_Dark.xml");
    std::string error;
    const bool ok = run_file(C, path, map, secure, error);
    fprintf(f, "case=2 leer Blender_Dark.xml (vacio) y reescribir\n");
    int n = 0;
    fprintf(f, "  %d ok=%d\n", n++, ok ? 1 : 0);
    dump_theme(f, n, C);
  }

  /* --- 3: el salvavidas. Con sólo "Theme" permitido, no se entra en ningún
   *        sub-tipo y el tema tiene que quedarse como estaba. -------------------- */
  {
    reset_theme(C);
    const char *only_theme[] = {"Theme"};
    const std::string path = bundled_theme("Blender_Light.xml");
    std::string error;
    const bool ok = run_file(
        C, path, map, blender::Span<const char *>(only_theme, ARRAY_SIZE(only_theme)), error);
    fprintf(f, "case=3 leer Blender_Light.xml con secure_types={Theme}\n");
    int n = 0;
    fprintf(f, "  %d ok=%d\n", n++, ok ? 1 : 0);
    dump_theme(f, n, C);
  }

  reset_theme(C);
}

}  // namespace

bool dump(bContext *C, const char *filepath)
{
  FILE *f = fopen(filepath, "w");
  if (f == nullptr) {
    fprintf(stderr, "fl-selftest-theme-xml: no se pudo escribir '%s'\n", filepath);
    return false;
  }
  fprintf(f, "# FL-THEME-XML v1\n");
  run_cases(C, f);
  fclose(f);
  fprintf(stderr, "fl-selftest-theme-xml: volcado en '%s'\n", filepath);
  return true;
}

bool check(bContext *C, const char *baseline_path)
{
  char actual_path[FILE_MAX];
  BLI_path_join(actual_path, sizeof(actual_path), BKE_tempdir_session(), "fl-theme-xml-actual.txt");
  if (!dump(C, actual_path)) {
    return false;
  }
  return flipendo::selftest::compare_to_baseline("fl-check-theme-xml", actual_path, baseline_path);
}

}  // namespace flipendo::rna_xml::selftest
