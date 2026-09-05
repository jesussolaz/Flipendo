# Backlog ejecutable — Migración del Python del editor a C++ (Flipendo)

> Repo: `/Users/jesussolaz/Flipendo/dev/upbge`. Doctrina: todo el código estructural propio converge a C++. Sin sync con upstream (divergencia total permitida).

---

## 1. Resumen honesto de magnitud

El macizo Python del editor es **~292.753 líneas en 882 ficheros `.py` (17 MB)** bajo `scripts/`. Medido de verdad. Se reparte así:

| Bloque | Líneas | Naturaleza |
|---|---:|---|
| `scripts/addons_core` (14 dirs + 7 sueltos) | ~151.293 | Autoría de editor (upstream). NINGÚN fichero importa `bge`. |
| `scripts/startup/bl_ui` (78 ficheros) | 64.452 | UI declarativa del editor (1173 Panel, 562 Menu…). 0 importan `bge`. |
| `scripts/startup/bl_operators` (34) | 19.165 | Operadores `bpy.types.Operator`. |
| `scripts/modules` (~90) | 31.544 | Módulos base + glue CPython (`bpy/`, `bpy_types.py`…). |
| **Runtime del juego (objetivo real)** | **4.186** | `bgui` 2.391 + `bge_extras` 158 + `templates_py_components` 1.637. |

**La distinción que ordena todo el backlog:**

- **Editor-solo (~288.500 líneas, >98%):** no lo carga jamás el Player exportado. El juego usa `.blend`/LibLoad y lógica C++, no `bpy`. Migrar esto a C++ tiene **valor CERO para el juego exportado** y solo entra en juego si algún día el editor entero converge a C++ (horizonte aspiracional).
- **Runtime-juego (~4.186 líneas + glue CPython del Player):** lo único con retorno doctrinal inmediato. El único módulo Python que el Player exportado **sí** carga es `bge_extras.logger` (`KX_PythonInit.cpp:2059`, `PyImport_ImportModule("bge_extras.logger")`).

**La honestidad de fondo:** convertir 293k líneas es **plurianual y en su mayoría nunca compensa**. El trabajo con retorno real no es "migrar el editor" sino **quitar CPython del Player** (~4k líneas Python + purga transversal de `#ifdef WITH_PYTHON` en el gameengine). El resto del editor Python se **queda en Python** o se **borra al recortar el editor**; no se reescribe a C++ salvo decisión estratégica de años.

**Framework nativo ya disponible (no partimos de cero):**
- Operadores nativos: **275 ficheros** en `source/blender/editors` definen `wmOperatorType` (mismo namespace `bpy.ops.xxx.yyy` que los Python → migrar uno mantiene idénticos los call-sites).
- Layout + RNA en C: `source/blender/editors/interface`, `makesrna`.
- Componentes nativos: `source/gameengine/Flipendo/FL_Component`, ya tickeando (`KX_Scene.cpp:2449`).
- `WITH_PYTHON=OFF` es config **diseñada** (`CMakeLists.txt:230`, ON por defecto): hoy **compila**.

---

## 2. Lotes ordenados y ejecutables

### LOTE 1 — Semanas: runtime del juego, pequeño y aislado

**Objetivo: acercar el Player a "sin CPython".** Todo lo pequeño, aislado y con reemplazo nativo ya existente.

| # | Módulo | Ruta | Líneas | Acción |
|---|---|---|---:|---|
| 1.1 | **KX_PythonComponent** | `source/gameengine/Ketsji/KX_PythonComponent.cpp/.hpp` | 203 (+46 refs) | **ELIMINAR.** `FL_Component` ya lo cubre y tickea. |
| 1.2 | **bge_extras/logger** | `scripts/modules/bge_extras/` | 158 | Reemplazar por logger nativo (`clog` ya en C++); quitar `PyImport_ImportModule` de `KX_PythonInit.cpp:2059`. |
| 1.3 | **wm.py — familia `WM_OT_context_*`** | `scripts/startup/bl_operators/wm.py` (18 clases) | ~900 | Glue RNA puro (get/set/toggle sobre data-path). Cada uno = `wmOperatorType` con `exec()` de 10-40 líneas. Mismo `idname`. |
| 1.4 | Operadores mate simples | `object_align.py` (407), `object_randomize_transform.py` (181), `add_mesh_torus.py` (262) | ~850 | Álgebra/geometría pura sobre BMesh + `BLI_math`. Buen ejercicio de patrón de migración. |

