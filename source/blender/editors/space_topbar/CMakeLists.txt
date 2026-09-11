# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../interface
  ../include
  ../../makesrna

  # RNA_prototypes.hh
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  fl_topbar_menus.cc
  space_topbar.cc

  FL_topbar_menus.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blenloader
  PRIVATE bf::blentranslation
  PRIVATE bf::dna
  PRIVATE bf::gpu
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::windowmanager
)


# `TOPBAR_MT_file_export` consulta las mismas banderas que `bpy.app.build_options`,
# que no son mas que estos `#ifdef`. Sin esto, sus filas desaparecerian del menu en
# silencio (y el volcado de dibujo lo cantaria).
if(WITH_ALEMBIC)
  add_definitions(-DWITH_ALEMBIC)
endif()
if(WITH_USD)
  add_definitions(-DWITH_USD)
endif()
if(WITH_IO_GREASE_PENCIL)
  add_definitions(-DWITH_IO_GREASE_PENCIL)
endif()
if(WITH_PUGIXML)
  add_definitions(-DWITH_PUGIXML)
endif()
if(WITH_HARU)
  add_definitions(-DWITH_HARU)
endif()
if(WITH_IO_WAVEFRONT_OBJ)
  add_definitions(-DWITH_IO_WAVEFRONT_OBJ)
endif()
if(WITH_IO_PLY)
  add_definitions(-DWITH_IO_PLY)
endif()
if(WITH_IO_STL)
  add_definitions(-DWITH_IO_STL)
endif()
# Ojo: la opcion se llama WITH_OPENCOLLADA y el macro WITH_COLLADA. Escribir
# `if(WITH_COLLADA)` compila sin quejarse y deja la fila de Collada fuera del menu en
# silencio; lo canto el volcado de dibujo a la primera.
if(WITH_OPENCOLLADA)
  add_definitions(-DWITH_COLLADA)
endif()

blender_add_lib(bf_editor_space_topbar "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")


# RNA_prototypes.hh dna_type_offsets.h
add_dependencies(bf_editor_space_topbar bf_rna)
