/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 *
 * Arnes de los tres operadores de niveles de detalle transliterados de
 * `bl_operators/object.py`. Ver `fl_lod_ops_selftest.cc` y politicas/LOD-A-CPP.md.
 *
 * Uso: Blender -b --fl-selftest-lod-ops <fichero>
 *      Blender -b --fl-check-lod-ops <linea-base>
 * Linea base: tests/flipendo/lod/baseline-python.txt
 */

#pragma once

struct bContext;

namespace flipendo::lod_selftest {

bool dump(bContext *C, const char *filepath);
bool check(bContext *C, const char *baseline_path);

}  // namespace flipendo::lod_selftest
