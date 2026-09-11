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
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

/* Las dos funciones del runtime que implementan `@autoreleasepool`. Son C puro y las
 * exporta libobjc, pero su declaracion vive en <objc/objc-internal.h>, que no esta en
 * el SDK publico; se declaran aqui. */
extern "C" void *objc_autoreleasePoolPush(void);
extern "C" void objc_autoreleasePoolPop(void *pool);

/* Simbolos del runtime de bloques (libSystem). Son C y se pueden declarar. */
extern "C" void *_NSConcreteStackBlock[32];

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

/* -------------------------------------------------------------------------
 * Bloques de Clang, fabricados desde C++ ESTANDAR.
 *
 * Hay APIs de Cocoa que solo aceptan un bloque (`^`) y no tienen variante con selector
 * ni con puntero a funcion. En `intern/ghost` hay exactamente UNA:
 * `NSColorSampler showSamplerWithSelectionHandler:`, el cuentagotas que toma un color
 * de la pantalla.
 *
 * Un bloque no es magia del compilador: es una struct con una disposicion publicada y
 * estable (el Block ABI de Clang). Se puede construir a mano, y asi no hace falta
 * activar `-fblocks`, que es una extension y no C++ estandar.
 *
 * MEDIDO antes de usarlo: se construye, se invoca en la pila, **sobrevive a
 * `_Block_copy` al monticulo** —que es lo que hace toda API que guarde el bloque— y
 * Objective-C lo ejecuta correctamente cuando se le pasa a un metodo de verdad.
 *
 * LIMITACION A PROPOSITO: solo captura UN PUNTERO. Con una sola captura POD no hacen
 * falta las ayudas de copia y destruccion (`BLOCK_HAS_COPY_DISPOSE`), que es donde
 * estan las complicaciones de verdad. Si algun dia hiciera falta capturar un objeto de
 * Objective-C (que hay que retener al copiar el bloque), esto NO vale tal cual: lo
 * correcto es pasar un puntero a una struct, como se hace en `getPixelAtCursor`.
 */
class Block {
 public:
  /** Firma del cuerpo: el primer parametro es el propio bloque. */
  using Invoke = void (*)(void *, ...);

  Block(Invoke invoke, void *context)
  {
    literal_.isa = (void *)_NSConcreteStackBlock;
    /* Sin BLOCK_HAS_COPY_DISPOSE: la unica captura es un puntero POD y la copia al
     * monticulo puede ser una copia de memoria tal cual. */
    literal_.flags = 0;
    literal_.reserved = 0;
    literal_.invoke = invoke;
    literal_.descriptor = &descriptor_;
    literal_.context = context;
  }

  /** El bloque a pasar a Cocoa. Vive mientras viva este objeto (o su copia). */
  void *get()
  {
    return &literal_;
  }

  /** Dentro del cuerpo: recupera la captura a partir del bloque recibido. */
  static void *context_of(void *block)
  {
    return static_cast<Literal *>(block)->context;
  }

 private:
  struct Descriptor {
    unsigned long reserved;
    unsigned long size;
  };
  struct Literal {
    void *isa;
    int flags;
    int reserved;
    Invoke invoke;
    Descriptor *descriptor;
    void *context;
  };

  Literal literal_{};
  Descriptor descriptor_{0, sizeof(Literal)};
};

