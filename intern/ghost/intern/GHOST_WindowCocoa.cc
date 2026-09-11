/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 *
 * C++ PURO. Era `GHOST_WindowCocoa.mm`.
 *
 * Contiene las dos clases de Objective-C que Cocoa LLAMA (el delegado de la ventana y
 * la subclase de NSWindow que recibe el arrastrar-y-soltar) y toda la implementacion de
 * `GHOST_WindowCocoa`. Las clases se fabrican con `ghost_objc::ClassBuilder`, que pide
 * al runtime la codificacion de tipo de cada metodo; un selector mal escrito aborta el
 * registro con un mensaje claro en vez de corromper la pila de argumentos.
 *
 * Los NOMBRES de las clases (`BlenderWindow`, `CocoaMetalView`, `CocoaOpenGLView`) son
 * obligatorios: `GHOST_WindowCocoa.hh` declara miembros con esos tipos.
 */

#include "GHOST_WindowCocoa.hh"
#include "GHOST_ContextNone.hh"
#include "GHOST_Debug.hh"
#include "GHOST_ObjCRuntime.hh"
#include "GHOST_SystemCocoa.hh"
#include "GHOST_WindowViewCocoa.hh"

#ifdef WITH_METAL_BACKEND
#  include "GHOST_ContextCGL.hh"
#endif

#ifdef WITH_VULKAN_BACKEND
#  include "GHOST_ContextVK.hh"
#endif

#include <CoreGraphics/CoreGraphics.h>

#include <sys/sysctl.h>

using ghost_objc::msg;

/* -------------------------------------------------------------------------
 * Simbolos globales de AppKit, Foundation y Metal.
 *
 * Todos son C: se declaran y el enlazador los resuelve. Se usan los SIMBOLOS de verdad
 * en vez de escribir las cadenas a mano («NSPasteboardTypeString»...) para que un
 * cambio de nombre lo diga el enlazador y no se convierta en un fallo silencioso.
 */
extern "C" {
/** La instancia de NSApplication. AppKit la exporta como variable global. */
extern id NSApp;
extern id NSPasteboardTypeTIFF;
extern id NSPasteboardTypeString;
extern id NSFilenamesPboardType;
extern id NSAppearanceNameVibrantLight;
extern id NSAppearanceNameVibrantDark;
extern id NSDeviceWhiteColorSpace;
/** Funciones de C de Foundation y Metal (no son metodos). */
CGRect NSInsetRect(CGRect aRect, CGFloat dX, CGFloat dY);
id MTLCreateSystemDefaultDevice(void);
}

