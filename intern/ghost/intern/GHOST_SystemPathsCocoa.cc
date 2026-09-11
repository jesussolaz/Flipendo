/* SPDX-FileCopyrightText: 2010 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 *
 * C++ PURO. Era `GHOST_SystemPathsCocoa.mm`. Los mensajes de Objective-C van por el
 * runtime de C (ver `GHOST_ObjCRuntime.hh`); no hay compilador de Objective-C de por
 * medio y por tanto NADIE comprueba los selectores: cualquier error aqui aparece en
 * ejecucion, no al compilar. Las rutas que devuelve este fichero las usa todo el
 * arranque (configuracion de usuario, scripts, datafiles), asi que un fallo se nota
 * inmediatamente.
 */

#include "GHOST_SystemPathsCocoa.hh"
#include "GHOST_Debug.hh"
#include "GHOST_ObjCRuntime.hh"

#include <cstdio>
#include <cstring>

using ghost_objc::AutoreleasePool;
using ghost_objc::msg;

/* -------------------------------------------------------------------------
 * Constantes de Foundation.
 *
 * En un `.mm` las traia `<Foundation/Foundation.h>`. Desde C++ hay que escribirlas, y
 * esto es EXACTAMENTE el sitio donde un error no da ningun aviso: pedir el directorio
 * 13 en vez del 14 compila igual y devuelve la carpeta equivocada. Los valores estan
 * copiados de `Foundation.framework/Headers/NSPathUtilities.h` y
 * `NSString.h` del SDK 26.5 y comprobados uno a uno con una sonda `.mm` que hace
 * `static_assert` contra los simbolos reales del SDK (ver informe GHOST-1).
 */
namespace {

using NSUInteger_ = unsigned long;

/* NSSearchPathDirectory */
constexpr NSUInteger_ kNSDocumentDirectory = 9;
constexpr NSUInteger_ kNSDesktopDirectory = 12;
constexpr NSUInteger_ kNSCachesDirectory = 13;
constexpr NSUInteger_ kNSApplicationSupportDirectory = 14;
constexpr NSUInteger_ kNSDownloadsDirectory = 15;
constexpr NSUInteger_ kNSMoviesDirectory = 17;
constexpr NSUInteger_ kNSMusicDirectory = 18;
constexpr NSUInteger_ kNSPicturesDirectory = 19;

/* NSSearchPathDomainMask */
constexpr NSUInteger_ kNSUserDomainMask = 1;
constexpr NSUInteger_ kNSLocalDomainMask = 2;

/* NSStringEncoding */
constexpr NSUInteger_ kNSASCIIStringEncoding = 1;

}  // namespace

/* `NSSearchPathForDirectoriesInDomains` es una FUNCION DE C de Foundation, no un
 * metodo: se llama directamente, sin `objc_msgSend`. Se declara aqui con el tipo de
 * retorno degradado a `id` porque `NSArray *` no existe en C++ puro; es el mismo
 * puntero. `BOOL` en macOS x86_64 es `signed char`. */
extern "C" id NSSearchPathForDirectoriesInDomains(NSUInteger_ directory,
                                                  NSUInteger_ domain_mask,
                                                  signed char expand_tilde);

/* --------------------------------------------------------------------
 * Base directories retrieval.
 */

static const char *GetApplicationSupportDir(const char *versionstr,
                                            const NSUInteger_ mask,
                                            char *tempPath,
                                            const std::size_t len_tempPath)
{
  {
    AutoreleasePool pool;

    id paths = NSSearchPathForDirectoriesInDomains(kNSApplicationSupportDirectory, mask, 1);

    if (msg<NSUInteger_>(paths, GHOST_SEL(count)) == 0) {
      return nullptr;
    }
    id basePath = msg<id>(paths, GHOST_SEL(objectAtIndex:), NSUInteger_(0));

    snprintf(tempPath,
             len_tempPath,
             "%s/UPBGE/%s",
             msg<const char *>(basePath, GHOST_SEL(cStringUsingEncoding:), kNSASCIIStringEncoding),
             versionstr);
  }
  return tempPath;
}

