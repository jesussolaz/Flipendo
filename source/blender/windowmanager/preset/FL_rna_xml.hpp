/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * `scripts/modules/rna_xml.py` en C++: volcar un árbol RNA a XML y volver a leerlo.
 *
 * Por qué existe
 * -------------
 * Es el bloqueo del quinto puente histórico de C++ a Python. Los **temas** de la
 * interfaz no son presets de propiedades (ver politicas/PRESETS-A-DATOS.md): son XML, y
 * quien los serializa es este módulo. Mientras estuviera en Python no se podían migrar
 * ni `script.execute_preset` ni los tres operadores de tema, porque los dos necesitan
 * escribir y leer ese XML.
 *
 * Y los temas son capacidad viva, no herencia muerta: el gestor de extensiones instala
 * temas además de add-ons.
 *
 * Qué es cada mitad
 * -----------------
 * - **Serialización XML**: resuelta. El árbol ya trae `pugixml` (lo usa la exportación
 *   SVG de lápiz de grasa). Para ESCRIBIR no se usa: el formato del original no es XML
 *   canónico sino un texto con una sangría muy concreta, y hay que reproducirlo tal
 *   cual, así que se escribe a mano. Para LEER sí se usa pugixml.
 * - **Introspección de RNA**: es el grueso, y es lo que de verdad se migra aquí.
 *
 * El formato, que hay que reproducir al byte
 * ------------------------------------------
 * ```
 * <bpy>
 *   <Theme>
 *     <user_interface>
 *       <ThemeUserInterface
 *         menu_shadow_fac="0.4"
 *         editor_border="#161616"
 *         >
 *       </ThemeUserInterface>
 *     </user_interface>
 *   </Theme>
 * </bpy>
 * ```
 * El elemento lleva el nombre del **tipo RNA**; los sub-punteros van envueltos en un
 * elemento con el nombre de la **propiedad**. El `>` que cierra la apertura va en la
 * sangría de los atributos, no en la del elemento. Sí, es raro; es lo que hay.
 *
 * Ver politicas/RNA-XML-A-CPP.md.
 */

#pragma once

#include <string>

#include "BLI_span.hh"
#include "BLI_string_ref.hh"

struct bContext;

namespace flipendo::rna_xml {

/** Una fila del `preset_xml_map` de la clase de menú. */
struct MapEntry {
  /** Ruta RNA relativa al contexto, p.ej. `"preferences.themes[0]"`. */
  const char *rna_path;
  /** Nombre del elemento XML que se busca al leer, p.ej. `"Theme"`. */
  const char *xml_tag;
};

/**
 * `rna_xml.xml_file_write`: escribe `<bpy>` con un árbol por cada entrada del mapa.
 *
 * Sólo implementa el modo `'ATTR'` del original (el que usan los temas). El modo
 * `'DATA'`, que recorría `bpy.data` entero, no lo usaba nadie en el árbol y no se
 * traslada; ver la deuda en la política.
 */
bool write_file(bContext *C,
                const std::string &filepath,
                blender::Span<MapEntry> rna_map,
                std::string &r_error);

/**
 * `rna_xml.xml_file_run`: lee el fichero y asigna por RNA.
 *
 * `secure_types` es el salvavidas del original: si no está vacío, sólo se entra en los
 * elementos cuyo nombre esté en la lista. Sirve para que un XML ajeno no se salga del
 * tema hacia las preferencias. Se conserva con el mismo significado.
 */
bool run_file(bContext *C,
              const std::string &filepath,
              blender::Span<MapEntry> rna_map,
              blender::Span<const char *> secure_types,
              std::string &r_error);

/** Serializa un árbol a texto sin tocar el disco. Público porque lo usa el arnés. */
bool write_string(bContext *C,
                  blender::Span<MapEntry> rna_map,
                  std::string &r_text,
                  std::string &r_error);

/**
 * `--fl-selftest-theme-xml <fichero>` / `--fl-check-theme-xml <linea-base>`.
 * Ver `fl_rna_xml_selftest.cc`. Línea base: `tests/flipendo/themexml/baseline-python.txt`.
 */
namespace selftest {
bool dump(bContext *C, const char *filepath);
bool check(bContext *C, const char *baseline_path);
}  // namespace selftest

}  // namespace flipendo::rna_xml
