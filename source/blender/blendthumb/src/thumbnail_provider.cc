/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup blendthumb
 *
 * C++ PURO. Era `thumbnail_provider.mm`, el ultimo Objective-C++ que el arbol COMPILA.
 *
 * Es la extension de miniaturas del Finder: un bundle aparte (`blender-thumbnailer.appex`)
 * que macOS carga cuando hay que dibujar el icono de un `.blend`.
 *
 * TIENE UNA DIFICULTAD QUE NO TENIA NINGUN OTRO FICHERO DE ESTA MIGRACION:
 * la clase `ThumbnailProvider` no la instancia nuestro codigo, la instancia EL SISTEMA.
 * El `Info.plist` del bundle dice `NSExtensionPrincipalClass = ThumbnailProvider`, y
 * macOS la busca por nombre **antes de que corra una sola linea nuestra**. Una clase
 * fabricada dentro de una funcion no existiria todavia. Por eso se registra desde un
 * constructor de carga (`__attribute__((constructor))`), que el enlazador dinamico
 * ejecuta al cargar el bundle, antes de que nadie pregunte por la clase.
 */

#include "BLI_fileops.h"
#include "BLI_filereader.h"
#include "BLI_utility_mixins.hh"
#include "blendthumb.hh"

#include <CoreGraphics/CoreGraphics.h>
#include <objc/message.h>
#include <objc/runtime.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

/**
 * This section intends to list the important steps for creating a thumbnail extension.
 * qlgenerator has been deprecated and removed in platforms we support. App extensions are the way
 * forward. But there's little guidance on how to do it outside Xcode.
 *
 * The process of thumbnail generation goes something like this:
 * 1. If an app is launched, or is registered with lsregister, its plugins also get registered.
 * 2. When a file thumbnail in Finder or QuickLook is requested, the system looks for a plugin
 *    that supports the file type UTI.
 * 3. The plugin is launched in a sand-boxed environment and should call the handler with a reply.
 *
 * # Plugin Info.plist
 * The Info.plist file should be properly configured with supported content type.
 *
 * # Codesigning
 * The plugin should be codesigned with entitlements at least for sandbox  and read-only/
 * read-write (for access to the given file). It's needed to even run the plugin locally.
 * com.apple.security.get-task-allow entitlement is required for debugging.
 *
 * # Registering the plugin
 * The plugin should be registered with lsregister. Either by calling lsregister or by launching
 * the parent app.
 * /System/Library/Frameworks/CoreServices.framework/Frameworks/LaunchServices.framework/Support/lsregister
 * \ -dump | grep blender-thumbnailer
 *
 * # Debugging
 * Since read-only entitlement is there, creating files to log is not possible. So NSLog and
 * viewing it in Console.app (after triggering a thumbnail) is the way to go. Interesting processes
 * are: qlmanage, quicklookd, kernel, blender-thumbnailer, secinitd,
 * com.apple.quicklook.ThumbnailsAgent
 *
 * # Triggering a thumbnail
 * - qlmanage -t -x /path/to/file.blend
 *
 * # External resources
 * https://developer.apple.com/library/archive/documentation/UserExperience/Conceptual/Quicklook_Programming_Guide/Introduction/Introduction.html#//apple_ref/doc/uid/TP40005020-CH1-SW1
 */

/* Simbolos de C del runtime de Objective-C y de Foundation. */
extern "C" {
void NSLog(id format, ...);
void *_NSConcreteStackBlock[32];
/* Clave de Foundation para el texto de un NSError. Es un `NSString *` global. */
extern id NSLocalizedDescriptionKey;
/* Lo que emite el compilador para `@autoreleasepool`. */
void *objc_autoreleasePoolPush(void);
void objc_autoreleasePoolPop(void *pool);
}

