/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 *
 * Publicar el juego, en C++ nativo.
 *
 * Migracion de `scripts/addons_core/game_engine_publishing.py` (576 lineas,
 * UPBGE; autores Mitchell Stokes, Oren Titane).
 *
 * ESTADO MEDIDO DEL ORIGINAL: ese addon estaba MUERTO. Su `register()` llamaba
 * a `bpy.utils.register_module()`, que Blender borro en la 2.80; al activarlo
 * en 4.5 salta
 * `AttributeError: module 'bpy.utils' has no attribute 'register_module'`
 * y no registra ni una sola clase. Ademas su panel sondeaba
 * `scene.render.engine == "BLENDER_GAME"`, un motor que ya no existe, y sus
 * `PropertyGroup` usaban asignacion (`x = Property(...)`) en vez de anotacion,
 * que la 2.80 tambien dejo de aceptar. Llevaba roto desde 2019.
 *
 * QUE SE MIGRA Y QUE SE RETIRA
 *
 * Se migra la capacidad real: producir el entregable en un directorio de
 * salida, llevarse los assets al lado y empaquetarlo en un archivo. Vive en
 * `wm.publish_platforms`, con el mismo `idname` y con las mismas propiedades
 * (mismos nombres, descripciones y valores por defecto) que tenian repartidas
 * `PublishSettings`, `PlatformSettings` y `AssetPath`.
 *
 * Se retira, y se justifica en `politicas/ADDONS-MOTOR-A-CPP.md`:
 *
 * - `scene.publish_download_platforms`: descargaba builds de Blender de
 *   `download.blender.org` con `urllib` y las descomprimia. No es capacidad de
 *   Flipendo (fork de un solo objetivo: Mac Intel), pide red, y la URL que
 *   construye no existe para esta version.
 * - `scene.publish_auto_platforms`, `scene.publish_add_platform`,
 *   `scene.publish_remove_platform`, `scene.publish_add_assetpath`,
 *   `scene.publish_remove_assetpath`, `RENDER_UL_platforms`,
 *   `RENDER_UL_assets` y `PUBLISH_MT_platform_specials`: eran el andamiaje para
 *   editar una lista de plataformas ajenas. Flipendo publica para una sola.
 *
 * Doctrina: `politicas/LENGUAJE-CPP.md`, `politicas/ADDONS-MOTOR-A-CPP.md`.
 */

#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

#include <sys/stat.h>
#ifndef WIN32
#  include <dirent.h>
#endif

#include <zlib.h>

#include "BLI_fileops.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "BLT_translation.hh"

#include "BKE_appdir.hh"
#include "BKE_context.hh"
#include "BKE_main.hh"
#include "BKE_report.hh"
#include "BKE_screen.hh"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "UI_interface_icons.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "FL_game_runtime.hh"
#include "FL_ui_registry.hh"

