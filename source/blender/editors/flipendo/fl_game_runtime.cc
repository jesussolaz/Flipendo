/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 *
 * Exportar el juego como ejecutable autonomo, en C++ nativo.
 *
 * Migracion de `scripts/addons_core/game_engine_save_as_runtime_eevee.py`
 * (416 lineas, UPBGE; autores Mitchell Stokes, Ulysse Martin, Jorge Bernal).
 * Mismo `idname` (`wm.save_as_runtime`), mismo rotulo, mismas propiedades con
 * sus mismos nombres, descripciones y valores por defecto.
 *
 * El formato del entregable NO se inventa aqui, se conserva:
 *
 * - Player que es un bundle `.app` (macOS): se copia el bundle entero y el
 *   `.blend` del juego se deja dentro como `Contents/Resources/game.blend`.
 *   Es lo que busca `GPG_ghost.cpp:577-610` al arrancar.
 * - Player que es un ejecutable suelto: se concatena `player || blend ||
 *   offset(4 bytes, big-endian) || "BRUNTIME"`, que es exactamente lo que
 *   lee `BLO_is_a_runtime()` / `BLO_read_runtime()`
 *   (`blenloader/intern/runtime.cc:66-135`).
 *
 * Doctrina: `politicas/LENGUAJE-CPP.md`, `politicas/ADDONS-MOTOR-A-CPP.md`.
 */

#include <cstdio>
#include <cstring>
#include <string>

#include <sys/stat.h>
#ifndef WIN32
#  include <dirent.h>
#  include <unistd.h>
#endif

#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_vector.hh"

#include "BLT_translation.hh"

#include "DNA_object_types.h"
#include "DNA_property_types.h"

#include "BKE_appdir.hh"
#include "BKE_context.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"

#include "BLO_readfile.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "FL_game_runtime.hh"

