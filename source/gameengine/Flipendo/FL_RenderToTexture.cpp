/* Implementación de la fachada C++ de VideoTexture. Ver FL_RenderToTexture.hpp. */
#include "FL_RenderToTexture.hpp"

#include "FL_Component.hpp"

#include "BLI_fileops.h"
#include "BLI_math_color.h"
#include "DNA_image_types.h"
#include "GPU_texture.hh"
#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"
#include "MEM_guardedalloc.h"

#include "CM_Message.hpp"
#include "EXP_ListValue.hpp"
#include "EXP_Value.hpp"
#include "ImageRender.hpp"
#include "KX_Camera.hpp"
#include "KX_GameObject.hpp"
#include "KX_Globals.hpp"
#include "KX_KetsjiEngine.hpp"
#include "KX_Scene.hpp"
#include "RAS_IPolygonMaterial.hpp"
#include "RAS_Texture.hpp"
#include "Texture.hpp"

#include <cstdlib>

namespace flipendo {

/* ----------------------------------------------------------------- helpers */

KX_Camera *FL_FindCamera(KX_GameObject *anyObject, const std::string &name)
{
  if (!anyObject) {
    return nullptr;
  }
  KX_Scene *scene = anyObject->GetScene();
  if (!scene) {
    return nullptr;
  }
  EXP_ListValue<KX_Camera> *cams = scene->GetCameraList();
  if (!cams) {
    return nullptr;
  }
  for (KX_Camera *cam : cams) {
    if (cam->GetName() == name) {
      return cam;
    }
  }
  return nullptr;
}

/* Primera camara de la escena que NO sea la activa ni la de overlay ni tenga
 * viewport propio: justamente las condiciones que ImageRender exige. */
static KX_Camera *FirstSpareCamera(KX_Scene *scene)
{
  if (!scene) {
    return nullptr;
  }
  EXP_ListValue<KX_Camera> *cams = scene->GetCameraList();
  if (!cams) {
    return nullptr;
  }
  for (KX_Camera *cam : cams) {
    if (cam == scene->GetActiveCamera() || cam == scene->GetOverlayCamera()) {
      continue;
    }
    if (cam->GetViewport()) {
      continue;
    }
    return cam;
  }
  return nullptr;
}

static std::string PropText(KX_GameObject *obj, const char *name, const std::string &def)
{
  EXP_Value *p = obj ? obj->GetProperty(name) : nullptr;
  return p ? p->GetText() : def;
}

static int PropInt(KX_GameObject *obj, const char *name, int def)
{
  EXP_Value *p = obj ? obj->GetProperty(name) : nullptr;
  return p ? (int)p->GetNumber() : def;
}

static std::string EnvText(const char *name, const std::string &def)
{
  const char *v = std::getenv(name);
  return (v && v[0]) ? std::string(v) : def;
}

static int EnvInt(const char *name, int def)
{
  const char *v = std::getenv(name);
  return (v && v[0]) ? atoi(v) : def;
}

/* ------------------------------------------------------- FL_RenderToTexture */

FL_RenderToTexture *FL_RenderToTexture::Create(KX_GameObject *target,
                                               const std::string &materialName,
                                               KX_Camera *camera,
                                               int width,
                                               int height,
                                               short textureSlot,
                                               int samples)
{
  if (!target) {
    CM_Error("FL_RenderToTexture: no hay objeto destino");
    return nullptr;
  }
  if (!camera) {
    CM_Error("FL_RenderToTexture: no hay camara para '" << target->GetName() << "'");
    return nullptr;
  }
  KX_Scene *scene = target->GetScene();
  if (!scene) {
    CM_Error("FL_RenderToTexture: el objeto '" << target->GetName() << "' no tiene escena");
    return nullptr;
  }

  short matID = 0;
  if (!materialName.empty()) {
    matID = getMaterialID(target, materialName.c_str());
    if (matID < 0) {
      CM_Error("FL_RenderToTexture: '" << target->GetName() << "' no tiene material '"
                                       << materialName << "'");
      return nullptr;
    }
  }

  Texture *tex = new Texture();
  if (!tex->SetGameObject(target, matID, textureSlot)) {
    /* SetGameObject ya ha dicho por que (material o ranura de textura ausente). */
    Texture::FreeTexture(tex);
    return nullptr;
  }

  ImageRender *render = new ImageRender(
      scene, camera, (unsigned int)width, (unsigned int)height, (unsigned short)samples);
  /* own = true: en el camino nativo no hay envoltorio de Python que la posea, asi
   * que la Texture es la duenna de la imagen y la destruye con ella. */
  tex->SetSource(render, true);

  FL_RenderToTexture *rtt = new FL_RenderToTexture();
  rtt->m_target = target;
  rtt->m_camera = camera;
  rtt->m_texture = tex;
  rtt->m_render = render;
  rtt->m_matID = matID;
  rtt->m_texID = textureSlot;
  return rtt;
}

FL_RenderToTexture::~FL_RenderToTexture()
{
  if (m_texture) {
    m_texture->Close();
    Texture::FreeTexture(m_texture);
    m_texture = nullptr;
  }
  /* m_render lo destruye la Texture: es su duenna (own = true). */
  m_render = nullptr;
}

void FL_RenderToTexture::Refresh(bool refreshSource)
{
  if (m_texture) {
    m_texture->Refresh(refreshSource);
  }
}

std::string FL_RenderToTexture::Describe() const
{
  std::string s = "FL_RenderToTexture " + (m_target ? m_target->GetName() : std::string("?")) +
                  " mat=" + std::to_string(m_matID) + " tex=" + std::to_string(m_texID) +
                  " cam=" + (m_camera ? m_camera->GetName() : std::string("?"));
  if (m_texture && m_texture->m_imgTexture) {
    GPUTexture *gt = m_texture->m_imgTexture->gputexture[TEXTARGET_2D][0];
    if (gt) {
      s += " gputex=" + std::to_string(GPU_texture_width(gt)) + "x" +
           std::to_string(GPU_texture_height(gt));
      s += m_texture->m_origGpuTex ? " (intercambiada)" : " (original)";
    }
    else {
      s += " gputex=ninguna";
    }
  }
  return s;
}

bool FL_RenderToTexture::WritePng(const std::string &path) const
{
  return FL_DumpObjectTexturePng(m_target, m_matID, m_texID, path);
}

/* Vuelca la textura de GPU que ese material esta enseñando ahora mismo, sin
 * preguntar quien la puso ahi. */
bool FL_DumpObjectTexturePng(KX_GameObject *obj, short matID, short texID, const std::string &path)
{
  RAS_IPolyMaterial *mat = obj ? getMaterial(obj, matID) : nullptr;
  RAS_Texture *rastex = mat ? mat->GetTexture(texID) : nullptr;
  Image *image = rastex ? rastex->GetImage() : nullptr;
  if (!image) {
    CM_Error("FL_DumpObjectTexturePng: '" << (obj ? obj->GetName() : std::string("?"))
                                          << "' no tiene imagen en el material " << matID
                                          << " ranura " << texID);
    return false;
  }
  GPUTexture *gputex = image->gputexture[TEXTARGET_2D][0];
  if (!gputex) {
    CM_Error("FL_DumpObjectTexturePng: la imagen de destino no tiene textura de GPU");
    return false;
  }
  const int w = GPU_texture_width(gputex);
  const int h = GPU_texture_height(gputex);
  if (w <= 0 || h <= 0) {
    CM_Error("FL_DumpObjectTexturePng: textura de GPU de tamanno invalido");
    return false;
  }

  /* La textura de color de un GPUViewport suele ser de coma flotante (RGBA16F),
   * asi que no vale leerla siempre como bytes: se pregunta el formato y, si es
   * flotante, se convierte aqui a sRGB de 8 bits. La conversion es la MISMA en
   * los dos binarios, que es lo que hace comparables los dos volcados. */
  uint8_t *pixels = nullptr;
  bool ownPixels = false;
  if (GPU_texture_has_float_format(gputex)) {
    float *fpixels = (float *)GPU_texture_read(gputex, GPU_DATA_FLOAT, 0);
    if (!fpixels) {
      CM_Error("FL_DumpObjectTexturePng: no se pudo leer la textura de GPU (flotante)");
      return false;
    }
    pixels = (uint8_t *)MEM_mallocN(size_t(w) * size_t(h) * 4, "FL_RTT dump");
    ownPixels = true;
    for (size_t i = 0; i < size_t(w) * size_t(h); ++i) {
      linearrgb_to_srgb_uchar4(pixels + i * 4, fpixels + i * 4);
    }
    MEM_freeN(fpixels);
  }
  else {
    pixels = (uint8_t *)GPU_texture_read(gputex, GPU_DATA_UBYTE, 0);
    ownPixels = true;
    if (!pixels) {
      CM_Error("FL_DumpObjectTexturePng: no se pudo leer la textura de GPU");
      return false;
    }
  }

  /* Firma del contenido, para poder comparar dos volcados sin abrir imagenes:
   * FNV-1a de 64 bits sobre el buffer RGBA8 ya convertido. */
  uint64_t firma = 1469598103934665603ull;
  for (size_t i = 0; i < size_t(w) * size_t(h) * 4; ++i) {
    firma ^= pixels[i];
    firma *= 1099511628211ull;
  }
  CM_Message("FL_RTT firma: " << w << "x" << h << " fnv1a=" << std::hex << firma << std::dec);

  /* El buffer crudo al lado del PNG: asi la comparacion pixel a pixel entre dos
   * ejecuciones es un `cmp` y no hace falta descodificar nada. */
  {
    const std::string rawPath = path + ".rgba";
    FILE *f = BLI_fopen(rawPath.c_str(), "wb");
    if (f) {
      fwrite(pixels, 1, size_t(w) * size_t(h) * 4, f);
      fclose(f);
      CM_Message("FL_RTT volcado crudo RGBA8 en " << rawPath);
    }
  }

  ImBuf *ibuf = IMB_allocFromBuffer(pixels, nullptr, (unsigned int)w, (unsigned int)h, 4);
  if (ownPixels) {
    MEM_freeN(pixels);
  }
  if (!ibuf) {
    CM_Error("FL_DumpObjectTexturePng: no se pudo crear el ImBuf del volcado");
    return false;
  }
  ibuf->ftype = IMB_FTYPE_PNG;
  const bool ok = IMB_save_image(ibuf, path.c_str(), IB_byte_data);
  IMB_freeImBuf(ibuf);
  if (ok) {
    CM_Message("FL_RTT volcado: textura "
               << w << "x" << h << " ("
               << GPU_texture_format_name(GPU_texture_format(gputex)) << ") de '"
               << obj->GetName() << "' material " << matID << " en " << path);
  }
  else {
    CM_Error("FL_DumpObjectTexturePng: no se pudo escribir " << path);
  }
  return ok;
}

/* --------------------------------------------------------------- la sonda */

void FL_RttProbeTick(KX_Scene *scene)
{
  static const std::string probeName = EnvText("FL_RTT_PROBE", "");
  static const std::string dumpPath = EnvText("FL_RTT_DUMP", "");
  static const int dumpFrame = EnvInt("FL_RTT_DUMP_FRAME", 90);
  static const bool exitAfter = EnvInt("FL_RTT_EXIT", 0) != 0;
  static int frames = 0;
  static bool done = false;

  if (probeName.empty() || dumpPath.empty() || done || !scene) {
    return;
  }
  ++frames;
  if (frames < dumpFrame) {
    return;
  }
  done = true;

  EXP_ListValue<KX_GameObject> *objs = scene->GetObjectList();
  KX_GameObject *target = nullptr;
  if (objs) {
    for (KX_GameObject *o : objs) {
      if (o->GetName() == probeName) {
        target = o;
        break;
      }
    }
  }
  if (!target) {
    CM_Error("FL_RTT_PROBE: no hay ningun objeto llamado '" << probeName << "' en la escena");
  }
  else {
    CM_Message("FL_RTT_PROBE: frame " << frames << ", volcando '" << probeName << "'");
    FL_DumpObjectTexturePng(target, 0, 0, dumpPath);
  }
  if (exitAfter && KX_GetActiveEngine()) {
    KX_GetActiveEngine()->RequestExit(KX_ExitRequest::QUIT_GAME);
  }
}

/* ------------------------------------------------------------- componente */

/** CctvMonitor — camara de vigilancia: una camara secundaria renderiza sobre la
 * textura del objeto que lleva el componente.
 *
 * Propiedades de juego que lee del objeto (todas opcionales menos la camara):
 *   fl_rtt_camera    (texto)  nombre del objeto camara. Si falta, la primera
 *                             camara libre de la escena.
 *   fl_rtt_material  (texto)  material o imagen de destino; vacio = material 0.
 *   fl_rtt_slot      (entero) ranura de textura dentro del material (0)
 *   fl_rtt_width     (entero) ancho del render (512)
 *   fl_rtt_height    (entero) alto del render (512)
 *   fl_rtt_samples   (entero) pasadas de anti-aliasing (1)
 *
 * El volcado de verificacion NO vive aqui: es FL_RttProbeTick, que funciona
 * igual para este camino nativo que para el de Python y por eso permite
 * compararlos. */
class FL_CctvMonitor : public FL_Component {
 public:
  ~FL_CctvMonitor() override
  {
    delete m_rtt;
    m_rtt = nullptr;
  }

