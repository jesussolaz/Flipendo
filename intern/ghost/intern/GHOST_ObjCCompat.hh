/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 *
 * Puente de migracion de `intern/ghost` de Objective-C++ a C++ puro.
 *
 * ANDAMIO TEMPORAL. Existe solo mientras queden ficheros `.mm` en este directorio.
 * Cuando el ultimo `.mm` de `intern/ghost/intern` pase a `.cc`, la rama `__OBJC__` se
 * borra y las cabeceras se quedan con las declaraciones opacas a secas.
 *
 * EL PROBLEMA
 *
 * `GHOST_ContextCGL.hh` incluia `<Cocoa/Cocoa.h>`, `<Metal/Metal.h>` y
 * `<QuartzCore/QuartzCore.h>`, y declaraba miembros con sintaxis de Objective-C
 * (`@class`, `id<MTLTexture>`). La incluyen `mtl_context.mm` y
 * `mtl_command_buffer.mm`, asi que mientras siguiera asi **ninguno de los dos podia
 * ser un `.cc`**: un `.cc` ni siquiera puede leer la cabecera. Medido con las flags
 * reales del build, un fichero que solo la incluyera daba ~36.000 errores, todos
 * dentro de las cabeceras del SDK de Apple. Y de esos dos ficheros cuelgan
 * `mtl_texture.mm`, `mtl_state.mm` y `mtl_storage_buffer.mm`, que ya estan traducidos
 * y solo esperan a que se les desbloquee el enlace.
 *
 * LA TECNICA, Y POR QUE AQUI ES MEJOR QUE EN `gpu/metal`
 *
 * Medido con `nm` sobre dos objetos compilados con las flags reales del arbol:
 *
 *     .mm :  @class NSView;  void Foo::take(bool, NSView *, CAMetalLayer *, int)
 *     .cc :   class NSView;  void Foo::take(bool, NSView *, CAMetalLayer *, int)
 *
 *     los DOS exportan  __ZN3Foo4takeEbP6NSViewP12CAMetalLayeri
 *
 * Un puntero a clase de Objective-C y un puntero a una clase C++ declarada y no
 * definida con EL MISMO NOMBRE **manglan igual**. Por tanto, para las CLASES no hace
 * falta ni alias ni casteo ni perder tipado: basta declarar el mismo nombre de las
 * dos formas segun el modo, y la frontera de enlazado DESAPARECE. Es mejor que lo que
 * se pudo hacer en `gpu/metal`, donde el conflicto era `id<MTLDevice>` contra
 * `MTL::Device *`: dos nombres distintos, que obligaron a neutralizar 8 firmas.
 *
 * TRAMPA MEDIDA: con los PROTOCOLOS no vale. `id<T>` lleva el protocolo DENTRO del
 * simbolo:
 *
 *     id<MTLTexture>  ->  PU21objcproto10MTLTexture11objc_object
 *     id              ->  P11objc_object
 *
 * Asi que un `id<T>` en una firma que cruce la frontera hay que dejarlo en `id`
 * pelado (que sí es `objc_object *` en los dos modos) o quitarlo de la firma.
 * En los TIPOS DE RETORNO da igual: el ABI de Itanium no mangla el tipo de retorno de
 * las funciones que no son plantilla, asi que ahi si se puede usar un alias de doble
 * modo sin romper el enlace. Eso es justo lo que hacen los alias `GHOST_*Ptr` de
 * abajo.
 *
 * SEGUNDA TRAMPA MEDIDA: los nombres `MTL*` a secas ya estan OCUPADOS en modo C++.
 * `mtl_objc_compat.hh` (del backend de Metal) aliasa cientos de ellos al espacio de
 * nombres de metal-cpp: `using MTLDevice = MTL::Device;`. Declarar aqui una clase
 * opaca global con ese mismo nombre rompe en cuanto un `.cc` incluye las dos
 * cabeceras, que es exactamente el caso que esto viene a desbloquear
 * (`mtl_context`, `mtl_command_buffer`). Por eso esta cabecera NO declara ningun
 * `MTL*` global: en modo C++ los declara dentro de `namespace MTL`, y en modo
 * Objective-C como los `@protocol` que realmente son. El `@class MTLDevice;` que
 * tenia antes GHOST_ContextCGL.hh era una clase FALSA con el nombre de un protocolo,
 * un apaño heredado de Blender, y se ha retirado.
 */

#pragma once

#ifndef __APPLE__
#  error Apple OSX only!
#endif

/* Runtime de Objective-C. Es una biblioteca de C pura: se puede incluir desde un
 * `.cc` sin arrastrar una sola cabecera de AppKit. De aqui sale `id`, que es el unico
 * tipo de Objective-C que mangla igual en los dos modos. */
#include <objc/objc.h>

/* -------------------------------------------------------------------------
 * Declaraciones opacas: el MISMO nombre en los dos modos.
 *
 * Solo hacen falta para los tipos que aparecen en PARAMETROS o en MIEMBROS de datos.
 * `NSView` y `CAMetalLayer` son CLASES de verdad, asi que se declaran opacas y el
 * puntero mangla igual en un `.mm` y en un `.cc` (medido con `nm`, ver arriba). No
 * chocan con nada: `mtl_objc_compat.hh` no aliasa ninguno de los dos.
 *
 * Los nombres `MTL*` de Apple NO se declaran aqui a proposito. `mtl_objc_compat.hh`
 * ya aliasa cientos de ellos a `MTL::*` en su rama C++ (`using MTLDevice =
 * MTL::Device;`), y declarar una clase opaca global con ese mismo nombre daria
 * «redefinition of 'MTLDevice' as a different kind of symbol» en cuanto un `.cc`
 * incluyera las dos cabeceras, que es justo lo que queremos que pase. Por eso en modo
 * C++ se declaran en su propio espacio de nombres `MTL`, que es donde metal-cpp los
 * pone, y en modo Objective-C como los PROTOCOLOS que realmente son.
 */

#ifdef __OBJC__

@class CAMetalLayer;
@class NSView;
@protocol MTLCommandQueue;
@protocol MTLDevice;
@protocol MTLTexture;

#else

class CAMetalLayer;
class NSView;
namespace MTL {
class CommandQueue;
class Device;
class RenderPassDescriptor;
class RenderPipelineState;
class Texture;
}  // namespace MTL
namespace CA {
class MetalDrawable;
}  // namespace CA

#endif

/* -------------------------------------------------------------------------
 * Alias para TIPOS DE RETORNO y MIEMBROS de datos.
 *
 * El tipo de retorno no entra en el mangling (ABI de Itanium, funciones que no son
 * plantilla) y los miembros son el mismo puntero en los dos modos, asi que aqui se
 * puede conservar el tipado fuerte de cada lado: `id<MTLTexture>` para los `.mm` que
 * quedan y `MTL::Texture *` para los `.cc`. PROHIBIDO usarlos en un parametro: ahi
 * las dos grafias manglan distinto y el enlace falla.
 */

#ifdef __OBJC__
using GHOST_MTLTexturePtr = id<MTLTexture>;
using GHOST_MTLCommandQueuePtr = id<MTLCommandQueue>;
using GHOST_MTLDevicePtr = id<MTLDevice>;
#else
using GHOST_MTLTexturePtr = MTL::Texture *;
using GHOST_MTLCommandQueuePtr = MTL::CommandQueue *;
using GHOST_MTLDevicePtr = MTL::Device *;
#endif
