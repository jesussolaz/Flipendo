/* Implementación del lienzo 2D nativo. Ver FL_UiCanvas.hpp. */
#include "FL_UiCanvas.hpp"

#include "BLF_api.hh"
#include "GPU_immediate.hh"
#include "GPU_state.hh"

#include "CM_Message.hpp"
#include "KX_Globals.hpp"
#include "KX_KetsjiEngine.hpp"
#include "RAS_ICanvas.hpp"

#include <cstdlib>
#include <map>

namespace flipendo {

/* ------------------------------------------------------------- el lienzo */

void FL_UiCanvas::BeginFrame(int width, int height)
{
  m_width = width;
  m_height = height;
  m_clipStack.clear();
  if (m_fontid < 0) {
    m_fontid = BLF_default();
  }
  GPU_blend(GPU_BLEND_ALPHA);
}

void FL_UiCanvas::EndFrame()
{
  while (!m_clipStack.empty()) {
    PopClip();
  }
  GPU_blend(GPU_BLEND_NONE);
}

/* Convierte la Y de «origen arriba» a la Y de la pantalla, que crece hacia
 * arriba. Es la misma cuenta que hace RAS_OpenGLDebugDraw con sus Box2D. */
static inline float FlipY(int height, float y)
{
  return float(height) - y;
}

void FL_UiCanvas::Rect(const FL_Rect &r, const FL_Color &color)
{
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformColor4fv(color.Value());
  const float y0 = FlipY(m_height, r.y);
  const float y1 = FlipY(m_height, r.y + r.h);
  immRectf(pos, r.x, y1, r.x + r.w, y0);
  immUnbindProgram();
}

void FL_UiCanvas::GradientRect(const FL_Rect &r,
                               const FL_Color &c0,
                               const FL_Color &c1,
                               const FL_Color &c2,
                               const FL_Color &c3)
{
  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  uint col = GPU_vertformat_attr_add(format, "color", GPU_COMP_F32, 4, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_SMOOTH_COLOR);

  const float x0 = r.x;
  const float x1 = r.x + r.w;
  const float y0 = FlipY(m_height, r.y);          /* arriba */
  const float y1 = FlipY(m_height, r.y + r.h);    /* abajo */

  /* Dos triángulos, esquinas en el orden de bgui:
   * c0 arriba-izquierda, c1 arriba-derecha, c2 abajo-derecha, c3 abajo-izquierda. */
  immBegin(GPU_PRIM_TRIS, 6);
  immAttr4fv(col, c0.Value()); immVertex2f(pos, x0, y0);
  immAttr4fv(col, c1.Value()); immVertex2f(pos, x1, y0);
  immAttr4fv(col, c2.Value()); immVertex2f(pos, x1, y1);

  immAttr4fv(col, c0.Value()); immVertex2f(pos, x0, y0);
  immAttr4fv(col, c2.Value()); immVertex2f(pos, x1, y1);
  immAttr4fv(col, c3.Value()); immVertex2f(pos, x0, y1);
  immEnd();
  immUnbindProgram();
}

void FL_UiCanvas::Border(const FL_Rect &r, float thickness, const FL_Color &color)
{
  if (thickness <= 0.0f) {
    return;
  }
  const float t = thickness;
  Rect(FL_Rect(r.x, r.y, r.w, t), color);                    /* arriba */
  Rect(FL_Rect(r.x, r.y + r.h - t, r.w, t), color);          /* abajo */
  Rect(FL_Rect(r.x, r.y + t, t, r.h - 2 * t), color);        /* izquierda */
  Rect(FL_Rect(r.x + r.w - t, r.y + t, t, r.h - 2 * t), color); /* derecha */
}

void FL_UiCanvas::TexturedRect(const FL_Rect &r,
                               GPUTexture *texture,
                               const FL_Color &tint,
                               const float texco[4][2])
{
  if (!texture) {
    return;
  }
  static const float porDefecto[4][2] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};
  const float(*uv)[2] = texco ? texco : porDefecto;

  GPUVertFormat *format = immVertexFormat();
  uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  uint uvattr = GPU_vertformat_attr_add(format, "texCoord", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_3D_IMAGE_COLOR);
  immBindTexture("image", texture);
  immUniformColor4fv(tint.Value());

  const float x0 = r.x;
  const float x1 = r.x + r.w;
  const float y0 = FlipY(m_height, r.y);        /* arriba */
  const float y1 = FlipY(m_height, r.y + r.h);  /* abajo */

  /* uv[0] es la esquina de abajo a la izquierda, como en bgui. */
  immBegin(GPU_PRIM_TRIS, 6);
  immAttr2f(uvattr, uv[3][0], uv[3][1]); immVertex2f(pos, x0, y0);
  immAttr2f(uvattr, uv[2][0], uv[2][1]); immVertex2f(pos, x1, y0);
  immAttr2f(uvattr, uv[1][0], uv[1][1]); immVertex2f(pos, x1, y1);

  immAttr2f(uvattr, uv[3][0], uv[3][1]); immVertex2f(pos, x0, y0);
  immAttr2f(uvattr, uv[1][0], uv[1][1]); immVertex2f(pos, x1, y1);
  immAttr2f(uvattr, uv[0][0], uv[0][1]); immVertex2f(pos, x0, y1);
  immEnd();

  immUnbindProgram();
  GPU_texture_unbind(texture);
}

