/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * FL_ui_registry — registro DECLARATIVO de UI nativa en C++.
 *
 * Por que existe
 * -------------
 * El editor puede registrar toda su interfaz desde C++: `rna_Panel_register()`
 * (makesrna/intern/rna_ui.cc) no hace nada que C++ no pueda hacer — rellena un
 * `PanelType` y lo mete en la lista de la region, exactamente igual que
 * `graph_buttons.cc`. El bucle de dibujo (`ED_region_panels_layout_ex`) no sabe
 * de donde vino el tipo.
 *
 * Lo que faltaba no era capacidad, era ergonomia: a mano, cada panel son ~15
 * lineas de `MEM_callocN` + `STRNCPY` + `BLI_addtail`, y hay que portar 1.044
 * paneles, 565 menus y 23 cabeceras. Esta capa reduce cada uno a una struct
 * literal, y centraliza en un solo sitio lo que hoy solo hacia el registro de
 * Python: el orden de insercion, la categoria de reserva y el enlace de
 * sub-paneles con su padre.
 *
 * Uso
 * ---
 * \code
 * static const flipendo::PanelDecl panels[] = {
 *     {"LOGIC_PT_properties", N_("Properties"), "Logic",
 *      nullptr, nullptr, nullptr, nullptr,
 *      logic_panel_properties, nullptr, nullptr, logic_panel_poll},
 * };
 * flipendo::panels_register(art, SPACE_LOGIC, panels);
 * \endcode
 *
 * Vive en `editors/include/` a proposito: es el contrato con el que CUALQUIER modulo
 * de editor declara su interfaz, asi que tiene que estar donde todos miran sin anadir
 * rutas privadas a su CMakeLists.
 *
 * Doctrina: politicas/LENGUAJE-CPP.md.
 */

#pragma once

#include "BLI_span.hh"

struct ARegionType;
struct bContext;
struct Header;
struct HeaderType;
struct Menu;
struct MenuType;
struct Panel;
struct PanelType;

namespace flipendo {

/** Declaracion de un panel. Los campos no puestos quedan a cero, que es el
 * valor correcto para todos ellos. */
struct PanelDecl {
  /** `SPACE_PT_nombre`. Obligatorio. */
  const char *idname = nullptr;
  /** Titulo de la cabecera. Pasalo por `N_()` para que sea traducible. */
  const char *label = nullptr;
  /** Pestana lateral (region UI). Si se deja vacio se usa la de reserva. */
  const char *category = nullptr;
  /** Pestana del editor de Propiedades (`"object"`, `"material"`...). */
  const char *context = nullptr;
  /** `idname` del panel padre: convierte este en sub-panel. */
  const char *parent_id = nullptr;
  /** Tooltip. */
  const char *description = nullptr;
  /** Contexto de traduccion; por defecto `BLT_I18NCONTEXT_DEFAULT_BPYRNA`. */
  const char *translation_context = nullptr;

  void (*draw)(const bContext *C, Panel *panel) = nullptr;
  void (*draw_header)(const bContext *C, Panel *panel) = nullptr;
  void (*draw_header_preset)(const bContext *C, Panel *panel) = nullptr;
  bool (*poll)(const bContext *C, PanelType *pt) = nullptr;

  /** `PANEL_TYPE_DEFAULT_CLOSED`, `PANEL_TYPE_NO_HEADER`... */
  short flag = 0;
  /** Orden dentro de la region; a menor valor, mas arriba. */
  int order = 0;
  short ui_units_x = 0;
};

/** Declaracion de un menu. Los menus siempre viven en el registro global. */
struct MenuDecl {
  /** `SPACE_MT_nombre`. Obligatorio. */
  const char *idname = nullptr;
  const char *label = nullptr;
  const char *description = nullptr;
  const char *translation_context = nullptr;

  void (*draw)(const bContext *C, Menu *menu) = nullptr;
  bool (*poll)(const bContext *C, MenuType *mt) = nullptr;

  /** `MenuTypeFlag`: `MENU_TYPE_CONTEXT_DEPENDENT`, `MENU_TYPE_SEARCH_ON_KEY_PRESS`. */
  int flag = 0;
};

/** Declaracion de una cabecera de editor. */
struct HeaderDecl {
  /** `SPACE_HT_nombre`. Obligatorio. */
  const char *idname = nullptr;

  void (*draw)(const bContext *C, Header *header) = nullptr;
  bool (*poll)(const bContext *C, HeaderType *ht) = nullptr;
};

/**
 * Da de alta paneles en una region.
 *
 * Reproduce lo que hasta ahora solo hacia el camino de Python
 * (`rna_ui.cc:395-435`): inserta respetando `order` — con los paneles sin
 * cabecera por delante —, pone la categoria de reserva cuando la region tiene
 * pestanas y no se indico ninguna, enlaza los sub-paneles con su padre en su
 * sitio por orden, y da de alta el tipo en el registro global — esto ultimo
 * igual que hacia Python, para que la busqueda de paneles y los popovers los
 * encuentren.
 *
 * Un `parent_id` debe referirse a un panel ya declarado (en esta misma llamada
 * o en una anterior sobre la misma region).
 *
 * \param space_type: el `SPACE_*` del editor al que pertenece la region.
 */
void panels_register(ARegionType *art, int space_type, blender::Span<const PanelDecl> decls);

/** Da de alta menus en el registro global (`WM_menutype_add`). */
void menus_register(blender::Span<const MenuDecl> decls);

/** Da de alta cabeceras en la region indicada. */
void headers_register(ARegionType *art,
                      int space_type,
                      int region_type,
                      blender::Span<const HeaderDecl> decls);

}  // namespace flipendo
