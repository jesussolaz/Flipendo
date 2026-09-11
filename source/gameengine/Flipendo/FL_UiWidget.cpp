/* Implementación del árbol de widgets nativo. Ver FL_UiWidget.hpp. */
#include "FL_UiWidget.hpp"

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
    if (onMouseEnter) {
      onMouseEnter(*this);
    }
  }
  else if (!dentro && m_hover) {
    m_hover = false;
    if (onMouseExit) {
      onMouseExit(*this);
    }
  }

  if (!dentro || frozen || (m_options & FL_UI_NO_FOCUS)) {
    return tomado;
  }

  if (onHover) {
    onHover(*this);
  }
  switch (event) {
    case FL_UiMouse::Click:
      if (onClick) {
        onClick(*this);
      }
      break;
    case FL_UiMouse::Release:
      if (onRelease) {
        onRelease(*this);
      }
      break;
    case FL_UiMouse::Active:
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

  static std::unique_ptr<FL_UiSystem> sistema;
  sistema = std::make_unique<FL_UiSystem>();

  auto panelPtr = std::make_unique<FL_UiFrame>("panel", FL_UI_NO_NORMALIZE);
  FL_UiFrame *panel = sistema->Add(std::move(panelPtr));
  panel->SetPosition(40.0f, 40.0f);
  panel->SetSize(420.0f, 150.0f);
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

  sistema->Attach();
  CM_Message("FL_UI_DEMO: arbol de widgets nativo montado y enganchado ("
             << "panel + 2 etiquetas + barra con relleno normalizado)");
}

}  // namespace flipendo
