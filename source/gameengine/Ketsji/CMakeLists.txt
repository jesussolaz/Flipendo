# ***** BEGIN GPL LICENSE BLOCK *****
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software Foundation,
# Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# The Original Code is Copyright (C) 2006, Blender Foundation
# All rights reserved.
#
# The Original Code is: all of this file.
#
# Contributor(s): Jacques Beaurain.
#
# ***** END GPL LICENSE BLOCK *****

set(INC
  .
  KXNetwork
  ../BlenderRoutines
  ../Common
  ../Converter
  ../Device
  ../Expressions
  ../GameLogic
  ../Launcher
  ../Physics/Common
  ../Rasterizer
  ../Rasterizer/RAS_OpenGLRasterizer
  ../SceneGraph
  ../../blender
  ../../blender/draw/intern
  ../../blender/editors/include
  ../../blender/makesrna
  ../../blender/python
  ../../blender/python/generic
  ../../blender/python/mathutils
  ../../blender/python/intern
  ../../../intern/ghost
)

set(INC_SYS
  ../../../intern/moto/include
  ../../../extern/recastnavigation/Recast/Include
  ../../../extern/recastnavigation/Detour/Include
  ${PTHREADS_INCLUDE_DIRS}
  ${BOOST_INCLUDE_DIR}
)

set(SRC
  BL_Action.cpp
  BL_ActionManager.cpp
  BL_Shader.cpp
  BL_Texture.cpp
  KX_2DFilter.cpp
  KX_2DFilterManager.cpp
  KX_2DFilterFrameBuffer.cpp
  KX_BlenderCanvas.cpp
  KX_BlenderMaterial.cpp
  KX_Camera.cpp
  KX_CameraIpoSGController.cpp
  KX_CharacterWrapper.cpp
  KX_CollisionEventManager.cpp
  KX_ConstraintWrapper.cpp
  KX_EmptyObject.cpp
  KX_FontObject.cpp
  KX_GameObject.cpp
  KX_Globals.cpp
  KX_IpoController.cpp
  KX_KetsjiEngine.cpp
  KX_LibLoadStatus.cpp
  KX_Light.cpp
  KX_LightIpoSGController.cpp
  KX_LodLevel.cpp
  KX_LodManager.cpp
  KX_MaterialShader.cpp
  KX_MeshProxy.cpp
  KX_MotionState.cpp
  KX_NavMeshObject.cpp
  KX_ObColorIpoSGController.cpp
  KX_ObstacleSimulation.cpp
  KX_PolyProxy.cpp
  KX_PyConstraintBinding.cpp
  KX_PyMath.cpp
  KX_PythonComponent.cpp
  KX_PythonProxyManager.cpp
  KX_PythonInit.cpp
  KX_PythonInitTypes.cpp
  KX_PythonMain.cpp
  KX_PythonProxy.cpp
  KX_RayCast.cpp
  KX_BoneParentNodeRelationship.cpp
  KX_NodeRelationships.cpp
  KX_ScalarInterpolator.cpp
  KX_Scene.cpp
  ../Flipendo/FL_Component.cpp
  ../Flipendo/FL_ArpgComponents.cpp
  ../Flipendo/FL_Component.hpp
  ../Flipendo/FL_ArpgCore.hpp
  KX_TimeCategoryLogger.cpp
  KX_TimeLogger.cpp
  KX_VehicleWrapper.cpp
  KX_VertexProxy.cpp
  KX_CollisionContactPoints.cpp

  BL_Action.hpp
  BL_ActionManager.hpp
  BL_Shader.hpp
  BL_Texture.hpp
  KX_2DFilter.hpp
  KX_2DFilterManager.hpp
  KX_2DFilterFrameBuffer.hpp
  KX_BlenderCanvas.hpp
  KX_BlenderMaterial.hpp
  KX_Camera.hpp
  KX_CameraIpoSGController.hpp
  KX_CharacterWrapper.hpp
  KX_ClientObjectInfo.hpp
  KX_ConstraintWrapper.hpp
  KX_EmptyObject.hpp
  KX_FontObject.hpp
  KX_GameObject.hpp
  KX_Globals.hpp
  KX_IInterpolator.hpp
  KX_IpoTransform.hpp
  KX_IpoController.hpp
  KX_IScalarInterpolator.hpp
  KX_ISystem.hpp
  KX_KetsjiEngine.hpp
  KX_LibLoadStatus.hpp
  KX_Light.hpp
  KX_LightIpoSGController.hpp
  KX_LodLevel.hpp
  KX_LodManager.hpp
  KX_MaterialShader.hpp
  KX_MeshProxy.hpp
  KX_MotionState.hpp
  KX_NavMeshObject.hpp
  KX_ObColorIpoSGController.hpp
  KX_ObstacleSimulation.hpp
  KX_PhysicsEngineEnums.hpp
  KX_PolyProxy.hpp
  KX_PyConstraintBinding.hpp
  KX_PyMath.hpp
  KX_PythonComponent.hpp
  KX_PythonProxyManager.hpp
  KX_PythonInit.hpp
  KX_PythonInitTypes.hpp
  KX_PythonMain.hpp
  KX_PythonProxy.hpp
  KX_RayCast.hpp
  KX_BoneParentNodeRelationship.hpp
  KX_NodeRelationships.hpp
  KX_ScalarInterpolator.hpp
  KX_Scene.hpp
  KX_TimeCategoryLogger.hpp
  KX_TimeLogger.hpp
  KX_CollisionEventManager.hpp
  KX_VehicleWrapper.hpp
  KX_VertexProxy.hpp
  KX_CollisionContactPoints.hpp
)

