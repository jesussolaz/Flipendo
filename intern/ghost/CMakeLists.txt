# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  PUBLIC .
  ../guardedalloc
)

set(INC_SYS
)

set(SRC
  intern/GHOST_Buttons.cc
  intern/GHOST_C-api.cc
  intern/GHOST_CallbackEventConsumer.cc
  intern/GHOST_Context.cc
  intern/GHOST_ContextNone.cc
  intern/GHOST_EventManager.cc
  intern/GHOST_ISystem.cc
  intern/GHOST_ISystemPaths.cc
  intern/GHOST_ModifierKeys.cc
  intern/GHOST_Path-api.cc
  intern/GHOST_PathUtils.cc
  intern/GHOST_Rect.cc
  intern/GHOST_System.cc
  intern/GHOST_TimerManager.cc
  intern/GHOST_Window.cc
  intern/GHOST_WindowManager.cc

  GHOST_C-api.h
  GHOST_IContext.hh
  GHOST_IEvent.hh
  GHOST_IEventConsumer.hh
  GHOST_ISystem.hh
  GHOST_ISystemPaths.hh
  GHOST_ITimerTask.hh
  GHOST_IWindow.hh
  GHOST_Path-api.hh
  GHOST_Rect.hh
  GHOST_Types.h

  intern/GHOST_Buttons.hh
  intern/GHOST_CallbackEventConsumer.hh
  intern/GHOST_Context.hh
  intern/GHOST_ContextNone.hh
  intern/GHOST_Debug.hh
  intern/GHOST_Event.hh
  intern/GHOST_EventButton.hh
  intern/GHOST_EventCursor.hh
  intern/GHOST_EventDragnDrop.hh
  intern/GHOST_EventKey.hh
  intern/GHOST_EventManager.hh
  intern/GHOST_EventString.hh
  intern/GHOST_EventTrackpad.hh
  intern/GHOST_EventWheel.hh
  intern/GHOST_ModifierKeys.hh
  intern/GHOST_PathUtils.hh
  intern/GHOST_System.hh
  intern/GHOST_SystemPaths.hh
  intern/GHOST_TimerManager.hh
  intern/GHOST_TimerTask.hh
  intern/GHOST_Util.hh
  intern/GHOST_Window.hh
  intern/GHOST_WindowManager.hh
  intern/GHOST_utildefines.hh
  intern/GHOST_utildefines_variadic.hh
)

set(LIB
  PRIVATE bf::blenlib
  PRIVATE bf::dna
  PRIVATE bf::gpu
  PRIVATE bf::imbuf
  PRIVATE bf::intern::clog
)

if(WITH_INPUT_IME)
  add_definitions(-DWITH_INPUT_IME)
endif()

