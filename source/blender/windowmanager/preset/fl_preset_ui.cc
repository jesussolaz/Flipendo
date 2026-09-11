/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * El panel de presets, en C++. Ver FL_preset_ui.hpp.
 *
 * Esto es la transliteracion de `Menu.path_menu()` y `Menu.draw_preset()`
 * (`scripts/modules/bpy_types.py`) y de `PresetPanel` (`scripts/startup/bl_ui/utils.py`).
 * Se ha hecho linea a linea a proposito, incluido el orden en que se crean los
 * elementos, porque el verificador de interfaz compara el arbol de `uiLayout` que sale
 * y cualquier elemento de mas, de menos o en otro sitio es una diferencia.
 */

#include "FL_preset_ui.hpp"

#include <algorithm>
#include <cctype>
#include <string>

#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_fileops.h"
#include "BLI_fileops_types.h"
#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "BKE_appdir.hh"
#include "BKE_context.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

namespace flipendo::preset::ui {

/* -------------------------------------------------------------------------- */
/** \name Las carpetas
 * \{ */

static void path_append_if_dir(blender::Vector<std::string> &out, const std::string &path)
{
  if (path.empty() || !BLI_is_dir(path.c_str())) {
    return;
  }
  /* `script_paths` quitaba las repetidas conservando el primer orden. */
  for (const std::string &existing : out) {
    if (existing == path) {
      return;
    }
  }
  out.append(path);
}

/** Las bases de scripts, en el orden de `bpy.utils.script_paths()`. */
static blender::Vector<std::string> script_bases()
{
  blender::Vector<std::string> bases;
  if (const std::optional<std::string> p = BKE_appdir_folder_id(BLENDER_SYSTEM_SCRIPTS, nullptr)) {
    bases.append(*p);
  }
  if (const std::optional<std::string> p = BKE_appdir_folder_id(BLENDER_USER_SCRIPTS, nullptr)) {
    bases.append(*p);
  }
  /* Las carpetas de scripts que el usuario anade en Preferencias. */
  LISTBASE_FOREACH (const bUserScriptDirectory *, dir, &U.script_directories) {
    if (dir->dir_path[0] != '\0') {
      bases.append(dir->dir_path);
    }
  }
  return bases;
}

blender::Vector<std::string> preset_paths(const blender::StringRef subdir)
{
  blender::Vector<std::string> out;
  const std::string sub(subdir);
  for (const std::string &base : script_bases()) {
    char path[FILE_MAX];
    BLI_path_join(path, sizeof(path), base.c_str(), "presets", sub.c_str());
    path_append_if_dir(out, path);
  }
  return out;
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name El nombre que se ve
 * \{ */

/** `_display_name_literals` de `bpy/path.py`, en su mismo orden. */
struct Literal {
  const char *shown;
  const char *in_file;
};
static const Literal display_literals[] = {
    {":", "_colon_"},
    {"+", "_plus_"},
    {"/", "_slash_"},
};

static void replace_all(std::string &s, const char *from, const char *to)
{
  const size_t from_len = strlen(from);
  if (from_len == 0) {
    return;
  }
  size_t pos = 0;
  while ((pos = s.find(from, pos)) != std::string::npos) {
    s.replace(pos, from_len, to);
    pos += strlen(to);
  }
}

/** `name.islower()` de Python: hay alguna letra y ninguna en mayuscula. */
static bool is_lower_py(const std::string &s)
{
  bool has_cased = false;
  for (const unsigned char c : s) {
    if (std::isupper(c)) {
      return false;
    }
    if (std::islower(c)) {
      has_cased = true;
    }
  }
  return has_cased;
}

/** `str.title()` de Python: mayuscula tras cualquier caracter no alfabetico. */
static std::string title_py(const std::string &s)
{
  std::string out;
  bool start_of_word = true;
  for (const unsigned char c : s) {
    if (std::isalpha(c)) {
      out += char(start_of_word ? std::toupper(c) : std::tolower(c));
      start_of_word = false;
    }
    else {
      out += char(c);
      start_of_word = true;
    }
  }
  return out;
}

std::string display_name(const blender::StringRef filename, const bool title_case)
{
  /* `os.path.splitext(basename(name))[0]`. */
  std::string name(filename);
  const size_t slash = name.find_last_of("/\\");
  if (slash != std::string::npos) {
    name = name.substr(slash + 1);
  }
  const size_t dot = name.find_last_of('.');
  if (dot != std::string::npos && dot != 0) {
    name = name.substr(0, dot);
  }

  for (const Literal &lit : display_literals) {
    replace_all(name, lit.in_file, lit.shown);
  }

  replace_all(name, "_", " ");
  /* `lstrip(" ")`, para permitir el prefijo de guion bajo. */
  const size_t first = name.find_first_not_of(' ');
  name = (first == std::string::npos) ? std::string() : name.substr(first);

  if (title_case && is_lower_py(name)) {
    name = title_py(name);
  }
  return name;
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name El orden natural
 *
 * El Python ordenaba con `re.split(r"(\d+)", nombre.lower())` y comparando como
 * numero los trozos de digitos. Se transcribe tal cual en vez de usar
 * `BLI_strcasecmp_natural`, que no es la misma funcion: aqui lo que manda es
 * reproducir el orden que ve el usuario hoy.
 * \{ */

struct NaturalKey {
  /** Alterna texto y numero, empezando por texto, como hace `re.split`. */
  blender::Vector<std::string> text;
  blender::Vector<long long> number;
};

static NaturalKey natural_key(const std::string &name_in)
{
  std::string name = name_in;
  for (char &c : name) {
    c = char(std::tolower(static_cast<unsigned char>(c)));
  }

  NaturalKey key;
  size_t i = 0;
  while (true) {
    size_t digit_start = i;
    while (digit_start < name.size() && !std::isdigit(static_cast<unsigned char>(name[digit_start])))
    {
      digit_start++;
    }
    key.text.append(name.substr(i, digit_start - i));
    if (digit_start >= name.size()) {
      break;
    }
    size_t digit_end = digit_start;
    while (digit_end < name.size() && std::isdigit(static_cast<unsigned char>(name[digit_end]))) {
      digit_end++;
    }
    /* Un numero absurdamente largo no cabe en `long long`; se satura, que es mejor
     * que envolver y colocarlo al principio. */
    const std::string digits = name.substr(digit_start, digit_end - digit_start);
    long long value = 0;
    for (const char c : digits) {
      if (value > 1000000000000LL) {
        break;
      }
      value = value * 10 + (c - '0');
    }
    key.number.append(value);
    i = digit_end;
  }
  return key;
}

static bool natural_less(const NaturalKey &a, const NaturalKey &b)
{
  const int64_t n = std::max(a.text.size(), b.text.size());
  for (int64_t i = 0; i < n; i++) {
    const std::string ta = i < a.text.size() ? a.text[i] : std::string();
    const std::string tb = i < b.text.size() ? b.text[i] : std::string();
    if (ta != tb) {
      return ta < tb;
    }
    const bool has_a = i < a.number.size();
    const bool has_b = i < b.number.size();
    if (!has_a || !has_b) {
      /* La tupla mas corta va antes, como en Python. */
      return !has_a && has_b;
    }
    if (a.number[i] != b.number[i]) {
      return a.number[i] < b.number[i];
    }
  }
  return false;
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name El dibujo
 * \{ */

struct PresetFile {
  std::string name;     /* el nombre del fichero, con extension */
  std::string filepath; /* la ruta completa */
  NaturalKey key;
};

/** Las extensiones que lista `draw_preset`: `.fpreset`, `.py` y `.xml`. */
static bool extension_valid(const std::string &name)
{
  const size_t dot = name.find_last_of('.');
  if (dot == std::string::npos) {
    return false;
  }
  std::string ext = name.substr(dot);
  for (char &c : ext) {
    c = char(std::tolower(static_cast<unsigned char>(c)));
  }
  return ext == ".fpreset" || ext == ".py" || ext == ".xml";
}

static blender::Vector<PresetFile> preset_files(const blender::StringRef subdir)
{
  blender::Vector<PresetFile> files;
  for (const std::string &dir : preset_paths(subdir)) {
    direntry *filelist = nullptr;
    const unsigned int count = BLI_filelist_dir_contents(dir.c_str(), &filelist);
    for (unsigned int i = 0; i < count; i++) {
      const char *name = filelist[i].relname;
      /* `if not f.startswith(".")`: fuera los ocultos, y de paso `.` y `..`. */
      if (name == nullptr || name[0] == '.') {
        continue;
      }
      if (!extension_valid(name)) {
        continue;
      }
      PresetFile file;
      file.name = name;
      char path[FILE_MAX];
      BLI_path_join(path, sizeof(path), dir.c_str(), name);
      file.filepath = path;
      file.key = natural_key(file.name);
      files.append(std::move(file));
    }
    if (filelist != nullptr) {
      BLI_filelist_free(filelist, count);
    }
  }

  std::stable_sort(files.begin(), files.end(), [](const PresetFile &a, const PresetFile &b) {
    return natural_less(a.key, b.key);
  });
  return files;
}

void draw_menu(const bContext *C, uiLayout *layout, const MenuSpec &spec)
{
  BLI_assert(spec.subdir != nullptr && spec.op != nullptr);

  const blender::Vector<std::string> paths = preset_paths(spec.subdir);
  if (paths.is_empty()) {
    /* Sin carpetas no hay presets que listar, y el Python lo dice con todas las
     * letras en vez de callarse. Se mantiene: es informacion para el usuario. */
    layout->label("* Missing Paths *", ICON_NONE);
  }

  const blender::Vector<PresetFile> files = preset_files(spec.subdir);

  /* La columna se crea SIEMPRE, tambien cuando no hay ni un fichero. */
  uiLayout *col = &layout->column(true);

  for (const PresetFile &file : files) {
    uiLayout *row = &col->row(true);
    /* `title_case=False`: es lo que pasaba `draw_preset`. */
    const std::string name = display_name(file.name, false);

    PointerRNA op_ptr = row->op(spec.op, name, ICON_NONE);
    if (op_ptr.data) {
      RNA_string_set(&op_ptr, "filepath", file.filepath.c_str());
      if (spec.menu_idname != nullptr && STREQ(spec.op, "SCRIPT_OT_execute_preset")) {
        RNA_string_set(&op_ptr, "menu_idname", spec.menu_idname);
      }
    }

    if (spec.add_op != nullptr) {
      PointerRNA rm_ptr = row->op(spec.add_op, "", ICON_REMOVE);
      if (rm_ptr.data) {
        RNA_string_set(&rm_ptr, "name", name.c_str());
        RNA_boolean_set(&rm_ptr, "remove_name", true);
      }
    }
  }

  if (spec.add_op != nullptr) {
    wmWindowManager *wm = CTX_wm_manager(C);

    layout->separator();
    uiLayout *row = &layout->row(false);

    uiLayout *sub = &row->row(false);
    uiLayoutSetEmboss(sub, blender::ui::EmbossType::Emboss);

    std::string preset_name;
    if (wm != nullptr) {
      PointerRNA wm_ptr = RNA_id_pointer_create(&wm->id);
      sub->prop(&wm_ptr, "preset_name", UI_ITEM_NONE, "", ICON_NONE);
      char *value = RNA_string_get_alloc(&wm_ptr, "preset_name", nullptr, 0, nullptr);
      if (value != nullptr) {
        preset_name = value;
        MEM_freeN(value);
      }
    }

    PointerRNA add_ptr = row->op(spec.add_op, "", ICON_ADD);
    if (add_ptr.data) {
      RNA_string_set(&add_ptr, "name", preset_name.c_str());
    }
  }
}

void draw_panel(const bContext *C, uiLayout *layout, const MenuSpec &spec)
{
  /* Lo que ponia `PresetPanel.draw()` antes de llamar a `draw_preset`. */
  uiLayoutSetEmboss(layout, blender::ui::EmbossType::Pulldown);
  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_DEFAULT);
  draw_menu(C, layout, spec);
}

void draw_panel_header(const bContext *C, uiLayout *layout, const char *panel_idname)
{
  uiLayoutSetEmboss(layout, blender::ui::EmbossType::None);
  uiItemPopoverPanel(layout, C, panel_idname, "", ICON_PRESET);
}

/** \} */

}  // namespace flipendo::preset::ui
