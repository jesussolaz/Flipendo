/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 *
 * Crear un personaje basico jugable, en C++ nativo.
 *
 * Migracion de `scripts/addons_core/game_engine_add_basic_character.py`
 * (338 lineas, UPBGE; autor "Moaaa").
 *
 * ESTADO MEDIDO DEL ORIGINAL: MUERTO. Al activarlo en 4.5:
 *
 *   RuntimeError: Error: Registering panel class: 'addcharacter' has category
 *   'Add Character'
 *
 * `register()` registraba el panel el PRIMERO, asi que al fallar ahi tampoco se
 * registraban `character.gen` ni `fly_camera.gen`. Y aunque hubieran
 * registrado, sus `execute()` estan escritos contra el API de la 2.7x:
 * `scene.objects.active`, `scene.cursor_location`, `scene.objects.link`,
 * `obj.select`, `layers=(...)`, `view_align=` y
 * `render.engine = 'BLENDER_GAME'`. Ninguna de esas siete existe desde la 2.80.
 *
 * QUE SE REPONE, Y EN QUE SISTEMA
 *
 * El original armaba el personaje con LADRILLOS de logica (sensores MOUSE y
 * KEYBOARD, controladores AND, actuadores MOTION y MOUSE). Flipendo no tiene
 * ese camino como sistema de juego: el sistema de componentes del motor es
 * `FL_Component`, nativo, atado por la propiedad de juego `fl_component`
 * (`politicas/PLAYER-SIN-CPYTHON.md` §4). Asi que el personaje que sale de aqui
 * es el mismo que usa la plantilla `ArpgNative.blend`, medido: una malla con
 * `fl_component = "PlayerController"` y una camara con
 * `fl_component = "ThirdPersonCamera"`, sin un solo ladrillo.
 *
 * Doctrina: `politicas/LENGUAJE-CPP.md`, `politicas/ADDONS-MOTOR-A-CPP.md`.
 */

#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_string.h"

#include "BLT_translation.hh"

#include "DNA_camera_types.h"
#include "DNA_object_types.h"
#include "DNA_property_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BKE_context.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_property.hh"
#include "BKE_report.hh"
#include "BKE_screen.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "ED_screen.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "FL_game_runtime.hh"
#include "FL_ui_registry.hh"

namespace flipendo::game {

/* -------------------------------------------------------------------- */
/** \name Utilidades
 * \{ */

/** Pone (o reemplaza) una propiedad de juego de texto en el objeto. */
static void object_game_prop_set(Object *ob, const char *name, const char *value)
{
  bProperty *prop = BKE_bproperty_new(GPROP_STRING);
  STRNCPY(prop->name, name);
  BKE_bproperty_set(prop, value);
  BKE_bproperty_object_set(ob, prop);
  BKE_bproperty_free(prop);
}

/** Llama a un operador por su idname con las propiedades que se le monten. */
static bool call_op(bContext *C, const char *idname, PointerRNA *props)
{
  wmOperatorType *ot = WM_operatortype_find(idname, false);
  if (ot == nullptr) {
    return false;
  }
  const wmOperatorStatus status = WM_operator_name_call_ptr(
      C, ot, WM_OP_EXEC_DEFAULT, props, nullptr);
  return (status & OPERATOR_FINISHED) != 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name `character.gen`
 * \{ */

static wmOperatorStatus character_gen_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  if (scene == nullptr) {
    return OPERATOR_CANCELLED;
  }

  char name[MAX_ID_NAME - 2];
  RNA_string_get(op->ptr, "character_name", name);
  const float size = RNA_float_get(op->ptr, "character_size");

  const float *cursor = scene->cursor.location;

  /* 1. El cuerpo: el mismo cono de 16 caras del addon. */
  wmOperatorType *cone_ot = WM_operatortype_find("MESH_OT_primitive_cone_add", false);
  if (cone_ot == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "mesh.primitive_cone_add no esta registrado");
    return OPERATOR_CANCELLED;
  }
  PointerRNA cone_props;
  WM_operator_properties_create_ptr(&cone_props, cone_ot);
  RNA_int_set(&cone_props, "vertices", 16);
  RNA_float_set(&cone_props, "radius1", size);
  RNA_float_set(&cone_props, "radius2", 0.0f);
  RNA_float_set(&cone_props, "depth", size);
  RNA_enum_set(&cone_props, "end_fill_type", 1); /* TRIFAN */
  float loc[3];
  copy_v3_v3(loc, cursor);
  /* El addon subia el cuerpo medio tamano al final; se hace ya al colocarlo. */
  loc[2] += size * 0.5f;
  RNA_float_set_array(&cone_props, "location", loc);
  const bool cone_ok = call_op(C, "MESH_OT_primitive_cone_add", &cone_props);
  WM_operator_properties_free(&cone_props);
  if (!cone_ok) {
    BKE_report(op->reports, RPT_ERROR, "No se pudo crear el cuerpo del personaje");
    return OPERATOR_CANCELLED;
  }

  Object *body = CTX_data_active_object(C);
  if (body == nullptr) {
    return OPERATOR_CANCELLED;
  }
  BKE_libblock_rename(*bmain, body->id, name);

  /* Fisica de personaje, la misma que ponia el addon (`physics_type='CHARACTER'`
   * mas `use_actor`, `use_collision_bounds` y `collision_bounds_type='CONE'`).
   * TRAMPA: `ob->body_type` NO es la fuente de verdad. El getter de RNA
   * (`rna_object.cc:1552-1594`) lo RECALCULA cada vez a partir de `gameflag`,
   * asi que ponerlo a mano no sirve de nada: hay que encender `OB_CHARACTER`.
   * Y el setter de `physics_type` APAGA `OB_ACTOR` al pasar a personaje
   * (`:1631`), por eso el addon ponia `use_actor = True` DESPUES; aqui se
   * enciende directamente el estado final. */
  body->gameflag |= OB_COLLISION | OB_CHARACTER | OB_ACTOR | OB_BOUNDS;
  body->gameflag &= ~(OB_SENSOR | OB_OCCLUDER | OB_DYNAMIC | OB_RIGID_BODY | OB_SOFT_BODY |
                      OB_NAVMESH);
  body->body_type = OB_BODY_TYPE_CHARACTER;
  body->boundtype = OB_BOUND_CONE;
  body->collision_boundtype = OB_BOUND_CONE;
  body->visibility_flag |= OB_HIDE_RENDER;
  /* Y el componente nativo, que es lo que lo mueve. */
  object_game_prop_set(body, "fl_component", "PlayerController");

  /* 2. La camara. */
  wmOperatorType *cam_ot = WM_operatortype_find("OBJECT_OT_camera_add", false);
  if (cam_ot == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "object.camera_add no esta registrado");
    return OPERATOR_CANCELLED;
  }
  PointerRNA cam_props;
  WM_operator_properties_create_ptr(&cam_props, cam_ot);
  float cam_loc[3] = {cursor[0], cursor[1] - 9.0f, cursor[2] + 5.0f};
  RNA_float_set_array(&cam_props, "location", cam_loc);
  const bool cam_ok = call_op(C, "OBJECT_OT_camera_add", &cam_props);
  WM_operator_properties_free(&cam_props);
  if (!cam_ok) {
    BKE_report(op->reports, RPT_ERROR, "No se pudo crear la camara");
    return OPERATOR_CANCELLED;
  }

