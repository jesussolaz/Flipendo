/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * `AddPresetBase` y sus veinte envoltorios, en C++.
 *
 * Que habia
 * ---------
 * `scripts/startup/bl_operators/presets.py` declaraba UNA clase base con la logica
 * (`AddPresetBase`, ~168 lineas) y **veinticuatro subclases** que no hacian otra cosa
 * que declarar datos: su `bl_idname`, su `bl_label`, a que menu pertenecen, en que
 * carpeta guardan y — hasta que el carril de presets se lo llevo a `fl_preset_spec.cc` —
 * que rutas RNA copiaban. De las 1.023 lineas del fichero, 298 eran esa tabla.
 *
 * Una subclase por familia no es codigo: es una fila. Aqui esta como fila.
 *
 * Que hace el operador (transliterado de `AddPresetBase.execute`)
 * --------------------------------------------------------------
 * - **anadir**: normaliza el nombre a un nombre de fichero, crea
 *   `<usuario>/scripts/presets/<familia>/`, escribe el preset con `WM_OT_preset_write`
 *   (que ya era C++) y pone la etiqueta del menu al nombre guardado;
 * - **quitar**: busca el fichero por nombre o por nombre visible, se niega a tocar los
 *   que vienen con Flipendo o con una extension, lo borra y devuelve la etiqueta a
 *   "Presets".
 *
 * La unica familia con logica propia sigue siendo `keyconfig`, que no guarda una lista
 * de propiedades sino un mapa de teclas entero: se apoya en
 * `preferences.keyconfig_export` / `keyconfig_import`, que ya son nativos.
 *
 * Lo que NO esta aqui, y por que
 * ------------------------------
 * Los tres operadores de tema (`wm.interface_theme_preset_add`, `_remove` y `_save`)
 * siguen en Python porque su formato es XML y lo serializa `scripts/modules/rna_xml.py`.
 * Es la deuda 6 de politicas/PRESETS-A-DATOS.md — "otra familia y otro formato" — y no
 * se cierra hasta que haya un lector/escritor de temas nativo.
 *
 * Verificacion: `--fl-check-preset-optypes` contra
 * `tests/flipendo/presetops/baseline-python.txt`, congelada con el binario que todavia
 * tenia las clases de Python dentro de su bundle.
 */

#include "FL_preset.hpp"
#include "FL_preset_ui.hpp"

#include <cctype>
#include <cstdio>
#include <cstring>
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

#include "BLT_translation.hh"

#include "BKE_appdir.hh"
#include "BKE_context.hh"
#include "BKE_preferences.h"
#include "BKE_report.hh"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_types.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_keymap.hh"
#include "WM_types.hh"

