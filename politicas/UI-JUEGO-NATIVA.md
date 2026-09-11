# Interfaz de juego nativa — inventario de `bgui` y qué haría falta en C++

> Un juego exportado con el Player sin CPython **no tiene sistema de interfaz**.
> `politicas/PLAYER-SIN-CPYTHON.md` §3.3 lo daba por deuda sin medir. Esto es la
> medida: qué ofrece `bgui` exactamente, con qué dibuja, qué hay ya en C++ en el
> motor y qué falta de verdad.

---

## 1. Qué es `bgui` y cuánto es

`scripts/modules/bgui` — **2.391 líneas de Python en 15 ficheros**. Es la librería
de interfaz que UPBGE hereda de Mitchell Stokes (MIT, 2010-2011). No es de
Blender: es del motor de juego, así que **es de Flipendo y hay que migrarla**, no
retirarla.

| Fichero | Líneas | Qué aporta |
|---|---:|---|
| `widget.py` | 522 | La clase base `Widget`: jerarquía, posición, eventos, animaciones |
| `text_input.py` | 518 | Campo de texto editable (el widget más complejo con diferencia) |
| `bgui_utils.py` | 158 | `System` para BGE: *layouts*, *overlays*, bucle por frame, entrada |
| `list_box.py` | 140 | Lista con *renderer* de filas personalizable |
| `key_defs.py` | 124 | Tabla de teclas → carácter |
| `progress_bar.py` | 124 | Barra de progreso |
| `label.py` | 117 | Texto de una o varias líneas |
| `text_block.py` | 117 | Bloque de texto con ajuste de línea |
| `image_button.py` | 102 | Botón con cuatro imágenes de estado |
| `system.py` | 100 | Raíz del árbol, foco, render |
| `frame_button.py` | 99 | Botón de color con texto |
| `theme.py` | 95 | Tema en formato INI (`configparser`) |
| `frame.py` | 84 | Rectángulo con degradado de 4 esquinas y borde |
| `image.py` | 75 | Imagen |
| `__init__.py` | 16 | Reexportación |

**Nadie lo usa todavía en Flipendo**: ni `game/anima` ni `game/template`
mencionan `bgui`. Es decir, migrarlo no rompe ninguna escena existente; lo que
hace es **devolverle a un juego exportado la posibilidad de tener interfaz**.

---

## 2. Qué ofrece, pieza a pieza

### 2.1 El árbol de widgets (`Widget`)

- **Jerarquía** padre/hijos con nombre (`OrderedDict`), `z_index` para el orden de
  dibujado, `visible` y `frozen` (no acepta eventos).
- **Posición y tamaño en dos modos**: normalizado (0..1 respecto al padre) o en
  píxeles (`BGUI_NO_NORMALIZE`), con centrado (`BGUI_CENTERX`, `BGUI_CENTERY`) y
  relación de aspecto forzada (`aspect`).
- **Seis eventos**: `on_click`, `on_release`, `on_hover`, `on_active`,
  `on_mouse_enter`, `on_mouse_exit`. Se guardan como referencias débiles
  (`WeakMethod`) para no retener objetos del juego.
- **Animaciones**: `Animation` y `ArrayAnimation` interpolan un atributo (o un
  array, como la posición) hasta un valor en un tiempo dado, con *callback* al
  terminar. `move(position, time, callback)`.
- **Opciones**: `BGUI_NO_THEME`, `BGUI_NO_FOCUS`, `BGUI_CACHE`,
  `BGUI_OVERFLOW_{NONE,HIDDEN,REPLACE,CALLBACK}`.

### 2.2 Los nueve widgets

| Widget | Lo que dibuja | Lo que expone |
|---|---|---|
| `Frame` | Rectángulo con **un color por esquina** (degradado) y borde de N píxeles | `colors[4]`, `border`, `border_color` |
| `Label` | Texto, varias líneas por `\n`, con contorno opcional | `text`, `pt_size`, `color`, `outline_color/size/smoothing`, `font` |
| `TextBlock` | Texto con ajuste automático de línea dentro de su caja | `text`, `pt_size`, `color` |
| `Image` | Imagen, con recorte (`texco`) y tinte | `image`, `texco`, `color` |
| `FrameButton` | `Frame` + `Label`, con color base y estados | `text`, `base_color` |
| `ImageButton` | Cuatro imágenes: normal, alternativa, *hover*, pulsada | `default_image`, `default2_image`, `hover_image`, `click_image` |
| `ProgressBar` | Dos `Frame` superpuestos | `percent`, `fill_colors`, `bg_colors` |
| `ListBox` | Lista con *scroll* y un `ListBoxRenderer` que decide cómo se pinta cada fila | `items`, `selected`, `renderer`, `padding` |
| `TextInput` | Campo editable: cursor, selección, doble clic, prefijo, colores por estado | `text`, `prefix`, `on_enter_key`, `select_all()`, `activate()` |

