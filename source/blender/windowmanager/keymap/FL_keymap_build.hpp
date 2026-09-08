/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Andamiaje para escribir el mapa de teclado por defecto en C++.
 *
 * Sustituye a `scripts/presets/keyconfig/keymap_data/blender_default.py` (8.669
 * lineas, 235 funciones, 3.673 atajos), que hoy ejecuta `WM_keyconfig_reload` con
 * el interprete. Mientras eso siga ahi, ni el editor ni el Player pueden prescindir
 * de CPython — el Player tambien lo carga (`GPG_ghost.cpp:1704`).
 *
 * El objetivo de este andamiaje es que la transliteracion sea LINEA A LINEA, para
 * que se pueda revisar comparando contra el Python original. Por eso los
 * identificadores se pasan como las mismas cadenas que usa el Python ('N', 'PRESS',
 * "logic.properties") y se resuelven aqui: por RNA los eventos, con
 * `WM_operator_bl_idname` los operadores. Un mapeo de 200 constantes escrito a mano
 * habria sido una fuente de errores silenciosos.
 *
 *     Python:  ("logic.properties", {"type": 'N', "value": 'PRESS'}, None),
 *     C++:     item(km, "logic.properties", {"N", "PRESS"});
 *
 *     Python:  ("object.select_all", {"type": 'A', "value": 'PRESS'},
 *                {"properties": [("action", 'SELECT')]}),
 *     C++:     item(km, "object.select_all", {"A", "PRESS"}).enum_("action", "SELECT");
 *
 * La verificacion no es por lectura: `--fl-dump-keymap` vuelca el resultado y se
 * compara contra `tests/flipendo/keymap/baseline-python.txt`, que es lo que genera
 * hoy el Python.
 */

#ifndef __FL_KEYMAP_BUILD_HPP__
#define __FL_KEYMAP_BUILD_HPP__

#include <initializer_list>

#include "RNA_types.hh"

struct wmKeyConfig;
struct wmKeyMap;
struct IDProperty;
struct wmKeyMapItem;

namespace flipendo::keymap {

/**
 * El evento de un atajo.
 *
 * Se construye encadenando, para que la linea C++ se lea casi igual que el
 * diccionario del Python y no dependa del orden de los campos:
 *
 *     {"type": 'RIGHTMOUSE', "value": 'ANY', "ctrl": True}
 *     ev("RIGHTMOUSE", "ANY").ctrl()
 */
class Event {
 public:
  /** `type`: 'N', 'RIGHTMOUSE', 'WHEELUPMOUSE'... `value`: 'PRESS', 'CLICK_DRAG', 'ANY'... */
  Event(const char *type, const char *value = "PRESS") : type_(type), value_(value) {}

  /* Cada modificador tiene TRES estados, como en el Python: sin pulsar (0),
   * pulsado (True) y cualquiera (-1). El tercero lo produce `any_except()`. */
  Event &shift() { shift_ = kHeld; return *this; }
  Event &ctrl() { ctrl_ = kHeld; return *this; }
  Event &alt() { alt_ = kHeld; return *this; }
  Event &oskey() { oskey_ = kHeld; return *this; }
  Event &hyper() { hyper_ = kHeld; return *this; }

  /** El modificador da igual: `{"shift": -1}` del Python. */
  Event &shift_any() { shift_ = kAny; return *this; }
  Event &ctrl_any() { ctrl_ = kAny; return *this; }
  Event &alt_any() { alt_ = kAny; return *this; }
  Event &oskey_any() { oskey_ = kAny; return *this; }
  Event &hyper_any() { hyper_ = kAny; return *this; }

  /** `"any": True` — todos los modificadores dan igual. */
  Event &any() { any_ = true; return *this; }
  /** `"direction"`: 'NORTH', 'SOUTH'... solo tiene sentido con value 'CLICK_DRAG'. */
  Event &direction(const char *d) { direction_ = d; return *this; }
  /** `"key_modifier"`: una tecla que debe estar pulsada ademas, p.ej. 'D'. */
  Event &key_modifier(const char *k) { key_modifier_ = k; return *this; }
  /** `"repeat": True` — el atajo se repite al mantener la tecla. */
  Event &repeat() { repeat_ = true; return *this; }

  /** Estados de un modificador. */
  enum : int8_t { kOff = 0, kHeld = 1, kAny = -1 };

  const char *type_ = nullptr;
  const char *value_ = "PRESS";
  int8_t shift_ = kOff;
  int8_t ctrl_ = kOff;
  int8_t alt_ = kOff;
  int8_t oskey_ = kOff;
  int8_t hyper_ = kOff;
  bool any_ = false;
  const char *direction_ = nullptr;
  const char *key_modifier_ = nullptr;
  bool repeat_ = false;
};

/** Atajo para escribir `Event(...)` sin repetir el nombre del tipo. */
inline Event ev(const char *type, const char *value = "PRESS")
{
  return Event(type, value);
}

/**
 * `any_except(...)` del Python: todos los modificadores dan igual menos los que se
 * nombren, que quedan sin pulsar. Se encadena sobre un evento ya creado:
 *
 *     {**params.tool_maybe_tweak_event, **any_except("alt")}
 *     any_except(ev(...), {"alt"})
 */
Event &any_except(Event &e, std::initializer_list<const char *> except);

/**
 * Las propiedades de una sub-operacion de una macro.
 *
 * Un operador macro guarda las propiedades de cada paso en un puntero con el nombre
 * de ese paso, y pueden anidarse:
 *
 *     {"properties": [("NODE_OT_translate_attach",
 *                       [("TRANSFORM_OT_translate", [("view2d_edge_pan", True)])])]}
 *     .sub("NODE_OT_translate_attach").sub("TRANSFORM_OT_translate")
 *         .boolean("view2d_edge_pan", true)
 */
class Props {
 public:
  Props(const PointerRNA &a, const PointerRNA &b) : ptr_{a, b} {}
  Props(const PointerRNA &a, const PointerRNA &b, IDProperty *ga, IDProperty *gb)
      : ptr_{a, b}, group_{ga, gb}
  {
  }

