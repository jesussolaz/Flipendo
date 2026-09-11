/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) Blender Foundation
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/object/object_lod.c
 *  \ingroup edobj
 */

#include <cctype>
#include <string>

#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_string_utf8.h"

#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_modifier.hh"
#include "BKE_report.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "ED_transform.hh"

#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "ED_object.hh"
#include "ED_screen.hh"

#ifdef WITH_GAMEENGINE
#  include "BKE_object.hh"

#  include "RNA_enum_types.hh"
#endif

#include "object_intern.hh"

namespace blender::ed::object {

static wmOperatorStatus object_lod_add_exec(bContext *C, wmOperator */*op*/)
{
  Object *ob = blender::ed::object::context_object(C);

#ifdef WITH_GAMEENGINE
  BKE_object_lod_add(ob);
#else
  (void)ob;
#endif

  return OPERATOR_FINISHED;
}

void OBJECT_OT_lod_add(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Add Level of Detail";
  ot->description = "Add a level of detail to this object";
  ot->idname = "OBJECT_OT_lod_add";

  /* api callbacks */
  ot->exec = object_lod_add_exec;
  ot->poll = ED_operator_object_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus object_lod_remove_exec(bContext *C, wmOperator *op)
{
  Object *ob = blender::ed::object::context_object(C);
  int index = RNA_int_get(op->ptr, "index");

#ifdef WITH_GAMEENGINE
  if (!BKE_object_lod_remove(ob, index))
    return OPERATOR_CANCELLED;
#else
  (void)ob;
  (void)index;
#endif

  WM_event_add_notifier(C, NC_OBJECT | ND_LOD, CTX_wm_view3d(C));
  return OPERATOR_FINISHED;
}

void OBJECT_OT_lod_remove(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Remove Level of Detail";
  ot->description = "Remove a level of detail from this object";
  ot->idname = "OBJECT_OT_lod_remove";

  /* api callbacks */
  ot->exec = object_lod_remove_exec;
  ot->poll = ED_operator_object_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  ot->prop = RNA_def_int(ot->srna, "index", 1, 1, INT_MAX, "Index", "", 1, INT_MAX);
}

/* -------------------------------------------------------------------- */
/** \name Los tres de `bl_operators/object.py`, transliterados
 *
 * `object.lod_by_name`, `object.lod_clear_all` y `object.lod_generate` eran las unicas
 * clases de Python del sistema de niveles de detalle -- que es una capacidad del MOTOR,
 * no del editor: es lo que decide que malla se dibuja segun la distancia a la camara.
 * El resto (`lod_add`, `lod_remove`, el `LodLevel` del DNA y su RNA) siempre fue C.
 *
 * Se transliteran linea a linea, incluido lo que el original hace mal, salvo donde se
 * dice lo contrario. Ver politicas/LOD-A-CPP.md.
 * \{ */

/** `str.lower()` ASCII, que es lo que hacen `startswith`/`endswith` tras `.lower()`. */
static std::string lower_ascii(const blender::StringRef text)
{
  std::string out(text);
  for (char &c : out) {
    c = char(std::tolower(static_cast<unsigned char>(c)));
  }
  return out;
}

static Object *object_by_name(Main *bmain, const std::string &name)
{
  /* `bpy.data.objects[nombre]`: busca por nombre en `bmain->objects`. */
  return static_cast<Object *>(
      BLI_findstring(&bmain->objects, name.c_str(), offsetof(ID, name) + 2));
}

/**
 * `context.view_layer.objects.active = ob`, que es literalmente lo que hace
 * `rna_LayerObjects_active_object_set`: buscar la base y apuntar `basact`. No se usa
 * `base_activate()` porque esa ademas puede salir del modo actual, y el original no lo
 * hacia.
 */
static void view_layer_active_object_set(bContext *C, Object *ob)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  if (Base *base = BKE_view_layer_base_find(view_layer, ob)) {
    view_layer->basact = base;
  }
}

/** `ob.lod_levels[index].object = source`, por RNA para que dispare la actualizacion. */
static void lod_level_object_set(Object *ob, const int index, Object *source)
{
  LodLevel *level = static_cast<LodLevel *>(BLI_findlink(&ob->lodlevels, index));
  if (level == nullptr) {
    return;
  }
  PointerRNA level_ptr = RNA_pointer_create_discrete(&ob->id, &RNA_LodLevel, level);
  PointerRNA source_ptr = RNA_id_pointer_create(&source->id);
  PropertyRNA *prop = RNA_struct_find_property(&level_ptr, "object");
  RNA_property_pointer_set(&level_ptr, prop, source_ptr, nullptr);
  RNA_property_update(nullptr, &level_ptr, prop);
}

static wmOperatorStatus object_lod_by_name_exec(bContext *C, wmOperator * /*op*/)
{
  Main *bmain = CTX_data_main(C);
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return OPERATOR_CANCELLED;
  }

