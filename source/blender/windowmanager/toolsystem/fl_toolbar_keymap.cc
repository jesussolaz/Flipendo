/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * El keymap del popup de la barra de herramientas.
 *
 * Transliteracion de `bl_keymap_utils.keymap_from_toolbar.generate`
 * (`scripts/modules/bl_keymap_utils/keymap_from_toolbar.py`). Se fabrica cada vez que se
 * abre el popup: copia el keymap "Toolbar Popup" del usuario y, para cada herramienta
 * visible, busca la tecla que ya la activa y la repite dentro del popup; a las que no
 * tienen ninguna les da un numero.
 *
 * Todo el orden de las pasadas importa: una tecla asignada en una pasada ya no la puede
 * usar la siguiente. Por eso se conservan tal cual, rarezas incluidas.
 */

#include <cstdio>
#include <cstring>
#include <string>

#include "BKE_context.hh"

#include "BLI_listbase.h"
#include "BLI_set.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "WM_api.hh"
#include "WM_keymap.hh"
#include "WM_types.hh"
#include "wm_event_types.hh"

#include "FL_toolsystem.hpp"

namespace flipendo::toolsystem {

/* -------------------------------------------------------------------- */
/** \name Huella de un atajo
 * \{ */

/**
 * La huella con la que el Python decide si una tecla ya esta ocupada: el diccionario de
 * `"type"` mas `modifier_keywords_from_item`, pasado por `dict_as_tuple`.
 *
 * En el Python el diccionario solo lleva lo que difiere del valor por defecto, pero dos
 * diccionarios son iguales exactamente cuando lo son todos los campos, asi que comparar
 * la estructura entera es lo mismo. Y en Python `True == 1`, asi que `shift=True` (de la
 * pasada de numeros) y `shift=1` (leido de un atajo) chocan; aqui los dos son
 * `KM_MOD_HELD`.
 */
struct KeyArgs {
  short type = EVENT_NONE;
  bool any = false;
  int8_t shift = KM_NOTHING;
  int8_t ctrl = KM_NOTHING;
  int8_t alt = KM_NOTHING;
  int8_t oskey = KM_NOTHING;
  int8_t hyper = KM_NOTHING;
  short key_modifier = EVENT_NONE;

  bool operator==(const KeyArgs &other) const
  {
    return type == other.type && any == other.any && shift == other.shift &&
           ctrl == other.ctrl && alt == other.alt && oskey == other.oskey &&
           hyper == other.hyper && key_modifier == other.key_modifier;
  }

