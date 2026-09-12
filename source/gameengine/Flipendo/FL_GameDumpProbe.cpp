/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ketsji
 *
 * Implementacion de la sonda. El razonamiento, en `FL_GameDumpProbe.hpp`.
 *
 * Formato v2 (noche de ANIMA, 2026-09-12): a la linea `OBJ` se le anaden, solo
 * cuando existen, dos cosas que la captura no puede demostrar:
 *
 *   accion=<nombre>@<frame>   la accion en marcha en la capa 0 (`BL_ActionManager`),
 *                             con su frame local a un decimal. Es la prueba de que el
 *                             personaje ANIMA (que el motor esta reproduciendo
 *                             `Andar`, `Ataque2`...), no solo de que se pinta.
 *   combo=<fase:indice> hp=<n>  en el objeto que lleva el PlayerController (el que
 *                             tiene `fl_component` = "PlayerController"): el estado
 *                             del combo y la vida, tal como los publica el componente.
 *
 * Todo lo demas se mantiene igual que en v1 (mismas lineas, mismo orden, misma
 * cuantizacion), asi que un `diff` entre un volcado v1 y uno v2 de la misma escena
 * solo cambia en la cabecera y en los objetos que animan.
 */

#include "FL_GameDumpProbe.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "DNA_object_types.h"

#include "BL_ActionManager.hpp"
#include "CM_Message.hpp"
#include "EXP_ListValue.hpp"
#include "EXP_Value.hpp"
#include "KX_GameObject.hpp"
#include "KX_Scene.hpp"

namespace flipendo {

/** Nombre estable del tipo de objeto de Blender del que salio el objeto de juego. */
static const char *blender_type_name(const short type)
{
  switch (type) {
    case OB_EMPTY:
      return "EMPTY";
    case OB_MESH:
      return "MESH";
    case OB_CURVES_LEGACY:
      return "CURVES_LEGACY";
    case OB_SURF:
      return "SURF";
    case OB_FONT:
      return "FONT";
    case OB_MBALL:
      return "MBALL";
    case OB_CURVES:
      return "CURVES";
    case OB_POINTCLOUD:
      return "POINTCLOUD";
    case OB_VOLUME:
      return "VOLUME";
    case OB_LAMP:
      return "LAMP";
    case OB_CAMERA:
      return "CAMERA";
    case OB_SPEAKER:
      return "SPEAKER";
    case OB_LIGHTPROBE:
      return "LIGHTPROBE";
    case OB_ARMATURE:
      return "ARMATURE";
    case OB_LATTICE:
      return "LATTICE";
    case OB_GREASE_PENCIL:
      return "GREASE_PENCIL";
    default:
      return "OTRO";
  }
}

/**
 * " accion=<nombre>@<frame>" si el objeto tiene una accion EN MARCHA en la capa 0;
 * cadena vacia si no. Se usa `GetActionManagerNoCreate` a proposito: la version que
 * crea el gestor le colgaria uno a cada objeto de la escena solo por volcarla, y una
 * sonda no debe cambiar lo que mide. Una accion PLAY que ya termino no cuenta como en
 * marcha (`IsActionDone`), aunque el gestor recuerde su nombre.
 */
static std::string action_suffix(KX_GameObject *obj)
{
  BL_ActionManager *am = obj->GetActionManagerNoCreate();
  if (am == nullptr || am->IsActionDone(0)) {
    return "";
  }
  const std::string name = am->GetActionName(0);
  if (name.empty()) {
    return "";
  }
  char buf[160];
  snprintf(buf, sizeof(buf), " accion=%s@%.1f", name.c_str(), double(am->GetActionFrame(0)));
  return buf;
}

/** " combo=<..> hp=<..>" para el objeto que lleva el PlayerController; vacia si no. */
static std::string player_suffix(KX_GameObject *obj)
{
  EXP_Value *comp = obj->GetProperty("fl_component");
  if (comp == nullptr || comp->GetText() != "PlayerController") {
    return "";
  }
  EXP_Value *combo = obj->GetProperty("combo");
  EXP_Value *hp = obj->GetProperty("hp");
  std::string out;
  if (combo != nullptr) {
    out += " combo=" + combo->GetText();
  }
  if (hp != nullptr) {
    out += " hp=" + std::to_string(int(hp->GetNumber()));
  }
  return out;
}

void FL_GameDumpProbeTick(KX_Scene *scene)
{
  static const char *pathEnv = std::getenv("FL_GAME_DUMP");
  static const std::string path = (pathEnv && pathEnv[0]) ? pathEnv : "";
  static const char *frameEnv = std::getenv("FL_GAME_DUMP_FRAME");
  static const int dumpFrame = (frameEnv && frameEnv[0]) ? atoi(frameEnv) : 60;
  static int frames = 0;
  static bool done = false;

  if (path.empty() || done || scene == nullptr) {
    return;
  }
  ++frames;
  if (frames < dumpFrame) {
    return;
  }
  done = true;

  FILE *fp = fopen(path.c_str(), "w");
  if (fp == nullptr) {
    CM_Error("FL_GAME_DUMP: no se pudo escribir '" << path << "'");
    return;
  }
  fprintf(fp, "# FL_GAME_DUMP v2 escena=%s frame=%d\n", scene->GetName().c_str(), frames);

  std::vector<std::string> lines;
  EXP_ListValue<KX_GameObject> *objs = scene->GetObjectList();
  if (objs) {
    for (KX_GameObject *obj : objs) {
      Object *blob = obj->GetBlenderObject();
      const MT_Vector3 pos = obj->NodeGetWorldPosition();
      char buf[512];
      /* Posicion a 2 decimales: el volcado se toma con la fisica corriendo y la
       * ultima cifra de un float acumulado no se reproduce entre ejecuciones. */
      snprintf(buf,
               sizeof(buf),
               "OBJ nombre=%s tipoblender=%s visible=%d pos=(%.2f,%.2f,%.2f)",
               obj->GetName().c_str(),
               blob ? blender_type_name(blob->type) : "SIN-OBJETO",
               obj->GetVisible() ? 1 : 0,
               double(pos.x()),
               double(pos.y()),
               double(pos.z()));
      lines.push_back(std::string(buf) + action_suffix(obj) + player_suffix(obj) + "\n");
    }
  }
  /* Ordenadas para que dos ejecuciones se puedan comparar con `diff`: el orden de
   * la lista del motor depende del orden de conversion, que no es parte de lo que
   * se quiere medir. */
  std::sort(lines.begin(), lines.end());
  for (const std::string &l : lines) {
    fputs(l.c_str(), fp);
  }
  fprintf(fp, "TOTAL objetos=%d\n", int(lines.size()));
  fclose(fp);

  CM_Message("FL_GAME_DUMP: frame " << frames << ", " << lines.size() << " objetos en '" << path
                                    << "'");
}

}  // namespace flipendo
