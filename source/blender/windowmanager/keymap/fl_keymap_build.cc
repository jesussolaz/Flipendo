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

#include "BKE_idprop.hh"

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

wmKeyMap *keymap_tool(wmKeyConfig *keyconf,
                      const char *idname,
                      const char *space_type,
                      const char *region_type)
{
  wmKeyMap *km = keymap(keyconf, idname, space_type, region_type);
  if (km) {
    km->flag |= KEYMAP_TOOL;
  }
  return km;
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
    /* Un modificador "pulsado" va en el campo directo; uno "cualquiera" va
     * desplazado con KMI_PARAMS_MOD_TO_ANY, que es como KeyMapItem_Params
     * distingue los dos casos (WM_keymap.hh:60-70). */
    int held = 0, any = 0;
    const struct {
      int8_t state;
      int bit;
    } mods[] = {
        {e.shift_, KM_SHIFT},
        {e.ctrl_, KM_CTRL},
        {e.alt_, KM_ALT},
        {e.oskey_, KM_OSKEY},
        {e.hyper_, KM_HYPER},
    };
    for (const auto &m : mods) {
      if (m.state == Event::kHeld) {
        held |= m.bit;
      }
      else if (m.state == Event::kAny) {
        any |= m.bit;
      }
    }
    p.modifier = held | KMI_PARAMS_MOD_TO_ANY(any);
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
  /* El Python comprueba `if "ctrl" in item_event` y luego `item_event.get("ctrl")`,
   * asi que tanto Ctrl pulsado como Ctrl-cualquiera (-1) generan gemelo. */
  if (e.ctrl_ == Event::kOff) {
    return false;
  }
  const char *t = e.type_ ? e.type_ : "";
  /* Ctrl-{tecla} sin Alt ni Shift. */
  if (e.alt_ == Event::kOff && e.shift_ == Event::kOff) {
    if (STREQ(t, "H") || STREQ(t, "M") || STREQ(t, "SPACE") || STREQ(t, "W") ||
        STREQ(t, "ACCENT_GRAVE") || STREQ(t, "PERIOD") || STREQ(t, "TAB"))
    {
      return false;
    }
  }
  /* Ctrl-Alt-Q sin Shift. */
  if (e.alt_ != Event::kOff && e.shift_ == Event::kOff && STREQ(t, "Q")) {
    return false;
  }
  return true;
}

