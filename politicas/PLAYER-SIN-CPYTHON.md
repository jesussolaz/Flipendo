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

### 3.1 VideoTexture / `bge.texture` — RESUELTO (2026-09-11), ver §6

Esta sección era la deuda más seria de la lista: `source/gameengine/CMakeLists.txt`
metía `VideoTexture` en el build **solo si `WITH_PYTHON`**, así que el Player de
distribución se quedaba sin render-a-textura (espejos, minimapas, cámaras de
vigilancia), que es **uno de los arreglos estrella de Flipendo** frente a UPBGE 0.44.

Ya no es deuda: es capacidad. El módulo tiene fachada C++, se construye siempre y
se ha verificado en los dos binarios. Lo que había, cómo se hizo y con qué cifras
está en **§6**.

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

Para que este Player sea el que se envíe de verdad quedaban dos cosas: la **fachada
C++ de VideoTexture** (§3.1) y la **UI nativa de juego** (§3.3). La primera está
hecha y verificada desde el 2026-09-11: ver **§6**. La segunda sigue abierta.

> Las cifras de esta tabla son del 2026-09-08. Con VideoTexture ya dentro del
> build, medido el 2026-09-11: bundle del Player **537 MB** sin CPython contra
> **772 MB** con Python, y **0 símbolos que empiecen por `_Py`** contra 2.267
> (la cuenta de 1.089 de arriba se hizo con un patrón más laxo).

---

## 6. VideoTexture con fachada C++ — CONSEGUIDO (2026-09-11)

**Render a textura existe en el Player sin CPython.** Una cámara secundaria
renderiza sobre el material de un objeto, frame a frame, sin una línea de Python.

### 6.1 Qué había y por qué no bastaba con quitar los bindings

La lógica de `ImageRender`, `ImageViewport`, `ImageBase` y los `Filter*` siempre
fue C++ normal. Lo que ataba el módulo al intérprete era el **plumbing**:
`Texture::SetSource()` recibía un `PyImage *`, `getMaterialID()` un `PyObject *`,
`ImageBase::setFilter()` un `PyFilter *`, `ImageSource::setSource()` otro
`PyImage *` y `FilterBase::setPrevious()` otro `PyFilter *`. Y `PyImage`/`PyFilter`
no son más que una cabecera de objeto de Python envolviendo un puntero nativo.

### 6.2 Qué se hizo

1. **El API pasa a ser nativo.** `Texture::SetSource(ImageBase *, bool own)`,
   `getMaterialID(KX_GameObject *, const char *)`, `ImageBase::getSource/setSource`
   y `getFilter/setFilter` sobre `ImageBase *`/`FilterBase *`,
   `FilterBase::setPrevious(FilterBase *)`. Las versiones con `PyImage *` /
   `PyFilter *` / `PyObject *` quedan como envoltorios delgados bajo
   `#ifdef WITH_PYTHON`.
2. **El contador de referencias, sin saber de Python.** Cada `ImageBase` y cada
   `FilterBase` guarda un puntero a su envoltorio (`getWrapper()`/`setWrapper()`),
   y `VT_WrapperIncRef/DecRef` (FilterBase.hpp) suben y bajan ese contador con
   Python y no hacen nada sin él. Sin intérprete, el dueño es quien creó el objeto.
3. **Nada de comparar tipos de Python.** `Texture::refresh()` decidía si avisar al
   depsgraph con `PyObject_TypeCheck` contra VideoFFmpegType / ImageFFmpegType /
   ImageMixType / ImageViewportType. Ahora lo responde cada clase con
   `ImageBase::needsDepsgraphNotifier()`, con el reparto **exacto** de aquella
   lista: sí en ImageViewport, ImageMix y VideoFFmpeg; **no** en ImageRender, que
   heredaba de ImageViewport en C++ pero no era subtipo suyo en Python
   (`tp_base = 0`) y por eso nunca pasaba el `PyObject_TypeCheck`.
4. **El `SRC` partido en dos** (`VideoTexture/CMakeLists.txt`): núcleo siempre,
   enlaces de Python dentro de `if(WITH_PYTHON)`.
