/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spbuttons
 *
 * El andamio de medicion de las pestanas de datos, en C++.
 *
 * PARA QUE: el volcado de diseno (`--fl-dump-ui-layout`) dibuja sobre la escena
 * de fabrica, que tiene un cubo, una camara y una luz. Ninguna pestana
 * `DATA_PT_*` de metaball, altavoz, rejilla, volumen, nube de puntos o curvas
 * pasa su `poll` ahi, asi que en la linea base salen todas como
 *
 *     NO-CUBIERTO motivo=poll
 *
 * y reproducir *eso* en C++ no prueba ni un boton del `draw()`. Es la misma
 * trampa que `UI-A-CPP.md` describe para `PresetPanel` («en instalacion de
 * fabrica la carpeta de presets no existe»): la verificacion que no verifica
 * nada.
 *
 * QUE HACE: monta en la escena el dato que la pestana necesita, ANTES de que
 * corra el volcado, de modo que
 *
 *     Blender --factory-startup --fl-ui-scene METABALL:CUBE \
 *             --fl-dump-ui-layout <salida>
 *
 * cubra esos paneles de verdad. Se congela la salida con el Python vivo y el
 * C++ tiene que reproducirla, igual que con las dos lineas base de fabrica.
 *
 * COMO: llamando a los mismos operadores que llamaria el usuario, no fabricando
 * los datos a mano. Es deliberado: `ED_mball_add_primitive()` escala el elemento
 * por el diametro de la vista, asi que un metaball construido con
 * `BKE_mball_element_add()` daria numeros distintos a los del original y la
 * comparacion byte a byte fallaria por el andamio, no por el codigo.
 *
 * Esto sustituye al apano con `--python-expr` que se uso para medir la pestana
 * del Metaball: el dia que no haya interprete, aquel andamio no se podria
 * repetir y este si.
 */

#include <cstdio>
#include <cstring>

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_speaker_types.h"

#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_layer.hh"

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "FL_properties_ui.hpp"