**Por qué Lote 1:** 1.1 y 1.2 tienen reemplazo nativo YA existente y viven en paralelo redundantes. 1.3 es el candidato ideal de conversión (patrón repetido en cientos de ops nativos, riesgo bajo). 1.4 valida el patrón "operador Python → `exec()` C++" sin dependencias externas.

---

### LOTE 2 — Meses: operadores y módulos base migrables a C++ nativo

Operadores con lógica acotada sobre APIs C ya disponibles (BMesh, depsgraph, constraints, rigidbody-setup) + helpers geométricos.

| Grupo | Rutas | Líneas | Notas |
|---|---|---:|---|
| **SCA_PythonController** | `source/gameengine/GameLogic/SCA_PythonController.cpp/.hpp` | 620 | Reemplazar por bricks nativos / FL_Component. **Riesgo: fallo silencioso** (`Trigger()` no-op en rama `#else`, línea 503) → auditar `.blend` del proyecto antes de retirar. |
| **templates_py_components (útiles)** | `scripts/templates_py_components/` (character controllers, vehículos, chaser AI) | ~1.637 | Reescribir como `FL_Component` nativos (patrón: `FL_ArpgComponents.cpp`). `python_component.py`/`bpytypes.py` se descartan al caer KX_PythonComponent. |
| **wm.py — utilidades sistema** | url/path/doc/owner/tool, `blenderplayer_start` | ~1.300 | `url_open`/`path_open` ya tienen equivalente C. `blenderplayer_start` es Flipendo → versión C++ propia. |
| **Ops malla/UV/físico** | `mesh.py` (250), `vertexpaint_dirt.py` (198), `constraint.py` (121), `rigidbody.py` (316), `uvcalc_*`, `uvcalc_lightmap.py` (688) | ~2.900 | `exec()` C++ sobre BMesh/constraints/rigidbody-world del editor. |
| **bpy_extras (subconjunto mate)** | `view3d_utils.py` (181), `mesh_utils.py` (464), `object_utils.py` (289) | ~934 | Pura mate/geometría → helpers C++ llamables desde operadores nativos. `io_utils`/`node_shader_utils` NO (sirven a addons Python). |

**Magnitud honesta:** convertir esta fracción "core" es esfuerzo de **varios trimestres**. Es la última capa con retorno técnico razonable.

---

### LOTE 3 / HORIZONTE — Años: el grueso del editor (UI, operadores pesados, addons grandes)

**Gated por una decisión estratégica: "reescribir el editor entero en C++".** Volumen enorme, complejidad por fichero baja-media (declarativo), valor de juego nulo. No se planifica hasta que exista esa decisión.

- **Toda `bl_ui` (64.452 líneas):** cada `Panel`/`Menu`/`Header` → `PanelType`/`MenuType`/`HeaderType` nativo con callback `draw()` C++ sobre `uiLayout`+RNA. Los mayores: `space_view3d.py` (9.381), pestañas del Properties (~24.000), editores de espacio (~20.300), toolsystem (~5.000 — la pieza más "lógica", registro de herramienta activa).
- **Piezas Flipendo aisladas dentro de `bl_ui`** (candidatas a piloto si se acomete el editor): `space_logic.py` (145) y `properties_game.py` (899) — dominio propio, pero siguen dependiendo de RNA `ob.game` y operadores `object.game_property_new/remove` nativos.
- **Operadores pesados:** `presets.py` (1.015, metaprogramación: genera `.py`), `userpref.py` (1.307, gestión de paquetes CPython), `node.py`/`geometry_nodes.py` (~1.590), `wm.py` batch_rename/properties_edit (~1.550, modal + introspección RNA).
- **bpy_types.py (1.487):** métodos Python sobre tipos RNA; cirugía en el corazón del binding.

**No prometemos fecha.** Es plurianual y buena parte podría nunca ejecutarse.

---

### NO-MIGRAR — Autoría/datos, o `bpy`/CPython obliga a Python (con motivo)