/* -------------------------------------------------------------------------
 * Fabricar clases de Objective-C en tiempo de ejecucion.
 *
 * Hace falta donde GHOST deja de MANDAR mensajes y pasa a RECIBIRLOS: los delegados de
 * ventana y de aplicacion, y las subclases de NSView y NSWindow.
 *
 * EL PROBLEMA QUE RESUELVE, Y QUE NO ES EL OBVIO
 *
 * `class_addMethod` necesita una CODIFICACION DE TIPO: una cadena que describe el tipo
 * de retorno y los de todos los parametros, empezando por los dos ocultos `self` (@) y
 * `_cmd` (:). Escribirlas a mano para las 89 de `intern/ghost` seria la parte mas
 * peligrosa de toda la migracion: una mal puesta NO da error de compilacion ni de
 * registro, da comportamiento indefinido dentro del manejador de eventos.
 *
 * LA SALIDA: NO ESCRIBIRLAS. EL RUNTIME YA LAS SABE.
 *
 *   - Si el metodo SOBREESCRIBE uno de la superclase (`drawRect:`, `keyDown:`,
 *     `canBecomeKeyWindow`...), la codificacion oficial esta en
 *     `class_getInstanceMethod(superclase, sel)` -> `method_getTypeEncoding`.
 *   - Si el metodo es de un PROTOCOLO (`NSTextInputClient`, `NSWindowDelegate`,
 *     `NSDraggingDestination`, `NSApplicationDelegate`), esta en
 *     `protocol_getMethodDescription`.
 *
 * Comprobado contra el SDK 26.5: el runtime devuelve, por ejemplo,
 *
 *     drawRect:                                v48@0:8{CGRect={CGPoint=dd}{CGSize=dd}}16
 *     firstRectForCharacterRange:actualRange:  {CGRect=...}40@0:8{_NSRange=QQ}16^{_NSRange=QQ}32
 *     draggingEntered:                         Q24@0:8@16
 *
 * Asi que `method()` las pide y **aborta con un mensaje claro si nadie las conoce**.
 * Eso convierte un selector MAL ESCRITO —el fallo silencioso mas probable de esta
 * migracion— en un fallo ruidoso y temprano, en el registro de la clase, antes de que
 * se dibuje un solo pixel. Es la razon de que esta clase exista.
 *
 * Solo se escribe una codificacion a mano en los metodos PROPIOS, que el runtime no
 * puede conocer porque no existen en ningun sitio mas (`initWithSystemCocoa:...`), y
 * esos son pocos y de firma trivial.
 */
class ClassBuilder {
 public:
  ClassBuilder(const char *name, const char *superclass_name) : name_(name)
  {
    Class super = objc_getClass(superclass_name);
    if (!super) {
      fail("no existe la superclase", superclass_name);
    }
    cls_ = objc_allocateClassPair(super, name, 0);
    if (!cls_) {
      /* Ya registrada: pasa si se construye dos veces. Es un error de uso. */
      fail("objc_allocateClassPair fallo (¿nombre repetido?)", name);
    }
    super_ = super;
  }

  /** Declara conformidad con un protocolo. LLAMAR ANTES que a `method()`: de ahi se
   *  sacan las codificaciones de los metodos del protocolo. */
  ClassBuilder &protocol(const char *protocol_name)
  {
    Protocol *p = objc_getProtocol(protocol_name);
    if (!p) {
      fail("no existe el protocolo", protocol_name);
    }
    class_addProtocol(cls_, p);
    if (n_protocols_ >= kMaxProtocols) {
      fail("demasiados protocolos", protocol_name);
    }
    protocols_[n_protocols_++] = p;
    return *this;
  }

  /** Variable de instancia. Llamar antes de `finish()`. */
  ClassBuilder &ivar(const char *ivar_name, size_t size, uint8_t alignment, const char *types)
  {
    if (!class_addIvar(cls_, ivar_name, size, alignment, types)) {
      fail("class_addIvar fallo", ivar_name);
    }
    return *this;
  }

  /** Metodo con la codificacion PEDIDA AL RUNTIME. Aborta si no se encuentra. */
  ClassBuilder &method(const char *selector_name, IMP imp)
  {
    const char *types = lookup_types(selector_name);
    if (!types) {
      fail(
          "el runtime no conoce este selector: ni la superclase ni los protocolos "
          "declarados lo tienen. Suele ser una errata en el nombre, o falta declarar "
          "el protocolo ANTES de anadir el metodo",
          selector_name);
    }
    add(selector_name, imp, types);
    return *this;
  }

