/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Exportar e importar una configuracion de teclado **como datos**.
 *
 * Era el segundo sitio donde el editor GENERABA codigo Python. `Exportar
 * configuracion de teclado` llamaba a
 * `bl_keymap_utils/io.py: keyconfig_export_as_data()`, que escribia un fichero
 * `.py` con una lista literal `keyconfig_data` y, al final, el pie:
 *
 *     from bl_keymap_utils.io import keyconfig_import_from_data
 *     keyconfig_import_from_data(..., keyconfig_data, **keywords)
 *
 * Es decir: el editor escribia un programa, y volver a importarlo era
 * *ejecutarlo*. Mientras eso existiera, «cero Python» era falso por
 * construccion aunque no quedara ni un `.py` en el arbol, porque el propio
 * editor los fabricaba en el disco del usuario.
 *
 * Aqui se escribe un fichero de datos `.fkeyconfig` y se lee sin ejecutar nada.
 * La compatibilidad hacia atras se conserva **leyendo**, nunca escribiendo: el
 * `.py` que el usuario ya tenga exportado se analiza de forma nativa, sin
 * interprete, con el mismo criterio que los presets heredados.
 *
 * Ver politicas/DATOS-SIN-INTERPRETE.md.
 */

#pragma once

#include <string>

struct bContext;
struct wmKeyConfig;
struct wmWindowManager;
struct wmOperatorType;

namespace flipendo::keyconfig {

/** Version del formato `.fkeyconfig` que escribe y entiende esta build. */
constexpr int FORMAT_VERSION = 1;

/** Extension del formato de datos. */
constexpr const char *FILE_EXT = ".fkeyconfig";

/**
 * Escribe `kc` como datos.
 *
 * Reproduce la seleccion que hacia `keyconfig_export_as_data()`: los keymaps
 * modificados por el usuario (`keyconfigs.user`) mas los de `kc` que no esten
 * ya en esa lista, ordenados por nombre. Con `all_keymaps` se escriben todos,
 * modificados o no.
 */
bool export_to_file(wmWindowManager *wm,
                    wmKeyConfig *kc,
                    const std::string &filepath,
                    bool all_keymaps,
                    std::string &r_error);

/**
 * Lee un fichero y construye con el una configuracion nueva llamada `name`.
 *
 * Acepta `.fkeyconfig` (datos) y el `.py` heredado que escribia Blender, este
 * ultimo analizado **sin interprete**. Si ya existe una configuracion con ese
 * nombre se sustituye, igual que hacia `keyconfigs.new()`.
 */
bool import_from_file(bContext *C,
                      const std::string &filepath,
                      const std::string &name,
                      std::string &r_error);

/** Analiza el texto del formato de datos y lo aplica sobre `kc`. */
bool read_fkeyconfig_text(const std::string &text,
                          const char *origin,
                          wmKeyConfig *kc,
                          std::string &r_error);

/**
 * Analiza el `keyconfig_data` de un `.py` exportado por Blender y lo aplica
 * sobre `kc`. Analizador propio del subconjunto exacto que generaba
 * `keyconfig_export_as_data()`: listas, tuplas, diccionarios y literales. Nada
 * de ejecutar. Cualquier otra cosa se rechaza con linea y motivo.
 */
bool read_legacy_python_text(const std::string &text,
                             const char *origin,
                             wmKeyConfig *kc,
                             std::string &r_error);

/**
 * `--fl-check-keyconfig-io <informe>`: exporta la configuracion activa, la
 * vuelve a importar en una configuracion nueva y compara las dos con el mismo
 * volcado determinista que usa `--fl-dump-keymap`. Si el ciclo pierde algo, el
 * volcado lo canta.
 */
bool check_roundtrip(bContext *C, const char *report_path, const char *legacy_py = nullptr);

}  // namespace flipendo::keyconfig

/* Operadores nativos. Mismos `idname` que tenian en `bl_operators/userpref.py`. */
void PREFERENCES_OT_keyconfig_export(wmOperatorType *ot);
void PREFERENCES_OT_keyconfig_import(wmOperatorType *ot);
