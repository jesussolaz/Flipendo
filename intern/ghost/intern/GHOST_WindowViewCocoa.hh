/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 *
 * La NSView de Blender: dibuja y recibe el raton y el teclado.
 *
 * ANTES esto era un fichero de IMPLEMENTACION disfrazado de cabecera: contenia un
 * `@implementation` entero y `GHOST_WindowCocoa.mm` lo incluia DOS VECES, con macros
 * distintas, para generar `CocoaOpenGLView : NSOpenGLView` y `CocoaMetalView : NSView`.
 * Objective-C no tiene herencia multiple y esa era la forma de no duplicar el codigo.
 *
 * AHORA la clase se fabrica en tiempo de ejecucion, asi que el problema desaparece: las
 * mismas funciones de implementacion se registran en DOS clases con superclase distinta.
 * Ni macros, ni doble inclusion, ni codigo duplicado. Esta cabecera solo declara la API.
 */

#pragma once

#include "GHOST_ObjCRuntime.hh"

class GHOST_SystemCocoa;
class GHOST_WindowCocoa;

namespace ghost_cocoa_view {

/**
 * Devuelve la clase de la vista, fabricandola la primera vez.
 *
 * \param metal: `true` para la variante con superclase `NSView` (`CocoaMetalView`),
 * `false` para la de superclase `NSOpenGLView` (`CocoaOpenGLView`). Los nombres son
 * EXACTAMENTE esos porque `GHOST_WindowCocoa.hh` declara miembros con ese tipo.
 */
Class view_class(bool metal);

/**
 * Crea una vista y le ata su contexto de C++.
 *
 * El original hacia `[super init]` e IGNORABA el `frame` que recibia, asi que aqui se
 * usa `init` a secas y se conserva esa conducta. No hace falta ningun metodo propio de
 * Objective-C: el contexto se guarda en una variable de instancia desde C++.
 */
id view_create(bool metal, GHOST_SystemCocoa *system, GHOST_WindowCocoa *window);

#ifdef WITH_INPUT_IME
/** Equivalen a `[view beginIME:...]` y `[view endIME]`. Solo los llama GHOST_WindowCocoa,
 *  asi que no necesitan ser metodos de Objective-C: son funciones de C++ normales. */
void begin_ime(id view, int32_t x, int32_t y, int32_t w, int32_t h, bool completed);
void end_ime(id view);
#endif

}  // namespace ghost_cocoa_view