namespace {

using NSUInteger_ = unsigned long;
using NSInteger_ = long;

/* Constantes de AppKit y Metal. Las 22 estan comprobadas con `static_assert` contra el
 * SDK 26.5 en la sonda de constantes (ver informe GHOST-1). Un valor equivocado aqui no
 * da ningun aviso: da una ventana sin botones, un formato de pixel distinto o un
 * arrastre que no se acepta. */
constexpr NSUInteger_ kNSWindowStyleMaskTitled = 1 << 0;
constexpr NSUInteger_ kNSWindowStyleMaskClosable = 1 << 1;
constexpr NSUInteger_ kNSWindowStyleMaskMiniaturizable = 1 << 2;
constexpr NSUInteger_ kNSWindowStyleMaskResizable = 1 << 3;
constexpr NSUInteger_ kNSWindowStyleMaskFullScreen = 1 << 14;
constexpr NSUInteger_ kNSBackingStoreBuffered = 2;
constexpr NSUInteger_ kNSWindowCollectionBehaviorFullScreenPrimary = 1 << 7;
constexpr NSUInteger_ kNSWindowCollectionBehaviorFullScreenAuxiliary = 1 << 8;
constexpr NSInteger_ kNSWindowAbove = 1;
constexpr NSUInteger_ kNSDragOperationNone = 0;
constexpr NSUInteger_ kNSDragOperationCopy = 1;
constexpr NSUInteger_ kNSCompositingOperationSourceOver = 2;
constexpr NSUInteger_ kNSTouchTypeMaskDirect = 1 << 0;
constexpr NSUInteger_ kNSTouchTypeMaskIndirect = 1 << 1;
constexpr NSUInteger_ kNSUTF8StringEncoding = 4;
constexpr NSUInteger_ kMTLPixelFormatRGBA16Float = 115;

/* -------------------------------------------------------------------------
 * Estado de las clases fabricadas.
 *
 * El original guardaba `m_systemCocoa` / `m_windowCocoa` como variables de instancia
 * sintetizadas por `@synthesize`. Aqui son variables de instancia explicitas, con los
 * mismos nombres, leidas y escritas desde C++.
 */
const char *const kIvarSystem = "m_systemCocoa";
const char *const kIvarWindow = "m_windowCocoa";
const char *const kIvarDraggedType = "m_draggedObjectType";

GHOST_SystemCocoa *system_of(id self)
{
  return ghost_objc::ivar_get<GHOST_SystemCocoa *>(self, kIvarSystem);
}

GHOST_WindowCocoa *window_of(id self)
{
  return ghost_objc::ivar_get<GHOST_WindowCocoa *>(self, kIvarWindow);
}

/* -------------------------------------------------------------------------
 * Blender window delegate object.
 */

void imp_windowDidBecomeKey(id self, SEL, id /*notification*/)
{
  GHOST_WindowCocoa *win = window_of(self);
  system_of(self)->handleWindowEvent(GHOST_kEventWindowActivate, win);
  /* Workaround for broken app-switching when combining Command-Tab and mission-control. */
  msg<void>(reinterpret_cast<id>(win->getOSWindow()), GHOST_SEL(orderFrontRegardless));
}

void imp_windowDidResignKey(id self, SEL, id)
{
  system_of(self)->handleWindowEvent(GHOST_kEventWindowDeactivate, window_of(self));
}

void imp_windowDidExpose(id self, SEL, id)
{
  system_of(self)->handleWindowEvent(GHOST_kEventWindowUpdate, window_of(self));
}

void imp_windowDidMove(id self, SEL, id)
{
  system_of(self)->handleWindowEvent(GHOST_kEventWindowMove, window_of(self));
}

void imp_windowWillMove(id self, SEL, id)
{
  system_of(self)->handleWindowEvent(GHOST_kEventWindowMove, window_of(self));
}

void imp_windowWillEnterFullScreen(id self, SEL, id)
{
  window_of(self)->setImmediateDraw(true);
}

void imp_windowDidEnterFullScreen(id self, SEL, id)
{
  /* macOS does not send a window resize event when switching between zoomed
   * and full-screen, when automatic show/hide of dock and menu bar are enabled.
   * Send our own to prevent artifacts. */
  system_of(self)->handleWindowEvent(GHOST_kEventWindowSize, window_of(self));

  window_of(self)->setImmediateDraw(false);
}

void imp_windowWillExitFullScreen(id self, SEL, id)
{
  window_of(self)->setImmediateDraw(true);
}

void imp_windowDidExitFullScreen(id self, SEL, id)
{
  /* See comment for windowWillEnterFullScreen. */
  system_of(self)->handleWindowEvent(GHOST_kEventWindowSize, window_of(self));
  window_of(self)->setImmediateDraw(false);
}

void imp_windowDidResize(id self, SEL, id notification)
{
  {
    /* Send event only once, at end of resize operation (when user has released mouse button). */
    system_of(self)->handleWindowEvent(GHOST_kEventWindowSize, window_of(self));
  }
  /* Live resize, send event, gets handled in wm_window.c.
   * Needed because live resize runs in a modal loop, not letting main loop run */
  if (msg<signed char>(msg<id>(notification, GHOST_SEL(object)), GHOST_SEL(inLiveResize))) {
    system_of(self)->dispatchEvents();
  }
}

void imp_windowDidChangeBackingProperties(id self, SEL, id)
{
  system_of(self)->handleWindowEvent(GHOST_kEventNativeResolutionChange, window_of(self));
  system_of(self)->handleWindowEvent(GHOST_kEventWindowSize, window_of(self));
}

signed char imp_windowShouldClose(id self, SEL, id /*sender*/)
{
  /* Let Blender close the window rather than closing immediately. */
  system_of(self)->handleWindowEvent(GHOST_kEventWindowClose, window_of(self));
  return 0; /* false */
}

Class build_window_delegate_class()
{
  ghost_objc::ClassBuilder b("BlenderWindowDelegate", "NSObject");
  b.protocol("NSWindowDelegate");
  b.ivar(kIvarSystem, sizeof(void *), 3, "^v");
  b.ivar(kIvarWindow, sizeof(void *), 3, "^v");
  b.method("windowDidBecomeKey:", (IMP)imp_windowDidBecomeKey);
  b.method("windowDidResignKey:", (IMP)imp_windowDidResignKey);
  b.method("windowDidExpose:", (IMP)imp_windowDidExpose);
  b.method("windowDidMove:", (IMP)imp_windowDidMove);
  b.method("windowWillMove:", (IMP)imp_windowWillMove);
  b.method("windowWillEnterFullScreen:", (IMP)imp_windowWillEnterFullScreen);
  b.method("windowDidEnterFullScreen:", (IMP)imp_windowDidEnterFullScreen);
  b.method("windowWillExitFullScreen:", (IMP)imp_windowWillExitFullScreen);
  b.method("windowDidExitFullScreen:", (IMP)imp_windowDidExitFullScreen);
  b.method("windowDidResize:", (IMP)imp_windowDidResize);
  b.method("windowDidChangeBackingProperties:", (IMP)imp_windowDidChangeBackingProperties);
  b.method("windowShouldClose:", (IMP)imp_windowShouldClose);
  return b.finish();
}

Class window_delegate_class()
{
  static Class cls = build_window_delegate_class();
  return cls;
}

/* -------------------------------------------------------------------------
 * BlenderWindow: subclase de NSWindow y destino de arrastrar-y-soltar.
 */

signed char imp_canBecomeKeyWindow(id self, SEL)
{
  /* Don't make other windows active when a dialog window is open. */
  return (window_of(self)->isDialog() || !system_of(self)->hasDialogWindow()) ? 1 : 0;
}

/* The drag & drop dragging destination methods. */
NSUInteger_ imp_draggingEntered(id self, SEL, id sender)
{
  GHOST_TDragnDropTypes dragged_type = GHOST_kDragnDropTypeUnknown;
  {
    ghost_objc::AutoreleasePool pool;
    id draggingPBoard = msg<id>(sender, GHOST_SEL(draggingPasteboard));
    id types = msg<id>(draggingPBoard, GHOST_SEL(types));
    if (msg<signed char>(types, GHOST_SEL(containsObject:), NSPasteboardTypeTIFF)) {
      dragged_type = GHOST_kDragnDropTypeBitmap;
    }
    else if (msg<signed char>(types, GHOST_SEL(containsObject:), NSFilenamesPboardType)) {
      dragged_type = GHOST_kDragnDropTypeFilenames;
    }
    else if (msg<signed char>(types, GHOST_SEL(containsObject:), NSPasteboardTypeString)) {
      dragged_type = GHOST_kDragnDropTypeString;
    }
    else {
      return kNSDragOperationNone;
    }
    ghost_objc::ivar_set<GHOST_TDragnDropTypes>(self, kIvarDraggedType, dragged_type);

    const CGPoint mouseLocation = msg<CGPoint>(sender, GHOST_SEL(draggingLocation));
    window_of(self)->setAcceptDragOperation(true); /* Drag operation is accepted by default. */
    system_of(self)->handleDraggingEvent(GHOST_kEventDraggingEntered,
                                         dragged_type,
                                         window_of(self),
                                         mouseLocation.x,
                                         mouseLocation.y,
                                         nullptr);
  }
  return kNSDragOperationCopy;
}

signed char imp_wantsPeriodicDraggingUpdates(id, SEL)
{
  return 0; /* No need to overflow blender event queue. Events shall be sent only on changes. */
}

NSUInteger_ imp_draggingUpdated(id self, SEL, id sender)
{
  const CGPoint mouseLocation = msg<CGPoint>(sender, GHOST_SEL(draggingLocation));

  system_of(self)->handleDraggingEvent(
      GHOST_kEventDraggingUpdated,
      ghost_objc::ivar_get<GHOST_TDragnDropTypes>(self, kIvarDraggedType),
      window_of(self),
      mouseLocation.x,
      mouseLocation.y,
      nullptr);
  return window_of(self)->canAcceptDragOperation() ? kNSDragOperationCopy : kNSDragOperationNone;
}

void imp_draggingExited(id self, SEL, id /*sender*/)
{
  system_of(self)->handleDraggingEvent(
      GHOST_kEventDraggingExited,
      ghost_objc::ivar_get<GHOST_TDragnDropTypes>(self, kIvarDraggedType),
      window_of(self),
      0,
      0,
      nullptr);
  ghost_objc::ivar_set<GHOST_TDragnDropTypes>(self, kIvarDraggedType, GHOST_kDragnDropTypeUnknown);
}

signed char imp_prepareForDragOperation(id self, SEL, id /*sender*/)
{
  return window_of(self)->canAcceptDragOperation() ? 1 : 0;
}

signed char imp_performDragOperation(id self, SEL, id sender)
{
  const GHOST_TDragnDropTypes dragged_type = ghost_objc::ivar_get<GHOST_TDragnDropTypes>(
      self, kIvarDraggedType);
  {
    ghost_objc::AutoreleasePool pool;
    id draggingPBoard = msg<id>(sender, GHOST_SEL(draggingPasteboard));
    id data = nullptr;

    switch (dragged_type) {
      case GHOST_kDragnDropTypeBitmap: {
        if (!msg<signed char>(
                GHOST_CLS(NSImage), GHOST_SEL(canInitWithPasteboard:), draggingPBoard))
        {
          return 0; /* NO */
        }
        /* Caller must [release] the returned data in this case. */
        id droppedImg = msg<id>(msg<id>(GHOST_CLS(NSImage), GHOST_SEL(alloc)),
                                GHOST_SEL(initWithPasteboard:),
                                draggingPBoard);
        data = droppedImg;
        break;
      }
      case GHOST_kDragnDropTypeFilenames:
        data = msg<id>(draggingPBoard, GHOST_SEL(propertyListForType:), NSFilenamesPboardType);
        break;
      case GHOST_kDragnDropTypeString:
        data = msg<id>(draggingPBoard, GHOST_SEL(stringForType:), NSPasteboardTypeString);
        break;
      default:
        return 0; /* NO */
    }

    const CGPoint mouseLocation = msg<CGPoint>(sender, GHOST_SEL(draggingLocation));
    system_of(self)->handleDraggingEvent(GHOST_kEventDraggingDropDone,
                                         dragged_type,
                                         window_of(self),
                                         mouseLocation.x,
                                         mouseLocation.y,
                                         (void *)data);
  }
  return 1; /* YES */
}

Class build_blender_window_class()
{
  ghost_objc::ClassBuilder b("BlenderWindow", "NSWindow");
  b.protocol("NSDraggingDestination");
  b.ivar(kIvarSystem, sizeof(void *), 3, "^v");
  b.ivar(kIvarWindow, sizeof(void *), 3, "^v");
  b.ivar(kIvarDraggedType, sizeof(GHOST_TDragnDropTypes), 2, "i");
  b.method("canBecomeKeyWindow", (IMP)imp_canBecomeKeyWindow);
  b.method("draggingEntered:", (IMP)imp_draggingEntered);
  b.method("wantsPeriodicDraggingUpdates", (IMP)imp_wantsPeriodicDraggingUpdates);
  b.method("draggingUpdated:", (IMP)imp_draggingUpdated);
  b.method("draggingExited:", (IMP)imp_draggingExited);
  b.method("prepareForDragOperation:", (IMP)imp_prepareForDragOperation);
  b.method("performDragOperation:", (IMP)imp_performDragOperation);
  return b.finish();
}

Class blender_window_class()
{
  static Class cls = build_blender_window_class();
  return cls;
}

/* Ayuda: `[NSString stringWithUTF8String:]` autoliberado. */
id ns(const char *utf8)
{
  return ghost_objc::nsstring(utf8);
}

}  // namespace

