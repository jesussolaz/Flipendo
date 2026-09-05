/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file gameengine/Ketsji/KX_PythonInitTypes.cpp
 *  \ingroup ketsji
 */

#ifdef WITH_PYTHON

#  include "KX_PythonInitTypes.hpp"

/* Only for Class::Parents */
#  include "BL_ArmatureActuator.hpp"
#  include "BL_ArmatureChannel.hpp"
#  include "BL_ArmatureConstraint.hpp"
#  include "BL_ArmatureObject.hpp"
#  include "BL_Shader.hpp"
#  include "BL_Texture.hpp"
#  include "EXP_ListWrapper.hpp"
#  include "KX_2DFilter.hpp"
#  include "KX_2DFilterFrameBuffer.hpp"
#  include "KX_2DFilterManager.hpp"
#  include "KX_BlenderMaterial.hpp"
#  include "KX_Camera.hpp"
#  include "KX_CharacterWrapper.hpp"
#  include "KX_CollisionContactPoints.hpp"
#  include "KX_ConstraintWrapper.hpp"
#  include "KX_EmptyObject.hpp"
#  include "KX_FontObject.hpp"
#  include "KX_LibLoadStatus.hpp"
#  include "KX_Light.hpp"
#  include "KX_LodLevel.hpp"
#  include "KX_LodManager.hpp"
#  include "KX_MeshProxy.hpp"
#  include "KX_NavMeshObject.hpp"
#  include "KX_PolyProxy.hpp"
#  include "KX_PythonComponent.hpp"
#  include "KX_VehicleWrapper.hpp"
#  include "KX_VertexProxy.hpp"
#  include "SCA_2DFilterActuator.hpp"
#  include "SCA_ANDController.hpp"
#  include "SCA_ActionActuator.hpp"
#  include "SCA_ActuatorSensor.hpp"
#  include "SCA_AddObjectActuator.hpp"
#  include "SCA_AlwaysSensor.hpp"
#  include "SCA_ArmatureSensor.hpp"
#  include "SCA_CameraActuator.hpp"
#  include "SCA_CollectionActuator.hpp"
#  include "SCA_CollisionSensor.hpp"
#  include "SCA_ConstraintActuator.hpp"
#  include "SCA_DelaySensor.hpp"
#  include "SCA_DynamicActuator.hpp"
#  include "SCA_EndObjectActuator.hpp"
#  include "SCA_GameActuator.hpp"
#  include "SCA_IController.hpp"
#  include "SCA_InputEvent.hpp"
#  include "SCA_JoystickSensor.hpp"
#  include "SCA_KeyboardSensor.hpp"
#  include "SCA_MouseActuator.hpp"
#  include "SCA_MouseFocusSensor.hpp"
#  include "SCA_MouseSensor.hpp"
#  include "SCA_MovementSensor.hpp"
#  include "SCA_NANDController.hpp"
#  include "SCA_NORController.hpp"
#  include "SCA_NearSensor.hpp"
#  include "SCA_NetworkMessageActuator.hpp"
#  include "SCA_NetworkMessageSensor.hpp"
#  include "SCA_ORController.hpp"
#  include "SCA_ObjectActuator.hpp"
#  include "SCA_ParentActuator.hpp"
#  include "SCA_PropertySensor.hpp"
#  include "SCA_PythonController.hpp"
#  include "SCA_PythonJoystick.hpp"
#  include "SCA_PythonKeyboard.hpp"
#  include "SCA_PythonMouse.hpp"
#  include "SCA_RadarSensor.hpp"
#  include "SCA_RandomActuator.hpp"
#  include "SCA_RandomSensor.hpp"
#  include "SCA_RaySensor.hpp"
#  include "SCA_ReplaceMeshActuator.hpp"
#  include "SCA_SceneActuator.hpp"
#  include "SCA_SoundActuator.hpp"
#  include "SCA_StateActuator.hpp"
#  include "SCA_SteeringActuator.hpp"
#  include "SCA_TrackToActuator.hpp"
#  include "SCA_VibrationActuator.hpp"
#  include "SCA_VisibilityActuator.hpp"
#  include "SCA_XNORController.hpp"
#  include "SCA_XORController.hpp"
#  include "../VideoTexture/Texture.hpp"