namespace {

using NSUInteger_ = unsigned long;

/* -------------------------------------------------------------------------
 * Envio de mensajes y bloques.
 *
 * Este fichero es un bundle aparte y no enlaza con `bf_intern_ghost`, asi que no puede
 * usar `GHOST_ObjCRuntime.hh`. Se repiten aqui las tres piezas minimas que necesita, y
 * solo esas: envio normal, envio con retorno de agregado grande (`stret`), y bloque.
 */

template<typename Ret> constexpr bool needs_stret()
{
  if constexpr (std::is_void<Ret>::value) {
    return false;
  }
  else if constexpr (std::is_class<Ret>::value || std::is_union<Ret>::value) {
    /* ABI de System V en x86_64: un agregado de mas de 16 bytes vuelve en memoria.
     * `CGSize` son 16 y NO cae; `CGRect` son 32 y SI. */
    return sizeof(Ret) > 16;
  }
  else {
    return false;
  }
}

template<typename Ret = id, typename... Args>
inline Ret msg(id receiver, SEL selector, Args... args)
{
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

#define SEL_(literal) \
  ([] { \
    static const SEL s_ = ::sel_registerName(#literal); \
    return s_; \
  }())
#define CLS_(literal) \
  ([]() -> id { \
    static const Class c_ = ::objc_getClass(#literal); \
    return (id)c_; \
  }())

id ns(const char *utf8)
{
  return msg<id>(CLS_(NSString), SEL_(stringWithUTF8String:), utf8 ? utf8 : "");
}

/* Disposicion de un bloque de Clang (Block ABI). Se construye a mano para no depender de
 * `-fblocks`, que es una extension y no C++ estandar. Una sola captura, un puntero POD:
 * asi no hacen falta las ayudas de copia y destruccion, y `_Block_copy` —que es lo que
 * hace QuickLook al guardarlo— puede limitarse a copiar la memoria. */
struct BlockDescriptor {
  unsigned long reserved;
  unsigned long size;
};
struct BlockLiteral {
  void *isa;
  int flags;
  int reserved;
  void (*invoke)(void *, ...);
  BlockDescriptor *descriptor;
  void *context;
};
BlockDescriptor g_block_descriptor = {0, sizeof(BlockLiteral)};

BlockLiteral make_block(void (*invoke)(void *, ...), void *context)
{
  BlockLiteral b{};
  b.isa = (void *)_NSConcreteStackBlock;
  b.flags = 0;
  b.reserved = 0;
  b.invoke = invoke;
  b.descriptor = &g_block_descriptor;
  b.context = context;
  return b;
}

/* -------------------------------------------------------------------------
 * El cuerpo, traducido linea a linea del original.
 */

class FileDescriptorRAII : blender::NonCopyable, blender::NonMovable {
 private:
  int src_fd = -1;

 public:
  explicit FileDescriptorRAII(const char *file_path)
  {
    src_fd = BLI_open(file_path, O_BINARY | O_RDONLY, 0);
  }

  ~FileDescriptorRAII()
  {
    if (good()) {
      int ok = close(src_fd);
      if (!ok) {
        NSLog(ns("Blender Thumbnailer Error: Failed to close the blend file."));
      }
    }
  }

  bool good()
  {
    return src_fd > 0;
  }

  int get()
  {
    return src_fd;
  }
};

id create_nserror_from_string(id errorStr)
{
  NSLog(ns("Blender Thumbnailer Error: %@"), errorStr);
  /* `@{ NSLocalizedDescriptionKey : errorStr }` es
   * `[NSDictionary dictionaryWithObject:errorStr forKey:NSLocalizedDescriptionKey]`. */
  id userInfo = msg<id>(CLS_(NSDictionary),
                        SEL_(dictionaryWithObject:forKey:),
                        errorStr,
                        NSLocalizedDescriptionKey);
  return msg<id>(CLS_(NSError),
                 SEL_(errorWithDomain:code:userInfo:),
                 ns("org.blenderfoundation.blender.thumbnailer"),
                 (long)-1,
                 userInfo);
}

id generate_nsimage_for_file(const char *src_blend_path, id error)
{
  /* OJO, CONDUCTA DEL ORIGINAL QUE SE CONSERVA: `error` se recibe POR VALOR, asi que
   * asignarle algo aqui NO llega a quien llamo. El original hacia lo mismo, de modo que
   * el manejador siempre recibe un error nulo. Se deja igual a proposito: arreglarlo
   * seria un cambio de conducta y no le toca a una migracion de lenguaje. */
  /* Open source file `src_blend`. */
  FileDescriptorRAII src_file_fd = FileDescriptorRAII(src_blend_path);
  if (!src_file_fd.good()) {
    error = create_nserror_from_string(ns("Failed to open blend"));
    return nullptr;
  }

  FileReader *file_content = BLI_filereader_new_file(src_file_fd.get());
  if (file_content == nullptr) {
    error = create_nserror_from_string(ns("Failed to read from blend"));
    return nullptr;
  }

  /* Extract thumbnail from file. */
  Thumbnail thumb;
  eThumbStatus err = blendthumb_create_thumb_from_file(file_content, &thumb);
  if (err != BT_OK) {
    error = create_nserror_from_string(ns("Failed to create thumbnail from file"));
    return nullptr;
  }

  std::optional<blender::Vector<uint8_t>> png_buf_opt = blendthumb_create_png_data_from_thumb(
      &thumb);
  if (!png_buf_opt) {
    error = create_nserror_from_string(ns("Failed to create png data from thumbnail"));
    return nullptr;
  }

  id ns_data = msg<id>(CLS_(NSData),
                       SEL_(dataWithBytes:length:),
                       (const void *)png_buf_opt->data(),
                       (NSUInteger_)png_buf_opt->size());
  id ns_image = msg<id>(
      msg<id>(CLS_(NSImage), SEL_(alloc)), SEL_(initWithData:), ns_data);
  return ns_image;
}

/* Lo que el bloque de dibujo necesita. El bloque solo captura UN puntero, asi que el
 * estado va aqui y se reserva en el monticulo: el bloque se ejecuta DESPUES de que
 * termine la funcion que lo creo. */
struct DrawContext {
  id image;
  CGRect rect;
};

/** Cuerpo del bloque `BOOL (^)(void)` que dibuja la miniatura. */
signed char draw_block_invoke(void *blk)
{
  DrawContext *ctx = (DrawContext *)((BlockLiteral *)blk)->context;
  msg<void>(ctx->image, SEL_(drawInRect:), ctx->rect);
  /* Release the image that was strongly captured by this block.
   * (En el original lo retenia el compilador al copiar el bloque; aqui la imagen se
   * posee desde `initWithData:` y se suelta aqui, que es el mismo momento.) */
  msg<void>(ctx->image, SEL_(release));
  delete ctx;
  return 1; /* YES */
}

/** Implementacion de `provideThumbnailForFileRequest:completionHandler:`. */
void imp_provideThumbnail(id /*self*/, SEL, id request, void *handler)
{
  /* `handler` es un BLOQUE que hay que LLAMAR: se invoca por su puntero `invoke`,
   * pasandole el propio bloque como primer argumento. */
  auto call_handler = [handler](id reply, id error) {
    BlockLiteral *b = (BlockLiteral *)handler;
    ((void (*)(void *, id, id))b->invoke)(handler, reply, error);
  };

  id fileURL = msg<id>(request, SEL_(fileURL));
  id path = msg<id>(fileURL, SEL_(path));
  NSLog(ns("Generating thumbnail for %@"), path);

  {
    void *pool = objc_autoreleasePoolPush();
    id error = nullptr;
    id image = generate_nsimage_for_file(
        msg<const char *>(path, SEL_(fileSystemRepresentation)), error);
    const CGSize image_size = image ? msg<CGSize>(image, SEL_(size)) : CGSizeMake(0, 0);
    if (image == nullptr || image_size.width <= 0 || image_size.height <= 0) {
      call_handler(nullptr, error);
      objc_autoreleasePoolPop(pool);
      return;
    }

    const CGSize maximumSize = msg<CGSize>(request, SEL_(maximumSize));
    const CGFloat width_ratio = maximumSize.width / image_size.width;
    const CGFloat height_ratio = maximumSize.height / image_size.height;
    const CGFloat scale_factor = width_ratio < height_ratio ? width_ratio : height_ratio;

    const CGSize context_size = CGSizeMake(image_size.width * scale_factor,
                                           image_size.height * scale_factor);

    const CGRect context_rect = CGRectMake(0, 0, context_size.width, context_size.height);

    DrawContext *ctx = new DrawContext{image, context_rect};
    BlockLiteral drawing_block = make_block((void (*)(void *, ...))draw_block_invoke, ctx);

    id thumbnailReply = msg<id>(CLS_(QLThumbnailReply),
                                SEL_(replyWithContextSize:currentContextDrawingBlock:),
                                context_size,
                                &drawing_block);

    /* Return the thumbnail reply. */
    call_handler(thumbnailReply, nullptr);
    objc_autoreleasePoolPop(pool);
  }
  NSLog(ns("Thumbnail generation succcessfully completed"));
}

/* -------------------------------------------------------------------------
 * Registro de la clase, EN LA CARGA DEL BUNDLE.
 *
 * `NSExtensionPrincipalClass = ThumbnailProvider` en el Info.plist: macOS busca la clase
 * por nombre en cuanto carga el bundle. Registrarla dentro de una funcion llegaria tarde.
 * Un constructor de carga corre al hacer `dlopen`, antes de que nadie pregunte.
 */
__attribute__((constructor)) void register_thumbnail_provider_class()
{
  Class super = objc_getClass("QLThumbnailProvider");
  if (!super) {
    NSLog(ns("Blender Thumbnailer Error: QLThumbnailProvider no esta cargada"));
    return;
  }
  Class cls = objc_allocateClassPair(super, "ThumbnailProvider", 0);
  if (!cls) {
    /* Ya existe: no deberia pasar, pero no se puede fallar en la carga del bundle. */
    return;
  }
  /* La codificacion de tipo la da la superclase, no se escribe a mano:
   * `v32@0:8@16@?24` (void; self; _cmd; un objeto; un BLOQUE). */
  const SEL sel = sel_registerName("provideThumbnailForFileRequest:completionHandler:");
  Method m = class_getInstanceMethod(super, sel);
  if (!m) {
    NSLog(ns("Blender Thumbnailer Error: el runtime no conoce provideThumbnail..."));
    return;
  }
  class_addMethod(cls, sel, (IMP)imp_provideThumbnail, method_getTypeEncoding(m));
  objc_registerClassPair(cls);
}

}  // namespace