  void Start() override
  {
    const std::string camName = PropText(
        m_owner, "fl_rtt_camera", EnvText("FL_RTT_CAMERA", ""));
    KX_Camera *cam = camName.empty() ? FirstSpareCamera(m_owner->GetScene()) :
                                       FL_FindCamera(m_owner, camName);
    if (!cam) {
      CM_Error("CctvMonitor en '" << m_owner->GetName() << "': no encuentro la camara "
                                  << (camName.empty() ? std::string("(ninguna libre)") : camName));
      return;
    }

    const std::string matName = PropText(m_owner, "fl_rtt_material", EnvText("FL_RTT_MATERIAL", ""));
    const int w = PropInt(m_owner, "fl_rtt_width", EnvInt("FL_RTT_WIDTH", 512));
    const int h = PropInt(m_owner, "fl_rtt_height", EnvInt("FL_RTT_HEIGHT", 512));
    const int slot = PropInt(m_owner, "fl_rtt_slot", 0);
    const int samples = PropInt(m_owner, "fl_rtt_samples", 1);

    m_rtt = FL_RenderToTexture::Create(m_owner, matName, cam, w, h, (short)slot, samples);
    if (m_rtt) {
      CM_Message("CctvMonitor: " << m_rtt->Describe());
    }
  }