const char *GHOST_SystemPathsCocoa::getSystemDir(int /* version */, const char *versionstr) const
{
  static char tempPath[512] = "";
  return GetApplicationSupportDir(versionstr, kNSLocalDomainMask, tempPath, sizeof(tempPath));
}

const char *GHOST_SystemPathsCocoa::getUserDir(int /* version */, const char *versionstr) const
{
  static char tempPath[512] = "";
  return GetApplicationSupportDir(versionstr, kNSUserDomainMask, tempPath, sizeof(tempPath));
}

const char *GHOST_SystemPathsCocoa::getUserSpecialDir(GHOST_TUserSpecialDirTypes type) const
{
  static char tempPath[512] = "";
  {
    AutoreleasePool pool;
    NSUInteger_ ns_directory;

    switch (type) {
      case GHOST_kUserSpecialDirDesktop:
        ns_directory = kNSDesktopDirectory;
        break;
      case GHOST_kUserSpecialDirDocuments:
        ns_directory = kNSDocumentDirectory;
        break;
      case GHOST_kUserSpecialDirDownloads:
        ns_directory = kNSDownloadsDirectory;
        break;
      case GHOST_kUserSpecialDirMusic:
        ns_directory = kNSMusicDirectory;
        break;
      case GHOST_kUserSpecialDirPictures:
        ns_directory = kNSPicturesDirectory;
        break;
      case GHOST_kUserSpecialDirVideos:
        ns_directory = kNSMoviesDirectory;
        break;
      case GHOST_kUserSpecialDirCaches:
        ns_directory = kNSCachesDirectory;
        break;
      default:
        GHOST_ASSERT(
            false,
            "GHOST_SystemPathsCocoa::getUserSpecialDir(): Invalid enum value for type parameter");
        return nullptr;
    }

    id paths = NSSearchPathForDirectoriesInDomains(ns_directory, kNSUserDomainMask, 1);
    if (msg<NSUInteger_>(paths, GHOST_SEL(count)) == 0) {
      return nullptr;
    }
    id basePath = msg<id>(paths, GHOST_SEL(objectAtIndex:), NSUInteger_(0));

    const char *basePath_cstr = msg<const char *>(
        basePath, GHOST_SEL(cStringUsingEncoding:), kNSASCIIStringEncoding);
    int basePath_len = strlen(basePath_cstr);

    /* Era `MIN(...)`, macro que llegaba por <Foundation/Foundation.h> -> <sys/param.h>.
     * Al quitar Foundation ya no existe; se escribe el recorte a mano con el mismo
     * resultado y sin comparar un `int` con un `size_t`. */
    if (basePath_len > int(sizeof(tempPath) - 1)) {
      basePath_len = int(sizeof(tempPath) - 1);
    }
    memcpy(tempPath, basePath_cstr, basePath_len);
    tempPath[basePath_len] = '\0';
  }
  return tempPath;
}

const char *GHOST_SystemPathsCocoa::getBinaryDir() const
{
  static char tempPath[512] = "";

  {
    AutoreleasePool pool;
    id basePath = msg<id>(msg<id>(GHOST_CLS(NSBundle), GHOST_SEL(mainBundle)),
                          GHOST_SEL(bundlePath));

    if (basePath == nullptr) {
      return nullptr;
    }

    const char *basePath_cstr = msg<const char *>(
        basePath, GHOST_SEL(cStringUsingEncoding:), kNSASCIIStringEncoding);
    int basePath_len = strlen(basePath_cstr);

    if (basePath_len > int(sizeof(tempPath) - 1)) {
      basePath_len = int(sizeof(tempPath) - 1);
    }
    memcpy(tempPath, basePath_cstr, basePath_len);
    tempPath[basePath_len] = '\0';
  }
  return tempPath;
}

void GHOST_SystemPathsCocoa::addToSystemRecentFiles(const char *filepath) const
{
  AutoreleasePool pool;
  id file_url = msg<id>(
      GHOST_CLS(NSURL), GHOST_SEL(fileURLWithPath:), ghost_objc::nsstring(filepath));
  msg<void>(msg<id>(GHOST_CLS(NSDocumentController), GHOST_SEL(sharedDocumentController)),
            GHOST_SEL(noteNewRecentDocumentURL:),
            file_url);
}