/* --------------------------------------------------------------------
 * Initialization / Finalization.
 */

GHOST_WindowCocoa::GHOST_WindowCocoa(GHOST_SystemCocoa *systemCocoa,
                                     const char *title,
                                     int32_t left,
                                     int32_t bottom,
                                     uint32_t width,
                                     uint32_t height,
                                     GHOST_TWindowState state,
                                     GHOST_TDrawingContextType type,
                                     const bool stereoVisual,
                                     bool is_debug,
                                     bool is_dialog,
                                     GHOST_WindowCocoa *parentWindow,
                                     const GHOST_GPUDevice &preferred_device)
    : GHOST_Window(width, height, state, stereoVisual, false),
      m_openGLView(nullptr),
      m_metalView(nullptr),
      m_metalLayer(nullptr),
      m_systemCocoa(systemCocoa),
      m_customCursor(nullptr),
      m_immediateDraw(false),
      m_debug_context(is_debug),
      m_is_dialog(is_dialog),
      m_preferred_device(preferred_device)
{
  m_fullScreen = false;

  ghost_objc::AutoreleasePool pool;

  /* Create the window. */
  CGRect rect;
  rect.origin.x = left;
  rect.origin.y = bottom;
  rect.size.width = width;
  rect.size.height = height;

  NSUInteger_ styleMask = kNSWindowStyleMaskTitled | kNSWindowStyleMaskClosable |
                          kNSWindowStyleMaskResizable;
  if (!is_dialog) {
    styleMask |= kNSWindowStyleMaskMiniaturizable;
  }

  /* El original tenia un inicializador propio `initWithSystemCocoa:...contentRect:...`
   * que solo guardaba dos punteros y llamaba al designado de NSWindow. Aqui se llama
   * directamente al designado —cuya codificacion conoce el runtime— y los dos punteros
   * se escriben desde C++. Un metodo menos que registrar y una codificacion menos que
   * pueda estar mal. */
  id window = msg<id>(msg<id>((id)blender_window_class(), GHOST_SEL(alloc)),
                      GHOST_SEL(initWithContentRect:styleMask:backing:defer:),
                      rect,
                      styleMask,
                      kNSBackingStoreBuffered,
                      (signed char)0);
  ghost_objc::ivar_set<GHOST_SystemCocoa *>(window, kIvarSystem, systemCocoa);
  ghost_objc::ivar_set<GHOST_WindowCocoa *>(window, kIvarWindow, this);
  ghost_objc::ivar_set<GHOST_TDragnDropTypes>(
      window, kIvarDraggedType, GHOST_kDragnDropTypeUnknown);
  m_window = reinterpret_cast<BlenderWindow *>(window);

  /* Forbid to resize the window below the blender defined minimum one. */
  const CGSize minSize = {320, 240};
  msg<void>(window, GHOST_SEL(setContentMinSize:), minSize);

  /* Create NSView inside the window. */
  id metalDevice = MTLCreateSystemDefaultDevice();
  id view;

  if (metalDevice) {
    /* Create metal layer and view if supported. */
    id layer = ghost_objc::alloc_init("CAMetalLayer");
    m_metalLayer = reinterpret_cast<CAMetalLayer *>(layer);
    msg<void>(layer, GHOST_SEL(setEdgeAntialiasingMask:), (unsigned int)0);
    msg<void>(layer, GHOST_SEL(setMasksToBounds:), (signed char)0);
    msg<void>(layer, GHOST_SEL(setOpaque:), (signed char)1);
    msg<void>(layer, GHOST_SEL(setFramebufferOnly:), (signed char)1);
    msg<void>(layer, GHOST_SEL(setPresentsWithTransaction:), (signed char)0);
    msg<void>(layer, GHOST_SEL(removeAllAnimations));
    msg<void>(layer, GHOST_SEL(setDevice:), metalDevice);

    if (type == GHOST_kDrawingContextTypeMetal) {
      /* Enable EDR support. This is done by:
       * 1. Using a floating point render target, so that values outside 0..1 can be used
       * 2. Informing the OS that we are EDR aware, and intend to use values outside 0..1
       * 3. Setting the extended sRGB color space so that the OS knows how to interpret the
       *    values.
       */
      msg<void>(layer, GHOST_SEL(setWantsExtendedDynamicRangeContent:), (signed char)1);
      msg<void>(layer, GHOST_SEL(setPixelFormat:), kMTLPixelFormatRGBA16Float);
      const CFStringRef name = kCGColorSpaceExtendedSRGB;
      CGColorSpaceRef colorspace = CGColorSpaceCreateWithName(name);
      msg<void>(layer, GHOST_SEL(setColorspace:), colorspace);
      CGColorSpaceRelease(colorspace);
    }

    id metal_view = ghost_cocoa_view::view_create(true, systemCocoa, this);
    m_metalView = reinterpret_cast<CocoaMetalView *>(metal_view);
    msg<void>(metal_view, GHOST_SEL(setWantsLayer:), (signed char)1);
    msg<void>(metal_view, GHOST_SEL(setLayer:), layer);
    view = metal_view;
  }
  else {
    /* Fall back to OpenGL view if there is no Metal support. */
    id gl_view = ghost_cocoa_view::view_create(false, systemCocoa, this);
    m_openGLView = reinterpret_cast<CocoaOpenGLView *>(gl_view);
    view = gl_view;
  }

  if (m_systemCocoa->m_nativePixel) {
    /* Needs to happen early when building with the 10.14 SDK, otherwise
     * has no effect until resizing the window. */
    if (msg<signed char>(view,
                         GHOST_SEL(respondsToSelector:),
                         ghost_objc::sel("setWantsBestResolutionOpenGLSurface:")))
    {
      msg<void>(view, GHOST_SEL(setWantsBestResolutionOpenGLSurface:), (signed char)1);
    }
  }

  msg<void>(window, GHOST_SEL(setContentView:), view);
  msg<void>(window, GHOST_SEL(setInitialFirstResponder:), view);

  msg<void>(window, GHOST_SEL(makeKeyAndOrderFront:), (id) nullptr);

  setDrawingContextType(type);
  updateDrawingContext();
  activateDrawingContext();

  setTitle(title);

  m_tablet = GHOST_TABLET_DATA_NONE;

  id windowDelegate = msg<id>(msg<id>((id)window_delegate_class(), GHOST_SEL(alloc)),
                              GHOST_SEL(init));
  ghost_objc::ivar_set<GHOST_SystemCocoa *>(windowDelegate, kIvarSystem, systemCocoa);
  ghost_objc::ivar_set<GHOST_WindowCocoa *>(windowDelegate, kIvarWindow, this);
  msg<void>(window, GHOST_SEL(setDelegate:), windowDelegate);

  msg<void>(window, GHOST_SEL(setAcceptsMouseMovedEvents:), (signed char)1);

  id contentview = msg<id>(window, GHOST_SEL(contentView));
  msg<void>(contentview,
            GHOST_SEL(setAllowedTouchTypes:),
            (NSUInteger_)(kNSTouchTypeMaskDirect | kNSTouchTypeMaskIndirect));

  id dragged_types = msg<id>(GHOST_CLS(NSArray),
                             GHOST_SEL(arrayWithObjects:),
                             NSFilenamesPboardType,
                             NSPasteboardTypeString,
                             NSPasteboardTypeTIFF,
                             (id) nullptr);
  msg<void>(window, GHOST_SEL(registerForDraggedTypes:), dragged_types);

  if (is_dialog && parentWindow) {
    msg<void>(reinterpret_cast<id>(parentWindow->getViewWindow()),
              GHOST_SEL(addChildWindow:ordered:),
              window,
              kNSWindowAbove);
    msg<void>(window,
              GHOST_SEL(setCollectionBehavior:),
              kNSWindowCollectionBehaviorFullScreenAuxiliary);
  }
  else {
    msg<void>(
        window, GHOST_SEL(setCollectionBehavior:), kNSWindowCollectionBehaviorFullScreenPrimary);
  }

  if (state == GHOST_kWindowStateFullScreen) {
    setState(GHOST_kWindowStateFullScreen);
  }

  setNativePixelSize();
}

