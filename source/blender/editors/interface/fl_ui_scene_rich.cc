/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * La ESCENA RICA del arnes de interfaz, construida en C++.
 *
 * EL PROBLEMA QUE QUITA
 * ---------------------
 * Las dos lineas base de `tests/flipendo/ui/` se congelan sobre la escena de
 * fabrica —un cubo, una camara y una luz—, y por eso **909 de los 2.004 bloques
 * del volcado de diseno salen `NO-CUBIERTO`**, la mayoria por `poll`. Eso no es un
 * defecto del volcador: es que la escena no tiene el dato. Tres carriles distintos
 * han chocado esta noche con lo mismo — la cabecera del editor de imagen, las
 * columnas de arista y cara del contextual de malla, las ramas por tipo de objeto
 * del contextual de objeto.
 *
 * Con una escena rica cargada ANTES del volcado, esos `draw()` se ejecutan de
 * verdad y se pueden verificar. Las opciones `--fl-*` corren en `ARG_PASS_FINAL`,
 * o sea **despues** de cargar el `.blend` de la linea de ordenes, asi que basta con:
 *
 *     Blender --factory-startup tests/flipendo/ui/escena-rica.blend \
 *             --fl-dump-ui-layout <salida>
 *
 * POR QUE SE CONSTRUYE CON EL BINARIO Y NO CON UN SCRIPT
 * -------------------------------------------------------
 * Un `.blend` en el repo es un asset, pero un asset que nadie sabe regenerar es un
 * dato opaco: si manana hace falta anadirle un objeto, o si cambia el formato, no
 * hay de donde volver a sacarlo. Con `--fl-make-ui-scene` la escena es
 * **reproducible desde el codigo**, y ademas no depende del interprete, que es a
 * donde va este proyecto.
 *
 * COMO: por operadores, igual que `fl_properties_ui_scene.cc`
 * -----------------------------------------------------------
 * Nada se fabrica a mano. Se llama a los mismos operadores que llamaria el usuario,
 * porque un dato construido a mano puede salir distinto del que sale por el camino
 * normal (el ejemplo escrito en aquel fichero: `ED_mball_add_primitive()` escala por
 * el diametro de la vista). Y si un operador falla, **se grita y se aborta**: un
 * andamio que falla en silencio deja el panel en `NO-CUBIERTO` y uno se cree que lo
 * ha verificado.
 *
 * DETERMINISMO
 * ------------
 * Es el requisito duro: si la escena no sale igual en cada ejecucion, la linea base
 * no se reproduce a si misma y el arnes es inestable (leccion de las 03:50). Por eso
 * todo va con posicion, rotacion y tipo EXPLICITOS, sin nada que dependa de la vista
 * ni del reloj, y el sistema de particulas se deja con su semilla por defecto (0).
 * No se compara el `.blend` byte a byte —un `.blend` lleva direcciones dentro y no
 * es identico entre guardados—: se comprueba lo que importa, que **el volcado de
 * diseno que sale de la escena sea identico** generandola tres veces.
 *
 * QUE **NO** LLEVA, Y POR QUE
 * ---------------------------
 * El clip de pelicula y la mascara. Un `MovieClip` necesita un fichero de video o
 * una secuencia de imagenes en disco, y `MASK_OT_new` solo se puede ejecutar desde
 * el editor de clips o el de imagen con un clip cargado. Meter un video de prueba en
 * el repo es una decision de assets que no es de este carril, asi que los cinco
 * menus del editor de clips siguen sin cubrir y queda dicho aqui y en el informe.
 *
 * Doctrina: politicas/LENGUAJE-CPP.md · Metodo: politicas/UI-A-CPP.md.
 */

#include <cstdio>

#include "BLI_utildefines.h"

#include "DNA_ID.h"
#include "DNA_layer_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "FL_ui_dump.hpp"

