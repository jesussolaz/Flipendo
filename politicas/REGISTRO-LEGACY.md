# Registro de código no-C++ (legacy / external / platform)

Índice de todo el código no-C++ que Flipendo toca, con su etiqueta. Regla 5c de
[`LENGUAJE-CPP.md`](LENGUAJE-CPP.md). Buscar por etiqueta para saber qué converge y
qué no.

> **Comprobado contra el árbol el 2026-09-11** (commit `87ce606a318`). La versión
> anterior era del 5 de septiembre y **tres de sus entradas estaban muertas**: una
> deuda que se cerró hora y media después de escribirla, un directorio de Python que
> ya no existe y un reparto de `.mm` que había cambiado. Lo que el árbol desmiente se
> corrige aquí diciendo que estaba mal y por qué.

---

## 0. Qué se comprobó, y cómo

Etiqueta a etiqueta, buscándolas en el árbol:

```sh
grep -rln 'LEGACY — PENDING\|LEGACY - PENDING\|PENDING C++/IR' source/ intern/ tools/
```

**Resultado: un solo fichero**, `source/gameengine/Rasterizer/RAS_Shader.cpp`. Es un
hallazgo, no un trámite: la regla 5c dice que *todo* fichero heredado no-C++ que
Flipendo toque lleve su etiqueta, y este registro daba por marcados los 11 (hoy 12)
`.glsl` de `RAS_OpenGLFilters/`. **No lo están**: ni uno lleva cabecera. Están
registrados aquí, que es lo que de verdad importa, pero la etiqueta en el fichero no
existe. Queda anotado como pendiente en vez de dar la regla por cumplida.

---

## `LEGACY — PENDING C++/IR MIGRATION` (deuda propia a converger)

| Ubicación | Qué es | Fase | Estado 2026-09-11 |
|-----------|--------|------|---|
| `source/gameengine/Rasterizer/RAS_Shader.cpp:296-485` | `GetParsedProgram`: parser de texto GLSL (compat GL2 por regex). Frágil. Reducir a la ruta runtime solo para filtros del usuario. | B | ✅ **vivo y etiquetado** — la etiqueta está en la línea 296; la función va de la 300 a la 485. La traducción de `gl_TexCoord[0]`/`gl_FragColor`/`texture2D(` está hoy en las líneas **470-478**. `LinkProgram` (ya C++, se conserva) empieza en la **563** y llama a `GPU_shader_create_from_info` en la **612**. |
| `source/gameengine/Rasterizer/RAS_OpenGLFilters/*.glsl` | Cuerpos de filtro 2D ensamblados por texto en runtime. Convertir a `*_info.hh` estático. | B | ⚠️ **vivos pero SIN etiqueta en el fichero.** Y son **12**, no 11: el registro anterior olvidaba `RAS_FlipendoKH2DFilter.glsl` (67 líneas), que además es **código propio de Flipendo**, no heredado. Total 183 líneas. |
| ~~`~/Flipendo/game/template/*.py` (1.822 líneas)~~ ✅ **ELIMINADO** | Gameplay ARPG, addons, tests — código propio en Python. | A | **`~/Flipendo/game/template/` ya no tiene ni un `.py`.** Quedan el `.blend` generado (`ArpgNative.blend`), el `KeyARPG.blend` y las capturas. |
| ~~`flipfx.py` (132 líneas)~~ ✅ **MIGRADO** | Look KH → filtro nativo `FILTER_FLIPENDOKH` (C++/GLSL integrado). | ✅ Fase A | Su cuerpo GLSL es el `RAS_FlipendoKH2DFilter.glsl` de la fila de arriba. |
| ~~`arpg_core.py` (150 líneas)~~ ✅ **MIGRADO** | Lógica pura (combos/salud/lock-on/IA) → `FL_ArpgCore.hpp` C++ + test C++. | ✅ Fase A | |
| ~~`arpg.py` (303 líneas)~~ ✅ **MIGRADO** | Componentes (Player/Cámara/Enemy) → sistema de componentes C++ nativo (`FL_Component` + `FL_ArpgComponents`). | ✅ Fase A | |

### Entrada NUEVA — el Python que ha aparecido fuera del repo