  Object *cam = CTX_data_active_object(C);
  if (cam == nullptr || cam == body) {
    return OPERATOR_CANCELLED;
  }
  char cam_name[MAX_ID_NAME - 2];
  SNPRINTF(cam_name, "cam%s", name);
  BKE_libblock_rename(*bmain, cam->id, cam_name);
  object_game_prop_set(cam, "fl_component", "ThirdPersonCamera");
  scene->camera = cam;

  DEG_id_tag_update(&scene->id, ID_RECALC_SYNC_TO_EVAL);
  DEG_relations_tag_update(bmain);
  WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
  WM_event_add_notifier(C, NC_OBJECT | ND_DRAW, nullptr);

  /* Aviso util, no error: los componentes nativos buscan al jugador POR NOMBRE
   * ("Player", `FL_ArpgComponents.cpp:234` y `:297`). */
  if (!STREQ(name, "Player")) {
    BKE_reportf(op->reports,
                RPT_INFO,
                "Personaje \"%s\" creado. La camara y los enemigos nativos siguen al objeto "
                "llamado \"Player\": renombralo si quieres que lo sigan",
                name);
  }
  else {
    BKE_report(op->reports, RPT_INFO, "Personaje \"Player\" y camara creados");
  }
  return OPERATOR_FINISHED;
}

static void CHARACTER_OT_gen(wmOperatorType *ot)
{
  /* Mismos `bl_label` y `bl_description` que la clase `simple_character`. */
  ot->name = "Add character";
  ot->idname = "CHARACTER_OT_gen";
  ot->description = "Generate a simple character";

  ot->exec = character_gen_exec;
  ot->poll = ED_operator_objectmode_poll_msg;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* Mismas propiedades que el addon colgaba de `bpy.types.Scene`, con sus
   * mismos nombres, tipos y rangos. Aqui son propiedades del operador: asi se
   * ajustan en el panel de "ultima operacion" y no ensucian la escena. */
  RNA_def_string(ot->srna,
                 "character_name",
                 "Player",
                 MAX_ID_NAME - 2,
                 "character_name",
                 "Name of the character object");
  RNA_def_float(ot->srna, "character_size", 2.0f, 1.0f, 10.0f, "character_size", "", 1.0f, 10.0f);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Panel de la vista 3D
 * \{ */

static void character_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;
  layout->op("CHARACTER_OT_gen", IFACE_("Add Character"), ICON_OUTLINER_OB_ARMATURE);
}

static void character_panel_register()
{
  SpaceType *st = BKE_spacetype_from_id(SPACE_VIEW3D);
  if (st == nullptr) {
    return;
  }
  ARegionType *art = BKE_regiontype_from_id(st, RGN_TYPE_UI);
  if (art == nullptr) {
    return;
  }

  /* El panel del addon vivia en `bl_region_type = 'TOOLS'` con
   * `bl_category = 'Add Character'`, y esa combinacion es JUSTO la que hace
   * fallar su registro en 4.5 (la region de herramientas ya no tiene
   * pestanas). Va a la barra lateral (region UI), que si las tiene, con la
   * misma pestana. Divergencia obligada y escrita. */
  PanelDecl panel{};
  panel.idname = "VIEW3D_PT_fl_add_character";
  panel.label = N_("Simple Character");
  panel.category = "Add Character";
  panel.draw = character_panel_draw;
  panels_register(art, SPACE_VIEW3D, blender::Span<const PanelDecl>(&panel, 1));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registro
 * \{ */

void character_operatortypes_register()
{
  WM_operatortype_append(CHARACTER_OT_gen);
  character_panel_register();
}

/** \} */

}  // namespace flipendo::game
