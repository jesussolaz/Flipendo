# Plan de migración a C++ de Flipendo

> Estado y roadmap de la doctrina de [`LENGUAJE-CPP.md`](LENGUAJE-CPP.md).
> Generado del análisis del árbol real (4 auditorías + síntesis) el 2026-09-05.

# Plan de Migración a C++ de Flipendo

*Fecha: 2026-09-05 · Base: UPBGE (fork de Blender 4.5.0) · Target: Mac Intel / Metal · Repo motor: `/Users/jesussolaz/Flipendo/dev/upbge`*

---

## 1. Doctrina

**Regla absoluta:** todo el código **estructural propio de Flipendo** converge a C++. Nada de Python en runtime, nada de C nuevo, nada de scripting propio. Objetivo: `Python propio = 0`, `C propio = 0`, `scripting runtime propio = 0`.

**Qué cuenta como código (y por tanto migra):**
- Lógica de motor, gameplay, herramientas, post-proceso, pipeline: **código**.
- Bindings/glue de scripting que Flipendo escriba: **código** (prohibido en runtime).

**Qué NO cuenta (queda fuera de la regla):**
- **Datos y assets**: `.blend`, texturas, mallas, sonidos, presets que solo asignan propiedades.
- **Formatos de serialización** (DNA/`.blend`).
- **Salidas de compilador/codegen** (MSL y SPIR-V generados en runtime, cadenas C embebidas de shaders).
- **Terceros vendorizados** en `extern/`: se mantienen verbatim como EXTERNAL.

**Matiz honesto sobre plataforma:** la regla del propio usuario permite *wrappers mínimos de plataforma aislados*. La API de Metal y la de Cocoa/AppKit son Objective-C por diseño de Apple; ahí ObjC++ es legítimo y se marca EXTERNAL/PLATFORM, no se persigue su erradicación como si fuera deuda propia.

---

## 2. Foto actual

Composición medida del árbol (`source/ intern/ extern/ release/ scripts/ tests/ tools/`):

| Lenguaje | Líneas | Ficheros | Naturaleza dominante |
|---|---:|---:|---|
| C++ | 2.807.263 | 4.967 | Núcleo del motor (heredado + Fase 3 propia) |
| Headers (.h/.hh) | 1.117.248 | 4.958 | Interfaces C/C++ |
| **Python** | **479.178** | **1.281** | Editor heredado + ~108.736 líneas que son **datos generados**, no código |
| GLSL | 68.324 | 744 (medido: 741 `.glsl`) | Cuerpos de shader escritos a mano |
| C | 64.514 | 48 | 92% terceros en `extern/`; resto glue/otras plataformas |
| Objective-C++ | 30.813 | 37 | GHOST macOS + backend Metal (plataforma) |
| MSL | 2.554 | 5 | Pegamento del backend Metal, no shaders de usuario |
| Shell | 1.302 | 12 | Utillaje |

**% no-C++ del código real:** si se suman Python + GLSL + C + ObjC++ + MSL + Shell frente al total, el no-C++ parece grande, pero **casi todo es heredado o no-código**:

- De los 479.178 de Python, **~108.736 son datos generados** (`tools/svn_rev_map/rev_to_sha1.py` + `sha1_to_rev.py`), y ~195.000 más son tests/doc/build que **no se distribuyen**. El Python estructural real ronda **292.753 líneas en `scripts/`** — y es **100% editor heredado de Blender**.
- De los 64.514 de C, **~58.187 son terceros en `extern/`**.
- Objective-C++ y MSL son **plataforma** (Metal/Cocoa).

**Conclusión de magnitud:** el no-C++ **propio de Flipendo** dentro de este repo es **prácticamente cero** — la Fase 3 (filtros 2D Metal, VideoTexture) ya está en C++. El código propio no-C++ (plantillas ARPG, `flipfx.py`, addons) vive **fuera** de este repo, en `~/Flipendo/`, y es pequeño. El grueso no-C++ es **herencia de Blender**, y su erradicación total es trabajo de años (parte, aspiracional o nunca completa).

---

## 3. Clasificación

### 3.1 Código PROPIO de Flipendo no-C++ — **MIGRAR YA** (auditado en disco: 1.954 líneas)

Medido en `~/Flipendo/` (game/, addons/, bin/) el 2026-09-05. Es **todo** nuestro código
no-C++ y es pequeño: el motor propio (Fase 3) ya está en C++.

