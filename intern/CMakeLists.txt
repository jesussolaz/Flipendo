# SPDX-FileCopyrightText: 2006 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# add_subdirectory(atomic)  # header only
add_subdirectory(atomic)
add_subdirectory(clog)
add_subdirectory(ghost)
add_subdirectory(guardedalloc)
add_subdirectory(libmv)
add_subdirectory(memutil)
add_subdirectory(opensubdiv)
add_subdirectory(mikktspace)
add_subdirectory(eigen)
add_subdirectory(sky)

if(WITH_GAMEENGINE_DECKLINK)
	add_subdirectory(decklink)
endif()

if (WITH_GAMEENGINE_BPPLAYER)
  add_subdirectory(spindle)
endif()

if(WITH_AUDASPACE)
  add_subdirectory(audaspace)
endif()

if(WITH_MOD_REMESH)
  add_subdirectory(dualcon)
endif()

if(WITH_IK_SOLVER)
  add_subdirectory(iksolver)
endif()

if(WITH_IK_ITASC)
  add_subdirectory(itasc)
endif()

if(WITH_GAMEENGINE)
  add_subdirectory(moto)
endif()

if(WITH_CYCLES)
  add_subdirectory(cycles)
endif()

if(WITH_BULLET)
  add_subdirectory(rigidbody)
endif()


if(WITH_MOD_FLUID)
  add_subdirectory(mantaflow)
endif()

if(WITH_UV_SLIM)
  add_subdirectory(slim)
endif()

if(WITH_OPENVDB)
  add_subdirectory(openvdb)
endif()

if(WITH_QUADRIFLOW)
  add_subdirectory(quadriflow)
endif()

if(WITH_RENDERDOC)
  add_subdirectory(renderdoc_dynload)
endif()