  Props &boolean(const char *name, bool value);
  Props &integer(const char *name, int value);
  Props &number(const char *name, float value);
  Props &string(const char *name, const char *value);
  Props &enum_(const char *name, const char *identifier);
  Props &boolean_array(const char *name, std::initializer_list<bool> values);
  Props &number_array(const char *name, std::initializer_list<float> values);

  /** Baja otro nivel, al paso indicado de la macro. */
  Props sub(const char *name);

 private:
  /* Dos, por el gemelo con Cmd de macOS; el segundo puede estar vacio. */
  PointerRNA ptr_[2];
  /* Respaldo cuando el operador macro aun no esta registrado y no hay RNA: se
   * escribe directamente en el grupo de IDProperty. */
  IDProperty *group_[2] = {nullptr, nullptr};
};

/**
 * Un atajo recien anadido, sobre el que encadenar sus propiedades.
 *
 * Los metodos devuelven `*this` para poder escribir la lista de propiedades en una
 * linea, igual que el `{"properties": [...]}` del Python.
 */
class Item {
 public:
  explicit Item(wmKeyMapItem *kmi) : kmi_{kmi, nullptr} {}
  Item(wmKeyMapItem *kmi, wmKeyMapItem *kmi_oskey) : kmi_{kmi, kmi_oskey} {}

  Item &boolean(const char *name, bool value);
  Item &integer(const char *name, int value);
  Item &number(const char *name, float value);
  Item &string(const char *name, const char *value);
  /** Enumeracion por identificador, como en el Python (`'SELECT'`, no un numero). */
  Item &enum_(const char *name, const char *identifier);
  /** Enumeracion de tipo "bandera": varios identificadores a la vez. */
  Item &enum_flag(const char *name, std::initializer_list<const char *> identifiers);
  Item &boolean_array(const char *name, std::initializer_list<bool> values);
  Item &number_array(const char *name, std::initializer_list<float> values);
  /** Propiedades de un paso de una macro. Ver Props. */
  Props sub(const char *name);

  wmKeyMapItem *raw() const { return kmi_[0]; }

 private:
  /* En macOS cada atajo con Ctrl lleva ademas un gemelo con Cmd (ver
   * `keyconfig_data_oskey_from_ctrl_for_macos` del Python). Las propiedades tienen
   * que ir a los dos, o el gemelo quedaria sin ellas. */
  wmKeyMapItem *kmi_[2] = {nullptr, nullptr};
};

/**
 * Crea (o recupera) un keymap.
 *
 * `space_type` y `region_type` se pasan con los mismos identificadores que el
 * Python ('VIEW_3D', 'WINDOW'); `nullptr` equivale a 'EMPTY'.
 */
wmKeyMap *keymap(wmKeyConfig *keyconf,
                 const char *idname,
                 const char *space_type = nullptr,
                 const char *region_type = "WINDOW");

/**
 * Keymap de una HERRAMIENTA del sistema de herramientas.
 *
 * Igual que `keymap()` pero marcando `KEYMAP_TOOL`. Esa marca no viene de los datos
 * del keymap: en el camino de Python la pone el sistema de herramientas al registrar
 * cada una (`bl_ui/space_toolsystem_common.py:498`, con `tool=True`). Mientras ese
 * subsistema siga en Python hay que ponerla aqui, o los ~99 keymaps de herramienta
 * salen sin ella y el motor no los trata como tales.
 */
wmKeyMap *keymap_tool(wmKeyConfig *keyconf,
                      const char *idname,
                      const char *space_type = nullptr,
                      const char *region_type = "WINDOW");

/** Keymap modal: sus elementos llevan un valor de enumeracion, no un operador. */
wmKeyMap *keymap_modal(wmKeyConfig *keyconf, const char *idname);

/** Anade un atajo que invoca un operador. `op` va en notacion Python: "logic.properties". */
Item item(wmKeyMap *km, const char *op, const Event &event);

/**
 * Anade un atajo a un keymap MODAL. `value` es el identificador de la enumeracion
 * del propio keymap ('CANCEL', 'CONFIRM'...), igual que en el Python.
 */
Item item_modal(wmKeyMap *km, const char *value, const Event &event);

/** `op_menu(...)` del Python: abre un menu. */
Item item_menu(wmKeyMap *km, const char *menu_idname, const Event &event);
/** `op_menu_pie(...)`. */
Item item_menu_pie(wmKeyMap *km, const char *menu_idname, const Event &event);
/** `op_panel(...)`: abre un panel como popover. */
Item item_panel(wmKeyMap *km, const char *panel_idname, const Event &event);
/** `op_tool(...)`: activa una herramienta. */
Item item_tool(wmKeyMap *km, const char *tool_idname, const Event &event);

}  // namespace flipendo::keymap

#endif /* __FL_KEYMAP_BUILD_HPP__ */