| Fichero (propio) | Líneas | Lenguaje | Destino C++ |
|---|---:|---|---|
| `addons/key_assistant/__init__.py` | 386 | Python (addon editor) | Herramienta de editor: utilidad/operador C++ si toca pipeline; si es solo editor, EXTERNAL de bajo riesgo |
| `game/template/arpg.py` (+ `components/`) | 303×2 | Python (gameplay ARPG) | **Componentes C++ nativos** (ABI que reemplaza `KX_PythonComponent`) |
| `addons/keyfx/keyfx.py` | 179 | Python (addon) | Igual criterio que key_assistant |
| `game/template/arpg_core.py` (+ `components/`) | 150×2 | Python (núcleo ARPG) | Subsistemas C++ del Player |
| `game/template/flipfx.py` | 132 | Python (post-proceso KH) | Clase C++ sobre `KX_2DFilterManager`/`RAS_2DFilter` |
| `game/template/gen_template.py` | 127 | Python (genera .blend) | Herramienta; puede quedar como generador de datos |
| `bin/flipendo` | 127 | Bash (gestor de versiones) | Utillaje, no runtime; convergencia a C++ baja prioridad |
| `game/template/*.py` (test/diag/probe) | ~97 | Python (tests propios) | **Tests C++** (regla 5d) |

**Total propio no-C++: 1.954 líneas** (13 `.py` + 1 bash). Comparar con las ~479.000
de Python heredado: **nuestro código propio es el 0,4% del problema**, y es lo único
que la doctrina exige migrar a corto plazo.

Ya en C++ (no requiere acción): **cambios de motor de la Fase 3** (filtros 2D Metal, VideoTexture).

### 3.2 Código HEREDADO (Blender/UPBGE) — el grueso, **plurianual, por fases**

- **Editor Python** (`scripts/`, 292.753 líneas): `bl_ui` (64.452), `bl_operators` (19.165), `addons_core` (151.293: rigify 46.480, bge_mixer 21.870, bl_pkg 19.687, glTF2 30.333, FBX-python 11.943…), `modules` bootstrap de bpy (31.544), freestyle (6.541), presets (14.323). **Mantener EXTERNAL.** El `CMakeLists.txt:230` declara `WITH_PYTHON` como *"only disable for development"*: `bpy` **es** el lenguaje de extensión del editor. Reescribirlo = forkear Blender entero. **No se persigue.**
- **Runtime de juego en Python** (`scripts/`, ~4.200 líneas): `bgui` (2.391, GUI runtime), `bge_extras` (158), `templates_py_components` (1.637). **Pequeño pero ESTRATÉGICO**: es el modelo de gameplay a sustituir por componentes C++ nativos.
- **Glue CPython del Player** (`source/gameengine`): `KX_PythonInit.cpp` (2.617), `KX_PythonComponent`, `SCA_PythonController.cpp` (510). 502 usos de `WITH_PYTHON` en 191 ficheros; `KX_PythonComponent` entero bajo `#ifdef WITH_PYTHON`. **El motor (Ketsji/Physics/Rasterizer) ya es C++**; lo que cae es la capa de scripting. **Abordable por fases.**
- **IO en Python**: solo quedan grandes glTF2 (30.333) y FBX-python (11.943). OBJ/USD/PLY/STL/FBX/Alembic/Collada/CSV **ya están en C++** en `source/blender/io/` (~73.000 líneas). glTF2 es el candidato realista de absorción a C++.
- **C interno migrable** (mínimo): `intern/clog/clog.c` (796, logging), `source/blender/makesdna/intern/dna_defaults.c` (677, ligado al formato `.blend` — no aislar), `source/blender/blenkernel/intern/bullet.c` (95, stub). Prioridad baja.

### 3.3 Plataforma (ObjC++) — **wrappers mínimos permitidos, EXTERNAL/PLATFORM**

- **GHOST macOS Cocoa/AppKit** (~4.340 líneas): `intern/ghost/intern/GHOST_SystemCocoa.mm` (2.199), `GHOST_WindowCocoa.mm` (1.300), `GHOST_ContextCGL.mm`, `GHOST_NDOFManagerCocoa.mm`, `GHOST_SystemPathsCocoa.mm`. **AppKit no tiene binding C++** → imposible C++ puro. **Mantener como wrapper aislado, marcar EXTERNAL/PLATFORM**, exponer solo interfaz `.hh` al motor.
- **Backend GPU Metal** (`source/blender/gpu/metal/*.mm`, ~20.950 líneas): usa API ObjC de Metal. **Matiz honesto:** SÍ podría converger a C++ con **metal-cpp** (binding oficial de Apple, *no* vendorizado en el árbol). Migración por fases de alto coste; el resultado seguiría sobre runtime ObjC. Prioridad media-alta *solo* si el objetivo es reducir `.mm` a cero.
- **Cycles Metal** (`intern/cycles/device/metal/*.mm`, ~5.060 líneas): render **offline**, probablemente innecesario en runtime de juego. **Candidato a excluir del build.**
- **Shims Apple** (~463 líneas): `storage_apple.mm`/`fileops_apple.mm` (Foundation+POSIX, recortables), `messages_apple.mm` (Cocoa/locale, inevitable), `thumbnail_provider.mm` (QuickLook, opcional → eliminable).

