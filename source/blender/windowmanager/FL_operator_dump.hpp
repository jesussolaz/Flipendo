/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

struct bContext;

/**
 * Vuelca el contrato observable de todos los tipos de operador registrados.
 *
 * El formato es determinista: operadores, propiedades, banderas y claves se
 * ordenan por identificador. Los items de enum conservan su orden porque ese
 * orden si es conducta de interfaz. No incluye de donde procede el tipo (Python
 * o C++), porque precisamente se usa para comprobar que una migracion no se nota.
 */
bool FL_operators_dump(bContext *C, const char *filepath);

/** Ejecuta los siete operadores contextuales por idname sobre datos reales. */
bool FL_context_operators_selftest(bContext *C, const char *filepath);

/** Espera al primer ciclo de interfaz antes de ejecutar la prueba real. */
bool FL_context_operators_selftest_schedule(bContext *C, const char *filepath);

/** Exercise URL/document resolution and safe error branches of the native system operators. */
bool FL_wm_system_operators_selftest(bContext *C, const char *filepath);

/** Exercise add/remove/context-change on real custom properties and a Properties editor. */
bool FL_wm_property_operators_selftest(bContext *C, const char *filepath);

/** Wait for the graphical context required by the Properties editor test. */
bool FL_wm_property_operators_selftest_schedule(bContext *C, const char *filepath);