  /** Metodo PROPIO: el runtime no puede conocerlo, la codificacion va a mano. */
  ClassBuilder &method(const char *selector_name, IMP imp, const char *types)
  {
    /* Si ademas resulta que el runtime SI lo conoce, se comprueba que coincidan: asi
     * una codificacion escrita a mano que no cuadre tampoco pasa desapercibida. */
    if (const char *known = lookup_types(selector_name)) {
      if (!same_encoding(known, types)) {
        fail_two("la codificacion escrita a mano no coincide con la del runtime",
                 selector_name,
                 types,
                 known);
      }
    }
    add(selector_name, imp, types);
    return *this;
  }

  Class finish()
  {
    objc_registerClassPair(cls_);
    return cls_;
  }

  Class get() const
  {
    return cls_;
  }

 private:
  static constexpr int kMaxProtocols = 8;

  const char *lookup_types(const char *selector_name) const
  {
    const SEL s = sel_registerName(selector_name);
    /* 1. La superclase (metodo sobreescrito). */
    if (Method m = class_getInstanceMethod(super_, s)) {
      if (const char *t = method_getTypeEncoding(m)) {
        return t;
      }
    }
    /* 2. Los protocolos declarados, en sus cuatro combinaciones de
     *    (requerido, opcional) x (de instancia, de clase). */
    for (int i = 0; i < n_protocols_; i++) {
      for (int req = 1; req >= 0; req--) {
        struct objc_method_description d = protocol_getMethodDescription(
            protocols_[i], s, req != 0, true);
        if (d.types) {
          return d.types;
        }
      }
    }
    return nullptr;
  }

  void add(const char *selector_name, IMP imp, const char *types)
  {
    if (!class_addMethod(cls_, sel_registerName(selector_name), imp, types)) {
      fail("class_addMethod fallo", selector_name);
    }
  }

  /** Compara dos codificaciones ignorando los numeros de desplazamiento, que el
   *  runtime anade (`v24@0:8@16`) y que a mano no se suelen escribir (`v@:@`). */
  static bool same_encoding(const char *a, const char *b)
  {
    while (*a || *b) {
      while (*a >= '0' && *a <= '9') {
        a++;
      }
      while (*b >= '0' && *b <= '9') {
        b++;
      }
      if (*a != *b) {
        return false;
      }
      if (*a) {
        a++;
        b++;
      }
    }
    return true;
  }

  [[noreturn]] void fail(const char *what, const char *detail) const
  {
    fprintf(stderr, "GHOST: al fabricar la clase '%s': %s -> '%s'\n", name_, what, detail);
    abort();
  }

  [[noreturn]] void fail_two(
      const char *what, const char *detail, const char *mine, const char *theirs) const
  {
    fprintf(stderr,
            "GHOST: al fabricar la clase '%s': %s -> '%s'\n  escrita:  %s\n  runtime:  %s\n",
            name_,
            what,
            detail,
            mine,
            theirs);
    abort();
  }

  const char *name_;
  Class cls_ = nullptr;
  Class super_ = nullptr;
  Protocol *protocols_[kMaxProtocols] = {};
  int n_protocols_ = 0;
};

/** Lee una variable de instancia por nombre. */
template<typename T> inline T ivar_get(id obj, const char *name)
{
  T value{};
  Ivar iv = class_getInstanceVariable(object_getClass(obj), name);
  if (iv) {
    memcpy(&value, (const char *)obj + ivar_getOffset(iv), sizeof(T));
  }
  return value;
}

/** Escribe una variable de instancia por nombre. */
template<typename T> inline void ivar_set(id obj, const char *name, T value)
{
  Ivar iv = class_getInstanceVariable(object_getClass(obj), name);
  if (iv) {
    memcpy((char *)obj + ivar_getOffset(iv), &value, sizeof(T));
  }
}

/** `[[Clase alloc] init]`. */
inline id alloc_init(const char *class_name)
{
  Class c = objc_getClass(class_name);
  return c ? msg<id>(msg<id>((id)c, GHOST_SEL(alloc)), GHOST_SEL(init)) : nullptr;
}

}  // namespace ghost_objc
