# Backlog ejecutable — Migración del Python del editor a C++ (Flipendo)

> Repo: `/Users/jesussolaz/Flipendo/dev/upbge`. Doctrina: todo el código estructural propio converge a C++. Sin sync con upstream (divergencia total permitida).

> ## 🔄 REMEDIDO EL 2026-09-11 — el árbol que describía este documento ya no existe
>
> Este backlog se escribió el 5 de septiembre y describe un `addons_core` con
> rigify, glTF2, FBX y bge_mixer que **hoy no está**, y llama «plurianual» a un
> trabajo que medía 479k líneas cuando se escribió la frase y mide **191.224** ahora.
> Todas las cifras de abajo se han vuelto a medir al commit `87ce606a318`
> (`git ls-tree` + `git grep -c ''`, sin `extern/` ni `lib/`). Lo que estaba mal se
> corrige diciendo que estaba mal y por qué; lo que ya está conseguido se marca en el
> nuevo §0.

---

## 0. Lo ya conseguido (medido, no recordado)

Estas piezas estaban aquí como trabajo futuro y **están hechas**, cada una con su
volcado idéntico como prueba y su política. Se listan para que nadie vuelva a
planificarlas.

| Pieza | Antes (10 sep 23:19) | Hoy | Prueba | Política |
|---|---:|---:|---|---|
| **Sistema de herramientas** | `space_toolsystem_toolbar.py` 3.779 + `space_toolsystem_common.py` 1.086 + `bpy/utils/toolsystem` + `keymap_from_toolbar.py` 394 | **0** | catálogo 30/30 · activación 474/474 · `--fl-check-tools` 0 diferencias | `TOOLSYSTEM-A-CPP.md` |
| **Keymap** | `keyconfig/keymap_data/blender_default.py` 8.669 + `keyconfig/Blender.py` 386 | **0** → `Blender.fpreset` + `fl_keyconfig_io.cc` | `--fl-check-keymap` 248 keymaps, 0 por transliterar | `KEYMAP-A-CPP.md`, `MENUS-DEL-KEYMAP-A-CPP.md` |
| **Presets (los datos)** | `scripts/presets/` 173 `.py` / 10.287 | **0 `.py`**, 172 `.fpreset` | `--fl-check-presets` 148/166 idénticos, **0 distintos** | `PRESETS-A-DATOS.md` |
| **`bl_operators/wm.py`** | 3.015 | **no existe** | `--fl-dump-operators` cmp=0 · 24 operadores | `WM-SISTEMA-A-CPP.md` y familia |
| **Editor de nodos** | `space_node.py` 1.201 (34 clases) | **84** (7 clases) | 27 de 34 tipos en `fl_node_ui.cc` (1.912 líneas) | `UI-A-CPP.md` |
| **Paneles de juego** | `properties_game.py` 899 | **0** | `--fl-check-ui` 2.113/2.113 | `UI-A-CPP.md` |
| **Addons del motor** | 4 `.py` de `addons_core` (1.617) | **0** | ÁNIMA publicada, `.zip` de 269 MB que arranca | `ADDONS-MOTOR-A-CPP.md` |
| **Player sin CPython** | — | **hecho** | 0 símbolos `^_Py`, bundle 772 → 537 MB, 5/5 componentes | `PLAYER-SIN-CPYTHON.md` |

Las **84 líneas que quedan de `space_node.py`** merecen una nota, porque son la regla
del proyecto hecha código: son los paneles que el editor de nodos **clona** de
Propiedades. No se reimplementan aquí; esperan a que su original esté en C++ y exponga
su `draw()` en una cabecera pública. Dos copias de la misma lógica se desincronizan.

---

## 1. Resumen honesto de magnitud — **remedido el 2026-09-11**

Python propio en todo el repo: **191.224 líneas en 613 ficheros**. Bajo `scripts/`,
que es de lo que trata este backlog: **119.105 líneas en 251 ficheros**.