void FL_UiCanvas::Text(const std::string &text, float x, float y, int ptSize, const FL_Color &color)
{
  if (m_fontid < 0) {
    m_fontid = BLF_default();
  }
  BLF_size(m_fontid, float(ptSize));
  BLF_color4fv(m_fontid, color.Value());

  /* bgui posiciona por la esquina superior izquierda; BLF por la línea base.
   * Se baja una altura de línea, y las líneas siguientes van debajo. */
  const float lineHeight = TextHeight(ptSize);
  float line = y + lineHeight;
  size_t start = 0;
  while (start <= text.size()) {
    const size_t nl = text.find('\n', start);
    const std::string chunk = text.substr(
        start, nl == std::string::npos ? std::string::npos : nl - start);
    BLF_position(m_fontid, x, FlipY(m_height, line), 0.0f);
    BLF_draw(m_fontid, chunk.c_str(), chunk.size());
    if (nl == std::string::npos) {
      break;
    }
    start = nl + 1;
    line += lineHeight;
  }
}

float FL_UiCanvas::TextWidth(const std::string &text, int ptSize)
{
  if (m_fontid < 0) {
    m_fontid = BLF_default();
  }
  BLF_size(m_fontid, float(ptSize));
  return BLF_width(m_fontid, text.c_str(), text.size());
}

float FL_UiCanvas::TextHeight(int ptSize)
{
  if (m_fontid < 0) {
    m_fontid = BLF_default();
  }
  BLF_size(m_fontid, float(ptSize));
  /* Mj es el par que usa bgui para medir el alto de una línea: una mayúscula con
   * una letra de cola, que cubre el alto real de la caja. */
  return BLF_height(m_fontid, "Mj", 2);
}

void FL_UiCanvas::PushClip(const FL_Rect &r)
{
  m_clipStack.push_back(r);
  GPU_scissor_test(true);
  GPU_scissor(int(r.x), int(FlipY(m_height, r.y + r.h)), int(r.w), int(r.h));
}

void FL_UiCanvas::PopClip()
{
  if (m_clipStack.empty()) {
    return;
  }
  m_clipStack.pop_back();
  if (m_clipStack.empty()) {
    GPU_scissor_test(false);
  }
  else {
    const FL_Rect &r = m_clipStack.back();
    GPU_scissor(int(r.x), int(FlipY(m_height, r.y + r.h)), int(r.w), int(r.h));
  }
}

/* ---------------------------------------------------- registro de dibujantes */

namespace {

struct DrawerRegistry {
  std::map<int, FL_UiDrawer> drawers;
  int nextId = 1;
  FL_UiCanvas canvas;
};

DrawerRegistry &Registry()
{
  static DrawerRegistry registry;
  return registry;
}

}  // namespace

int FL_UiAddDrawer(FL_UiDrawer drawer)
{
  DrawerRegistry &reg = Registry();
  const int id = reg.nextId++;
  reg.drawers[id] = std::move(drawer);
  return id;
}

void FL_UiRemoveDrawer(int id)
{
  Registry().drawers.erase(id);
}

int FL_UiDrawerCount()
{
  return int(Registry().drawers.size());
}

void FL_UiProbeTick()
{
  static const char *shotEnv = std::getenv("FL_UI_SHOT");
  static const std::string shotPath = (shotEnv && shotEnv[0]) ? shotEnv : "";
  static const char *frameEnv = std::getenv("FL_UI_SHOT_FRAME");
  static const int shotFrame = (frameEnv && frameEnv[0]) ? atoi(frameEnv) : 90;
  static const char *exitEnv = std::getenv("FL_UI_EXIT");
  static const bool exitAfter = (exitEnv && exitEnv[0] && exitEnv[0] != '0');
  static int frames = 0;
  static bool done = false;

  if (shotPath.empty()) {
    return;
  }
  ++frames;
  if (done) {
    /* La captura se encola y el motor la vuelca en EndFrame; salir en el mismo
     * tic la dejaria sin escribir. Se espera un puñado de frames. */
    if (exitAfter && frames > shotFrame + 15 && KX_GetActiveEngine()) {
      KX_GetActiveEngine()->RequestExit(KX_ExitRequest::QUIT_GAME);
    }
    return;
  }
  if (frames < shotFrame) {
    return;
  }
  done = true;

  KX_KetsjiEngine *engine = KX_GetActiveEngine();
  if (!engine || !engine->GetCanvas()) {
    CM_Error("FL_UI_SHOT: no hay lienzo del que tomar la captura");
    return;
  }
  CM_Message("FL_UI_SHOT: frame " << frames << ", capturando " << shotPath << " ("
                                  << FL_UiDrawerCount() << " dibujantes nativos)");
  /* La captura se encola y el motor la vuelca en EndFrame, DESPUES de dibujar:
   * por eso sale con la interfaz nativa ya pintada encima. */
  engine->GetCanvas()->MakeScreenShot(shotPath);
}

void FL_UiDrawAll(int width, int height)
{
  FL_UiMaybeAddDemo();

  DrawerRegistry &reg = Registry();
  if (reg.drawers.empty() || width <= 0 || height <= 0) {
    return;
  }
  reg.canvas.BeginFrame(width, height);
  for (auto &pair : reg.drawers) {
    if (pair.second) {
      pair.second(reg.canvas);
    }
  }
  reg.canvas.EndFrame();
}

}  // namespace flipendo