GHOST_WindowCocoa::~GHOST_WindowCocoa()
{
  ghost_objc::AutoreleasePool pool;

  if (m_customCursor) {
    ghost_objc::release(reinterpret_cast<id>(m_customCursor));
    m_customCursor = nullptr;
  }

  releaseNativeHandles();

  if (m_openGLView) {
    ghost_objc::release(reinterpret_cast<id>(m_openGLView));
    m_openGLView = nullptr;
  }
  if (m_metalView) {
    ghost_objc::release(reinterpret_cast<id>(m_metalView));
    m_metalView = nullptr;
  }
  if (m_metalLayer) {
    ghost_objc::release(reinterpret_cast<id>(m_metalLayer));
    m_metalLayer = nullptr;
  }

  if (m_window) {
    msg<void>(reinterpret_cast<id>(m_window), GHOST_SEL(close));
  }

  /* Check for other blender opened windows and make the front-most key
   * NOTE: for some reason the closed window is still in the list. */
  id windowsList = msg<id>(NSApp, GHOST_SEL(orderedWindows));
  const NSUInteger_ count = msg<NSUInteger_>(windowsList, GHOST_SEL(count));
  for (NSUInteger_ a = 0; a < count; a++) {
    id other = msg<id>(windowsList, GHOST_SEL(objectAtIndex:), a);
    if (reinterpret_cast<id>(m_window) != other) {
      msg<void>(other, GHOST_SEL(makeKeyWindow));
      break;
    }
  }
  m_window = nullptr;
}

/* --------------------------------------------------------------------
 * Accessors.
 */

bool GHOST_WindowCocoa::getValid() const
{
  const void *view = (m_openGLView) ? (const void *)m_openGLView : (const void *)m_metalView;
  return GHOST_Window::getValid() && m_window != nullptr && view != nullptr;
}

void *GHOST_WindowCocoa::getOSWindow() const
{
  return (void *)m_window;
}

void GHOST_WindowCocoa::setTitle(const char *title)
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::setTitle(): window invalid");

  ghost_objc::AutoreleasePool pool;
  id windowTitle = msg<id>(msg<id>(GHOST_CLS(NSString), GHOST_SEL(alloc)),
                           GHOST_SEL(initWithCString:encoding:),
                           title,
                           kNSUTF8StringEncoding);
  msg<void>(reinterpret_cast<id>(m_window), GHOST_SEL(setTitle:), windowTitle);

  ghost_objc::release(windowTitle);
}

std::string GHOST_WindowCocoa::getTitle() const
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::getTitle(): window invalid");

  std::string title;
  {
    ghost_objc::AutoreleasePool pool;
    id windowTitle = msg<id>(reinterpret_cast<id>(m_window), GHOST_SEL(title));
    if (windowTitle != nullptr) {
      const char *utf8 = ghost_objc::utf8_string(windowTitle);
      if (utf8) {
        title = utf8;
      }
    }
  }
  return title;
}

GHOST_TSuccess GHOST_WindowCocoa::setPath(const char *filepath)
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::setAssociatedFile(): window invalid");

  {
    ghost_objc::AutoreleasePool pool;
    id associatedFileName = ghost_objc::autorelease(
        msg<id>(msg<id>(GHOST_CLS(NSString), GHOST_SEL(alloc)),
                GHOST_SEL(initWithCString:encoding:),
                filepath,
                kNSUTF8StringEncoding));

    msg<void>(
        reinterpret_cast<id>(m_window), GHOST_SEL(setRepresentedFilename:), associatedFileName);
  }

  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowCocoa::applyWindowDecorationStyle()
{
  ghost_objc::AutoreleasePool pool;
  id window = reinterpret_cast<id>(m_window);

  if (m_windowDecorationStyleFlags & GHOST_kDecorationColoredTitleBar) {
    const float *background_color = m_windowDecorationStyleSettings.colored_titlebar_bg_color;

    /* Title-bar background color. */
    id color = msg<id>(GHOST_CLS(NSColor),
                       GHOST_SEL(colorWithRed:green:blue:alpha:),
                       (CGFloat)background_color[0],
                       (CGFloat)background_color[1],
                       (CGFloat)background_color[2],
                       (CGFloat)1.0);
    msg<void>(window, GHOST_SEL(setBackgroundColor:), color);

    /* Title-bar foreground color.
     * Use the value component of the title-bar background's HSV representation to determine
     * whether we should use the macOS dark or light title-bar text appearance. With values below
     * 0.5 considered as dark themes, and values above 0.5 considered as light themes.
     */
    const float hsv_v = MAX(background_color[0], MAX(background_color[1], background_color[2]));

    id win_appearance = hsv_v > 0.5 ? NSAppearanceNameVibrantLight : NSAppearanceNameVibrantDark;

    msg<void>(window,
              GHOST_SEL(setAppearance:),
              msg<id>(GHOST_CLS(NSAppearance), GHOST_SEL(appearanceNamed:), win_appearance));
    msg<void>(window, GHOST_SEL(setTitlebarAppearsTransparent:), (signed char)1);
  }
  else {
    msg<void>(window, GHOST_SEL(setTitlebarAppearsTransparent:), (signed char)0);
  }
  return GHOST_kSuccess;
}

void GHOST_WindowCocoa::getWindowBounds(GHOST_Rect &bounds) const
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::getWindowBounds(): window invalid");

  ghost_objc::AutoreleasePool pool;
  id window = reinterpret_cast<id>(m_window);
  const CGRect screenSize = msg<CGRect>(msg<id>(window, GHOST_SEL(screen)),
                                        GHOST_SEL(visibleFrame));
  const CGRect rect = msg<CGRect>(window, GHOST_SEL(frame));

  bounds.m_b = screenSize.size.height - (rect.origin.y - screenSize.origin.y);
  bounds.m_l = rect.origin.x - screenSize.origin.x;
  bounds.m_r = rect.origin.x - screenSize.origin.x + rect.size.width;
  bounds.m_t = screenSize.size.height - (rect.origin.y + rect.size.height - screenSize.origin.y);
}