namespace flipendo::ui_dump {

/** Llama a un operador y grita si no existe o no termina. */
static bool rich_op(bContext *C, const char *idname, PointerRNA *props)
{
  if (WM_operatortype_find(idname, true) == nullptr) {
    fprintf(stderr, "fl-make-ui-scene: no existe el operador '%s'.\n", idname);
    return false;
  }
  const wmOperatorStatus status = WM_operator_name_call(
      C, idname, WM_OP_EXEC_DEFAULT, props, nullptr);
  if (!(status & OPERATOR_FINISHED)) {
    fprintf(stderr,
            "fl-make-ui-scene: el operador '%s' no termino (estado %d).\n",
            idname,
            int(status));
    return false;
  }
  return true;
}

static bool rich_op(bContext *C, const char *idname)
{
  return rich_op(C, idname, nullptr);
}

/** Un operador `OBJECT_OT_*_add` con una posicion explicita. */
static bool add_at(bContext *C, const char *idname, const float x, const float y)
{
  PointerRNA props;
  WM_operator_properties_create(&props, idname);
  const float loc[3] = {x, y, 0.0f};
  RNA_float_set_array(&props, "location", loc);
  const bool ok = rich_op(C, idname, &props);
  WM_operator_properties_free(&props);
  return ok;
}

/** Un `OBJECT_OT_*_add` con `type=` y posicion explicita. */
static bool add_typed_at(
    bContext *C, const char *idname, const char *type, const float x, const float y)
{
  PointerRNA props;
  WM_operator_properties_create(&props, idname);
  RNA_enum_set_identifier(C, &props, "type", type);
  const float loc[3] = {x, y, 0.0f};
  RNA_float_set_array(&props, "location", loc);
  const bool ok = rich_op(C, idname, &props);
  WM_operator_properties_free(&props);
  return ok;
}

/**
 * Deja activo y seleccionado en solitario el objeto que se llame `name`.
 *
 * No va por operador a proposito: `OBJECT_OT_select_all` con `DESELECT` devuelve
 * CANCELLED cuando no hay nada seleccionado, y `select_pattern` no fija el activo.
 * Con la capa de vista es exacto y no depende del estado previo.
 */
static bool make_active(bContext *C, const char *name)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  if (bmain == nullptr || scene == nullptr || view_layer == nullptr) {
    return false;
  }
  Object *ob = reinterpret_cast<Object *>(BKE_libblock_find_name(bmain, ID_OB, name));
  if (ob == nullptr) {
    fprintf(stderr, "fl-make-ui-scene: no existe el objeto '%s'.\n", name);
    return false;
  }
  BKE_view_layer_synced_ensure(scene, view_layer);
  Base *base = BKE_view_layer_base_find(view_layer, ob);
  if (base == nullptr) {
    fprintf(stderr, "fl-make-ui-scene: '%s' no esta en la capa de vista.\n", name);
    return false;
  }
  BKE_view_layer_base_deselect_all(scene, view_layer);
  BKE_view_layer_base_select_and_set_active(view_layer, base);
  return true;
}

bool make_scene(bContext *C, const char *filepath)
{
  Scene *scene = CTX_data_scene(C);
  if (scene == nullptr) {
    fprintf(stderr, "fl-make-ui-scene: no hay escena.\n");
    return false;
  }

  /* Un objeto por rama del contextual de objeto y de las pestanas de datos. Las
   * posiciones son explicitas para que no dependan del cursor 3D ni de la vista. */
  /* `OBJECT_OT_camera_add` no tiene `type`. */
  if (!add_at(C, "OBJECT_OT_camera_add", 4.0f, 0.0f)) {
    return false;
  }
  if (!add_typed_at(C, "OBJECT_OT_light_add", "POINT", 6.0f, 0.0f)) {
    return false;
  }
  /* No hay `OBJECT_OT_curve_add`: la curva se anade con su primitiva, igual que
   * desde el menu Anadir > Curva > Bezier. */
  if (!add_at(C, "CURVE_OT_primitive_bezier_curve_add", 8.0f, 0.0f)) {
    return false;
  }
  if (!add_at(C, "OBJECT_OT_text_add", 10.0f, 0.0f)) {
    return false;
  }
  if (!add_typed_at(C, "OBJECT_OT_empty_add", "PLAIN_AXES", 12.0f, 0.0f)) {
    return false;
  }
  if (!add_at(C, "OBJECT_OT_armature_add", 14.0f, 0.0f)) {
    return false;
  }

  /* El esqueleto se queda en modo POSE. El modo es del objeto, no de la escena, asi
   * que sobrevive a que despues se active otro; `context.mode` lo marca el activo. */
  if (!rich_op(C, "OBJECT_OT_posemode_toggle")) {
    return false;
  }

  /* El cubo de fabrica: sistema de particulas (la pestana de Particulas entera) y
   * modo edicion con las TRES selecciones, que es lo que abre las columnas de arista
   * y de cara del contextual de malla. */
  if (!make_active(C, "Cube")) {
    return false;
  }
  if (!rich_op(C, "OBJECT_OT_particle_system_add")) {
    return false;
  }

  /* Las tres a la vez: `mesh_select_mode` es un conjunto de banderas, y es asi como
   * el usuario las enciende con Mayus. */
  scene->toolsettings->selectmode = SCE_SELECT_VERTEX | SCE_SELECT_EDGE | SCE_SELECT_FACE;

  if (!rich_op(C, "OBJECT_OT_editmode_toggle")) {
    return false;
  }
  PointerRNA sel_all;
  WM_operator_properties_create(&sel_all, "MESH_OT_select_all");
  RNA_enum_set_identifier(C, &sel_all, "action", "SELECT");
  const bool ok_sel = rich_op(C, "MESH_OT_select_all", &sel_all);
  WM_operator_properties_free(&sel_all);
  if (!ok_sel) {
    return false;
  }

  /* Guardar por el operador de siempre, sin compresion y sin reescribir rutas, para
   * que el fichero no dependa de donde este el arbol. */
  PointerRNA save;
  WM_operator_properties_create(&save, "WM_OT_save_as_mainfile");
  RNA_string_set(&save, "filepath", filepath);
  RNA_boolean_set(&save, "compress", false);
  RNA_boolean_set(&save, "relative_remap", false);
  RNA_boolean_set(&save, "copy", false);
  const bool ok_save = rich_op(C, "WM_OT_save_as_mainfile", &save);
  WM_operator_properties_free(&save);
  if (!ok_save) {
    return false;
  }

  printf("FL_UI_SCENE_OK %s\n", filepath);
  return true;
}

}  // namespace flipendo::ui_dump