namespace flipendo::game {

/* -------------------------------------------------------------------- */
/** \name Archivo ZIP
 *
 * El addon empaquetaba con `shutil.make_archive(..., 'zip')`, que en Python
 * PIERDE el bit de ejecucion (por eso el propio addon se fabricaba a mano el
 * `.tar.gz` de Linux "so we can handle permission bits"). Este escritor guarda
 * el modo Unix completo en los atributos externos de cada entrada, asi que el
 * `.zip` de macOS sale ya con el Player ejecutable y con los enlaces simbolicos
 * como enlaces. Es una mejora deliberada sobre el original.
 * \{ */

struct ZipEntry {
  std::string name;   /* Ruta relativa dentro del archivo. */
  uint32_t crc = 0;
  uint32_t csize = 0;
  uint32_t usize = 0;
  uint32_t offset = 0;
  uint16_t method = 0;
  uint32_t mode = 0;
};

static void zip_put16(FILE *f, uint16_t v)
{
  const unsigned char b[2] = {(unsigned char)(v & 0xFF), (unsigned char)((v >> 8) & 0xFF)};
  fwrite(b, 1, 2, f);
}

static void zip_put32(FILE *f, uint32_t v)
{
  const unsigned char b[4] = {(unsigned char)(v & 0xFF),
                              (unsigned char)((v >> 8) & 0xFF),
                              (unsigned char)((v >> 16) & 0xFF),
                              (unsigned char)((v >> 24) & 0xFF)};
  fwrite(b, 1, 4, f);
}

/** Comprime `src` con deflate crudo (sin cabecera zlib), como pide el formato ZIP. */
static bool zip_deflate(const blender::Vector<unsigned char> &src,
                        blender::Vector<unsigned char> &dst)
{
  z_stream strm = {};
  if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) !=
      Z_OK)
  {
    return false;
  }
  dst.resize(src.size() + (src.size() >> 8) + 128);
  strm.next_in = const_cast<unsigned char *>(src.data());
  strm.avail_in = uInt(src.size());
  strm.next_out = dst.data();
  strm.avail_out = uInt(dst.size());
  const int ret = deflate(&strm, Z_FINISH);
  const size_t written = dst.size() - strm.avail_out;
  deflateEnd(&strm);
  if (ret != Z_STREAM_END) {
    return false;
  }
  dst.resize(written);
  return true;
}

static void zip_collect(const std::string &root,
                        const std::string &rel,
                        blender::Vector<std::pair<std::string, char>> &out)
{
#ifndef WIN32
  const std::string abs = rel.empty() ? root : root + "/" + rel;
  DIR *dir = opendir(abs.c_str());
  if (dir == nullptr) {
    return;
  }
  blender::Vector<std::string> subdirs;
  while (dirent *de = readdir(dir)) {
    if (STREQ(de->d_name, ".") || STREQ(de->d_name, "..")) {
      continue;
    }
    const std::string child = rel.empty() ? std::string(de->d_name) : rel + "/" + de->d_name;
    struct stat st;
    if (lstat((root + "/" + child).c_str(), &st) != 0) {
      continue;
    }
    if (S_ISLNK(st.st_mode)) {
      out.append({child, 'l'});
    }
    else if (S_ISDIR(st.st_mode)) {
      out.append({child, 'd'});
      subdirs.append(child);
    }
    else {
      out.append({child, 'f'});
    }
  }
  closedir(dir);
  for (const std::string &sub : subdirs) {
    zip_collect(root, sub, out);
  }
#else
  UNUSED_VARS(root, rel, out);
#endif
}

/**
 * Escribe `<zip_path>` con todo el contenido de `<dir_path>`, con el propio
 * directorio como raiz (igual que hacia `shutil.make_archive`, que archiva el
 * contenido de `output_dir`).
 */