void GHOST_WindowCocoa::getClientBounds(GHOST_Rect &bounds) const
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::getClientBounds(): window invalid");

  ghost_objc::AutoreleasePool pool;
  id window = reinterpret_cast<id>(m_window);
  const CGRect screenSize = msg<CGRect>(msg<id>(window, GHOST_SEL(screen)),
                                        GHOST_SEL(visibleFrame));

  /* Max window contents as screen size (excluding title bar...). */
  const CGRect contentRect = msg<CGRect>((id)blender_window_class(),
                                         GHOST_SEL(contentRectForFrameRect:styleMask:),
                                         screenSize,
                                         msg<NSUInteger_>(window, GHOST_SEL(styleMask)));

  const CGRect rect = msg<CGRect>(window,
                                  GHOST_SEL(contentRectForFrameRect:),
                                  msg<CGRect>(window, GHOST_SEL(frame)));

  bounds.m_b = contentRect.size.height - (rect.origin.y - contentRect.origin.y);
  bounds.m_l = rect.origin.x - contentRect.origin.x;
  bounds.m_r = rect.origin.x - contentRect.origin.x + rect.size.width;
  bounds.m_t = contentRect.size.height - (rect.origin.y + rect.size.height - contentRect.origin.y);
}

GHOST_TSuccess GHOST_WindowCocoa::setClientWidth(uint32_t width)
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::setClientWidth(): window invalid");

  ghost_objc::AutoreleasePool pool;
  GHOST_Rect cBnds;
  getClientBounds(cBnds);

  if ((uint32_t(cBnds.getWidth())) != width) {
    const CGSize size = {(CGFloat)width, (CGFloat)cBnds.getHeight()};
    msg<void>(reinterpret_cast<id>(m_window), GHOST_SEL(setContentSize:), size);
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowCocoa::setClientHeight(uint32_t height)
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::setClientHeight(): window invalid");

  ghost_objc::AutoreleasePool pool;
  GHOST_Rect cBnds;
  getClientBounds(cBnds);

  if ((uint32_t(cBnds.getHeight())) != height) {
    const CGSize size = {(CGFloat)cBnds.getWidth(), (CGFloat)height};
    msg<void>(reinterpret_cast<id>(m_window), GHOST_SEL(setContentSize:), size);
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowCocoa::setClientSize(uint32_t width, uint32_t height)
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::setClientSize(): window invalid");

  ghost_objc::AutoreleasePool pool;
  GHOST_Rect cBnds;
  getClientBounds(cBnds);
  if (((uint32_t(cBnds.getWidth())) != width) || ((uint32_t(cBnds.getHeight())) != height)) {
    const CGSize size = {(CGFloat)width, (CGFloat)height};
    msg<void>(reinterpret_cast<id>(m_window), GHOST_SEL(setContentSize:), size);
  }
  return GHOST_kSuccess;
}

GHOST_TWindowState GHOST_WindowCocoa::getState() const
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::getState(): window invalid");

  ghost_objc::AutoreleasePool pool;
  id window = reinterpret_cast<id>(m_window);
  const NSUInteger_ masks = msg<NSUInteger_>(window, GHOST_SEL(styleMask));

  if (masks & kNSWindowStyleMaskFullScreen) {
    /* Lion style full-screen. */
    if (!m_immediateDraw) {
      return GHOST_kWindowStateFullScreen;
    }
    return GHOST_kWindowStateNormal;
  }
  if (msg<signed char>(window, GHOST_SEL(isMiniaturized))) {
    return GHOST_kWindowStateMinimized;
  }
  if (msg<signed char>(window, GHOST_SEL(isZoomed))) {
    return GHOST_kWindowStateMaximized;
  }
  if (m_immediateDraw) {
    return GHOST_kWindowStateFullScreen;
  }
  return GHOST_kWindowStateNormal;
}

void GHOST_WindowCocoa::screenToClient(int32_t inX,
                                       int32_t inY,
                                       int32_t &outX,
                                       int32_t &outY) const
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::screenToClient(): window invalid");

  screenToClientIntern(inX, inY, outX, outY);

  /* switch y to match ghost convention */
  GHOST_Rect cBnds;
  getClientBounds(cBnds);
  outY = (cBnds.getHeight() - 1) - outY;
}

void GHOST_WindowCocoa::clientToScreen(int32_t inX,
                                       int32_t inY,
                                       int32_t &outX,
                                       int32_t &outY) const
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::clientToScreen(): window invalid");

  /* switch y to match ghost convention */
  GHOST_Rect cBnds;
  getClientBounds(cBnds);
  inY = (cBnds.getHeight() - 1) - inY;

  clientToScreenIntern(inX, inY, outX, outY);
}

void GHOST_WindowCocoa::screenToClientIntern(int32_t inX,
                                             int32_t inY,
                                             int32_t &outX,
                                             int32_t &outY) const
{
  CGRect screenCoord;
  screenCoord.origin = {(CGFloat)inX, (CGFloat)inY};
  screenCoord.size = {0, 0};

  const CGRect baseCoord = msg<CGRect>(
      reinterpret_cast<id>(m_window), GHOST_SEL(convertRectFromScreen:), screenCoord);

  outX = baseCoord.origin.x;
  outY = baseCoord.origin.y;
}

void GHOST_WindowCocoa::clientToScreenIntern(int32_t inX,
                                             int32_t inY,
                                             int32_t &outX,
                                             int32_t &outY) const
{
  CGRect baseCoord;
  baseCoord.origin = {(CGFloat)inX, (CGFloat)inY};
  baseCoord.size = {0, 0};

  const CGRect screenCoord = msg<CGRect>(
      reinterpret_cast<id>(m_window), GHOST_SEL(convertRectToScreen:), baseCoord);

  outX = screenCoord.origin.x;
  outY = screenCoord.origin.y;
}

NSScreen *GHOST_WindowCocoa::getScreen()
{
  return reinterpret_cast<NSScreen *>(msg<id>(reinterpret_cast<id>(m_window), GHOST_SEL(screen)));
}

/* called for event, when window leaves monitor to another */
void GHOST_WindowCocoa::setNativePixelSize()
{
  id view = (m_openGLView) ? reinterpret_cast<id>(m_openGLView) :
                             reinterpret_cast<id>(m_metalView);
  const CGRect backingBounds = msg<CGRect>(
      view, GHOST_SEL(convertRectToBacking:), msg<CGRect>(view, GHOST_SEL(bounds)));

  GHOST_Rect rect;
  getClientBounds(rect);

  m_nativePixelSize = float(backingBounds.size.width) / float(rect.getWidth());
}

/**
 * \note Full-screen switch is not actual full-screen with display capture.
 * As this capture removes all OS X window manager features.
 *
 * Instead, the menu bar and the dock are hidden, and the window is made border-less and enlarged.
 * Thus, process switch, exposé, spaces, ... still work in full-screen mode.
 */