| Bloque | Decía (5 sep) | **Hoy** | Ficheros | Qué pasó |
|---|---:|---:|---:|---|
| `scripts/startup/bl_ui` | 64.452 | **51.741** | 67 | toolsystem, nodos y paneles de juego, en C++ |
| `scripts/modules` | 31.544 | **24.544** | 87 | |
| `scripts/addons_core` | ~151.293 | **22.703** | 17 | rigify, glTF2, FBX y bge_mixer **ya no están** |
| `scripts/startup/bl_operators` | 19.165 | **12.569** | 27 | `wm.py` entero fuera |
| `scripts/freestyle` | 6.541 | **6.541** | 46 | intacto |
| `scripts/presets` | ~14.323 | **0** | 0 | datos + lector C++ |
| **Runtime del juego** | **4.186** | **2.391** | 15 | solo `bgui`; `bge_extras` y `templates_py_components`, fuera |

Fuera de `scripts/` quedan 72.119 líneas más de Python: `tests/` 44.842 (278 f.),
`intern/` 6.262, `release/` 2.063, `build_files/` 1.879, `doc/` 1.484 y utillaje.
**No se distribuyen**, pero cuentan para la doctrina y para el total de arriba.

### Qué decía este §1 antes, y por qué estaba mal

1. **«~292.753 líneas en 882 ficheros `.py` (17 MB)».** Hoy son **119.105 en 251**.
   El documento no envejeció: **nació ya con dos días de retraso**, porque la cifra
   venía de `METRICAS.md`, que el 8 de septiembre publicaba un «snapshot» que en
   realidad era del 5 (ver `METRICAS.md §6.1`). La consecuencia es concreta: se
   llamó «plurianual» a un trabajo cuyo tamaño ya era un 30 % menor cuando se
   escribió la frase.

2. **El desglose de `addons_core` describe un directorio que no existe.** Decía
   «14 dirs + 7 sueltos, ~151.293 líneas: rigify 46.480, bge_mixer 21.870, bl_pkg
   19.687, glTF2 30.333, FBX-python 11.943». Medido hoy: **17 ficheros `.py`,
   22.703 líneas**, y de aquella lista solo sobrevive `bl_pkg`:

   | Fichero | Líneas |
   |---|---:|
   | `bl_pkg/` (gestor de extensiones, 14 f.) | 19.677 |
   | `game_engine_spring_bones.py` | 1.301 |
   | `copy_global_transform.py` | 1.261 |
   | `normal_map_to_group.py` | 464 |

3. **«4 dirs `bge_*` vacíos […] Limpieza: eliminar directorios»: consejo peligroso,
   corregido.** No son directorios vacíos: son **submódulos de git** (`.gitmodules`,
   modo `160000`) que apuntan a `UPBGE-logicnodes`, `bricky-nodes`, `EasyOnline` y
   `The_Lightmapper`. Están vacíos porque **no están inicializados**, no porque no
   tengan nada dentro. Borrarlos con `rmdir` no es limpieza: es retirar del fork
   cuatro addons de UPBGE, uno de ellos (`bge_netlogic`) el sistema de nodos
   lógicos, que es capacidad **de motor de juego**, no de editor. Si se retiran,
   se retiran con la doctrina delante —demostrando que no son capacidad, o
   sustituyéndolos— y por su camino de submódulo. Y hay una segunda razón para no
   tocarlos a la ligera: la lección de las 02:30 de la noche del 10 al 11 es
   justamente que una operación de git sobre un gitlink dejó el árbol sin compilar
   para todos los carriles.

4. **La conclusión de fondo está desmentida.** Decía: *«convertir 293k líneas es
   plurianual y en su mayoría nunca compensa […] el resto del editor Python se queda
   en Python»*. En una noche cayeron **52.454 líneas** de Python sin perder una sola
   capacidad: 52/52 operadores y 24/24 clases de interfaz de los `.py` retirados
   están presentes y medidos **en el binario**
   (`REVISION-FINAL-2026-09-11.md §2.1`). Lo que sí sigue siendo cierto de aquel
   párrafo es la **prioridad**: quitar CPython del Player era lo que tenía retorno
   inmediato, y se hizo primero.

**La distinción que ordena todo el backlog:**

