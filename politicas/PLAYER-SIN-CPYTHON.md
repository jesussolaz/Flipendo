# Player sin CPython — configuración de build y estado real

> Objetivo doctrinal (`LENGUAJE-CPP.md`): que el juego exportado no lleve intérprete
> de Python. Este documento registra **cómo se consigue** y, con la misma honestidad,
> **qué se pierde por el camino**.

---

## 1. El bloqueante que se creía estructural, y por qué no lo era

`BACKLOG-EDITOR-PYTHON.md` daba por bloqueante que `WITH_PYTHON` es una opción
**global** de CMake, no del Player: apagarla apaga también el Python del editor, y
un editor sin Python no tiene UI. De ahí la conclusión de que hacía falta inventar
un target o flag «Player-only».

No hace falta. La salida es **dos configuraciones de build del mismo árbol**:

| Build | Flags | Para qué |
|---|---|---|
| **Editor** (`dev/build`) | `WITH_PYTHON=ON` | Trabajar: modelar, montar escenas, pulsar P. Es el Blender/Flipendo de siempre. |
| **Player de distribución** (`dev/build-nopy`) | `WITH_PYTHON=OFF`, `WITH_USD=OFF`, `WITH_HYDRA=OFF`, `WITH_MATERIALX=OFF` | Empaquetar el juego. Sin intérprete. |

El juego se **crea** con el primero y se **envía** con el segundo. El `.blend` es el
mismo en los dos lados; lo único que tiene que cumplir es no depender de Python
(componentes nativos `fl_component`, no bricks de Python).

Comando de configuración del build de distribución:

```
cmake -S <repo> -B dev/build-nopy -G Ninja \
  -C build_files/cmake/config/blender_release.cmake \
  -DWITH_GAMEENGINE=ON -DWITH_PLAYER=ON -DWITH_CYCLES=OFF \
  -DWITH_PYTHON=OFF -DWITH_USD=OFF -DWITH_HYDRA=OFF -DWITH_MATERIALX=OFF \
  -DCMAKE_BUILD_TYPE=Release
```

### Por qué `WITH_USD=OFF` es obligatorio y no opcional

Blender ya resuelve solo casi todas las dependencias de Python: `CMakeLists.txt:1376`
tiene `set_and_warn_dependency(WITH_PYTHON WITH_MOD_FLUID OFF)`, que apaga Mantaflow
al apagar Python. **USD no está en esa lista.** Su `boost::python`
(`lib/macos_x64/usd/include/pxr/external/boost/python/detail/wrap_python.hpp:62`)
hace `#include <pyconfig.h>` incondicionalmente, así que el build revienta al
generar el PCH de `bf_io_usd`. Para un Player, USD (intercambio de escenas para el
editor) no pinta nada. Se apaga y ya.

---

## 2. Qué se gana

Con `WITH_PYTHON=OFF`, `source/blenderplayer/CMakeLists.txt` deja de instalar en el
bundle tanto el árbol `scripts/` como la librería estándar de Python (las reglas de
instalación de las líneas 232 y 377 están dentro de `if(WITH_PYTHON)`).

Referencia del Player **con** Python: bundle de **771 MB**, de los cuales **193 MB**
son `Contents/Resources/4.5/python`, y el ejecutable lleva 1.089 símbolos `_Py`
enlazados estáticamente.

---

## 3. Qué se pierde — la parte incómoda

### 3.1 VideoTexture / `bge.texture` desaparece

`source/gameengine/CMakeLists.txt:52` mete `VideoTexture` en el build **solo si
`WITH_PYTHON`**. El módulo entero es API de Python: no hay forma de pedir un
`ImageRender` desde C++ porque la fachada nativa no existe.

Esto es serio, porque render-a-textura (espejos, minimapas, cámaras de vigilancia)
es **uno de los arreglos estrella de Flipendo** frente a UPBGE 0.44 — y el Player de
distribución, tal cual, no lo tiene.

**Salida:** una fachada C++ de VideoTexture (crear un `ImageRender` sobre el material
de un objeto y refrescarlo por frame) compilada sin Python, usable desde un
`FL_Component`. Es el siguiente trabajo real de esta línea, no un detalle.