static bool zip_write_dir(const char *dir_path, const char *zip_path, ReportList *reports)
{
  std::string root = dir_path;
  while (!root.empty() && root.back() == '/') {
    root.pop_back();
  }

  blender::Vector<std::pair<std::string, char>> items;
  zip_collect(root, "", items);
  std::sort(items.begin(), items.end());

  FILE *f = BLI_fopen(zip_path, "wb");
  if (f == nullptr) {
    BKE_reportf(reports, RPT_ERROR, "No se pudo escribir el archivo '%s'", zip_path);
    return false;
  }

  /* Fecha MS-DOS: 1980-01-01 fija. El original tampoco conservaba fechas
   * utiles y asi el `.zip` es reproducible byte a byte entre dos publicaciones
   * del mismo juego, que es lo que hace verificable esta migracion. */
  const uint16_t dos_time = 0;
  const uint16_t dos_date = (1 << 5) | 1; /* mes 1, dia 1, ano 1980. */

  blender::Vector<ZipEntry> dir_entries;
  bool ok = true;

  for (const std::pair<std::string, char> &item : items) {
    const std::string abs = root + "/" + item.first;
    struct stat st;
    if (lstat(abs.c_str(), &st) != 0) {
      ok = false;
      continue;
    }

    ZipEntry e;
    e.name = item.first + (item.second == 'd' ? "/" : "");
    e.mode = uint32_t(st.st_mode);
    e.offset = uint32_t(ftell(f));

    blender::Vector<unsigned char> raw;
    if (item.second == 'f') {
      FILE *in = BLI_fopen(abs.c_str(), "rb");
      if (in == nullptr) {
        ok = false;
        continue;
      }
      raw.resize(size_t(st.st_size));
      if (st.st_size > 0 && fread(raw.data(), 1, size_t(st.st_size), in) != size_t(st.st_size)) {
        ok = false;
      }
      fclose(in);
    }
    else if (item.second == 'l') {
#ifndef WIN32
      raw.resize(size_t(st.st_size) + 1);
      const ssize_t n = readlink(abs.c_str(), (char *)raw.data(), size_t(st.st_size) + 1);
      raw.resize(n > 0 ? size_t(n) : 0);
#endif
    }

    e.usize = uint32_t(raw.size());
    e.crc = raw.is_empty() ? 0 :
                             uint32_t(crc32(0L, raw.data(), uInt(raw.size())));

    blender::Vector<unsigned char> packed;
    if (item.second == 'f' && !raw.is_empty() && zip_deflate(raw, packed) &&
        packed.size() < raw.size())
    {
      e.method = 8;
      e.csize = uint32_t(packed.size());
    }
    else {
      e.method = 0;
      packed = raw;
      e.csize = uint32_t(raw.size());
    }

    zip_put32(f, 0x04034b50);
    zip_put16(f, 20);
    zip_put16(f, 0);
    zip_put16(f, e.method);
    zip_put16(f, dos_time);
    zip_put16(f, dos_date);
    zip_put32(f, e.crc);
    zip_put32(f, e.csize);
    zip_put32(f, e.usize);
    zip_put16(f, uint16_t(e.name.size()));
    zip_put16(f, 0);
    fwrite(e.name.data(), 1, e.name.size(), f);
    if (!packed.is_empty()) {
      fwrite(packed.data(), 1, packed.size(), f);
    }

    dir_entries.append(e);
  }

  const uint32_t cd_offset = uint32_t(ftell(f));
  for (const ZipEntry &e : dir_entries) {
    zip_put32(f, 0x02014b50);
    zip_put16(f, 0x031E); /* Hecho por: Unix, version 3.0 -> hay modo Unix. */
    zip_put16(f, 20);
    zip_put16(f, 0);
    zip_put16(f, e.method);
    zip_put16(f, dos_time);
    zip_put16(f, dos_date);
    zip_put32(f, e.crc);
    zip_put32(f, e.csize);
    zip_put32(f, e.usize);
    zip_put16(f, uint16_t(e.name.size())); /* Largo del nombre. */
    zip_put16(f, 0);                       /* Campo extra. */
    zip_put16(f, 0);                       /* Comentario. */
    zip_put16(f, 0);                       /* Disco de inicio. */
    zip_put16(f, 0);                       /* Atributos internos. */
    /* TRAMPA que costo un `.zip` roto de 269 MB: la cabecera central son 46
     * bytes y no admite relleno. Aqui habia un `zip_put32(f, 0)` de mas, que
     * corria cuatro bytes los atributos externos y el desplazamiento; `unzip`
     * leia como desplazamiento el modo Unix (`bad zipfile offset (lseek):
     * 1106051072`, que es 040755 << 16). */
    zip_put32(f, e.mode << 16); /* Atributos externos: modo Unix arriba. */
    zip_put32(f, e.offset);     /* Desplazamiento de la cabecera local. */
    fwrite(e.name.data(), 1, e.name.size(), f);
  }
  const uint32_t cd_size = uint32_t(ftell(f)) - cd_offset;

  zip_put32(f, 0x06054b50);
  zip_put16(f, 0);
  zip_put16(f, 0);
  zip_put16(f, uint16_t(dir_entries.size()));
  zip_put16(f, uint16_t(dir_entries.size()));
  zip_put32(f, cd_size);
  zip_put32(f, cd_offset);
  zip_put16(f, 0);

  fclose(f);
  return ok;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name `wm.publish_platforms`
 * \{ */

static wmOperatorStatus publish_platforms_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);

  char output_dir[FILE_MAX];
  char runtime_name[FILE_MAX];
  char player_path[FILE_MAX];
  char asset_path[FILE_MAX];
  RNA_string_get(op->ptr, "output_path", output_dir);
  RNA_string_get(op->ptr, "runtime_name", runtime_name);
  RNA_string_get(op->ptr, "player_path", player_path);
  RNA_string_get(op->ptr, "asset_path", asset_path);
  const bool overwrite_asset = RNA_boolean_get(op->ptr, "overwrite_asset");
  const bool make_archive = RNA_boolean_get(op->ptr, "make_archive");

  /* `bpy.path.abspath()`: las rutas `//` son relativas al .blend abierto. */
  BLI_path_abs(output_dir, bmain->filepath);
  BLI_path_abs(player_path, bmain->filepath);
  if (asset_path[0] != '\0') {
    BLI_path_abs(asset_path, bmain->filepath);
  }

  if (runtime_name[0] == '\0') {
    BKE_report(op->reports, RPT_ERROR, "El nombre del ejecutable no puede estar vacio");
    return OPERATOR_CANCELLED;
  }
  if (!BLI_exists(output_dir) && !BLI_dir_create_recursive(output_dir)) {
    BKE_reportf(op->reports, RPT_ERROR, "No se pudo crear '%s'", output_dir);
    return OPERATOR_CANCELLED;
  }

  /* El entregable lo escribe el MISMO codigo que `wm.save_as_runtime`: un solo
   * sitio donde vive el formato del bundle. */
  char output_path[FILE_MAX];
  BLI_path_join(output_path, FILE_MAX, output_dir, runtime_name);

  wmOperatorType *ot = WM_operatortype_find("WM_OT_save_as_runtime", false);
  if (ot == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "wm.save_as_runtime no esta registrado");
    return OPERATOR_CANCELLED;
  }
  PointerRNA props;
  WM_operator_properties_create_ptr(&props, ot);
  RNA_string_set(&props, "player_path", player_path);
  RNA_string_set(&props, "filepath", output_path);
  const wmOperatorStatus status = WM_operator_name_call_ptr(
      C, ot, WM_OP_EXEC_DEFAULT, &props, nullptr);
  WM_operator_properties_free(&props);
  if ((status & OPERATOR_FINISHED) == 0) {
    BKE_report(op->reports, RPT_ERROR, "No se pudo escribir el ejecutable");
    return OPERATOR_CANCELLED;
  }

  /* Assets al lado del ejecutable, con la misma regla del addon: si ya existe
   * y no se pidio sobrescribir, se deja el que hay. */
  int assets_copied = 0;
  if (asset_path[0] != '\0') {
    if (!BLI_exists(asset_path)) {
      BKE_reportf(op->reports, RPT_ERROR, "Could not find asset path: '%s'", asset_path);
    }
    else {
      char dst[FILE_MAX];
      BLI_path_join(dst, FILE_MAX, output_dir, BLI_path_basename(asset_path));
      const bool exists = BLI_exists(dst) != 0;
      if (exists && overwrite_asset) {
        BLI_delete(dst, true, true);
      }
      if (!exists || overwrite_asset) {
        if (BLI_copy(asset_path, dst) == 0) {
          assets_copied = 1;
        }
        else {
          BKE_reportf(op->reports, RPT_ERROR, "No se pudo copiar el asset a '%s'", dst);
        }
      }
    }
  }

  char archive_path[FILE_MAX] = "";
  if (make_archive) {
    STRNCPY(archive_path, output_dir);
    BLI_path_slash_rstrip(archive_path);
    BLI_strncat(archive_path, ".zip", FILE_MAX);
    if (!zip_write_dir(output_dir, archive_path, op->reports)) {
      return OPERATOR_CANCELLED;
    }
  }

  printf("FL-PUBLISH runtime=%s assets=%d archive=%s\n",
         output_path,
         assets_copied,
         archive_path[0] ? archive_path : "-");
  BKE_reportf(op->reports, RPT_INFO, "Juego publicado en \"%s\"", output_dir);
  return OPERATOR_FINISHED;
}

