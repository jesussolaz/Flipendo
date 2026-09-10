/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Conversion de los presets heredados y verificacion con evidencia.
 *
 * `--fl-convert-presets <dir>` reescribe cada `.py` como `.fpreset` usando el
 * MISMO lector nativo que dara servicio a los presets del usuario. No es una
 * herramienta aparte que "traduce" y luego habria que confiar en ella: si el
 * lector se equivoca, se equivoca en los dos sitios y la comprobacion lo caza.
 *
 * `--fl-check-presets <dir> <informe>` aplica cada preset por los dos caminos
 * -- el Python de hoy y el C++ nuevo -- sobre el mismo estado de partida, y
 * compara el estado resultante propiedad a propiedad.
 *
 * La trampa de esta comprobacion es el CONTEXTO: `bpy.context.camera` no existe
 * en segundo plano. Se resuelve empujando los datos que hacen falta al almacen
 * de contexto (`CTX_store_set`), que es justo lo primero que mira `ctx_data_get`
 * y por donde pasan tanto el interprete como el resolvedor nativo. Asi los dos
 * caminos ven exactamente el mismo objeto.
 */

#include "FL_preset.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "MEM_guardedalloc.h"

#include "BLI_fileops.h"
#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "BKE_camera.h"
#include "BKE_context.hh"
#include "BKE_fluid.h"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_mesh.hh"
#include "BKE_material.hh"
#include "BKE_modifier.hh"
#include "BKE_movieclip.h"
#include "BKE_object.hh"
#include "BKE_tracking.h"

#include "DNA_camera_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_movieclip_types.h"
#include "DNA_object_types.h"
#include "DNA_tracking_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#ifdef WITH_PYTHON
#  include "BPY_extern_run.hh"
#endif