namespace flipendo::preset {

/* -------------------------------------------------------------------------- */
/** \name La tabla: una fila por familia
 * \{ */

enum class AddPresetKind {
  /** Lista de propiedades RNA; la escribe `WM_OT_preset_write`. */
  Property,
  /** Mapa de teclas entero; lo exporta `preferences.keyconfig_export`. */
  Keyconfig,
};

struct AddPresetType {
  const char *idname;
  /** `bl_label` de la clase de Python. */
  const char *name;
  /** La cadena de documentacion de la clase, que es lo que Python usaba de descripcion. */
  const char *description;
  /** `preset_menu`: el menu o panel cuya etiqueta se actualiza. */
  const char *preset_menu;
  /** `preset_subdir`. `nullptr` cuando lo decide la propiedad `operator`. */
  const char *preset_subdir;
  /** `preset_ext`; `nullptr` significa `.fpreset`. */
  const char *preset_ext;
  AddPresetKind kind;
  /** Defecto de `remove_active`: los tres `*_remove` lo traen a `true`. */
  bool remove_active_default;
  /** Anade la propiedad `use_focal_length` (camara y camara de seguimiento). */
  bool has_focal_length;
  bool focal_length_default;
  /** Anade la propiedad `operator` (`wm.operator_preset_add`). */
  bool has_operator_prop;
};

/* El orden es el del volcado de verificacion (alfabetico por idname), para que la
 * comparacion con la linea base no dependa del orden de registro. */
const AddPresetType add_preset_types[] = {
    {"CAMERA_OT_preset_add",
     "Add Camera Preset",
     "Add or remove a Camera Preset",
     "CAMERA_PT_presets",
     "camera",
     nullptr,
     AddPresetKind::Property,
     false,
     true,
     false,
     false},
    {"CAMERA_OT_safe_areas_preset_add",
     "Add Safe Area Preset",
     "Add or remove a Safe Areas Preset",
     "CAMERA_PT_safe_areas_presets",
     "safe_areas",
     nullptr,
     AddPresetKind::Property,
     false,
     false,
     false,
     false},
    {"CLIP_OT_camera_preset_add",
     "Add Camera Preset",
     "Add or remove a Tracking Camera Intrinsics Preset",
     "CLIP_PT_camera_presets",
     "tracking_camera",
     nullptr,
     AddPresetKind::Property,
     false,
     true,
     /* Ojo: aqui el defecto es `true`, al reves que en la camara normal. */
     true,
     false},
    {"CLIP_OT_track_color_preset_add",
     "Add Track Color Preset",
     "Add or remove a Clip Track Color Preset",
     "CLIP_PT_track_color_presets",
     "tracking_track_color",
     nullptr,
     AddPresetKind::Property,
     false,
     false,
     false,
     false},
    {"CLIP_OT_tracking_settings_preset_add",
     "Add Tracking Settings Preset",
     "Add or remove a motion tracking settings preset",
     "CLIP_PT_tracking_settings_presets",
     "tracking_settings",
     nullptr,
     AddPresetKind::Property,
     false,
     false,
     false,
     false},
    {"CLOTH_OT_preset_add",
     "Add Cloth Preset",
     "Add or remove a Cloth Preset",
     "CLOTH_PT_presets",
     "cloth",
     nullptr,
     AddPresetKind::Property,
     false,
     false,
     false,
     false},
    {"FLUID_OT_preset_add",
     "Add Fluid Preset",
     "Add or remove a Fluid Preset",
     "FLUID_PT_presets",
     "fluid",
     nullptr,
     AddPresetKind::Property,
     false,
     false,
     false,
     false},
    {"NODE_OT_node_color_preset_add",
     "Add Node Color Preset",
     "Add or remove a Node Color Preset",
     "NODE_PT_node_color_presets",
     "node_color",
     nullptr,
     AddPresetKind::Property,
     false,
     false,
     false,
     false},
    {"PARTICLE_OT_hair_dynamics_preset_add",
     "Add Hair Dynamics Preset",
     "Add or remove a Hair Dynamics Preset",
     "PARTICLE_PT_hair_dynamics_presets",
     "hair_dynamics",
     nullptr,
     AddPresetKind::Property,
     false,
     false,
     false,
     false},
    {"RENDER_OT_color_management_white_balance_preset_add",
     "Add White Balance Preset",
     "Add or remove a white balance preset",
     "RENDER_PT_color_management_white_balance_presets",
     "color_management/white_balance",
     nullptr,
     AddPresetKind::Property,
     false,
     false,
     false,
     false},
    {"RENDER_OT_eevee_raytracing_preset_add",
     "Add Raytracing Preset",
     "Add or remove an EEVEE ray-tracing preset",
     "RENDER_PT_eevee_next_raytracing_presets",
     "eevee/raytracing",
     nullptr,
     AddPresetKind::Property,
     false,
     false,
     false,
     false},
    {"RENDER_OT_preset_add",
     "Add Render Preset",
     "Add or remove a Render Preset",
     "RENDER_PT_format_presets",
     "render",
     nullptr,
     AddPresetKind::Property,
     false,
     false,
     false,
     false},
    {"SCENE_OT_gpencil_brush_preset_add",
     "Add Grease Pencil Brush Preset",
     "Add or remove grease pencil brush preset",
     "VIEW3D_PT_gpencil_brush_presets",
     "gpencil_brush",
     nullptr,
     AddPresetKind::Property,
     false,
     false,
     false,
     false},
    {"SCENE_OT_gpencil_material_preset_add",
     "Add Grease Pencil Material Preset",
     "Add or remove Grease Pencil material preset",
     "MATERIAL_PT_gpencil_material_presets",
     "gpencil_material",
     nullptr,
     AddPresetKind::Property,
     false,
     false,
     false,
     false},
    {"TEXT_EDITOR_OT_preset_add",
     "Add Text Editor Preset",
     "Add or remove a Text Editor Preset",
     "USERPREF_PT_text_editor_presets",
     "text_editor",
     nullptr,
     AddPresetKind::Property,
     false,
     false,
     false,
     false},
    {"WM_OT_keyconfig_preset_add",
     "Add Custom Keymap Configuration",
     "Add a custom keymap configuration to the preset list",
     "USERPREF_MT_keyconfigs",
     "keyconfig",
     ".fkeyconfig",
     AddPresetKind::Keyconfig,
     false,
     false,
     false,
     false},
    {"WM_OT_keyconfig_preset_remove",
     "Remove Keymap Configuration",
     "Remove a custom keymap configuration from the preset list",
     "USERPREF_MT_keyconfigs",
     "keyconfig",
     ".fkeyconfig",
     AddPresetKind::Keyconfig,
     true,
     false,
     false,
     false},
    {"WM_OT_operator_preset_add",
     "Operator Preset",
     "Add or remove an Operator Preset",
     "WM_MT_operator_presets",
     /* La familia sale de la propiedad `operator`. */
     nullptr,
     nullptr,
     AddPresetKind::Property,
     false,
     false,
     false,
     true},
};

/** \} */

/* -------------------------------------------------------------------------- */
/** \name Utilidades de ruta y de nombre
 *
 * Transliteracion de `bpy.path` y `bpy.utils`, que sin interprete no existen.
 * \{ */

namespace {

/** `str.strip()` de Python: los seis blancos ASCII a los dos lados. */
std::string strip_py(const std::string &s)
{
  const char *ws = " \t\n\r\v\f";
  const size_t first = s.find_first_not_of(ws);
  if (first == std::string::npos) {
    return "";
  }
  const size_t last = s.find_last_not_of(ws);
  return s.substr(first, last - first + 1);
}

/** `str.strip("_")`. */
std::string strip_underscores(const std::string &s)
{
  const size_t first = s.find_first_not_of('_');
  if (first == std::string::npos) {
    return "";
  }
  const size_t last = s.find_last_not_of('_');
  return s.substr(first, last - first + 1);
}

void replace_all(std::string &s, const char *from, const char *to)
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

/**
 * `bpy.path.display_name_to_filepath`: el inverso de `display_name` para los tres
 * caracteres que no caben en un nombre de fichero. El orden del diccionario de Python
 * (`:`, `+`, `/`) se conserva; aqui da igual porque ninguna sustitucion introduce otro
 * de los tres, pero se deja escrito para que nadie lo reordene "por limpieza".
 */
std::string display_name_to_filepath(std::string name)
{
  replace_all(name, ":", "_colon_");
  replace_all(name, "+", "_plus_");
  replace_all(name, "/", "_slash_");
  return name;
}

/**
 * `AddPresetBase.as_filename`: recorta, aplica los literales y sustituye por `_` los
 * caracteres de la lista negra, incluidos los tres que se acaban de expandir (ya no
 * quedan, pero el original lo hacia en este orden y el resultado es el mismo).
 */
std::string as_filename(const std::string &name_in)
{
  std::string name = display_name_to_filepath(strip_py(name_in));
  /* La tabla exacta de `maketrans` del original. */
  static const char *bad = " !@#$%^&*(){}:\";'[]<>,.\\/?";
  for (char &c : name) {
    if (c != '\0' && strchr(bad, c) != nullptr) {
      c = '_';
    }
  }
  return strip_underscores(name);
}

/** `bpy.utils.resource_path(id)` para los tres que mira `is_path_builtin`. */
bool path_is_parent_of(const std::string &parent, const std::string &path)
{
  if (parent.empty() || path.empty()) {
    return false;
  }
  /* `BLI_path_contains` normaliza y compara por componentes, que es lo que hacia
   * `os.path.commonpath`. `samefile` de Python ademas resuelve enlaces simbolicos; aqui
   * no, y se dice: un preset alcanzado por un enlace hacia la carpeta del sistema se
   * consideraria borrable. No ocurre en una instalacion normal. */
  return BLI_path_contains(parent.c_str(), path.c_str());
}

/** `bpy.utils.is_path_builtin`. */
bool is_path_builtin(const std::string &path)
{
  const std::optional<std::string> user_path = BKE_appdir_resource_path_id(
      BLENDER_RESOURCE_PATH_USER, false);
  for (const int res : {int(BLENDER_RESOURCE_PATH_SYSTEM), int(BLENDER_RESOURCE_PATH_LOCAL)}) {
    const std::optional<std::string> parent = BKE_appdir_resource_path_id(res, false);
    if (!parent || parent->empty()) {
      continue;
    }
    /* Una instalacion portatil puede tener la del sistema igual que la del usuario; en
     * ese caso el original NO la considera de solo lectura. */
    if (user_path && *parent == *user_path) {
      continue;
    }
    if (path_is_parent_of(*parent, path)) {
      return true;
    }
  }
  return false;
}

/** `bpy.utils.is_path_extension`: las carpetas de los repositorios de extensiones. */
bool is_path_extension(const std::string &path)
{
  LISTBASE_FOREACH (const bUserExtensionRepo *, repo, &U.extension_repos) {
    if (repo->flag & USER_EXTENSION_REPO_FLAG_DISABLED) {
      continue;
    }
    char dirpath[FILE_MAX];
    if (BKE_preferences_extension_repo_dirpath_get(repo, dirpath, sizeof(dirpath)) == 0) {
      continue;
    }
    if (path_is_parent_of(dirpath, path)) {
      return true;
    }
  }
  return false;
}

/**
 * `bpy.utils.user_resource('SCRIPTS', path=..., create=True)`.
 *
 * NO vale `BKE_appdir_folder_id_create()`, aunque lo parezca. Esa funcion prueba
 * primero con `BKE_appdir_folder_id()`, que EXIGE que la carpeta exista, y solo si no
 * existe cae al camino del usuario. Resultado: con `BLENDER_USER_SCRIPTS` puesto pero
 * su `presets/<familia>` todavia sin crear, y la carpeta por defecto ya creada, escribe
 * en la POR DEFECTO -- justo al reves que el Python, que llama al equivalente de
 * `folder_id_user_notest` (sin comprobar existencia) y crea lo que falte.
 *
 * Lo cazo `--fl-check-preset-ops`, y de la peor manera posible: el arnes escribio siete
 * presets en la carpeta de verdad del usuario en vez de en la temporal.
 */
std::string user_resource_create(const char *subfolder)
{
  const std::optional<std::string> target = BKE_appdir_folder_id_user_notest(BLENDER_USER_SCRIPTS,
                                                                            subfolder);
  if (!target || target->empty()) {
    return "";
  }
  if (!BLI_exists(target->c_str())) {
    if (!BLI_dir_create_recursive(target->c_str())) {
      return "";
    }
  }
  else if (!BLI_is_dir(target->c_str())) {
    printf("Path '%s' found but isn't a directory!\n", target->c_str());
    return "";
  }
  return *target;
}

/**
 * `WM_keyconfig_active()` de `wm_keymap.cc` es estatica, asi que se replica aqui: es
 * lo que hay detras de `bpy.context.window_manager.keyconfigs.active`.
 */
wmKeyConfig *keyconfig_active(wmWindowManager *wm)
{
  if (wm == nullptr) {
    return nullptr;
  }
  if (wmKeyConfig *kc = static_cast<wmKeyConfig *>(
          BLI_findstring(&wm->keyconfigs, U.keyconfigstr, offsetof(wmKeyConfig, idname))))
  {
    return kc;
  }
  return wm->defaultconf;
}

/** `presets.py::_is_path_readonly`. */
bool path_is_readonly(const std::string &path)
{
  return is_path_builtin(path) || is_path_extension(path);
}

/**
 * `bpy.utils.preset_find`. Se apoya en las mismas carpetas que enumera el menu
 * (`flipendo::preset::ui::preset_paths`), de modo que lo que se ve es lo que se borra.
 */
std::string preset_find(const std::string &name,
                        const char *subdir,
                        const bool use_display_name,
                        const std::string &ext)
{
  if (name.empty()) {
    return "";
  }
  for (const std::string &dir : ui::preset_paths(subdir)) {
    std::string filename;
    if (use_display_name) {
      direntry *filelist = nullptr;
      const unsigned int count = BLI_filelist_dir_contents(dir.c_str(), &filelist);
      for (unsigned int i = 0; i < count; i++) {
        const char *relname = filelist[i].relname;
        if (relname == nullptr) {
          continue;
        }
        const std::string fn(relname);
        if (fn.size() < ext.size() || fn.compare(fn.size() - ext.size(), ext.size(), ext) != 0) {
          continue;
        }
        if (name == ui::display_name(fn, false)) {
          filename = fn;
          break;
        }
      }
      if (filelist != nullptr) {
        BLI_filelist_free(filelist, count);
      }
    }
    else {
      filename = name + ext;
    }
    if (filename.empty()) {
      continue;
    }
    char filepath[FILE_MAX];
    BLI_path_join(filepath, sizeof(filepath), dir.c_str(), filename.c_str());
    if (BLI_exists(filepath)) {
      return filepath;
    }
  }
  return "";
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name El operador generico
 * \{ */

const AddPresetType *type_of(const wmOperator *op)
{
  if (op == nullptr || op->type == nullptr || op->type->idname == nullptr) {
    return nullptr;
  }
  for (const AddPresetType &type : add_preset_types) {
    if (STREQ(type.idname, op->type->idname)) {
      return &type;
    }
  }
  return nullptr;
}

/**
 * `AddPresetOperator.operator_path`: `WM_OT_foo` -> `operator/wm.foo`.
 * El separador es `/` y no `SEP`: es una ruta relativa dentro de `presets/`, y en el
 * formato y en el disco se escribe siempre asi.
 */
std::string operator_subdir(const std::string &operator_idname)
{
  const size_t pos = operator_idname.find("_OT_");
  if (pos == std::string::npos) {
    return "";
  }
  std::string prefix = operator_idname.substr(0, pos);
  for (char &c : prefix) {
    c = char(std::tolower(static_cast<unsigned char>(c)));
  }
  return "operator/" + prefix + "." + operator_idname.substr(pos + 4);
}

/** La familia de esta ejecucion: la de la fila, o la que diga `operator`. */
std::string subdir_of(const AddPresetType &type, wmOperator *op)
{
  if (type.has_operator_prop) {
    char buf[128];
    RNA_string_get(op->ptr, "operator", buf);
    return operator_subdir(buf);
  }
  return type.preset_subdir ? type.preset_subdir : "";
}

std::string ext_of(const AddPresetType &type)
{
  return type.preset_ext ? type.preset_ext : ".fpreset";
}

/** El `pre_cb` que solo tenia `RemovePresetKeyconfig`. */
void pre_cb(const AddPresetType &type, bContext *C)
{
  if (type.kind != AddPresetKind::Keyconfig || !type.remove_active_default) {
    return;
  }
  if (const wmKeyConfig *kc = keyconfig_active(CTX_wm_manager(C))) {
    ui::menu_label_set(type.preset_menu, kc->idname);
  }
}

/** El `post_cb` que solo tenia `RemovePresetKeyconfig`. */
void post_cb(const AddPresetType &type, bContext *C)
{
  if (type.kind != AddPresetKind::Keyconfig || !type.remove_active_default) {
    return;
  }
  wmWindowManager *wm = CTX_wm_manager(C);
  if (wm == nullptr) {
    return;
  }
  if (wmKeyConfig *kc = keyconfig_active(wm)) {
    WM_keyconfig_remove(wm, kc);
  }
}

wmOperatorStatus add_preset_exec(bContext *C, wmOperator *op)
{
  const AddPresetType *type_ptr = type_of(op);
  if (type_ptr == nullptr) {
    return OPERATOR_CANCELLED;
  }
  const AddPresetType &type = *type_ptr;

  pre_cb(type, C);

  const std::string ext = ext_of(type);
  const std::string subdir = subdir_of(type, op);
  const bool remove_name = RNA_boolean_get(op->ptr, "remove_name");
  const bool remove_active = RNA_boolean_get(op->ptr, "remove_active");
  const bool is_preset_add = !(remove_name || remove_active);

  char name_buf[256];
  RNA_string_get(op->ptr, "name", name_buf);
  std::string name = is_preset_add ? strip_py(name_buf) : std::string(name_buf);

  std::string filepath;

  if (is_preset_add) {
    if (name.empty()) {
      /* El original devuelve FINISHED, no CANCELLED: pulsar "Aceptar" sin nombre no es
       * un error, simplemente no guarda. */
      return OPERATOR_FINISHED;
    }

    /* Devolver el campo del popover a "New Preset" cuando se guardo con ese texto. */
    if (wmWindowManager *wm = CTX_wm_manager(C)) {
      PointerRNA wm_ptr = RNA_id_pointer_create(&wm->id);
      char *current = RNA_string_get_alloc(&wm_ptr, "preset_name", nullptr, 0, nullptr);
      if (current != nullptr) {
        if (name == current) {
          RNA_string_set(&wm_ptr, "preset_name", DATA_("New Preset"));
        }
        MEM_freeN(current);
      }
    }

    const std::string filename = as_filename(name);

    char target_sub[FILE_MAX];
    BLI_path_join(target_sub, sizeof(target_sub), "presets", subdir.c_str());
    const std::string target_path = user_resource_create(target_sub);
    if (target_path.empty()) {
      BKE_report(op->reports, RPT_WARNING, "Failed to create presets path");
      return OPERATOR_CANCELLED;
    }

    char path_buf[FILE_MAX];
    BLI_path_join(path_buf, sizeof(path_buf), target_path.c_str(), (filename + ext).c_str());
    filepath = path_buf;

    if (type.kind == AddPresetKind::Keyconfig) {
      /* Lo que hacia `AddPresetKeyconfig.add()`. Las dos llamadas ponen TODAS sus
       * propiedades: `WM_operator_name_call` arrastra las de la ejecucion anterior y
       * `bpy.ops` no lo hacia (trampa 1 de politicas/RIGIDBODY-A-CPP.md). */
      PointerRNA props;
      WM_operator_properties_create(&props, "PREFERENCES_OT_keyconfig_export");
      RNA_string_set(&props, "filepath", filepath.c_str());
      WM_operator_name_call(
          C, "PREFERENCES_OT_keyconfig_export", WM_OP_EXEC_DEFAULT, &props, nullptr);
      WM_operator_properties_free(&props);

      WM_operator_properties_create(&props, "PREFERENCES_OT_keyconfig_import");
      RNA_string_set(&props, "filepath", filepath.c_str());
      RNA_boolean_set(&props, "keep_original", true);
      WM_operator_name_call(
          C, "PREFERENCES_OT_keyconfig_import", WM_OP_EXEC_DEFAULT, &props, nullptr);
      WM_operator_properties_free(&props);
    }
    else {
      printf("Writing Preset: '%s'\n", filepath.c_str());
      const bool use_focal_length = type.has_focal_length ?
                                        RNA_boolean_get(op->ptr, "use_focal_length") :
                                        false;
      std::string error;
      if (!write_preset(C, subdir, filepath, use_focal_length, error)) {
        BKE_reportf(op->reports, RPT_ERROR, RPT_("Unable to write preset: %s"), error.c_str());
        return OPERATOR_CANCELLED;
      }
    }

    ui::menu_label_set(type.preset_menu, ui::display_name(filename, true));
  }
  else {
    if (remove_active) {
      name = ui::menu_label_get(type.preset_menu);
    }

    /* Se busca con la extension de hoy y, si es `.fpreset`, tambien con `.py`: un
     * preset guardado antes de la migracion se tiene que poder borrar igual. */
    blender::Vector<std::string> exts;
    exts.append(ext);
    if (ext == ".fpreset") {
      exts.append(".py");
    }
    for (const std::string &ext_try : exts) {
      filepath = preset_find(name, subdir.c_str(), false, ext_try);
      if (filepath.empty()) {
        filepath = preset_find(name, subdir.c_str(), true, ext_try);
      }
      if (!filepath.empty()) {
        break;
      }
    }

    if (filepath.empty()) {
      return OPERATOR_CANCELLED;
    }
    if (path_is_readonly(filepath)) {
      BKE_report(op->reports, RPT_WARNING, "Unable to remove default presets");
      return OPERATOR_CANCELLED;
    }
    if (BLI_delete(filepath.c_str(), false, false) != 0) {
      BKE_reportf(op->reports, RPT_ERROR, RPT_("Unable to remove preset: %s"), filepath.c_str());
      return OPERATOR_CANCELLED;
    }

    /* El comentario del original decia "XXX, stupid!". Se conserva el comportamiento. */
    ui::menu_label_set(type.preset_menu, "Presets");
  }

  post_cb(type, C);
  return OPERATOR_FINISHED;
}

wmOperatorStatus add_preset_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  const AddPresetType *type_ptr = type_of(op);
  if (type_ptr == nullptr) {
    return OPERATOR_CANCELLED;
  }
  const bool remove_name = RNA_boolean_get(op->ptr, "remove_name");
  const bool remove_active = RNA_boolean_get(op->ptr, "remove_active");

  if (remove_name || remove_active) {
    if (type_ptr->kind == AddPresetKind::Keyconfig && type_ptr->remove_active_default) {
      /* `RemovePresetKeyconfig.invoke`: comprueba que la configuracion activa venga de
       * un fichero del usuario y pide confirmacion.
       *
       * Correccion deliberada respecto del Python: buscaba con `ext=".py"` aunque el
       * carril B4 ya habia cambiado la extension de la familia a `.fkeyconfig`, asi que
       * NUNCA encontraba nada y el operador estaba muerto. Aqui se busca con la
       * extension de la familia y, despues, con `.py` para las guardadas de antes.
       * Queda dicho en politicas/PRESETS-A-DATOS.md. */
      const wmKeyConfig *kc = keyconfig_active(CTX_wm_manager(C));
      const std::string name = kc ? kc->idname : "";
      std::string filepath;
      for (const char *ext_try : {".fkeyconfig", ".py"}) {
        filepath = preset_find(name, "keyconfig", false, ext_try);
        if (!filepath.empty()) {
          break;
        }
      }
      if (filepath.empty() || path_is_readonly(filepath)) {
        BKE_report(op->reports, RPT_ERROR, "Built-in keymap configurations cannot be removed");
        return OPERATOR_CANCELLED;
      }
      /* -1 es `ALERT_ICON_NONE`, que es el defecto de `invoke_confirm` en RNA (el de
       * `WM_operator_confirm_ex` a secas es `ALERT_ICON_WARNING`, que NO es lo que
       * hacia el Python). Se escribe el numero para no arrastrar aqui una cabecera de
       * `editors/include`. */
      return WM_operator_confirm_ex(C, op, "Remove Keymap Configuration", nullptr, "Delete", -1);
    }
    return add_preset_exec(C, op);
  }
  return WM_operator_props_dialog_popup(C, op, 300);
}

/** `AddPresetBase.check`: normaliza el nombre mientras el dialogo esta abierto. */
bool add_preset_check(bContext * /*C*/, wmOperator *op)
{
  char name_buf[256];
  RNA_string_get(op->ptr, "name", name_buf);
  const std::string fixed = as_filename(strip_py(name_buf));
  if (fixed == name_buf) {
    return false;
  }
  RNA_string_set(op->ptr, "name", fixed.c_str());
  return true;
}

/* -------------------------------------------------------------------------- */
/** \name `wm.operator_presets_cleanup`
 *
 * Quita de los presets de operador las propiedades que ya no valen. En el original era
 * un filtro de lineas con una expresion regular sobre ficheros `.py`.
 *
 * Se conserva ese filtro para los `.py` que el usuario tenga de antes, y se ANADE el
 * equivalente para `.fpreset`, que es el formato en el que se escriben hoy: sin eso el
 * operador seguiria existiendo pero no limpiaria nada de lo nuevo, o sea que la
 * capacidad se habria perdido en silencio al cambiar de formato. Queda dicho en
 * politicas/PRESETS-A-DATOS.md.
 * \{ */

/** Los operadores de importacion/exportacion que limpia cuando no se le dice cual. */
const char *cleanup_default_operators[] = {
    "WM_OT_alembic_export",
    "WM_OT_alembic_import",
    "WM_OT_collada_export",
    "WM_OT_collada_import",
    "WM_OT_obj_export",
    "WM_OT_obj_import",
    "WM_OT_ply_export",
    "WM_OT_ply_import",
    "WM_OT_stl_export",
    "WM_OT_stl_import",
    "WM_OT_usd_export",
    "WM_OT_usd_import",
};

const char *cleanup_default_properties[] = {
    "filepath",
    "directory",
    "files",
    "filename",
};

/**
 * `\b` de Python tras el nombre de la propiedad: el siguiente caracter no puede ser
 * alfanumerico ni `_`. Sin esto, quitar `files` se llevaria tambien `files_extra`.
 */
bool is_word_boundary(const std::string &line, const size_t pos)
{
  if (pos >= line.size()) {
    return true;
  }
  const unsigned char c = static_cast<unsigned char>(line[pos]);
  return !(std::isalnum(c) || c == '_');
}

/** Devuelve true si la linea hay que tirarla. */
bool cleanup_line_excluded(const std::string &line,
                           const blender::Span<std::string> properties,
                           const bool is_fpreset)
{
  for (const std::string &prop : properties) {
    /* `.py`: `op.<prop>` al principio de la linea (`regex.match`, que ancla). */
    const std::string py_prefix = "op." + prop;
    if (!is_fpreset && line.compare(0, py_prefix.size(), py_prefix) == 0 &&
        is_word_boundary(line, py_prefix.size()))
    {
      return true;
    }
    /* `.fpreset`: la misma propiedad, escrita como dato. */
    const std::string fp_prefix = "set context.active_operator." + prop;
    if (is_fpreset && line.compare(0, fp_prefix.size(), fp_prefix) == 0 &&
        is_word_boundary(line, fp_prefix.size()))
    {
      return true;
    }
  }
  return false;
}

void cleanup_preset_file(const std::string &filepath, const blender::Span<std::string> properties)
{
  if (!BLI_is_file(filepath.c_str())) {
    return;
  }
  std::string ext;
  const size_t dot = filepath.find_last_of('.');
  if (dot != std::string::npos) {
    ext = filepath.substr(dot);
    for (char &c : ext) {
      c = char(std::tolower(static_cast<unsigned char>(c)));
    }
  }
  const bool is_fpreset = (ext == ".fpreset");
  if (!is_fpreset && ext != ".py") {
    return;
  }

  size_t size = 0;
  void *buffer = BLI_file_read_text_as_mem(filepath.c_str(), 0, &size);
  if (buffer == nullptr) {
    return;
  }
  const std::string text(static_cast<const char *>(buffer), size);
  MEM_freeN(buffer);
  if (text.empty()) {
    return;
  }

  /* `splitlines(True)`: cada linea conserva su salto. */
  std::string out;
  size_t start = 0;
  bool changed = false;
  while (start < text.size()) {
    size_t end = text.find('\n', start);
    const size_t line_end = (end == std::string::npos) ? text.size() : end + 1;
    const std::string line = text.substr(start, line_end - start);
    if (cleanup_line_excluded(line, properties, is_fpreset)) {
      changed = true;
    }
    else {
      out += line;
    }
    start = line_end;
  }
  if (!changed) {
    return;
  }
  if (FILE *f = BLI_fopen(filepath.c_str(), "w")) {
    fwrite(out.data(), 1, out.size(), f);
    fclose(f);
  }
}

wmOperatorStatus operator_presets_cleanup_exec(bContext * /*C*/, wmOperator *op)
{
  blender::Vector<std::string> operators;
  blender::Vector<std::string> properties;

  char operator_buf[256];
  RNA_string_get(op->ptr, "operator", operator_buf);
  if (operator_buf[0] != '\0') {
    operators.append(operator_buf);
    RNA_BEGIN (op->ptr, item_ptr, "properties") {
      char *name = RNA_string_get_alloc(&item_ptr, "name", nullptr, 0, nullptr);
      if (name != nullptr) {
        properties.append(name);
        MEM_freeN(name);
      }
    }
    RNA_END;
  }
  else {
    for (const char *idname : cleanup_default_operators) {
      operators.append(idname);
    }
    for (const char *prop : cleanup_default_properties) {
      properties.append(prop);
    }
  }

  const std::optional<std::string> base = BKE_appdir_folder_id_user_notest(BLENDER_USER_SCRIPTS,
                                                                          "presets");
  if (!base || base->empty() || !BLI_is_dir(base->c_str())) {
    return OPERATOR_FINISHED;
  }

  for (const std::string &operator_idname : operators) {
    const std::string sub = operator_subdir(operator_idname);
    if (sub.empty()) {
      continue;
    }
    char directory[FILE_MAX];
    BLI_path_join(directory, sizeof(directory), base->c_str(), sub.c_str());
    if (!BLI_is_dir(directory)) {
      continue;
    }
    direntry *filelist = nullptr;
    const unsigned int count = BLI_filelist_dir_contents(directory, &filelist);
    for (unsigned int i = 0; i < count; i++) {
      const char *relname = filelist[i].relname;
      if (relname == nullptr || STREQ(relname, ".") || STREQ(relname, "..")) {
        continue;
      }
      char filepath[FILE_MAX];
      BLI_path_join(filepath, sizeof(filepath), directory, relname);
      cleanup_preset_file(filepath, properties);
    }
    if (filelist != nullptr) {
      BLI_filelist_free(filelist, count);
    }
  }
  return OPERATOR_FINISHED;
}

/** \} */

void add_preset_optype(wmOperatorType *ot, void *userdata)
{
  const AddPresetType &type = *static_cast<const AddPresetType *>(userdata);

  ot->name = type.name;
  ot->idname = type.idname;
  ot->description = type.description;

  ot->exec = add_preset_exec;
  ot->invoke = add_preset_invoke;
  ot->check = add_preset_check;
  /* `AddPresetBase.bl_options = {'REGISTER', 'INTERNAL'}`. */
  ot->flag = OPTYPE_REGISTER | OPTYPE_INTERNAL;

  PropertyRNA *prop;

  /* `maxlen=64` de Python se registra como 65: `bpy_props` reserva el byte nulo
   * (trampa 2 de politicas/OBJECT-SELECT-A-CPP.md). */
  prop = RNA_def_string(ot->srna,
                        "name",
                        nullptr,
                        65,
                        "Name",
                        "Name of the preset, used to make the path name");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);

  /* Las propiedades definidas desde Python NO son animables; las de C++ lo son por
   * defecto. Sin este `clear_flag` la superficie de registro cambia y lo canta
   * `--fl-check-preset-optypes`. */
  prop = RNA_def_boolean(ot->srna, "remove_name", false, "remove_name", "");
  RNA_def_property_flag(prop, PropertyFlag(PROP_HIDDEN | PROP_SKIP_SAVE));
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  prop = RNA_def_boolean(
      ot->srna, "remove_active", type.remove_active_default, "remove_active", "");
  RNA_def_property_flag(prop, PropertyFlag(PROP_HIDDEN | PROP_SKIP_SAVE));
  RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);