static wmOperatorStatus publish_platforms_invoke(bContext *C,
                                                 wmOperator *op,
                                                 const wmEvent * /*event*/)
{
  return WM_operator_props_dialog_popup(C, op, 400, IFACE_("Publishing Info"));
}

static void WM_OT_publish_platforms(wmOperatorType *ot)
{
  static char player_path_default[FILE_MAX] = {0};
  if (player_path_default[0] == '\0') {
    default_player_path_get(player_path_default);
  }

  /* `bl_label` literal del addon. */
  ot->name = "Exports a runtime for each listed platform";
  ot->idname = "WM_OT_publish_platforms";

  ot->invoke = publish_platforms_invoke;
  ot->exec = publish_platforms_exec;

  ot->flag = OPTYPE_REGISTER;

  PropertyRNA *prop;

  prop = RNA_def_string(ot->srna,
                        "output_path",
                        "//bin/",
                        FILE_MAX,
                        "Publish Output",
                        "Where to publish the game");
  RNA_def_property_subtype(prop, PROP_DIRPATH);

  RNA_def_string(ot->srna,
                 "runtime_name",
                 "game",
                 FILE_MAX,
                 "Runtime name",
                 "The filename for the created runtime");

  prop = RNA_def_string(ot->srna,
                        "player_path",
                        player_path_default,
                        FILE_MAX,
                        "Player Path",
                        "The path to the Blenderplayer to use for this platform");
  RNA_def_property_subtype(prop, PROP_FILEPATH);

  prop = RNA_def_string(
      ot->srna, "asset_path", nullptr, FILE_MAX, "Asset Path", "Path to the asset to be copied");
  RNA_def_property_subtype(prop, PROP_FILEPATH);

  RNA_def_boolean(ot->srna,
                  "overwrite_asset",
                  true,
                  "Overwrite Asset",
                  "Overwrite the asset if it already exists in the destination folder");

  RNA_def_boolean(ot->srna,
                  "make_archive",
                  true,
                  "Make Archive",
                  "Create a zip archive of the published game");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Panel `RENDER_PT_publish`
 * \{ */

static void publish_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;
  layout->label(IFACE_("Publish the game as a stand-alone runtime"), ICON_NONE);
  layout->op("WM_OT_publish_platforms", IFACE_("Publish Platforms"), ICON_NONE);
  layout->op("WM_OT_save_as_runtime", IFACE_("Save as game runtime"), ICON_NONE);
}

static void publish_panel_register()
{
  SpaceType *st = BKE_spacetype_from_id(SPACE_PROPERTIES);
  if (st == nullptr) {
    return;
  }
  ARegionType *art = BKE_regiontype_from_id(st, RGN_TYPE_WINDOW);
  if (art == nullptr) {
    return;
  }

  /* Mismo `bl_idname`, `bl_label` y `bl_context` que la clase de Python.
   * El `poll` original era `scene.render.engine == "BLENDER_GAME"`, un motor
   * que no existe desde la 2.79: el panel NUNCA se dibujaba. Aqui no hay
   * `poll`, asi que por fin se ve. Divergencia deliberada, escrita. */
  PanelDecl panel{};
  panel.idname = "RENDER_PT_publish";
  panel.label = N_("Publishing Info");
  panel.context = "render";
  panel.flag = PANEL_TYPE_DEFAULT_CLOSED;
  panel.draw = publish_panel_draw;
  panels_register(art, SPACE_PROPERTIES, blender::Span<const PanelDecl>(&panel, 1));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registro
 * \{ */

void publishing_operatortypes_register()
{
  WM_operatortype_append(WM_OT_publish_platforms);
  publish_panel_register();
}

/** \} */

}  // namespace flipendo::game
