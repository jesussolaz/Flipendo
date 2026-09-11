/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Ver `FL_keymap_menu_check.hpp`.
 */

#include "FL_keymap_menu_check.hpp"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "DNA_windowmanager_types.h"

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BKE_idprop.hh"

#include "WM_api.hh"
#include "WM_keymap.hh"

#include "FL_keymap_default.hpp"

namespace {

/** Un nombre invocado por el keymap, con por que vias y desde donde. */
struct Referencia {
  int call_menu = 0;
  int call_menu_pie = 0;
  int call_panel = 0;
  /** Primer keymap que lo nombra: es lo que hace falta para ir a buscarlo. */
  std::string primer_keymap;
};

/** `props["name"]` de un elemento de keymap, o `nullptr` si no lo lleva. */
const char *nombre_invocado(const wmKeyMapItem *kmi)
{
  if (kmi->properties == nullptr) {
    return nullptr;
  }
  const IDProperty *prop = IDP_GetPropertyFromGroup(kmi->properties, "name");
  if (prop == nullptr || prop->type != IDP_STRING) {
    return nullptr;
  }
  const char *value = IDP_String(prop);
  return (value != nullptr && value[0] != '\0') ? value : nullptr;
}

}  // namespace

bool FL_keymap_check_menus(wmWindowManager *wm, const char *list_filepath)
{
  if (wm == nullptr) {
    std::fprintf(stderr, "FL_keymap_check_menus: no hay gestor de ventanas\n");
    return false;
  }

  /* Configuracion temporal, para no tocar la que usa el editor. Es el mismo camino que
   * `FL_keyconfig_dump_native`, y por eso esto vale en `--background`: el keymap no se
   * lee del editor, se construye aqui. */
  wmKeyConfig *kc = WM_keyconfig_new(wm, "Flipendo Native (comprobacion de menus)", false);
  flipendo::keymap::register_default(kc);

  std::map<std::string, Referencia> refs;
  int elementos = 0;
  int sin_nombre = 0;

  LISTBASE_FOREACH (wmKeyMap *, km, &kc->keymaps) {
    LISTBASE_FOREACH (wmKeyMapItem *, kmi, &km->items) {
      const bool es_menu = STREQ(kmi->idname, "WM_OT_call_menu");
      const bool es_pie = STREQ(kmi->idname, "WM_OT_call_menu_pie");
      const bool es_panel = STREQ(kmi->idname, "WM_OT_call_panel");
      if (!(es_menu || es_pie || es_panel)) {
        continue;
      }
      elementos++;
      const char *nombre = nombre_invocado(kmi);
      if (nombre == nullptr) {
        /* Un `call_menu` sin `name` no abre nada y tampoco avisa: cuenta como fallo. */
        sin_nombre++;
        std::printf("SIN-NOMBRE keymap='%s' operador=%s\n", km->idname, kmi->idname);
        continue;
      }
      Referencia &ref = refs[nombre];
      if (ref.primer_keymap.empty()) {
        ref.primer_keymap = km->idname;
      }
      ref.call_menu += es_menu ? 1 : 0;
      ref.call_menu_pie += es_pie ? 1 : 0;
      ref.call_panel += es_panel ? 1 : 0;
    }
  }

  FILE *lista = nullptr;
  if (list_filepath != nullptr && list_filepath[0] != '\0') {
    lista = std::fopen(list_filepath, "w");
    if (lista == nullptr) {
      std::fprintf(stderr, "FL_keymap_check_menus: no se pudo escribir %s\n", list_filepath);
      WM_keyconfig_remove(wm, kc);
      return false;
    }
  }

  int total_menu = 0, total_pie = 0, total_panel = 0;
  std::vector<std::string> ausentes;
  for (const auto &entrada : refs) {
    const Referencia &ref = entrada.second;
    total_menu += ref.call_menu;
    total_pie += ref.call_menu_pie;
    total_panel += ref.call_panel;

    /* Un mismo nombre puede invocarse por las dos vias; se exige que resuelva por las
     * que de verdad se usan, y solo por esas. */
    const bool quiere_menu = (ref.call_menu + ref.call_menu_pie) > 0;
    const bool quiere_panel = ref.call_panel > 0;
    if (lista != nullptr) {
      std::fprintf(lista,
                   "%s call_menu=%d call_menu_pie=%d call_panel=%d keymap='%s'\n",
                   entrada.first.c_str(),
                   ref.call_menu,
                   ref.call_menu_pie,
                   ref.call_panel,
                   ref.primer_keymap.c_str());
    }

    const bool hay_menu = WM_menutype_find(entrada.first, true) != nullptr;
    const bool hay_panel = WM_paneltype_find(entrada.first, true) != nullptr;

    if ((quiere_menu && !hay_menu) || (quiere_panel && !hay_panel)) {
      ausentes.push_back(entrada.first);
      std::printf("AUSENTE %s (%s) call_menu=%d call_menu_pie=%d call_panel=%d keymap='%s'\n",
                  entrada.first.c_str(),
                  quiere_panel ? "panel" : "menu",
                  ref.call_menu,
                  ref.call_menu_pie,
                  ref.call_panel,
                  ref.primer_keymap.c_str());
    }
  }

  std::printf(
      "FL-KEYMAP-MENUS nombres=%zu elementos=%d call_menu=%d call_menu_pie=%d call_panel=%d "
      "resueltos=%zu ausentes=%zu sin-nombre=%d\n",
      refs.size(),
      elementos,
      total_menu,
      total_pie,
      total_panel,
      refs.size() - ausentes.size(),
      ausentes.size(),
      sin_nombre);

  if (lista != nullptr) {
    std::fclose(lista);
  }
  WM_keyconfig_remove(wm, kc);
  return ausentes.empty() && sin_nombre == 0;
}