| Qué | Líneas | Motivo |
|---|---:|---|
| **Capa glue `bpy/`** (`__init__`, `ops.py`, `utils`, `bpy_types`) | ~2.247 | **ES** el puente CPython↔C (`from _bpy import ...`). No se traduce: se **borra** el día que el editor no ejecute Python. Intocable mientras haya editor Python. |
| **KX_PythonInit + KX_PythonInitTypes** | ~3.060 C++ | Corazón del CPython embebido del Player (crea `bge.logic/render/types`). No se "migra": **desaparece** cuando el Player sea python-free. |
| **`rigify`** | 46.480 | Autoría de rig, genera armaduras en editor. Reescribir a C++ no aporta al runtime. |
| **`io_scene_gltf2`** | 30.333 | Sin equivalente C++ (verificado: 0 en `source/blender/io/`). El juego usa `.blend`, no glTF en runtime. |
| **`bge_mixer`** (colaborativo), `bl_pkg` (extensiones) | 21.870 + 19.687 | Infraestructura de editor. El Player no tiene extensiones ni edición en red. |
| **`io_scene_fbx`** | 11.943 | Import ya cubierto en C++ (`source/blender/io/fbx`, solo importer); export FBX no. Dejar como está. |
| **`rna_manual_reference.py`** | 4.272 | Tupla autogenerada (RNA-path → URL docs.blender). Ni es código. Candidato a **borrar**. |
| **`bl_i18n_utils`** | 5.254 | Tooling offline de traducción `.po`. Ni se carga en el editor en ejecución. |
| **`addon_utils` + `_bpy_internal`** | ~4.800 | Descubrimiento/activación de addons (wheels pip, junctions). CPython por definición mientras los addons sean Python. |
| **Consola Python** (`console_python`, `bge/interpreter.py`) | ~1.300 | Un REPL de Python "en C++" es una contradicción. Se jubila con el intérprete. |
| **`rna_info`, `blend_render_info`, `graphviz_export`, `gpu_extras`** | ~1.500 | Tooling de build/docs y atajos de dibujo GPU para addons. Offline o azúcar. |
| **VFX/import irrelevantes** (`clip.py`, `sequencer.py`, `freestyle.py`, `image_as_planes.py`…) | ~4.750 | Video/VSE/NPR/importadores. Nada que necesite un motor de juego. Dejar o borrar. |
| **4 dirs `bge_*` vacíos** (bricknodes, easyonline, netlogic, thelightmapper) | 0 | Nada que migrar. **Limpieza:** eliminar directorios. |

---

## 3. PRIMER OBJETIVO CONCRETO — para ejecutar YA

### Retirar `KX_PythonComponent` (el reemplazo nativo ya existe y ya tickea)

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

### Alcanzable a medio plazo: **"Player sin CPython"**

- El andamiaje **ya existe**: `EXP_PyObjectPlus.hpp:871` colapsa `Py_Header` a `public:` vacío en la rama `#else`; `SCA_PythonController.cpp:503` tiene `Trigger()` no-op; `WITH_PYTHON=OFF` está diseñado (`CMakeLists.txt:230`) y **hoy compila**.
- Camino: cerrar los 3 huecos que hoy caen en silencio con `WITH_PYTHON=OFF` → (a) bricks Python (`SCA_PythonController`), (b) componentes de usuario (**ya resuelto por FL_Component**), (c) UI `bgui`. Es decir: retirar `KX_PythonComponent` (Lote 1) + portar los `templates_py_components` usados a FL_Component + auditar cero-bricks-Python (Lote 2).
- **Caveat técnico real:** `WITH_PYTHON` es **global**, no del Player (`CMakeLists.txt`, además fuerza `WITH_CYCLES OFF` en :1374). Un Player sin CPython **con editor Python** exige un **target/flag nuevo Player-only** — trabajo de build system, no de módulo. Es el bloqueante estructural a resolver.

### Horizonte / aspiracional: **"editor sin Python"**

- Requiere reescribir ~288.500 líneas de UI/operadores/addons como `PanelType`/`MenuType`/`wmOperatorType` nativos, más purgar la superficie de binding: **511 guardas `#ifdef WITH_PYTHON`**, **88 clases con `Py_Header`**, **200 ficheros con `PyObject`** a lo largo de las 112.039 líneas del gameengine, más los 7.134 de `Expressions`.
- Es **plurianual**, transversal (no modular), y buena parte **nunca compensa** (se borra al recortar el editor, o se queda en Python indefinidamente).
- **No se promete.** Se marca como dirección, no como plan con fecha.

---

**Resumen de una línea:** el retorno real está en las **~4.000 líneas de runtime + la purga de CPython del Player** (medio plazo, empezando por eliminar `KX_PythonComponent` esta semana); las **~288k del editor** son horizonte condicionado a una decisión estratégica que hoy no tiene fecha ni obligación doctrinal.