### 2.3 El sistema (`System` y `bgui_utils.System`)

- Raíz del árbol, con el **tamaño del viewport** (se reajusta solo si cambia).
- **Foco**: `focused_widget`, `lock_focus`.
- **Entrada**: `update_mouse(pos, estado)` y `update_keyboard(tecla, mayúsculas)`.
- **Layouts y overlays**: `load_layout()`, `add_overlay()`, `remove_overlay()`,
  `toggle_overlay()`. Un *layout* es una pantalla completa; los *overlays* se
  apilan encima.
- **Bucle**: `run()` se llama cada frame, actualiza layout y overlays, traduce el
  ratón y el teclado de `bge.logic` y reparte los eventos.
- **Dibujado**: se engancha a `logic.getCurrentScene().post_draw`.

### 2.4 El tema

Fichero INI leído con `configparser`, una sección por widget
(`[Frame]`, `[Label]`, …) y subtemas al estilo de las clases de CSS
(`[Frame:MiSubtema]`). Los valores son números, listas de números separadas por
comas (colores) y rutas de imagen con el prefijo `img:`.

---

## 3. Con qué dibuja y de dónde saca la entrada

Esto es lo que de verdad lo ata al intérprete:

| Necesita | Cómo lo hace hoy (Python) |
|---|---|
| Rectángulos y degradados | módulo `gpu`: `shader.from_builtin('SMOOTH_COLOR'/'UNIFORM_COLOR')` + `batch_for_shader` |
| Texto | módulo `blf`: `blf.size`, `blf.position`, `blf.color`, `blf.draw`, `blf.dimensions` |
| Imágenes | módulo `gpu` + `bge.texture` para cargarlas |
| Tamaño del viewport | `gpu.state.viewport_get()` |
| Gancho de dibujado | `bge.logic.getCurrentScene().post_draw` |
| Ratón | `bge.logic.mouse` (`position`, `inputs[LEFTMOUSE]`) |
| Teclado | `bge.logic.keyboard` (`inputs`, `activated`) |
| Tema | `configparser` (INI de la librería estándar) |

Nada de eso es lógica: es **fontanería**. La lógica —el árbol, el reparto de
eventos, el ajuste de línea, el cursor del campo de texto— es algoritmo puro y se
traduce a C++ sin perder nada.

---

## 4. Lo que YA existe en C++ en el motor (y casi nadie sabe)

**`RAS_DebugDraw` dibuja en 2D, en C++, y se compila siempre** (el Rasterizer no
depende de Python):

```cpp
void RenderBox2D(const MT_Vector2 &pos, const MT_Vector2 &size, const MT_Vector4 &color);
void RenderText2D(const std::string &text, const MT_Vector2 &pos, const MT_Vector4 &color);
```

Y su implementación (`RAS_OpenGLDebugDraw.cpp`) ya hace exactamente lo que
necesita una interfaz:

- **Rectángulos**: `immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR)` +
  `immRectf`, con la coordenada Y ya invertida a «origen arriba a la izquierda»,
  que es como piensa `bgui`.
- **Texto**: `BLF_size` / `BLF_color4fv` / `BLF_position` / `BLF_draw` sobre
  `blf_mono_font`, con sombra.

Es decir: **el 80 % de la fontanería de dibujo de `bgui` ya está escrita en C++ en
este árbol**. Lo que falta en esa capa es corto y concreto:

1. **Degradado de cuatro esquinas** (`Frame` lo usa): hoy solo hay color plano.
   Es `GPU_SHADER_3D_SMOOTH_COLOR` con cuatro vértices, el mismo patrón.
2. **Quad con textura** (`Image`, `ImageButton`): `GPU_SHADER_3D_IMAGE` y un
   `GPUTexture`, que además ya sabemos conseguir desde una `Image` de Blender por
   el trabajo de VideoTexture (§6 de `PLAYER-SIN-CPYTHON.md`).
3. **Fuente y tamaño por widget**: hoy el tamaño sale de `gm.profileSize` y la
   fuente es siempre `blf_mono_font`. Hace falta `BLF_load()` por fuente y medir
   con `BLF_width`/`BLF_height` (que es lo que `blf.dimensions` hace en Python).
4. **Recorte** (`BGUI_OVERFLOW_HIDDEN`, el *scroll* del `ListBox`): `GPU_scissor`.
5. **Un gancho de dibujado nativo.** El sitio exacto ya está marcado:
   `RAS_OpenGLDebugDraw.cpp` llama a
   `KX_GetActiveScene()->RunDrawingCallbacks(KX_Scene::POST_DRAW, nullptr)`
   **dentro de un `#ifdef WITH_PYTHON`**. Justo ahí va la lista de dibujantes
   nativos.

