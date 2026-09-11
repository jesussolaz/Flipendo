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
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_idprop.hh"

#include "WM_api.hh"
#include "WM_keymap.hh"

#include "FL_keymap_default.hpp"
#include "FL_keymap_params.hpp"

namespace {

using flipendo::keymap::AltMmbDragAction;
using flipendo::keymap::Params;
using flipendo::keymap::SpacebarAction;
using flipendo::keymap::TildeAction;

/** Un nombre invocado por el keymap, con por que vias y desde donde. */
struct Referencia {
  int call_menu = 0;
  int call_menu_pie = 0;
  int call_panel = 0;
  /** Primer keymap que lo nombra: es lo que hace falta para ir a buscarlo. */
  std::string primer_keymap;
  /** Primera configuracion de preferencias que lo hace aparecer. */
  std::string primera_variante;
  /** Falso si solo sale cambiando alguna preferencia. */
  bool en_defecto = false;
};

/** Una permutacion de preferencias con la que hay que construir el keymap. */
struct Variante {
  const char *nombre;
  void (*aplicar)(flipendo::keymap::Params &p);
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

/**
 * Las preferencias que cambian QUE menu abre una tecla.
 *
 * No es una permutacion exhaustiva —son 17 opciones, o sea 2^17 configuraciones— sino
 * cada una movida por separado desde la de fabrica, que es lo que hace falta para que
 * todo nombre alcanzable aparezca al menos una vez. Si alguna vez hiciera falta un
 * nombre que solo sale con DOS preferencias a la vez, se anade aqui esa pareja.
 */
const Variante variantes[] = {
    {"legacy", [](Params &p) { p.legacy = true; }},
    {"select_mouse_right", [](Params &p) { p.select_mouse_right = true; }},
    {"use_mouse_emulate_3_button", [](Params &p) { p.use_mouse_emulate_3_button = true; }},
    {"use_alt_tool_or_cursor", [](Params &p) { p.use_alt_tool_or_cursor = true; }},
    {"spacebar_action=TOOL", [](Params &p) { p.spacebar_action = SpacebarAction::Tool; }},
    {"spacebar_action=SEARCH", [](Params &p) { p.spacebar_action = SpacebarAction::Search; }},
    {"use_key_activate_tools", [](Params &p) { p.use_key_activate_tools = true; }},
    {"use_region_toggle_pie", [](Params &p) { p.use_region_toggle_pie = true; }},
    {"use_select_all_toggle", [](Params &p) { p.use_select_all_toggle = true; }},
    {"use_gizmo_drag=off", [](Params &p) { p.use_gizmo_drag = false; }},
    {"use_fallback_tool=off", [](Params &p) { p.use_fallback_tool = false; }},
    {"use_fallback_tool_select_handled=off",
     [](Params &p) { p.use_fallback_tool_select_handled = false; }},
    {"use_v3d_tab_menu", [](Params &p) { p.use_v3d_tab_menu = true; }},
    {"use_v3d_shade_ex_pie", [](Params &p) { p.use_v3d_shade_ex_pie = true; }},
    {"use_v3d_mmb_pan", [](Params &p) { p.use_v3d_mmb_pan = true; }},
    {"use_alt_click_leader", [](Params &p) { p.use_alt_click_leader = true; }},
    {"use_pie_click_drag", [](Params &p) { p.use_pie_click_drag = true; }},
    {"use_alt_navigation=off", [](Params &p) { p.use_alt_navigation = false; }},
    {"use_file_single_click", [](Params &p) { p.use_file_single_click = true; }},
    {"v3d_tilde_action=GIZMO", [](Params &p) { p.v3d_tilde_action = TildeAction::Gizmo; }},
    {"v3d_alt_mmb_drag_action=ABSOLUTE",
     [](Params &p) { p.v3d_alt_mmb_drag_action = AltMmbDragAction::Absolute; }},
};

/** Recorre un keymap ya construido y acumula los nombres que invoca. */
void recoger(const wmKeyConfig *kc,
             const char *variante,
             const bool es_defecto,
             std::map<std::string, Referencia> &refs,
             int &elementos,
             int &sin_nombre)
{
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
        std::printf("SIN-NOMBRE keymap='%s' operador=%s variante=%s\n",
                    km->idname,
                    kmi->idname,
                    variante);
        continue;
      }
      Referencia &ref = refs[nombre];
      if (ref.primer_keymap.empty()) {
        ref.primer_keymap = km->idname;
        ref.primera_variante = variante;
      }
      ref.en_defecto = ref.en_defecto || es_defecto;
      ref.call_menu += es_menu ? 1 : 0;
      ref.call_menu_pie += es_pie ? 1 : 0;
      ref.call_panel += es_panel ? 1 : 0;
    }
  }
}

}  // namespace

