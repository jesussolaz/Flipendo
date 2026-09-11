/* FL_UiCanvas — lienzo 2D NATIVO en C++ para la interfaz de juego de Flipendo.
 *
 * Es la primera pieza del sustituto de `bgui` (2.391 líneas de Python que un
 * juego sin intérprete no puede usar). Ver politicas/UI-JUEGO-NATIVA.md.
 *
 * Dibuja en coordenadas de PANTALLA con el origen ARRIBA A LA IZQUIERDA, que es
 * como piensa bgui y como ya dibuja `RAS_DebugDraw::RenderBox2D`. Se apoya en
 * `GPU_immediate` y en `BLF`, los mismos que usa el rasterizador para su propio
 * dibujo 2D, así que no depende de Python por ningún lado.
 *
 * El motor llama a `FL_UiDrawAll()` una vez por frame, justo donde antes se
 * llamaba a los callbacks POST_DRAW de Python. */
#ifndef __FL_UICANVAS_HPP__
#define __FL_UICANVAS_HPP__

#include <functional>
#include <string>
#include <vector>

/* Ojo: esta declaración adelantada va FUERA del namespace a propósito. Escribir
 * `struct GPUTexture *` dentro de `namespace flipendo` declararía un tipo nuevo,
 * `flipendo::GPUTexture`, y las funciones de GPU dejarían de encajar. Es la misma
 * trampa que dejó el árbol sin enlazar esta noche con
 * `blender::ed::object::VIEW3D_OT_transform_gizmo_set`. */
struct GPUTexture;

namespace flipendo {

/** Color RGBA en 0..1. Struct simple a propósito: esto se pasa por valor miles
 * de veces por frame. */
struct FL_Color {
  float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
  FL_Color() = default;
  FL_Color(float rr, float gg, float bb, float aa = 1.0f) : r(rr), g(gg), b(bb), a(aa) {}
  const float *Value() const
  {
    return &r;
  }
};

/** Rectángulo en píxeles, origen arriba a la izquierda. */
struct FL_Rect {
  float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
  FL_Rect() = default;
  FL_Rect(float xx, float yy, float ww, float hh) : x(xx), y(yy), w(ww), h(hh) {}
  bool Contains(float px, float py) const
  {
    return px >= x && px <= x + w && py >= y && py <= y + h;
  }
};

class FL_UiCanvas {
 public:
  int Width() const
  {
    return m_width;
  }
  int Height() const
  {
    return m_height;
  }

  /// Rectángulo de color plano.
  void Rect(const FL_Rect &r, const FL_Color &color);
  /** Rectángulo con un color por esquina, en el orden de bgui:
   * arriba-izquierda, arriba-derecha, abajo-derecha, abajo-izquierda. */
  void GradientRect(const FL_Rect &r,
                    const FL_Color &c0,
                    const FL_Color &c1,
                    const FL_Color &c2,
                    const FL_Color &c3);
  /// Borde de `thickness` píxeles por dentro del rectángulo.
  void Border(const FL_Rect &r, float thickness, const FL_Color &color);

  /** Rectángulo con textura, teñido por `tint` (blanco = sin teñir).
   * `texco` son las coordenadas de textura de las esquinas, en el orden de bgui
   * (abajo-izq, abajo-der, arriba-der, arriba-izq); nullptr = 0..1. */
  void TexturedRect(const FL_Rect &r,
                    ::GPUTexture *texture,
                    const FL_Color &tint = FL_Color(),
                    const float texco[4][2] = nullptr);

  /// Texto. `x`,`y` es la esquina superior izquierda de la primera línea.
  void Text(const std::string &text, float x, float y, int ptSize, const FL_Color &color);
  /// Ancho en píxeles que ocuparía ese texto.
  float TextWidth(const std::string &text, int ptSize);
  /// Alto en píxeles de una línea a ese tamaño.
  float TextHeight(int ptSize);

  /// Recorte (lo que bgui llama BGUI_OVERFLOW_HIDDEN y el scroll del ListBox).
  void PushClip(const FL_Rect &r);
  void PopClip();

  /// Fuente a usar; por defecto la de la interfaz de Blender.
  void SetFont(int fontid)
  {
    m_fontid = fontid;
  }
  int Font() const
  {
    return m_fontid;
  }

  /* Lo llama el motor, no el juego. */
  void BeginFrame(int width, int height);
  void EndFrame();

 private:
  int m_width = 0;
  int m_height = 0;
  int m_fontid = -1;
  std::vector<FL_Rect> m_clipStack;
};

/// Una función que pinta. El juego registra las suyas y el motor las llama.
using FL_UiDrawer = std::function<void(FL_UiCanvas &)>;

/** Registra un dibujante y devuelve su identificador, para poder quitarlo.
 * Los identificadores no se reutilizan. */
int FL_UiAddDrawer(FL_UiDrawer drawer);
/// Quita un dibujante por identificador. Silencioso si ya no estaba.
void FL_UiRemoveDrawer(int id);
/// Cuántos dibujantes hay registrados.
int FL_UiDrawerCount();

/** Sonda de verificación de la interfaz nativa, del estilo de los `--fl-dump-*`
 * del editor (el Player no tiene analizador de opciones largas):
 *
 *   FL_UI_DEMO        "1" registra un dibujante de demostración
 *   FL_UI_SHOT        ruta del PNG con la captura de pantalla
 *   FL_UI_SHOT_FRAME  en qué frame capturarla (90 por defecto)
 *   FL_UI_EXIT        "1" para salir del juego después de capturar
 *
 * La llama `FL_ComponentManager::Tick` una vez por frame. */
void FL_UiProbeTick();

/** Con `FL_UI_DEMO=1`, monta un árbol de widgets de demostración y lo engancha al
 * lienzo. Lo define `FL_UiWidget.cpp`, para que el lienzo no tenga que saber nada
 * de widgets. No hace nada si la variable no está. */
void FL_UiMaybeAddDemo();

/** Pinta todo lo registrado. La llama el rasterizador una vez por frame, en el
 * mismo punto en el que se llamaba a los callbacks POST_DRAW de Python. */
void FL_UiDrawAll(int width, int height);

}  // namespace flipendo

#endif  // __FL_UICANVAS_HPP__
