/* SPDX-FileCopyrightText: 2014 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup GHOST
 *
 * Esta cabecera es LEGIBLE DESDE C++ PURO. No incluye Cocoa, ni Metal, ni QuartzCore,
 * y no tiene sintaxis de Objective-C: los tipos del sistema llegan como
 * declaraciones opacas desde `GHOST_ObjCCompat.hh`, que las escribe con el mismo
 * nombre en los dos modos para que los punteros manglen igual (ver la explicacion y
 * las medidas alli).
 *
 * Antes incluia `<Cocoa/Cocoa.h>` y eso impedia que `mtl_context` y
 * `mtl_command_buffer` pudieran ser `.cc`, y con ellos `mtl_texture`, `mtl_state` y
 * `mtl_storage_buffer`. NO VOLVER A METER UNA CABECERA DEL SDK AQUI.
 */

#pragma once

#include "GHOST_Context.hh"
#include "GHOST_ObjCCompat.hh"

class GHOST_ContextCGL : public GHOST_Context {

 public:
  /* Defines the number of simultaneous command buffers which can be in flight.
   * The default limit of `64` is considered to be optimal for Blender. Too many command buffers
   * will result in workload fragmentation and additional system-level overhead. This limit should
   * also only be increased if the application is consistently exceeding the limit, and there are
   * no command buffer leaks.
   *
   * If this limit is reached, starting a new command buffer will fail. The Metal back-end will
   * therefore stall until completion and log a warning when this limit is reached in order to
   * ensure correct function of the app.
   *
   * It is generally preferable to reduce the prevalence of GPU_flush or GPU Context switches
   * (which will both break command submissions), rather than increasing this limit. */
  static const int max_command_buffer_count = 64;

  /**
   * Tipo del callback de presentacion. Lo registra el backend de Metal
   * (`blender::gpu::present`) y lo invoca `metalSwapBuffers()`.
   *
   * HISTORIA, porque explica por que esto vuelve a estar tipado. Esta firma fue la
   * UNICA de GHOST_ContextCGL que cruzaba la frontera entre Objective-C++ y C++ puro, y
   * en un PARAMETRO la grafia entra en el simbolo mangleado:
   *
   *     id<MTLTexture>            ->  PU21objcproto10MTLTexture11objc_object
   *     MTL::Texture *            ->  PN3MTL7TextureE
   *     id                        ->  P11objc_object      (en los DOS modos)
   *
   * Mientras uno de los dos lados fue `.mm`, cualquiera de las dos grafias tipadas
   * compilaba en los dos sitios y NO ENLAZABA, asi que hubo que dejar los cuatro
   * parametros en `id` pelado. Eso costaba cuatro `reinterpret_cast` en
   * `blender::gpu::present`, que eran el unico sitio del backend grafico donde el
   * compilador no comprobaba nada.
   *
   * Ya no hace falta: **ningun `.mm` lee esta cabecera** (los siete que la incluyen son
   * `.cc`), asi que la frontera no existe y los tipos vuelven. Los cuatro casteos
   * desaparecen y el compilador vuelve a comprobar la firma del callback.
   *
   * Los tipos se DECLARAN (en GHOST_ObjCCompat.hh), no se incluye metal-cpp: asi
   * `bf_intern_ghost` no adquiere una dependencia de los simbolos de `NS::Private` que
   * materializa `bf_gpu`.
   */
  using GHOST_MetalPresentCallback = void (*)(MTL::RenderPassDescriptor *blit_descriptor,
                                              MTL::RenderPipelineState *blit_pso,
                                              MTL::Texture *swapchain_texture,
                                              CA::MetalDrawable *drawable);

 public:
  /**
   * Constructor.
   */
  GHOST_ContextCGL(bool stereoVisual, NSView *metalView, CAMetalLayer *metalLayer, int debug);

  /**
   * Destructor.
   */
  ~GHOST_ContextCGL() override;

  /**
   * Swaps front and back buffers of a window.
   * \return A boolean success indicator.
   */
  GHOST_TSuccess swapBuffers() override;