  void Update(float) override
  {
    if (!m_rtt) {
      return;
    }
    m_rtt->Refresh(true);
    ++m_frames;
    if (m_frames == 1) {
      CM_Message("CctvMonitor: primer refresco hecho, " << m_rtt->Describe());
    }
  }

 private:
  FL_RenderToTexture *m_rtt = nullptr;
  int m_frames = 0;
};

void FL_RegisterRenderToTextureComponents(FL_ComponentManager &mgr)
{
  mgr.RegisterType("CctvMonitor", []() -> FL_Component * { return new FL_CctvMonitor(); });
}

int FL_RttAttachFromEnvironment(KX_Scene *scene, FL_ComponentManager &mgr)
{
  const std::string targetName = EnvText("FL_RTT_TARGET", "");
  if (targetName.empty() || !scene) {
    return 0;
  }
  EXP_ListValue<KX_GameObject> *objs = scene->GetObjectList();
  if (!objs) {
    return 0;
  }
  int attached = 0;
  for (KX_GameObject *obj : objs) {
    if (obj->GetName() != targetName) {
      continue;
    }
    if (obj->GetProperty("fl_component")) {
      /* ya lo ata el camino normal */
      continue;
    }
    CM_Message("FL_RTT_TARGET: atando CctvMonitor a '" << targetName << "'");
    mgr.AttachComponent(obj, "CctvMonitor");
    ++attached;
  }
  if (attached == 0) {
    CM_Error("FL_RTT_TARGET: no hay ningun objeto llamado '" << targetName << "' en la escena");
  }
  return attached;
}

}  // namespace flipendo