La entrada tampoco es problema: `SCA_IInputDevice` ya se usa desde C++ en
`FL_ArpgComponents.cpp` (`GetInput(code).Find(SCA_InputEvent::ACTIVE)`), y la
posición del ratón sale del mismo sitio del que la saca `bge.logic.mouse`.

---

## 5. Qué haría falta escribir, y dónde

Propuesta de reparto, en `source/gameengine/Flipendo/`, en el mismo estilo que
`FL_Component` y `FL_RenderToTexture`:

| Pieza | Fichero | Qué es | Tamaño estimado |
|---|---|---|---:|
| Lienzo | `FL_UiCanvas.{hpp,cpp}` | Primitivas: `Rect`, `GradientRect`, `Border`, `TexturedRect`, `Text`, `MeasureText`, `PushClip`/`PopClip`. Sobre `GPU_immediate` + `BLF`, en coordenadas de pantalla con origen arriba a la izquierda | ~350 |
| Gancho | (en `RAS_OpenGLDebugDraw.cpp` + `KX_Scene`) | Lista nativa de dibujantes por escena, llamada donde hoy está `RunDrawingCallbacks(POST_DRAW)` bajo `#ifdef WITH_PYTHON` | ~60 |
| Base | `FL_UiWidget.{hpp,cpp}` | Árbol, posición normalizada/píxeles, aspecto, centrado, `z_index`, `visible`, `frozen`, los seis eventos con `std::function`, animaciones | ~450 |
| Widgets | `FL_UiWidgets.cpp` | `Frame`, `Label`, `TextBlock`, `Image`, `FrameButton`, `ImageButton`, `ProgressBar`, `ListBox` | ~700 |
| Campo de texto | `FL_UiTextInput.{hpp,cpp}` | Cursor, selección, doble clic, prefijo, colores por estado, `on_enter_key` | ~500 |
| Sistema | `FL_UiSystem.{hpp,cpp}` | Raíz, foco, layouts y overlays, `Run()` por frame, traducción de ratón y teclado desde `SCA_IInputDevice` | ~300 |
| Tema | `FL_UiTheme.{hpp,cpp}` | Lector de INI en C++ (secciones, subtemas, números, listas de números, `img:`) | ~200 |
| Componente | (en `FL_UiSystem.cpp`) | `FL_Component` integrado que crea el sistema y lo tickea, atado con `fl_component` | ~60 |

**Total estimado: ~2.600 líneas de C++** para sustituir 2.391 de Python. La
proporción es la esperada: el C++ gana en las partes de fontanería (porque
`RAS_DebugDraw` ya tiene la mitad hecha) y pierde en las de azúcar (propiedades,
diccionarios, `configparser`).

---

## 6. Cómo se verificará (el patrón del proyecto)

El mismo que acaba de funcionar con VideoTexture:

1. Una sonda nativa que vuelque a PNG **y a RGBA8 crudo** lo que la interfaz ha
   dibujado en un frame dado (`FL_UI_DUMP`, `FL_UI_DUMP_FRAME`), reutilizando el
   volcado que ya existe en `FL_RenderToTexture.cpp`.
2. Una escena con un *layout* de cada widget.
3. Congelar la línea base con `bgui` todavía vivo, en el Player **con** Python.
4. Exigir que el C++ la reproduzca, y contar los píxeles que difieren.

Trampa ya conocida y pagada en VideoTexture, que aquí también aplica: **el
`.blend` tiene que guardarse con la vista en modo cámara**
(`region_3d.view_perspective = 'CAMERA'`), o el Player no dibuja por la cámara.

---

## 7. Orden de trabajo propuesto

1. `FL_UiCanvas` + el gancho nativo de dibujado. Sin esto no se ve nada, y con
   esto ya se puede pintar un rectángulo y un texto en pantalla sin Python: es la
   primera prueba con evidencia posible.
2. `FL_UiWidget` + `Frame` + `Label` + `FL_UiSystem`. Con eso ya hay un HUD.
3. Botones, imagen, barra de progreso.
4. `ListBox`, `TextBlock`.
5. `TextInput` (el más largo, y el que más se nota si se hace mal).
6. Tema INI.
7. Retirar `scripts/modules/bgui` **solo** cuando los 9 widgets estén y la
   comparación de píxeles salga. Migrar, no borrar.

---

## 8. Lo que este documento NO dice

- No se ha escrito todavía una línea de la interfaz nativa: esto es el inventario
  y el plan, hecho leyendo los 15 ficheros y la capa de dibujo del rasterizador.
- Las cifras de tamaño son estimaciones, no medidas.
- `doc/python_api/rst/bgui/` (la documentación de la API) tendrá que reescribirse
  o retirarse con la librería; no se ha contado en las 2.391 líneas.
