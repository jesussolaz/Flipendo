/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 *
 * Definition of GHOST_ContextCGL class.
 *
 * C++ PURO. Era `GHOST_ContextCGL.mm`. Es el puente entre GHOST y Metal: crea la capa
 * de dibujo, la cadena de intercambio y el pipeline de blit, y presenta cada fotograma.
 *
 * AVISO: aqui NADIE comprueba los selectores. Este fichero no lo toca el render en
 * `--background` (no hay ventana ni contexto), asi que su verificacion es el EDITOR en
 * modo grafico y el PLAYER: si algo esta mal, o no se ve nada, o se ve mal, o revienta.
 * Las constantes de Metal y AppKit escritas a mano estan comprobadas una a una con una
 * sonda `.mm` que hace `static_assert` contra el SDK (ver informe GHOST-1).
 */

#include "GHOST_ContextCGL.hh"
#include "GHOST_ObjCRuntime.hh"

/* CoreGraphics y CoreFoundation son C puro: se pueden incluir desde un `.cc`. De aqui
 * salen CGRect / CGSize, el espacio de color y sus funciones, tal cual las usaba el
 * original. */
#include <CoreGraphics/CoreGraphics.h>

#include <cassert>
#include <cstdio>
#include <vector>

using ghost_objc::msg;

/* `MTLCreateSystemDefaultDevice` es una FUNCION DE C de Metal, no un metodo: se llama
 * directamente, sin `objc_msgSend`. Fuera del espacio de nombres anonimo a proposito:
 * una declaracion con enlace de C no debe quedar dentro de uno. */
extern "C" id MTLCreateSystemDefaultDevice(void);

namespace {

/* -------------------------------------------------------------------------
 * Constantes de Metal, QuartzCore y AppKit.
 *
 * Comprobadas con `static_assert` contra el SDK 26.5. Un valor equivocado aqui no da
 * ningun aviso: da un formato de pixel distinto o un modo de almacenamiento
 * incorrecto, y eso se ve como basura en pantalla o como nada en absoluto.
 */
using NSUInteger_ = unsigned long;
using NSInteger_ = long;

constexpr NSUInteger_ kMTLPixelFormatRGBA16Float = 115;
constexpr NSUInteger_ kMTLStorageModePrivate = 2;
constexpr NSUInteger_ kMTLTextureUsageShaderRead = 0x0001;
constexpr NSUInteger_ kMTLTextureUsageRenderTarget = 0x0004;
constexpr NSUInteger_ kMTLLoadActionClear = 2;
constexpr NSUInteger_ kMTLStoreActionStore = 1;
constexpr NSUInteger_ kMTLBlendFactorSourceAlpha = 4;
constexpr NSUInteger_ kMTLBlendFactorOneMinusSourceAlpha = 5;
constexpr NSUInteger_ kMTLLanguageVersion1_1 = (1 << 16) + 1;
constexpr NSInteger_ kNSAlertStyleCritical = 2;

/** `MTLClearColor` del SDK: cuatro `double`, 32 bytes, se pasa POR VALOR. */
struct MTLClearColor_ {
  double red, green, blue, alpha;
};

const NSUInteger_ METAL_FRAMEBUFFERPIXEL_FORMAT_EDR = kMTLPixelFormatRGBA16Float;

void ghost_fatal_error_dialog(const char *msg_text)
{
  {
    ghost_objc::AutoreleasePool pool;

    /* Era `[NSString stringWithFormat:@"Error opening window:\n%s", msg]`. Un metodo
     * variadico a traves de `objc_msgSend` es posible pero fragil (la ABI de varargs
     * no se puede describir con una firma fija de forma portable), asi que se formatea
     * con `snprintf` y se crea la cadena ya hecha. El texto que ve el usuario es
     * exactamente el mismo. */
    char buf[1024];
    snprintf(buf, sizeof(buf), "Error opening window:\n%s", msg_text);
    id message = ghost_objc::nsstring(buf);

    id alert = ghost_objc::alloc_init("NSAlert");

    msg<void>(alert, GHOST_SEL(setMessageText:), ghost_objc::nsstring("Blender"));
    msg<void>(alert, GHOST_SEL(setInformativeText:), message);
    msg<void>(alert, GHOST_SEL(setAlertStyle:), kNSAlertStyleCritical);

    msg<id>(alert, GHOST_SEL(addButtonWithTitle:), ghost_objc::nsstring("Quit"));
    msg<NSInteger_>(alert, GHOST_SEL(runModal));
  }

  exit(1);
}

}  // namespace