  const std::string ob_name(ob->id.name + 2);
  const std::string lowered = lower_ascii(ob_name);

  std::string prefix;
  std::string suffix;
  std::string name;
  if (lowered.compare(0, 4, "lod0") == 0) {
    prefix = ob_name.substr(0, 4);
    name = ob_name.substr(4);
  }
  else if (lowered.size() >= 4 && lowered.compare(lowered.size() - 4, 4, "lod0") == 0) {
    name = ob_name.substr(0, ob_name.size() - 4);
    suffix = ob_name.substr(ob_name.size() - 4);
  }
  else {
    return OPERATOR_CANCELLED;
  }

  int level = 0;
  while (true) {
    level += 1;

    /* `prefix = prefix[:3] + str(level)`: el digito NO se limita a uno, asi que a
     * partir del nivel 10 el nombre buscado tiene una letra mas. Es lo que hacia el
     * original y se conserva. */
    if (!prefix.empty()) {
      prefix = prefix.substr(0, 3) + std::to_string(level);
    }
    if (!suffix.empty()) {
      suffix = suffix.substr(0, 3) + std::to_string(level);
    }

    Object *lod = object_by_name(bmain, prefix + name + suffix);
    if (lod == nullptr) {
      /* El `except KeyError: break` del original. */
      break;
    }

    if (BLI_findlink(&ob->lodlevels, level) == nullptr) {
      /* El `except IndexError: bpy.ops.object.lod_add()`. `OBJECT_OT_lod_add` opera
       * sobre el objeto activo, que aqui es `ob`; se llama a la funcion directamente
       * para no depender del contexto de la llamada. */
      BKE_object_lod_add(ob);
    }

    lod_level_object_set(ob, level, lod);
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_LOD, CTX_wm_view3d(C));
  return OPERATOR_FINISHED;
}

void OBJECT_OT_lod_by_name(wmOperatorType *ot)
{
  ot->name = "Setup Levels of Detail By Name";
  ot->description = "Add levels of detail to this object based on object names";
  ot->idname = "OBJECT_OT_lod_by_name";

  ot->exec = object_lod_by_name_exec;
  ot->poll = ED_operator_object_active;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus object_lod_clear_all_exec(bContext *C, wmOperator * /*op*/)
{
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return OPERATOR_CANCELLED;
  }

  /* `while 'CANCELLED' not in bpy.ops.object.lod_remove(): pass`, o sea: quitar el
   * nivel 1 una y otra vez hasta que no quede. `BKE_object_lod_remove` se lleva
   * ademas el nivel base cuando se queda solo. */
  if (!BLI_listbase_is_empty(&ob->lodlevels)) {
    while (BKE_object_lod_remove(ob, 1)) {
      /* Vacio a proposito. */
    }
  }

  WM_event_add_notifier(C, NC_OBJECT | ND_LOD, CTX_wm_view3d(C));
  return OPERATOR_FINISHED;
}

