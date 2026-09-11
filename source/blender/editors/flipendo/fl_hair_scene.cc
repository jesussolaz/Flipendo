/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edflipendo
 *
 * La escena de pelo del arnes, construida en C++ por operadores.
 *
 * El razonamiento entero esta en `FL_hair_scene.hh`. Aqui solo el como:
 *
 * - La CABEZA es una esfera UV de radio 0,95 en el origen. 0,95 y no 1 porque el
 *   pelo de `primitive_random_sphere` **nace exactamente en el radio 1** y crece
 *   hacia fuera: con la cabeza un poco mas pequena, la raiz de cada mechon queda
 *   fuera de la malla y la melena se ve entera.
 * - El PELO es `OBJECT_OT_curves_random_add`: 500 mechones de 8 puntos, radio 0,02
 *   en la raiz afilandose a 0 en la punta. Es el unico operador del arbol que crea
 *   un `Curves` **con geometria dentro**; `OBJECT_OT_curves_empty_hair_add` crea uno
 *   vacio, y un objeto vacio no prueba que se dibuje nada.
 * - La camara y la luz son las de fabrica, sin tocar, para que la escena dependa de
 *   lo menos posible.
 * - El cubo de fabrica se borra: taparia el pelo.
 */

#include <cstdio>

#include "BLI_listbase.h"
#include "BLI_math_vector_types.hh"
#include "BLI_utildefines.h"

#include "DNA_ID.h"
#include "DNA_curves_types.h"
#include "DNA_layer_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_scene.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "FL_hair_scene.hh"

namespace flipendo::hair_scene {

/** Llama a un operador y grita si no existe o no termina. */
static bool hair_op(bContext *C, const char *idname, PointerRNA *props)
{
  if (WM_operatortype_find(idname, true) == nullptr) {
    fprintf(stderr, "fl-make-hair-scene: no existe el operador '%s'.\n", idname);
    return false;
  }
  const wmOperatorStatus status = WM_operator_name_call(
      C, idname, WM_OP_EXEC_DEFAULT, props, nullptr);
  if (!(status & OPERATOR_FINISHED)) {
    fprintf(stderr,
            "fl-make-hair-scene: el operador '%s' no termino (estado %d).\n",
            idname,
            int(status));
    return false;
  }
  return true;
}

static bool hair_op(bContext *C, const char *idname)
{
  return hair_op(C, idname, nullptr);
}

/**
 * Deja activo y seleccionado en solitario el objeto que se llame `name`.
 *
 * Por la capa de vista y no por operador, por lo mismo que en `fl_ui_scene_rich.cc`:
 * `OBJECT_OT_select_all` con `DESELECT` devuelve CANCELLED si no habia nada
 * seleccionado, y `select_pattern` no fija el activo.
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
    fprintf(stderr, "fl-make-hair-scene: no existe el objeto '%s'.\n", name);
    return false;
  }
  BKE_view_layer_synced_ensure(scene, view_layer);
  Base *base = BKE_view_layer_base_find(view_layer, ob);
  if (base == nullptr) {
    fprintf(stderr, "fl-make-hair-scene: '%s' no esta en la capa de vista.\n", name);
    return false;
  }
  BKE_view_layer_base_deselect_all(scene, view_layer);
  BKE_view_layer_base_select_and_set_active(view_layer, base);
  return true;
}

/**
 * Pone TODAS las vistas 3D del fichero en modo camara.
 *
 * Es la trampa numero 3 de `politicas/UI-JUEGO-NATIVA.md`, y la misma que costo
 * tiempo en VideoTexture: el Player dibuja a traves del `View3D` guardado en el
 * `.blend`, y si su vista no esta en `RV3D_CAMOB` no usa la camara de la escena.
 * En `--background` no hay ventana, asi que no vale el operador `view3d.view_camera`:
 * se recorren las pantallas de `bmain` y se toca el `RegionView3D` a mano.
 */
static int views_to_camera(Main *bmain)
{
  int touched = 0;
  LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
        if (sl->spacetype != SPACE_VIEW3D) {
          continue;
        }
        /* El espacio activo guarda sus regiones en el area; los de detras, en el
         * propio SpaceLink. */
        ListBase *regions = (sl == area->spacedata.first) ? &area->regionbase : &sl->regionbase;
        LISTBASE_FOREACH (ARegion *, region, regions) {
          if (region->regiontype != RGN_TYPE_WINDOW || region->regiondata == nullptr) {
            continue;
          }
          RegionView3D *rv3d = static_cast<RegionView3D *>(region->regiondata);
          rv3d->persp = RV3D_CAMOB;
          touched++;
        }
      }
    }
  }
  return touched;
}