GHOST_TSuccess GHOST_WindowCocoa::setState(GHOST_TWindowState state)
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::setState(): window invalid");

  ghost_objc::AutoreleasePool pool;
  id window = reinterpret_cast<id>(m_window);

  switch (state) {
    case GHOST_kWindowStateMinimized:
      msg<void>(window, GHOST_SEL(miniaturize:), (id) nullptr);
      break;
    case GHOST_kWindowStateMaximized:
      msg<void>(window, GHOST_SEL(zoom:), (id) nullptr);
      break;

    case GHOST_kWindowStateFullScreen: {
      const NSUInteger_ masks = msg<NSUInteger_>(window, GHOST_SEL(styleMask));

      if (!(masks & kNSWindowStyleMaskFullScreen)) {
        msg<void>(window, GHOST_SEL(toggleFullScreen:), (id) nullptr);
      }
      break;
    }
    case GHOST_kWindowStateNormal:
    default: {
      ghost_objc::AutoreleasePool inner;
      const NSUInteger_ masks = msg<NSUInteger_>(window, GHOST_SEL(styleMask));

      if (masks & kNSWindowStyleMaskFullScreen) {
        /* Lion style full-screen. */
        msg<void>(window, GHOST_SEL(toggleFullScreen:), (id) nullptr);
      }
      else if (msg<signed char>(window, GHOST_SEL(isMiniaturized))) {
        msg<void>(window, GHOST_SEL(deminiaturize:), (id) nullptr);
      }
      else if (msg<signed char>(window, GHOST_SEL(isZoomed))) {
        msg<void>(window, GHOST_SEL(zoom:), (id) nullptr);
      }
      break;
    }
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowCocoa::setModifiedState(bool isUnsavedChanges)
{
  {
    ghost_objc::AutoreleasePool pool;
    msg<void>(reinterpret_cast<id>(m_window),
              GHOST_SEL(setDocumentEdited:),
              (signed char)(isUnsavedChanges ? 1 : 0));
  }
  return GHOST_Window::setModifiedState(isUnsavedChanges);
}

GHOST_TSuccess GHOST_WindowCocoa::setOrder(GHOST_TWindowOrder order)
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::setOrder(): window invalid");

  ghost_objc::AutoreleasePool pool;
  id window = reinterpret_cast<id>(m_window);

  if (order == GHOST_kWindowOrderTop) {
    msg<void>(NSApp, GHOST_SEL(activateIgnoringOtherApps:), (signed char)1);
    msg<void>(window, GHOST_SEL(makeKeyAndOrderFront:), (id) nullptr);
  }
  else {
    msg<void>(window, GHOST_SEL(orderBack:), (id) nullptr);

    /* Check for other blender opened windows and make the front-most key. */
    id windowsList = msg<id>(NSApp, GHOST_SEL(orderedWindows));
    if (msg<NSUInteger_>(windowsList, GHOST_SEL(count))) {
      msg<void>(msg<id>(windowsList, GHOST_SEL(objectAtIndex:), (NSUInteger_)0),
                GHOST_SEL(makeKeyAndOrderFront:),
                (id) nullptr);
    }
  }
  return GHOST_kSuccess;
}

/* --------------------------------------------------------------------
 * Drawing context.
 */

GHOST_Context *GHOST_WindowCocoa::newDrawingContext(GHOST_TDrawingContextType type)
{
  switch (type) {
#ifdef WITH_VULKAN_BACKEND
    case GHOST_kDrawingContextTypeVulkan: {
      GHOST_Context *context = new GHOST_ContextVK(
          m_wantStereoVisual, m_metalLayer, 1, 2, true, m_preferred_device);
      if (context->initializeDrawingContext()) {
        return context;
      }
      delete context;
      return nullptr;
    }
#endif

#ifdef WITH_METAL_BACKEND
    case GHOST_kDrawingContextTypeMetal: {
      GHOST_Context *context = new GHOST_ContextCGL(
          m_wantStereoVisual, reinterpret_cast<NSView *>(m_metalView), m_metalLayer, false);
      if (context->initializeDrawingContext()) {
        return context;
      }
      delete context;
      return nullptr;
    }
#endif

    default:
      /* Unsupported backend. */
      return nullptr;
  }
}

/* --------------------------------------------------------------------
 * Invalidate.
 */

GHOST_TSuccess GHOST_WindowCocoa::invalidate()
{
  GHOST_ASSERT(getValid(), "GHOST_WindowCocoa::invalidate(): window invalid");

  ghost_objc::AutoreleasePool pool;
  id view = (m_openGLView) ? reinterpret_cast<id>(m_openGLView) :
                             reinterpret_cast<id>(m_metalView);
  msg<void>(view, GHOST_SEL(setNeedsDisplay:), (signed char)1);
  return GHOST_kSuccess;
}

/* --------------------------------------------------------------------
 * Progress bar.
 */

