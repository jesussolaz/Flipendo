/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ketsji
 *
 * Sonda nativa: vuelca la LISTA DE OBJETOS DE LA ESCENA DEL JUEGO.
 *
 * Por que hace falta
 * ------------------
 * Una captura de pantalla dice si algo se PINTA, y eso es lo unico que ve el
 * jugador; pero no dice si el motor lo tiene. Son dos cosas distintas y en el caso
 * del pelo se separan de verdad: EEVEE dibuja por el depsgraph de Blender, asi que
 * un objeto que el convertidor NO convierte se sigue pintando — clavado donde
 * estaba, sin que el juego pueda tocarlo. Medido en el Player el 2026-09-12; ver
 * `politicas/PELO-A-CPP.md`.
 *
 * Por eso la evidencia del carril PELO son DOS: la captura (se ve) y este volcado
 * (el motor lo tiene). Con una sola no se distingue «el pelo llega al juego» de
 * «el pelo se queda pintado de adorno».
 *
 * Uso
 * ---
 *     FL_GAME_DUMP=<fichero> [FL_GAME_DUMP_FRAME=<n>] [FL_UI_EXIT=1] \
 *       Blenderplayer -w 800 600 100 100 <escena.blend>
 *
 * Escribe una linea por objeto de `scene->GetObjectList()`, ordenadas por nombre
 * para que el fichero sea comparable con `diff`, con el tipo del objeto de Blender
 * del que salio y su posicion cuantizada. La cuantizacion es a proposito: la lista
 * se vuelca con la fisica ya corriendo y la ultima cifra de un float acumulado no
 * se reproduce (leccion de las 03:50 del REGLAMENTO).
 *
 * No depende del interprete: se compila siempre.
 */

#ifndef __FL_GAMEDUMPPROBE_HPP__
#define __FL_GAMEDUMPPROBE_HPP__

class KX_Scene;

namespace flipendo {

/**
 * Se llama una vez por tic desde `FL_ComponentManager::Tick`. No hace nada si no
 * esta `FL_GAME_DUMP`. Cuenta frames — por eso va en el tic y no al atar la escena
 * (trampa numero 2 de `politicas/UI-JUEGO-NATIVA.md`).
 */
void FL_GameDumpProbeTick(KX_Scene *scene);

}  // namespace flipendo

#endif /* __FL_GAMEDUMPPROBE_HPP__ */