GHOST_MTLCommandQueuePtr GHOST_ContextCGL::s_sharedMetalCommandQueue = nullptr;
int GHOST_ContextCGL::s_sharedCount = 0;

GHOST_ContextCGL::GHOST_ContextCGL(bool stereoVisual,
                                   NSView *metalView,
                                   CAMetalLayer *metalLayer,
                                   int debug)
    : GHOST_Context(stereoVisual),
      m_metalView(metalView),
      m_metalLayer(metalLayer),
      m_metalRenderPipeline(nullptr),
      m_debug(debug)
{
  ghost_objc::AutoreleasePool pool;

  /* Initialize Metal Swap-chain. */
  current_swapchain_index = 0;
  for (int i = 0; i < METAL_SWAPCHAIN_SIZE; i++) {
    m_defaultFramebufferMetalTexture[i].texture = nullptr;
    m_defaultFramebufferMetalTexture[i].index = i;
  }

  if (m_metalView) {
    m_ownsMetalDevice = false;
    metalInit();
  }
  else {
    /* Prepare offscreen GHOST Context Metal device. */
    id metalDevice = MTLCreateSystemDefaultDevice();

    if (m_debug) {
      printf("Selected Metal Device: %s\n",
             ghost_objc::utf8_string(msg<id>(metalDevice, GHOST_SEL(name))));
    }

    m_ownsMetalDevice = true;
    if (metalDevice) {
      id layer = ghost_objc::alloc_init("CAMetalLayer");
      m_metalLayer = reinterpret_cast<CAMetalLayer *>(layer);
      msg<void>(layer, GHOST_SEL(setEdgeAntialiasingMask:), (unsigned int)0);
      msg<void>(layer, GHOST_SEL(setMasksToBounds:), (signed char)0);
      msg<void>(layer, GHOST_SEL(setOpaque:), (signed char)1);
      msg<void>(layer, GHOST_SEL(setFramebufferOnly:), (signed char)1);
      msg<void>(layer, GHOST_SEL(setPresentsWithTransaction:), (signed char)0);
      msg<void>(layer, GHOST_SEL(removeAllAnimations));
      msg<void>(layer, GHOST_SEL(setDevice:), metalDevice);
      msg<void>(layer, GHOST_SEL(setAllowsNextDrawableTimeout:), (signed char)0);

      /* Enable EDR support. This is done by:
       * 1. Using a floating point render target, so that values outside 0..1 can be used
       * 2. Informing the OS that we are EDR aware, and intend to use values outside 0..1
       * 3. Setting the extended sRGB color space so that the OS knows how to interpret the
       *    values.
       */
      msg<void>(layer, GHOST_SEL(setWantsExtendedDynamicRangeContent:), (signed char)1);
      msg<void>(layer, GHOST_SEL(setPixelFormat:), METAL_FRAMEBUFFERPIXEL_FORMAT_EDR);
      const CFStringRef name = kCGColorSpaceExtendedSRGB;
      CGColorSpaceRef colorspace = CGColorSpaceCreateWithName(name);
      msg<void>(layer, GHOST_SEL(setColorspace:), colorspace);
      CGColorSpaceRelease(colorspace);

      metalInit();
    }
    else {
      ghost_fatal_error_dialog(
          "[ERROR] Failed to create Metal device for offscreen GHOST Context.\n");
    }
  }

  /* Initialize swap-interval. */
  mtl_SwapInterval = 60;
}