  if (type.has_focal_length) {
    prop = RNA_def_boolean(ot->srna,
                           "use_focal_length",
                           type.focal_length_default,
                           "Include Focal Length",
                           "Include focal length into the preset");
    RNA_def_property_flag(prop, PROP_SKIP_SAVE);
    RNA_def_property_clear_flag(prop, PROP_ANIMATABLE);
  }

  if (type.has_operator_prop) {
    prop = RNA_def_string(ot->srna, "operator", nullptr, 65, "Operator", "");
    RNA_def_property_flag(prop, PropertyFlag(PROP_HIDDEN | PROP_SKIP_SAVE));
  }
}

void operator_presets_cleanup_optype(wmOperatorType *ot)
{
  ot->name = "Clean Up Operator Presets";
  ot->idname = "WM_OT_operator_presets_cleanup";
  ot->description = "Remove outdated operator properties from presets that may cause problems";

  ot->exec = operator_presets_cleanup_exec;
  /* La clase de Python no declaraba `bl_options`, y `bl_options` es
   * `PROP_REGISTER_OPTIONAL`: el tipo se quedaba con `flag` a cero. */
  ot->flag = 0;

  /* `StringProperty(name="operator")` sin `maxlen`: cadena dinamica (maxlen 0). */
  PropertyRNA *prop = RNA_def_string(ot->srna, "operator", nullptr, 0, "operator", "");
  /* `CollectionProperty(name="properties", type=OperatorFileListElement)`.
   *
   * No vale `RNA_def_collection()`: su version con el tipo por NOMBRE solo funciona
   * durante el preproceso de makesrna y en ejecucion imprime
   * «".properties": only during preprocessing» y deja la coleccion SIN tipo de
   * elemento -- una coleccion que no se puede recorrer. En ejecucion el tipo se pone
   * con el puntero, que es lo que hace `bpy_props` para `CollectionProperty`. */
  prop = RNA_def_property(ot->srna, "properties", PROP_COLLECTION, PROP_NONE);
  RNA_def_property_struct_runtime(ot->srna, prop, &RNA_OperatorFileListElement);
  RNA_def_property_ui_text(prop, "properties", "");
}

}  // namespace

