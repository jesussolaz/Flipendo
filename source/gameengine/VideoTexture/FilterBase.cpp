/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2006 The Zdeno Ash Miklas. */

/** \file gameengine/VideoTexture/FilterBase.cpp
 *  \ingroup bgevideotex
 */

#include "FilterBase.hpp"

// FilterBase class implementation

// constructor
FilterBase::FilterBase(void) : m_previous(nullptr), m_wrapper(nullptr)
{
}

// destructor
FilterBase::~FilterBase(void)
{
  // release Python objects, if not released yet
  release();
}

// release python objects
void FilterBase::release(void)
{
  // release previous filter object
  setPrevious(nullptr);
}

// set new previous filter
void FilterBase::setPrevious(FilterBase *filt, bool useRefCnt)
{
  // if reference counting has to be used
  if (useRefCnt) {
    // reference new filter
    if (filt != nullptr)
      VT_WrapperIncRef(filt->getWrapper());
    // release old filter
    if (m_previous != nullptr)
      VT_WrapperDecRef(m_previous->getWrapper());
  }
  // set new previous filter
  m_previous = filt;
}

// find first filter
FilterBase *FilterBase::findFirst(void)
{
  // find first filter in chain
  FilterBase *frst;
  for (frst = this; frst->m_previous != nullptr; frst = frst->m_previous) {
  };
  // set first filter
  return frst;
}

#ifdef WITH_PYTHON

// list offilter types
PyTypeList pyFilterTypes;

// functions for python interface

// object allocation
PyObject *Filter_allocNew(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
  // allocate object
  PyFilter *self = reinterpret_cast<PyFilter *>(type->tp_alloc(type, 0));
  // initialize object structure
  self->m_filter = nullptr;
  // return allocated object
  return reinterpret_cast<PyObject *>(self);
}

// object deallocation
void Filter_dealloc(PyFilter *self)
{
  // release object attributes
  if (self->m_filter != nullptr) {
    self->m_filter->release();
    delete self->m_filter;
    self->m_filter = nullptr;
  }
  Py_TYPE((PyObject *)self)->tp_free((PyObject *)self);
}

// get previous pixel filter object
PyObject *Filter_getPrevious(PyFilter *self, void *closure)
{
  // if filter object is available
  if (self->m_filter != nullptr) {
    // pixel filter object
    FilterBase *prev = self->m_filter->getPrevious();
    // if filter is present, return its python wrapper
    if (prev != nullptr && prev->getWrapper() != nullptr) {
      PyObject *filt = reinterpret_cast<PyObject *>(prev->getWrapper());
      Py_INCREF(filt);
      return filt;
    }
  }
  // otherwise return none
  Py_RETURN_NONE;
}

// set previous pixel filter object
int Filter_setPrevious(PyFilter *self, PyObject *value, void *closure)
{
  // if filter object is available
  if (self->m_filter != nullptr) {
    // check new value
    if (value == nullptr || !pyFilterTypes.in(Py_TYPE(value))) {
      // report value error
      PyErr_SetString(PyExc_TypeError, "Invalid type of value");
      return -1;
    }
    // set new value
    self->m_filter->setPrevious(reinterpret_cast<PyFilter *>(value)->m_filter);
  }
  // return success
  return 0;
}

#endif  // WITH_PYTHON