void OBJECT_OT_lod_clear_all(wmOperatorType *ot)
{
  ot->name = "Clear All Levels of Detail";
  ot->description = "Remove all levels of detail from this object";
  ot->idname = "OBJECT_OT_lod_clear_all";

  ot->exec = object_lod_clear_all_exec;
  ot->poll = ED_operator_object_active;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static wmOperatorStatus object_lod_generate_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return OPERATOR_CANCELLED;
  }

  const int count = RNA_int_get(op->ptr, "count");
  /* El `target` se lee como double, igual que Python, porque la cuenta del ratio se
   * hacia en coma flotante de doble precision antes de guardarse en un `float`. */
  const double target = double(RNA_float_get(op->ptr, "target"));
  const bool package = RNA_boolean_get(op->ptr, "package");

  if (package) {
    /* El original llamaba a `bpy.ops.object.group_link` y `bpy.ops.group.create`, que
     * NO EXISTEN desde Blender 2.8 (los grupos son colecciones). Comprobado contra el
     * binario de referencia: `bpy.ops.object.group_link` levanta
     * «could not be found». O sea que esta rama lleva rota desde antes de Flipendo.
     * Aqui se dice con todas las letras en vez de dejar una excepcion en la consola.
     * Ver la deuda en politicas/LOD-A-CPP.md. */
    BKE_report(op->reports,
               RPT_ERROR,
               "Packing into a group is not supported: groups became collections in "
               "Blender 2.8 and this option was never ported");
    return OPERATOR_CANCELLED;
  }

  if (count < 2) {
    /* Con `count` 1 el original dividia por cero y con 0 usaba una variable sin
     * asignar; las dos cosas acaban en excepcion. La propiedad no tiene minimo, asi
     * que el caso es alcanzable. Se rechaza de forma limpia. */
    BKE_report(op->reports, RPT_ERROR, "Count must be 2 or more");
    return OPERATOR_CANCELLED;
  }

  const std::string ob_name(ob->id.name + 2);
  const std::string lowered = lower_ascii(ob_name);

  std::string lod_name = ob_name;
  std::string lod_suffix = "lod";
  std::string lod_prefix;
  if (lowered.size() >= 4 && lowered.compare(lowered.size() - 4, 4, "lod0") == 0) {
    /* Ojo: `lod_name[-3:-1]` son DOS caracteres, no tres, y `lod_name[:-3]` deja el
     * primero de los cuatro. Es asimetrico respecto de la rama del prefijo y es lo
     * que hace el original. */
    lod_suffix = ob_name.substr(ob_name.size() - 3, 2);
    lod_name = ob_name.substr(0, ob_name.size() - 3);
  }
  else if (lowered.compare(0, 4, "lod0") == 0) {
    lod_suffix = "";
    lod_prefix = ob_name.substr(0, 3);
    lod_name = ob_name.substr(4);
  }

  const double step = (1.0 - target) / double(count - 1);

  Object *lod = nullptr;
  for (int i = 1; i < count; i++) {
    /* NO se deselecciona nada: el original tampoco lo hacia, y no es un descuido.
     * `OBJECT_OT_duplicate` copia lo SELECCIONADO, y tras la primera vuelta lo
     * seleccionado es el nivel anterior -- que ya lleva el modificador de diezmado.
     * Por eso a partir de la segunda vuelta el original coge `lod.modifiers[-1]` en vez
     * de crear otro: el duplicado lo hereda. Deseleccionar aqui rompe la cadena y el
     * operador no duplica nada. Lo cazo `--fl-check-lod-ops` (3 casos de 11). */
    view_layer_active_object_set(C, ob);

    PointerRNA props;
    WM_operator_properties_create(&props, "OBJECT_OT_duplicate");
    RNA_boolean_set(&props, "linked", false);
    RNA_enum_set(&props, "mode", int(blender::ed::transform::TFM_TRANSLATION));
    WM_operator_name_call(C, "OBJECT_OT_duplicate", WM_OP_EXEC_DEFAULT, &props, nullptr);
    WM_operator_properties_free(&props);

    /* `lod = context.selected_objects[0]`. */
    lod = nullptr;
    CTX_DATA_BEGIN (C, Object *, ob_iter, selected_objects) {
      if (lod == nullptr) {
        lod = ob_iter;
      }
    }
    CTX_DATA_END;
    if (lod == nullptr) {
      BKE_report(op->reports, RPT_ERROR, "Could not duplicate the object");
      return OPERATOR_CANCELLED;
    }

    view_layer_active_object_set(C, ob);
    BKE_object_lod_add(ob);
    view_layer_active_object_set(C, lod);

    std::string new_name;
    if (!lod_prefix.empty()) {
      new_name = lod_prefix + std::to_string(i) + lod_name;
    }
    else {
      new_name = lod_name + lod_suffix + std::to_string(i);
    }
    PointerRNA lod_ptr = RNA_id_pointer_create(&lod->id);
    RNA_string_set(&lod_ptr, "name", new_name.c_str());

    lod->loc[1] = ob->loc[1] + 3.0f * float(i);

    ModifierData *md;
    if (i == 1) {
      md = BKE_modifier_new(eModifierType_Decimate);
      STRNCPY_UTF8(md->name, "lod_decimate");
      BKE_modifier_unique_name(&lod->modifiers, md);
      BLI_addtail(&lod->modifiers, md);
      BKE_modifiers_persistent_uid_init(*lod, *md);
    }
    else {
      md = static_cast<ModifierData *>(lod->modifiers.last);
    }
    if (md != nullptr && md->type == eModifierType_Decimate) {
      DecimateModifierData *dmd = reinterpret_cast<DecimateModifierData *>(md);
      dmd->percent = float(1.0 - step * double(i));
    }

    lod_level_object_set(ob, i, lod);
  }

  /* `lod.select_set(False)` / `ob.select_set(True)`. Con `count` >= 2 el bucle corre al
   * menos una vez, asi que `lod` nunca es nulo aqui. */
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  if (lod != nullptr) {
    base_select(BKE_view_layer_base_find(view_layer, lod), BA_DESELECT);
  }
  base_select(BKE_view_layer_base_find(view_layer, ob), BA_SELECT);
  view_layer_active_object_set(C, ob);

  DEG_id_tag_update(&scene->id, ID_RECALC_SELECT);
  DEG_relations_tag_update(CTX_data_main(C));
  WM_event_add_notifier(C, NC_SCENE | ND_OB_SELECT, scene);
  WM_event_add_notifier(C, NC_OBJECT | ND_LOD, CTX_wm_view3d(C));
  return OPERATOR_FINISHED;
}