namespace flipendo::game {

/* -------------------------------------------------------------------- */
/** \name Utilidades de ruta
 * \{ */

/** Quita `count` componentes del final de `path`. */
static void path_strip_components(char *path, int count)
{
  for (int i = 0; i < count; i++) {
    BLI_path_slash_rstrip(path);
    char *slash = (char *)BLI_path_slash_rfind(path);
    if (slash == nullptr) {
      return;
    }
    *slash = '\0';
  }
}

/**
 * Ruta por defecto del Player, la misma que calculaba el Python.
 *
 * macOS: `bpy.app.binary_path.split('/')[0:-4]` + `Blenderplayer.app`, es decir
 * el directorio que contiene a `Blender.app`. Resto: el directorio del binario
 * + `blenderplayer` + la extension del propio binario.
 */
void default_player_path_get(char r_path[FILE_MAX])
{
  char dir[FILE_MAX];
  STRNCPY(dir, BKE_appdir_program_path());

#ifdef __APPLE__
  /* <dir>/Blender.app/Contents/MacOS/Blender -> <dir> */
  path_strip_components(dir, 4);
  BLI_path_join(r_path, FILE_MAX, dir, "Blenderplayer.app");
#else
  const char *ext = BLI_path_extension(dir);
  char name[64];
  SNPRINTF(name, "blenderplayer%s", ext ? ext : "");
  path_strip_components(dir, 1);
  BLI_path_join(r_path, FILE_MAX, dir, name);
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Escritura del entregable
 * \{ */

/**
 * Guarda una copia del `.blend` actual en `filepath`, sin comprimir y sin
 * reescribir rutas relativas, y sin tocar el fichero activo.
 *
 * Reproduce literalmente la llamada del addon:
 * `bpy.ops.wm.save_as_mainfile(filepath=..., relative_remap=False,
 * compress=False, copy=True)`. Se pasa por el operador y no por
 * `BKE_blendfile_write` a proposito: asi el `.blend` que sale es el mismo
 * fichero que salia por el camino de Python, con los mismos avisos y el mismo
 * versionado.
 */
static bool write_blend_copy(bContext *C, const char *filepath, ReportList *reports)
{
  wmOperatorType *ot = WM_operatortype_find("WM_OT_save_as_mainfile", false);
  if (ot == nullptr) {
    BKE_report(reports, RPT_ERROR, "wm.save_as_mainfile no esta registrado");
    return false;
  }

  PointerRNA props;
  WM_operator_properties_create_ptr(&props, ot);
  RNA_string_set(&props, "filepath", filepath);
  RNA_boolean_set(&props, "relative_remap", false);
  RNA_boolean_set(&props, "compress", false);
  RNA_boolean_set(&props, "copy", true);

  const wmOperatorStatus status = WM_operator_name_call_ptr(
      C, ot, WM_OP_EXEC_DEFAULT, &props, nullptr);
  WM_operator_properties_free(&props);

  if ((status & OPERATOR_FINISHED) == 0) {
    BKE_reportf(reports, RPT_ERROR, "No se pudo escribir '%s'", filepath);
    return false;
  }
  return BLI_exists(filepath) != 0;
}

/**
 * Player en bundle `.app`: copiar el bundle y meter el juego dentro.
 *
 * Trampa heredada, y aqui corregida: el Python hacia
 * `os.system('cp -R "src" "dst"')`. Si `dst` ya existia, `cp -R` copia DENTRO
 * y sale un `Juego.app/Blenderplayer.app` que no arranca. Aqui se borra el
 * destino primero, que es lo que ya hacia el otro addro (`game_engine_publishing.py`
 * usaba `shutil.rmtree` antes de `copytree`). Queda escrito en el commit.
 */
static bool write_runtime_app(bContext *C,
                              const char *player_path,
                              const char *output_path,
                              ReportList *reports)
{
  if (BLI_exists(output_path)) {
    BLI_delete(output_path, true, true);
  }

  if (BLI_copy(player_path, output_path) != 0) {
    BKE_reportf(reports, RPT_ERROR, "No se pudo copiar el Player a '%s'", output_path);
    return false;
  }

  char blend_path[FILE_MAX];
  BLI_path_join(blend_path, FILE_MAX, output_path, "Contents", "Resources", "game.blend");
  /* Misma ruta que `FL_GAME_BLEND_REL`, la que lee `GPG_ghost.cpp:604`. */

  char blend_dir[FILE_MAX];
  BLI_path_split_dir_part(blend_path, blend_dir, FILE_MAX);
  BLI_dir_create_recursive(blend_dir);

  return write_blend_copy(C, blend_path, reports);
}

/**
 * Player que es un ejecutable suelto: `player || blend || offset || "BRUNTIME"`.
 * El offset son 4 bytes en orden de mayor a menor peso, como los lee
 * `handle_read_msb_int()` en `blenloader/intern/runtime.cc:57`.
 */
static bool write_runtime_single(bContext *C,
                                 const char *player_path,
                                 const char *output_path,
                                 ReportList *reports)
{
  /* El .blend intermedio va al directorio temporal de la sesion, igual que el
   * `tempfile.mkdtemp()` del Python. */
  char blend_path[FILE_MAX];
  BLI_path_join(blend_path, FILE_MAX, BKE_tempdir_session(), "fl_runtime_payload.blend");
  if (!write_blend_copy(C, blend_path, reports)) {
    return false;
  }

  FILE *player = BLI_fopen(player_path, "rb");
  if (player == nullptr) {
    BKE_reportf(reports, RPT_ERROR, "No se pudo abrir el Player '%s'", player_path);
    return false;
  }
  FILE *out = BLI_fopen(output_path, "wb");
  if (out == nullptr) {
    fclose(player);
    BKE_reportf(reports, RPT_ERROR, "No se pudo escribir '%s'", output_path);
    return false;
  }

  char buf[1 << 16];
  size_t offset = 0;
  size_t len;
  while ((len = fread(buf, 1, sizeof(buf), player)) > 0) {
    fwrite(buf, 1, len, out);
    offset += len;
  }
  fclose(player);

  FILE *blend = BLI_fopen(blend_path, "rb");
  if (blend == nullptr) {
    fclose(out);
    BKE_reportf(reports, RPT_ERROR, "No se pudo leer el .blend intermedio '%s'", blend_path);
    return false;
  }
  while ((len = fread(buf, 1, sizeof(buf), blend)) > 0) {
    fwrite(buf, 1, len, out);
  }
  fclose(blend);
  BLI_delete(blend_path, false, false);

  const unsigned char offset_msb[4] = {
      (unsigned char)((offset >> 24) & 0xFF),
      (unsigned char)((offset >> 16) & 0xFF),
      (unsigned char)((offset >> 8) & 0xFF),
      (unsigned char)((offset >> 0) & 0xFF),
  };
  fwrite(offset_msb, 1, 4, out);
  fwrite("BRUNTIME", 1, 8, out);
  fclose(out);

#ifndef WIN32
  chmod(output_path, 0755);
#endif
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name `wm.save_as_runtime`
 * \{ */

static wmOperatorStatus save_as_runtime_exec(bContext *C, wmOperator *op)
{
  char player_path[FILE_MAX];
  char output_path[FILE_MAX];
  RNA_string_get(op->ptr, "player_path", player_path);
  RNA_string_get(op->ptr, "filepath", output_path);

  if (output_path[0] == '\0') {
    BKE_report(op->reports, RPT_ERROR, "No se indico donde guardar el ejecutable");
    return OPERATOR_CANCELLED;
  }

  const bool player_is_app = BLI_path_extension_check(player_path, ".app");
  if (!BLI_exists(player_path) || (BLI_is_dir(player_path) && !player_is_app)) {
    /* Mismo texto que el addon, para no perder el mensaje traducible. */
    BKE_report(op->reports, RPT_ERROR, "The player could not be found! Runtime not saved");
    return OPERATOR_CANCELLED;
  }

  bool ok;
  if (player_is_app) {
    BLI_path_extension_ensure(output_path, FILE_MAX, ".app");
    ok = write_runtime_app(C, player_path, output_path, op->reports);
  }
  else {
    if (BLI_path_extension_check(player_path, ".exe")) {
      BLI_path_extension_ensure(output_path, FILE_MAX, ".exe");
    }
    ok = write_runtime_single(C, player_path, output_path, op->reports);
  }

  if (!ok) {
    return OPERATOR_CANCELLED;
  }

  BKE_reportf(op->reports, RPT_INFO, "Runtime guardado en \"%s\"", output_path);
  printf("FL-RUNTIME saved %s\n", output_path);
  return OPERATOR_FINISHED;
}

static wmOperatorStatus save_as_runtime_invoke(bContext *C,
                                               wmOperator *op,
                                               const wmEvent * /*event*/)
{
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "filepath");
  char filepath[FILE_MAX];
  RNA_property_string_get(op->ptr, prop, filepath);

  if (filepath[0] == '\0') {
    /* `bpy.path.ensure_ext(bpy.data.filepath, ext)` del addon. */
    const Main *bmain = CTX_data_main(C);
    STRNCPY(filepath, bmain->filepath);
#ifdef __APPLE__
    BLI_path_extension_ensure(filepath, FILE_MAX, ".app");
#else
    const char *ext = BLI_path_extension(BKE_appdir_program_path());
    if (ext) {
      BLI_path_extension_ensure(filepath, FILE_MAX, ext);
    }
#endif
    RNA_property_string_set(op->ptr, prop, filepath);
  }

  WM_event_add_fileselect(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static void WM_OT_save_as_runtime(wmOperatorType *ot)
{
  /* TRAMPA: `RNA_def_string()` guarda el PUNTERO del valor por defecto, no una
   * copia (`rna_define.cc:2215`). Con un buffer de pila la propiedad quedaba
   * apuntando a memoria muerta. Por eso es `static`. */
  static char player_path[FILE_MAX] = {0};
  if (player_path[0] == '\0') {
    default_player_path_get(player_path);
  }

  /* Rotulo identico al `bl_label` del addon. El addon no declaraba
   * `bl_description`, asi que la descripcion queda vacia igual que antes. */
  ot->name = "Save As Game Engine Runtime";
  ot->idname = "WM_OT_save_as_runtime";
  /* Sin `description`: el addon no declaraba `bl_description`, asi que RNA ponia
   * "(undocumented operator)". Se conserva tal cual. */

  ot->invoke = save_as_runtime_invoke;
  ot->exec = save_as_runtime_exec;

  /* `bl_options = {'REGISTER'}`: sin UNDO, sin INTERNAL. */
  ot->flag = OPTYPE_REGISTER;

  PropertyRNA *prop;

  prop = RNA_def_string(ot->srna,
                        "player_path",
                        player_path,
                        FILE_MAX,
                        "Player Path",
                        "The path to the player to use");
  RNA_def_property_subtype(prop, PROP_FILEPATH);

  /* El `bl_label` que RNA deduce de un `StringProperty` sin nombre es el propio
   * identificador, en minusculas: asi salia en el addon y asi se deja. */
  prop = RNA_def_string(ot->srna, "filepath", nullptr, FILE_MAX, "filepath", "");
  RNA_def_property_subtype(prop, PROP_FILEPATH);

  RNA_def_boolean(
      ot->srna, "copy_python", true, "Copy Python", "Copy bundle Python with the runtime");
  RNA_def_boolean(ot->srna,
                  "overwrite_lib",
                  false,
                  "Overwrite 'lib' folder",
                  "Overwrites the lib folder (if one exists) with the bundled Python lib folder");
  RNA_def_boolean(ot->srna,
                  "copy_scripts",
                  false,
                  "Copy Scripts folder",
                  "Copy bundle Scripts folder with the runtime");
  RNA_def_boolean(ot->srna,
                  "copy_datafiles",
                  true,
                  "Copy Datafiles folder",
                  "Copy bundle datafiles folder with the runtime");
  RNA_def_boolean(ot->srna,
                  "copy_modules",
                  true,
                  "Copy Script>Modules folder",
                  "Copy bundle modules folder with the runtime");
  RNA_def_boolean(ot->srna,
                  "copy_logic_nodes",
                  true,
                  "Copy Logic Nodes game folder",
                  "Copy Logic Nodes game with the runtime");

#ifdef WIN32
  /* El addon solo declaraba estas dos cuando la extension del binario era `.exe`. */
  RNA_def_boolean(ot->srna, "copy_dlls", true, "Copy DLLs", "Copy all needed DLLs with the runtime");
  prop = RNA_def_string(ot->srna,
                        "new_icon_path",
                        "",
                        FILE_MAX,
                        "New Icon Path",
                        "The path to the new icon for player to use");
  RNA_def_property_subtype(prop, PROP_FILEPATH);
#else
  /* ...y esta solo cuando `os.name == 'posix'`, que en macOS es cierto. */
  RNA_def_boolean(ot->srna,
                  "copy_libs",
                  true,
                  "Copy shared libs",
                  "Copy all the needed executable shared libs with the runtime");
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Volcador de verificacion
 * \{ */

/** Ruta relativa del juego dentro del bundle del Player. */
static const char *FL_GAME_BLEND_REL = "Contents/Resources/game.blend";

struct DumpEntry {
  std::string path;
  size_t size;
  char kind; /* 'f' regular, 'l' enlace, 'd' directorio */
};

static void dump_walk(const std::string &root,
                      const std::string &rel,
                      blender::Vector<DumpEntry> &entries,
                      bool *r_ok)
{
#ifndef WIN32
  const std::string abs = rel.empty() ? root : root + "/" + rel;
  DIR *dir = opendir(abs.c_str());
  if (dir == nullptr) {
    *r_ok = false;
    return;
  }
  blender::Vector<std::string> subdirs;
  while (dirent *de = readdir(dir)) {
    if (STREQ(de->d_name, ".") || STREQ(de->d_name, "..")) {
      continue;
    }
    const std::string child_rel = rel.empty() ? std::string(de->d_name) :
                                                rel + "/" + de->d_name;
    const std::string child_abs = root + "/" + child_rel;
    struct stat st;
    if (lstat(child_abs.c_str(), &st) != 0) {
      *r_ok = false;
      continue;
    }
    if (S_ISLNK(st.st_mode)) {
      entries.append({child_rel, size_t(st.st_size), 'l'});
    }
    else if (S_ISDIR(st.st_mode)) {
      entries.append({child_rel, 0, 'd'});
      subdirs.append(child_rel);
    }
    else {
      entries.append({child_rel, size_t(st.st_size), 'f'});
    }
  }
  closedir(dir);
  for (const std::string &sub : subdirs) {
    dump_walk(root, sub, entries, r_ok);
  }
#else
  UNUSED_VARS(root, rel, entries);
  *r_ok = false;
#endif
}

/** Cuenta los objetos con propiedad de juego `fl_component` en un `.blend`. */
static void dump_components(const char *blend_path, FILE *out, int *r_count)
{
  *r_count = 0;
  ReportList reports;
  BKE_reports_init(&reports, RPT_STORE);
  BlendFileReadReport breports = {};
  breports.reports = &reports;

  BlendFileData *bfd = BLO_read_from_file(blend_path, BLO_READ_SKIP_USERDEF, &breports);
  if (bfd && bfd->main) {
    blender::Vector<std::string> lines;
    LISTBASE_FOREACH (Object *, ob, &bfd->main->objects) {
      LISTBASE_FOREACH (bProperty *, prop, &ob->prop) {
        if (!STREQ(prop->name, "fl_component")) {
          continue;
        }
        const char *value = (prop->type == GPROP_STRING && prop->poin) ? (const char *)prop->poin :
                                                                        "";
        lines.append(std::string(ob->id.name + 2) + " " + value);
      }
    }
    std::sort(lines.begin(), lines.end());
    for (const std::string &line : lines) {
      fprintf(out, "COMPONENT %s\n", line.c_str());
    }
    *r_count = int(lines.size());
  }
  if (bfd) {
    BLO_blendfiledata_free(bfd);
  }
  BKE_reports_free(&reports);
}

bool runtime_dump(const char *bundle_path, const char *filepath_out)
{
  FILE *out = stdout;
  if (filepath_out != nullptr) {
    out = BLI_fopen(filepath_out, "w");
    if (out == nullptr) {
      fprintf(stderr, "FL-RUNTIME-DUMP: no se pudo escribir '%s'\n", filepath_out);
      return false;
    }
  }

  std::string root = bundle_path;
  while (!root.empty() && root.back() == '/') {
    root.pop_back();
  }

  bool ok = true;
  blender::Vector<DumpEntry> entries;
  if (BLI_is_dir(root.c_str())) {
    dump_walk(root, "", entries, &ok);
  }
  else {
    struct stat st;
    if (BLI_exists(root.c_str()) && stat(root.c_str(), &st) == 0) {
      /* Ejecutable suelto: el entregable es un unico fichero. */
      entries.append({std::string(BLI_path_basename(root.c_str())), size_t(st.st_size), 'f'});
    }
    else {
      ok = false;
    }
  }

  std::sort(entries.begin(), entries.end(), [](const DumpEntry &a, const DumpEntry &b) {
    return a.path < b.path;
  });

  int files = 0, links = 0, dirs = 0;
  size_t bytes = 0;
  std::string payload;
  size_t payload_size = 0;
  for (const DumpEntry &e : entries) {
    switch (e.kind) {
      case 'f':
        files++;
        bytes += e.size;
        break;
      case 'l':
        links++;
        break;
      default:
        dirs++;
        break;
    }
    /* El juego SIEMPRE va en `Contents/Resources/game.blend`: es la ruta que
     * busca `GPG_ghost.cpp:604` al arrancar. Si no esta, se cae al `.blend` mas
     * grande, para que el volcador sirva tambien con bundles hechos a mano.
     * TRAMPA medida: sin esta preferencia el volcador elegia
     * `Contents/Resources/4.5/scripts/addons_core/bge_mixer/blender_data/tests/
     * test_data.blend` (881.212 bytes), mayor que el juego de ArpgNative
     * (421.331) y que no es el juego. */
    if (e.kind == 'f' && BLI_path_extension_check(e.path.c_str(), ".blend")) {
      const bool entry_is_game = (e.path == FL_GAME_BLEND_REL);
      const bool payload_is_game = (payload == FL_GAME_BLEND_REL);
      if (entry_is_game || (!payload_is_game && (payload.empty() || e.size > payload_size))) {
        payload = e.path;
        payload_size = e.size;
      }
    }
  }

  fprintf(out, "FL-RUNTIME-DUMP 1\n");
  fprintf(out, "BUNDLE %s\n", BLI_path_basename(root.c_str()));
  fprintf(out, "FILES %d\n", files);
  fprintf(out, "LINKS %d\n", links);
  fprintf(out, "DIRS %d\n", dirs);
  fprintf(out, "BYTES %zu\n", bytes);

  int components = 0;
  if (!payload.empty()) {
    fprintf(out, "PAYLOAD %s %zu\n", payload.c_str(), payload_size);
    const std::string payload_abs = root + "/" + payload;
    dump_components(payload_abs.c_str(), out, &components);
  }
  else {
    fprintf(out, "PAYLOAD - 0\n");
  }
  fprintf(out, "COMPONENTS %d\n", components);

  for (const DumpEntry &e : entries) {
    if (e.kind == 'd') {
      fprintf(out, "ENTRY %s dir\n", e.path.c_str());
    }
    else if (e.kind == 'l') {
      fprintf(out, "ENTRY %s link\n", e.path.c_str());
    }
    else {
      fprintf(out, "ENTRY %s %zu\n", e.path.c_str(), e.size);
    }
  }

  if (out != stdout) {
    fclose(out);
  }
  return ok;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registro
 * \{ */

void operatortypes_register()
{
  WM_operatortype_append(WM_OT_save_as_runtime);
  publishing_operatortypes_register();
  character_operatortypes_register();
  camera_cull_operatortypes_register();
}

/** \} */

}  // namespace flipendo::game
