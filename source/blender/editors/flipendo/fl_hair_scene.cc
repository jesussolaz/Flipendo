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
#include "BLI_string.h"
#include "BLI_math_vector_types.hh"
#include "BLI_utildefines.h"

#include "DNA_ID.h"
#include "DNA_light_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_world_types.h"
#include "DNA_curves_types.h"
#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
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

#include "BKE_customdata.hh"
#include "BKE_material.hh"
#include "BKE_mesh.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_tree_update.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "ED_curves.hh"

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

bool make_scene(bContext *C, const char *filepath, const bool pegada)
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

  /* VARIANTE «PEGADA»: el pelo cosido a la superficie de la cabeza, que es como se
   * hace el pelo sobre un personaje de verdad.
   *
   * Es exactamente lo que monta `OBJECT_OT_curves_empty_hair_add`: objeto de
   * superficie, mapa UV de enganche, el nodo *Deform Curves on Surface*, y la marca
   * que obliga a la malla a llevar el atributo `rest_position` — sin el, el nodo no
   * tiene contra que deformar. Se llama a la MISMA funcion del arbol
   * (`ed::curves::ensure_surface_deformation_node_exists`) en vez de armar el arbol
   * de nodos a mano, por lo de siempre: un dato construido por otro camino sale
   * distinto del que sale por el normal.
   *
   * Se monta aparte porque el pelo suelto y el pelo pegado NO son el mismo caso para
   * el motor: el pegado depende de la malla de la cabeza, y de esa malla si toma el
   * control el motor (`BL_ConvertMesh`). Es el sitio donde hay que mirar si alguien
   * ve pelo desaparecer de verdad. */
  if (pegada) {
    Object *hair = reinterpret_cast<Object *>(BKE_libblock_find_name(bmain, ID_OB, "Melena"));
    Object *head = reinterpret_cast<Object *>(BKE_libblock_find_name(bmain, ID_OB, "Cabeza"));
    if (hair == nullptr || head == nullptr || head->data == nullptr) {
      fprintf(stderr, "fl-make-hair-scene: falta la melena o la cabeza para pegarlas.\n");
      return false;
    }
    Curves *curves_id = static_cast<Curves *>(hair->data);
    curves_id->surface = head;
    Mesh *surface_mesh = static_cast<Mesh *>(head->data);
    const char *uv_name = CustomData_get_active_layer_name(&surface_mesh->corner_data,
                                                           CD_PROP_FLOAT2);
    if (uv_name == nullptr) {
      fprintf(stderr, "fl-make-hair-scene: la cabeza no tiene mapa UV; el pelo no se puede pegar.\n");
      return false;
    }
    curves_id->surface_uv_map = BLI_strdup(uv_name);
    blender::ed::curves::ensure_surface_deformation_node_exists(*C, *hair);
    head->modifier_flag |= OB_MODIFIER_FLAG_ADD_REST_POSITION;
    if (BLI_listbase_is_empty(&hair->modifiers)) {
      fprintf(stderr, "fl-make-hair-scene: no quedo ningun modificador de deformacion en la melena.\n");
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

  printf("FL_HAIR_SCENE_OK %s (%d vistas en modo camara, pelo %s)\n",
         filepath,
         views,
         pegada ? "PEGADO a la superficie" : "SUELTO");
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


/* -------------------------------------------------------------------- */
/** \name La escena del SOMBREADO (fase 2)
 *
 * La fase 1 preguntaba «¿llega el pelo al juego?» y se contestaba con una lista de
 * objetos. La fase 2 pregunta «¿parece pelo?», y eso solo se contesta con PIXELES.
 * Por eso esta escena es distinta de la otra en tres cosas, y las tres importan:
 *
 * 1. **No hay fisica y no hay animacion.** La cabeza no cae. Un render del
 *    fotograma 1 de una escena quieta es lo unico que se puede comparar consigo
 *    mismo sin arrastrar el estado del simulador.
 * 2. **El mundo es NEGRO y hay UNA sola luz.** Con el mundo gris de fabrica, la
 *    iluminacion ambiente aplana todo y una diferencia de sombreado se pierde
 *    dentro de ella. Aqui todo lo que se ve viene de la luz que se controla.
 * 3. **El material se elige.** `PELO` monta el `Principled Hair BSDF`, que es el
 *    camino nuevo. `PLANO` monta un `Diffuse BSDF` con EXACTAMENTE el color que el
 *    codigo viejo metia en su `ClosureDiffuse` (el valor por defecto del zocalo
 *    `Color` del nodo de pelo). No es «un material parecido»: es la reproduccion
 *    del `#else` que habia en `gpu_shader_material_hair.glsl`, y por eso sirve de
 *    ANTES en la comparacion A/B sin tener que recompilar el binario viejo.
 *
 * El angulo de la luz es la rotacion en X de un SOL: 90 grados = de frente (desde
 * detras de la camara), 0 = cenital, 270 = a CONTRALUZ (detras de la cabeza). Con
 * ese unico mando se recorre lo que hay que demostrar: el brillo que corre por el
 * mechon y la luz que lo atraviesa.
 * \{ */

/** El color por defecto del zocalo `Color` del nodo de pelo, que es lo que el
 * camino viejo pintaba como difuso plano. Copiado de `node_declare()` de
 * `node_shader_bsdf_hair_principled.cc`. */
static const float hair_flat_color[3] = {0.017513f, 0.005763f, 0.002059f};

/** Crea un material con su arbol de nodos y el nodo `type` enchufado a la salida. */
static Material *hair_material_make(Main *bmain, const char *name, const int node_type)
{
  Material *ma = BKE_material_add(bmain, name);
  if (ma == nullptr) {
    return nullptr;
  }
  ma->use_nodes = true;
  if (ma->nodetree == nullptr) {
    ma->nodetree = blender::bke::node_tree_add_tree_embedded(
        nullptr, &ma->id, "Shader Nodetree", "ShaderNodeTree");
  }
  bNodeTree *ntree = ma->nodetree;
  bNode *shader = blender::bke::node_add_static_node(nullptr, *ntree, node_type);
  bNode *output = blender::bke::node_add_static_node(nullptr, *ntree, SH_NODE_OUTPUT_MATERIAL);
  if (shader == nullptr || output == nullptr) {
    fprintf(stderr, "fl-make-hair-shading-scene: no se pudo crear el nodo %d.\n", node_type);
    return nullptr;
  }
  bNodeSocket *from = blender::bke::node_find_socket(*shader, SOCK_OUT, "BSDF");
  bNodeSocket *to = blender::bke::node_find_socket(*output, SOCK_IN, "Surface");
  if (from == nullptr || to == nullptr) {
    fprintf(stderr, "fl-make-hair-shading-scene: el nodo %d no tiene BSDF/Surface.\n", node_type);
    return nullptr;
  }
  blender::bke::node_add_link(*ntree, *shader, *from, *output, *to);
  shader->location[0] = -300.0f;
  output->location[0] = 100.0f;
  blender::bke::node_set_active(*ntree, *output);
  BKE_ntree_update_after_single_tree_change(*bmain, *ntree);
  return ma;
}

static bool socket_set_value(bNode *node, const char *name, const float value)
{
  bNodeSocket *sock = blender::bke::node_find_socket(*node, SOCK_IN, name);
  if (sock == nullptr || sock->default_value == nullptr) {
    fprintf(stderr, "fl-make-hair-shading-scene: no existe el zocalo '%s'.\n", name);
    return false;
  }
  static_cast<bNodeSocketValueFloat *>(sock->default_value)->value = value;
  return true;
}

static bool socket_set_color(bNode *node, const char *name, const float rgb[3])
{
  bNodeSocket *sock = blender::bke::node_find_socket(*node, SOCK_IN, name);
  if (sock == nullptr || sock->default_value == nullptr) {
    fprintf(stderr, "fl-make-hair-shading-scene: no existe el zocalo '%s'.\n", name);
    return false;
  }
  float *v = static_cast<bNodeSocketValueRGBA *>(sock->default_value)->value;
  v[0] = rgb[0];
  v[1] = rgb[1];
  v[2] = rgb[2];
  v[3] = 1.0f;
  return true;
}

/** El primer nodo del arbol que no sea la salida. */
static bNode *material_shader_node(Material *ma)
{
  if (ma == nullptr || ma->nodetree == nullptr) {
    return nullptr;
  }
  LISTBASE_FOREACH (bNode *, node, &ma->nodetree->nodes) {
    if (node->type_legacy != SH_NODE_OUTPUT_MATERIAL) {
      return node;
    }
  }
  return nullptr;
}

bool make_shading_scene(bContext *C,
                        const char *filepath,
                        const bool flat,
                        const float light_deg,
                        const float roughness,
                        const bool forward)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  if (bmain == nullptr || scene == nullptr) {
    fprintf(stderr, "fl-make-hair-shading-scene: no hay escena.\n");
    return false;
  }

  /* Fuera el cubo de fabrica. */
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

  /* La cabeza, en el origen y SIN fisica: aqui no se mide movimiento. */
  {
    PointerRNA props;
    WM_operator_properties_create(&props, "MESH_OT_primitive_uv_sphere_add");
    RNA_float_set(&props, "radius", 0.95f);
    const float loc[3] = {0.0f, 0.0f, 0.0f};
    RNA_float_set_array(&props, "location", loc);
    const bool ok = hair_op(C, "MESH_OT_primitive_uv_sphere_add", &props);
    WM_operator_properties_free(&props);
    if (!ok) {
      return false;
    }
  }
  Object *head = reinterpret_cast<Object *>(BKE_libblock_find_name(bmain, ID_OB, "Sphere"));
  if (head == nullptr) {
    fprintf(stderr, "fl-make-hair-shading-scene: no salio la cabeza.\n");
    return false;
  }
  BKE_libblock_rename(*bmain, head->id, "Cabeza");

  /* La melena: los mismos 500 mechones deterministas de la fase 1. */
  {
    PointerRNA props;
    WM_operator_properties_create(&props, "OBJECT_OT_curves_random_add");
    const float loc[3] = {0.0f, 0.0f, 0.0f};
    RNA_float_set_array(&props, "location", loc);
    const bool ok = hair_op(C, "OBJECT_OT_curves_random_add", &props);
    WM_operator_properties_free(&props);
    if (!ok) {
      return false;
    }
  }
  Object *hair = reinterpret_cast<Object *>(BKE_libblock_find_name(bmain, ID_OB, "Curves"));
  if (hair == nullptr || hair->type != OB_CURVES) {
    fprintf(stderr, "fl-make-hair-shading-scene: no salio la melena.\n");
    return false;
  }
  BKE_libblock_rename(*bmain, hair->id, "Melena");

  /* EL MATERIAL. Es lo unico que cambia entre el ANTES y el DESPUES. */
  Material *ma = hair_material_make(
      bmain, flat ? "PeloPlano" : "PeloSombreado", flat ? SH_NODE_BSDF_DIFFUSE : SH_NODE_BSDF_HAIR_PRINCIPLED);
  if (ma == nullptr) {
    return false;
  }
  bNode *shader = material_shader_node(ma);
  if (shader == nullptr) {
    fprintf(stderr, "fl-make-hair-shading-scene: el material salio sin nodo de sombreado.\n");
    return false;
  }
  if (flat) {
    /* El `#else` de antes, al pie de la letra: difuso con el color del zocalo. */
    if (!socket_set_color(shader, "Color", hair_flat_color)) {
      return false;
    }
  }
  else {
    /* Coloracion directa para que el color sea comparable con el del difuso plano,
     * y la rugosidad que pida la prueba. */
    NodeShaderHairPrincipled *storage = static_cast<NodeShaderHairPrincipled *>(shader->storage);
    if (storage == nullptr) {
      fprintf(stderr, "fl-make-hair-shading-scene: el nodo de pelo salio sin almacenamiento.\n");
      return false;
    }
    storage->parametrization = SHD_PRINCIPLED_HAIR_REFLECTANCE;
    storage->model = SHD_PRINCIPLED_HAIR_CHIANG;
    if (!socket_set_color(shader, "Color", hair_flat_color) ||
        !socket_set_value(shader, "Roughness", roughness))
    {
      return false;
    }
  }
  /* Las dos tuberias de EEVEE compilan el sombreado de pelo por caminos distintos: la
   * diferida lo guarda en el gbuffer y lo ilumina en otra pasada, la de ADELANTE lo
   * ilumina en el propio fragmento. Un modelo nuevo tiene que probarse en las dos o la
   * mitad de los usuarios veria un fallo de compilacion de shader que nadie midio. */
  ma->surface_render_method = forward ? MA_SURFACE_METHOD_FORWARD : MA_SURFACE_METHOD_DEFERRED;
  BKE_object_material_assign(bmain, hair, ma, 1, BKE_MAT_ASSIGN_OBDATA);

  /* La cabeza, difusa y oscura: esta para tapar las raices, no para lucirse. */
  {
    Material *head_ma = hair_material_make(bmain, "CabezaPlana", SH_NODE_BSDF_DIFFUSE);
    bNode *head_shader = material_shader_node(head_ma);
    const float dark[3] = {0.02f, 0.02f, 0.02f};
    if (head_shader == nullptr || !socket_set_color(head_shader, "Color", dark)) {
      return false;
    }
    BKE_object_material_assign(bmain, head, head_ma, 1, BKE_MAT_ASSIGN_OBDATA);
  }

  /* LA LUZ. Se borra la de fabrica (un punto en un sitio raro) y se pone un SOL,
   * cuya direccion es exactamente su rotacion: sin caida con la distancia no hay
   * una variable de mas entre dos capturas. */
  if (BKE_libblock_find_name(bmain, ID_OB, "Light") != nullptr) {
    if (!make_active(C, "Light")) {
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
  {
    PointerRNA props;
    WM_operator_properties_create(&props, "OBJECT_OT_light_add");
    RNA_enum_set_identifier(C, &props, "type", "SUN");
    const float loc[3] = {0.0f, 0.0f, 5.0f};
    RNA_float_set_array(&props, "location", loc);
    const bool ok = hair_op(C, "OBJECT_OT_light_add", &props);
    WM_operator_properties_free(&props);
    if (!ok) {
      return false;
    }
  }
  Object *sun = reinterpret_cast<Object *>(BKE_libblock_find_name(bmain, ID_OB, "Sun"));
  if (sun == nullptr || sun->data == nullptr) {
    fprintf(stderr, "fl-make-hair-shading-scene: no salio el sol.\n");
    return false;
  }
  BKE_libblock_rename(*bmain, sun->id, "Sol");
  sun->rot[0] = light_deg * float(M_PI) / 180.0f;
  sun->rot[1] = 0.0f;
  sun->rot[2] = 0.0f;
  {
    Light *la = static_cast<Light *>(sun->data);
    la->energy = 5.0f;
    /* Un sol de radio 0 da una sombra dura y una integral de angulo solido minima:
     * asi el lobulo se ve tal cual, sin el suavizado del area de la luz. */
    la->sun_angle = 0.009f;
  }

  /* LA CAMARA. Fija, mirando por +Y desde -Y, para que el mechon se vea de lado. */
  Object *cam = reinterpret_cast<Object *>(BKE_libblock_find_name(bmain, ID_OB, "Camera"));
  if (cam == nullptr) {
    fprintf(stderr, "fl-make-hair-shading-scene: no hay camara.\n");
    return false;
  }
  cam->loc[0] = 0.0f;
  cam->loc[1] = -5.0f;
  cam->loc[2] = 0.0f;
  cam->rot[0] = float(M_PI) * 0.5f;
  cam->rot[1] = 0.0f;
  cam->rot[2] = 0.0f;
  scene->camera = cam;

  /* EL MUNDO NEGRO. Sin esto la ambiente aplana el resultado y la medida no separa
   * el sombreado nuevo del viejo. */
  if (scene->world != nullptr) {
    scene->world->horr = scene->world->horg = scene->world->horb = 0.0f;
    if (scene->world->nodetree != nullptr) {
      LISTBASE_FOREACH (bNode *, node, &scene->world->nodetree->nodes) {
        bNodeSocket *sock = blender::bke::node_find_socket(*node, SOCK_IN, "Color");
        if (sock != nullptr && sock->default_value != nullptr) {
          float *v = static_cast<bNodeSocketValueRGBA *>(sock->default_value)->value;
          v[0] = v[1] = v[2] = 0.0f;
        }
        bNodeSocket *stren = blender::bke::node_find_socket(*node, SOCK_IN, "Strength");
        if (stren != nullptr && stren->default_value != nullptr) {
          static_cast<bNodeSocketValueFloat *>(stren->default_value)->value = 0.0f;
        }
      }
      BKE_ntree_update_after_single_tree_change(*bmain, *scene->world->nodetree);
    }
  }

  /* EL RENDER. EEVEE, tamano fijo, un solo fotograma, PNG sin compresion perceptible.
   * Las muestras se fijan a mano: son la fuente del suelo de ruido y hay que poder
   * decir cuantas eran. */
  STRNCPY(scene->r.engine, "BLENDER_EEVEE_NEXT");
  scene->r.xsch = 800;
  scene->r.ysch = 600;
  scene->r.size = 100;
  scene->r.sfra = scene->r.efra = scene->r.cfra = 1;
  scene->r.im_format.imtype = R_IMF_IMTYPE_PNG;
  scene->r.im_format.depth = R_IMF_CHAN_DEPTH_8;
  scene->eevee.taa_render_samples = 64;

  const int views = views_to_camera(bmain);
  if (views == 0) {
    fprintf(stderr, "fl-make-hair-shading-scene: ninguna vista 3D en modo camara.\n");
    return false;
  }

  /* Y las vistas en modo RENDERIZADO. Sin esto el editor y el Player dibujan en modo
   * solido (Workbench) y NO pasan por EEVEE: la escena se veria, pero no probaria ni
   * una linea del sombreado nuevo. Es la version para el visor de la trampa 2 de
   * `politicas/PELO-A-CPP.md` (el `.blend` manda sobre la camara). */
  int shaded = 0;
  LISTBASE_FOREACH (bScreen *, screen, &bmain->screens) {
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      LISTBASE_FOREACH (SpaceLink *, sl, &area->spacedata) {
        if (sl->spacetype != SPACE_VIEW3D) {
          continue;
        }
        View3D *v3d = reinterpret_cast<View3D *>(sl);
        v3d->shading.type = OB_RENDER;
        shaded++;
      }
    }
  }
  if (shaded == 0) {
    fprintf(stderr, "fl-make-hair-shading-scene: ninguna vista 3D en modo renderizado.\n");
    return false;
  }

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

  printf("FL_HAIR_SHADING_SCENE_OK %s (material=%s luz=%.1f grados rugosidad=%.2f tuberia=%s)\n",
         filepath,
         flat ? "PLANO(difuso, el camino viejo)" : "PELO(Principled Hair BSDF)",
         double(light_deg),
         double(roughness),
         forward ? "ADELANTE" : "DIFERIDA");
  printf("  (%d vistas en modo camara, %d en modo renderizado)\n", views, shaded);
  return true;
}

/** \} */

}  // namespace flipendo::hair_scene