bool make_scene(bContext *C, const char *filepath)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  if (bmain == nullptr || scene == nullptr) {
    fprintf(stderr, "fl-make-hair-scene: no hay escena.\n");
    return false;
  }

  /* Fuera el cubo de fabrica: taparia la cabeza y el pelo. */
  if (BKE_libblock_find_name(bmain, ID_OB, "Cube") != nullptr) {
    if (!make_active(C, "Cube")) {
      return false;
    }
    PointerRNA del;
    WM_operator_properties_create(&del, "OBJECT_OT_delete");
    RNA_boolean_set(&del, "use_global", false);
    RNA_boolean_set(&del, "confirm", false);
    const bool ok_del = hair_op(C, "OBJECT_OT_delete", &del);
    WM_operator_properties_free(&del);
    if (!ok_del) {
      return false;
    }
  }

  /* EL SUELO. Estatico con colision (es lo de fabrica para una malla), para que la
   * cabeza caiga y PARE: una escena que no se estabiliza no da una captura
   * comparable. */
  {
    PointerRNA props;
    WM_operator_properties_create(&props, "MESH_OT_primitive_plane_add");
    RNA_float_set(&props, "size", 20.0f);
    const float loc[3] = {0.0f, 0.0f, -2.0f};
    RNA_float_set_array(&props, "location", loc);
    const bool ok = hair_op(C, "MESH_OT_primitive_plane_add", &props);
    WM_operator_properties_free(&props);
    if (!ok) {
      return false;
    }
  }
  if (Object *floor_ob = reinterpret_cast<Object *>(BKE_libblock_find_name(bmain, ID_OB, "Plane")))
  {
    floor_ob->gameflag |= OB_COLLISION;
    BKE_libblock_rename(*bmain, floor_ob->id, "Suelo");
  }

  /* LA CABEZA. Radio explicito y posicion explicita: nada que dependa del cursor 3D
   * ni de la vista, o la escena dejaria de reproducirse. */
  {
    PointerRNA props;
    WM_operator_properties_create(&props, "MESH_OT_primitive_uv_sphere_add");
    RNA_float_set(&props, "radius", 0.95f);
    const float loc[3] = {0.0f, 0.0f, 2.0f};
    RNA_float_set_array(&props, "location", loc);
    const bool ok = hair_op(C, "MESH_OT_primitive_uv_sphere_add", &props);
    WM_operator_properties_free(&props);
    if (!ok) {
      return false;
    }
  }
  /* El nombre que le pone el operador es "Sphere"; se renombra a "Cabeza" para que
   * el volcado y los informes se lean solos.
   *
   * Y se le da FISICA de cuerpo rigido: la cabeza cae desde z=2 hasta el suelo. Ese
   * movimiento es el nucleo de la prueba — el pelo estatico ya se pintaba antes de
   * este carril (medido); lo que hay que demostrar es que el pelo va CON la cabeza
   * porque el motor lo tiene, no porque el depsgraph lo arrastre. Los indicadores
   * son los mismos que enciende la pestana de Fisica con `physics_type='RIGID_BODY'`.
   */
  if (Object *head = reinterpret_cast<Object *>(BKE_libblock_find_name(bmain, ID_OB, "Sphere"))) {
    head->gameflag |= OB_DYNAMIC | OB_RIGID_BODY | OB_COLLISION | OB_ACTOR | OB_BOUNDS;
    head->boundtype = OB_BOUND_SPHERE;
    head->collision_boundtype = OB_BOUND_SPHERE;
    BKE_libblock_rename(*bmain, head->id, "Cabeza");
  }

  /* El pelo: 500 mechones de 8 puntos, deterministas. */
  {
    PointerRNA props;
    WM_operator_properties_create(&props, "OBJECT_OT_curves_random_add");
    /* En el MISMO sitio que la cabeza: el pelo de `primitive_random_sphere` nace en
     * el radio 1 alrededor del origen del objeto. */
    const float loc[3] = {0.0f, 0.0f, 2.0f};
    RNA_float_set_array(&props, "location", loc);
    const bool ok = hair_op(C, "OBJECT_OT_curves_random_add", &props);
    WM_operator_properties_free(&props);
    if (!ok) {
      return false;
    }
  }
  if (Object *hair = reinterpret_cast<Object *>(BKE_libblock_find_name(bmain, ID_OB, "Curves"))) {
    if (hair->type != OB_CURVES) {
      fprintf(stderr,
              "fl-make-hair-scene: el objeto de pelo salio de tipo %d, no OB_CURVES (%d).\n",
              int(hair->type),
              int(OB_CURVES));
      return false;
    }
    BKE_libblock_rename(*bmain, hair->id, "Melena");
  }
  else {
    fprintf(stderr, "fl-make-hair-scene: el operador no dejo ningun objeto 'Curves'.\n");
    return false;
  }

  /* El pelo EMPARENTADO a la cabeza, por el operador de siempre (Ctrl+P > Objeto):
   * la melena seleccionada, la cabeza activa. Es la unica forma honesta de montarlo
   * — a mano habria que calcular `parentinv` y saldria distinto del camino normal. */
  {
    Scene *sc = CTX_data_scene(C);
    ViewLayer *vl = CTX_data_view_layer(C);
    Object *hair = reinterpret_cast<Object *>(BKE_libblock_find_name(bmain, ID_OB, "Melena"));
    Object *head = reinterpret_cast<Object *>(BKE_libblock_find_name(bmain, ID_OB, "Cabeza"));
    if (hair == nullptr || head == nullptr) {
      fprintf(stderr, "fl-make-hair-scene: falta la melena o la cabeza para emparentar.\n");
      return false;
    }
    BKE_view_layer_synced_ensure(sc, vl);
    Base *base_hair = BKE_view_layer_base_find(vl, hair);
    Base *base_head = BKE_view_layer_base_find(vl, head);
    if (base_hair == nullptr || base_head == nullptr) {
      fprintf(stderr, "fl-make-hair-scene: melena o cabeza fuera de la capa de vista.\n");
      return false;
    }
    BKE_view_layer_base_deselect_all(sc, vl);
    base_hair->flag |= BASE_SELECTED;
    BKE_view_layer_base_select_and_set_active(vl, base_head);

    PointerRNA props;
    WM_operator_properties_create(&props, "OBJECT_OT_parent_set");
    RNA_enum_set_identifier(C, &props, "type", "OBJECT");
    RNA_boolean_set(&props, "keep_transform", false);
    const bool ok = hair_op(C, "OBJECT_OT_parent_set", &props);
    WM_operator_properties_free(&props);
    if (!ok) {
      return false;
    }
    if (hair->parent != head) {
      fprintf(stderr, "fl-make-hair-scene: la melena no quedo emparentada a la cabeza.\n");
      return false;
    }
  }

  const int views = views_to_camera(bmain);
  if (views == 0) {
    fprintf(stderr, "fl-make-hair-scene: ninguna vista 3D pasada a modo camara.\n");
    return false;
  }

  /* Guardar por el operador de siempre, sin compresion y sin reescribir rutas. */
  PointerRNA save;
  WM_operator_properties_create(&save, "WM_OT_save_as_mainfile");
  RNA_string_set(&save, "filepath", filepath);
  RNA_boolean_set(&save, "compress", false);
  RNA_boolean_set(&save, "relative_remap", false);
  RNA_boolean_set(&save, "copy", false);
  const bool ok_save = hair_op(C, "WM_OT_save_as_mainfile", &save);
  WM_operator_properties_free(&save);
  if (!ok_save) {
    return false;
  }

  printf("FL_HAIR_SCENE_OK %s (%d vistas en modo camara)\n", filepath, views);
  return true;
}

