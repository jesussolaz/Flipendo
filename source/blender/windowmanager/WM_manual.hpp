/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <string>

#include "BLI_string_ref.hh"

namespace flipendo::manual {

/**
 * Proveedor nativo de enlaces del manual.
 *
 * Devuelve true y escribe una URL absoluta cuando reconoce `rna_id`. Los
 * proveedores se consultan en orden inverso al registro, de modo que una
 * extensión nativa puede anteponer su documentación a la tabla integrada.
 */
using URLProvider = bool (*)(blender::StringRef rna_id, std::string &r_url);

void provider_register(URLProvider provider);
void provider_unregister(URLProvider provider);
bool url_lookup(blender::StringRef rna_id, std::string &r_url);

}  // namespace flipendo::manual