- **Editor-solo (~288.500 líneas, >98% → hoy ~116.700):** no lo carga jamás el Player exportado. El juego usa `.blend`/LibLoad y lógica C++, no `bpy`. Migrar esto a C++ tiene **valor CERO para el juego exportado** y solo entra en juego si algún día el editor entero converge a C++ ~~(horizonte aspiracional)~~ → **decidido el 2026-09-11: sí converge**.
- **Runtime-juego (~4.186 líneas → hoy 2.391, solo `bgui`):** ~~lo único con retorno doctrinal inmediato~~ → **conseguido.** `bge_extras` ya no existe: el Player exportado **no importa ningún módulo Python al arrancar**, y en la configuración de distribución no lleva intérprete (0 símbolos `^_Py`). De `bgui` hay 4 de 9 widgets con equivalente nativo (`UI-JUEGO-NATIVA.md`, `K1`/`K2`), comparados píxel a píxel: 316 px distintos de 63.000.

~~**La honestidad de fondo:** convertir 293k líneas es **plurianual y en su mayoría nunca compensa**. El trabajo con retorno real no es "migrar el editor" sino **quitar CPython del Player** (~4k líneas Python + purga transversal de `#ifdef WITH_PYTHON` en el gameengine). El resto del editor Python se **queda en Python** o se **borra al recortar el editor**; no se reescribe a C++ salvo decisión estratégica de años.~~

> **Corregido el 2026-09-11**, ver el punto 4 de arriba: el Player sin CPython está
> hecho **y** el editor se está migrando, sin que lo segundo dependiera de una
> «decisión estratégica de años». La estimación honesta que queda, medida al ritmo de
> la noche del 10 al 11, es de **40 a 60 noches como esa** para el «full C++»
> completo (`NOCHE-2026-09-11.md`). Es una cifra con método, no una promesa.

**Framework nativo ya disponible (remedido el 2026-09-11):**
- Operadores nativos: **408 ficheros** en `source/blender/editors` definen `wmOperatorType` — decía 275. Mismo namespace `bpy.ops.xxx.yyy` que los Python → migrar uno mantiene idénticos los call-sites.
- Layout + RNA en C: `source/blender/editors/interface`, `makesrna`.
- Componentes nativos: `source/gameengine/Flipendo/FL_Component`, ya tickeando (`KX_Scene.cpp:2449`).
- **50 opciones `--fl-*`** en el binario (volcadores, comprobadores y autotests): eran 8 antes de la noche del 10 al 11. Medido con `grep -oh '"--fl-[a-z0-9-]*"' -r source/ | sort -u | wc -l`. Es la red de seguridad que permite retirar Python sin adivinar: sin un volcado que se reproduzca a sí mismo, una migración no se puede dar por buena.
- `WITH_PYTHON=OFF` ya no es solo «config diseñada»: es la **build de distribución** (`dev/build-nopy`), y produce un Player sin intérprete que ata 5/5 componentes en `T1_LaMancha.blend`. El «bloqueante estructural» que este documento daba por resuelto solo con un target nuevo no existía: la salida fueron **dos configuraciones del mismo árbol** (`PLAYER-SIN-CPYTHON.md §1`).

---

## 2. Lotes ordenados y ejecutables

### LOTE 1 — Semanas: runtime del juego, pequeño y aislado

**Objetivo: acercar el Player a "sin CPython".** Todo lo pequeño, aislado y con reemplazo nativo ya existente.

| # | Módulo | Ruta | Líneas | Acción |
|---|---|---|---:|---|
| 1.1 | **KX_PythonComponent** | `source/gameengine/Ketsji/KX_PythonComponent.cpp/.hpp` | 203 (+46 refs) | ✅ **HECHO (2026-09-08).** Eliminado; `FL_Component` es el único sistema de componentes. |
| 1.2 | **bge_extras/logger** | `scripts/modules/bge_extras/` | 158 | ✅ **HECHO.** El Player ya no importa ningún módulo Python al arrancar. |
| 1.3 | **wm.py — familia `WM_OT_context_*`** | **HECHO carril B** | 0 | Migrada a `wm_context_ops.cc`; el fichero completo fue eliminado al cerrar también sus operadores pesados y menús. |
| 1.4 | Operadores mate simples | `object_align.py` (407), `object_randomize_transform.py` (181), `add_mesh_torus.py` (262) | ~850 | ✅ **HECHO (2026-09-11, carril C).** `OBJECT_OT_align`/`OBJECT_OT_randomize_transform` en `editors/object/`, `MESH_OT_primitive_torus_add` en `editors/mesh/editmesh_add.cc`. Verificado con `--fl-selftest-object-ops` contra `tests/flipendo/objectops/baseline-python.txt`: randomize_transform bit-exacto (MT19937 de CPython reimplementado a mano), align y torus dentro de 1e-7/1e-8 relativo (suelo de precision float32, documentado en los commits). Detalle en `~/Flipendo/dev/noche/informes/C1-object-ops.md`. |

