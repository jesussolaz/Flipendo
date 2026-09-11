/* FL_RenderToTexture — render a textura NATIVO en C++ para Flipendo.
 *
 * Espejos, minimapas y camaras de vigilancia sin una linea de Python. Es la
 * fachada C++ de VideoTexture: crea un `ImageRender` sobre el material de un
 * objeto y lo refresca por frame, que es exactamente lo que en UPBGE habia que
 * pedirle a `bge.texture` desde un script.
 *
 *   bge.texture (Python)                 FL_RenderToTexture (C++)
 *   --------------------------------     ------------------------------------
 *   tex = texture.Texture(ob, matID)     FL_RenderToTexture::Create(ob, mat, cam)
 *   tex.source = texture.ImageRender(..)   (lo hace Create)
 *   tex.refresh(True)   # cada frame     rtt->Refresh()   # cada frame
 *
 * Doctrina: politicas/LENGUAJE-CPP.md y politicas/PLAYER-SIN-CPYTHON.md §3.1. */
#ifndef __FL_RENDERTOTEXTURE_HPP__
#define __FL_RENDERTOTEXTURE_HPP__

#include <string>

class KX_Camera;
class KX_GameObject;
class KX_Scene;
class Texture;
class ImageRender;

namespace flipendo {

class FL_ComponentManager;

class FL_RenderToTexture {
 public:
  /** Ata la camara `camera` a una textura del objeto `target`.
   *
   * `materialName` acepta el nombre del material tal cual lo devuelve el motor
   * (p.ej. "MAM_Pantalla") o el de la imagen si empieza por "IM"; vacio =
   * material 0. `textureSlot` es la ranura de textura dentro del material.
   * Devuelve nullptr y deja dicho el motivo si algo falta. */
  static FL_RenderToTexture *Create(KX_GameObject *target,
                                    const std::string &materialName,
                                    KX_Camera *camera,
                                    int width = 512,
                                    int height = 512,
                                    short textureSlot = 0,
                                    int samples = 1);

  ~FL_RenderToTexture();

  /// Una vez por frame. `refreshSource` invalida la imagen para forzar el render.
  void Refresh(bool refreshSource = true);

  KX_Camera *Camera() const
  {
    return m_camera;
  }
  KX_GameObject *Target() const
  {
    return m_target;
  }

  /** Volcar a PNG la textura que el material esta enseñando AHORA MISMO.
   * Es la prueba con evidencia: si el enlace funciona, lo que sale es lo que ve
   * la camara de vigilancia, no la imagen original del material. */
  bool WritePng(const std::string &path) const;

  /// Linea de estado legible, para dejar constancia en la consola del Player.
  std::string Describe() const;

 private:
  FL_RenderToTexture() = default;

  KX_GameObject *m_target = nullptr;
  KX_Camera *m_camera = nullptr;
  Texture *m_texture = nullptr;
  ImageRender *m_render = nullptr;
  short m_matID = 0;
  short m_texID = 0;
};

/* Busca una camara por nombre en la escena del objeto. */
KX_Camera *FL_FindCamera(KX_GameObject *anyObject, const std::string &name);

/** Vuelca a PNG la textura de GPU que el material `matID` (ranura `texID`) del
 * objeto esta enseñando en este instante.
 *
 * Es deliberadamente independiente de quien haya montado el render a textura:
 * sirve igual para el camino nativo (FL_RenderToTexture) que para el camino de
 * Python (bge.texture), y por eso permite comparar los dos pixel a pixel. */
bool FL_DumpObjectTexturePng(KX_GameObject *obj,
                             short matID,
                             short texID,
                             const std::string &path);

/** Sonda de verificacion, del estilo de los --fl-dump-* del editor (el Player no
 * tiene analizador de opciones largas, asi que va por variables de entorno):
 *
 *   FL_RTT_PROBE       nombre del objeto cuya textura se vuelca
 *   FL_RTT_DUMP        ruta del PNG
 *   FL_RTT_DUMP_FRAME  en que frame volcarla (90 por defecto)
 *   FL_RTT_EXIT        "1" para salir del juego tras volcar
 *
 * La llama FL_ComponentManager::Tick una vez por frame. */
void FL_RttProbeTick(::KX_Scene *scene);

/* Registra el componente integrado "CctvMonitor". */
void FL_RegisterRenderToTextureComponents(FL_ComponentManager &mgr);

/* Sonda de verificacion: si la variable de entorno FL_RTT_TARGET nombra un
 * objeto de la escena, se le ata un CctvMonitor aunque el .blend no traiga la
 * propiedad "fl_component". Devuelve cuantos ato. */
int FL_RttAttachFromEnvironment(::KX_Scene *scene, FL_ComponentManager &mgr);

}  // namespace flipendo

#endif  // __FL_RENDERTOTEXTURE_HPP__