bool dump_hair(bContext *C, const char *filepath)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  if (bmain == nullptr || scene == nullptr) {
    fprintf(stderr, "fl-dump-hair: no hay escena.\n");
    return false;
  }

  FILE *fp = fopen(filepath, "w");
  if (fp == nullptr) {
    fprintf(stderr, "fl-dump-hair: no se pudo escribir '%s'.\n", filepath);
    return false;
  }
  fprintf(fp, "# FL_HAIR_DUMP v1\n");

  Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
  if (depsgraph != nullptr) {
    BKE_scene_graph_update_tagged(depsgraph, bmain);
  }

  int seen = 0;
  LISTBASE_FOREACH (Object *, ob, &bmain->objects) {
    if (ob->type != OB_CURVES || ob->data == nullptr) {
      continue;
    }
    seen++;
    const Curves *curves_id = static_cast<const Curves *>(ob->data);
    const blender::bke::CurvesGeometry &geom = curves_id->geometry.wrap();
    const blender::Span<blender::float3> positions = geom.positions();

    blender::float3 lo(1e30f), hi(-1e30f);
    for (const blender::float3 &p : positions) {
      lo = blender::math::min(lo, p);
      hi = blender::math::max(hi, p);
    }
    if (positions.is_empty()) {
      lo = hi = blender::float3(0.0f);
    }

    /* Evaluado: es lo que de verdad llega al dibujado. Si el depsgraph no lo saca,
     * EEVEE no tiene nada que pintar por muy bien que este el original. */
    int eval_points = -1;
    int eval_curves = -1;
    if (depsgraph != nullptr) {
      const Object *ob_eval = DEG_get_evaluated(depsgraph, ob);
      if (ob_eval != nullptr && ob_eval->data != nullptr && ob_eval->type == OB_CURVES) {
        const Curves *eval_id = static_cast<const Curves *>(ob_eval->data);
        eval_points = eval_id->geometry.point_num;
        eval_curves = eval_id->geometry.curve_num;
      }
    }

    /* Las posiciones se cuantizan a 4 decimales a proposito: la acumulacion en coma
     * flotante no es determinista en este arbol (leccion de las 03:50) y una linea
     * base que mira la septima cifra falla sola. */
    fprintf(fp,
            "CURVES nombre=%s padre=%s loc=(%.4f,%.4f,%.4f) curvas=%d puntos=%d "
            "evaluadas=%d evalpuntos=%d "
            "caja=(%.4f,%.4f,%.4f)-(%.4f,%.4f,%.4f) oculto=%d capa=%d\n",
            ob->id.name + 2,
            (ob->parent != nullptr) ? ob->parent->id.name + 2 : "-",
            double(ob->loc[0]),
            double(ob->loc[1]),
            double(ob->loc[2]),
            geom.curves_num(),
            geom.points_num(),
            eval_curves,
            eval_points,
            double(lo.x),
            double(lo.y),
            double(lo.z),
            double(hi.x),
            double(hi.y),
            double(hi.z),
            (ob->visibility_flag & OB_HIDE_VIEWPORT) ? 1 : 0,
            int(ob->lay));
  }
  fprintf(fp, "TOTAL objetos-curves=%d\n", seen);
  fclose(fp);

  if (seen == 0) {
    fprintf(stderr, "fl-dump-hair: la escena no tiene ningun objeto Curves; no hay nada que comparar.\n");
    return false;
  }
  printf("FL_HAIR_DUMP_OK %s (%d objetos)\n", filepath, seen);
  return true;
}

}  // namespace flipendo::hair_scene
