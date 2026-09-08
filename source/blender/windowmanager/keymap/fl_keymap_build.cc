/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Implementacion del andamiaje del keymap. Ver FL_keymap_build.hpp.
 *
 * Todos los identificadores se resuelven por RNA, que es la MISMA tabla que usaba el
 * camino de Python. Asi no hay un mapeo paralelo escrito a mano que pueda
 * desincronizarse: si un nombre de tecla cambia en RNA, cambia para los dos.
 */

#include "FL_keymap_build.hpp"

#include <cstdio>

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_string.h"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "WM_api.hh"
#include "WM_keymap.hh"
#include "WM_types.hh"

namespace flipendo::keymap {

/* -------------------------------------------------------------------- */
/** \name Resolucion de identificadores
 * \{ */

/* Un identificador que no existe es un error del programador, no del usuario: se
 * avisa una vez y se devuelve el valor de reserva, para que la migracion no se
 * detenga pero el fallo se vea. */
static int enum_value(const EnumPropertyItem *items,
                      const char *identifier,
                      const int fallback,
                      const char *what)
{
  if (identifier == nullptr) {
    return fallback;
  }
  int value = fallback;
  if (!RNA_enum_value_from_id(items, identifier, &value)) {
    std::fprintf(stderr, "FL_keymap: %s desconocido: '%s'\n", what, identifier);
    return fallback;
  }
  return value;
}

static int event_type_value(const char *identifier)
{
  return enum_value(rna_enum_event_type_items, identifier, 0 /* EVENT_NONE */, "tipo de evento");
}

static int event_value_value(const char *identifier)
{
  return enum_value(rna_enum_event_value_items, identifier, KM_PRESS, "valor de evento");
}

static int event_direction_value(const char *identifier)
{
  if (identifier == nullptr) {
    return KM_ANY;
  }
  return enum_value(rna_enum_event_direction_items, identifier, KM_ANY, "direccion de evento");
}

static int space_type_value(const char *identifier)
{
  if (identifier == nullptr) {
    return SPACE_EMPTY;
  }
  return enum_value(rna_enum_space_type_items, identifier, SPACE_EMPTY, "tipo de espacio");
}

static int region_type_value(const char *identifier)
{
  if (identifier == nullptr) {
    return RGN_TYPE_WINDOW;
  }
  return enum_value(rna_enum_region_type_items, identifier, RGN_TYPE_WINDOW, "tipo de region");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Keymaps
 * \{ */

wmKeyMap *keymap(wmKeyConfig *keyconf,
                 const char *idname,
                 const char *space_type,
                 const char *region_type)
{
  return WM_keymap_ensure(
      keyconf, idname, space_type_value(space_type), region_type_value(region_type));
}

wmKeyMap *keymap_modal(wmKeyConfig *keyconf, const char *idname)
{
  return WM_modalkeymap_ensure(keyconf, idname, nullptr);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Atajos
 * \{ */

static KeyMapItem_Params to_params(const Event &e)
{
  KeyMapItem_Params p = {};
  p.type = int16_t(event_type_value(e.type_));
  p.value = int8_t(event_value_value(e.value_));
  p.direction = int8_t(event_direction_value(e.direction_));
  p.keymodifier = int16_t(event_type_value(e.key_modifier_));

  if (e.any_) {
    /* `"any": True` del Python: todos los modificadores en KM_ANY. */
    p.modifier = KM_ANY;
  }
  else {
    int modifier = 0;
    if (e.shift_) modifier |= KM_SHIFT;
    if (e.ctrl_) modifier |= KM_CTRL;
    if (e.alt_) modifier |= KM_ALT;
    if (e.oskey_) modifier |= KM_OSKEY;
    if (e.hyper_) modifier |= KM_HYPER;
    p.modifier = modifier;
  }
  return p;
}

/* El Python nombra los operadores en notacion de puntos ("logic.properties"); el
 * keymap los guarda en notacion de clase ("LOGIC_OT_properties"). */
static void op_bl_idname(char dst[OP_MAX_TYPENAME], const char *op_py)
{
  WM_operator_bl_idname(dst, op_py);
}

static void apply_repeat(wmKeyMapItem *kmi, const Event &e)
{
  /* El keymap guarda "ignorar repeticion", que es lo contrario de lo que dice el
   * diccionario de Python. */
  if (e.repeat_) {
    kmi->flag &= ~KMI_REPEAT_IGNORE;
  }
  else {
    kmi->flag |= KMI_REPEAT_IGNORE;
  }
}

/* En macOS, Cmd hace el papel de Ctrl. El camino de Python resolvia esto con un
 * pase sobre la configuracion entera
 * (`bl_keymap_utils/platform_helpers.py: keyconfig_data_oskey_from_ctrl_for_macos`,
 * invocado desde `presets/keyconfig/Blender.py:379`): por cada atajo con Ctrl
 * inserta ANTES un gemelo con Cmd. Estas son sus excepciones, donde Ctrl y Cmd no
 * son intercambiables porque el sistema ya usa la combinacion.
 *
 * Flipendo es Mac-only, asi que este pase esta siempre activo; en vez de
 * transformar la lista a posteriori, se anade el gemelo justo antes, que da el
 * mismo orden y no puede olvidarse. */
static bool oskey_twin_wanted(const Event &e)
{
  if (!e.ctrl_) {
    return false;
  }
  const char *t = e.type_ ? e.type_ : "";
  /* Ctrl-{tecla} sin Alt ni Shift. */
  if (!e.alt_ && !e.shift_) {
    if (STREQ(t, "H") || STREQ(t, "M") || STREQ(t, "SPACE") || STREQ(t, "W") ||
        STREQ(t, "ACCENT_GRAVE") || STREQ(t, "PERIOD") || STREQ(t, "TAB"))
    {
      return false;
    }
  }
  /* Ctrl-Alt-Q sin Shift. */
  if (e.alt_ && !e.shift_ && STREQ(t, "Q")) {
    return false;
  }
  return true;
}

static KeyMapItem_Params to_params_oskey(const Event &e)
{
  KeyMapItem_Params p = to_params(e);
  /* El gemelo cambia Ctrl por Cmd; el resto del evento es identico. */
  p.modifier &= ~KM_CTRL;
  p.modifier |= KM_OSKEY;
  return p;
}

Item item(wmKeyMap *km, const char *op, const Event &event)
{
  char idname[OP_MAX_TYPENAME];
  op_bl_idname(idname, op);

  wmKeyMapItem *kmi_oskey = nullptr;
  if (oskey_twin_wanted(event)) {
    const KeyMapItem_Params params_oskey = to_params_oskey(event);
    kmi_oskey = WM_keymap_add_item(km, idname, &params_oskey);
    apply_repeat(kmi_oskey, event);
  }

  const KeyMapItem_Params params = to_params(event);
  wmKeyMapItem *kmi = WM_keymap_add_item(km, idname, &params);
  apply_repeat(kmi, event);
  return Item(kmi, kmi_oskey);
}

static Item item_wrapper(wmKeyMap *km,
                         wmKeyMapItem *(*add)(wmKeyMap *, const char *, const KeyMapItem_Params *),
                         const char *idname,
                         const Event &event)
{
  wmKeyMapItem *kmi_oskey = nullptr;
  if (oskey_twin_wanted(event)) {
    const KeyMapItem_Params params_oskey = to_params_oskey(event);
    kmi_oskey = add(km, idname, &params_oskey);
    apply_repeat(kmi_oskey, event);
  }

  const KeyMapItem_Params params = to_params(event);
  wmKeyMapItem *kmi = add(km, idname, &params);
  apply_repeat(kmi, event);
  return Item(kmi, kmi_oskey);
}

Item item_menu(wmKeyMap *km, const char *menu_idname, const Event &event)
{
  return item_wrapper(km, WM_keymap_add_menu, menu_idname, event);
}

Item item_menu_pie(wmKeyMap *km, const char *menu_idname, const Event &event)
{
  return item_wrapper(km, WM_keymap_add_menu_pie, menu_idname, event);
}

Item item_panel(wmKeyMap *km, const char *panel_idname, const Event &event)
{
  return item_wrapper(km, WM_keymap_add_panel, panel_idname, event);
}

Item item_tool(wmKeyMap *km, const char *tool_idname, const Event &event)
{
  return item_wrapper(km, WM_keymap_add_tool, tool_idname, event);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Propiedades del operador
 * \{ */

Item &Item::boolean(const char *name, const bool value)
{
  for (wmKeyMapItem *kmi : kmi_) {
    if (kmi) {
      RNA_boolean_set(kmi->ptr, name, value);
    }
  }
  return *this;
}

Item &Item::integer(const char *name, const int value)
{
  for (wmKeyMapItem *kmi : kmi_) {
    if (kmi) {
      RNA_int_set(kmi->ptr, name, value);
    }
  }
  return *this;
}

Item &Item::number(const char *name, const float value)
{
  for (wmKeyMapItem *kmi : kmi_) {
    if (kmi) {
      RNA_float_set(kmi->ptr, name, value);
    }
  }
  return *this;
}

Item &Item::string(const char *name, const char *value)
{
  for (wmKeyMapItem *kmi : kmi_) {
    if (kmi) {
      RNA_string_set(kmi->ptr, name, value);
    }
  }
  return *this;
}

Item &Item::enum_(const char *name, const char *identifier)
{
  for (wmKeyMapItem *kmi : kmi_) {
    if (kmi) {
      RNA_enum_set_identifier(nullptr, kmi->ptr, name, identifier);
    }
  }
  return *this;
}

Item &Item::enum_flag(const char *name, std::initializer_list<const char *> identifiers)
{
  for (wmKeyMapItem *kmi : kmi_) {
    if (kmi == nullptr) {
      continue;
    }
    PropertyRNA *prop = RNA_struct_find_property(kmi->ptr, name);
    if (prop == nullptr) {
      std::fprintf(stderr, "FL_keymap: propiedad desconocida: '%s'\n", name);
      continue;
    }
    const EnumPropertyItem *items = nullptr;
    bool free_items = false;
    RNA_property_enum_items(nullptr, kmi->ptr, prop, &items, nullptr, &free_items);

    int flags = 0;
    for (const char *identifier : identifiers) {
      int value = 0;
      if (items && RNA_enum_value_from_id(items, identifier, &value)) {
        flags |= value;
      }
      else {
        std::fprintf(stderr, "FL_keymap: valor desconocido '%s' en '%s'\n", identifier, name);
      }
    }
    if (free_items && items) {
      MEM_freeN(items);
    }
    RNA_property_enum_set(kmi->ptr, prop, flags);
  }
  return *this;
}

Item &Item::boolean_array(const char *name, std::initializer_list<bool> values)
{
  for (wmKeyMapItem *kmi : kmi_) {
    if (kmi == nullptr) {
      continue;
    }
    PropertyRNA *prop = RNA_struct_find_property(kmi->ptr, name);
    if (prop == nullptr) {
      std::fprintf(stderr, "FL_keymap: propiedad desconocida: '%s'\n", name);
      continue;
    }
    int i = 0;
    for (const bool value : values) {
      RNA_property_boolean_set_index(kmi->ptr, prop, i++, value);
    }
  }
  return *this;
}

Item &Item::number_array(const char *name, std::initializer_list<float> values)
{
  for (wmKeyMapItem *kmi : kmi_) {
    if (kmi == nullptr) {
      continue;
    }
    PropertyRNA *prop = RNA_struct_find_property(kmi->ptr, name);
    if (prop == nullptr) {
      std::fprintf(stderr, "FL_keymap: propiedad desconocida: '%s'\n", name);
      continue;
    }
    int i = 0;
    for (const float value : values) {
      RNA_property_float_set_index(kmi->ptr, prop, i++, value);
    }
  }
  return *this;
}

/** \} */

}  // namespace flipendo::keymap