Mientras se cerraba la Fase A, ÁNIMA ha traído Python nuevo a `~/Flipendo/game/`. No
estaba en este registro porque no existía el 5 de septiembre. **Se registra ahora, que
es justo lo que la regla 5c pide**, con el criterio que ya se aplicó a
`gen_template.py`: son **generadores de datos** (producen `.blend`), no
implementación del motor.

| Fichero | Líneas | Qué es |
|---|---:|---|
| `~/Flipendo/game/anima/gen_t1_lamancha.py` | 1.107 | genera la escena `T1_LaMancha.blend` |
| `~/Flipendo/game/anima/gen_molino.py` | 946 | genera el molino |
| `~/Flipendo/game/anima/gen_molino_interior.py` | 737 | genera el interior |
| `~/Flipendo/game/anima/gen_alonso_procedural_descartado.py` | 691 | descartado, se conserva como registro |
| `~/Flipendo/game/anima/anima_kit.py` | 578 | utilidades de los generadores |
| `~/Flipendo/game/anima/gen_alonso.py` | 403 | genera a Alonso (base MPFB2, CC0) |
| `~/Flipendo/game/anima/gen_alonso_estilo.py` | 336 | estilo del personaje |
| `~/Flipendo/game/anima/gen_alonso_base.py` | 66 | base del personaje |
| **Total** | **4.864** | |

**Criterio, escrito para que no se discuta dos veces:** un generador que corre una vez
en el editor y deja un `.blend` es **herramienta de autoría**, igual que
`gen_template.py`, que se eliminó sustituyéndolo por su salida. La doctrina no exige
reescribirlos en C++ mientras no formen parte del motor ni del juego exportado. Lo que
**sí** exige es que estén registrados: el día que uno de estos generadores acabe
haciendo lógica de juego, deja de ser un generador.

---

## ~~`PLATFORM` (wrapper de plataforma inevitable — permitido, no es deuda)~~ · **DEROGADA la etiqueta el 2026-09-11**

> La «Decisión del 2026-09-11» de [`LENGUAJE-CPP.md`](LENGUAJE-CPP.md) dice que **full
> C++ incluye el Objective-C++**. Por tanto **`PLATFORM` deja de significar
> «permitido, no es deuda»** y pasa a ser sinónimo de deuda abierta con su plan en
> [`OBJC-A-CPP.md`](OBJC-A-CPP.md). El argumento con el que se creó esta etiqueta
> —«AppKit no tiene binding C++»— es además falso: el runtime de Objective-C es una
> biblioteca de C, y `metal-cpp`, el binding oficial de Apple, es exactamente eso.

| Ubicación | Ficheros | Líneas | Estado |
|-----------|---:|---:|---|
| `source/blender/gpu/metal/*.mm` | 18 | **20.718** | **en migración.** Eran 20 ficheros / 20.950: `mtl_query.mm` y `mtl_shader_log.mm` ya son `.cc`. El trabajo real son **441 envíos de mensaje** en los que quedan, no 20.000 líneas. |
| `intern/ghost/intern/*Cocoa*.mm` + `GHOST_ContextCGL.mm` | 5 | **4.340** | pendiente. `GHOST_SystemCocoa.mm` 2.199 · `GHOST_WindowCocoa.mm` 1.300 · `GHOST_ContextCGL.mm` 434 · `GHOST_NDOFManagerCocoa.mm` 280 · `GHOST_SystemPathsCocoa.mm` 127. |
| `intern/cycles/device/metal/*.mm` + `bvh/metal.mm` | 8 | **5.060** | **aplazado con motivo medido, no por pereza:** `WITH_CYCLES:BOOL=OFF` en `dev/build/CMakeCache.txt` y `grep -c 'cycles/device/metal' dev/build/build.ninja` da **0**. No se compilan, así que migrarlos no quita ni una línea del binario y **no habría forma de verificarlo**. Se migran cuando (y si) se encienda Cycles. |
| `source/blender/blendthumb/src/thumbnail_provider.mm` | 1 | **186** | suelto (QuickLook). |
| **Total `.mm`** | **32** | **30.304** | |

### Los tres *shims* de Apple ya no están aquí, y uno era «inevitable»

El registro anterior listaba `storage_apple.mm`, `fileops_apple.mm` y
`messages_apple.mm` (~463 líneas) como wrappers de plataforma. **Los tres son hoy
`.cpp`**:

- `source/blender/blenlib/intern/storage_apple.cpp`
- `source/blender/blenlib/intern/fileops_apple.cpp`
- `source/blender/blentranslation/intern/messages_apple.cpp`

De `messages_apple` se decía en `MIGRACION-CPP.md §3.3` que era «Cocoa/locale,
**inevitable**». No lo era. Es el mejor argumento contra clasificar algo como
imposible sin intentarlo.

---

## `EXTERNAL` (terceros vendorizados / editor heredado)

| Ubicación | Qué es | Estado 2026-09-11 |
|-----------|--------|---|
| `extern/` **y `lib/`** | Librerías vendorizadas (ufbx, lzma, lzo, cuew/hipew, xxhash, **metal-cpp**…) | ✅ **Única excepción doctrinal al «full C++»**, y ya está en el árbol: `.gitattributes:107-108` los marca `linguist-vendored`. 2.564 ficheros, 919.268 líneas. Se mantienen verbatim y se actualizan desde upstream. **`lib/` es nuevo en esta entrada**: antes solo figuraba `extern/`. |
| ~~`scripts/` (~292.753 líneas Python)~~ → **119.105 en 251 ficheros** | Editor heredado de Blender | ⚠️ **Ya no es EXTERNAL: es deuda en migración.** La decisión del 11 y la noche del 10 al 11 sacan el editor de esta tabla. Ver [`BACKLOG-EDITOR-PYTHON.md`](BACKLOG-EDITOR-PYTHON.md). |
| ~~CPython embebido~~ | Intérprete del Player/Editor durante la transición | ✅ **Fuera del Player.** La configuración de distribución (`dev/build-nopy`, `WITH_PYTHON=OFF`) no lo lleva: 0 símbolos `^_Py`, bundle 772 → 537 MB. Sigue en el **editor**, que es donde tocaba. |

> Detalle y roadmap completo en [`MIGRACION-CPP.md`](MIGRACION-CPP.md) (con sus §3.3 y
> §4 derogados) y en [`METRICAS.md`](METRICAS.md).

---

## Estado del código PROPIO de Flipendo fuera del repo

**Código de motor / gameplay / runtime propio: 100% C++.** Sigue siendo cierto.

| Fichero | Líneas | Estado |
|---|---:|---|
| `~/Flipendo/addons/key_assistant/__init__.py` | 386 | **Vivo.** Addon del **editor**; la API de addons de Blender (`bpy`) es Python. Categoría EXTERNAL (capa editor) mientras el editor ejecute Python. Cae con él, no antes. |
| `~/Flipendo/game/anima/*.py` | 4.864 | **Vivo, nuevo, registrado arriba.** Generadores de datos de ÁNIMA. |
| ~~`~/Flipendo/addons/keyfx/keyfx.py` (179)~~ | 0 | Ya no está en disco. |
| ~~`gen_template.py` (127)~~ ✅ **ELIMINADO** | | Sustituido por el `.blend` ya generado (dato). |
| ~~`bin/flipendo` bash (127)~~ ✅ **MIGRADO a C++** | | `tools/flipendo_cli/flipendo_cli.cpp` (clonefile APFS, removexattr, statvfs). **Comprobado:** `file ~/Flipendo/bin/flipendo` → `Mach-O 64-bit executable x86_64`. |

### Shell: cero `.sh`, pero no cero shell — dicho antes de que lo encuentre otro

`METRICAS.md` da **shell propio = 0 ficheros**, y es cierto **por extensión `.sh`**.
Buscando por *shebang* en lugar de por extensión aparecen dos ficheros que ese
recuento no ve, y los dos están bien donde están, pero deben figurar en este registro:

| Fichero | Líneas | Qué es | Etiqueta |
|---|---:|---|---|
| `tools/flipendo_cli/flipendo.bash.legacy` | 127 | el gestor de versiones en bash, **conservado a propósito** como registro de lo que se migró a `flipendo_cli.cpp`. No se ejecuta ni se instala. | `LEGACY` histórico, **no** deuda |
| `release/darwin/scripts/blender-system-info.sh.in` | 16 | plantilla que CMake configura (`.in`) para el informe de sistema del bundle de macOS. Heredado de Blender. | `EXTERNAL` heredado |

Comprobación reproducible:

```sh
git grep -l -E '^#!.*(bash|/bin/sh|zsh|ksh)' HEAD -- '*' \
  | sed 's|^HEAD:||' | grep -vE '^(extern|lib)/'
```

**143 líneas en total, y ninguna se ejecuta en el motor ni en el juego.** El objetivo
doctrinal «cero shell propio» se da por cumplido, pero se da por cumplido **sabiendo
esto**, no por no haber mirado. Un contador que solo conoce `.sh` no puede certificar
un objetivo de este tipo: es el mismo fallo de método que el `+1` por fichero
documentado en `METRICAS.md §6.2`.

---

## C heredado de Blender migrado a C++ — **objetivo cerrado**

| Fichero | Líneas | Estado |
|---|---:|---|
| `source/blender/blenkernel/intern/bullet.c` | 95 | ✅ → `bullet.cc` |
| `intern/clog/clog.c` | 796 | ✅ → `clog.cc` (logging; linkage C preservado) |
| `source/blender/makesdna/intern/dna_defaults.c` | 677 | ✅ → **`dna_defaults.cc` (683 líneas)** — ver abajo |

### La entrada de `dna_defaults.c` llevaba seis días mintiendo

La versión anterior de este registro la describía así: *«🔬 **DEUDA con evidencia**
(no HOLD ciego). Intentado .c→.cc el 2026-09-05. Compilador: 15 errores
`non-aggregate type ... with a designated initializer list` […] **Causa raíz:**
`DNA_DEFINE_CXX_METHODS` declara ctores copy/move `= delete` en TODOS los structs DNA
→ no-agregados en C++ […] **Estado:** revertido a `.c` […] deuda abierta pendiente de
decisión arquitectónica, no cerrada»*.

**Medido: el fichero se llama `dna_defaults.cc` y se llama así desde el 2026-09-05 a
las 19:38**, commit `0623fbd3add` — una hora y cuarenta y cuatro minutos después de
que se escribiera el párrafo de arriba (`bab970fd7b3`, 17:54). El registro nunca se
actualizó.

**Y el diagnóstico era correcto; lo que falló fue el catálogo de salidas.** Se
plantearon dos, ambas malas: (G) quitar `DNA_DEFINE_CXX_METHODS` de todos los DNA,
perdiendo la seguridad de ownership; (H) rearquitecturar los defaults a asignación,
reescribiendo ~80 cabeceras. La solución fue una tercera que no estaba en la lista:
**un flag, `DNA_DEFAULTS_SKIP_CXX_METHODS` en `DNA_defs.h`, que desactiva esos
constructores solo en las unidades de traducción de datos**. Los structs vuelven a
ser agregados ahí y la inicialización designada de C++ es legal, sin tocar el resto
del árbol. El layout no cambia (los métodos no añaden campos) y los defaults cruzan
como `void *` por `DNA_default_table`. `AssetMetaData`, que tiene ctor/dtor propios,
se trata como un buffer de bytes a cero.

Verificado en su commit: build verde, `camera.lens` por defecto = 50.0, guardar y
recargar `.blend` íntegro, preferencias y tema por defecto correctos, ARPG 5/5.

**La lección, que es lo que hay que llevarse:** «deuda con evidencia» describía bien
el problema y mal el espacio de soluciones. Dos alternativas malas no demuestran que
no haya una tercera buena. Y un registro que no se revisa convierte un problema
resuelto en un obstáculo que nadie vuelve a mirar durante seis días.

**C propio en todo el repo: 0 ficheros, 0 líneas.** Los 24 ficheros `.c` / 58.188
líneas que quedan están todos en `extern/` (ufbx, lzma, lzo…): EXTERNAL, no se
reescriben a mano.

---

## Lo que queda pendiente de este registro

1. **Poner la etiqueta `LEGACY — PENDING C++/IR MIGRATION` en los 12 `.glsl`** de
   `RAS_OpenGLFilters/`. Hoy solo `RAS_Shader.cpp` la lleva.
2. **Decidir qué significa `PLATFORM` a partir de ahora.** Se ha dejado como
   sinónimo de deuda abierta, pero la etiqueta sigue escrita en documentos antiguos
   con su sentido viejo («permitido, no es deuda»). O se renombra, o se retira.
3. **Revisar este fichero cada vez que se cierre una deuda**, no cada varios días. El
   caso `dna_defaults` es la prueba de lo que cuesta no hacerlo.
