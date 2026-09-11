/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 *
 * Llamar a Cocoa desde C++ PURO, sin compilador de Objective-C.
 *
 * POR QUE ESTO EXISTE
 *
 * `metal-cpp` (el binding oficial de Apple) cubre Foundation, Metal y QuartzCore,
 * pero NO cubre AppKit: no hay `NSWindow`, ni `NSView`, ni `NSApplication`. Y GHOST
 * es justo la capa que abre la ventana y recibe el raton y el teclado. La salida no
 * es «entonces AppKit se queda en Objective-C»: el runtime de Objective-C es una
 * biblioteca de C (`objc_getClass`, `sel_registerName`, `objc_msgSend`), asi que
 * cualquier `.cc` puede mandar cualquier mensaje a cualquier objeto de Cocoa. Es
 * exactamente lo que hace metal-cpp por dentro; aqui se escribe el mismo pegamento
 * para AppKit.
 *
 * LO QUE CAMBIA RESPECTO A UN `.mm`, Y HAY QUE TENERLO PRESENTE
 *
 * En un `.mm` el compilador comprueba que el selector exista y que los tipos cuadren.
 * Aqui NO. Un `objc_msgSend` mal casteado **compila igual y es comportamiento
 * indefinido**. Por eso cada fichero migrado con esto necesita verificacion en
 * ejecucion, no solo build verde.
 *
 * LAS TRES VARIANTES DE `objc_msgSend` EN x86_64
 *
 * No hay una sola funcion de envio: la ABI de System V decide como vuelve el
 * resultado y el runtime tiene una entrada distinta para cada caso.
 *
 *   - Agregado que NO cabe en registros (mas de 16 bytes) -> `objc_msgSend_stret`,
 *     que recibe un puntero oculto al hueco del resultado como PRIMER argumento.
 *     `NSRect` son 4 `double` = 32 bytes y cae aqui. `NSPoint`, `NSSize` y `NSRange`
 *     son 16 y NO caen: vuelven en registros con el envio normal. Equivocarse aqui
 *     no da error de compilacion, da basura.
 *   - `long double` -> `objc_msgSend_fpret`.
 *     OJO: metal-cpp manda por `fpret` TODO lo que sea coma flotante. En x86_64 eso
 *     es innecesario para `float` y `double` (vuelven en `xmm0` con el envio normal);
 *     la documentacion de Apple reserva `fpret` para `long double`. Aqui se sigue la
 *     regla documentada.
 *   - Todo lo demas -> `objc_msgSend`.
 *
 * El reparto lo hace `msg()` con `if constexpr`, asi que quien lo usa solo escribe el
 * tipo de retorno y se olvida.
 */

#pragma once

#ifndef __APPLE__
#  error Apple OSX only!
#endif

#include <objc/message.h>
#include <objc/objc.h>
#include <objc/runtime.h>

#include <cstddef>
#include <type_traits>

/* Las dos funciones del runtime que implementan `@autoreleasepool`. Son C puro y las
 * exporta libobjc, pero su declaracion vive en <objc/objc-internal.h>, que no esta en
 * el SDK publico; se declaran aqui. */
extern "C" void *objc_autoreleasePoolPush(void);
extern "C" void objc_autoreleasePoolPop(void *pool);