**Por qué Lote 1:** 1.1 y 1.2 tienen reemplazo nativo YA existente y viven en paralelo redundantes. 1.3 es el candidato ideal de conversión (patrón repetido en cientos de ops nativos, riesgo bajo). 1.4 valida el patrón "operador Python → `exec()` C++" sin dependencias externas.

---

### LOTE 2 — Meses: operadores y módulos base migrables a C++ nativo

Operadores con lógica acotada sobre APIs C ya disponibles (BMesh, depsgraph, constraints, rigidbody-setup) + helpers geométricos.

| Grupo | Rutas | Líneas | Notas |
|---|---|---:|---|
| **SCA_PythonController** | `source/gameengine/GameLogic/SCA_PythonController.cpp/.hpp` | 620 → **518 (.cpp) medido hoy** | Reemplazar por bricks nativos / FL_Component. **Riesgo: fallo silencioso** (`Trigger()` no-op en rama `#else`) → auditar `.blend` del proyecto antes de retirar. **Sigue pendiente.** |
| ~~**templates_py_components (útiles)**~~ ✅ **HECHO (2026-09-11)** | ~~`scripts/templates_py_components/`~~ | ~~1.637~~ → **0** | El directorio ya no existe. Junto con `scripts/templates_py` son **40 ficheros y 2.700 líneas** convertidas a componentes nativos `FL_Component`. Efecto colateral que conviene saber: el menú *Plantillas* del editor de texto queda vacío (`REVISION-FINAL-2026-09-11.md §3.E`). |
| ~~**wm.py — utilidades sistema**~~ ✅ **HECHO (2026-09-11, carril B)** | ~~url/path/doc/owner/tool, `blenderplayer_start`~~ | ~~1.300~~ → **0** | `wm.py` entero (3.015 líneas) fuera. 24 operadores en C++, volcado `cmp=0`. Ver `WM-SISTEMA-A-CPP.md`, `WM-OWNER-A-CPP.md`, `WM-PROPIEDADES-A-CPP.md`, `WM-BATCH-RENAME-A-CPP.md`, `WM-MENUS-A-CPP.md`. |
| **Ops malla/UV/físico** | ~~`mesh.py` (250)~~, ~~`vertexpaint_dirt.py` (198)~~, ~~`constraint.py` (121)~~, ~~`rigidbody.py` (316)~~, `uvcalc_*`, `uvcalc_lightmap.py` (688) | ~2.900 → **~1.500 pendientes** | **Hechos el 2026-09-11:** `constraint.py` (`object_constraint.cc`); `vertexpaint_dirt.py` → `PAINT_OT_vertex_color_dirt` en `editors/mesh/mesh_vertex_dirt.cc`, 13.112/13.112 elementos de color idénticos (`--fl-check-mesh-ops`, `VERTEX-DIRT-A-CPP.md`); `mesh.py` (250) y `rigidbody.py` (316) retirados, este último con `--fl-check-rigidbody-ops` 63/63. **Pendientes:** los `uvcalc_*`, incluido `uvcalc_lightmap.py` (688, intacto). |
| **bpy_extras (subconjunto mate)** | `view3d_utils.py` (181), ~~`mesh_utils.py` (464)~~, `object_utils.py` (289) | ~934 → **470 pendientes** | `mesh_utils.py` retirado el 2026-09-11 (prueba **estática**: sin una sola referencia viva; queda anotado en `REVISION-FINAL-2026-09-11.md §3.A` que es de las cuatro retiradas **sin arnés propio**). El resto: pura mate/geometría → helpers C++. `io_utils`/`node_shader_utils` NO (sirven a addons Python). |

~~**Magnitud honesta:** convertir esta fracción "core" es esfuerzo de **varios trimestres**. Es la última capa con retorno técnico razonable.~~ → **Medido el 2026-09-11:** de este lote quedan unas **2.100 líneas**, no las ~7.400 que sumaba. Lo que se llamó «varios trimestres» ha sido, en su mayor parte, una noche.

