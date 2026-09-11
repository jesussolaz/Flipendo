/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edflipendo
 *
 * La ESCENA DE PELO del arnes: una cabeza y una melena de tipo `Curves`.
 *
 * Por que existe
 * --------------
 * El objetivo grande del carril PELO es que una melena de muchos cabellos llegue al
 * juego, se sombree, se simule y tenga niveles de detalle. Nada de eso se puede ni
 * empezar a probar si el objeto de pelo no llega a la escena del juego, y no habia
 * ninguna escena con pelo con la que mirarlo.
 *
 * Por que se construye con el binario y no con un script
 * ------------------------------------------------------
 * Misma razon que `fl_ui_scene_rich.cc`: un `.blend` en el repo es un asset, pero un
 * asset que nadie sabe regenerar es un dato opaco. Con `--fl-make-hair-scene` la
 * escena sale del codigo, es reproducible, y no depende del interprete — que es a
 * donde va este proyecto (politicas/PELO-A-CPP.md).
 *
 * Determinismo
 * ------------
 * El pelo lo pone `OBJECT_OT_curves_random_add`, que llama a
 * `ed::curves::primitive_random_sphere(500, 8)`. Su generador
 * (`RandomNumberGenerator rng;`) se construye con la semilla por defecto, asi que
 * los 500 mechones de 8 puntos salen IGUALES en cada ejecucion. Medido: tres
 * ejecuciones dan el mismo volcado de posiciones. Sin eso, la comparacion de
 * pixeles del Player no seria una linea base (leccion de las 03:50 del REGLAMENTO).
 *
 * Trampa pagada, heredada de VideoTexture y de la interfaz nativa
 * ---------------------------------------------------------------
 * El Player dibuja a traves del `View3D` guardado en el fichero: si su vista no esta
 * en modo camara (`region_3d.view_perspective = 'CAMERA'`) no se dibuja por la
 * camara y la captura no se parece a nada. Aqui se fuerza a mano sobre todos los
 * `View3D` de todas las pantallas antes de guardar, porque en `--background` no hay
 * ventana desde la que hacerlo con un operador.
 *
 * Uso:  Blender --factory-startup -b --fl-make-hair-scene tests/flipendo/pelo/escena-pelo.blend
 *
 * Doctrina: politicas/LENGUAJE-CPP.md · Metodo: politicas/PELO-A-CPP.md.
 */

#ifndef __FL_HAIR_SCENE_HH__
#define __FL_HAIR_SCENE_HH__

struct bContext;

namespace flipendo::hair_scene {

/**
 * Construye la escena de pelo (cabeza de malla + melena `Curves`) y la guarda en
 * `filepath`. Devuelve `false` — gritando por `stderr` — en cuanto un operador no
 * exista o no termine: un andamio que falla en silencio deja creyendo que se ha
 * verificado algo.
 */
bool make_scene(bContext *C, const char *filepath);

/**
 * Vuelca el estado observable del pelo de la escena cargada: cuantos objetos
 * `Curves` hay, cuantas curvas y puntos tiene cada uno, su caja envolvente y si el
 * depsgraph los evalua. Sirve de linea base para comprobar que el fichero no cambia
 * bajo los pies de la comparacion de pixeles. Vale en `--background`.
 */
bool dump_hair(bContext *C, const char *filepath);

}  // namespace flipendo::hair_scene

#endif /* __FL_HAIR_SCENE_HH__ */
