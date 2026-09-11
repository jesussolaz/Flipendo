/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * `UnifiedPaintPanel`: los tres ayudantes de dibujo de
 * `scripts/startup/bl_ui/properties_paint_common.py` que comparten TODOS los paneles de
 * pintado del arbol (vista 3D, editor de imagen, Propiedades y la barra de herramientas).
 *
 * Por que vive en `editors/include/`
 * ---------------------------------
 * Es el mixin mas reutilizado del Python de interfaz. La decision del jefe de proyecto de
 * las 03:40 es explicita: «los `draw()` se exponen como funciones C++ compartidas en una
 * cabecera publica y el otro editor las llama; prohibido reimplementarlos por segunda
 * vez». Asi que aqui esta el unico sitio del arbol donde se escribe esta logica.
 *
 * El contrato de los fallos del Python
 * -----------------------------------
 * En Python estos ayudantes reciben `brush` y, cuando el pincel es `None`,
 * `layout.prop(None, ...)` lanza un `TypeError` que **corta el `draw()` a media faena**:
 * lo dibujado hasta ese punto se queda y lo que venia detras no se pinta. Eso es
 * comportamiento observable, no un accidente, y la linea base lo tiene congelado.
 *
 * Por eso estas funciones **no dibujan y avisan** en vez de reventar: devuelven `nullptr`
 * / `false` justo donde el Python habria lanzado la excepcion, y quien las llama tiene que
 * hacer `return` en ese momento — ni antes ni despues. Adelantar la comprobacion al
 * principio del `draw()` deja el panel vacio y lo canta el volcado.
 */

#pragma once

#include <optional>

#include "BLI_string_ref.hh"

#include "RNA_types.hh"

struct bContext;
struct uiLayout;

namespace flipendo::paint_common {

/**
 * `context.tool_settings.unified_paint_settings`.
 *
 * Devuelve `PointerRNA_NULL` si no hay escena; en Python eso seria un `AttributeError`
 * sobre `None.unified_paint_settings`.
 */
PointerRNA unified_paint_settings(const bContext *C);

/**
 * `UnifiedPaintPanel.prop_unified` (`properties_paint_common.py:260`).
 *
 * Ningun argumento tiene valor por defecto a proposito: el Python distingue `text=None`
 * (la etiqueta sale de la propiedad) de `text=""` (boton mudo), y esa diferencia ya ha
 * costado tres trampas en esta migracion. Aqui se escribe siempre cual de los dos es.
 *
 * \param unified_name: nombre de la propiedad de `UnifiedPaintSettings` que decide quien
 * es el dueno del valor, o `nullptr` (el `None` del Python). La cadena vacia es falsa,
 * igual que en Python.
 * \param pressure_name: propiedad del pincel para la presion del lapiz, o `nullptr`.
 * \param header: en la cabecera no se pinta el interruptor de ajustes unificados.
 *
 * \return la fila creada, o `nullptr` si el Python habria lanzado una excepcion (pincel
 * `None`). La fila YA esta creada cuando se devuelve `nullptr`: el Python tambien la
 * crea antes de reventar.
 */
uiLayout *prop_unified(uiLayout *layout,
                       const bContext *C,
                       PointerRNA *brush,
                       const char *prop_name,
                       const char *unified_name,
                       const char *pressure_name,
                       int icon,
                       std::optional<blender::StringRef> text,
                       bool slider,
                       bool header);

/**
 * `UnifiedPaintPanel.prop_unified_color` (`properties_paint_common.py:287`).
 *
 * \return `false` si el Python habria lanzado la excepcion (pincel `None` y color no
 * unificado).
 */
bool prop_unified_color(uiLayout *parent,
                        const bContext *C,
                        PointerRNA *brush,
                        const char *prop_name,
                        std::optional<blender::StringRef> text);

/**
 * `UnifiedPaintPanel.prop_unified_color_picker` (`properties_paint_common.py:293`).
 *
 * \return `false` si el Python habria lanzado la excepcion.
 */
bool prop_unified_color_picker(uiLayout *parent,
                               const bContext *C,
                               PointerRNA *brush,
                               const char *prop_name,
                               bool value_slider);

/**
 * `UnifiedPaintPanel.get_brush_mode` (`properties_paint_common.py:213`).
 *
 * Devuelve la cadena del modo (`"SCULPT"`, `"PAINT_VERTEX"`...) o `nullptr` donde el
 * Python devuelve `None`, que es su forma de decir «aqui no se pinta ninguna opcion de
 * pincel».
 */
const char *get_brush_mode(const bContext *C);

/**
 * `UnifiedPaintPanel.paint_settings` (`properties_paint_common.py:252`).
 *
 * Devuelve `PointerRNA_NULL` donde el Python devuelve `None`. Quien lo llame tiene que
 * cortar el dibujo en ese punto si el Python hacia `settings.brush` justo despues: eso
 * es un `AttributeError` sobre `None`.
 */
PointerRNA paint_settings(const bContext *C);

/**
 * `brush_basic_grease_pencil_weight_settings` (`properties_paint_common.py:1886`).
 *
 * \return `false` si el Python habria lanzado la excepcion (pincel `None`).
 */
bool brush_basic_grease_pencil_weight_settings(uiLayout *layout,
                                               const bContext *C,
                                               PointerRNA *brush,
                                               bool compact);

}  // namespace flipendo::paint_common
