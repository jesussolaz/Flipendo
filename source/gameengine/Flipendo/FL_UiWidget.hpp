/* FL_UiWidget — árbol de widgets NATIVO en C++ para la interfaz de juego.
 *
 * Segunda pieza del sustituto de `bgui` (ver politicas/UI-JUEGO-NATIVA.md). El
 * lienzo (`FL_UiCanvas`) sabe pintar; esto sabe **dónde** y **quién** recibe los
 * eventos. Reproduce la semántica de `bgui.Widget` sin una línea de Python:
 *
 *   - jerarquía padre/hijos con nombre y `z_index`
 *   - tamaño y posición normalizados (0..1 respecto al padre) o en píxeles
 *   - centrado en X y en Y, y relación de aspecto forzada
 *   - `visible` y `frozen` (no acepta eventos)
 *   - los seis eventos de bgui: click, release, hover, active, mouse_enter,
 *     mouse_exit
 *
 * Diferencia deliberada con bgui: aquí TODO va en coordenadas de pantalla con el
 * origen arriba a la izquierda, que es como dibuja el rasterizador. bgui mezcla
 * los dos criterios y por eso su `_update_position` es tan enrevesada. */
#ifndef __FL_UIWIDGET_HPP__
#define __FL_UIWIDGET_HPP__

#include "FL_UiCanvas.hpp"

#include <memory>
#include <string>
#include <vector>

namespace flipendo {

/// Las mismas banderas que `BGUI_*`, con los mismos valores.
enum FL_UiOption {
  FL_UI_DEFAULT = 0,
  FL_UI_CENTERX = 1,
  FL_UI_CENTERY = 2,
  FL_UI_NO_NORMALIZE = 4,
  FL_UI_NO_FOCUS = 16,
  FL_UI_CENTERED = FL_UI_CENTERX | FL_UI_CENTERY,
};

/// Estado del botón principal del ratón en este frame.
enum class FL_UiMouse { None, Click, Release, Active };

class FL_UiWidget;
using FL_UiCallback = std::function<void(FL_UiWidget &)>;

class FL_UiWidget {
 public:
  FL_UiWidget(const std::string &name = "widget", int options = FL_UI_DEFAULT);
  virtual ~FL_UiWidget() = default;

  /* ---- árbol ---- */
  /// Añade un hijo y se queda con su propiedad. Devuelve el puntero, para encadenar.
  template<class T> T *Add(std::unique_ptr<T> child)
  {
    T *raw = child.get();
    raw->m_parent = this;
    m_children.push_back(std::move(child));
    return raw;
  }
  /// Busca un descendiente por nombre (primero en anchura). nullptr si no está.
  FL_UiWidget *Find(const std::string &name);
  FL_UiWidget *Parent() const
  {
    return m_parent;
  }
  const std::string &Name() const
  {
    return m_name;
  }

  /* ---- colocación ---- */
  /** Tamaño y posición. Si el widget NO lleva `FL_UI_NO_NORMALIZE`, ambos van en
   * fracción del padre (0..1); si la lleva, en píxeles. */
  void SetSize(float w, float h);
  void SetPosition(float x, float y);
  /// Fuerza la relación ancho/alto (como el `aspect` de bgui). 0 = sin forzar.
  void SetAspect(float aspect);
  /// Rectángulo ya resuelto, en píxeles absolutos de pantalla.
  const FL_Rect &Frame() const
  {
    return m_frame;
  }

  /* ---- estado ---- */
  bool visible = true;
  bool frozen = false;
  int zIndex = 0;
  int Options() const
  {
    return m_options;
  }

  /* ---- eventos ---- */
  FL_UiCallback onClick;
  FL_UiCallback onRelease;
  FL_UiCallback onHover;
  FL_UiCallback onActive;
  FL_UiCallback onMouseEnter;
  FL_UiCallback onMouseExit;
  bool Hovered() const
  {
    return m_hover;
  }

  /* ---- lo llama el sistema ---- */
  void Layout(const FL_Rect &parentFrame);
  void DrawTree(FL_UiCanvas &canvas);
  /// Reparte el ratón. Devuelve true si este widget o un hijo se lo quedó.
  bool DispatchMouse(float x, float y, FL_UiMouse event);

 protected:
  /// Lo que pinta este widget (los hijos se pintan solos, después).
  virtual void Draw(FL_UiCanvas &canvas)
  {
    (void)canvas;
  }

  std::string m_name;
  int m_options = FL_UI_DEFAULT;
  float m_size[2] = {1.0f, 1.0f};
  float m_pos[2] = {0.0f, 0.0f};
  float m_aspect = 0.0f;
  FL_Rect m_frame;
  bool m_hover = false;
  FL_UiWidget *m_parent = nullptr;
  std::vector<std::unique_ptr<FL_UiWidget>> m_children;
};

/* ------------------------------------------------------------- los widgets */

/// Rectángulo con un color por esquina y borde, como `bgui.Frame`.
class FL_UiFrame : public FL_UiWidget {
 public:
  FL_UiFrame(const std::string &name = "frame", int options = FL_UI_DEFAULT);

  /// Colores de las cuatro esquinas: arriba-izq, arriba-der, abajo-der, abajo-izq.
  FL_Color colors[4];
  float border = 0.0f;
  FL_Color borderColor{0.0f, 0.0f, 0.0f, 1.0f};

  /// Atajo para poner el mismo color en las cuatro esquinas.
  void SetColor(const FL_Color &c);

 protected:
  void Draw(FL_UiCanvas &canvas) override;
};

/// Texto de una o varias líneas, como `bgui.Label`.
class FL_UiLabel : public FL_UiWidget {
 public:
  FL_UiLabel(const std::string &name = "label", int options = FL_UI_DEFAULT);

  std::string text;
  int ptSize = 18;
  FL_Color color{1.0f, 1.0f, 1.0f, 1.0f};

 protected:
  void Draw(FL_UiCanvas &canvas) override;
};

/* ------------------------------------------------------------- el sistema */

/** Raíz del árbol: se dimensiona con la pantalla, se dibuja sola y reparte el
 * ratón. Es `bgui.System` + `bgui_utils.System` en una pieza, porque sin Python
 * no hay motivo para separarlas. */
class FL_UiSystem : public FL_UiWidget {
 public:
  FL_UiSystem();
  ~FL_UiSystem() override;

  /// Registra el sistema en el lienzo para que el motor lo pinte cada frame.
  void Attach();
  /// Lo saca del lienzo.
  void Detach();

  /// Una vez por frame: lee el ratón del motor y reparte los eventos.
  void Run();

  /// Widget con el foco (el último que recibió un click). nullptr si ninguno.
  FL_UiWidget *Focused() const
  {
    return m_focused;
  }

 private:
  void DrawRoot(FL_UiCanvas &canvas);

  int m_drawerId = 0;
  FL_UiWidget *m_focused = nullptr;
  int m_width = 0;
  int m_height = 0;
};

/** Llama a `Run()` de todos los sistemas enganchados. La llama el motor una vez
 * por frame; un juego que prefiera hacerlo a mano puede no enganchar el sistema y
 * llamar a `Run()` desde su propio `FL_Component`. */
void FL_UiRunAll();

}  // namespace flipendo

#endif  // __FL_UIWIDGET_HPP__
