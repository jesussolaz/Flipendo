/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ketsji
 *
 * Implementacion de la sonda. El razonamiento, en `FL_GameDumpProbe.hpp`.
 */

#include "FL_GameDumpProbe.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "DNA_object_types.h"

#include "CM_Message.hpp"
#include "EXP_ListValue.hpp"
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
  fprintf(fp, "# FL_GAME_DUMP v1 escena=%s frame=%d\n", scene->GetName().c_str(), frames);

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
               "OBJ nombre=%s tipoblender=%s visible=%d pos=(%.2f,%.2f,%.2f)\n",
               obj->GetName().c_str(),
               blob ? blender_type_name(blob->type) : "SIN-OBJETO",
               obj->GetVisible() ? 1 : 0,
               double(pos.x()),
               double(pos.y()),
               double(pos.z()));
      lines.push_back(buf);
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