GHOST_TSuccess GHOST_WindowCocoa::setProgressBar(float progress)
{
  ghost_objc::AutoreleasePool pool;

  if ((progress >= 0.0) && (progress <= 1.0)) {
    const CGSize icon_size = {128, 128};
    id dockIcon = msg<id>(
        msg<id>(GHOST_CLS(NSImage), GHOST_SEL(alloc)), GHOST_SEL(initWithSize:), icon_size);

    msg<void>(dockIcon, GHOST_SEL(lockFocus));

    msg<void>(msg<id>(GHOST_CLS(NSImage), GHOST_SEL(imageNamed:), ns("NSApplicationIcon")),
              GHOST_SEL(drawAtPoint:fromRect:operation:fraction:),
              CGPointMake(0, 0),
              CGRectMake(0, 0, 0, 0),
              kNSCompositingOperationSourceOver,
              (CGFloat)1.0);

    CGRect progressRect = {{8, 8}, {112, 14}};
    id progressPath;

    /* Draw white track. */
    msg<void>(msg<id>(msg<id>(GHOST_CLS(NSColor), GHOST_SEL(whiteColor)),
                      GHOST_SEL(colorWithAlphaComponent:),
                      (CGFloat)0.6),
              GHOST_SEL(setFill));
    progressPath = msg<id>(GHOST_CLS(NSBezierPath),
                           GHOST_SEL(bezierPathWithRoundedRect:xRadius:yRadius:),
                           progressRect,
                           (CGFloat)7,
                           (CGFloat)7);
    msg<void>(progressPath, GHOST_SEL(fill));

    /* Black progress fill. */
    msg<void>(msg<id>(msg<id>(GHOST_CLS(NSColor), GHOST_SEL(blackColor)),
                      GHOST_SEL(colorWithAlphaComponent:),
                      (CGFloat)0.7),
              GHOST_SEL(setFill));
    progressRect = NSInsetRect(progressRect, 2, 2);
    progressRect.size.width *= progress;
    progressPath = msg<id>(GHOST_CLS(NSBezierPath),
                           GHOST_SEL(bezierPathWithRoundedRect:xRadius:yRadius:),
                           progressRect,
                           (CGFloat)5,
                           (CGFloat)5);
    msg<void>(progressPath, GHOST_SEL(fill));

    msg<void>(dockIcon, GHOST_SEL(unlockFocus));
    msg<void>(NSApp, GHOST_SEL(setApplicationIconImage:), dockIcon);
    ghost_objc::release(dockIcon);

    m_progressBarVisible = true;
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowCocoa::endProgressBar()
{
  if (!m_progressBarVisible) {
    return GHOST_kFailure;
  }
  m_progressBarVisible = false;

  /* Reset application icon to remove the progress bar. */
  {
    ghost_objc::AutoreleasePool pool;
    const CGSize icon_size = {128, 128};
    id dockIcon = msg<id>(
        msg<id>(GHOST_CLS(NSImage), GHOST_SEL(alloc)), GHOST_SEL(initWithSize:), icon_size);
    msg<void>(dockIcon, GHOST_SEL(lockFocus));
    msg<void>(msg<id>(GHOST_CLS(NSImage), GHOST_SEL(imageNamed:), ns("NSApplicationIcon")),
              GHOST_SEL(drawAtPoint:fromRect:operation:fraction:),
              CGPointMake(0, 0),
              CGRectMake(0, 0, 0, 0),
              kNSCompositingOperationSourceOver,
              (CGFloat)1.0);
    msg<void>(dockIcon, GHOST_SEL(unlockFocus));
    msg<void>(NSApp, GHOST_SEL(setApplicationIconImage:), dockIcon);
    ghost_objc::release(dockIcon);
  }
  return GHOST_kSuccess;
}

/* --------------------------------------------------------------------
 * Cursor handling.
 */

static NSCursor *getImageCursor(GHOST_TStandardCursor shape, const char *name, CGPoint hotspot)
{
  static id cursors[GHOST_kStandardCursorNumCursors] = {nullptr};
  static bool loaded[GHOST_kStandardCursorNumCursors] = {false};

  const int index = int(shape);
  if (!loaded[index]) {
    /* Load image from file in application Resources folder. */
    {
      ghost_objc::AutoreleasePool pool;
      id image = msg<id>(GHOST_CLS(NSImage), GHOST_SEL(imageNamed:), ns(name));
      if (image != nullptr) {
        cursors[index] = msg<id>(msg<id>(GHOST_CLS(NSCursor), GHOST_SEL(alloc)),
                                 GHOST_SEL(initWithImage:hotSpot:),
                                 image,
                                 hotspot);
      }
    }

    loaded[index] = true;
  }

  return reinterpret_cast<NSCursor *>(cursors[index]);
}

/** Ayuda: `[NSCursor <nombre>]`, que es un metodo de CLASE. */
static NSCursor *sysCursor(const char *selector_name)
{
  return reinterpret_cast<NSCursor *>(
      msg<id>(GHOST_CLS(NSCursor), ghost_objc::sel(selector_name)));
}

NSCursor *GHOST_WindowCocoa::getStandardCursor(GHOST_TStandardCursor shape) const
{
  ghost_objc::AutoreleasePool pool;

  switch (shape) {
    case GHOST_kStandardCursorCustom:
      if (m_customCursor) {
        return m_customCursor;
      }
      else {
        return nullptr;
      }
    case GHOST_kStandardCursorDestroy:
      return sysCursor("disappearingItemCursor");
    case GHOST_kStandardCursorText:
      return sysCursor("IBeamCursor");
    case GHOST_kStandardCursorCrosshair:
      return sysCursor("crosshairCursor");
    case GHOST_kStandardCursorUpDown:
      return sysCursor("resizeUpDownCursor");
    case GHOST_kStandardCursorLeftRight:
      return sysCursor("resizeLeftRightCursor");
    case GHOST_kStandardCursorTopSide:
      return sysCursor("resizeUpCursor");
    case GHOST_kStandardCursorBottomSide:
      return sysCursor("resizeDownCursor");
    case GHOST_kStandardCursorLeftSide:
      return sysCursor("resizeLeftCursor");
    case GHOST_kStandardCursorRightSide:
      return sysCursor("resizeRightCursor");
    case GHOST_kStandardCursorCopy:
      return sysCursor("dragCopyCursor");
    case GHOST_kStandardCursorStop:
      return sysCursor("operationNotAllowedCursor");
    case GHOST_kStandardCursorMove:
      return sysCursor("openHandCursor");
    case GHOST_kStandardCursorHandOpen:
      return sysCursor("openHandCursor");
    case GHOST_kStandardCursorHandClosed:
      return sysCursor("closedHandCursor");
    case GHOST_kStandardCursorHandPoint:
      return sysCursor("pointingHandCursor");
    case GHOST_kStandardCursorDefault:
      return sysCursor("arrowCursor");
    case GHOST_kStandardCursorWait:
      /* `busyButClickableCursor` es API NO DOCUMENTADA de NSCursor, en uso desde al
       * menos OS X 10.4. El original la declaraba con una categoria `@interface
       * NSCursor (Undocumented)` solo para que el compilador la aceptara; desde C++ no
       * hace falta ninguna declaracion, y la comprobacion con `respondsToSelector:`
       * —que el original ya hacia— sigue siendo la unica proteccion real. */
      if (msg<signed char>(GHOST_CLS(NSCursor),
                           GHOST_SEL(respondsToSelector:),
                           ghost_objc::sel("busyButClickableCursor")))
      {
        return sysCursor("busyButClickableCursor");
      }
      return nullptr;
    case GHOST_kStandardCursorKnife:
      return getImageCursor(shape, "knife.pdf", CGPointMake(6, 24));
    case GHOST_kStandardCursorEraser:
      return getImageCursor(shape, "eraser.pdf", CGPointMake(6, 24));
    case GHOST_kStandardCursorPencil:
      return getImageCursor(shape, "pen.pdf", CGPointMake(6, 24));
    case GHOST_kStandardCursorEyedropper:
      return getImageCursor(shape, "eyedropper.pdf", CGPointMake(6, 24));
    case GHOST_kStandardCursorZoomIn:
      return getImageCursor(shape, "zoomin.pdf", CGPointMake(8, 7));
    case GHOST_kStandardCursorZoomOut:
      return getImageCursor(shape, "zoomout.pdf", CGPointMake(8, 7));
    case GHOST_kStandardCursorNSEWScroll:
      return getImageCursor(shape, "scrollnsew.pdf", CGPointMake(16, 16));
    case GHOST_kStandardCursorNSScroll:
      return getImageCursor(shape, "scrollns.pdf", CGPointMake(16, 16));
    case GHOST_kStandardCursorEWScroll:
      return getImageCursor(shape, "scrollew.pdf", CGPointMake(16, 16));
    case GHOST_kStandardCursorUpArrow:
      return getImageCursor(shape, "arrowup.pdf", CGPointMake(16, 16));
    case GHOST_kStandardCursorDownArrow:
      return getImageCursor(shape, "arrowdown.pdf", CGPointMake(16, 16));
    case GHOST_kStandardCursorLeftArrow:
      return getImageCursor(shape, "arrowleft.pdf", CGPointMake(16, 16));
    case GHOST_kStandardCursorRightArrow:
      return getImageCursor(shape, "arrowright.pdf", CGPointMake(16, 16));
    case GHOST_kStandardCursorVerticalSplit:
      return getImageCursor(shape, "splitv.pdf", CGPointMake(16, 16));
    case GHOST_kStandardCursorHorizontalSplit:
      return getImageCursor(shape, "splith.pdf", CGPointMake(16, 16));
    case GHOST_kStandardCursorCrosshairA:
      return getImageCursor(shape, "paint_cursor_cross.pdf", CGPointMake(16, 15));
    case GHOST_kStandardCursorCrosshairB:
      return getImageCursor(shape, "paint_cursor_dot.pdf", CGPointMake(16, 15));
    case GHOST_kStandardCursorCrosshairC:
      return getImageCursor(shape, "crossc.pdf", CGPointMake(16, 16));
    case GHOST_kStandardCursorLeftHandle:
      return getImageCursor(shape, "handle_left.pdf", CGPointMake(12, 14));
    case GHOST_kStandardCursorRightHandle:
      return getImageCursor(shape, "handle_right.pdf", CGPointMake(10, 14));
    case GHOST_kStandardCursorBothHandles:
      return getImageCursor(shape, "handle_both.pdf", CGPointMake(11, 14));
    default:
      return nullptr;
  }
}

void GHOST_WindowCocoa::loadCursor(bool visible, GHOST_TStandardCursor shape) const
{
  static bool systemCursorVisible = true;

  ghost_objc::AutoreleasePool pool;

  if (visible != systemCursorVisible) {
    if (visible) {
      msg<void>(GHOST_CLS(NSCursor), GHOST_SEL(unhide));
      systemCursorVisible = true;
    }
    else {
      msg<void>(GHOST_CLS(NSCursor), GHOST_SEL(hide));
      systemCursorVisible = false;
    }
  }

  NSCursor *cursor = getStandardCursor(shape);
  if (cursor == nullptr) {
    cursor = getStandardCursor(GHOST_kStandardCursorDefault);
  }

  msg<void>(reinterpret_cast<id>(cursor), GHOST_SEL(set));
}

bool GHOST_WindowCocoa::isDialog() const
{
  return m_is_dialog;
}

GHOST_TSuccess GHOST_WindowCocoa::setWindowCursorVisibility(bool visible)
{
  ghost_objc::AutoreleasePool pool;
  if (msg<signed char>(reinterpret_cast<id>(m_window), GHOST_SEL(isVisible))) {
    loadCursor(visible, getCursorShape());
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowCocoa::setWindowCursorGrab(GHOST_TGrabCursorMode mode)
{
  ghost_objc::AutoreleasePool pool;

  if (mode != GHOST_kGrabDisable) {
    /* No need to perform grab without warp as it is always on in OS X. */
    if (mode != GHOST_kGrabNormal) {
      ghost_objc::AutoreleasePool inner;
      m_systemCocoa->getCursorPosition(m_cursorGrabInitPos[0], m_cursorGrabInitPos[1]);
      setCursorGrabAccum(0, 0);

      if (mode == GHOST_kGrabHide) {
        setWindowCursorVisibility(false);
      }

      /* Make window key if it wasn't to get the mouse move events. */
      msg<void>(reinterpret_cast<id>(m_window), GHOST_SEL(makeKeyWindow));
    }
  }
  else {
    if (m_cursorGrab == GHOST_kGrabHide) {
      m_systemCocoa->setCursorPosition(m_cursorGrabInitPos[0], m_cursorGrabInitPos[1]);
      setWindowCursorVisibility(true);
    }

    /* Almost works without but important otherwise the mouse GHOST location
     * can be incorrect on exit. */
    setCursorGrabAccum(0, 0);
    m_cursorGrabBounds.m_l = m_cursorGrabBounds.m_r = -1; /* disable */
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowCocoa::setWindowCursorShape(GHOST_TStandardCursor shape)
{
  ghost_objc::AutoreleasePool pool;
  if (msg<signed char>(reinterpret_cast<id>(m_window), GHOST_SEL(isVisible))) {
    loadCursor(getCursorVisibility(), shape);
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_WindowCocoa::hasCursorShape(GHOST_TStandardCursor shape)
{
  ghost_objc::AutoreleasePool pool;
  const GHOST_TSuccess success = (getStandardCursor(shape)) ? GHOST_kSuccess : GHOST_kFailure;
  return success;
}

/** Reverse the bits in a uint16_t */
static uint16_t uns16ReverseBits(uint16_t shrt)
{
  shrt = ((shrt >> 1) & 0x5555) | ((shrt << 1) & 0xAAAA);
  shrt = ((shrt >> 2) & 0x3333) | ((shrt << 2) & 0xCCCC);
  shrt = ((shrt >> 4) & 0x0F0F) | ((shrt << 4) & 0xF0F0);
  shrt = ((shrt >> 8) & 0x00FF) | ((shrt << 8) & 0xFF00);
  return shrt;
}

GHOST_TSuccess GHOST_WindowCocoa::setWindowCustomCursorShape(
    uint8_t *bitmap, uint8_t *mask, int sizex, int sizey, int hotX, int hotY, bool canInvertColor)
{
  ghost_objc::AutoreleasePool pool;

  if (m_customCursor) {
    ghost_objc::release(reinterpret_cast<id>(m_customCursor));
    m_customCursor = nullptr;
  }

  /* TRAMPA: este selector NO puede ir dentro de `GHOST_SEL(...)` partido en dos lineas.
   * La operacion `#` del preprocesador convierte la secuencia de espacios en UN espacio,
   * asi que el nombre saldria con un espacio en medio y `sel_registerName` registraria un
   * selector que no existe: no da error, y al usarlo la aplicacion muere con
   * «unrecognized selector». Va como cadena literal de una sola pieza. */
  static const SEL sel_initBitmapRep = ghost_objc::sel(
      "initWithBitmapDataPlanes:pixelsWide:pixelsHigh:bitsPerSample:samplesPerPixel:hasAlpha:"
      "isPlanar:colorSpaceName:bytesPerRow:bitsPerPixel:");
  id cursorImageRep = msg<id>(
      msg<id>(GHOST_CLS(NSBitmapImageRep), GHOST_SEL(alloc)),
      sel_initBitmapRep,
      (unsigned char **)nullptr,
      (NSInteger_)sizex,
      (NSInteger_)sizey,
      (NSInteger_)1,
      (NSInteger_)2,
      (signed char)1,
      (signed char)1,
      NSDeviceWhiteColorSpace,
      (NSInteger_)(sizex / 8 + (sizex % 8 > 0 ? 1 : 0)),
      (NSInteger_)1);

  uint16_t *cursorBitmap = (uint16_t *)msg<unsigned char *>(cursorImageRep,
                                                            GHOST_SEL(bitmapData));
  const int nbUns16 = msg<NSInteger_>(cursorImageRep, GHOST_SEL(bytesPerPlane)) / 2;

  for (int y = 0; y < nbUns16; y++) {
#if !defined(__LITTLE_ENDIAN__)
    cursorBitmap[y] = uns16ReverseBits((bitmap[2 * y] << 0) | (bitmap[2 * y + 1] << 8));
    cursorBitmap[nbUns16 + y] = uns16ReverseBits((mask[2 * y] << 0) | (mask[2 * y + 1] << 8));
#else
    cursorBitmap[y] = uns16ReverseBits((bitmap[2 * y + 1] << 0) | (bitmap[2 * y] << 8));
    cursorBitmap[nbUns16 + y] = uns16ReverseBits((mask[2 * y + 1] << 0) | (mask[2 * y] << 8));
#endif

    /* Flip white cursor with black outline to black cursor with white outline
     * to match macOS platform conventions. */
    if (canInvertColor) {
      cursorBitmap[y] = ~cursorBitmap[y];
    }
  }

  const CGSize imSize = {(CGFloat)sizex, (CGFloat)sizey};
  id cursorImage = msg<id>(
      msg<id>(GHOST_CLS(NSImage), GHOST_SEL(alloc)), GHOST_SEL(initWithSize:), imSize);
  msg<void>(cursorImage, GHOST_SEL(addRepresentation:), cursorImageRep);

  const CGPoint hotSpotPoint = {(CGFloat)(hotX), (CGFloat)(hotY)};

  /* Foreground and background color parameter is not handled for now (10.6). */
  m_customCursor = reinterpret_cast<NSCursor *>(
      msg<id>(msg<id>(GHOST_CLS(NSCursor), GHOST_SEL(alloc)),
              GHOST_SEL(initWithImage:hotSpot:),
              cursorImage,
              hotSpotPoint));

  ghost_objc::release(cursorImageRep);
  ghost_objc::release(cursorImage);

  if (msg<signed char>(reinterpret_cast<id>(m_window), GHOST_SEL(isVisible))) {
    loadCursor(getCursorVisibility(), GHOST_kStandardCursorCustom);
  }
  return GHOST_kSuccess;
}

#ifdef WITH_INPUT_IME
void GHOST_WindowCocoa::beginIME(int32_t x, int32_t y, int32_t w, int32_t h, bool completed)
{
  if (m_openGLView) {
    ghost_cocoa_view::begin_ime(reinterpret_cast<id>(m_openGLView), x, y, w, h, completed);
  }
  else {
    ghost_cocoa_view::begin_ime(reinterpret_cast<id>(m_metalView), x, y, w, h, completed);
  }
}

void GHOST_WindowCocoa::endIME()
{
  if (m_openGLView) {
    ghost_cocoa_view::end_ime(reinterpret_cast<id>(m_openGLView));
  }
  else {
    ghost_cocoa_view::end_ime(reinterpret_cast<id>(m_metalView));
  }
}
#endif /* WITH_INPUT_IME */