### 3.4 Terceros en `extern/` — **EXTERNAL, no reescribir**

`extern/ufbx/ufbx.c` (32.988), `extern/lzma/*.c` (~10.000), `extern/lzo/minilzo.c` (6.053), `extern/cuew` + `extern/hipew` (wranglers CUDA/HIP, **irrelevantes en Mac/Metal → excluir del build**), `curve_fit_nd`, `rangetree`, `xdnd`, `binreloc`, `wcwidth`, `xxhash`, `nanosvg`. **Mantener verbatim, actualizar desde upstream. Nunca reescribir a mano.**

**C de otras plataformas** (Windows/Linux, ~800 líneas: `wayland_dynload`, `decklink/win`, `libc_compat`, `blender_launcher_win32.c`): **no se compila en Mac. Excluir del build.**

---

## 4. Shaders

**Hallazgo central: NO hay que inventar un "Shader IR". Blender YA lo tiene y se llama `ShaderCreateInfo`.**

### Lo que ADOPTAMOS tal cual (ya es C++ puro)

- **`ShaderCreateInfo`** (`source/blender/gpu/intern/gpu_shader_create_info.hh`/`.cc` + 156 ficheros `*_info.hh`): describe en C++ la **interfaz** del shader (entradas, salidas, uniforms, samplers, push-constants, qué `.glsl` es vert/frag/compute, compilación estática). **Este es el "Shader IR en C++" que se pedía.** Se adopta como IR canónico de Flipendo. **No inventar uno propio.**
- **Codegen en runtime desde ese descriptor:**
  - Metal: `source/blender/gpu/metal/mtl_shader_generator.mm` (GLSL→MSL). Crítico para el target Mac/Metal.
  - Vulkan: `source/blender/gpu/vulkan/vk_shader_compiler.cc` (GLSL→SPIR-V vía `shaderc`, con caché en disco). `shaderc` = dependencia **EXTERNAL**.
- **`gpu_glsl_cpp_stubs.hh`** (macro `GLSL_CPP_STUBS`): hace que un `.glsl` compile como C++ moderno (tipos `vec`, swizzles). **Pieza clave**: es el camino para que los cuerpos sean *de facto* C++ linteable, sin crear un lenguaje nuevo.
- **`glsl_preprocess.cc`**: transform de build-time que embebe cada `.glsl` como cadena C. Infraestructura, ya C++.

**Blender YA evita el triple mantenimiento GLSL+MSL+HLSL.** Los 4 `.msl` a mano son pegamento del backend (`mtl_shader_defines.msl`, `mtl_shader_common.msl` + 2 kernels compute), **no** duplicados por-shader. No existe backend DX/HLSL (0 `.hlsl`).

### Lo que MIGRAMOS (lo único no-C++ real)

- **Cuerpos de shader `.glsl`** (~68.000 líneas, 741 ficheros, **heredado**): son fuente escrita a mano, NO generada. Convergencia al 100% C++ = compilarlos como C++ vía los stubs y prohibir GLSL crudo. **Inviable de golpe** (fork enorme frente a upstream). Solo por fases y priorizando lo propio.
- **Filtros 2D del game engine** (`source/gameengine/Rasterizer/RAS_OpenGLFilters/*.glsl`, 11 ficheros, ~150 líneas): Sobel, Prewitt, Dilation, Erosion, Sharpen, Laplacian, Blur, GrayScale, Sepia, Invert, VertexShader2DFilter. **La Fase 3 ya los enruta por create-info → ya cross-compilan a Metal/Vulkan.** Pendiente: convertirlos en `*_info.hh` estáticos + cuerpo GLSL limpio, en vez de ensamblarlos por texto en runtime. **Volumen pequeño, primer objetivo de convergencia total.**

### Deuda LEGACY a registrar

**`RAS_Shader.cpp` — shim de texto GLSL (código PROPIO Fase 3):**
- `RAS_Shader.cpp:559-608` (`LinkProgram` → `GPU_shader_create_from_info`): ya C++, **se conserva**.
- `RAS_Shader.cpp:296-481` (`GetParsedProgram`, compat GL2 con regex) y `:464-474` (traducción `gl_TexCoord`/`gl_FragColor`/`texture2D`): **frágil string-munging**. Marcar:

