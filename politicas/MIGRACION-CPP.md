# Plan de migración a C++ de Flipendo

> Estado y roadmap de la doctrina de [`LENGUAJE-CPP.md`](LENGUAJE-CPP.md).
> Generado del análisis del árbol real (4 auditorías + síntesis) el 2026-09-05.

> ## ⚠️ AVISO DE VIGENCIA — revisado el 2026-09-11
>
> Este documento es del **5 de septiembre** y se actualizó por última vez el 8. La
> **«Decisión del 2026-09-11»** de [`LENGUAJE-CPP.md`](LENGUAJE-CPP.md) —*full C++
> significa TODO, no «todo lo razonable»*— **deroga dos de sus secciones**. No se
> borran: se dejan con su marca, porque la razón por la que se pensaba así sigue
> siendo información útil y porque un plan sin su historia no se puede juzgar.
>
> | Sección | Qué decía | Estado |
> |---|---|---|
> | §1, «Matiz honesto sobre plataforma» | ObjC++ es legítimo, no se persigue | **DEROGADA** → [`LENGUAJE-CPP.md`](LENGUAJE-CPP.md), [`OBJC-A-CPP.md`](OBJC-A-CPP.md) |
> | [§3.3](#33-plataforma-objc--wrappers-mínimos-permitidos-externalplatform) | ObjC++ = PLATFORM, «imposible C++ puro» | **DEROGADA** → [`OBJC-A-CPP.md`](OBJC-A-CPP.md) |
> | [§4](#4-shaders) | GLSL a mano: «inviable de golpe» | **DEROGADA** → [`LENGUAJE-CPP.md`](LENGUAJE-CPP.md) |
> | §2, «Foto actual» | composición del árbol | **REGENERADA** hoy, medida |
> | §6 Fase Z, «Frontera honesta» | editor EXTERNAL permanente | **desmentida por los hechos**, ver §2 |
>
> Lo que **sigue en vigor sin un rasguño**: §1 (doctrina), §5 (las cuatro reglas de
> código nuevo), §7 (qué no se migra, salvo lo que el 11 saca de la lista) y la
> adopción de `ShaderCreateInfo` como IR canónico, que es el hallazgo más útil del
> documento y no lo toca nadie.

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
- **Terceros vendorizados** en `extern/` **y `lib/`** (añadido el 2026-09-11): se mantienen verbatim como EXTERNAL. `.gitattributes:107-108` los marca `linguist-vendored`.

**Matiz honesto sobre plataforma:** ~~la regla del propio usuario permite *wrappers mínimos de plataforma aislados*. La API de Metal y la de Cocoa/AppKit son Objective-C por diseño de Apple; ahí ObjC++ es legítimo y se marca EXTERNAL/PLATFORM, no se persigue su erradicación como si fuera deuda propia.~~

> **DEROGADO el 2026-09-11.** Jesús decidió, con el coste sobre la mesa, que full C++
> incluye el Objective-C++. El argumento de arriba además era **técnicamente falso**,
> y está refutado en [`LENGUAJE-CPP.md`](LENGUAJE-CPP.md): el runtime de Objective-C
> es una biblioteca de C (`objc_msgSend`, `sel_registerName`,
> `objc_allocateClassPair`) y cualquier C++ puede llamar a cualquier método de Cocoa
> sin compilador de Objective-C. `metal-cpp`, el binding oficial de Apple, es
> exactamente eso, y desde hoy está vendorizado en `extern/metal-cpp`. El plan real
> está en [`OBJC-A-CPP.md`](OBJC-A-CPP.md).

---

## 2. Foto actual — **regenerada el 2026-09-11** (la anterior era del 5 de septiembre)

Medida al commit `87ce606a318` con `git ls-tree` (ficheros) y `git grep -c ''`
(líneas), **excluyendo `extern/` y `lib/`**, que desde la decisión del 11 son
terceros vendorizados y no código de Flipendo. El método y las series completas están
en [`METRICAS.md`](METRICAS.md); aquí solo va la foto.

| Lenguaje | Líneas | Ficheros | Qué es hoy |
|---|---:|---:|---|
| **C++** (`.cpp`/`.cc`) | **2.540.538** | 4.313 | el estándar del árbol |
| `.hh` | 292.099 | 1.850 | cabecera C++ heredada de Blender |
| `.h` | 248.745 | 1.389 | **deuda**: cabecera heredada, a `.hpp` al reescribir su subsistema |
| `.hpp` | 36.679 | 305 | la convención propia |
| **Python** | **191.224** | 613 | **deuda**: editor; ya no hay Python en el Player de distribución |
| **GLSL** | **68.391** | 745 | **deuda desde el 11** |
| **Objective-C++** | **30.304** | 32 | **deuda desde el 11** |
| MSL (`.msl`) | 1.722 | 4 | pegamento del backend Metal |
| Metal (`.metal`) | 832 | 1 | kernel de Cycles; **no se compila** (`WITH_CYCLES=OFF`) |
| **C** | **0** | **0** | ✅ cerrado el 2026-09-05 |
| **Shell** | **0** | **0** | ✅ cerrado (ver `SHELL-Y-PLANTILLAS-CPP.md`) |

**Total propio: 3.410.534 líneas.** Familia C/C++ 91,4 %; 84,1 % si las `.h`
heredadas cuentan como deuda, que es lo que dice la decisión del 11.
Terceros vendorizados (`extern/` + `lib/`): 2.564 ficheros, 919.268 líneas, de los
que 58.188 son C y 792 shell — **las únicas de esos dos lenguajes en todo el repo**.

### Qué decía esta sección antes, y qué era falso

La tabla anterior (5 de septiembre) mezclaba `extern/` con lo propio y se apoyaba en
un contador que suma una línea de más por fichero (demostrado en
[`METRICAS.md` §6.2](METRICAS.md)). Pero el problema de fondo no era ese: era la
**conclusión**, y hay que decirlo porque se citó mucho.

Decía: *«el no-C++ propio de Flipendo dentro de este repo es prácticamente cero […]
el grueso no-C++ es herencia de Blender, y su erradicación total es trabajo de años
(parte, aspiracional o nunca completa)»*, y *«el código propio no-C++ vive fuera de
este repo»*.

**El árbol lo ha desmentido en seis días.** Las 479.178 líneas de Python que el
documento daba por casi inamovibles son hoy **191.224**: −301.142, el **61 %**, sin
que nadie haya renunciado a una sola capacidad y con el Player de distribución ya
**sin intérprete** (`PLAYER-SIN-CPYTHON.md`). El error de razonamiento fue tratar
«heredado de Blender» como sinónimo de «no es nuestro problema»: en un fork que ha
renunciado a sincronizar con upstream, **todo lo que se compila es propio**, lo
escribiera quien lo escribiera. Es justamente lo que dice el punto 5 del reglamento
de la noche: *no te autolimites con «es demasiado grande» o «es herencia de
Blender»*.

Lo que sí sigue siendo cierto de aquella foto: el Python que queda es editor
(`scripts/`), y el C de `extern/` no se toca.

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

### 3.2 Código HEREDADO (Blender/UPBGE) — el grueso, ~~**plurianual, por fases**~~

> **Cifras caducadas, remedidas el 2026-09-11** (commit `87ce606a318`). El reparto de
> abajo describe un árbol que ya no existe; se conserva porque explica el
> razonamiento, pero **no se use para estimar nada**. Hoy:
>
> | Bloque | Decía (5 sep) | Hoy | Qué pasó |
> |---|---:|---:|---|
> | `scripts/` Python total | 292.753 | **119.105** (251 f.) | −59,3 % |
> | `scripts/startup/bl_ui` | 64.452 | 51.741 (67 f.) | `space_logic.py`, `properties_game.py`, toolsystem y el editor de nodos, en C++ |
> | `scripts/startup/bl_operators` | 19.165 | 12.569 (27 f.) | `wm.py` entero fuera |
> | `scripts/addons_core` | 151.293 | **22.703** | rigify, glTF2, FBX, bge_mixer: ya no están |
> | `scripts/modules` | 31.544 | 24.544 (87 f.) | |
> | `scripts/presets` | 14.323 | **0 `.py`** | 172 `.fpreset` (datos) + lector C++ |
> | Runtime de juego Python | 4.186 | **2.391** (`bgui`) | `bge_extras` y `templates_py_components`, fuera |
>
> Y la frase que peor ha envejecido: *«`bpy` **es** el lenguaje de extensión del
> editor. Reescribirlo = forkear Blender entero. **No se persigue**»*. Se persiguió,
> y el editor sigue arrancando. Ver [`UI-A-CPP.md`](UI-A-CPP.md) y
> [`BACKLOG-EDITOR-PYTHON.md`](BACKLOG-EDITOR-PYTHON.md).
>
> El «C interno migrable (mínimo)» que lista más abajo también está cerrado: **C
> propio = 0 ficheros** desde el 5 de septiembre. Solo queda `dna_defaults.c`
> revertido a `.c` como dato del formato `.blend`, con su evidencia en
> [`REGISTRO-LEGACY.md`](REGISTRO-LEGACY.md).

- **Editor Python** (`scripts/`, 292.753 líneas): `bl_ui` (64.452), `bl_operators` (19.165), `addons_core` (151.293: rigify 46.480, bge_mixer 21.870, bl_pkg 19.687, glTF2 30.333, FBX-python 11.943…), `modules` bootstrap de bpy (31.544), freestyle (6.541), presets (14.323). **Mantener EXTERNAL.** El `CMakeLists.txt:230` declara `WITH_PYTHON` como *"only disable for development"*: `bpy` **es** el lenguaje de extensión del editor. Reescribirlo = forkear Blender entero. **No se persigue.**
- **Runtime de juego en Python** (`scripts/`, ~4.200 líneas): `bgui` (2.391, GUI runtime), `bge_extras` (158), `templates_py_components` (1.637). **Pequeño pero ESTRATÉGICO**: es el modelo de gameplay a sustituir por componentes C++ nativos.
- **Glue CPython del Player** (`source/gameengine`): `KX_PythonInit.cpp` (2.617), `KX_PythonComponent`, `SCA_PythonController.cpp` (510). 502 usos de `WITH_PYTHON` en 191 ficheros; `KX_PythonComponent` entero bajo `#ifdef WITH_PYTHON`. **El motor (Ketsji/Physics/Rasterizer) ya es C++**; lo que cae es la capa de scripting. **Abordable por fases.**
- **IO en Python**: solo quedan grandes glTF2 (30.333) y FBX-python (11.943). OBJ/USD/PLY/STL/FBX/Alembic/Collada/CSV **ya están en C++** en `source/blender/io/` (~73.000 líneas). glTF2 es el candidato realista de absorción a C++.
- **C interno migrable** (mínimo): `intern/clog/clog.c` (796, logging), `source/blender/makesdna/intern/dna_defaults.c` (677, ligado al formato `.blend` — no aislar), `source/blender/blenkernel/intern/bullet.c` (95, stub). Prioridad baja.

### 3.3 ~~Plataforma (ObjC++) — **wrappers mínimos permitidos, EXTERNAL/PLATFORM**~~ · **DEROGADA el 2026-09-11**

> **Esta sección ya no está en vigor.** La deroga la «Decisión del 2026-09-11» de
> [`LENGUAJE-CPP.md`](LENGUAJE-CPP.md): *full C++ incluye el Objective-C++*. El
> objetivo es **cero `.mm`**, y el plan medido está en [`OBJC-A-CPP.md`](OBJC-A-CPP.md).
>
> **Por qué se deroga, y no solo porque Jesús lo diga:** el argumento central de esta
> sección —«**AppKit no tiene binding C++** → imposible C++ puro»— es **falso**. El
> runtime de Objective-C es una biblioteca de C; `objc_getClass`, `sel_registerName`,
> `objc_msgSend` y `objc_allocateClassPair` se llaman desde C++ sin que intervenga un
> compilador de Objective-C. La prueba es que `metal-cpp`, el binding **oficial de
> Apple**, es exactamente eso: cabeceras que envuelven `objc_msgSend`. Desde hoy está
> vendorizado en `extern/metal-cpp`.
>
> **Lo que sí acertaba, y conviene no perder:** que esto es caro y que el compilador
> deja de avisar. En un `.mm` el pegamento lo genera Clang; en C++ puro lo escribimos
> nosotros, con `objc_msgSend` casteado a la firma exacta de cada método (y sus
> variantes `_stret`/`_fpret` en x86_64), delegados fabricados en tiempo de ejecución
> y `retain`/`release` a mano sin ARC. Por eso cada pieza migrada aquí necesita **más**
> verificación que una de interfaz, no menos.
>
> **Y una medida que cambia el plan** (`OBJC-A-CPP.md` §2): el trabajo no son 30.000
> líneas, son **441 envíos de mensaje** en los ficheros pendientes. `mtl_shader_log.mm`
> tenía **0**: era C++ disfrazado de `.mm`. `mtl_shader_generator.mm`, el mayor con
> 3.413 líneas, tiene **16**. Ordenar este trabajo por líneas habría sido ordenarlo
> justo al revés de como conviene.
>
> **Cifras de la sección, remedidas hoy** (commit `87ce606a318`): 32 ficheros `.mm`,
> **30.304 líneas** — no 37/30.813. `mtl_query.mm` y `mtl_shader_log.mm` ya son `.cc`.
> Reparto: `source/blender/gpu/metal` 18 ficheros / 20.718; `intern/cycles` 8 / 5.060
> (**no se compilan**: `WITH_CYCLES:BOOL=OFF`, y `grep -c 'cycles/device/metal'
> dev/build/build.ninja` da 0); GHOST Cocoa 5 / 4.340; `blendthumb` 1 / 186.
>
> Los tres *shims* de Apple que esta sección listaba (`storage_apple.mm`,
> `fileops_apple.mm`, `messages_apple.mm`, ~463 líneas) **ya no existen**: hoy son
> `.cpp`. Uno de ellos, `messages_apple`, se daba aquí por «Cocoa/locale, inevitable».
> No lo era.

*Texto original, conservado como registro de por qué se pensaba así:*

- **GHOST macOS Cocoa/AppKit** (~4.340 líneas): `intern/ghost/intern/GHOST_SystemCocoa.mm` (2.199), `GHOST_WindowCocoa.mm` (1.300), `GHOST_ContextCGL.mm`, `GHOST_NDOFManagerCocoa.mm`, `GHOST_SystemPathsCocoa.mm`. **AppKit no tiene binding C++** → imposible C++ puro. **Mantener como wrapper aislado, marcar EXTERNAL/PLATFORM**, exponer solo interfaz `.hh` al motor.
- **Backend GPU Metal** (`source/blender/gpu/metal/*.mm`, ~20.950 líneas): usa API ObjC de Metal. **Matiz honesto:** SÍ podría converger a C++ con **metal-cpp** (binding oficial de Apple, *no* vendorizado en el árbol). Migración por fases de alto coste; el resultado seguiría sobre runtime ObjC. Prioridad media-alta *solo* si el objetivo es reducir `.mm` a cero.
- **Cycles Metal** (`intern/cycles/device/metal/*.mm`, ~5.060 líneas): render **offline**, probablemente innecesario en runtime de juego. **Candidato a excluir del build.**
- **Shims Apple** (~463 líneas): `storage_apple.mm`/`fileops_apple.mm` (Foundation+POSIX, recortables), `messages_apple.mm` (Cocoa/locale, inevitable), `thumbnail_provider.mm` (QuickLook, opcional → eliminable).

### 3.4 Terceros en `extern/` — **EXTERNAL, no reescribir**

`extern/ufbx/ufbx.c` (32.988), `extern/lzma/*.c` (~10.000), `extern/lzo/minilzo.c` (6.053), `extern/cuew` + `extern/hipew` (wranglers CUDA/HIP, **irrelevantes en Mac/Metal → excluir del build**), `curve_fit_nd`, `rangetree`, `xdnd`, `binreloc`, `wcwidth`, `xxhash`, `nanosvg`. **Mantener verbatim, actualizar desde upstream. Nunca reescribir a mano.**

**C de otras plataformas** (Windows/Linux, ~800 líneas: `wayland_dynload`, `decklink/win`, `libc_compat`, `blender_launcher_win32.c`): **no se compila en Mac. Excluir del build.**

---

## 4. Shaders — **el veredicto «inviable de golpe» queda DEROGADO el 2026-09-11**

> **Qué se deroga, exactamente:** la frase de más abajo *«Convergencia al 100% C++ =
> compilarlos como C++ vía los stubs y prohibir GLSL crudo. **Inviable de golpe**
> (fork enorme frente a upstream). Solo por fases y priorizando lo propio»*.
>
> La «Decisión del 2026-09-11» de [`LENGUAJE-CPP.md`](LENGUAJE-CPP.md) incluye **los
> cuerpos de shader** en el objetivo: cero GLSL escrito a mano. Sigue siendo caro;
> deja de ser aceptable.
>
> **Y el motivo que daba ya no existe.** «Fork enorme frente a upstream» era un coste
> real mientras Flipendo intentara incorporar cambios de Blender. Pero la propia
> `LENGUAJE-CPP.md` declaró la **divergencia total** el 2026-09-05 —*«Flipendo ya no
> persigue mantener sincronía con Blender/UPBGE»*—, así que este §4 se escribió
> apoyándose en una restricción que la doctrina ya había levantado. No es que la
> decisión del 11 cambie el balance: es que el balance estaba mal calculado.
>
> **Cifras remedidas hoy** (commit `87ce606a318`): **745 ficheros `.glsl`, 68.391
> líneas** — clavadas desde el 5 de septiembre, nadie las ha tocado. De ellas, solo
> **183 en 12 ficheros** son del game engine (`RAS_OpenGLFilters/`), y ya
> cross-compilan a Metal y Vulkan desde una sola fuente. Es decir: el frente propio de
> Flipendo son 183 líneas y el heredado son 68.208.
>
> **Lo que NO se deroga, y es lo mejor del documento:** todo lo de abajo sobre
> `ShaderCreateInfo` como IR canónico, `gpu_glsl_cpp_stubs.hh` y `glsl_preprocess.cc`
> sigue intacto y es el camino. Nadie tiene que inventar un lenguaje: el andamio para
> que un `.glsl` compile como C++ moderno ya está en el árbol.

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

- **Cuerpos de shader `.glsl`** (medido hoy: **68.391 líneas, 745 ficheros**, **heredado**): son fuente escrita a mano, NO generada. Convergencia al 100% C++ = compilarlos como C++ vía los stubs y prohibir GLSL crudo. ~~**Inviable de golpe** (fork enorme frente a upstream). Solo por fases y priorizando lo propio.~~ → **DEROGADO el 2026-09-11**, ver el aviso al principio de este §4: el objetivo es cero GLSL a mano, y «fork frente a upstream» dejó de ser un coste el 5 de septiembre, cuando se declaró la divergencia total.
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

> **Nota del 2026-09-11.** De esas 479k quedan **191.224** seis días después. El
> orden de las fases de abajo sigue siendo razonable, pero **los plazos no**: la
> Fase C («1-2 años») y la Fase D («2-4 años») están hechas en su parte que
> importaba —el Player de distribución arranca sin intérprete y ata 5/5 componentes
> nativos (`PLAYER-SIN-CPYTHON.md`)— y la Fase Z dejó de ser aspiracional el día que
> empezó a ejecutarse. La estimación honesta que queda, medida al ritmo de la noche
> del 10 al 11, está en `NOCHE-2026-09-11.md`: **entre 40 y 60 noches como esa** para
> el «full C++» completo. Es una cifra con método, no una promesa.

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

### Fase Z — Editor ~~*(horizonte lejano / aspiracional / posiblemente permanente-EXTERNAL)*~~ · **EN MARCHA desde el 2026-09-10**
- ~~`bl_ui` (64.452) + `bl_operators` (19.165) + addons (151.293)~~ → medido hoy: `bl_ui` **51.741**, `bl_operators` **12.569**, `addons_core` **22.703**. Sigue en pie: `bpy` interno (~80.000 C/C++ de servicio a Python) + IO restante.
- ~~**Recomendación pragmática y honesta:** el editor heredado se **MANTIENE como dependencia EXTERNAL documentada con wrapper C++.** Intentar su eliminación total = fork completo de la arquitectura de Blender. **No se compromete su erradicación.** La regla "todo lo propio a C++" **se cumple** migrando el runtime del Player y las herramientas propias, **no** reescribiendo el editor heredado.~~

> **Esta recomendación está desmentida por los hechos, no derogada por una orden.**
> La noche del 10 al 11 se migraron a C++, con volcado idéntico como prueba, el
> sistema de herramientas, el keymap y sus menús, 52 operadores, cuatro pestañas de
> Propiedades, el editor de nodos entero y los paneles de juego. El editor sigue
> arrancando. La frase «intentar su eliminación total = fork completo de la
> arquitectura de Blender» era cierta **bajo el supuesto de mantener la sincronía con
> upstream**, y ese supuesto se había levantado ya el 5 de septiembre.
>
> Lo que sí quedó demostrado de esta fase, y hay que conservar: **la interfaz se migra
> por editores completos, nunca panel suelto** (el orden dentro de la región cambia y
> el volcado marca diferencia aunque no cambie ningún campo), y **los `draw()` que dos
> editores comparten se exponen como funciones C++ en una cabecera pública**, nunca se
> reimplementan dos veces. Ver `UI-A-CPP.md` y `PROPERTIES-A-CPP.md`.

---

## 7. Qué NO se migra

- **Assets y datos**: `.blend`, texturas, mallas, audio.
- **Datos disfrazados de código**: ~~`scripts/presets/` (~14.323, solo asignan propiedades → convertibles a JSON/TOML, baja prioridad)~~ **HECHO (2026-09-11):** `scripts/presets/` ya no tiene **ni un `.py`** — son 172 `.fpreset` (datos) con lector, escritor y conversor en C++ (`PRESETS-A-DATOS.md`, `--fl-check-presets`). `tools/svn_rev_map/*.py` (108.736 líneas — HECHO: asset binario + lector C++).
- **Formatos de serialización**: sistema DNA (`dna_defaults.c`, layout de `.blend`) mientras se conserve el formato.
- **Salidas de compilador/codegen**: MSL y SPIR-V generados en runtime, cadenas C embebidas por `glsl_preprocess`.
- **Terceros aislados** en `extern/` y `lib/` (ufbx, lzma, lzo, cuew/hipew, metal-cpp, etc.): EXTERNAL, actualizar desde upstream. **Es la única excepción doctrinal** al «full C++» de la decisión del 2026-09-11.
- **Código de otras plataformas** (Windows/Linux): excluir del build de Mac.
- ~~**Wrappers de plataforma inevitables** (GHOST Cocoa/AppKit, shims Cocoa/QuickLook): PLATFORM, no C++ puro por diseño de Apple.~~ → **SALE DE ESTA LISTA el 2026-09-11** (ver §3.3): no eran inevitables. Los tres shims de Apple ya son `.cpp` y el backend de Metal está en camino con `extern/metal-cpp`. `OBJC-A-CPP.md`.
- **Tests/doc/build heredados**: utillaje de desarrollo, no se distribuye; los tests **nuevos** de Flipendo sí en C++.

---

*Resumen de una línea: lo PROPIO de Flipendo y las reglas de código nuevo son de hoy; el motor heredado converge por fases a lo largo de años; el editor Blender se mantiene EXTERNAL y su erradicación total no se promete. Todo lo verificado procede del árbol real; el código propio en `~/Flipendo/` se auditó en disco (1.954 líneas no-C++).*
---

## Actualización Fase B (2026-09-05): el game engine ya es C++

Auditoría de `source/gameengine/` (el núcleo del motor que Flipendo mantiene):

| Tipo | Ficheros (5 sep) | Líneas (5 sep) | **Ficheros (11 sep)** | **Líneas (11 sep)** |
|------|---------:|-------:|---------:|-------:|
| `.cpp` | 220 | 83.705 | **223** | **85.325** |
| `.h` (headers) | 246 | 28.013 | **0** | **0** |
| `.hpp` (nuestros) | 2 | 321 | **250** | **28.988** |
| `.glsl` (filtros 2D) | 12 | 183 | **12** | **183** |
| `.c` / `.py` / `.mm` | **0** | **0** | **0** | **0** |

> **Corrección del 2026-09-11: las columnas de «5 sep» ya eran falsas el día que se
> escribieron.** Medido en el propio commit del 5 de septiembre (`d2a1ca3c2b7`),
> `source/gameengine/` tenía **0 ficheros `.h` y 248 `.hpp`**. El renombrado en masa
> ya había ocurrido; la tabla describía un árbol anterior. Y la conclusión que se
> apoyaba en ella —*«Renombrarlos a `.hpp` rompería miles de `#include` y, sobre todo,
> la sincronización con UPBGE […] Se convierten solo cuando se reescriba su
> subsistema, no en masa»*— **describe como imposible algo que ya estaba hecho y
> compilando**. Hoy son 250 `.hpp` y 28.988 líneas.
>
> Fuera del game engine sí quedan las **1.389 cabeceras `.h` / 248.745 líneas** del
> resto del árbol, intactas desde el 5 de septiembre. Ahí la regla de «solo al
> reescribir su subsistema» sigue siendo la que hay.

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
- ~~Los 246 `.h` de `source/gameengine/` son de **UPBGE** (heredados). Renombrarlos a
  `.hpp` rompería miles de `#include` y, sobre todo, **la sincronización con UPBGE**
  (la estrategia de `externo/` para aprovechar sus mejoras). Se convierten solo
  cuando se reescriba su subsistema, no en masa. Es coherente con la doctrina
  (heredado = EXTERNAL hasta reescritura).~~ → **Obsoleto: ya estaban renombrados
  cuando se escribió esto** (0 `.h`, 248 `.hpp` en `d2a1ca3c2b7`). La regla de «solo
  al reescribir su subsistema» sigue valiendo para las 1.389 `.h` del **resto** del
  árbol.

## Frontera honesta de "todo a C++" — **revisada el 2026-09-11**

- **Todo lo que Flipendo escribe y el game engine entero: C++.** ✔ (sigue siendo cierto)
- **Shaders propios: fuente única C++/create-info → Metal/Vulkan.** ✔ (sigue siendo cierto)
- ~~**Frontera:** el editor de Blender (≈293k líneas Python) y sus shaders/headers
  heredados. Migrarlos = forkear Blender entero y **perder** la capacidad de
  incorporar sus actualizaciones. Se mantienen EXTERNAL por diseño. No es una
  limitación de esfuerzo, es la decisión de arquitectura que hace viable el fork.~~

> **Esta «frontera» ya no existe, y conviene entender por qué se creía en ella.**
>
> El argumento era correcto salvo por una premisa: *perder la capacidad de incorporar
> las actualizaciones de Blender*. Flipendo **renunció a esa capacidad el 2026-09-05**,
> en la misma `LENGUAJE-CPP.md` («Estrategia: divergencia total»). Con la premisa
> caída, lo que quedaba no era una decisión de arquitectura sino una estimación de
> esfuerzo — y las estimaciones de esfuerzo se comprueban midiendo, no argumentando.
> Se midió: 292.753 → 119.105 líneas en `scripts/` en seis días.
>
> **La frontera real de hoy**, medida y sin retórica:
>
> | Qué | Líneas | Por qué sigue ahí |
> |---|---:|---|
> | Python del editor | 191.224 | se migra por editores completos; `BACKLOG-EDITOR-PYTHON.md` |
> | `.h` heredadas (fuera del game engine) | 248.745 | nadie ha empezado; a `.hpp` al reescribir su subsistema |
> | GLSL heredado | 68.208 | nadie ha empezado; el propio son 183 líneas |
> | Objective-C++ | 30.304 | 441 envíos de mensaje; `OBJC-A-CPP.md` |
> | `extern/` + `lib/` | 919.268 | **frontera doctrinal de verdad**: terceros vendorizados, no se reescriben nunca |
>
> Solo la última fila es una frontera en el sentido en que lo era esta sección. Las
> otras cuatro son trabajo pendiente con su documento y su método.