  /**
   * Activates the drawing context of this window.
   * \return A boolean success indicator.
   */
  GHOST_TSuccess activateDrawingContext() override;

  /**
   * Release the drawing context of the calling thread.
   * \return A boolean success indicator.
   */
  GHOST_TSuccess releaseDrawingContext() override;

  unsigned int getDefaultFramebuffer() override;

  /**
   * Call immediately after new to initialize.  If this fails then immediately delete the object.
   * \return Indication as to whether initialization has succeeded.
   */
  GHOST_TSuccess initializeDrawingContext() override;

  /**
   * Removes references to native handles from this context and then returns
   * \return GHOST_kSuccess if it is OK for the parent to release the handles and
   * GHOST_kFailure if releasing the handles will interfere with sharing
   */
  GHOST_TSuccess releaseNativeHandles() override;

  /**
   * Sets the swap interval for #swapBuffers.
   * \param interval: The swap interval to use.
   * \return A boolean success indicator.
   */
  GHOST_TSuccess setSwapInterval(int interval) override;

  /**
   * Gets the current swap interval for #swapBuffers.
   * \param intervalOut: Variable to store the swap interval if it can be read.
   * \return Whether the swap interval can be read.
   */
  GHOST_TSuccess getSwapInterval(int &intervalOut) override;

  /**
   * Updates the drawing context of this window.
   * Needed whenever the window is changed.
   * \return Indication of success.
   */
  GHOST_TSuccess updateDrawingContext() override;

  /**
   * Returns a texture that Metal code can use as a render target. The current
   * contents of this texture will be composited on top of the frame-buffer
   * each time `swapBuffers` is called.
   *
   * El tipo de retorno NO entra en el mangling (ABI de Itanium), asi que aqui si se
   * puede usar el alias de doble modo y conservar `id<MTLTexture>` en los `.mm`.
   */
  GHOST_MTLTexturePtr metalOverlayTexture();

  /**
   * Return a pointer to the Metal command queue used by this context.
   */
  GHOST_MTLCommandQueuePtr metalCommandQueue();

  /**
   * Return a pointer to the Metal device associated with this context.
   */
  GHOST_MTLDevicePtr metalDevice();

  /**
   * Register present callback
   */
  void metalRegisterPresentCallback(GHOST_MetalPresentCallback callback);

 private:
  /** Metal state */
  NSView *m_metalView;
  CAMetalLayer *m_metalLayer;
  /* `id` y no un `MTLRenderPipelineState *`: en modo C++ ese nombre ya lo ocupa el
   * alias `MTLRenderPipelineState = MTL::RenderPipelineState` de mtl_objc_compat.hh. */
  id m_metalRenderPipeline;
  bool m_ownsMetalDevice;

  /** The virtualized default frame-buffer's texture. */
  /**
   * Texture that you can render into with Metal. The texture will be
   * composited on top of `m_defaultFramebufferMetalTexture` whenever
   * `swapBuffers` is called.
   */
  static const int METAL_SWAPCHAIN_SIZE = 3;
  struct MTLSwapchainTexture {
    GHOST_MTLTexturePtr texture;
    unsigned int index;
  };
  MTLSwapchainTexture m_defaultFramebufferMetalTexture[METAL_SWAPCHAIN_SIZE];
  unsigned int current_swapchain_index = 0;

  /* Present callback.
   * We use this such that presentation can be controlled from within the Metal
   * Context. This is required for optimal performance and clean control flow.
   * Also helps ensure flickering does not occur by present being dependent
   * on existing submissions. */
  GHOST_MetalPresentCallback contextPresentCallback;

  int mtl_SwapInterval;
  const bool m_debug;

  static int s_sharedCount;

  /* Single device queue for multiple contexts. */
  static GHOST_MTLCommandQueuePtr s_sharedMetalCommandQueue;

  /* Metal functions */
  void metalInit();
  void metalFree();
  void metalInitFramebuffer();
  void metalUpdateFramebuffer();
  void metalSwapBuffers();
  void initClear(){};
};
