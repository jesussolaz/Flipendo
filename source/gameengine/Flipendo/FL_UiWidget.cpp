/* Implementación del árbol de widgets nativo. Ver FL_UiWidget.hpp. */
#include "FL_UiWidget.hpp"

#include "BKE_context.hh"
#include "BKE_image.hh"
#include "BKE_main.hh"
#include "BLI_listbase.h"
#include "DNA_ID.h"
#include "DNA_image_types.h"

#include "CM_Message.hpp"
#include "KX_Globals.hpp"
#include "KX_KetsjiEngine.hpp"
#include "RAS_ICanvas.hpp"
#include "SCA_IInputDevice.hpp"
#include "SCA_InputEvent.hpp"

#include <algorithm>
#include <cstdlib>

namespace flipendo {

/* --------------------------------------------------------------- FL_UiWidget */

FL_UiWidget::FL_UiWidget(const std::string &name, int options)
    : m_name(name), m_options(options)
{
}

FL_UiWidget *FL_UiWidget::Find(const std::string &name)
{
  for (auto &child : m_children) {
    if (child->m_name == name) {
      return child.get();
    }
  }
  for (auto &child : m_children) {
    if (FL_UiWidget *found = child->Find(name)) {
      return found;
    }
  }
  return nullptr;
}

void FL_UiWidget::SetSize(float w, float h)
{
  m_size[0] = w;
  m_size[1] = h;
}

void FL_UiWidget::SetPosition(float x, float y)
{
  m_pos[0] = x;
  m_pos[1] = y;
}

void FL_UiWidget::SetAspect(float aspect)
{
  m_aspect = aspect;
}

void FL_UiWidget::Layout(const FL_Rect &parentFrame)
{
  const bool pixels = (m_options & FL_UI_NO_NORMALIZE) != 0;

  float w = pixels ? m_size[0] : m_size[0] * parentFrame.w;
  float h = pixels ? m_size[1] : m_size[1] * parentFrame.h;

  /* Relación de aspecto: manda el alto, como en bgui. */
  if (m_aspect > 0.0f) {
    w = h * m_aspect;
  }

  float x = pixels ? m_pos[0] : m_pos[0] * parentFrame.w;
  float y = pixels ? m_pos[1] : m_pos[1] * parentFrame.h;

  if (m_options & FL_UI_CENTERX) {
    x = (parentFrame.w - w) * 0.5f;
  }
  if (m_options & FL_UI_CENTERY) {
    y = (parentFrame.h - h) * 0.5f;
  }

  m_frame = FL_Rect(parentFrame.x + x, parentFrame.y + y, w, h);

  for (auto &child : m_children) {
    child->Layout(m_frame);
  }
}

void FL_UiWidget::DrawTree(FL_UiCanvas &canvas)
{
  if (!visible) {
    return;
  }
  Draw(canvas);

  /* Los hijos, de menor a mayor z_index. Se ordenan por índice, no por puntero,
   * para que el orden sea estable entre frames. */
  std::vector<FL_UiWidget *> orden;
  orden.reserve(m_children.size());
  for (auto &child : m_children) {
    orden.push_back(child.get());
  }
  std::stable_sort(orden.begin(), orden.end(), [](FL_UiWidget *a, FL_UiWidget *b) {
    return a->zIndex < b->zIndex;
  });
  for (FL_UiWidget *child : orden) {
    child->DrawTree(canvas);
  }
  PostDraw(canvas);
}

bool FL_UiWidget::DispatchMouse(float x, float y, FL_UiMouse event)
{
  if (!visible) {
    return false;
  }

  /* Los hijos primero y de arriba abajo: el que está más al frente se lo queda. */
  std::vector<FL_UiWidget *> orden;
  orden.reserve(m_children.size());
  for (auto &child : m_children) {
    orden.push_back(child.get());
  }
  std::stable_sort(orden.begin(), orden.end(), [](FL_UiWidget *a, FL_UiWidget *b) {
    return a->zIndex > b->zIndex;
  });

  bool tomado = false;
  for (FL_UiWidget *child : orden) {
    if (child->DispatchMouse(x, y, event)) {
      tomado = true;
      break;
    }
  }

  const bool dentro = !tomado && m_frame.Contains(x, y);

  /* Entrar y salir se avisan aunque el widget esté congelado: son informativos. */
  if (dentro && !m_hover) {
    m_hover = true;
    HandleMouseEnter();
    if (onMouseEnter) {
      onMouseEnter(*this);
    }
  }
  else if (!dentro && m_hover) {
    m_hover = false;
    HandleMouseExit();
    if (onMouseExit) {
      onMouseExit(*this);
    }
  }

  if (!dentro || frozen || (m_options & FL_UI_NO_FOCUS)) {
    return tomado;
  }

  HandleHover();
  if (onHover) {
    onHover(*this);
  }
  switch (event) {
    case FL_UiMouse::Click:
      HandleClick();
      if (onClick) {
        onClick(*this);
      }
      break;
    case FL_UiMouse::Release:
      HandleRelease();
      if (onRelease) {
        onRelease(*this);
      }
      break;
    case FL_UiMouse::Active:
      HandleActive();
      if (onActive) {
        onActive(*this);
      }
      break;
    case FL_UiMouse::None:
      break;
  }
  return true;
}

/* ---------------------------------------------------------------- FL_UiFrame */

FL_UiFrame::FL_UiFrame(const std::string &name, int options) : FL_UiWidget(name, options)
{
  SetColor(FL_Color(0.0f, 0.0f, 0.0f, 0.0f));
}

void FL_UiFrame::SetColor(const FL_Color &c)
{
  for (int i = 0; i < 4; ++i) {
    colors[i] = c;
  }
}

void FL_UiFrame::Draw(FL_UiCanvas &canvas)
{
  canvas.GradientRect(m_frame, colors[0], colors[1], colors[2], colors[3]);
  if (border > 0.0f) {
    canvas.Border(m_frame, border, borderColor);
  }
}

/* ---------------------------------------------------------------- FL_UiLabel */

FL_UiLabel::FL_UiLabel(const std::string &name, int options) : FL_UiWidget(name, options)
{
}

void FL_UiLabel::Draw(FL_UiCanvas &canvas)
{
  if (text.empty()) {
    return;
  }
  canvas.Text(text, m_frame.x, m_frame.y, ptSize, color);
}

/* ---------------------------------------------------------------- FL_UiImage */

FL_UiImage::FL_UiImage(const std::string &name, int options) : FL_UiWidget(name, options)
{
}

::GPUTexture *FL_UiTextureFromImage(const std::string &name)
{
  KX_KetsjiEngine *engine = KX_GetActiveEngine();
  bContext *C = engine ? engine->GetContext() : nullptr;
  Main *bmain = C ? CTX_data_main(C) : nullptr;
  if (!bmain) {
    return nullptr;
  }
  Image *ima = (Image *)BLI_findstring(&bmain->images, name.c_str(), offsetof(ID, name) + 2);
  if (!ima) {
    return nullptr;
  }
  if (ima->gputexture[TEXTARGET_2D][0]) {
    return ima->gputexture[TEXTARGET_2D][0];
  }
  return BKE_image_get_gpu_texture(ima, nullptr);
}

void FL_UiImage::Draw(FL_UiCanvas &canvas)
{
  ::GPUTexture *tex = texture;
  if (!tex && !imageName.empty()) {
    tex = FL_UiTextureFromImage(imageName);
  }
  if (!tex) {
    return;
  }
  canvas.TexturedRect(m_frame, tex, color, texco);
}

/* ---------------------------------------------------------- FL_UiFrameButton */

FL_UiFrameButton::FL_UiFrameButton(const std::string &name, int options)
    : FL_UiWidget(name, options)
{
  /* Los mismos valores por defecto que el tema de bgui para FrameButton. */
  baseColors[0] = FL_Color(0.4f, 0.4f, 0.4f, 1.0f);
  baseColors[1] = FL_Color(0.4f, 0.4f, 0.4f, 1.0f);
  baseColors[2] = FL_Color(0.7f, 0.7f, 0.7f, 1.0f);
  baseColors[3] = FL_Color(0.7f, 0.7f, 0.7f, 1.0f);
  for (int i = 0; i < 4; ++i) {
    m_drawColors[i] = baseColors[i];
  }
}

void FL_UiFrameButton::SetBaseColor(const FL_Color &c)
{
  for (int i = 0; i < 4; ++i) {
    baseColors[i] = c;
    m_drawColors[i] = c;
  }
}

void FL_UiFrameButton::Tint(float delta)
{
  for (int i = 0; i < 4; ++i) {
    m_drawColors[i] = FL_Color(baseColors[i].r + delta,
                               baseColors[i].g + delta,
                               baseColors[i].b + delta,
                               baseColors[i].a);
  }
}

void FL_UiFrameButton::HandleHover()
{
  Tint(0.1f);
}

void FL_UiFrameButton::HandleActive()
{
  Tint(-0.1f);
}

void FL_UiFrameButton::Draw(FL_UiCanvas &canvas)
{
  canvas.GradientRect(m_frame, m_drawColors[0], m_drawColors[1], m_drawColors[2], m_drawColors[3]);
  if (border > 0.0f) {
    canvas.Border(m_frame, border, borderColor);
  }
  if (!text.empty()) {
    /* Centrado midiendo el texto en el momento de pintar, que es cuando hay
     * fuente cargada y tamaño fijado. */
    const float w = canvas.TextWidth(text, ptSize);
    const float h = canvas.TextHeight(ptSize);
    canvas.Text(text,
                m_frame.x + (m_frame.w - w) * 0.5f,
                m_frame.y + (m_frame.h - h) * 0.5f,
                ptSize,
                textColor);
  }
}

void FL_UiFrameButton::PostDraw(FL_UiCanvas &canvas)
{
  (void)canvas;
  /* Igual que bgui: el tinte del estado dura un frame y luego se vuelve al reposo.
   * Si el ratón sigue encima, el reparto del frame siguiente lo vuelve a poner. */
  for (int i = 0; i < 4; ++i) {
    m_drawColors[i] = baseColors[i];
  }
}

/* --------------------------------------------------------------- FL_UiSystem */

FL_UiSystem::FL_UiSystem() : FL_UiWidget("<System>", FL_UI_NO_NORMALIZE | FL_UI_NO_FOCUS)
{
}

FL_UiSystem::~FL_UiSystem()
{
  Detach();
}

/* Sistemas enganchados, para que el motor pueda tickearlos. No son duennos: el
 * duenno es quien creo el sistema. */
static std::vector<FL_UiSystem *> &EnganchadosLista()
{
  static std::vector<FL_UiSystem *> lista;
  return lista;
}

void FL_UiSystem::Attach()
{
  if (m_drawerId != 0) {
    return;
  }
  m_drawerId = FL_UiAddDrawer([this](FL_UiCanvas &canvas) { this->DrawRoot(canvas); });
  EnganchadosLista().push_back(this);
}

void FL_UiSystem::Detach()
{
  if (m_drawerId != 0) {
    FL_UiRemoveDrawer(m_drawerId);
    m_drawerId = 0;
  }
  std::vector<FL_UiSystem *> &lista = EnganchadosLista();
  lista.erase(std::remove(lista.begin(), lista.end(), this), lista.end());
}

void FL_UiRunAll()
{
  FL_UiRunSelfTest();
  for (FL_UiSystem *sistema : EnganchadosLista()) {
    sistema->Run();
  }
}

void FL_UiSystem::DrawRoot(FL_UiCanvas &canvas)
{
  /* El sistema siempre ocupa la pantalla; si ha cambiado de tamaño, se recoloca
   * el árbol entero, que es lo que hace `bgui.System.render()`. */
  if (canvas.Width() != m_width || canvas.Height() != m_height) {
    m_width = canvas.Width();
    m_height = canvas.Height();
    SetSize(float(m_width), float(m_height));
    SetPosition(0.0f, 0.0f);
    Layout(FL_Rect(0.0f, 0.0f, float(m_width), float(m_height)));
  }
  DrawTree(canvas);
}

void FL_UiSystem::Run()
{
  KX_KetsjiEngine *engine = KX_GetActiveEngine();
  if (!engine) {
    return;
  }
  SCA_IInputDevice *dev = engine->GetInputDevice();
  RAS_ICanvas *canvas = engine->GetCanvas();
  if (!dev || !canvas) {
    return;
  }

  /* El motor da la posición del ratón en píxeles de ventana, con el origen
   * arriba a la izquierda: el mismo criterio que el árbol. */
  const SCA_InputEvent &ex = dev->GetInput(SCA_IInputDevice::MOUSEX);
  const SCA_InputEvent &ey = dev->GetInput(SCA_IInputDevice::MOUSEY);
  const float mx = ex.m_values.empty() ? -1.0f : float(ex.m_values.back());
  const float my = ey.m_values.empty() ? -1.0f : float(ey.m_values.back());

  const SCA_InputEvent &left = dev->GetInput(SCA_IInputDevice::LEFTMOUSE);
  FL_UiMouse estado = FL_UiMouse::None;
  if (left.Find(SCA_InputEvent::JUSTACTIVATED)) {
    estado = FL_UiMouse::Click;
  }
  else if (left.Find(SCA_InputEvent::JUSTRELEASED)) {
    estado = FL_UiMouse::Release;
  }
  else if (left.Find(SCA_InputEvent::ACTIVE)) {
    estado = FL_UiMouse::Active;
  }

  DispatchMouse(mx, my, estado);

  if (estado == FL_UiMouse::Click) {
    /* El foco se queda en el widget más al frente que contiene el ratón. */
    m_focused = nullptr;
    for (auto &child : m_children) {
      if (child->Frame().Contains(mx, my) && !(child->Options() & FL_UI_NO_FOCUS)) {
        m_focused = child.get();
      }
    }
  }
}

/* ------------------------------------------------- demostracion verificable */

/* El arbol de demostracion vive aqui para que la autoprueba pueda empujarle
 * eventos sinteticos. */
static std::unique_ptr<FL_UiSystem> &DemoSistema()
{
  static std::unique_ptr<FL_UiSystem> sistema;
  return sistema;
}

/* Registro de lo que ha recibido cada widget, en orden. Es la evidencia. */
static std::vector<std::string> &DemoEventos()
{
  static std::vector<std::string> eventos;
  return eventos;
}

static void Apunta(const std::string &quien, const char *que)
{
  DemoEventos().push_back(quien + ":" + que);
}

static void SuscribeTodo(FL_UiWidget *w)
{
  const std::string quien = w->Name();
  w->onMouseEnter = [quien](FL_UiWidget &) { Apunta(quien, "entra"); };
  w->onMouseExit = [quien](FL_UiWidget &) { Apunta(quien, "sale"); };
  w->onHover = [quien](FL_UiWidget &) { Apunta(quien, "encima"); };
  w->onClick = [quien](FL_UiWidget &) { Apunta(quien, "pulsa"); };
  w->onActive = [quien](FL_UiWidget &) { Apunta(quien, "mantiene"); };
  w->onRelease = [quien](FL_UiWidget &) { Apunta(quien, "suelta"); };
}

/* Con FL_UI_DEMO=1 se monta un arbol de widgets: un panel (FL_UiFrame) con
 * degradado de cuatro esquinas y borde, dos etiquetas (FL_UiLabel) y una barra de
 * progreso hecha con dos marcos anidados. Sirve de linea base para comparar los
 * dos binarios y, sobre todo, para que la prueba use el arbol y no el lienzo a
 * pelo: asi se verifica tambien la colocacion normalizada y el anidamiento. */
void FL_UiMaybeAddDemo()
{
  static bool preguntado = false;
  if (preguntado) {
    return;
  }
  preguntado = true;

  const char *v = std::getenv("FL_UI_DEMO");
  if (!v || v[0] == '\0' || v[0] == '0') {
    return;
  }

  DemoSistema() = std::make_unique<FL_UiSystem>();
  FL_UiSystem *sistema = DemoSistema().get();

  auto panelPtr = std::make_unique<FL_UiFrame>("panel", FL_UI_NO_NORMALIZE);
  FL_UiFrame *panel = sistema->Add(std::move(panelPtr));
  panel->SetPosition(40.0f, 40.0f);
  panel->SetSize(420.0f, 195.0f);
  panel->colors[0] = FL_Color(0.05f, 0.08f, 0.14f, 0.92f);
  panel->colors[1] = FL_Color(0.10f, 0.14f, 0.24f, 0.92f);
  panel->colors[2] = FL_Color(0.04f, 0.06f, 0.10f, 0.92f);
  panel->colors[3] = FL_Color(0.02f, 0.03f, 0.06f, 0.92f);
  panel->border = 2.0f;
  panel->borderColor = FL_Color(0.95f, 0.62f, 0.10f, 1.0f);

  FL_UiLabel *titulo = panel->Add(std::make_unique<FL_UiLabel>("titulo", FL_UI_NO_NORMALIZE));
  titulo->SetPosition(18.0f, 16.0f);
  titulo->SetSize(200.0f, 40.0f);
  titulo->text = "FLIPENDO";
  titulo->ptSize = 34;
  titulo->color = FL_Color(0.98f, 0.72f, 0.20f, 1.0f);

  FL_UiLabel *texto = panel->Add(std::make_unique<FL_UiLabel>("texto", FL_UI_NO_NORMALIZE));
  texto->SetPosition(18.0f, 62.0f);
  texto->SetSize(380.0f, 40.0f);
  texto->text = "interfaz de juego NATIVA, sin Python\narbol FL_UiWidget sobre FL_UiCanvas";
  texto->ptSize = 18;
  texto->color = FL_Color(0.90f, 0.92f, 0.96f, 1.0f);

  /* La barra: el fondo en pixeles dentro del panel, y el relleno NORMALIZADO
   * dentro del fondo, para que la prueba ejercite los dos modos de colocacion. */
  FL_UiFrame *barra = panel->Add(std::make_unique<FL_UiFrame>("barra", FL_UI_NO_NORMALIZE));
  barra->SetPosition(18.0f, 120.0f);
  barra->SetSize(384.0f, 12.0f);
  barra->SetColor(FL_Color(0.12f, 0.14f, 0.18f, 1.0f));

  FL_UiFrame *relleno = barra->Add(std::make_unique<FL_UiFrame>("relleno"));
  relleno->SetPosition(0.0f, 0.0f);
  relleno->SetSize(0.62f, 1.0f);
  relleno->SetColor(FL_Color(0.20f, 0.80f, 0.35f, 1.0f));

  /* El boton: coloreado como el tema por defecto de bgui.FrameButton, y con todos
   * sus eventos suscritos para la autoprueba. */
  FL_UiFrameButton *boton = panel->Add(
      std::make_unique<FL_UiFrameButton>("boton", FL_UI_NO_NORMALIZE));
  boton->SetPosition(240.0f, 145.0f);
  boton->SetSize(162.0f, 34.0f);
  boton->text = "JUGAR";
  boton->ptSize = 18;
  boton->border = 1.0f;
  boton->borderColor = FL_Color(0.95f, 0.62f, 0.10f, 1.0f);
  /* El monitor: un FL_UiImage que ensenna la imagen "CctvTex" del .blend. Como en
   * esa escena es la que FL_RenderToTexture esta refrescando con la camara de
   * vigilancia, lo que sale en el HUD es la camara en vivo. Es render a textura y
   * interfaz nativa funcionando juntos, los dos sin Python. */
  FL_UiImage *monitor = panel->Add(std::make_unique<FL_UiImage>("monitor", FL_UI_NO_NORMALIZE));
  monitor->SetPosition(18.0f, 145.0f);
  monitor->SetSize(60.0f, 34.0f);
  monitor->imageName = "CctvTex";

  SuscribeTodo(boton);
  SuscribeTodo(panel);

  sistema->Attach();
  CM_Message("FL_UI_DEMO: arbol de widgets nativo montado y enganchado ("
             << "panel + 2 etiquetas + barra con relleno normalizado + boton + monitor)");
}

/* ---------------------------------------------------------- la autoprueba */

void FL_UiRunSelfTest()
{
  static bool hecho = false;
  if (hecho) {
    return;
  }
  const char *v = std::getenv("FL_UI_SELFTEST");
  if (!v || v[0] == '\0' || v[0] == '0') {
    hecho = true;
    return;
  }
  FL_UiSystem *sistema = DemoSistema().get();
  if (!sistema) {
    return;
  }
  FL_UiWidget *boton = sistema->Find("boton");
  if (!boton || boton->Frame().w <= 0.0f) {
    /* Todavia no se ha colocado el arbol (eso pasa en el primer dibujado). */
    return;
  }
  hecho = true;

  const FL_Rect &r = boton->Frame();
  const float cx = r.x + r.w * 0.5f;
  const float cy = r.y + r.h * 0.5f;
  /* Un punto que esta dentro del panel pero FUERA del boton, para comprobar que
   * el hijo se queda el evento y el padre no lo ve. */
  const float px = r.x - 60.0f;
  const float py = cy;

  struct Paso {
    const char *nombre;
    float x, y;
    FL_UiMouse estado;
  };
  const Paso pasos[] = {
      {"fuera del panel", 700.0f, 500.0f, FL_UiMouse::None},
      {"sobre el panel, fuera del boton", px, py, FL_UiMouse::None},
      {"sobre el boton", cx, cy, FL_UiMouse::None},
      {"pulsando el boton", cx, cy, FL_UiMouse::Click},
      {"manteniendo el boton", cx, cy, FL_UiMouse::Active},
      {"soltando el boton", cx, cy, FL_UiMouse::Release},
      {"fuera del panel otra vez", 700.0f, 500.0f, FL_UiMouse::None},
  };

  CM_Message("FL_UI_SELFTEST: boton en (" << int(r.x) << "," << int(r.y) << ") "
                                          << int(r.w) << "x" << int(r.h));
  for (const Paso &paso : pasos) {
    DemoEventos().clear();
    sistema->DispatchMouse(paso.x, paso.y, paso.estado);
    std::string linea;
    for (const std::string &e : DemoEventos()) {
      if (!linea.empty()) {
        linea += " ";
      }
      linea += e;
    }
    if (linea.empty()) {
      linea = "(nada)";
    }
    CM_Message("FL_UI_SELFTEST: " << paso.nombre << " -> " << linea);
  }
  CM_Message("FL_UI_SELFTEST: fin");
}

}  // namespace flipendo