5. `add_subdirectory(VideoTexture)` fuera de su `if(WITH_PYTHON)`; Ketsji y
   Launcher vuelven a enlazar `ge_videotexture` sin condición; y
   `Texture::FreeAllTextures` deja de estar bajo `#ifdef WITH_PYTHON` en
   `LA_Launcher` y `BL_Converter` — sin eso las texturas nativas no se soltarían.

### 6.3 Cómo se usa desde un juego

Fachada: `source/gameengine/Flipendo/FL_RenderToTexture.hpp`.

| `bge.texture` (Python) | `FL_RenderToTexture` (C++) |
|---|---|
| `tex = texture.Texture(ob, matID)` | `FL_RenderToTexture::Create(ob, material, cam, w, h)` |
| `tex.source = texture.ImageRender(sc, cam)` | (lo hace `Create`) |
| `tex.refresh(True)` cada frame | `rtt->Refresh()` cada frame |

Y el componente integrado **`CctvMonitor`**, que se ata con la propiedad de juego
`fl_component` y lee `fl_rtt_camera`, `fl_rtt_material`, `fl_rtt_slot`,
`fl_rtt_width`, `fl_rtt_height` y `fl_rtt_samples`. Un juego no necesita escribir
C++ para tener una cámara de vigilancia: le basta con poner las propiedades.

### 6.4 Verificación con evidencia

Sonda `FL_RttProbeTick` (estilo `--fl-dump-*`; el Player no tiene analizador de
opciones largas, así que va por entorno: `FL_RTT_PROBE`, `FL_RTT_DUMP`,
`FL_RTT_DUMP_FRAME`, `FL_RTT_EXIT`). Vuelca a PNG **y a RGBA8 crudo** la textura
de GPU que el material está enseñando en ese instante, sin preguntar quién la
puso ahí; la conversión de coma flotante a sRGB de 8 bits es la misma en los dos
binarios, que es lo que los hace comparables.

Escena: un plano `Pantalla` con material de imagen (magenta plano, 256×256), una
cámara secundaria `CamVigilancia` apuntando a un cartel naranja y al suelo, y la
cámara de juego mirando a la pantalla.

| Medida | Resultado |
|---|---|
| Textura del material antes del refresco | 256×256 `SRGB8_A8` (la imagen original) |
| Textura del material tras el refresco | **513×513 `RGBA16F`** (el render de la cámara) |
| Píxeles magenta (la imagen original) que quedan | **0 de 263.169** |
| Píxeles naranjas (el cartel que ve la cámara) | **189.161 de 263.169** |
| Componentes nativos atados | **1/1** en los dos binarios |

Comparación pixel a pixel de la textura resultante, 513×513 = 263.169 píxeles:

| Comparación | Píxeles distintos | Delta medio | Delta máximo |
|---|---:|---:|---:|
| Player **con Python**, dos ejecuciones | 0 (0,00 %) — idénticas byte a byte | 0,00 | 0 |
| Player **sin CPython**, ejecuciones 1 y 2 | 3.408 (1,29 %) | 5,57 | 120 |
| Player **sin CPython**, ejecuciones 1 y 3 | 4.109 (1,56 %) | 5,76 | 103 |
| **Sin CPython (1) contra con Python** | 3.730 (1,42 %) | 5,70 | 123 |
| **Sin CPython (2) contra con Python** | 3.736 (1,42 %) | 2,63 | 117 |
| **Sin CPython (3) contra con Python** | 4.617 (1,75 %) | 3,70 | 108 |

**Por qué difieren esos píxeles:** no por el camino, sino por el frame. La
diferencia entre los dos binarios (1,42–1,75 %) es del mismo tamaño que la
diferencia del Player sin CPython **consigo mismo** entre dos ejecuciones
(1,29–1,56 %): es la acumulación temporal de EEVEE y el frame concreto que
atrapa la sonda al tic 120, que depende del reloj. El 98,5 % de los píxeles es
idéntico byte a byte y el 85–91 % de los bytes que cambian lo hacen en 8 niveles
o menos, concentrados en los bordes. El Player con Python salió determinista en
sus dos ejecuciones; el de distribución, que va más rápido, no.

Y lo que no cambia: el Player sin CPython **sigue sin un solo símbolo de CPython**
y sigue atando todos los componentes de las dos escenas reales.