if(WITH_OPENGL_BACKEND)
  list(APPEND INC_SYS
    ${Epoxy_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${Epoxy_LIBRARIES}
  )
  add_definitions(-DWITH_OPENGL_BACKEND)
endif()

if(WITH_VULKAN_BACKEND)
  if(WITH_VULKAN_MOLTENVK)
    list(APPEND INC_SYS
      PUBLIC ${MOLTENVK_INCLUDE_DIRS}
    )

    list(APPEND LIB
      ${MOLTENVK_LIBRARIES}
    )
  endif()

  list(APPEND SRC
    intern/GHOST_ContextVK.cc

    intern/GHOST_ContextVK.hh
  )
  list(APPEND INC
    ../../extern/vulkan_memory_allocator/
  )
  list(APPEND INC_SYS
    PUBLIC ${VULKAN_INCLUDE_DIRS}
  )

  list(APPEND LIB
    ${VULKAN_LIBRARIES}
  )

  add_definitions(-DWITH_VULKAN_BACKEND)
endif()

if(WITH_GHOST_DEBUG)
  list(APPEND SRC
    intern/GHOST_EventPrinter.cc

    intern/GHOST_EventPrinter.hh
  )
  add_definitions(-DWITH_GHOST_DEBUG)
endif()

if(WITH_INPUT_NDOF)
  add_definitions(-DWITH_INPUT_NDOF)

  list(APPEND SRC
    intern/GHOST_NDOFManager.cc

    intern/GHOST_EventNDOF.hh
    intern/GHOST_NDOFManager.hh
  )

  list(APPEND INC_SYS
    ${NDOF_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${NDOF_LIBRARIES}
  )
endif()

list(APPEND SRC
  intern/GHOST_SystemHeadless.hh
  intern/GHOST_WindowNULL.hh
)

if(WITH_HEADLESS)
  add_definitions(-DWITH_HEADLESS)
elseif(WITH_GHOST_SDL)
  list(APPEND SRC
    intern/GHOST_ContextSDL.cc
    intern/GHOST_SystemSDL.cc
    intern/GHOST_WindowSDL.cc

    intern/GHOST_ContextSDL.hh
    intern/GHOST_SystemSDL.hh
    intern/GHOST_WindowSDL.hh
  )
  add_definitions(-DWITH_GHOST_SDL)

  list(APPEND INC_SYS
    ${SDL_INCLUDE_DIR}
  )
  list(APPEND LIB
    ${SDL_LIBRARY}
  )
elseif(APPLE AND NOT WITH_GHOST_X11)
  list(APPEND SRC
    intern/GHOST_SystemCocoa.mm
    intern/GHOST_WindowCocoa.mm

    intern/GHOST_SystemCocoa.hh
    intern/GHOST_WindowCocoa.hh
    intern/GHOST_WindowViewCocoa.hh
  )

  if(WITH_INPUT_NDOF)
    list(APPEND SRC
      intern/GHOST_NDOFManagerCocoa.mm

      intern/GHOST_NDOFManagerCocoa.hh
    )
  endif()

endif()


if(APPLE)
  if(WITH_METAL_BACKEND)
    list(APPEND SRC
      intern/GHOST_ContextCGL.mm

      intern/GHOST_ContextCGL.hh
    )
  endif()
endif()

if(APPLE)
  list(APPEND SRC
    intern/GHOST_SystemPathsCocoa.hh
    intern/GHOST_SystemPathsCocoa.mm
  )

elseif(UNIX)
  list(APPEND SRC
    intern/GHOST_SystemPathsUnix.cc
    intern/GHOST_SystemPathsUnix.hh
  )

  if(NOT WITH_INSTALL_PORTABLE)
    add_definitions(-DPREFIX="${CMAKE_INSTALL_PREFIX}")
  endif()

elseif(WIN32)
  list(APPEND SRC
    intern/GHOST_SystemPathsWin32.cc
    intern/GHOST_SystemPathsWin32.hh
  )

  list(APPEND INC
    ../utfconv
  )

endif()

if(WITH_XR_OPENXR)
  list(APPEND SRC
    intern/GHOST_Xr.cc
    intern/GHOST_XrAction.cc
    intern/GHOST_XrContext.cc
    intern/GHOST_XrControllerModel.cc
    intern/GHOST_XrEvent.cc
    intern/GHOST_XrGraphicsBinding.cc
    intern/GHOST_XrSession.cc
    intern/GHOST_XrSwapchain.cc

    GHOST_IXrContext.hh
    intern/GHOST_IXrGraphicsBinding.hh
    intern/GHOST_XrAction.hh
    intern/GHOST_XrContext.hh
    intern/GHOST_XrControllerModel.hh
    intern/GHOST_XrException.hh
    intern/GHOST_XrSession.hh
    intern/GHOST_XrSwapchain.hh
    intern/GHOST_Xr_intern.hh
    intern/GHOST_Xr_openxr_includes.hh

    # Header only library.
    ../../extern/tinygltf/tiny_gltf.h
  )
  if(WIN32)
    list(APPEND SRC
      intern/GHOST_XrGraphicsBindingD3D.cc

      intern/GHOST_XrGraphicsBindingD3D.hh
    )
  endif()

  list(APPEND INC_SYS
    ../../extern/json/include
    ../../extern/tinygltf
  )
  list(APPEND INC_SYS
    ${XR_OPENXR_SDK_INCLUDE_DIR}
  )
  list(APPEND LIB
    ${XR_OPENXR_SDK_LIBRARIES}
    PRIVATE bf::dependencies::eigen
  )

  set(XR_PLATFORM_DEFINES
  )
  if(WITH_OPENGL_BACKEND)
    list(APPEND XR_PLATFORM_DEFINES -DXR_USE_GRAPHICS_API_OPENGL)
  endif()
  if(WITH_VULKAN_BACKEND)
    list(APPEND XR_PLATFORM_DEFINES -DXR_USE_GRAPHICS_API_VULKAN)
    list(APPEND SRC
      intern/GHOST_XrGraphicsBindingVulkan.cc

      intern/GHOST_XrGraphicsBindingVulkan.hh
    )
  endif()

  # Add compiler defines as required by the OpenXR specification.
  if(WIN32)
    list(APPEND XR_PLATFORM_DEFINES
      -DXR_USE_PLATFORM_WIN32
      -DXR_USE_GRAPHICS_API_D3D11
    )
    list(APPEND LIB
      shlwapi
    )
  elseif(UNIX AND NOT APPLE)
    list(APPEND XR_PLATFORM_DEFINES -DXR_OS_LINUX)
    if(WITH_GHOST_WAYLAND)
      list(APPEND XR_PLATFORM_DEFINES -DXR_USE_PLATFORM_WAYLAND)
    endif()
    if(WITH_GHOST_X11)
      list(APPEND XR_PLATFORM_DEFINES -DXR_USE_PLATFORM_EGL)
      list(APPEND XR_PLATFORM_DEFINES -DXR_USE_PLATFORM_XLIB)
    endif()
  endif()

  add_definitions(-DWITH_XR_OPENXR ${XR_PLATFORM_DEFINES})

  unset(XR_PLATFORM_DEFINES)
endif()

blender_add_lib(bf_intern_ghost "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
