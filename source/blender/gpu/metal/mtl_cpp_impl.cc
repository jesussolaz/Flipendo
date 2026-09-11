/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 *
 * metal-cpp es una biblioteca de solo cabeceras, pero necesita que EXACTAMENTE UN
 * traductor del programa defina estas tres macros antes de incluirla: son las que
 * materializan los simbolos privados (las tablas de selectores y las clases que el
 * resto de cabeceras solo declaran `extern`).
 *
 * Si no lo hace ninguno, el enlace falla con simbolos indefinidos de `NS::Private`.
 * Si lo hacen dos, falla con simbolos duplicados. Este fichero es ese traductor y no
 * debe contener nada mas.
 */

#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <QuartzCore/QuartzCore.hpp>