GHOST_ContextCGL::~GHOST_ContextCGL()
{
  metalFree();

  if (m_ownsMetalDevice) {
    if (m_metalLayer) {
      ghost_objc::release(reinterpret_cast<id>(m_metalLayer));
      m_metalLayer = nullptr;
    }
  }
  assert(s_sharedCount);

  s_sharedCount--;
  /* Era `[s_sharedMetalCommandQueue release]` SIN comprobar el nulo, que en
   * Objective-C es un no-op legal. `ghost_objc::release` conserva esa tolerancia a
   * proposito: sin ella, el cambio de idioma convierte un no-op silencioso en un
   * fallo de segmentacion (trampa 6 de la politica OBJC-A-CPP). */
  ghost_objc::release(reinterpret_cast<id>(s_sharedMetalCommandQueue));
  if (s_sharedCount == 0) {
    s_sharedMetalCommandQueue = nullptr;
  }
}

GHOST_TSuccess GHOST_ContextCGL::swapBuffers()
{
  if (m_metalView) {
    metalSwapBuffers();
  }
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextCGL::setSwapInterval(int interval)
{
  mtl_SwapInterval = interval;
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextCGL::getSwapInterval(int &intervalOut)
{
  intervalOut = mtl_SwapInterval;
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextCGL::activateDrawingContext()
{
  active_context_ = this;
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextCGL::releaseDrawingContext()
{
  active_context_ = nullptr;
  return GHOST_kSuccess;
}

unsigned int GHOST_ContextCGL::getDefaultFramebuffer()
{
  /* NOTE(Metal): This is not valid. */
  return 0;
}

GHOST_TSuccess GHOST_ContextCGL::updateDrawingContext()
{
  if (m_metalView) {
    metalUpdateFramebuffer();
    return GHOST_kSuccess;
  }
  return GHOST_kFailure;
}

GHOST_MTLTexturePtr GHOST_ContextCGL::metalOverlayTexture()
{
  /* Increment Swap-chain - Only needed if context is requesting a new texture */
  current_swapchain_index = (current_swapchain_index + 1) % METAL_SWAPCHAIN_SIZE;

  /* Ensure backing texture is ready for current swapchain index */
  updateDrawingContext();

  /* Return texture. */
  return m_defaultFramebufferMetalTexture[current_swapchain_index].texture;
}

GHOST_MTLCommandQueuePtr GHOST_ContextCGL::metalCommandQueue()
{
  return s_sharedMetalCommandQueue;
}

GHOST_MTLDevicePtr GHOST_ContextCGL::metalDevice()
{
  return reinterpret_cast<GHOST_MTLDevicePtr>(
      msg<id>(reinterpret_cast<id>(m_metalLayer), GHOST_SEL(device)));
}

void GHOST_ContextCGL::metalRegisterPresentCallback(GHOST_MetalPresentCallback callback)
{
  this->contextPresentCallback = callback;
}

GHOST_TSuccess GHOST_ContextCGL::initializeDrawingContext()
{
  {
    ghost_objc::AutoreleasePool pool;
    if (m_metalView) {
      metalInitFramebuffer();
    }
  }
  active_context_ = this;
  return GHOST_kSuccess;
}

GHOST_TSuccess GHOST_ContextCGL::releaseNativeHandles()
{
  m_metalView = nullptr;

  return GHOST_kSuccess;
}

void GHOST_ContextCGL::metalInit()
{
  ghost_objc::AutoreleasePool pool;

  id device = msg<id>(reinterpret_cast<id>(m_metalLayer), GHOST_SEL(device));

  /* Create a command queue for blit/present operation.
   * NOTE: All context should share a single command queue
   * to ensure correct ordering of work submitted from multiple contexts. */
  if (s_sharedMetalCommandQueue == nullptr) {
    s_sharedMetalCommandQueue = reinterpret_cast<GHOST_MTLCommandQueuePtr>(
        msg<id>(device,
                GHOST_SEL(newCommandQueueWithMaxCommandBufferCount:),
                NSUInteger_(GHOST_ContextCGL::max_command_buffer_count)));
  }
  /* Ensure active GHOSTContext retains a reference to the shared context. */
  ghost_objc::retain(reinterpret_cast<id>(s_sharedMetalCommandQueue));
  s_sharedCount++;

  /* Create shaders for blit operation.
   * El original usaba un literal `@R"msl(...)"`. En C++ es el mismo texto en un literal
   * crudo normal, convertido a NSString donde se usa: el MSL que compila Metal es
   * byte a byte el mismo. */
  static const char *blit_source = R"msl(
      using namespace metal;

      struct Vertex {
        float4 position [[position]];
        float2 texCoord [[attribute(0)]];
      };

      vertex Vertex vertex_shader(uint v_id [[vertex_id]]) {
        Vertex vtx;

        vtx.position.x = float(v_id & 1) * 4.0 - 1.0;
        vtx.position.y = float(v_id >> 1) * 4.0 - 1.0;
        vtx.position.z = 0.0;
        vtx.position.w = 1.0;

        vtx.texCoord = vtx.position.xy * 0.5 + 0.5;

        return vtx;
      }

      constexpr sampler s {};

      fragment float4 fragment_shader(Vertex v [[stage_in]],
                      texture2d<float> t [[texture(0)]]) {

        /* Final blit should ensure alpha is 1.0. This resolves
         * rendering artifacts for blitting of final back-buffer. */
        float4 out_tex = t.sample(s, v.texCoord);
        out_tex.a = 1.0;
        return out_tex;
      }
    )msl";
  id source = ghost_objc::nsstring(blit_source);

  id options = ghost_objc::autorelease(ghost_objc::alloc_init("MTLCompileOptions"));
  msg<void>(options, GHOST_SEL(setLanguageVersion:), kMTLLanguageVersion1_1);

  id error = nullptr;
  id library = msg<id>(device,
                       GHOST_SEL(newLibraryWithSource:options:error:),
                       source,
                       options,
                       &error);
  if (error) {
    ghost_fatal_error_dialog(
        "GHOST_ContextCGL::metalInit: newLibraryWithSource:options:error: failed!");
  }

  /* Create a render pipeline for blit operation. */
  id desc = ghost_objc::autorelease(ghost_objc::alloc_init("MTLRenderPipelineDescriptor"));

  id fragment_fn = msg<id>(
      library, GHOST_SEL(newFunctionWithName:), ghost_objc::nsstring("fragment_shader"));
  id vertex_fn = msg<id>(
      library, GHOST_SEL(newFunctionWithName:), ghost_objc::nsstring("vertex_shader"));
  msg<void>(desc, GHOST_SEL(setFragmentFunction:), fragment_fn);
  msg<void>(desc, GHOST_SEL(setVertexFunction:), vertex_fn);

  /* `desc.colorAttachments[0]` en Objective-C es
   * `[[desc colorAttachments] objectAtIndexedSubscript:0]`: el subindice de un array
   * de descriptores NO es un array de C. */
  id color_attachments = msg<id>(desc, GHOST_SEL(colorAttachments));
  id attachment0 = msg<id>(color_attachments, GHOST_SEL(objectAtIndexedSubscript:), NSUInteger_(0));
  msg<void>(attachment0, GHOST_SEL(setPixelFormat:), METAL_FRAMEBUFFERPIXEL_FORMAT_EDR);

  /* Ensure library is released. */
  ghost_objc::autorelease(library);

  m_metalRenderPipeline = msg<id>(
      device, GHOST_SEL(newRenderPipelineStateWithDescriptor:error:), desc, &error);
  if (error) {
    ghost_fatal_error_dialog(
        "GHOST_ContextCGL::metalInit: newRenderPipelineStateWithDescriptor:error: failed!");
  }

  /* Create a render pipeline to composite things rendered with Metal on top
   * of the frame-buffer contents. Uses the same vertex and fragment shader
   * as the blit above, but with alpha blending enabled. */
  msg<void>(desc, GHOST_SEL(setLabel:), ghost_objc::nsstring("Metal Overlay"));
  msg<void>(attachment0, GHOST_SEL(setBlendingEnabled:), (signed char)1);
  msg<void>(attachment0, GHOST_SEL(setSourceRGBBlendFactor:), kMTLBlendFactorSourceAlpha);
  msg<void>(
      attachment0, GHOST_SEL(setDestinationRGBBlendFactor:), kMTLBlendFactorOneMinusSourceAlpha);

  if (error) {
    ghost_fatal_error_dialog(
        "GHOST_ContextCGL::metalInit: newRenderPipelineStateWithDescriptor:error: failed (when "
        "creating the Metal overlay pipeline)!");
  }

  ghost_objc::release(fragment_fn);
  ghost_objc::release(vertex_fn);
}

void GHOST_ContextCGL::metalFree()
{
  if (m_metalRenderPipeline) {
    ghost_objc::release(m_metalRenderPipeline);
    m_metalRenderPipeline = nullptr;
  }

  for (int i = 0; i < METAL_SWAPCHAIN_SIZE; i++) {
    if (m_defaultFramebufferMetalTexture[i].texture) {
      ghost_objc::release(reinterpret_cast<id>(m_defaultFramebufferMetalTexture[i].texture));
      m_defaultFramebufferMetalTexture[i].texture = nullptr;
    }
  }
}

void GHOST_ContextCGL::metalInitFramebuffer()
{
  updateDrawingContext();
}

void GHOST_ContextCGL::metalUpdateFramebuffer()
{
  ghost_objc::AutoreleasePool pool;

  /* `bounds` devuelve un `NSRect`: 32 bytes, o sea que va por `objc_msgSend_stret`.
   * `convertSizeToBacking:` devuelve y recibe un `NSSize`: 16 bytes, envio normal.
   * `msg<>` lo reparte solo por el tamano del tipo de retorno; equivocarse aqui no da
   * error de compilacion, da numeros basura y una ventana negra. */
  const CGRect bounds = msg<CGRect>(reinterpret_cast<id>(m_metalView), GHOST_SEL(bounds));
  const CGSize backingSize = msg<CGSize>(
      reinterpret_cast<id>(m_metalView), GHOST_SEL(convertSizeToBacking:), bounds.size);
  const size_t width = size_t(backingSize.width);
  const size_t height = size_t(backingSize.height);

  id current_texture = reinterpret_cast<id>(
      m_defaultFramebufferMetalTexture[current_swapchain_index].texture);
  if (current_texture && msg<NSUInteger_>(current_texture, GHOST_SEL(width)) == width &&
      msg<NSUInteger_>(current_texture, GHOST_SEL(height)) == height)
  {
    return;
  }

  /* Free old texture */
  ghost_objc::release(current_texture);

  id device = msg<id>(reinterpret_cast<id>(m_metalLayer), GHOST_SEL(device));
  id overlayDesc = msg<id>(GHOST_CLS(MTLTextureDescriptor),
                           GHOST_SEL(texture2DDescriptorWithPixelFormat:width:height:mipmapped:),
                           METAL_FRAMEBUFFERPIXEL_FORMAT_EDR,
                           NSUInteger_(width),
                           NSUInteger_(height),
                           (signed char)0);
  msg<void>(overlayDesc, GHOST_SEL(setStorageMode:), kMTLStorageModePrivate);
  msg<void>(overlayDesc,
            GHOST_SEL(setUsage:),
            NSUInteger_(kMTLTextureUsageRenderTarget | kMTLTextureUsageShaderRead));

  id overlayTex = msg<id>(device, GHOST_SEL(newTextureWithDescriptor:), overlayDesc);
  if (!overlayTex) {
    ghost_fatal_error_dialog(
        "GHOST_ContextCGL::metalUpdateFramebuffer: failed to create Metal overlay texture!");
  }
  else {
    char label[128];
    snprintf(label, sizeof(label), "Metal Overlay for GHOST Context %p", (void *)this);
    msg<void>(overlayTex, GHOST_SEL(setLabel:), ghost_objc::nsstring(label));
  }

  m_defaultFramebufferMetalTexture[current_swapchain_index].texture =
      reinterpret_cast<GHOST_MTLTexturePtr>(overlayTex);

  /* Clear texture on create */
  id cmdBuffer = msg<id>(reinterpret_cast<id>(s_sharedMetalCommandQueue),
                         GHOST_SEL(commandBuffer));
  id passDescriptor = msg<id>(GHOST_CLS(MTLRenderPassDescriptor), GHOST_SEL(renderPassDescriptor));
  {
    id attachment = msg<id>(msg<id>(passDescriptor, GHOST_SEL(colorAttachments)),
                            GHOST_SEL(objectAtIndexedSubscript:),
                            NSUInteger_(0));
    msg<void>(attachment,
              GHOST_SEL(setTexture:),
              reinterpret_cast<id>(
                  m_defaultFramebufferMetalTexture[current_swapchain_index].texture));
    msg<void>(attachment, GHOST_SEL(setLoadAction:), kMTLLoadActionClear);
    /* `MTLClearColor` son 32 bytes que se pasan POR VALOR. Al castear msgSend a la
     * firma exacta, el compilador genera el paso por pila que manda la ABI. */
    const MTLClearColor_ clear_color{0.294, 0.294, 0.294, 1.000};
    msg<void>(attachment, GHOST_SEL(setClearColor:), clear_color);
    msg<void>(attachment, GHOST_SEL(setStoreAction:), kMTLStoreActionStore);
  }
  {
    id enc = msg<id>(cmdBuffer, GHOST_SEL(renderCommandEncoderWithDescriptor:), passDescriptor);
    msg<void>(enc, GHOST_SEL(endEncoding));
  }
  msg<void>(cmdBuffer, GHOST_SEL(commit));

  msg<void>(reinterpret_cast<id>(m_metalLayer),
            GHOST_SEL(setDrawableSize:),
            CGSizeMake(CGFloat(width), CGFloat(height)));
}

void GHOST_ContextCGL::metalSwapBuffers()
{
  ghost_objc::AutoreleasePool pool;

  updateDrawingContext();

  id drawable = msg<id>(reinterpret_cast<id>(m_metalLayer), GHOST_SEL(nextDrawable));
  if (!drawable) {
    return;
  }

  id passDescriptor = msg<id>(GHOST_CLS(MTLRenderPassDescriptor), GHOST_SEL(renderPassDescriptor));
  {
    id attachment = msg<id>(msg<id>(passDescriptor, GHOST_SEL(colorAttachments)),
                            GHOST_SEL(objectAtIndexedSubscript:),
                            NSUInteger_(0));
    msg<void>(attachment, GHOST_SEL(setTexture:), msg<id>(drawable, GHOST_SEL(texture)));
    msg<void>(attachment, GHOST_SEL(setLoadAction:), kMTLLoadActionClear);
    const MTLClearColor_ clear_color{1.0, 0.294, 0.294, 1.000};
    msg<void>(attachment, GHOST_SEL(setClearColor:), clear_color);
    msg<void>(attachment, GHOST_SEL(setStoreAction:), kMTLStoreActionStore);
  }

  assert(contextPresentCallback);
  assert(m_defaultFramebufferMetalTexture[current_swapchain_index].texture != nullptr);
  (*contextPresentCallback)(
      passDescriptor,
      m_metalRenderPipeline,
      reinterpret_cast<id>(m_defaultFramebufferMetalTexture[current_swapchain_index].texture),
      drawable);
}
