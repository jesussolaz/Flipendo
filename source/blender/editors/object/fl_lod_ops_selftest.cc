/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 *
 * `--fl-selftest-lod-ops` / `--fl-check-lod-ops`: la prueba de conducta de los tres
 * operadores de niveles de detalle que estaban en `bl_operators/object.py`.
 *
 * Once escenas deterministas, construidas llamando SOLO a operadores por su idname
 * (borrar todo, anadir cubos, renombrar por RNA), para que la bateria en Python y la
 * bateria en C++ partan exactamente del mismo sitio. Se vuelca, por caso: el resultado
 * del operador, todos los objetos de `bmain` con su seleccion, si son el activo, su
 * posicion y sus modificadores, y la lista de niveles de detalle del objeto activo con
 * su objeto, distancia, histeresis y banderas.
 *
 * Lo que NO entra en la bateria, y por que: `lod_generate` con `count` menor que 2 y
 * con `package=True`. Las dos ramas revientan en el original (division por cero /
 * variable sin asignar la primera; operadores de Blender 2.7x que ya no existen la
 * segunda) y dejan la escena a medio construir, asi que no hay conducta que congelar.
 * Estan medidas y escritas en politicas/LOD-A-CPP.md.
 */

#include <cstdio>
#include <string>

#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"

#include "BKE_appdir.hh"
#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_main.hh"

#include "ED_select_utils.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "FL_lod_ops_selftest.hh"
#include "FL_selftest_compare.hh"

namespace flipendo::lod_selftest {

namespace {

const char *status_name(const wmOperatorStatus status)
{
  if (status & OPERATOR_FINISHED) {
    return "FINISHED";
  }
  if (status & OPERATOR_CANCELLED) {
    return "CANCELLED";
  }
  return "OTRO";
}

wmOperatorStatus call_op(bContext *C, const char *idname, PointerRNA *props)
{
  return WM_operator_name_call(C, idname, WM_OP_EXEC_DEFAULT, props, nullptr);
}

void scene_clear(bContext *C)
{
  PointerRNA props;
  WM_operator_properties_create(&props, "OBJECT_OT_select_all");
  /* SEL_SELECT == 1; el 2 es DESELECT, y con el la bateria no borraba nada. */
  RNA_enum_set(&props, "action", SEL_SELECT);
  call_op(C, "OBJECT_OT_select_all", &props);
  WM_operator_properties_free(&props);

  WM_operator_properties_create(&props, "OBJECT_OT_delete");
  RNA_boolean_set(&props, "use_global", false);
  RNA_boolean_set(&props, "confirm", false);
  call_op(C, "OBJECT_OT_delete", &props);
  WM_operator_properties_free(&props);
}

/** Anade un cubo y lo renombra. Devuelve el objeto activo resultante. */
Object *cube_add(bContext *C, const char *name)
{
  PointerRNA props;
  WM_operator_properties_create(&props, "MESH_OT_primitive_cube_add");
  RNA_float_set(&props, "size", 2.0f);
  call_op(C, "MESH_OT_primitive_cube_add", &props);
  WM_operator_properties_free(&props);

  Object *ob = CTX_data_active_object(C);
  if (ob != nullptr) {
    PointerRNA ob_ptr = RNA_id_pointer_create(&ob->id);
    RNA_string_set(&ob_ptr, "name", name);
  }
  return ob;
}

void active_set(bContext *C, Object *ob)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  if (Base *base = BKE_view_layer_base_find(view_layer, ob)) {
    view_layer->basact = base;
  }
}

std::string fmt_float(const float value)
{
  char buf[64];
  BLI_snprintf(buf, sizeof(buf), "%.9g", double(value));
  return buf;
}

void dump_state(FILE *f, int &n, bContext *C)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  const Object *active = BKE_view_layer_active_object_get(view_layer);

  LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
    const Base *base = BKE_view_layer_base_find(view_layer, ob);
    const bool selected = base && (base->flag & BASE_SELECTED) != 0;
    /* El tipo se lee por RNA y no como numero de DNA: el guion que congelo la linea
     * base solo ve `modifier.type`, que es el identificador de la enumeracion. */
    std::string mods;
    LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
      if (!mods.empty()) {
        mods += ";";
      }
      PointerRNA md_ptr = RNA_pointer_create_discrete(&ob->id, &RNA_Modifier, md);
      PropertyRNA *type_prop = RNA_struct_find_property(&md_ptr, "type");
      const char *type_id = "";
      RNA_property_enum_identifier(
          nullptr, &md_ptr, type_prop, RNA_property_enum_get(&md_ptr, type_prop), &type_id);
      mods += std::string(md->name) + ":" + type_id;
      if (md->type == eModifierType_Decimate) {
        mods += ":" + fmt_float(RNA_float_get(&md_ptr, "ratio"));
      }
    }
    fprintf(f,
            "  %d obj=%s sel=%d act=%d loc=%s,%s,%s mods=%s\n",
            n++,
            ob->id.name + 2,
            selected ? 1 : 0,
            (ob == active) ? 1 : 0,
            fmt_float(ob->loc[0]).c_str(),
            fmt_float(ob->loc[1]).c_str(),
            fmt_float(ob->loc[2]).c_str(),
            mods.c_str());
  }

  if (active != nullptr) {
    int index = 0;
    LISTBASE_FOREACH (const LodLevel *, level, &active->lodlevels) {
      fprintf(f,
              "  %d lod=%d obj=%s distance=%s hyst=%d flags=%d\n",
              n++,
              index++,
              level->source ? level->source->id.name + 2 : "-",
              fmt_float(level->distance).c_str(),
              level->obhysteresis,
              level->flags);
    }
  }
}