namespace flipendo::properties_ui {

/**
 * Llama a un operador por nombre y **grita** si no existe o no se ejecuta.
 *
 * Gritar es la mitad del valor de este fichero: un andamio que falla en silencio
 * deja la pestana sin cubrir y el volcado dice `NO-CUBIERTO motivo=poll`, que es
 * exactamente lo que decia antes. O sea, un falso verde.
 */
static bool call_op(bContext *C, const char *idname, PointerRNA *props)
{
  wmOperatorType *ot = WM_operatortype_find(idname, true);
  if (ot == nullptr) {
    fprintf(stderr, "fl-ui-scene: no existe el operador '%s'.\n", idname);
    return false;
  }
  const wmOperatorStatus status = WM_operator_name_call(
      C, idname, WM_OP_EXEC_DEFAULT, props, nullptr);
  if (!(status & OPERATOR_FINISHED)) {
    fprintf(stderr, "fl-ui-scene: el operador '%s' no termino (estado %d).\n", idname, int(status));
    return false;
  }
  return true;
}

static bool call_op(bContext *C, const char *idname)
{
  return call_op(C, idname, nullptr);
}

/** `bpy.ops.object.metaball_add(type=...)` y entrar en modo edicion. */
static bool setup_metaball(bContext *C, const char *type)
{
  PointerRNA props;
  WM_operator_properties_create(&props, "OBJECT_OT_metaball_add");
  RNA_enum_set_identifier(C, &props, "type", type);
  const bool ok = call_op(C, "OBJECT_OT_metaball_add", &props);
  WM_operator_properties_free(&props);
  if (!ok) {
    return false;
  }
  /* Imprescindible: `DATA_PT_metaball_element` mira `elements.active`, que es
   * `MetaBall::lastelem`, y ese puntero **solo esta puesto en modo edicion**
   * (`ED_mball_editmball_make()` lo asigna al entrar y
   * `ED_mball_editmball_free()` lo borra al salir). En modo objeto el panel
   * sigue sin cubrirse y uno se cree que lo ha verificado. */
  return call_op(C, "OBJECT_OT_editmode_toggle");
}

/**
 * El objeto activo, por la capa de vista y no por el contexto de la pantalla.
 *
 * No es un capricho: en modo grafico, cuando corre esta opcion de linea de
 * ordenes todavia no hay area activa en el contexto, asi que la ruta de
 * `CTX_data_active_object()` -que pasa por el callback de contexto de la
 * pantalla- devuelve nullptr aunque el operador haya dejado el objeto bien
 * puesto. En `--background` SI funcionaba, que es la forma perfecta de que esto
 * pase desapercibido: lo cazo el propio andamio al gritar en vez de dejar la
 * pestana sin cubrir.
 */
static Object *active_object(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  if (scene == nullptr || view_layer == nullptr) {
    return nullptr;
  }
  BKE_view_layer_synced_ensure(scene, view_layer);
  return BKE_view_layer_active_object_get(view_layer);
}

/** `bpy.ops.object.speaker_add()`, y opcionalmente silenciarlo. */
static bool setup_speaker(bContext *C, const bool muted)
{
  if (!call_op(C, "OBJECT_OT_speaker_add")) {
    return false;
  }
  if (muted) {
    Object *ob = active_object(C);
    if (ob == nullptr || ob->type != OB_SPEAKER || ob->data == nullptr) {
      fprintf(stderr, "fl-ui-scene: el altavoz no quedo activo.\n");
      return false;
    }
    /* `speaker.muted = True`. Las cuatro pestanas de Sonido, Distancia y Cono
     * cambian de `activo` con este bit, asi que hay que medir las dos ramas. */
    static_cast<Speaker *>(ob->data)->flag |= SPK_MUTED;
  }
  return true;
}

/** `bpy.ops.object.add(type='LATTICE')`. */
static bool setup_object_type(bContext *C, const char *type)
{
  PointerRNA props;
  WM_operator_properties_create(&props, "OBJECT_OT_add");
  RNA_enum_set_identifier(C, &props, "type", type);
  const bool ok = call_op(C, "OBJECT_OT_add", &props);
  WM_operator_properties_free(&props);
  return ok;
}

bool scene_setup(bContext *C, const char *spec)
{
  /* `FAMILIA` o `FAMILIA:VARIANTE`. */
  char family[64];
  const char *colon = strchr(spec, ':');
  const char *variant = (colon != nullptr) ? colon + 1 : nullptr;
  const size_t family_len = (colon != nullptr) ? size_t(colon - spec) : strlen(spec);
  if (family_len >= sizeof(family)) {
    fprintf(stderr, "fl-ui-scene: familia demasiado larga en '%s'.\n", spec);
    return false;
  }
  memcpy(family, spec, family_len);
  family[family_len] = '\0';

  bool ok = false;
  if (STREQ(family, "METABALL")) {
    /* Las cinco formas del enum RNA: el `draw()` del elemento activo tiene
     * cuatro ramas (CUBE/ELLIPSOID juntas, CAPSULE, PLANE y el caso vacio). */
    ok = setup_metaball(C, (variant != nullptr) ? variant : "BALL");
  }
  else if (STREQ(family, "SPEAKER")) {
    ok = setup_speaker(C, variant != nullptr && STREQ(variant, "MUTED"));
  }
  else if (STREQ(family, "LATTICE")) {
    ok = setup_object_type(C, "LATTICE");
  }
  else if (STREQ(family, "VOLUME")) {
    ok = call_op(C, "OBJECT_OT_volume_add");
  }
  else if (STREQ(family, "CURVES")) {
    ok = call_op(C, "OBJECT_OT_curves_empty_hair_add");
  }
  else {
    fprintf(stderr,
            "fl-ui-scene: familia desconocida '%s'. Conocidas: METABALL[:BALL|CAPSULE|PLANE|"
            "ELLIPSOID|CUBE], SPEAKER[:MUTED], LATTICE, VOLUME, CURVES.\n",
            family);
    return false;
  }

  if (!ok) {
    return false;
  }
  const Object *ob = active_object(C);
  fprintf(stdout,
          "fl-ui-scene: '%s' montado; objeto activo '%s' tipo %d.\n",
          spec,
          (ob != nullptr) ? ob->id.name + 2 : "(ninguno)",
          (ob != nullptr) ? int(ob->type) : -1);
  return true;
}

}  // namespace flipendo::properties_ui