static KeyMapItem_Params to_params_oskey(const Event &e)
{
  /* El gemelo traslada el valor de Ctrl a Cmd y deja Ctrl sin pulsar
   * (`item_event["oskey"] = item_event["ctrl"]; del item_event["ctrl"]`).
   * Traslada el VALOR, no solo el hecho: un Ctrl-cualquiera da un Cmd-cualquiera. */
  Event twin = e;
  twin.oskey_ = e.ctrl_;
  twin.ctrl_ = Event::kOff;
  return to_params(twin);
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

Item item_modal(wmKeyMap *km, const char *value, const Event &event)
{
  /* El valor se resuelve contra la enumeracion que declara el propio keymap modal,
   * que define el codigo nativo del operador. Si el keymap aun no la tiene, se
   * guarda el identificador como texto y el motor lo resuelve al enlazar. */
  const EnumPropertyItem *items = static_cast<const EnumPropertyItem *>(km->modal_items);
  int propvalue = 0;
  const bool resolved = (items != nullptr && RNA_enum_value_from_id(items, value, &propvalue));

  auto add = [&](const KeyMapItem_Params &p) {
    return resolved ? WM_modalkeymap_add_item(km, &p, propvalue) :
                      WM_modalkeymap_add_item_str(km, &p, value);
  };

  /* El pase de macOS tambien alcanza a los keymaps modales: el Python lo aplica
   * sobre la configuracion entera, no solo sobre los keymaps de operador. */
  wmKeyMapItem *kmi_oskey = nullptr;
  if (oskey_twin_wanted(event)) {
    const KeyMapItem_Params params_oskey = to_params_oskey(event);
    kmi_oskey = add(params_oskey);
    apply_repeat(kmi_oskey, event);
  }

  const KeyMapItem_Params params = to_params(event);
  wmKeyMapItem *kmi = add(params);
  apply_repeat(kmi, event);
  return Item(kmi, kmi_oskey);
}

/* Los envoltorios WM_keymap_add_menu y companeros ponen el nombre con
 * RNA_string_set directo, que no hace nada si el operador todavia no esta
 * registrado. Se construyen aqui con `string()`, que tiene respaldo a IDProperty. */
static Item item_named_op(wmKeyMap *km,
                          const char *op,
                          const char *name_value,
                          const Event &event)
{
  Item it = item(km, op, event);
  it.string("name", name_value);
  return it;
}

Item item_menu(wmKeyMap *km, const char *menu_idname, const Event &event)
{
  return item_named_op(km, "wm.call_menu", menu_idname, event);
}

Item item_menu_pie(wmKeyMap *km, const char *menu_idname, const Event &event)
{
  return item_named_op(km, "wm.call_menu_pie", menu_idname, event);
}

Item item_panel(wmKeyMap *km, const char *panel_idname, const Event &event)
{
  /* NO se usa WM_keymap_add_panel: ademas del nombre fija keep_open=false, y el
   * `op_panel` del Python solo pone el nombre. Esa propiedad de mas aparecia en el
   * volcado y hacia que el keymap "Window" no coincidiera. */
  return item_named_op(km, "wm.call_panel", panel_idname, event);
}

Item item_tool(wmKeyMap *km, const char *tool_idname, const Event &event)
{
  return item_named_op(km, "wm.tool_set_by_id", tool_idname, event);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Propiedades del operador
 * \{ */

/* -------------------------------------------------------------------- */
/** \name Escritura de propiedades con respaldo
 *
 * Un atajo puede referirse a un operador que TODAVIA no esta registrado. Pasa hoy
 * con la familia `wm.context_*`, que sigue viviendo en Python y no existe cuando el
 * keymap se construye al arrancar.
 *
 * En ese caso `kmi->ptr` no tiene RNA y `RNA_string_set` y companeros no hacen nada,
 * en silencio. El camino de Python no perdia el valor porque `setattr` sobre un grupo
 * de IDProperty crea la propiedad al vuelo (bl_keymap_utils/io.py:241). Aqui se
 * reproduce: si la propiedad no esta en RNA, se escribe como IDProperty cruda. El
 * motor la resuelve cuando el operador aparece.
 *
 * Este respaldo se puede quitar el dia que no queden operadores en Python.
 * \{ */

static bool rna_has_property(PointerRNA *ptr, const char *name)
{
  return ptr != nullptr && ptr->data != nullptr &&
         RNA_struct_find_property(ptr, name) != nullptr;
}

static void idprop_set(wmKeyMapItem *kmi, IDProperty *prop)
{
  if (kmi->properties == nullptr || prop == nullptr) {
    return;
  }
  IDP_ReplaceInGroup(kmi->properties, prop);
}

Item &Item::boolean(const char *name, const bool value)
{
  for (wmKeyMapItem *kmi : kmi_) {
    if (kmi == nullptr) {
      continue;
    }
    if (rna_has_property(kmi->ptr, name)) {
      RNA_boolean_set(kmi->ptr, name, value);
    }
    else {
      /* Un booleano de operador se guarda como entero, que es lo que produce
       * RNA_boolean_set cuando el operador SI esta registrado. Usar IDP_BOOLEAN
       * daria un tipo distinto para el mismo atajo segun el momento en que se
       * construya. */
      IDPropertyTemplate val = {};
      val.i = value ? 1 : 0;
      idprop_set(kmi, IDP_New(IDP_INT, &val, name));
    }
  }
  return *this;
}

Item &Item::integer(const char *name, const int value)
{
  for (wmKeyMapItem *kmi : kmi_) {
    if (kmi == nullptr) {
      continue;
    }
    if (rna_has_property(kmi->ptr, name)) {
      RNA_int_set(kmi->ptr, name, value);
    }
    else {
      IDPropertyTemplate val = {};
      val.i = value;
      idprop_set(kmi, IDP_New(IDP_INT, &val, name));
    }
  }
  return *this;
}

Item &Item::number(const char *name, const float value)
{
  for (wmKeyMapItem *kmi : kmi_) {
    if (kmi == nullptr) {
      continue;
    }
    if (rna_has_property(kmi->ptr, name)) {
      RNA_float_set(kmi->ptr, name, value);
    }
    else {
      IDPropertyTemplate val = {};
      val.f = value;
      idprop_set(kmi, IDP_New(IDP_FLOAT, &val, name));
    }
  }
  return *this;
}

Item &Item::string(const char *name, const char *value)
{
  for (wmKeyMapItem *kmi : kmi_) {
    if (kmi == nullptr) {
      continue;
    }
    if (rna_has_property(kmi->ptr, name)) {
      RNA_string_set(kmi->ptr, name, value);
    }
    else {
      idprop_set(kmi, IDP_NewString(value, name));
    }
  }
  return *this;
}

Item &Item::enum_(const char *name, const char *identifier)
{
  for (wmKeyMapItem *kmi : kmi_) {
    if (kmi == nullptr) {
      continue;
    }
    /* RNA_enum_set_identifier no dice nada si la propiedad no es una enumeracion, y
     * varias propiedades de operador que en el Python se escriben con comillas
     * simples son en realidad cadenas (wm.context_toggle_enum.value_1, por ejemplo).
     * Sin este aviso el valor se perdia sin dejar rastro. */
    PropertyRNA *prop = RNA_struct_find_property(kmi->ptr, name);
    if (prop == nullptr) {
      std::fprintf(stderr, "FL_keymap: propiedad desconocida: '%s'\n", name);
      continue;
    }
    if (RNA_property_type(prop) != PROP_ENUM) {
      std::fprintf(stderr,
                   "FL_keymap: '%s' no es una enumeracion; usa .string() en su lugar\n",
                   name);
      continue;
    }
    RNA_enum_set_identifier(nullptr, kmi->ptr, name, identifier);
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

/* -------------------------------------------------------------------- */
/** \name Propiedades anidadas de macros
 * \{ */

/* Baja a la sub-operacion `name` de una macro.
 *
 * El `property_unset` previo no es opcional: sin el, RNA_pointer_get materializa
 * TODOS los pasos de la macro anidada y aparecen grupos vacios que el keymap de
 * Python no tiene. El camino de Python hace exactamente esto
 * (bl_keymap_utils/io.py:236-238: property_unset y luego getattr). */
static PointerRNA sub_pointer(PointerRNA *ptr, const char *name)
{
  if (ptr == nullptr || ptr->data == nullptr) {
    return PointerRNA_NULL;
  }
  PropertyRNA *prop = RNA_struct_find_property(ptr, name);
  if (prop == nullptr) {
    std::fprintf(stderr, "FL_keymap: paso de macro desconocido: '%s'\n", name);
    return PointerRNA_NULL;
  }
  RNA_property_unset(ptr, prop);
  return RNA_pointer_get(ptr, name);
}

/* Grupo de IDProperty anidado, creandolo si hace falta. Es el respaldo para cuando
 * el operador macro aun no esta registrado y por tanto no hay RNA que recorrer. */
static IDProperty *sub_group(IDProperty *parent, const char *name)
{
  if (parent == nullptr) {
    return nullptr;
  }
  IDProperty *existing = IDP_GetPropertyFromGroup(parent, name);
  if (existing != nullptr && existing->type == IDP_GROUP) {
    return existing;
  }
  IDPropertyTemplate val = {};
  IDProperty *group = IDP_New(IDP_GROUP, &val, name);
  IDP_ReplaceInGroup(parent, group);
  return group;
}

Props Item::sub(const char *name)
{
  PointerRNA a = kmi_[0] ? sub_pointer(kmi_[0]->ptr, name) : PointerRNA_NULL;
  PointerRNA b = kmi_[1] ? sub_pointer(kmi_[1]->ptr, name) : PointerRNA_NULL;
  IDProperty *ga = (a.data == nullptr && kmi_[0]) ? sub_group(kmi_[0]->properties, name) : nullptr;
  IDProperty *gb = (b.data == nullptr && kmi_[1]) ? sub_group(kmi_[1]->properties, name) : nullptr;
  return Props(a, b, ga, gb);
}

Props Props::sub(const char *name)
{
  PointerRNA a = sub_pointer(&ptr_[0], name);
  PointerRNA b = sub_pointer(&ptr_[1], name);
  IDProperty *ga = (a.data == nullptr) ? sub_group(group_[0], name) : nullptr;
  IDProperty *gb = (b.data == nullptr) ? sub_group(group_[1], name) : nullptr;
  return Props(a, b, ga, gb);
}

Props &Props::boolean(const char *name, const bool value)
{
  for (int i = 0; i < 2; i++) {
    if (ptr_[i].data) {
      RNA_boolean_set(&ptr_[i], name, value);
    }
    else if (group_[i]) {
      IDPropertyTemplate val = {};
      val.i = value ? 1 : 0;
      IDP_ReplaceInGroup(group_[i], IDP_New(IDP_INT, &val, name));
    }
  }
  return *this;
}

Props &Props::integer(const char *name, const int value)
{
  for (int i = 0; i < 2; i++) {
    if (ptr_[i].data) {
      RNA_int_set(&ptr_[i], name, value);
    }
    else if (group_[i]) {
      IDPropertyTemplate val = {};
      val.i = value;
      IDP_ReplaceInGroup(group_[i], IDP_New(IDP_INT, &val, name));
    }
  }
  return *this;
}

Props &Props::number(const char *name, const float value)
{
  for (int i = 0; i < 2; i++) {
    if (ptr_[i].data) {
      RNA_float_set(&ptr_[i], name, value);
    }
    else if (group_[i]) {
      IDPropertyTemplate val = {};
      val.f = value;
      IDP_ReplaceInGroup(group_[i], IDP_New(IDP_FLOAT, &val, name));
    }
  }
  return *this;
}

Props &Props::string(const char *name, const char *value)
{
  for (int i = 0; i < 2; i++) {
    if (ptr_[i].data) {
      RNA_string_set(&ptr_[i], name, value);
    }
    else if (group_[i]) {
      IDP_ReplaceInGroup(group_[i], IDP_NewString(value, name));
    }
  }
  return *this;
}

Props &Props::boolean_array(const char *name, std::initializer_list<bool> values)
{
  for (PointerRNA &p : ptr_) {
    if (p.data == nullptr) {
      continue;
    }
    PropertyRNA *prop = RNA_struct_find_property(&p, name);
    if (prop == nullptr) {
      std::fprintf(stderr, "FL_keymap: propiedad desconocida: '%s'\n", name);
      continue;
    }
    int i = 0;
    for (const bool value : values) {
      RNA_property_boolean_set_index(&p, prop, i++, value);
    }
  }
  return *this;
}

Props &Props::number_array(const char *name, std::initializer_list<float> values)
{
  for (PointerRNA &p : ptr_) {
    if (p.data == nullptr) {
      continue;
    }
    PropertyRNA *prop = RNA_struct_find_property(&p, name);
    if (prop == nullptr) {
      std::fprintf(stderr, "FL_keymap: propiedad desconocida: '%s'\n", name);
      continue;
    }
    int i = 0;
    for (const float value : values) {
      RNA_property_float_set_index(&p, prop, i++, value);
    }
  }
  return *this;
}

Props &Props::enum_(const char *name, const char *identifier)
{
  for (PointerRNA &p : ptr_) {
    if (p.data) {
      RNA_enum_set_identifier(nullptr, &p, name, identifier);
    }
  }
  return *this;
}

/** \} */

Event &any_except(Event &e, std::initializer_list<const char *> except)
{
  e.shift_ = Event::kAny;
  e.ctrl_ = Event::kAny;
  e.alt_ = Event::kAny;
  e.oskey_ = Event::kAny;
  e.hyper_ = Event::kAny;
  for (const char *name : except) {
    if (STREQ(name, "shift")) e.shift_ = Event::kOff;
    else if (STREQ(name, "ctrl")) e.ctrl_ = Event::kOff;
    else if (STREQ(name, "alt")) e.alt_ = Event::kOff;
    else if (STREQ(name, "oskey")) e.oskey_ = Event::kOff;
    else if (STREQ(name, "hyper")) e.hyper_ = Event::kOff;
  }
  return e;
}

}  // namespace flipendo::keymap