void OBJECT_OT_lod_generate(wmOperatorType *ot)
{
  ot->name = "Generate Levels of Detail";
  ot->description = "Generate levels of detail using the decimate modifier";
  ot->idname = "OBJECT_OT_lod_generate";

  ot->exec = object_lod_generate_exec;
  ot->poll = ED_operator_object_active;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Las tres propiedades del Python, con sus mismos nombres, tipos, defectos y rangos.
   * `count` y `package` no tenian rango declarado, asi que salen con el que pone
   * `bpy.props` por defecto: entero completo y ningun limite.
   *
   * Aqui SI se dejan animables, al reves que en los operadores de presets: alli las
   * propiedades llevaban `options={'SKIP_SAVE'}`, y ese conjunto SUSTITUYE al defecto de
   * `bpy.props`, que es `{'ANIMATABLE'}`. Estas no llevan `options`, asi que conservan
   * el defecto. Lo confirma `--fl-check-lod-optypes`. */
  RNA_def_int(ot->srna, "count", 3, INT_MIN, INT_MAX, "Count", "", INT_MIN, INT_MAX);
  RNA_def_float(ot->srna, "target", 0.1f, 0.0f, 1.0f, "Target Size", "", 0.0f, 1.0f);
  RNA_def_boolean(ot->srna, "package", false, "Package into Group", "");
}

/** \} */

}  // namespace blender::ed::object
