/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Volcado y verificacion de la INTERFAZ del editor.
 *
 * Por que existe
 * -------------
 * Lo que queda de Python en Flipendo es, sobre todo, `scripts/startup/bl_ui`:
 * 77 ficheros, 64.160 lineas, mas de mil paneles y menus. Migrarlos a C++ ya es
 * posible (`FL_ui_registry.hh`), pero hasta ahora no habia forma de DEMOSTRAR que
 * el panel migrado dibuja lo mismo que el de Python; sin esa prueba, migrar a
 * escala es tirar una moneda mil veces.
 *
 * Estos volcados son esa prueba. Mismo patron que `--fl-dump-keymap` y
 * `--fl-dump-tools`: el binario escribe el estado observable, se congela la linea
 * base con el Python todavia vivo, y el C++ tiene que reproducirla byte a byte.
 *
 * Dos niveles, porque uno solo no basta:
 *
 * 1. `--fl-dump-ui` — el REGISTRO. Cada `PanelType`, `MenuType` y `HeaderType`
 *    dado de alta, con su idname, etiqueta, espacio, region, categoria, contexto,
 *    padre, orden, banderas, opciones y contexto de traduccion. Prueba que el
 *    panel migrado *existe donde tiene que existir*. Corre en `--background`.
 * 2. `--fl-dump-ui-layout` — el DIBUJO. Ejecuta el `draw()` de cada panel, menu y
 *    cabecera sobre una escena de fabrica y serializa el arbol de `uiLayout` que
 *    sale: filas, columnas, separadores, etiquetas, propiedades, operadores con
 *    sus argumentos, popovers y plantillas. Prueba que el panel migrado *dibuja
 *    lo mismo*. Necesita modo grafico: sin ventana no hay region ni contexto.
 *
 * Y `--fl-check-ui <linea-base>` compara contra cualquiera de las dos lineas base
 * (distingue por la marca de la primera linea) y da el parte.
 *
 * Uso:  Blender --factory-startup -b --fl-dump-ui        tests/flipendo/ui/baseline-python.txt
 *       Blender --factory-startup    --fl-dump-ui-layout tests/flipendo/ui/baseline-python-layout.txt
 *       Blender --factory-startup -b --fl-check-ui       tests/flipendo/ui/baseline-python.txt
 *
 * Doctrina: politicas/LENGUAJE-CPP.md · Metodo: politicas/UI-A-CPP.md.
 */

#ifndef __FL_UI_DUMP_HPP__
#define __FL_UI_DUMP_HPP__

struct bContext;
struct uiBut;
struct uiItem;

namespace flipendo::ui_dump {

/** Escribe el registro de interfaz (paneles, menus y cabeceras). */
bool dump_registry(const bContext *C, const char *filepath);

/** Ejecuta el `draw()` de todo lo que se pueda dibujar y escribe el resultado. */
bool dump_layout(bContext *C, const char *filepath);

/**
 * Compara contra una linea base — de registro o de dibujo, lo decide la marca de
 * la primera linea — e informa por stdout. Devuelve `true` si no hay diferencias.
 */
bool check(bContext *C, const char *baseline_filepath);

/* -------------------------------------------------------------------- */
/** \name Puente con `interface_layout.cc`
 *
 * Los tipos concretos de item (`uiButtonItem`, `uiLayoutItemSplit`, ...) y el
 * propio `blender::ui::ItemType` son privados de `interface_layout.cc`: en el
 * header publico `uiItem::type_` existe pero su enumeracion solo esta declarada.
 * En vez de duplicar aqui esas structs — que es como se desincronizan las cosas —
 * el volcador pide lo justo por estas funciones, implementadas alli mismo.
 * \{ */

/** Nombre estable del tipo de item: `BUTTON`, `LAYOUT_ROW`, `LAYOUT_COLUMN`... */
const char *item_type_name(const uiItem *item);

/** El boton de un item de tipo `BUTTON`; `nullptr` si el item es un sub-layout. */
const uiBut *item_button(const uiItem *item);

/** Porcentaje de un `LAYOUT_SPLIT`. */
bool item_split_percentage(const uiItem *item, float *r_percentage);

/** Numero de columnas pedido a un `LAYOUT_COLUMN_FLOW` / `LAYOUT_ROW_FLOW`. */
bool item_flow_number(const uiItem *item, int *r_number);

/** Parametros de un `LAYOUT_GRID_FLOW`. */
bool item_grid_flow(const uiItem *item,
                    bool *r_row_major,
                    int *r_columns_len,
                    bool *r_even_columns,
                    bool *r_even_rows);

/** Propiedad que guarda el abierto/cerrado de un `LAYOUT_PANEL_HEADER`. */
const char *item_panel_open_prop(const uiItem *item);

/** \} */

}  // namespace flipendo::ui_dump

#endif /* __FL_UI_DUMP_HPP__ */
