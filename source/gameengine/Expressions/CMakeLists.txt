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
  ../Common
  ../SceneGraph
  ../../../intern/termcolor
)

set(INC_SYS
  ../../../intern/moto/include
  ${BOOST_INCLUDE_DIR}
)

set(SRC
  intern/BaseListValue.cpp
  intern/BoolValue.cpp
  intern/ConstExpr.cpp
  intern/EmptyValue.cpp
  intern/ErrorValue.cpp
  intern/Expression.cpp
  intern/FloatValue.cpp
  intern/IdentifierExpr.cpp
  intern/IfExpr.cpp
  intern/InputParser.cpp
  intern/IntValue.cpp
  intern/Operator1Expr.cpp
  intern/Operator2Expr.cpp
  intern/PyObjectPlus.cpp
  intern/StringValue.cpp
  intern/Value.cpp
  intern/ListWrapper.cpp

  EXP_BaseListValue.hpp
  EXP_BoolValue.hpp
  EXP_ConstExpr.hpp
  EXP_EmptyValue.hpp
  EXP_ErrorValue.hpp
  EXP_Expression.hpp
  EXP_FloatValue.hpp
  EXP_IdentifierExpr.hpp
  EXP_IfExpr.hpp
  EXP_InputParser.hpp
  EXP_IntValue.hpp
  EXP_Operator1Expr.hpp
  EXP_Operator2Expr.hpp
  EXP_PyObjectPlus.hpp
  EXP_Python.hpp
  EXP_StringValue.hpp
  EXP_Value.hpp
  EXP_ListWrapper.hpp
)

set(LIB
  PRIVATE bf::blenlib
  PRIVATE bf::extern::fmtlib
  PRIVATE bf::intern::guardedalloc
)

if(WITH_PYTHON)
  list(APPEND SRC
    intern/PythonCallBack.cpp

    EXP_PythonCallBack.hpp
  )
endif()

blender_add_lib(ge_expressions "${SRC}" "${INC}" "${INC_SYS}" "${LIB}")