> **`LEGACY — PENDING C++/IR MIGRATION`**: `source/gameengine/Rasterizer/RAS_Shader.cpp:296-481`. Reducir el shim a la ruta runtime **solo** para filtros GLSL suministrados por el usuario vía API del juego (esos no se pueden pre-hornear). Los filtros integrados pasan a `*_info.hh` estáticos.

> **`LEGACY — PENDING C++/IR MIGRATION`**: los 11 `source/gameengine/Rasterizer/RAS_OpenGLFilters/*.glsl` como cuerpos legacy hasta su conversión a create-info estático.

---

## 5. Reglas en vigor DESDE HOY

Estas reglas entran en vigor **ya**, aunque el grueso heredado tarde años:

1. **(a) Todo código nuevo de Flipendo se escribe en C++.** Cero Python de gameplay/runtime nuevo, cero C nuevo, cero scripting propio. Nuevas herramientas de pipeline: C++ (operadores/utilidades nativas).
2. **(b) No aumentar la deuda.** Prohibido añadir cuerpos GLSL crudos nuevos: todo shader nuevo se declara con `ShaderCreateInfo` (`*_info.hh`) + cuerpo linteable como C++ vía `GLSL_CPP_STUBS`. Prohibido añadir `.mm` nuevos fuera de los wrappers de plataforma ya aislados.
3. **(c) Registrar lo legacy.** Todo fichero heredado no-C++ que Flipendo toque se marca con cabecera/etiqueta: `EXTERNAL` (terceros/editor), `PLATFORM` (GHOST/Metal ObjC++), o `LEGACY — PENDING C++/IR MIGRATION` (deuda propia a converger). Mantener un índice de estas etiquetas.
4. **(d) Cada pieza migrada pasa por tests + benchmark.** Ninguna migración se da por hecha sin: suite de tests C++ que cubra el comportamiento, y benchmark que demuestre paridad o mejora de rendimiento (relevante para filtros 2D Metal y runtime del Player). Los tests nuevos de Flipendo, en C++.

---

## 6. Roadmap por fases

Honestidad primero: **la parte PROPIA y las reglas del punto 5 son corto plazo (hoy).** El grueso heredado es **plurianual**, y el editor es **horizonte lejano o aspiracional — no se promete migrar 479k líneas de Python pronto.**

### Fase A — Nuestro código propio *(corto plazo, meses)*
- Código propio ya auditado (1.954 líneas, ver §3.1).
- Migrar `arpg.py` / `arpg_core.py` a **componentes C++ nativos**.
- Migrar `flipfx.py` a clase C++ sobre `KX_2DFilterManager`/`RAS_2DFilter`.
- Definir el **ABI de componente C++** que reemplazará `KX_PythonComponent`.
- CPython del Player permanece como wrapper EXTERNAL durante la transición.
- **Entregable:** gameplay propio de Flipendo 100% C++, con tests+benchmark.

### Fase B — Filtros del game engine al IR *(corto-medio plazo)*
- Convertir los 11 `RAS_OpenGLFilters/*.glsl` en `*_info.hh` estáticos + cuerpo limpio.
- Reducir `GetParsedProgram` (`RAS_Shader.cpp:296-481`) a la ruta runtime solo para filtros del usuario.
- **Entregable:** filtros integrados como create-info estático, shim de texto acotado.

### Fase C — Quitar Python del gameplay/runtime *(1-2 años)*
- **API `bge.*` nativa en C++** sin exigir binding Python (el motor ya es C++; cae el binding).
- Camino compilado para logic bricks (`SCA_PythonController` y familia; 54/67 bricks `SCA_` tocan `PyObject`).
- Objetivo: **compilar el Player con `WITH_PYTHON=OFF` sin perder gameplay.**
- **Entregable:** Player funcional sin intérprete embebido.

### Fase D — Retirar CPython del Player + IO propio *(2-4 años)*
- Eliminar el intérprete del Player por completo.
- Absorber a C++ los importadores Python que aún usemos (glTF2), apoyándose en que OBJ/USD/PLY/STL/FBX/Alembic ya son C++.
- (Opcional) Migrar backend Metal a **metal-cpp** si se decide reducir `.mm` a cero.