set(LIB
  PRIVATE bf::animrig
  PRIVATE bf::blenfont
  PRIVATE bf::blenkernel
  PRIVATE bf::blenlib
  PRIVATE bf::blenloader
  PRIVATE bf::blentranslation
  PRIVATE bf::bmesh
  PRIVATE bf::depsgraph
  PRIVATE bf::dna
  PRIVATE bf::draw
  PRIVATE bf::extern::fmtlib
  PRIVATE bf::gpu
  PRIVATE bf::imbuf
  PRIVATE bf::intern::clog
  PRIVATE bf::intern::guardedalloc
  PRIVATE bf::render
  PRIVATE bf::windowmanager
  ge_converter
  ge_logic_bricks
  ge_scenegraph
  ge_msg_network
  ge_device
  extern_recastnavigation
  ge_rasterizer
  ge_videotexture
)

add_definitions(${GL_DEFINITIONS})

if(MSVC)
  add_definitions(
    /DNOMINMAX
  )
endif()

if(WITH_IMAGE_DDS)
  add_definitions(-DWITH_DDS)
endif()

if(WITH_SDL)
  list(APPEND INC_SYS
    ${SDL_INCLUDE_DIR}
  )
  add_definitions(-DWITH_SDL)
endif()

if(WITH_CODEC_FFMPEG)
  add_definitions(-DWITH_FFMPEG)
endif()

if(WITH_BULLET)
  list(APPEND INC
    ../Physics/Bullet
  )
  list(APPEND INC_SYS
    ${BULLET_INCLUDE_DIRS}
  )
  add_definitions(-DWITH_BULLET)
endif()

if(WITH_AUDASPACE)
  list(APPEND INC_SYS
    ${AUDASPACE_C_INCLUDE_DIRS}
  )
  list(APPEND LIB
    ${AUDASPACE_C_LIBRARIES}
    ${AUDASPACE_PY_LIBRARIES}
  )
  add_definitions(-DWITH_AUDASPACE)
endif()

if(WITH_GAMEENGINE)
  add_definitions(-DWITH_GAMEENGINE)
endif()

if(WITH_CYCLES)
  add_definitions(-DWITH_CYCLES)
endif()

if(WITH_MOD_FLUID)
  list(APPEND LIB
    bf_intern_mantaflow
  )
  add_definitions(-DWITH_FLUID)
endif()

if(WITH_XR_OPENXR)
  add_definitions(-DWITH_XR_OPENXR)
endif()

blender_add_lib(ge_ketsji "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