namespace flipendo::preset {

namespace {

/* -------------------------------------------------------------------------- */
/** \name Recorrido de directorios
 * \{ */

void collect_files(const std::string &dir, const char *ext, std::vector<std::string> &r_out)
{
  direntry *filelist = nullptr;
  const unsigned int count = BLI_filelist_dir_contents(dir.c_str(), &filelist);
  std::vector<std::string> subdirs;
  for (unsigned int i = 0; i < count; i++) {
    const char *name = filelist[i].relname;
    if (STREQ(name, ".") || STREQ(name, "..")) {
      continue;
    }
    std::string full = dir + "/" + name;
    if (BLI_is_dir(full.c_str())) {
      subdirs.push_back(full);
      continue;
    }
    const size_t dot = full.find_last_of('.');
    if (dot != std::string::npos && full.compare(dot, std::string::npos, ext) == 0) {
      r_out.push_back(full);
    }
  }
  if (filelist) {
    BLI_filelist_free(filelist, count);
  }
  std::sort(subdirs.begin(), subdirs.end());
  for (const std::string &sub : subdirs) {
    collect_files(sub, ext, r_out);
  }
  std::sort(r_out.begin(), r_out.end());
}

std::string swap_ext(const std::string &path, const char *ext)
{
  const size_t dot = path.find_last_of('.');
  return (dot == std::string::npos ? path : path.substr(0, dot)) + ext;
}

/** El `subdir` del preset: lo que hay entre la raiz y el nombre del fichero. */
std::string subdir_of(const std::string &root, const std::string &path)
{
  if (path.compare(0, root.size(), root) != 0) {
    return "";
  }
  std::string rel = path.substr(root.size());
  while (!rel.empty() && (rel[0] == '/' || rel[0] == '\\')) {
    rel = rel.substr(1);
  }
  const size_t slash = rel.find_last_of('/');
  return slash == std::string::npos ? "" : rel.substr(0, slash);
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name Contexto sintetico para la comprobacion
 * \{ */

struct HarnessContext {
  blender::Vector<std::unique_ptr<bContextStore>> owner;
  bContextStore *store = nullptr;
  std::vector<std::string> missing;
  /* Los datos que monta el arnes se borran al terminar. Dejarlos puestos hace
   * que `ED_editors_exit` recorra un objeto que no esta en ninguna escena y el
   * proceso se caiga al salir -- verificado con backtrace. */
  std::vector<ID *> made;

  void add(const char *name, const PointerRNA &ptr, const char *why)
  {
    if (ptr.data == nullptr) {
      missing.push_back(std::string(name) + " (" + why + ")");
      return;
    }
    store = CTX_store_add(owner, name, &ptr);
  }
};

void harness_build(bContext *C, HarnessContext &h)
{
  Main *bmain = CTX_data_main(C);

  /* `camera` -> los 31 presets de sensor de camara. */
  if (Camera *cam = BKE_camera_add(bmain, "FL_PresetCamera")) {
    h.made.push_back(&cam->id);
    h.add("camera", RNA_id_pointer_create(&cam->id), "BKE_camera_add");
  }

  /* `edit_movieclip` -> seguimiento: camara, ajustes y color de marcador.
   * No sirve `BKE_movieclip_file_add`: sin un fichero de video de verdad
   * devuelve null. El ID vacio basta, porque `MovieTracking` va empotrado en el
   * `MovieClip` y su `init_data` ya deja los ajustes por defecto puestos. */
  if (MovieClip *clip = BKE_id_new<MovieClip>(bmain, "FL_PresetClip")) {
    if (MovieTrackingObject *tobj = BKE_tracking_object_get_active(&clip->tracking)) {
      if (MovieTrackingTrack *track = BKE_tracking_track_add_empty(&clip->tracking, &tobj->tracks))
      {
        tobj->active_track = track;
      }
    }
    h.made.push_back(&clip->id);
    h.add("edit_movieclip", RNA_id_pointer_create(&clip->id), "BKE_id_new<MovieClip>");
  }

  /* `cloth` y `fluid` -> modificadores de un objeto de verdad. El puntero lleva
   * el ID del objeto como propietario porque las llamadas de actualizacion de
   * RNA (`rna_cloth_update`) lo desreferencian sin comprobarlo.
   * `object` -> el mismo objeto, con una malla y un material de lapiz de grasa,
   * que es lo que piden los presets de `gpencil_material`. */
  Object *ob = BKE_object_add_only_object(bmain, OB_MESH, "FL_PresetObject");
  if (ob) {
    h.made.push_back(&ob->id);
    if (Mesh *mesh = BKE_id_new<Mesh>(bmain, "FL_PresetMesh")) {
      ob->data = mesh;
      if (Material *ma = BKE_gpencil_material_add(bmain, "FL_PresetGPMaterial")) {
        BKE_object_material_assign(bmain, ob, ma, 1, BKE_MAT_ASSIGN_OBDATA);
        ob->actcol = 1;
      }
    }
    if (ModifierData *md = BKE_modifier_new(eModifierType_Cloth)) {
      BLI_addtail(&ob->modifiers, md);
      h.add("cloth",
            RNA_pointer_create_discrete(&ob->id, &RNA_ClothModifier, md),
            "BKE_modifier_new(Cloth)");
    }
    if (ModifierData *md = BKE_modifier_new(eModifierType_Fluid)) {
      FluidModifierData *fmd = reinterpret_cast<FluidModifierData *>(md);
      fmd->type = MOD_FLUID_TYPE_DOMAIN;
      BKE_fluid_modifier_create_type_data(fmd);
      BLI_addtail(&ob->modifiers, md);
      h.add("fluid",
            RNA_pointer_create_discrete(&ob->id, &RNA_FluidModifier, md),
            "BKE_modifier_new(Fluid)");
    }
    h.add("object", RNA_id_pointer_create(&ob->id), "BKE_object_add_only_object");
  }

  if (h.store) {
    CTX_store_set(C, h.store);
  }
}

void harness_free(bContext *C, HarnessContext &h)
{
  CTX_store_set(C, nullptr);
  Main *bmain = CTX_data_main(C);
  /* Al reves de como se crearon: el objeto antes que su malla y su material. */
  for (auto it = h.made.rbegin(); it != h.made.rend(); ++it) {
    BKE_id_delete(bmain, *it);
  }
  h.made.clear();
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name Estado observable
 * \{ */

/** Rutas tocadas por el preset, sin repetir y en el orden en que aparecen. */
std::vector<std::string> touched_paths(const Preset &preset)
{
  std::vector<std::string> out;
  for (const Op &op : preset.ops) {
    if (op.kind != OpKind::Set || op.path.empty() || op.path[0] == '.') {
      continue;
    }
    if (std::find(out.begin(), out.end(), op.path) == out.end()) {
      out.push_back(op.path);
    }
  }
  return out;
}

std::string state_text(bContext *C, const std::vector<std::string> &paths)
{
  std::string out;
  for (const std::string &path : paths) {
    Preset one;
    std::string error;
    std::vector<std::string> single{path};
    if (capture(C, single, one, error)) {
      out += path + " = " + format_value(one.ops.front().value) + "\n";
    }
    else {
      out += path + " ! " + error + "\n";
    }
  }
  return out;
}

std::string python_repr(const std::string &s)
{
  std::string out = "'";
  for (const char c : s) {
    if (c == '\\' || c == '\'') {
      out += '\\';
    }
    out += c;
  }
  out += '\'';
  return out;
}

/** \} */

}  // namespace

/* -------------------------------------------------------------------------- */
/** \name Conversion
 * \{ */

bool convert_tree(const char *dir)
{
  const std::string root(dir);
  std::vector<std::string> files;
  collect_files(root, ".py", files);

  int ok = 0, skipped = 0;
  for (const std::string &path : files) {
    /* El keymap por defecto ya es C++ (politicas/KEYMAP-A-CPP.md); su preset no
     * es una lista de asignaciones y no se toca aqui. */
    if (subdir_of(root, path).compare(0, 9, "keyconfig") == 0) {
      printf("OMITIDO  %s (familia keyconfig, ver politicas/PRESETS-A-DATOS.md)\n", path.c_str());
      skipped++;
      continue;
    }
    Preset preset;
    std::string error;
    if (!read_file(path, preset, error)) {
      printf("NO-DATO  %s\n         %s\n", path.c_str(), error.c_str());
      skipped++;
      continue;
    }
    preset.subdir = subdir_of(root, path);
    const std::string out = swap_ext(path, ".fpreset");
    if (!write_file(out, preset, error)) {
      printf("ERROR    %s\n         %s\n", out.c_str(), error.c_str());
      skipped++;
      continue;
    }
    ok++;
  }
  printf("\npresets convertidos: %d de %d (%d sin convertir)\n", ok, int(files.size()), skipped);
  return skipped == 0;
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name Comprobacion de los dos caminos
 * \{ */

bool check_tree(bContext *C, const char *dir, const char *report_path)
{
  const std::string root(dir);
  std::vector<std::string> files;
  collect_files(root, ".py", files);

  HarnessContext harness;
  harness_build(C, harness);

  std::ostringstream rep;
  rep << "# Presets: mismo estado por el camino Python y por el camino C++\n";
  rep << "#\n";
  rep << "# Para cada preset se captura el estado de las rutas que toca, se aplica\n";
  rep << "# el .py con el interprete, se restaura, se aplica el .fpreset con el\n";
  rep << "# lector nativo y se comparan los dos estados.\n#\n";
  for (const std::string &m : harness.missing) {
    rep << "# contexto NO construido: " << m << "\n";
  }
  rep << "\n";

  int same = 0, diff = 0, skipped = 0, total = 0;

#ifndef WITH_PYTHON
  rep << "SIN-PYTHON: esta build no lleva interprete, no hay con que comparar.\n";
#endif

  for (const std::string &py : files) {
    if (subdir_of(root, py).compare(0, 9, "keyconfig") == 0) {
      continue;
    }
    const std::string fp = swap_ext(py, ".fpreset");
    if (!BLI_exists(fp.c_str())) {
      continue;
    }
    total++;

    Preset py_data;
    std::string error;
    if (!read_file(py, py_data, error)) {
      rep << "OMITIDO  " << py << "\n         " << error << "\n";
      skipped++;
      continue;
    }
    const std::vector<std::string> paths = touched_paths(py_data);
    if (paths.empty()) {
      rep << "OMITIDO  " << py << "\n         no toca ninguna propiedad\n";
      skipped++;
      continue;
    }

    /* Estado de partida. Si no se puede capturar, el contexto de esta familia no
     * esta montado y la comparacion no probaria nada: se dice y se pasa. */
    Preset before;
    if (!capture(C, paths, before, error)) {
      rep << "OMITIDO  " << py << "\n         contexto no disponible: " << error << "\n";
      skipped++;
      continue;
    }

    std::string after_python;
#ifdef WITH_PYTHON
    {
      const std::string cmd = "import bpy\nbpy.utils.execfile(" + python_repr(py) + ")\n";
      if (!BPY_run_string_exec(C, nullptr, cmd.c_str())) {
        rep << "OMITIDO  " << py << "\n         el interprete fallo al ejecutarlo\n";
        skipped++;
        ApplyReport restore;
        apply(C, before, restore);
        continue;
      }
    }
    after_python = state_text(C, paths);
#else
    skipped++;
    continue;
#endif

    /* Volver al estado de partida antes del camino nativo. */
    {
      ApplyReport restore;
      apply(C, before, restore);
    }

    Preset native;
    if (!read_file(fp, native, error)) {
      rep << "ERROR    " << fp << "\n         " << error << "\n";
      diff++;
      continue;
    }
    ApplyReport nrep;
    apply(C, native, nrep);
    const std::string after_native = state_text(C, paths);

    if (after_python == after_native && nrep.errors.empty()) {
      same++;
    }
    else {
      diff++;
      rep << "DISTINTO " << py << "\n";
      for (const std::string &e : nrep.errors) {
        rep << "         error nativo: " << e << "\n";
      }
      std::istringstream a(after_python), b(after_native);
      std::string la, lb;
      while (std::getline(a, la)) {
        if (!std::getline(b, lb)) {
          lb = "<falta>";
        }
        if (la != lb) {
          rep << "         python: " << la << "\n";
          rep << "         c++   : " << lb << "\n";
        }
      }
    }

    ApplyReport restore;
    apply(C, before, restore);
  }

  rep << "\nidenticos " << same << " de " << total << " comparados";
  rep << " (" << diff << " distintos, " << skipped << " sin comparar)\n";

  /* Segunda pasada: leer y aplicar TODOS los `.fpreset`, con `.py` al lado o
   * sin el. La comparacion de arriba se queda sin material el dia que se retire
   * el Python; esta no, y sigue cazando lo que de verdad se rompe con el tiempo:
   * un fichero mal escrito a mano o una ruta RNA que desaparece. */
  std::vector<std::string> natives;
  collect_files(root, ".fpreset", natives);
  int applied = 0, failed = 0, no_context = 0;
  rep << "\n# Segunda pasada: cada .fpreset se lee y se aplica.\n\n";
  for (const std::string &fp : natives) {
    Preset preset;
    std::string error;
    if (!read_file(fp, preset, error)) {
      rep << "ILEGIBLE " << fp << "\n         " << error << "\n";
      failed++;
      continue;
    }
    const std::vector<std::string> paths = touched_paths(preset);
    Preset before;
    if (!paths.empty() && !capture(C, paths, before, error)) {
      /* Ni el contexto ni la propiedad estan: no se puede aplicar ni juzgar. */
      rep << "SIN-CTX  " << fp << "\n         " << error << "\n";
      no_context++;
      continue;
    }
    ApplyReport areport;
    apply(C, preset, areport);
    if (areport.errors.empty()) {
      applied++;
    }
    else {
      failed++;
      rep << "ERROR    " << fp << "\n";
      for (const std::string &e : areport.errors) {
        rep << "         " << e << "\n";
      }
    }
    ApplyReport restore;
    apply(C, before, restore);
  }
  rep << "\naplicados sin error " << applied << " de " << int(natives.size())
      << " (" << failed << " con error, " << no_context << " sin contexto)\n";

  const std::string text = rep.str();
  fputs(text.c_str(), stdout);
  if (report_path && report_path[0]) {
    std::ofstream out(report_path, std::ios::binary | std::ios::trunc);
    out << text;
  }
  harness_free(C, harness);
  return diff == 0 && failed == 0 && (same > 0 || applied > 0);
}

/** \} */

}  // namespace flipendo::preset