static void PyType_Attr_Set(PyGetSetDef *attr_getset, PyAttributeDef *attr)
{
  attr_getset->name = (char *)attr->m_name.c_str();
  attr_getset->doc = nullptr;

  attr_getset->get = reinterpret_cast<getter>(EXP_PyObjectPlus::py_get_attrdef);

  if (attr->m_access == EXP_PYATTRIBUTE_RO)
    attr_getset->set = nullptr;
  else
    attr_getset->set = reinterpret_cast<setter>(EXP_PyObjectPlus::py_set_attrdef);

  attr_getset->closure = reinterpret_cast<void *>(attr);
}

static void PyType_Ready_ADD(PyObject *dict,
                             PyTypeObject *tp,
                             PyAttributeDef *attributes,
                             PyAttributeDef *attributesPtr,
                             int init_getset)
{
  PyAttributeDef *attr;

  if (init_getset) {
    /* we need to do this for all types before calling PyType_Ready
     * since they will call the parents PyType_Ready and those might not have initialized vars yet
     */

    if (tp->tp_getset == nullptr && ((attributes && !attributes->m_name.empty()) ||
                                     (attributesPtr && !attributesPtr->m_name.empty()))) {
      PyGetSetDef *attr_getset;
      int attr_tot = 0;

      if (attributes) {
        for (attr = attributes; !attr->m_name.empty(); attr++, attr_tot++)
          attr->m_usePtr = false;
      }
      if (attributesPtr) {
        for (attr = attributesPtr; !attr->m_name.empty(); attr++, attr_tot++)
          attr->m_usePtr = true;
      }

      tp->tp_getset = attr_getset = reinterpret_cast<PyGetSetDef *>(
          PyMem_Malloc((attr_tot + 1) * sizeof(PyGetSetDef)));  // XXX - Todo, free

      if (attributes) {
        for (attr = attributes; !attr->m_name.empty(); attr++, attr_getset++) {
          PyType_Attr_Set(attr_getset, attr);
        }
      }
      if (attributesPtr) {
        for (attr = attributesPtr; !attr->m_name.empty(); attr++, attr_getset++) {
          PyType_Attr_Set(attr_getset, attr);
        }
      }
      memset(attr_getset, 0, sizeof(PyGetSetDef));
    }
  }
  else {
    PyType_Ready(tp);
    PyDict_SetItemString(dict, tp->tp_name, reinterpret_cast<PyObject *>(tp));
  }
}

#  define PyType_Ready_Attr(d, n, i) PyType_Ready_ADD(d, &n::Type, n::Attributes, nullptr, i)
#  define PyType_Ready_AttrPtr(d, n, i) \
    PyType_Ready_ADD(d, &n::Type, n::Attributes, n::AttributesPtr, i)

PyDoc_STRVAR(GameTypes_module_documentation,
             "This module provides access to the game engine data types.");
static struct PyModuleDef GameTypes_module_def = {
    PyModuleDef_HEAD_INIT,
    "GameTypes",                    /* m_name */
    GameTypes_module_documentation, /* m_doc */
    0,                              /* m_size */
    nullptr,                        /* m_methods */
    nullptr,                        /* m_reload */
    nullptr,                        /* m_traverse */
    nullptr,                        /* m_clear */
    nullptr,                        /* m_free */
};