### Fase Z — Editor *(horizonte lejano / aspiracional / posiblemente permanente-EXTERNAL)*
- `bl_ui` (64.452) + `bl_operators` (19.165) + addons (151.293) + `bpy` interno (~80.000 C/C++ de servicio a Python) + IO restante.
- **Recomendación pragmática y honesta:** el editor heredado se **MANTIENE como dependencia EXTERNAL documentada con wrapper C++.** Intentar su eliminación total = fork completo de la arquitectura de Blender. **No se compromete su erradicación.** La regla "todo lo propio a C++" **se cumple** migrando el runtime del Player y las herramientas propias, **no** reescribiendo el editor heredado.

---

## 7. Qué NO se migra

- **Assets y datos**: `.blend`, texturas, mallas, audio.
- **Datos disfrazados de código**: `scripts/presets/` (~14.323, solo asignan propiedades → convertibles a JSON/TOML, baja prioridad), `tools/svn_rev_map/*.py` (108.736 líneas de diccionarios generados).
- **Formatos de serialización**: sistema DNA (`dna_defaults.c`, layout de `.blend`) mientras se conserve el formato.
- **Salidas de compilador/codegen**: MSL y SPIR-V generados en runtime, cadenas C embebidas por `glsl_preprocess`.
- **Terceros aislados** en `extern/` (ufbx, lzma, lzo, cuew/hipew, etc.): EXTERNAL, actualizar desde upstream.
- **Código de otras plataformas** (Windows/Linux): excluir del build de Mac.
- **Wrappers de plataforma inevitables** (GHOST Cocoa/AppKit, shims Cocoa/QuickLook): PLATFORM, no C++ puro por diseño de Apple.
- **Tests/doc/build heredados**: utillaje de desarrollo, no se distribuye; los tests **nuevos** de Flipendo sí en C++.

---

*Resumen de una línea: lo PROPIO de Flipendo y las reglas de código nuevo son de hoy; el motor heredado converge por fases a lo largo de años; el editor Blender se mantiene EXTERNAL y su erradicación total no se promete. Todo lo verificado procede del árbol real; el código propio en `~/Flipendo/` se auditó en disco (1.954 líneas no-C++).*
---

## Actualización Fase B (2026-09-05): el game engine ya es C++

Auditoría de `source/gameengine/` (el núcleo del motor que Flipendo mantiene):

| Tipo | Ficheros | Líneas |
|------|---------:|-------:|
| `.cpp` | 220 | 83.705 |
| `.h` (headers) | 246 | 28.013 |
| `.hpp` (nuestros) | 2 | 321 |
| `.glsl` (filtros 2D) | 12 | 183 |
| `.c` / `.py` / `.mm` | **0** | **0** |

**El game engine no tiene nada de C, Python ni Objective-C.** Es C++ íntegro.
Lo único no-C++ son 183 líneas de GLSL (11 filtros integrados + el KH nativo), y
**ya cross-compilan a Metal y Vulkan desde una sola fuente** vía `ShaderCreateInfo`
(integrado en la Fase 3, `RAS_Shader::LinkProgram`). Es la meta de shaders de la
doctrina: una fuente lógica, salida generada por backend.

### Shaders: estado real
- Fuente única: el cuerpo GLSL de cada filtro (autor humano) → `ShaderCreateInfo`.
- Salida generada: MSL (Metal) y SPIR-V (Vulkan) por el pipeline de Blender.
- **No** hay triple mantenimiento GLSL+MSL+HLSL a mano.
- Deuda restante acotada: el parser de texto para filtros GLSL *de usuario en runtime*
  (`RAS_Shader.cpp:296-481`, marcado LEGACY). Solo afecta a filtros que el juego
  suministra en caliente; los integrados no lo necesitan.

### Headers .h → .hpp: decisión estratégica
- **Nuestros** headers nuevos ya son `.hpp` (`FL_*.hpp`).
- Los 246 `.h` de `source/gameengine/` son de **UPBGE** (heredados). Renombrarlos a
  `.hpp` rompería miles de `#include` y, sobre todo, **la sincronización con UPBGE**
  (la estrategia de `externo/` para aprovechar sus mejoras). Se convierten solo
  cuando se reescriba su subsistema, no en masa. Es coherente con la doctrina
  (heredado = EXTERNAL hasta reescritura).

## Frontera honesta de "todo a C++"

- **Todo lo que Flipendo escribe y el game engine entero: C++.** ✔
- **Shaders propios: fuente única C++/create-info → Metal/Vulkan.** ✔
- **Frontera:** el editor de Blender (≈293k líneas Python) y sus shaders/headers
  heredados. Migrarlos = forkear Blender entero y **perder** la capacidad de
  incorporar sus actualizaciones. Se mantienen EXTERNAL por diseño. No es una
  limitación de esfuerzo, es la decisión de arquitectura que hace viable el fork.
