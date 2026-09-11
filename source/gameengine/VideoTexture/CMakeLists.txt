# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2006 Blender Foundation. All rights reserved.

set(INC
  .
  ../BlenderRoutines
  ../Common
  ../Device
  ../Expressions
  ../Flipendo
  ../GameLogic
  ../Ketsji
  ../Rasterizer
  ../Rasterizer/RAS_OpenGLRasterizer
  ../SceneGraph
  ../../blender/editors/include
  ../../blender/gpu/intern
  ../../blender/imbuf/movie/intern
  ../../blender/makesrna
  ../../../intern/decklink
  ../../../intern/gpudirect
  ../../../intern/atomic
)

if(WITH_PYTHON)
  list(APPEND INC
    ../../blender/python
    ../../blender/python/generic
  )
endif()

set(INC_SYS
  ../../../intern/moto/include
  ${Epoxy_INCLUDE_DIRS}
  ${BOOST_INCLUDE_DIR}
)

add_definitions(${GL_DEFINITIONS})

# NUCLEO: C++ puro, se construye SIEMPRE (tambien en el Player sin CPython).
# Es el render-a-textura de Flipendo: ImageRender / ImageViewport / ImageBase /
# Filter* / Texture, con el API escrito en terminos de ImageBase * en vez de
# PyImage *. Ver politicas/PLAYER-SIN-CPYTHON.md §3.1.
set(SRC
  Exception.cpp
  FilterBase.cpp
  FilterBlueScreen.cpp
  FilterColor.cpp
  FilterNormal.cpp
  ImageBase.cpp
  ImageBuff.cpp
  ImageMix.cpp
  ImageRender.cpp
  ImageViewport.cpp
  Texture.cpp

  Common.hpp
  Exception.hpp
  FilterBase.hpp
  FilterBlueScreen.hpp
  FilterColor.hpp
  FilterNormal.hpp
  FilterSource.hpp
  ImageBase.hpp
  ImageBuff.hpp
  ImageMix.hpp
  ImageRender.hpp
  ImageViewport.hpp
  Texture.hpp
)

# ENLACES DE PYTHON: el modulo bge.texture y todo lo que solo existe para el.
# PyTypeList es la lista de tipos de Python; blendVideoTex es el modulo;
# FilterSource.cpp son solo PyTypeObject (las clases viven en FilterSource.hpp).
# DECKLINK y FFMPEG siguen aqui: su logica todavia esta trenzada con la capa de
# enlace y es la deuda que queda escrita en la politica.
if(WITH_PYTHON)
  list(APPEND SRC
    FilterSource.cpp
    PyTypeList.cpp
    blendVideoTex.cpp
    DeckLink.cpp
    VideoBase.cpp
    VideoFFmpeg.cpp
    VideoDeckLink.cpp

    PyTypeList.hpp
    DeckLink.hpp
    VideoBase.hpp
    VideoFFmpeg.hpp
    VideoDeckLink.hpp
  )
endif()

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blentranslation
  PRIVATE bf::bmesh
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::draw
  PRIVATE bf::gpu
  PRIVATE bf::imbuf
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::render
  ge_scenegraph
)

if(MSVC)
  add_definitions(
    /DNOMINMAX
  )
endif()

if(WITH_GAMEENGINE_DECKLINK)
  add_definitions(-DWITH_GAMEENGINE_DECKLINK)

  list(APPEND LIB
    bf_intern_decklink
  )
endif()

if(WIN32)
    list(APPEND LIB
      bf_intern_gpudirect
    )
endif()

if(WITH_CODEC_FFMPEG)
  list(APPEND INC_SYS
    ${FFMPEG_INCLUDE_DIRS}
    ${PTHREADS_INCLUDE_DIRS}
  )
  add_definitions(-DWITH_FFMPEG)
endif()

blender_add_lib(ge_videotexture "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