namespace ghost_objc {

/* -------------------------------------------------------------------------
 * Clases y selectores.
 *
 * `objc_getClass` y `sel_registerName` hacen una busqueda por cadena. En un camino
 * caliente conviene cachear el resultado en un `static` local, que en C++ se
 * inicializa una sola vez y de forma segura entre hilos.
 */

inline Class cls(const char *name)
{
  return objc_getClass(name);
}

inline SEL sel(const char *name)
{
  return sel_registerName(name);
}

/** Selector cacheado. Uso: `GHOST_SEL(bounds)` o `GHOST_SEL(setFrame:display:)`. */
#define GHOST_SEL(literal) \
  ([] { \
    static const SEL s_ = ::sel_registerName(#literal); \
    return s_; \
  }())

/** Clase cacheada. Uso: `GHOST_CLS(NSString)`. */
#define GHOST_CLS(literal) \
  ([]() -> id { \
    static const Class c_ = ::objc_getClass(#literal); \
    return (id)c_; \
  }())

/* -------------------------------------------------------------------------
 * Envio de mensajes.
 */

/** Cierto si el tipo de retorno debe ir por `objc_msgSend_stret` en x86_64. */
template<typename Ret> constexpr bool needs_stret()
{
  if constexpr (std::is_void<Ret>::value) {
    return false;
  }
  else if constexpr (std::is_class<Ret>::value || std::is_union<Ret>::value) {
    /* Regla de la ABI de System V para x86_64: un agregado de mas de 16 bytes vuelve
     * en memoria. Vale para las structs geometricas de Apple, que son escalares
     * (`NSRect` 32 B -> memoria; `NSPoint`, `NSSize`, `NSRange` 16 B -> registros). */
    return sizeof(Ret) > 16;
  }
  else {
    return false;
  }
}

/**
 * Manda `selector` a `receiver` con los argumentos dados.
 *
 * El tipo de retorno y los tipos de los argumentos hay que escribirlos exactos: el
 * compilador NO puede comprobarlos contra el selector real. Un entero donde el metodo
 * espera un `double`, o al reves, no da error y corrompe la pila de argumentos.
 *
 * Mandar un mensaje a `nil` devuelve 0 / nulo / struct a ceros, igual que en
 * Objective-C: es el propio runtime quien lo garantiza, no este codigo.
 */
template<typename Ret = id, typename... Args>
inline Ret msg(id receiver, SEL selector, Args... args)
{
#if defined(__x86_64__) || defined(__i386__)
  if constexpr (std::is_same<Ret, long double>::value) {
    using Proc = Ret (*)(id, SEL, Args...);
    return reinterpret_cast<Proc>(&objc_msgSend_fpret)(receiver, selector, args...);
  }
  else
#endif
#if !defined(__arm64__)
      if constexpr (needs_stret<Ret>()) {
    using Proc = void (*)(Ret *, id, SEL, Args...);
    Ret out;
    reinterpret_cast<Proc>(&objc_msgSend_stret)(&out, receiver, selector, args...);
    return out;
  }
  else
#endif
  {
    using Proc = Ret (*)(id, SEL, Args...);
    return reinterpret_cast<Proc>(&objc_msgSend)(receiver, selector, args...);
  }
}

/**
 * Manda `selector` a la implementacion de la SUPERCLASE. Hace falta en los delegados
 * fabricados en tiempo de ejecucion, donde un metodo tiene que llamar al de arriba.
 */
template<typename Ret = id, typename... Args>
inline Ret msg_super(id receiver, Class superclass, SEL selector, Args... args)
{
  objc_super sup{receiver, superclass};
#if !defined(__arm64__)
  if constexpr (needs_stret<Ret>()) {
    using Proc = void (*)(Ret *, objc_super *, SEL, Args...);
    Ret out;
    reinterpret_cast<Proc>(&objc_msgSendSuper_stret)(&out, &sup, selector, args...);
    return out;
  }
  else
#endif
  {
    using Proc = Ret (*)(objc_super *, SEL, Args...);
    return reinterpret_cast<Proc>(&objc_msgSendSuper)(&sup, selector, args...);
  }
}

/* -------------------------------------------------------------------------
 * Utilidades que salen en casi todos los ficheros.
 */

/**
 * Equivalente de `@autoreleasepool { ... }`.
 *
 * Usa `objc_autoreleasePoolPush` / `objc_autoreleasePoolPop`, que es EXACTAMENTE lo
 * que emite el compilador para `@autoreleasepool` (comprobado con `nm` sobre el `.mm`
 * original de GHOST_SystemPathsCocoa: sus simbolos indefinidos son esos dos). El
 * primer intento fue `[[NSAutoreleasePool alloc] init]` + `drain`, que da el mismo
 * resultado observable pero NO es el mismo mecanismo: crea un objeto, pasa por
 * `objc_msgSend` dos veces y en codigo con ARC alrededor puede comportarse distinto.
 * Con la pareja push/pop la traduccion es literal.
 *
 * Se drena al salir del ambito, tambien por excepcion, igual que la construccion del
 * compilador.
 */
class AutoreleasePool {
 public:
  AutoreleasePool() : pool_(objc_autoreleasePoolPush()) {}
  ~AutoreleasePool()
  {
    objc_autoreleasePoolPop(pool_);
  }
  AutoreleasePool(const AutoreleasePool &) = delete;
  AutoreleasePool &operator=(const AutoreleasePool &) = delete;

 private:
  void *pool_;
};

/**
 * Equivalente de `@"texto"`.
 *
 * TRAMPA HEREDADA DEL BACKEND DE METAL: un literal `@""` de Objective-C es un objeto
 * inmortal creado por el compilador, NO es nulo. Traducirlo a `nullptr` compila y
 * revienta en ejecucion donde el original devolvia 0 (en Objective-C un mensaje a
 * `nil` es legal). Por eso esto devuelve siempre un objeto de verdad, tambien para la
 * cadena vacia. La cadena resultante esta autoliberada: si tiene que sobrevivir al
 * ambito, hay que retenerla.
 */
inline id nsstring(const char *utf8)
{
  return msg<id>(GHOST_CLS(NSString), GHOST_SEL(stringWithUTF8String:), utf8 ? utf8 : "");
}

/** Equivalente de `[s UTF8String]`. Devuelve `nullptr` si `s` es nulo, igual que ObjC. */
inline const char *utf8_string(id nsstr)
{
  return nsstr ? msg<const char *>(nsstr, GHOST_SEL(UTF8String)) : nullptr;
}

/** `[obj retain]` / `[obj release]`, tolerantes con el nulo como en Objective-C. */
inline id retain(id obj)
{
  return obj ? msg<id>(obj, GHOST_SEL(retain)) : nullptr;
}

inline void release(id obj)
{
  if (obj) {
    msg<void>(obj, GHOST_SEL(release));
  }
}

inline id autorelease(id obj)
{
  return obj ? msg<id>(obj, GHOST_SEL(autorelease)) : nullptr;
}

/** `[[Clase alloc] init]`. */
inline id alloc_init(const char *class_name)
{
  Class c = objc_getClass(class_name);
  return c ? msg<id>(msg<id>((id)c, GHOST_SEL(alloc)), GHOST_SEL(init)) : nullptr;
}

}  // namespace ghost_objc
