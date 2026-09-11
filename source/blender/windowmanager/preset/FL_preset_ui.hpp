/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * El panel de presets, en C++.
 *
 * Por que existe
 * -------------
 * `PresetPanel` (`bl_ui/utils.py`) no es un panel mas: es el patron del que
 * cuelgan **17 clases en 12 ficheros** de `bl_ui` — camara, areas seguras,
 * materiales de lapiz, fluidos, formato y FFmpeg de salida, balance de blancos,
 * trazado de rayos, dinamica de pelo, color de nodo, tres del editor de clips,
 * tela, el editor de texto de Preferencias y los pinceles de la vista 3D.
 *
 * Su `draw()` llamaba a `Menu.draw_preset()` y este a `Menu.path_menu()`
 * (`bpy_types.py`), que ENUMERA una carpeta y pinta un elemento por fichero. El
 * carril de presets migro los DATOS — leer, aplicar, escribir, capturar, ver
 * `FL_preset.hpp` y `politicas/PRESETS-A-DATOS.md` — pero no ese dibujo, y
 * mientras faltara no habia forma de migrar ninguno de los 17. Era el camino
 * critico de toda la interfaz.
 *
 * La trampa, que conviene leer antes de tocar esto
 * -----------------------------------------------
 * En una instalacion de fabrica la carpeta de presets NO existe, asi que lo que
 * dibuja el Python es un `* Missing Paths *` y poco mas. Reproducir *eso* —
 * escribir una funcion que pinte esa etiqueta y nada mas — haria pasar al
 * verificador **perdiendo la capacidad entera**. Por eso esto se verifica con una
 * carpeta de presets de verdad, con ficheros dentro, comparando Python contra
 * C++. Ver `politicas/UI-A-CPP.md`.
 *
 * Doctrina: politicas/LENGUAJE-CPP.md.
 */

#ifndef __FL_PRESET_UI_HPP__
#define __FL_PRESET_UI_HPP__

#include <string>

#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

struct bContext;
struct uiLayout;

namespace flipendo::preset::ui {

/**
 * Las carpetas de presets que existen para una familia, en el mismo orden que
 * devolvia `bpy.utils.preset_paths(subdir)`: las rutas de scripts del sistema, las
 * del usuario y las de las preferencias, cada una con `presets/<subdir>` pegado, sin
 * repetidas y solo las que existen de verdad.
 */
blender::Vector<std::string> preset_paths(blender::StringRef subdir);

/**
 * El nombre para mostrar de un fichero, como `bpy.path.display_name`: quita carpeta
 * y extension, deshace los literales de caracteres que no caben en un nombre de
 * fichero (`_colon_` -> `:`, `_plus_` -> `+`, `_slash_` -> `/`), cambia los guiones
 * bajos por espacios y quita los espacios de la izquierda.
 *
 * \param title_case: los menus de preset lo piden en `false`, que es lo que hacia
 * `draw_preset` con su `title_case=False`.
 */
std::string display_name(blender::StringRef filename, bool title_case);

/** Lo que define un panel de presets; el equivalente de los campos de `PresetPanel`. */
struct MenuSpec {
  /** La carpeta bajo `presets/`: `"node_color"`, `"camera"`, `"ffmpeg"`... */
  const char *subdir = nullptr;
  /** El operador que aplica uno: casi siempre `"SCRIPT_OT_execute_preset"`. */
  const char *op = nullptr;
  /**
   * El `idname` del propio panel o menu. `script.execute_preset` lo necesita para
   * encontrar de vuelta el menu del que salio, asi que solo se pone con ese operador
   * — igual que hacia `path_menu`.
   */
  const char *menu_idname = nullptr;
  /** El operador de anadir/quitar, si la familia lo tiene. `nullptr` si no. */
  const char *add_op = nullptr;
};

/**
 * Dibuja el cuerpo de un menu de presets: el equivalente exacto de
 * `Menu.draw_preset()` + `Menu.path_menu()`.
 *
 * No toca el relieve ni el contexto de operador del layout que recibe; de eso se
 * encarga #draw_panel, que es lo que usan los paneles.
 */
void draw_menu(const bContext *C, uiLayout *layout, const MenuSpec &spec);

/**
 * Dibuja un panel de presets entero: pone el relieve de menu desplegable y el
 * contexto de operador que ponia `PresetPanel.draw()`, y llama a #draw_menu.
 *
 * Es lo que va en el `draw` de un `PanelDecl`.
 */
void draw_panel(const bContext *C, uiLayout *layout, const MenuSpec &spec);

/**
 * El `bl_label` del menu o panel de presets de una familia.
 *
 * `AddPresetBase.execute` terminaba con `preset_menu_class.bl_label = <nombre>`, o sea
 * escribiendo un atributo de clase de Python; asi es como el boton de presets acaba
 * mostrando el nombre del preset que se acaba de guardar (y "Presets" cuando se borra).
 * Sin interprete no hay atributo de clase que escribir, asi que la etiqueta vive aqui,
 * en un registro nativo indexado por el `idname` del menu o del panel.
 *
 * #menu_label_set escribe ademas, si el tipo existe ya registrado, el `label` del
 * `MenuType`/`PanelType` correspondiente, que es de donde lo saca el dibujo en C.
 *
 * Deuda declarada: un menu de presets que TODAVIA sea una clase de Python en `bl_ui`
 * lee su propio `cls.bl_label`, que desde aqui no se puede tocar. Ver
 * politicas/PRESETS-A-DATOS.md.
 */
void menu_label_set(blender::StringRef menu_idname, blender::StringRef label);
std::string menu_label_get(blender::StringRef menu_idname);

/**
 * El boton de preset que va en la cabecera de otro panel: el equivalente de
 * `PresetPanel.draw_panel_header()`, un popover sin relieve con el icono `PRESET`.
 */
void draw_panel_header(const bContext *C, uiLayout *layout, const char *panel_idname);

}  // namespace flipendo::preset::ui

#endif /* __FL_PRESET_UI_HPP__ */
