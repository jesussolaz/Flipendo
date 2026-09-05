# SPDX-License-Identifier: GPL-2.0-or-later
# Copyright 2006 Blender Foundation. All rights reserved.

set(INC
  .
  ../Common
)

set(INC_SYS
  ../../../intern/moto/include
)

set(SRC
  SG_BBox.cpp
  SG_Controller.cpp
  SG_CullingNode.cpp
  SG_Familly.cpp
  SG_Frustum.cpp
  SG_Node.cpp

  SG_BBox.hpp
  SG_Controller.hpp
  SG_CullingNode.hpp
  SG_DList.hpp
  SG_Familly.hpp
  SG_Frustum.hpp
  SG_Node.hpp
  SG_ParentRelation.hpp
  SG_QList.hpp
)

set(LIB
  PRIVATE bf::blenlib
  ge_common
)

blender_add_lib(ge_scenegraph "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
