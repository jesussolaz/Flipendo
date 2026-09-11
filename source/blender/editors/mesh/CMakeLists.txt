# SPDX-FileCopyrightText: 2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

set(INC
  ../include
  ../uvedit
  ../../makesrna
  # RNA_prototypes.hh
  ${CMAKE_BINARY_DIR}/source/blender/makesrna
)

set(INC_SYS
)

set(SRC
  editface.cc
  editmesh_add.cc
  editmesh_add_gizmo.cc
  editmesh_attribute.cc
  editmesh_automerge.cc
  editmesh_bevel.cc
  editmesh_bisect.cc
  editmesh_extrude.cc
  editmesh_find_adjacent.cc
  editmesh_extrude_screw.cc
  editmesh_extrude_spin.cc
  editmesh_extrude_spin_gizmo.cc
  editmesh_inset.cc
  editmesh_intersect.cc
  editmesh_knife.cc
  editmesh_knife_project.cc
  editmesh_loopcut.cc
  editmesh_mask_extract.cc
  editmesh_path.cc
  editmesh_polybuild.cc
  editmesh_preselect_edgering.cc
  editmesh_preselect_elem.cc
  editmesh_rip.cc
  editmesh_rip_edge.cc
  editmesh_select.cc
  editmesh_select_similar.cc
  editmesh_set_sharpness_by_angle.cc
  editmesh_tools.cc
  editmesh_undo.cc
  editmesh_utils.cc
  fl_mesh_ops_selftest.cc
  mesh_data.cc
  mesh_faces_mirror_uv.cc
  mesh_mirror.cc
  mesh_ops.cc
  mesh_vertex_dirt.cc
  meshtools.cc
  view3d_edit_mesh_extrude.cc

  mesh_intern.hh
)

set(LIB
  PRIVATE bf::blenfont
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blentranslation
  PRIVATE bf::bmesh
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::draw
  PRIVATE bf::extern::fmtlib
  PRIVATE bf::functions
  PRIVATE bf::geometry
  PRIVATE bf::gpu
  PRIVATE bf::imbuf
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::render
  PRIVATE bf::windowmanager
)

if(WITH_GAMEENGINE)
  add_definitions(-DWITH_GAMEENGINE)
  list(APPEND INC
    ../../../../extern/recastnavigation
  )

  list(APPEND SRC
    mesh_navmesh.cc
  )

  list(APPEND LIB
    extern_recastnavigation
  )
endif()

if(WITH_FREESTYLE)
  add_definitions(-DWITH_FREESTYLE)
endif()

if(WITH_BULLET)
  add_definitions(-DWITH_BULLET)
endif()

if(WITH_GMP)
  add_definitions(-DWITH_GMP)
endif()

blender_add_lib(bf_editor_mesh "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")

# RNA_prototypes.hh
add_dependencies(bf_editor_mesh bf_rna)