---

### LOTE 3 / HORIZONTE — Años: el grueso del editor (UI, operadores pesados, addons grandes)

**Gated por una decisión estratégica: "reescribir el editor entero en C++".** Volumen enorme, complejidad por fichero baja-media (declarativo), valor de juego nulo. No se planifica hasta que exista esa decisión.

> **Este lote dejó de estar «gated» el 2026-09-10: se está ejecutando.** La decisión
> estratégica que esperaba existe (`LENGUAJE-CPP.md`, «Decisión del 2026-09-11»), y
> `bl_ui` ha bajado de 64.452 a **51.741** líneas. La lista de abajo se conserva con
> las piezas ya cerradas tachadas, porque el orden que propone sigue valiendo.
>
> **Y con una regla de método que este lote no tenía y que costó caro descubrir: la
> interfaz se migra por *editores completos*, nunca panel suelto.** El orden dentro de
> la región cambia y el volcado marca diferencia aunque no cambie ningún campo.

- **`bl_ui`: 64.452 → 51.741 líneas (67 ficheros).** Cada `Panel`/`Menu`/`Header` → `PanelType`/`MenuType`/`HeaderType` nativo con callback `draw()` C++ sobre `uiLayout`+RNA. Los mayores que quedan: `space_view3d.py` (**9.381 → 6.919**), `space_view3d_toolbar.py` (3.080), `space_userpref.py` (3.040), `space_sequencer.py` (2.943). ~~toolsystem (~5.000 — la pieza más "lógica", registro de herramienta activa)~~ ✅ **HECHO**, y fue efectivamente la más lógica: catálogo 30/30, activación 474/474, keymap 248/248 (`TOOLSYSTEM-A-CPP.md`).
- ~~**Piezas Flipendo aisladas dentro de `bl_ui`** (candidatas a piloto si se acomete el editor): `space_logic.py` (145) y `properties_game.py` (899)~~ ✅ **HECHO (2026-09-11, carril D).** Ninguno de los dos existe ya; el editor de lógica es nativo (`source/blender/editors/space_logic/`, 7 ficheros) y los paneles de juego están en `fl_game_*.cc`. Verificado con `--fl-check-ui` 2.113/2.113 y el diseño 2.004/2.004.
- ~~**Editor de nodos**~~ ✅ **HECHO (2026-09-11):** `space_node.py` 1.201 → **84** líneas, 34 clases → 7. Los 27 tipos propios están en `fl_node_ui.cc` (1.912 líneas). Pendiente aparte: los cinco `node_add_menu*.py` (2.133 líneas en total) siguen en Python.
- **Operadores pesados (todos siguen en pie, remedidos):** `presets.py` (**1.022**, metaprogramación: genera `.py` — ojo, los *datos* de preset ya son C++, el *operador* no), `userpref.py` (1.307 → **1.156**, gestión de paquetes CPython), `node.py` (743) / `geometry_nodes.py` (391), `image_as_planes.py` (1.230), `clip.py` (1.098). ~~`wm.py` batch_rename/properties_edit (~1.550)~~ ✅ **HECHO.**
- **`bpy_types.py` (1.487 → 1.491):** métodos Python sobre tipos RNA; cirugía en el corazón del binding. **Intacto.**

~~**No prometemos fecha.** Es plurianual y buena parte podría nunca ejecutarse.~~ →
Sigue sin haber fecha, pero ya no es «podría nunca ejecutarse»: se está ejecutando.
La estimación con método es la de `NOCHE-2026-09-11.md`: 40-60 noches como aquella
para el «full C++» completo, del que esto es una parte.

---

### NO-MIGRAR — Autoría/datos, o `bpy`/CPython obliga a Python (con motivo)

