# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../interface
  ../include
  ../../makesrna

)

set(INC_SYS
)

set(SRC
  fl_text_menus.cc
  space_text.cc
  text_autocomplete.cc
  text_draw.cc
  text_format.cc
  text_format_glsl.cc
  text_format_osl.cc
  text_format_pov.cc
  text_format_pov_ini.cc
  text_format_py.cc
  text_header.cc
  fl_external_editor.cc
  text_ops.cc
  text_undo.cc

  text_format.hh
  FL_text_menus.hh
  text_intern.hh
)

set(LIB
  PRIVATE bf::blenfont
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blenloader
  PRIVATE bf::blentranslation
  PRIVATE bf::dna
  PRIVATE bf::gpu
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::nodes
  PRIVATE bf::render
  PRIVATE bf::windowmanager
)


if(WITH_PYTHON)
  list(APPEND INC
    ../../python
  )
  add_definitions(-DWITH_PYTHON)
endif()


blender_add_lib(bf_editor_space_text "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
