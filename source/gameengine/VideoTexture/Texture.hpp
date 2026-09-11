/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2007 The Zdeno Ash Miklas. */

/** \file VideoTexture/Texture.h
 *  \ingroup bgevideotex
 */

#pragma once

#include "DNA_image_types.h"

#include "EXP_Value.hpp"
#include "Exception.hpp"
#include "ImageBase.hpp"

struct GPUTexture;

struct ImBuf;
class RAS_Texture;
class RAS_IPolyMaterial;
class KX_Scene;
class KX_GameObject;

/** Textura dinamica: ata una imagen (ImageRender, ImageViewport, video...) a la
 * textura de un material de la escena.
 *
 * Flipendo: el API de esta clase es NATIVO. Antes la fuente se pedia y se
 * guardaba como `PyImage *`, es decir, una cabecera de objeto de Python
 * envolviendo un `ImageBase *`; eso ataba render-a-textura al interprete y por
 * eso VideoTexture no entraba en el Player sin CPython. Ahora la fuente es un
 * `ImageBase *` y las versiones con `PyImage *` / `PyObject *` son envoltorios
 * delgados bajo `#ifdef WITH_PYTHON` que solo aportan el contador de
 * referencias. Ver politicas/PLAYER-SIN-CPYTHON.md §3.1. */
class Texture : public EXP_Value {
  Py_Header protected : virtual void DestructFromPython();

 public:
  // texture is using blender material
  bool m_useMatTexture;

  // video texture bind code
  // original texture bind code
  unsigned int m_orgTex;
  // original image bind code
  struct Image *m_orgImg;
  // original texture saved
  bool m_orgSaved;

  // kernel image buffer, to make sure the image is loaded before we swap the bindcode
  struct ImBuf *m_imgBuf;
  // texture image for game materials
  Image *m_imgTexture;

  // texture for blender materials
  RAS_Texture *m_matTexture;

  KX_Scene *m_scene;
  KX_GameObject *m_gameobj;
  GPUTexture *m_origGpuTex;
  GPUTexture *m_modifiedGPUTexture;
  void *m_py_color;

  // use mipmapping
  bool m_mipmap;

  // scaled image buffer
  ImBuf *m_scaledImBuf;
  // last refresh
  double m_lastClock;

  /// image source (nativa)
  ImageBase *m_source;
  /// la textura es la duenna de la fuente: la destruye al morir. Solo en el
  /// camino nativo; cuando la fuente la creo Python, manda su envoltorio.
  bool m_ownSource;

  Texture();
  virtual ~Texture();

  virtual std::string GetName();

  void Close();

  /** Atar la textura a la ranura `texID` del material `matID` del objeto.
   * Es el cuerpo que antes vivia dentro de `Texture_init` (Python).
   * Devuelve false y deja el error dicho si no hay material o textura. */
  bool SetGameObject(KX_GameObject *gameObj, short matID = 0, short texID = 0);

  /// fijar la imagen fuente (nativo)
  void SetSource(ImageBase *source, bool own = false);

  /** Refrescar la textura desde su fuente. Es el cuerpo del metodo `refresh()`
   * de Python, ya sin Python: se llama una vez por frame. */
  void Refresh(bool refreshSource, double ts = -1.0);

#ifdef WITH_PYTHON
  /// envoltorio delgado: del PyImage solo se usa el contador de referencias
  void SetSource(PyImage *source);
#endif

  // load texture
  void loadTexture(unsigned int *texture,
                   short *size,
                   bool mipmap,
                   blender::gpu::TextureFormat format);

  static void FreeAllTextures(KX_Scene *scene);
  /// soltar una sola textura (lo que FreeAllTextures hace con todas las de una escena)
  static void FreeTexture(Texture *texture);

 private:
  /// soltar la fuente actual respetando quien sea su duenno
  void ReleaseSource();

#ifdef WITH_PYTHON
 public:
  EXP_PYMETHOD_DOC(Texture, close);
  EXP_PYMETHOD_DOC(Texture, refresh);

  static PyObject *pyattr_get_mipmap(EXP_PyObjectPlus *self_v, const EXP_PYATTRIBUTE_DEF *attrdef);
  static int pyattr_set_mipmap(EXP_PyObjectPlus *self_v,
                               const EXP_PYATTRIBUTE_DEF *attrdef,
                               PyObject *value);
  static PyObject *pyattr_get_source(EXP_PyObjectPlus *self_v, const EXP_PYATTRIBUTE_DEF *attrdef);
  static int pyattr_set_source(EXP_PyObjectPlus *self_v,
                               const EXP_PYATTRIBUTE_DEF *attrdef,
                               PyObject *value);
  static PyObject *pyattr_get_gputexture(EXP_PyObjectPlus *self_v, const EXP_PYATTRIBUTE_DEF *attrdef);
#endif  // WITH_PYTHON
};

// get material
RAS_IPolyMaterial *getMaterial(KX_GameObject *gameObj, short matID);

// get material ID (nativo)
short getMaterialID(KX_GameObject *gameObj, const char *name);

#ifdef WITH_PYTHON
// get material ID (envoltorio de Python)
short getMaterialID(PyObject *obj, const char *name);
#endif

// Exceptions
extern ExceptionID MaterialNotAvail;
extern ExceptionID TextureNotAvail;
