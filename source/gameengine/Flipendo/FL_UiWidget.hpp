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
  /// Después de pintar los hijos. bgui lo usa para devolver el estado al reposo.
  virtual void PostDraw(FL_UiCanvas &canvas)
  {
    (void)canvas;
  }

  /* Ganchos internos del widget, que corren ANTES de la llamada del usuario.
   * Son los `_handle_*` de bgui: el widget reacciona a su estado (un botón se
   * aclara al pasar por encima) y además avisa a quien se haya suscrito. */
  virtual void HandleClick() {}
  virtual void HandleRelease() {}
  virtual void HandleHover() {}
  virtual void HandleActive() {}
  virtual void HandleMouseEnter() {}
  virtual void HandleMouseExit() {}

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

/** Imagen, como `bgui.Image`.
 *
 * La fuente puede ser una textura de GPU directa o el **nombre de una imagen del
 * `.blend`** (sin el prefijo "IM"). Esto último es lo que convierte el widget en
 * un minimapa o un monitor de vigilancia: si esa imagen es la que `FL_RenderToTexture`
 * está refrescando, el HUD enseña en vivo lo que ve la cámara secundaria. */
class FL_UiImage : public FL_UiWidget {
 public:
  FL_UiImage(const std::string &name = "image", int options = FL_UI_DEFAULT);

  /// Textura directa. Si es nullptr, se resuelve `imageName` cada frame.
  ::GPUTexture *texture = nullptr;
  /// Nombre de una imagen del .blend (sin "IM"). Se resuelve al dibujar.
  std::string imageName;
  /// Tinte (blanco = la imagen tal cual).
  FL_Color color{1.0f, 1.0f, 1.0f, 1.0f};
  /// Coordenadas de textura de las esquinas, en el orden de bgui.
  float texco[4][2] = {{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}};

 protected:
  void Draw(FL_UiCanvas &canvas) override;
};

/** Textura de GPU de una imagen del `.blend`, por nombre y sin el prefijo "IM".
 *
 * Devuelve la que la imagen YA tenga en su ranura si la hay —que es la que
 * VideoTexture pudo haber intercambiado por el render de una cámara— y sólo pide
 * una nueva a Blender cuando la ranura está vacía. Al revés se le quitaría a
 * `FL_RenderToTexture` la textura que acaba de poner. */
::GPUTexture *FL_UiTextureFromImage(const std::string &name);

/** Botón de color con texto centrado, como `bgui.FrameButton`.
 *
 * Los colores y el comportamiento son los de bgui: cuatro colores de base (por
 * defecto gris 0,4 arriba y 0,7 abajo), **+0,1 en RGB al pasar el ratón por
 * encima** y **−0,1 mientras se mantiene pulsado**, y vuelta al reposo después
 * de pintar.
 *
 * Diferencia deliberada: bgui compone el botón con un `Frame` y un `Label`
 * hijos; aquí es una hoja que se pinta ella misma y centra el texto midiéndolo
 * en el momento de dibujar. El resultado en pantalla es el mismo y se ahorra un
 * árbol de tres nodos por botón. */
class FL_UiFrameButton : public FL_UiWidget {
 public:
  FL_UiFrameButton(const std::string &name = "button", int options = FL_UI_DEFAULT);

  std::string text;
  int ptSize = 20;
  FL_Color textColor{1.0f, 1.0f, 1.0f, 1.0f};

  /// Colores de base de las cuatro esquinas (arriba-izq, arriba-der, abajo-der, abajo-izq).
  FL_Color baseColors[4];
  float border = 1.0f;
  FL_Color borderColor{0.0f, 0.0f, 0.0f, 1.0f};

  /// Pone el mismo color de base en las cuatro esquinas.
  void SetBaseColor(const FL_Color &c);

 protected:
  void Draw(FL_UiCanvas &canvas) override;
  void PostDraw(FL_UiCanvas &canvas) override;
  void HandleHover() override;
  void HandleActive() override;

 private:
  void Tint(float delta);
  FL_Color m_drawColors[4];
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

/** Autoprueba del reparto de eventos, sin ratón de verdad: con `FL_UI_SELFTEST=1`
 * se envían posiciones y estados sintéticos al árbol de demostración y se
 * imprime, en orden, qué widget recibió qué. La salida es determinista, así que
 * comparar los dos binarios es un `diff`. La llama `FL_UiRunAll`. */
void FL_UiRunSelfTest();

}  // namespace flipendo

#endif  // __FL_UIWIDGET_HPP__