| Qué | Líneas | Motivo |
|---|---:|---|
| **Capa glue `bpy/`** (`__init__`, `ops.py`, `utils`, `bpy_types`) | ~2.247 | **ES** el puente CPython↔C (`from _bpy import ...`). No se traduce: se **borra** el día que el editor no ejecute Python. Intocable mientras haya editor Python. |
| **KX_PythonInit + KX_PythonInitTypes** | ~3.060 C++ | Corazón del CPython embebido del Player (crea `bge.logic/render/types`). No se "migra": **desaparece** cuando el Player sea python-free. |
| ~~**`rigify`**~~ | ~~46.480~~ → **0** | **Ya no está en el árbol.** Se retiró en la poda; no llegó a ser una decisión de este backlog. |
| ~~**`io_scene_gltf2`**~~ | ~~30.333~~ → **0** | **Ya no está.** Nota para quien lo eche de menos: el fork **no importa ni exporta glTF**. Si alguna vez se necesita, hay que escribirlo en C++ (`source/blender/io/` no lo cubre). |
| ~~**`bge_mixer`**~~, `bl_pkg` (extensiones) | ~~21.870~~ → **0** · bl_pkg 19.687 → **19.677** | `bge_mixer` ya no está. `bl_pkg` **sí sigue**, y su clasificación cambió al medirla: admite **dos** tipos de extensión, y un tema es un `theme.xml` —datos, no Python—, así que instalar y elegir un tema **es capacidad de Flipendo**. Además la mitad de abajo del gestor ya estaba en C++ (`blenkernel/intern/preferences.cc:218-250`). Ver `EXTENSIONES-Y-EL-INTERPRETE.md`. |
| ~~**`io_scene_fbx`**~~ | ~~11.943~~ → **0** | Ya no está. El **importador** FBX sigue en C++ (`source/blender/io/fbx`); el **exportador** no existe en el fork. |
| ~~**`rna_manual_reference.py`**~~ | ~~4.272~~ → **0** | Retirado el 2026-09-11 y **sustituido, no borrado**: el mapa RNA-path → URL es ahora un dato con lector C++. Verificado con `--fl-check-manual`: **7.470 rutas idénticas, 0 distintas**. |
| **`bl_i18n_utils`** | 5.254 → **5.227** | Tooling offline de traducción `.po`. Ni se carga en el editor en ejecución. **Sigue en pie.** |
| **`addon_utils` + `_bpy_internal`** | ~4.800 → **4.209** | Descubrimiento/activación de addons (wheels pip, junctions). CPython por definición mientras los addons sean Python. **Sigue en pie.** |
| **Consola Python** (`console_python` 368, `bge/interpreter.py`) | ~1.300 | Un REPL de Python "en C++" es una contradicción. Se jubila con el intérprete. **Sigue en pie.** |
| **`rna_info`, `blend_render_info`, ~~`graphviz_export`~~, `gpu_extras`** | ~1.500 → ~1.300 | `graphviz_export` (194) retirado el 2026-09-11 con prueba **estática**; es una de las cuatro retiradas sin arnés propio (`REVISION-FINAL-2026-09-11.md §3.A`). El resto sigue. |
| **VFX/import irrelevantes** (`clip.py` 1.098, `sequencer.py` 423, `freestyle.py` 223, `image_as_planes.py` 1.230…) | ~4.750 | Video/VSE/NPR/importadores. Nada que necesite un motor de juego. Dejar o borrar. **Sin tocar.** |
| ~~**4 dirs `bge_*` vacíos** (bricknodes, easyonline, netlogic, thelightmapper)~~ | ~~0~~ | ⚠️ **ENTRADA ERRÓNEA, corregida el 2026-09-11.** No son directorios vacíos: son **submódulos de git** (`.gitmodules`, modo `160000`) hacia `UPBGE-logicnodes`, `bricky-nodes`, `EasyOnline` y `The_Lightmapper`, sin inicializar. «Eliminar directorios» habría retirado cuatro addons de UPBGE —uno de ellos el sistema de **nodos lógicos**, capacidad de motor de juego— creyendo que no había nada dentro. Ver §1, punto 3. |

---

## 3. PRIMER OBJETIVO CONCRETO — ✅ COMPLETADO 2026-09-08