PyMODINIT_FUNC initGameTypesPythonBinding(void)
{
  PyObject *m;
  PyObject *dict;

  m = PyModule_Create(&GameTypes_module_def);
  PyDict_SetItemString(PySys_GetObject("modules"), GameTypes_module_def.m_name, m);

  dict = PyModule_GetDict(m);

  for (int init_getset = 1; init_getset > -1;
       init_getset--) { /* run twice, once to init the getsets another to run PyType_Ready */
    PyType_Ready_Attr(dict, SCA_ActionActuator, init_getset);
    PyType_Ready_Attr(dict, BL_Shader, init_getset);
    PyType_Ready_Attr(dict, BL_ArmatureObject, init_getset);
    PyType_Ready_Attr(dict, BL_ArmatureActuator, init_getset);
    PyType_Ready_Attr(dict, BL_ArmatureConstraint, init_getset);
    PyType_Ready_AttrPtr(dict, BL_ArmatureBone, init_getset);
    PyType_Ready_AttrPtr(dict, BL_ArmatureChannel, init_getset);
    PyType_Ready_Attr(dict, BL_Texture, init_getset);
    // PyType_Ready_Attr(dict, EXP_PropValue, init_getset);  // doesn't use Py_Header
    PyType_Ready_Attr(dict, EXP_BaseListValue, init_getset);
    PyType_Ready_Attr(dict, EXP_ListWrapper, init_getset);
    PyType_Ready_Attr(dict, EXP_Value, init_getset);
    PyType_Ready_Attr(dict, KX_2DFilter, init_getset);
    PyType_Ready_Attr(dict, KX_2DFilterManager, init_getset);
    PyType_Ready_Attr(dict, KX_2DFilterFrameBuffer, init_getset);
    PyType_Ready_Attr(dict, SCA_ArmatureSensor, init_getset);
    PyType_Ready_Attr(dict, KX_BlenderMaterial, init_getset);
    PyType_Ready_Attr(dict, KX_Camera, init_getset);
    PyType_Ready_Attr(dict, SCA_CameraActuator, init_getset);
    PyType_Ready_Attr(dict, SCA_CollectionActuator, init_getset);
    PyType_Ready_Attr(dict, KX_CharacterWrapper, init_getset);
    PyType_Ready_Attr(dict, SCA_ConstraintActuator, init_getset);
    PyType_Ready_Attr(dict, KX_ConstraintWrapper, init_getset);
    PyType_Ready_Attr(dict, SCA_GameActuator, init_getset);
    PyType_Ready_Attr(dict, KX_GameObject, init_getset);
    PyType_Ready_Attr(dict, KX_EmptyObject, init_getset);
    PyType_Ready_Attr(dict, KX_LibLoadStatus, init_getset);
    PyType_Ready_Attr(dict, KX_LightObject, init_getset);
    PyType_Ready_Attr(dict, KX_LodLevel, init_getset);
    PyType_Ready_Attr(dict, KX_LodManager, init_getset);
    PyType_Ready_Attr(dict, KX_FontObject, init_getset);
    PyType_Ready_Attr(dict, KX_MeshProxy, init_getset);
    PyType_Ready_Attr(dict, SCA_MouseFocusSensor, init_getset);
    PyType_Ready_Attr(dict, SCA_MovementSensor, init_getset);
    PyType_Ready_Attr(dict, SCA_NearSensor, init_getset);
    PyType_Ready_Attr(dict, SCA_NetworkMessageActuator, init_getset);
    PyType_Ready_Attr(dict, SCA_NetworkMessageSensor, init_getset);
    PyType_Ready_Attr(dict, SCA_ObjectActuator, init_getset);
    PyType_Ready_Attr(dict, SCA_ParentActuator, init_getset);
    PyType_Ready_Attr(dict, KX_PolyProxy, init_getset);
    PyType_Ready_Attr(dict, KX_PythonComponent, init_getset);
    PyType_Ready_Attr(dict, SCA_RadarSensor, init_getset);
    PyType_Ready_Attr(dict, SCA_RaySensor, init_getset);
    PyType_Ready_Attr(dict, SCA_AddObjectActuator, init_getset);
    PyType_Ready_Attr(dict, SCA_DynamicActuator, init_getset);
    PyType_Ready_Attr(dict, SCA_EndObjectActuator, init_getset);
    PyType_Ready_Attr(dict, SCA_ReplaceMeshActuator, init_getset);
    PyType_Ready_Attr(dict, KX_Scene, init_getset);
    PyType_Ready_Attr(dict, KX_NavMeshObject, init_getset);
    PyType_Ready_Attr(dict, SCA_SceneActuator, init_getset);
    PyType_Ready_Attr(dict, SCA_SoundActuator, init_getset);
    PyType_Ready_Attr(dict, SCA_StateActuator, init_getset);
    PyType_Ready_Attr(dict, SCA_SteeringActuator, init_getset);
    PyType_Ready_Attr(dict, SCA_CollisionSensor, init_getset);
    PyType_Ready_Attr(dict, SCA_TrackToActuator, init_getset);
    PyType_Ready_Attr(dict, KX_VehicleWrapper, init_getset);
    PyType_Ready_Attr(dict, KX_VertexProxy, init_getset);
    PyType_Ready_Attr(dict, SCA_VisibilityActuator, init_getset);
    PyType_Ready_Attr(dict, SCA_MouseActuator, init_getset);
    PyType_Ready_Attr(dict, KX_CollisionContactPoint, init_getset);
    PyType_Ready_Attr(dict, EXP_PyObjectPlus, init_getset);
    PyType_Ready_Attr(dict, SCA_2DFilterActuator, init_getset);
    PyType_Ready_Attr(dict, SCA_ANDController, init_getset);
    // PyType_Ready_Attr(dict, SCA_Actuator, init_getset);  // doesn't use Py_Header
    PyType_Ready_Attr(dict, SCA_ActuatorSensor, init_getset);
    PyType_Ready_Attr(dict, SCA_AlwaysSensor, init_getset);
    PyType_Ready_Attr(dict, SCA_DelaySensor, init_getset);
    PyType_Ready_Attr(dict, SCA_ILogicBrick, init_getset);
    PyType_Ready_Attr(dict, SCA_InputEvent, init_getset);
    PyType_Ready_Attr(dict, SCA_IObject, init_getset);
    PyType_Ready_Attr(dict, SCA_ISensor, init_getset);
    PyType_Ready_Attr(dict, SCA_JoystickSensor, init_getset);
    PyType_Ready_Attr(dict, SCA_KeyboardSensor, init_getset);
    PyType_Ready_Attr(dict, SCA_MouseSensor, init_getset);
    PyType_Ready_Attr(dict, SCA_NANDController, init_getset);
    PyType_Ready_Attr(dict, SCA_NORController, init_getset);
    PyType_Ready_Attr(dict, SCA_ORController, init_getset);
    PyType_Ready_Attr(dict, SCA_PropertyActuator, init_getset);
    PyType_Ready_Attr(dict, SCA_PropertySensor, init_getset);
    PyType_Ready_Attr(dict, SCA_PythonController, init_getset);
    PyType_Ready_Attr(dict, SCA_RandomActuator, init_getset);
    PyType_Ready_Attr(dict, SCA_RandomSensor, init_getset);
    PyType_Ready_Attr(dict, SCA_VibrationActuator, init_getset);
    PyType_Ready_Attr(dict, SCA_XNORController, init_getset);
    PyType_Ready_Attr(dict, SCA_XORController, init_getset);
    PyType_Ready_Attr(dict, SCA_IController, init_getset);
    PyType_Ready_Attr(dict, SCA_PythonJoystick, init_getset);
    PyType_Ready_Attr(dict, SCA_PythonKeyboard, init_getset);
    PyType_Ready_Attr(dict, SCA_PythonMouse, init_getset);
    PyType_Ready_Attr(dict, Texture, init_getset);
  }

#  ifdef USE_MATHUTILS
  /* Init mathutils callbacks */
  KX_GameObject_Mathutils_Callback_Init();
  SCA_ObjectActuator_Mathutils_Callback_Init();
#  endif

  return m;
}

#endif  // WITH_PYTHON