**El obstáculo concreto, ya localizado:** no basta con quitar los bindings. La
lógica de `ImageRender`, `ImageViewport`, `ImageBase` y los `Filter*` sí es C++
normal (los `PyObject` que hay en esos ficheros son la capa de binding), pero el
plumbing entre `Texture` e imagen pasa por Python: `Texture::SetSource()`
(`Texture.hpp:71`) recibe un `PyImage *`, y `getMaterialID()` (`Texture.hpp:99`)
recibe un `PyObject *`. `PyImage` no es más que una cabecera de objeto Python
envolviendo un `ImageBase *`, así que el camino es:

1. Reescribir el API de `Texture` en términos de `ImageBase *` (nativo), y dejar
   la versión `PyImage *` como envoltorio delgado bajo `#ifdef WITH_PYTHON`.
2. Trocear el `SRC` de `VideoTexture/CMakeLists.txt` en núcleo (siempre) y
   bindings (`PyTypeList.cpp`, `blendVideoTex.cpp` y los bloques `Py_Header`).
3. Sacar `add_subdirectory(VideoTexture)` de su `if(WITH_PYTHON)` en
   `source/gameengine/CMakeLists.txt:52`.
4. Exponerlo como componente/servicio nativo y verificarlo con la escena CCTV que
   ya se usó para validar el arreglo original.

### 3.2 Bricks de controlador Python

Se compilan, pero no ejecutan nada. Ya **no fallan en silencio**: desde el commit
«el build sin CPython avisa de los bricks Python en vez de callar»,
`SCA_PythonController::Trigger()` reporta una vez por controlador y dice qué hacer
(portarlo a `fl_component`).

### 3.3 `bgui`

La librería de interfaz de UPBGE (`scripts/modules/bgui`, ~2.400 líneas) es Python.
Un juego sin CPython no puede usarla. Hace falta UI nativa; hasta entonces, un juego
python-free no tiene sistema de interfaz de serie.

---

## 4. Qué sobrevive intacto

- **Componentes de juego**: `FL_Component` es C++ puro y es ya el único sistema del
  motor (`KX_PythonComponent` eliminado).
- **Filtros 2D**: `RAS_2DFilter*` y `KX_2DFilter*` se compilan siempre; el
  `if(WITH_PYTHON)` de `Rasterizer/CMakeLists.txt:46` solo añade directorios de
  include. Los filtros se atan por actuador, no por script.
- **Físicas, animación, sonido, render EEVEE, logic bricks no-Python**: nativos.

---

## 5. Estado — CONSEGUIDO (2026-09-08)

**El Player sin CPython existe, compila y juega.**

| Medida | Player con Python | Player sin CPython |
|---|---:|---:|
| Símbolos `_Py` en el binario | 1.089 | **0** |
| `libpython` enlazada | — (estático) | **no** |
| Librería estándar en el bundle | 193 MB | **no se instala** |
| Bundle completo | 771 MB | **536 MB** |

Verificado ejecutando el Player sin CPython sobre dos escenas reales: ata y tickea
5/5 componentes nativos en `game/template/ArpgNative.blend` (PlayerController, 3×
EnemyAI, ThirdPersonCamera) y 5/5 en `game/anima/T1_LaMancha.blend` (el mapa de
ÁNIMA), sin un solo error. El gameplay entero corre en C++.

### Las tres reparaciones que exigió compilar sin Python

Apagar `WITH_PYTHON` activa ramas `#else` que en un build normal no se compilan
jamás, y que por eso llevaban tiempo podridas:

1. `source/blender/editors/interface/regions/interface_region_tooltip.cc:800` —
   `UNUSED_VARS(is_label, ...)` nombraba una variable que upstream renombró a
   `is_quick_tip`.
2. `source/gameengine/Ketsji/KX_PythonProxy.cpp:33` — la lista de inicialización
   terminaba en coma justo antes del `#ifdef WITH_PYTHON`, así que sin Python
   quedaba una coma colgante.
3. `source/gameengine/Ketsji/CMakeLists.txt` — enlazaba `ge_videotexture`
   incondicionalmente, pero ese subdirectorio solo se construye con Python.
   `Launcher/CMakeLists.txt:88` ya lo guardaba bien; ahora Ketsji también.

### Deuda pendiente

Para que este Player sea el que se envíe de verdad: **fachada C++ de VideoTexture**
(§3.1) y **UI nativa de juego** (§3.3).