/** Los nombres de una escena de niveles de detalle, en orden de creacion. */
struct SceneCase {
  const char *title;
  /** Hasta seis objetos; el primero es el que queda activo antes de llamar. */
  const char *names[6];
  const char *op;
  /** Solo para `lod_generate`. */
  int count;
  float target;
  /** Llama al operador DOS veces (prueba de idempotencia). */
  bool twice;
  /** Monta los niveles con `lod_by_name` ANTES de llamar al operador del caso. */
  bool setup_by_name;
};

const SceneCase scene_cases[] = {
    {"by_name prefijo",
     {"LOD0Mesh", "LOD1Mesh", "LOD2Mesh", nullptr},
     "OBJECT_OT_lod_by_name",
     0,
     0.0f,
     false},
    {"by_name sufijo",
     {"MeshLOD0", "MeshLOD1", "MeshLOD2", nullptr},
     "OBJECT_OT_lod_by_name",
     0,
     0.0f,
     false},
    {"by_name minusculas y hueco en el 2",
     {"obj_lod0", "obj_lod1", "obj_lod3", nullptr},
     "OBJECT_OT_lod_by_name",
     0,
     0.0f,
     false},
    {"by_name sin lod0 en el nombre",
     {"Cubo", "Cubo.001", nullptr},
     "OBJECT_OT_lod_by_name",
     0,
     0.0f,
     false},
    {"by_name dos veces seguidas",
     {"LOD0Rep", "LOD1Rep", "LOD2Rep", nullptr},
     "OBJECT_OT_lod_by_name",
     0,
     0.0f,
     true},
    {"by_name con un solo nivel",
     {"LOD0Solo", nullptr},
     "OBJECT_OT_lod_by_name",
     0,
     0.0f,
     false},
    /* Los demas dejan `setup_by_name` a false por inicializacion agregada. */
    {"clear_all con niveles",
     {"LOD0Clear", "LOD1Clear", "LOD2Clear", nullptr},
     "OBJECT_OT_lod_clear_all",
     0,
     0.0f,
     false,
     true},
    {"clear_all sin niveles",
     {"Pelado", nullptr},
     "OBJECT_OT_lod_clear_all",
     0,
     0.0f,
     false},
    {"generate count=3 target=0.1",
     {"Cube", nullptr},
     "OBJECT_OT_lod_generate",
     3,
     0.1f,
     false},
    {"generate count=2 target=0.5 con prefijo",
     {"LOD0Gen", nullptr},
     "OBJECT_OT_lod_generate",
     2,
     0.5f,
     false},
    {"generate count=4 target=0.2 con sufijo",
     {"Genlod0", nullptr},
     "OBJECT_OT_lod_generate",
     4,
     0.2f,
     false},
};

void run_case(bContext *C, FILE *f, const int index, const SceneCase &sc)
{
  scene_clear(C);

  Object *first = nullptr;
  for (const char *name : sc.names) {
    if (name == nullptr) {
      break;
    }
    Object *ob = cube_add(C, name);
    if (first == nullptr) {
      first = ob;
    }
  }
  if (first != nullptr) {
    active_set(C, first);
  }

  if (sc.setup_by_name) {
    PointerRNA setup_props;
    WM_operator_properties_create(&setup_props, "OBJECT_OT_lod_by_name");
    call_op(C, "OBJECT_OT_lod_by_name", &setup_props);
    WM_operator_properties_free(&setup_props);
  }

  PointerRNA props;
  WM_operator_properties_create(&props, sc.op);
  if (STREQ(sc.op, "OBJECT_OT_lod_generate")) {
    RNA_int_set(&props, "count", sc.count);
    RNA_float_set(&props, "target", sc.target);
    RNA_boolean_set(&props, "package", false);
  }
  wmOperatorStatus status = call_op(C, sc.op, &props);
  if (sc.twice) {
    status = call_op(C, sc.op, &props);
  }
  WM_operator_properties_free(&props);

  fprintf(f, "case=%d %s\n", index, sc.title);
  int n = 0;
  fprintf(f, "  %d status=%s\n", n++, status_name(status));
  dump_state(f, n, C);
}

}  // namespace

bool dump(bContext *C, const char *filepath)
{
  FILE *f = fopen(filepath, "w");
  if (f == nullptr) {
    fprintf(stderr, "fl-selftest-lod-ops: no se pudo escribir '%s'\n", filepath);
    return false;
  }
  fprintf(f, "# FL-LOD-OPS v1\n");
  int index = 0;
  for (const SceneCase &sc : scene_cases) {
    run_case(C, f, index++, sc);
  }
  fclose(f);
  fprintf(stderr, "fl-selftest-lod-ops: volcado en '%s' (%d casos)\n", filepath, index);
  return true;
}

bool check(bContext *C, const char *baseline_path)
{
  char actual_path[FILE_MAX];
  BLI_path_join(actual_path, sizeof(actual_path), BKE_tempdir_session(), "fl-lod-ops-actual.txt");
  if (!dump(C, actual_path)) {
    return false;
  }
  return flipendo::selftest::compare_to_baseline("fl-check-lod-ops", actual_path, baseline_path);
}

}  // namespace flipendo::lod_selftest