> **Estado:** `KX_PythonComponent` eliminado del motor (−373 líneas netas).
> `BL_ConvertComponentsObject` (la conversión `Object.components` → instancias
> Python) ya no existe; el registro de tipo, el `EXP_ListValue<KX_PythonComponent>`
> de `KX_GameObject` y el atributo Python `obj.components` han caído con él.
> `FL_Component` (nativo, atado por la propiedad de juego `fl_component`) es el
> **único** sistema de componentes del motor.
>
> **Verificado:** build Mac verde · `test_arpg_core` 15/15 · Player sobre
> `ArpgNative.blend` ata 5/5 componentes nativos · Player sobre
> `game/anima/T1_LaMancha.blend` ata 5/5 · editor arranca (4.5.0 Alpha).
>
> **Pendiente relacionado (editor, no runtime):** `source/blender/blenkernel/intern/python_proxy.cc`
> mantiene el tipo falso `FT_KX_PythonComponent` para el panel de componentes del
> editor y el DNA `Object.components`. Es Python de editor: cae con el Lote 3.

### Registro de lo hecho: retirar `KX_PythonComponent`

**Rutas:**
- Eliminar: `source/gameengine/Ketsji/KX_PythonComponent.cpp` (146) + `.hpp` (57).
- Reemplazo vivo: `source/gameengine/Flipendo/FL_Component` (nativo, tickeando en `KX_Scene.cpp:2449`).

**Por qué este primero:** 203 líneas propias, 46 referencias **todas** bajo `#ifdef WITH_PYTHON`, y `FL_Component` ya lo sustituye y corre en el bucle de frame. Es el módulo más pequeño y aislado con reemplazo nativo **ya cableado**; hoy conviven en paralelo redundantes (`BL_DataConversion.cpp:1010` aún instancia `EXP_ListValue<KX_PythonComponent>`).

**Plan de pasos:**
1. **Auditar uso:** confirmar que los `.blend` del proyecto no dependen de `KX_PythonComponent` de usuario (o que sus componentes ya tienen equivalente `FL_Component`). Portar los que falten como componentes nativos.
2. **Migrar el camino de conversión:** en `BL_DataConversion.cpp` (líneas 1010, 1045, 1064), sustituir la instanciación de `EXP_ListValue<KX_PythonComponent>` por la lectura de `fl_component` → construir `FL_Component` nativos.
3. **Quitar el registro de tipo:** eliminar la entrada de `KX_PythonComponent` en `KX_PythonInitTypes.cpp`.
4. **Limpiar refs** en `KX_GameObject.cpp/.hpp` y borrar `KX_PythonComponent.cpp/.hpp`.
5. **Compilar y validar** con una escena que use componentes; confirmar que el tick de `FL_Component` cubre el comportamiento.

**Segundo candidato inmediato (Lote 1.2):** `bge_extras/logger.py` (131 líneas) → logger nativo C++ (`clog` ya migrado). Quitar `PyImport_ImportModule("bge_extras.logger")` de `KX_PythonInit.cpp:2059`. Elimina la **última importación Python que hace el Player exportado**.

---

## 4. La meta realista

### ~~Alcanzable a medio plazo:~~ ✅ **CONSEGUIDO el 2026-09-11: "Player sin CPython"**

- El andamiaje **ya existía**: `EXP_PyObjectPlus.hpp` colapsa `Py_Header` a `public:` vacío en la rama `#else`; `SCA_PythonController.cpp` tiene `Trigger()` no-op; `WITH_PYTHON=OFF` está diseñado (`CMakeLists.txt:230`).
- Los tres huecos están cerrados: (a) bricks Python, (b) componentes de usuario (`FL_Component`), (c) UI `bgui` (4 de 9 widgets nativos y el camino abierto).
- ~~**Caveat técnico real:** `WITH_PYTHON` es **global**, no del Player […] exige un **target/flag nuevo Player-only** — trabajo de build system […] Es el bloqueante estructural a resolver.~~

> **Ese bloqueante no existía, y es la corrección más útil de este documento.** No
> hacía falta ningún target nuevo: la salida son **dos configuraciones de build del
> mismo árbol** (`PLAYER-SIN-CPYTHON.md §1`). El editor se compila con
> `WITH_PYTHON=ON` en `dev/build`; el Player de distribución con `WITH_PYTHON=OFF`
> (+ `WITH_USD=OFF`, obligatorio: el `boost::python` de USD hace
> `#include <pyconfig.h>` incondicional y revienta el PCH de `bf_io_usd`) en
> `dev/build-nopy`. El juego se **crea** con el primero y se **envía** con el segundo.
>
> **Medido:** 0 símbolos `^_Py` (eran 2.267), `libpython` no enlazada, bundle del
> Player **772 → 537 MB**, y 5/5 componentes atados tanto en `ArpgNative.blend` como
> en `game/anima/T1_LaMancha.blend`. Y un aviso de método del mismo informe: hay 89
> símbolos que *contienen* `_Py` en el binario sin CPython, pero son nombres de clases
> de Flipendo (`KX_PythonProxy`, `SCA_PythonController`); **la cuenta honesta es
> `^_Py`**.