| Medida | Player con Python | Player sin CPython |
|---|---:|---:|
| Símbolos que empiezan por `_Py` (`nm`) | 2.267 | **0** |
| `libpython` enlazada (`otool -L`) | — (estático) | **no** |
| Bundle del Player | 772 MB | **537 MB** |
| Componentes en `game/template/ArpgNative.blend` | 5/5 | **5/5** |
| Componentes en `game/anima/T1_LaMancha.blend` | 5/5 | **5/5** |

(Los 89 símbolos que *contienen* `_Py` en el binario sin CPython son nombres de
clases de Flipendo —`KX_PythonProxy`, `SCA_PythonController`, `EXP_PyObjectPlus`—,
no símbolos del intérprete. Por eso se cuenta con `^_Py`, no con `_Py`.)

### 6.5 Trampas pagadas

- **La textura de destino no existe en el primer frame.** Blender sube la textura
  de GPU de una imagen de forma perezosa, en el primer dibujado.
  `ImageViewport::calcViewport` marcaba `m_texInit = true` aunque el intercambio
  no hubiera ocurrido, y entonces no se reintentaba nunca: la pantalla se quedaba
  con la imagen original para siempre. Ahora solo se da por inicializada cuando
  el intercambio ocurrió de verdad.
- **El `.blend` manda más de lo que parece.** El Player dibuja a través del
  `View3D` guardado en el fichero: si su vista no está en modo cámara
  (`region_3d.view_perspective = 'CAMERA'`), ni el render principal ni el
  `ImageRender` usan la cámara, y lo que sale en la textura es el punto de vista
  del usuario que quedó grabado. Pasamos un buen rato creyendo que el
  `ImageRender` ignoraba la cámara: no la ignoraba, la ignoraba el `.blend`.
  Cualquier escena que use render a textura tiene que guardarse con la vista en
  modo cámara.
- `ImageRender` tiene dos constructores y el del **espejo** no inicializaba
  `m_preDrawCallbacks` ni `m_postDrawCallbacks`, que el destructor hace
  `Py_CLEAR`. Basura de pila desde antes. Corregido.
- `Texture::m_useMatTexture` tampoco se inicializaba. Corregido.
- `Exception::report()` llamaba a `PyErr_SetString` sin guarda; sin intérprete no
  hay `PyErr` donde dejar el aviso, así que ahora sale por consola con `CM_Error`.
- `registerAllExceptions()` registra descriptores que viven en `VideoBase.cpp`;
  al quedarse ese fichero del lado de Python, el enlace del Player se rompía.
- `MEM_freeN`/`MEM_mallocN` llegaban a `ImageBase.cpp` por el include de
  `mathutils` que solo existe con Python.
- Poner `override` en un método hace que clang avise de sus hermanos sin marcar:
  se marcaron los seis que faltaban en ImageMix, ImageViewport e ImageRender.

### 6.6 Lo que queda de VideoTexture

- **Vídeo sobre textura sigue siendo de Python.** `VideoBase.cpp`,
  `VideoFFmpeg.cpp`, `DeckLink.cpp` y `VideoDeckLink.cpp` siguen dentro del
  `if(WITH_PYTHON)` del `CMakeLists`: su lógica está trenzada con la capa de
  enlace (parsers de argumentos, getsets y estado mezclados en los mismos
  métodos) y no se cierra en una noche. Un juego sin intérprete puede hoy poner
  una cámara en una textura, pero **no un vídeo**. Es la siguiente pieza de esta
  línea.
- **`ImageMirror` (espejos) no se ha probado en ejecución.** Se compila en las dos
  configuraciones y su constructor nativo existe, pero la fachada
  `FL_RenderToTexture` solo expone hoy el caso cámara. Falta exponerlo y probarlo.
- **No se pudo comparar contra el camino de Python en ejecución.** Montar una
  escena con controlador Python que llamara a `bge.texture` exigía
  `bpy.ops.logic.sensor_add`, que **revienta en `--background`**: la pila es
  `sensor_add_exec + 397` → `ED_undo_push_old(C, "sensor_add_exec")` sin pila de
  deshacer. La comparación se hizo entonces entre los dos binarios por el camino
  nativo, que es la pregunta que de verdad importa. Queda dicho lo que no se
  comparó.
- **`Error: totblock: 10`** al salir del Player. Aparece también en
  `ArpgNative.blend`, que no usa VideoTexture, así que no es de esta migración;
  queda anotado para quien persiga las fugas del cierre del motor.