  uint64_t hash() const
  {
    uint64_t h = uint64_t(uint16_t(type));
    h = h * 31 + uint64_t(any);
    h = h * 31 + uint64_t(uint8_t(shift));
    h = h * 31 + uint64_t(uint8_t(ctrl));
    h = h * 31 + uint64_t(uint8_t(alt));
    h = h * 31 + uint64_t(uint8_t(oskey));
    h = h * 31 + uint64_t(uint8_t(hyper));
    h = h * 31 + uint64_t(uint16_t(key_modifier));
    return h;
  }
};

/** `{"type": kmi.type, **modifier_keywords_from_item(kmi)}`. `kmi.any` es un getter: vale
 * si los cinco modificadores son "cualquiera". */
static KeyArgs key_args_from_item(const wmKeyMapItem *kmi)
{
  KeyArgs args;
  args.type = kmi->type;
  args.any = kmi->shift == KM_ANY && kmi->ctrl == KM_ANY && kmi->alt == KM_ANY &&
             kmi->oskey == KM_ANY && kmi->hyper == KM_ANY;
  args.shift = kmi->shift;
  args.ctrl = kmi->ctrl;
  args.alt = kmi->alt;
  args.oskey = kmi->oskey;
  args.hyper = kmi->hyper;
  args.key_modifier = kmi->keymodifier;
  return args;
}

/** `keymap_item_modifier_flag_from_args` de `rna_wm_api.cc`, que alli es `static`. */
static int16_t modifier_flag_from_args(bool any, int shift, int ctrl, int alt, int oskey, int hyper)
{
  int16_t modifier = 0;
  if (any) {
    return KM_ANY;
  }
  const auto assign = [&modifier](const int mod_var, const int16_t mod_flag) {
    if (mod_var == KM_MOD_HELD) {
      modifier |= mod_flag;
    }
    else if (mod_var == KM_ANY) {
      modifier |= KMI_PARAMS_MOD_TO_ANY(mod_flag);
    }
  };
  assign(shift, KM_SHIFT);
  assign(ctrl, KM_CTRL);
  assign(alt, KM_ALT);
  assign(oskey, KM_OSKEY);
  assign(hyper, KM_HYPER);
  return modifier;
}

/**
 * `keymap.keymap_items.new(idname, value=..., **args)` (`rna_KeyMap_item_new`). Por
 * defecto `repeat=False`, que pone `KMI_REPEAT_IGNORE`: sin eso, mantener pulsada una
 * tecla del popup la repetiria.
 */
static wmKeyMapItem *item_new(wmKeyMap *keymap,
                              const char *idname_bl,
                              const KeyArgs &args,
                              const int value)
{
  KeyMapItem_Params params{};
  params.type = args.type;
  params.value = value;
  params.modifier = modifier_flag_from_args(
      args.any, args.shift, args.ctrl, args.alt, args.oskey, args.hyper);
  params.keymodifier = args.key_modifier;
  params.direction = KM_ANY;
  wmKeyMapItem *kmi = WM_keymap_add_item(keymap, idname_bl, &params);
  kmi->flag |= KMI_REPEAT_IGNORE;
  return kmi;
}

static void item_set_tool(wmKeyMapItem *kmi, const char *idname)
{
  if (kmi != nullptr && kmi->ptr != nullptr) {
    RNA_string_set(kmi->ptr, "name", idname);
  }
}

/** `keyconf.keymaps.get(name)`: el primero con ese nombre, sin mirar espacio ni region. */
static wmKeyMap *keymap_by_name(wmKeyConfig *keyconf, const char *name)
{
  if (keyconf == nullptr || name == nullptr) {
    return nullptr;
  }
  LISTBASE_FOREACH (wmKeyMap *, km, &keyconf->keymaps) {
    if (STREQ(km->idname, name)) {
      return km;
    }
  }
  return nullptr;
}

/** Longitud del identificador RNA de un tipo de evento: 1 para las letras. */
static size_t event_identifier_len(const short type)
{
  const char *identifier = nullptr;
  if (!RNA_enum_identifier(rna_enum_event_type_items, type, &identifier) || identifier == nullptr)
  {
    return 0;
  }
  return strlen(identifier);
}

/** `wm.keyconfigs.find_item_from_operator(...)[1]`. */
static wmKeyMapItem *find_item(const bContext *C,
                               const char *idname,
                               const wmOperatorCallContext opcontext,
                               IDProperty *properties,
                               const short include_mask,
                               const short exclude_mask)
{
  char idname_bl[OP_MAX_TYPENAME];
  WM_operator_bl_idname(idname_bl, idname);
  wmKeyMap *unused = nullptr;
  return WM_key_event_operator(
      C, idname_bl, opcontext, properties, include_mask, exclude_mask, &unused);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Generacion
 * \{ */

wmKeyMap *toolbar_keymap_generate_in(bContext *C,
                                     const ToolbarDecl &toolbar,
                                     const char *mode,
                                     const bool use_fallback_keys,
                                     const bool use_reset)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  if (wm == nullptr || wm->userconf == nullptr) {
    return nullptr;
  }

  struct ItemContainer {
    const ToolDecl *tool = nullptr;
    /** El atajo que ya activa la herramienta (acceso directo). */
    wmKeyMapItem *found = nullptr;
    /** El atajo nuevo, dentro del popup. */
    wmKeyMapItem *exist = nullptr;
  };
  blender::Vector<ItemContainer> items_all;
  blender::Set<std::string> items_all_id;
  for (const ToolDecl *tool : tools_for_space_mode(C, toolbar, mode)) {
    items_all.append({tool});
    items_all_id.add(tool->idname);
  }

  /* Pulsar otra vez la tecla del popup deja la herramienta por defecto: el cursor, que en
   * la practica es "ninguna". Solo si esta en este contexto. */
  bool use_tap_reset = use_reset;
  const char *tap_reset_tool = "builtin.cursor";
  if (!items_all_id.contains(tap_reset_tool)) {
    use_tap_reset = false;
  }
  /* Soltar para confirmar, como un menu de tarta. */
  const bool use_release_confirm = use_reset;
  /* `use_auto_keymap_alpha` es falso en el Python: las letras se ponen a mano en el keymap
   * por defecto. Esa pasada no se traslada porque nunca corre. */
  const bool use_auto_keymap_num = use_fallback_keys;

  const char *km_name_default = "Toolbar Popup";
  const char *km_name = "Toolbar Popup <temp>";
  wmKeyConfig *keyconf_user = wm->userconf;
  /* `keyconfigs.active`: la de las preferencias, o la de por defecto. */
  wmKeyConfig *keyconf_active = static_cast<wmKeyConfig *>(
      BLI_findstring(&wm->keyconfigs, U.keyconfigstr, offsetof(wmKeyConfig, idname)));
  if (keyconf_active == nullptr) {
    keyconf_active = wm->defaultconf;
  }

  wmKeyMap *keymap = keymap_by_name(keyconf_active, km_name);
  if (keymap == nullptr) {
    keymap = WM_keymap_ensure(keyconf_active, km_name, SPACE_EMPTY, RGN_TYPE_TEMPORARY);
  }
  while (keymap->items.first != nullptr) {
    WM_keymap_remove_item(keymap, static_cast<wmKeyMapItem *>(keymap->items.first));
  }

  if (wmKeyMap *keymap_src = keymap_by_name(keyconf_user, km_name_default)) {
    LISTBASE_FOREACH (wmKeyMapItem *, kmi_src, &keymap_src->items) {
      /* Las herramientas que no se ven ahora no tienen tecla en el popup. */
      if (STREQ(kmi_src->idname, "WM_OT_tool_set_by_id")) {
        std::string name;
        if (kmi_src->ptr != nullptr) {
          char *value = RNA_string_get_alloc(kmi_src->ptr, "name", nullptr, 0, nullptr);
          name = value;
          MEM_freeN(value);
        }
        if (!items_all_id.contains(name)) {
          continue;
        }
      }
      WM_keymap_add_item_copy(keymap, kmi_src);
    }
  }

  blender::Set<KeyArgs> kmi_unique_args;
  const auto kmi_unique_or_pass = [&kmi_unique_args](const KeyArgs &args) {
    return kmi_unique_args.add(args);
  };

  /* Un atajo de usar y tirar, solo para poder pasarle propiedades a la busqueda. */
  wmKeyMapItem *kmi_hack = item_new(keymap, "WM_OT_tool_set_by_id", KeyArgs{}, KM_PRESS);
  kmi_hack->flag |= KMI_INACTIVE;
  IDProperty *kmi_hack_properties = kmi_hack->properties;

  short kmi_toolbar_type = EVENT_NONE;
  bool has_toolbar_key = false;
  KeyArgs kmi_toolbar_args;
  KeyArgs kmi_toolbar_args_type_only;
  if (use_release_confirm || use_tap_reset) {
    const wmKeyMapItem *kmi_toolbar = find_item(
        C, "wm.toolbar", WM_OP_INVOKE_DEFAULT, nullptr, EVT_TYPE_MASK_ALL, 0);
    has_toolbar_key = kmi_toolbar != nullptr;
    if (has_toolbar_key) {
      kmi_toolbar_type = kmi_toolbar->type;
    }
    if (use_tap_reset && has_toolbar_key) {
      kmi_toolbar_args_type_only.type = kmi_toolbar_type;
      kmi_toolbar_args = key_args_from_item(kmi_toolbar);
    }
    else {
      use_tap_reset = false;
    }
  }

  if (use_tap_reset) {
    /* Si el cursor ya tiene tecla propia, no hace falta el truco. */
    item_set_tool(kmi_hack, tap_reset_tool);
    if (find_item(C,
                  "wm.tool_set_by_id",
                  WM_OP_INVOKE_REGION_WIN,
                  kmi_hack_properties,
                  EVT_TYPE_MASK_KEYBOARD,
                  0))
    {
      use_tap_reset = false;
    }
  }
  if (use_tap_reset) {
    use_tap_reset = kmi_unique_or_pass(kmi_toolbar_args);
  }
  if (use_tap_reset) {
    items_all.remove_if([&](const ItemContainer &ic) {
      return STREQ(ic.tool->idname, tap_reset_tool);
    });
  }

  /* Acceso directo: la tecla que ya activa la herramienta, su operador o el primer atajo
   * de su keymap. */
  for (ItemContainer &ic : items_all) {
    item_set_tool(kmi_hack, ic.tool->idname);
    wmKeyMapItem *kmi_found = find_item(C,
                                        "wm.tool_set_by_id",
                                        WM_OP_INVOKE_REGION_WIN,
                                        kmi_hack_properties,
                                        EVT_TYPE_MASK_KEYBOARD,
                                        0);
    if (kmi_found != nullptr) {
      /* pass */
    }
    else if (ic.tool->op != nullptr) {
      kmi_found = find_item(
          C, ic.tool->op, WM_OP_INVOKE_REGION_WIN, nullptr, EVT_TYPE_MASK_KEYBOARD, 0);
    }
    else if (ic.tool->keymap_name != nullptr) {
      wmKeyMap *km = keymap_by_name(keyconf_user, ic.tool->keymap_name);
      if (km == nullptr) {
        printf("Keymap '%s' not found for tool %s\n", ic.tool->keymap_name, ic.tool->idname);
      }
      else if (const wmKeyMapItem *kmi_first = static_cast<wmKeyMapItem *>(km->items.first)) {
        kmi_found = find_item(
            C, kmi_first->idname, WM_OP_INVOKE_REGION_WIN, nullptr, EVT_TYPE_MASK_KEYBOARD, 0);
        if (kmi_found == nullptr) {
          /* Sin teclado, para encontrar los que usan una tecla como `key_modifier`. */
          kmi_found = find_item(C,
                                kmi_first->idname,
                                WM_OP_INVOKE_REGION_WIN,
                                nullptr,
                                EVT_TYPE_MASK_ALL,
                                EVT_TYPE_MASK_KEYBOARD);
          if (kmi_found != nullptr && kmi_found->keymodifier == EVENT_NONE) {
            kmi_found = nullptr;
          }
        }
      }
    }
    ic.found = kmi_found;
  }

  /* Tecla sola: solo letras, o cualquier cosa si ya era un `tool_set_by_id`. */
  for (ItemContainer &ic : items_all) {
    if (ic.found == nullptr) {
      continue;
    }
    if (event_identifier_len(ic.found->type) == 1 ||
        STREQ(ic.found->idname, "WM_OT_tool_set_by_id"))
    {
      const KeyArgs args = key_args_from_item(ic.found);
      if (kmi_unique_or_pass(args)) {
        wmKeyMapItem *kmi = item_new(keymap, "WM_OT_tool_set_by_id", args, KM_PRESS);
        item_set_tool(kmi, ic.tool->idname);
        ic.exist = kmi;
      }
    }
  }

  /* Tecla como modificador de raton: mantener 'D' en lapiz de grasa, por ejemplo. */
  static const short mouse_buttons[] = {
      LEFTMOUSE, RIGHTMOUSE, MIDDLEMOUSE, BUTTON4MOUSE, BUTTON5MOUSE, BUTTON6MOUSE, BUTTON7MOUSE};
  for (ItemContainer &ic : items_all) {
    if (ic.found == nullptr || ic.exist != nullptr) {
      continue;
    }
    bool is_mouse = false;
    for (const short button : mouse_buttons) {
      is_mouse |= ic.found->type == button;
    }
    if (!is_mouse || event_identifier_len(ic.found->keymodifier) != 1) {
      continue;
    }
    KeyArgs args = key_args_from_item(ic.found);
    args.type = ic.found->keymodifier;
    args.key_modifier = EVENT_NONE;
    if (kmi_unique_or_pass(args)) {
      wmKeyMapItem *kmi = item_new(keymap, "WM_OT_tool_set_by_id", args, KM_PRESS);
      item_set_tool(kmi, ic.tool->idname);
      ic.exist = kmi;
    }
  }

  /* Numeros para las que siguen sin tecla. El iterador es UNO para todas las
   * herramientas: cada una sigue donde lo dejo la anterior, y al agotarse no queda nada
   * para las siguientes. Se recorre modificador a modificador y, dentro, tecla a tecla. */
  if (use_auto_keymap_num) {
    static const short keys[] = {EVT_ONEKEY,
                                 EVT_TWOKEY,
                                 EVT_THREEKEY,
                                 EVT_FOURKEY,
                                 EVT_FIVEKEY,
                                 EVT_SIXKEY,
                                 EVT_SEVENKEY,
                                 EVT_EIGHTKEY,
                                 EVT_NINEKEY,
                                 EVT_ZEROKEY};
    static const short numpad[] = {EVT_PAD1,
                                   EVT_PAD2,
                                   EVT_PAD3,
                                   EVT_PAD4,
                                   EVT_PAD5,
                                   EVT_PAD6,
                                   EVT_PAD7,
                                   EVT_PAD8,
                                   EVT_PAD9,
                                   EVT_PAD0};
    const int keys_num = int(ARRAY_SIZE(keys));
    const int events_num = 4 * keys_num;
    int event_iter = 0;
    for (ItemContainer &ic : items_all) {
      if (ic.exist != nullptr) {
        continue;
      }
      bool have = false;
      int key_index = 0;
      KeyArgs args;
      while (event_iter < events_num) {
        const int mod = event_iter / keys_num;
        key_index = event_iter % keys_num;
        event_iter++;
        args = KeyArgs{};
        args.type = keys[key_index];
        args.shift = (mod == 1) ? KM_MOD_HELD : KM_NOTHING;
        args.ctrl = (mod == 2) ? KM_MOD_HELD : KM_NOTHING;
        args.alt = (mod == 3) ? KM_MOD_HELD : KM_NOTHING;
        if (!kmi_unique_args.contains(args)) {
          have = true;
          break;
        }
      }
      if (!have) {
        continue;
      }
      wmKeyMapItem *kmi = item_new(keymap, "WM_OT_tool_set_by_id", args, KM_PRESS);
      item_set_tool(kmi, ic.tool->idname);
      ic.exist = kmi;
      kmi_unique_args.add(args);

      /* El mismo numero en el teclado numerico, si esta libre. */
      KeyArgs dupe = args;
      dupe.type = numpad[key_index];
      if (!kmi_unique_args.contains(dupe)) {
        wmKeyMapItem *kmi_dupe = item_new(keymap, "WM_OT_tool_set_by_id", dupe, KM_PRESS);
        item_set_tool(kmi_dupe, ic.tool->idname);
        kmi_unique_args.add(dupe);
      }
    }
  }

  WM_keymap_remove_item(keymap, kmi_hack);

  /* Al final, para poder intentar una tecla sin modificadores si el popup se abrio con
   * ellos. */
  if (use_tap_reset) {
    KeyArgs available = kmi_toolbar_args;
    if (!(kmi_toolbar_args == kmi_toolbar_args_type_only)) {
      if (!kmi_unique_args.contains(kmi_toolbar_args_type_only)) {
        available = kmi_toolbar_args_type_only;
        kmi_unique_args.add(kmi_toolbar_args_type_only);
      }
    }
    wmKeyMapItem *kmi = item_new(keymap, "WM_OT_tool_set_by_id", available, KM_DBL_CLICK);
    item_set_tool(kmi, tap_reset_tool);
  }

  if (use_release_confirm && has_toolbar_key) {
    KeyArgs args;
    args.type = kmi_toolbar_type;
    args.any = true;
    wmKeyMapItem *kmi = item_new(keymap, "UI_OT_button_execute", args, KM_RELEASE);
    if (kmi->ptr != nullptr) {
      RNA_boolean_set(kmi->ptr, "skip_depressed", true);
    }
  }

  /* `wm.keyconfigs.update()`: `rna_KeyConfig_update` con `keep_properties=False`. */
  WM_keyconfig_update_ex(wm, false);
  return keymap;
}

wmKeyMap *toolbar_keymap_generate(bContext *C,
                                  const int space_type,
                                  const bool use_fallback_keys,
                                  const bool use_reset)
{
  const ToolbarDecl *toolbar = toolbar_for_space(space_type);
  if (toolbar == nullptr) {
    return nullptr;
  }
  const char *mode = toolbar->mode_from_context != nullptr ? toolbar->mode_from_context(C) :
                                                             nullptr;
  return toolbar_keymap_generate_in(C, *toolbar, mode, use_fallback_keys, use_reset);
}

/** \} */

}  // namespace flipendo::toolsystem