### Horizonte: **"editor sin Python"** — ya no es aspiracional, está en marcha

- Requiere reescribir ~~288.500~~ **~116.700** líneas de UI/operadores/addons como `PanelType`/`MenuType`/`wmOperatorType` nativos, más purgar la superficie de binding. **Remedida hoy, y esto importa: la superficie de binding ha CRECIDO, no bajado.**

  | Superficie | Decía (5 sep) | **Hoy** |
  |---|---:|---:|
  | Guardas `#ifdef/#ifndef WITH_PYTHON` | 511 | **554** |
  | Clases con `Py_Header` en el gameengine | 88 | **94** |
  | Ficheros con `PyObject` en el gameengine | 200 | **198** |
  | Líneas del gameengine | 112.039 | **116.385** |
  | `Expressions` | 7.134 | **7.225** |

  Que las guardas suban mientras el Python baja **no es una contradicción ni un
  retroceso**: siete de las guardas nuevas de la noche se auditaron una a una y cinco
  son **comentarios que documentan un hueco ya existente** («con `WITH_PYTHON=OFF` esa
  tecla no abre nada»). Escribir el hueco es lo contrario de esconderlo. Las otras dos
  son mejoras: en `numinput.cc` el `#ifdef` envolvía la función entera y ahora solo la
  llamada a CPython; en `fcurve_driver.cc` la rama `#ifndef WITH_PYTHON` era
  `UNUSED_VARS(...)`, de modo que un driver con expresión no soportada **se quedaba
  clavado en 0.0 sin decir una palabra**, y ahora avisa. Ver
  `REVISION-FINAL-2026-09-11.md §2.4`.

- ~~Es **plurianual**, transversal (no modular), y buena parte **nunca compensa**.~~
  Transversal sí sigue siendo. Lo de «nunca compensa» lo desmiente el árbol.
- ~~**No se promete.**~~ Sigue sin prometerse fecha, pero ya no es una dirección: es
  trabajo en curso con arneses que lo verifican.

---

~~**Resumen de una línea:** el retorno real está en las **~4.000 líneas de runtime + la purga de CPython del Player** (medio plazo, empezando por eliminar `KX_PythonComponent` esta semana); las **~288k del editor** son horizonte condicionado a una decisión estratégica que hoy no tiene fecha ni obligación doctrinal.~~

**Resumen de una línea, 2026-09-11:** la purga de CPython del Player **está hecha** —el
Player de distribución no lleva intérprete y ata 5/5 componentes—, y las ~288k del
editor son hoy **119.105 líneas en `scripts/`** que sí se están migrando, por editores
completos y con volcado idéntico como prueba; la decisión estratégica que este
documento esperaba se tomó el 2026-09-11 y está en `LENGUAJE-CPP.md`.

---

## 5. Cómo se mantiene este documento (añadido el 2026-09-11)

Este backlog se equivocó dos veces por el mismo motivo: **copió cifras de otro
documento en vez de medir el árbol**. La de `scripts/` venía de un `METRICAS.md` que
ya iba dos días atrasado, y la de `addons_core` describía un directorio podado.

Antes de citar una cifra de aquí, **remídela**:

```sh
cd ~/Flipendo/dev/upbge
REV=$(git rev-parse --short HEAD)
git grep -c '' $REV -- 'scripts/*' | sed "s|^$REV:||" \
  | grep -E '\.py:[0-9]+$' | awk -F: '{s+=$NF} END{print s}'
```

Y si el árbol desmiente lo que dice un párrafo, **corrígelo diciendo que estaba mal y
por qué**, como se ha hecho arriba. Un backlog con cifras viejas es peor que no tener
backlog: alguien planifica con él.
