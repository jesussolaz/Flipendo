# SPDX-FileCopyrightText: 2026 Flipendo
#
# SPDX-License-Identifier: GPL-2.0-or-later

# Codigo del MOTOR dentro del editor: publicar el juego, exportarlo como
# ejecutable autonomo, y las herramientas de escena que UPBGE traia como addons
# de Python. Ver politicas/ADDONS-MOTOR-A-CPP.md.

set(INC
  ../include
  ../../makesrna

  # dna_type_offsets.h
  ${CMAKE_CURRENT_BINARY_DIR}/../../makesdna/intern
  # RNA_prototypes.hh
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
  ${ZLIB_INCLUDE_DIRS}
)

set(SRC
  fl_game_publishing.cc
  fl_game_runtime.cc

  ../include/FL_game_runtime.hh
)

set(LIB
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blenloader
  PRIVATE bf::blentranslation
  PRIVATE bf::dna
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::windowmanager

  ${ZLIB_LIBRARIES}
)

blender_add_lib(bf_editor_flipendo "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.hh
add_dependencies(bf_editor_flipendo bf_rna)