void register_add_preset_types()
{
  for (const AddPresetType &type : add_preset_types) {
    WM_operatortype_append_ptr(add_preset_optype, const_cast<AddPresetType *>(&type));
  }
  WM_operatortype_append(operator_presets_cleanup_optype);

  /* `WindowManager.preset_name`, que `presets.py` anadia a la clase con
   * `StringProperty(...)` en cuanto se importaba el modulo. Es el campo de texto del
   * popover de presets, y lo LEE codigo nativo: `fl_preset_ui.cc` lo dibuja y saca de
   * ahi el nombre con el que llama al operador de anadir. Sin esto, retirar el Python
   * deja el panel nativo sin propiedad y sin nombre.
   *
   * Se define igual que lo hacia Python: `RNA_def_property()` en ejecucion pone solo
   * `PROP_IDPROPERTY`, asi que el valor vive en las propiedades de identificador del
   * gestor de ventanas, exactamente donde vivia antes.
   * `RNA_def_property_duplicate_pointers` hace que RNA se quede con copias propias de
   * los textos, que es lo que hace `bpy_props` y lo que evita depender de la cache de
   * traduccion. */
  StructRNA *srna_wm = &RNA_WindowManager;
  PropertyRNA *prop = RNA_def_property(srna_wm, "preset_name", PROP_STRING, PROP_NONE);
  RNA_def_property_string_default(prop, DATA_("New Preset"));
  RNA_def_property_ui_text(prop, "Preset Name", "Name for new preset");
  RNA_def_property_duplicate_pointers(srna_wm, prop);
}

/** \} */

}  // namespace flipendo::preset