bool FL_keymap_check_menus(wmWindowManager *wm, const char *list_filepath)
{
  if (wm == nullptr) {
    std::fprintf(stderr, "FL_keymap_check_menus: no hay gestor de ventanas\n");
    return false;
  }

  std::map<std::string, Referencia> refs;
  int elementos = 0;
  int sin_nombre = 0;
  int nombres_defecto = 0;

  /* Configuracion temporal, para no tocar la que usa el editor. Es el mismo camino que
   * `FL_keyconfig_dump_native`, y por eso esto vale en `--background`: el keymap no se
   * lee del editor, se construye aqui.
   *
   * Se construye una vez con las preferencias de fabrica y otra por cada preferencia
   * movida, porque hay teclas que abren un menu DISTINTO segun la preferencia
   * (`VIEW3D_MT_snap` frente a `VIEW3D_MT_snap_pie`...). Comprobar solo la de fabrica
   * dejaria sin mirar justo esos, y fallan igual de callados. */
  {
    wmKeyConfig *kc = WM_keyconfig_new(wm, "Flipendo Native (menus, defecto)", false);
    flipendo::keymap::register_default(kc);
    recoger(kc, "defecto", true, refs, elementos, sin_nombre);
    WM_keyconfig_remove(wm, kc);
  }
  nombres_defecto = int(refs.size());

  for (const Variante &variante : variantes) {
    flipendo::keymap::Params params = flipendo::keymap::params_from_preferences();
    variante.aplicar(params);
    params.finalize();

    char idname[128];
    SNPRINTF(idname, "Flipendo Native (menus, %s)", variante.nombre);
    wmKeyConfig *kc = WM_keyconfig_new(wm, idname, false);
    flipendo::keymap::register_default_with_params(kc, params);
    recoger(kc, variante.nombre, false, refs, elementos, sin_nombre);
    WM_keyconfig_remove(wm, kc);
  }

  FILE *lista = nullptr;
  if (list_filepath != nullptr && list_filepath[0] != '\0') {
    lista = std::fopen(list_filepath, "w");
    if (lista == nullptr) {
      std::fprintf(stderr, "FL_keymap_check_menus: no se pudo escribir %s\n", list_filepath);
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
                   "%s call_menu=%d call_menu_pie=%d call_panel=%d keymap='%s' variante=%s\n",
                   entrada.first.c_str(),
                   ref.call_menu,
                   ref.call_menu_pie,
                   ref.call_panel,
                   ref.primer_keymap.c_str(),
                   ref.en_defecto ? "defecto" : ref.primera_variante.c_str());
    }

    const bool hay_menu = WM_menutype_find(entrada.first, true) != nullptr;
    const bool hay_panel = WM_paneltype_find(entrada.first, true) != nullptr;

    if ((quiere_menu && !hay_menu) || (quiere_panel && !hay_panel)) {
      ausentes.push_back(entrada.first);
      std::printf(
          "AUSENTE %s (%s) call_menu=%d call_menu_pie=%d call_panel=%d keymap='%s' variante=%s\n",
          entrada.first.c_str(),
          quiere_panel ? "panel" : "menu",
          ref.call_menu,
          ref.call_menu_pie,
          ref.call_panel,
          ref.primer_keymap.c_str(),
          ref.en_defecto ? "defecto" : ref.primera_variante.c_str());
    }
  }

  std::printf(
      "FL-KEYMAP-MENUS nombres=%zu (defecto=%d, solo-con-preferencia=%zu) configuraciones=%zu "
      "elementos=%d call_menu=%d call_menu_pie=%d call_panel=%d resueltos=%zu ausentes=%zu "
      "sin-nombre=%d\n",
      refs.size(),
      nombres_defecto,
      refs.size() - size_t(nombres_defecto),
      ARRAY_SIZE(variantes) + 1,
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
  return ausentes.empty() && sin_nombre == 0;
}
