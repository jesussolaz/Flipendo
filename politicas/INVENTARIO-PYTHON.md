# Inventario del Python que queda — destino de los 1.031 ficheros

> Generado midiendo el árbol el 2026-09-11 sobre `flipendo-main` @ `4e88d96a431`.
> No hereda conclusiones de `MIGRACION-CPP.md` ni de `BACKLOG-EDITOR-PYTHON.md`: donde
> discrepa de ellos, manda este documento y se dice por qué. Ambos están desactualizados
> en puntos concretos y se señalan uno a uno.

## 0. La foto, medida

```
git ls-files '*.py' | wc -l                     -> 1031
git ls-files '*.py' | xargs wc -l | tail -1     -> 249382
```

1.031 ficheros, **249.382 líneas**. Coincide con `dev/noche/foto-inicial.txt` al dígito.

Para dimensionar lo que hay al otro lado: **2.025 clases Python registradas** en el
editor (1.042 `_PT_`, 571 `_MT_`, 22 `_HT_`, 37 `_UL_`, 167 `_OT_` por convención de
nombre; 274 clases derivan de `Operator` y declaran 272 `bl_idname` distintos).
`FL_ui_registry.hh` ya cifra el objetivo en «1.044 paneles, 565 menús y 23 cabeceras»:
la medición de hoy lo confirma.

---

## 1. Cómo se comprobó qué carga el binario en ejecución

«Que se instale» no es «que se cargue». Las dos cosas se comprobaron por separado.

**Se instala** (regla de CMake): `source/creator/CMakeLists.txt:463-489` copia el árbol
`scripts/` **entero** dentro de `if(WITH_PYTHON)`, excluyendo solo `.git*`,
`__pycache__`, `site/`, `freestyle/*` si `WITH_FREESTYLE=OFF`, y los ficheros de
desarrollo de `bl_pkg`. Es decir: **todo lo que hay bajo `scripts/` viaja en el bundle**.
Nada de `tests/`, `tools/`, `doc/`, `build_files/` ni `release/` viaja.

**Se carga** (cuatro caminos, todos verificados en el árbol):

| Camino | Evidencia | Qué arrastra |
|---|---|---|
| Registro de arranque | `bpy/utils/__init__.py:231-339` (`load_scripts`, `_script_module_dirs = "startup", "modules"`), disparado desde `wm_init_exit.cc:510` (`BPY_run_string_eval(..., "bpy.utils.load_scripts_extensions()")`) | `scripts/startup/**` entero + lo que importe |
| Import bajo demanda | `scripts/modules/` está en `sys.path` | `scripts/modules/**` |
| `addon_utils` | `scripts/modules/addon_utils.py:65`; los activados por defecto están en `blendfile.cc:1509-1517` | `scripts/addons_core/**` y `intern/cycles/blender/addon/` |
| Ejecución de fichero | `script.execute_preset` → `bpy.utils.execfile` (`presets.py:291`); `keyconfig_set` → `execfile` (`bpy/utils/__init__.py:852`) | `scripts/presets/**` |

**Trampa encontrada en `blendfile.cc:1509`:** la lista de addons activados de fábrica
todavía nombra `io_anim_bvh`, `io_curve_svg`, `io_mesh_uv_layout`, `io_scene_fbx`,
`io_scene_gltf2` y `pose_library`, **que ya no existen en el árbol** (los quitó la poda
Mac-only del 2026-09-05). De los ocho, solo `cycles` y `bl_pkg` existen. Son seis
entradas muertas en el `UserDef` por defecto; no rompen nada, pero mienten. No las toco
porque `blendfile.cc` no es de mi carril: queda anotado aquí.

**Trampa 2:** `BACKLOG-EDITOR-PYTHON.md` describe `addons_core` como 151.293 líneas con
rigify (46.480), glTF2 (30.333), bge_mixer (21.870) y FBX (11.943). **Nada de eso existe
ya.** Hoy `scripts/addons_core` son **22 ficheros y 24.330 líneas**: `bl_pkg` (19.687),
cinco addons `game_engine_*` de UPBGE (2.918), `copy_global_transform` (1.261),
`normal_map_to_group` (464) y cuatro submódulos `bge_*` **sin inicializar (0 líneas)**.

**Cómo se midieron los puentes C++ → Python.** Se extrajeron del C++ todos los literales
con forma de identificador (`^"[A-Z]\w*_(OT|MT|PT|HT|UL)_\w+"$`, 2.372 distintos) y todos
los `bl_idname` con punto de Python (272), y se cruzaron contra (a) los identificadores
que el C++ **define** (2.219) y (b) las clases que Python **registra de verdad** (2.025,
extraídas de las tuplas `classes = (...)` y de las llamadas a `register_class`). El
cruce da **170 tipos de UI (145 menús, 24 paneles y 1 lista) y 15 operadores** —185 identificadores— que el C++ nombra por cadena y que hoy solo
existen como clase Python. El filtro por «registrada de verdad» importa: descarta los
cuatro `*_PT_tools_active`, cuyas clases Python siguen en el fichero pero **ya no se
registran** (los pinta `fl_toolbar_ui.cc:818-855`).

---

## 2. La tabla: destino de cada fichero

Leyenda de destino: **MIG** = MIGRAR-A-C++ · **DAT** = CONVERTIR-A-DATOS ·
**RET** = RETIRAR · **EXT** = EXTERNAL.
Esfuerzo en **h·p** (horas-persona-agente: una hora de un trabajador, humano o agente).
La base de cálculo está en §6.

### 2.1 `scripts/startup/bl_ui` — la interfaz del editor (77 ficheros, 64.160)

Todo se carga en el arranque (`bl_ui/__init__.py:13-104` importa los 77 módulos y
`register()` los registra). Es la mayor bolsa de Python viva del árbol.

| Ruta | Fich | Líneas | Qué hace | ¿Lo carga el binario? | Destino | h·p | Dependencias | Riesgo |
|---|---:|---:|---|---|---|---:|---|---|
| `space_view3d.py` | 1 | 9.381 | Menús y cabecera de la vista 3D. Solo aquí viven **65 de los 129 menús que el keymap nativo invoca por nombre** | Sí, registro de arranque | **MIG** `MenuType` vía `FL_ui_registry::menus_register` | 65–95 | `FL_ui_registry` (existe), `--fl-dump-ui` (carril D, sin commitear) | **Alto**: si falta un menú, la tecla del keymap nativo no abre nada y no hay error |
| `space_view3d_toolbar.py` | 1 | 3.090 | Paneles de la región T (ajustes de pincel, opciones de modo) | Sí | **MIG** `PanelType` + `FL_tool_settings_ui` | 22–33 | Capa `FL_tool_settings_ui.hh` (existe) | Medio |
| `space_sequencer.py`, `space_clip.py`, `space_image.py`, `space_node.py`, `space_userpref.py`, `space_dopesheet.py`, `space_filebrowser.py`, `space_topbar.py`, `space_graph.py`, `space_outliner.py`, `space_text.py`, `space_nla.py`, `space_time.py`, `space_properties.py`, `space_console.py`, `space_info.py`, `space_spreadsheet.py`, `space_statusbar.py` | 18 | 17.245 | Cabecera, menús y paneles de cada editor | Sí | **MIG** `HeaderType`/`MenuType`/`PanelType` | 120–180 | Igual | Medio-alto (`space_topbar.py` tiene 8 puentes vivos, entre ellos `TOPBAR_PT_tool_settings_extra` y `TOPBAR_PT_tool_fallback`, que ya llama el C++ del sistema de herramientas) |
| `properties_*.py` (45: `properties_particle` 2.312, `properties_paint_common` 1.991, `properties_constraint` 1.900, `properties_physics_fluid` 1.629, `properties_freestyle` 1.348, `properties_render` 1.230, `properties_texture` 1.026, `properties_physics_dynamicpaint` 981, `properties_game` 899, `properties_grease_pencil_common` 821, `properties_data_mesh` 755, `properties_output` 715, `properties_object` 659, `properties_data_camera` 647, `properties_data_bone` 618, y 30 más) | 45 | 26.717 | Las pestañas del editor de Propiedades. Dibujo declarativo sobre RNA | Sí | **MIG** `PanelType` (`properties_game.py` es de Flipendo → carril D) | 180–270 | `FL_ui_registry`, `--fl-check-ui` | Medio. `properties_freestyle.py` (1.348) cae con Freestyle (§2.9) |
| `node_add_menu*.py` (5) | 5 | 2.133 | Menú «Añadir» de los editores de nodos. Tablas puras | Sí | **MIG** `MenuType`, muy mecánico | 15–25 | — | Bajo |
| `__init__.py`, `utils.py`, `anim.py`, `asset_shelf.py`, `generic_ui_list.py` | 5 | 729 | Registro del paquete y ayudantes de dibujo | Sí | **MIG** (el `__init__` desaparece; los ayudantes pasan a `uiLayout`) | 6–10 | Los 75 anteriores | Bajo |
| `space_toolsystem_common.py`, `space_toolsystem_toolbar.py` | 2 | 4.865 | Armazón y catálogo de 416 herramientas | **Sí, pero ya no como paneles**: solo su `register()` convierte `keymap=()` en nombres, disparado por un menú invisible (`WM_MT_toolsystem_catalog_keymaps`) | **RET** — el sustituto C++ existe y está verificado: catálogo 416/416, keymaps de barra 912/912, barra 14/14 capturas idénticas, cabeceras 819 capturas | 4–8 (solo desenganchar lectores) | Que caigan sus lectores Python: `space_topbar`, `space_view3d`, `space_image`, `space_sequencer`, `space_node`, `properties_paint_common`, `space_view3d_toolbar`, `bl_keymap_utils/keymap_hierarchy`, `bpy/utils` | Bajo (ya demostrado idéntico) |

**Trampa de `bl_ui`, ya pagada una vez:** la lección de la fase 4a del sistema de
herramientas es que el Python pasa `text=""` a los botones de solo icono, y que en C++
«sin texto» no es lo mismo que «texto vacío» (`op()` pone el nombre del operador). Y que
las columnas de la barra son corrutinas `yield`/`send`, no bucles. Cualquier
transliteración de `bl_ui` que no reproduzca esos detalles falla al píxel.

### 2.2 `scripts/startup/bl_operators` — los operadores (36 ficheros, 18.427)

Todos se cargan en el arranque. **Es el bloque con más lógica real por línea de todo el
inventario.** Carriles B y C están dentro ahora mismo.

| Ruta | Líneas | Qué hace | Carga | Destino | h·p | Dependencias | Riesgo |
|---|---:|---|---|---|---:|---|---|
| `wm.py` | 0 (**migrado y eliminado por carril B**) | Operadores de contexto, `batch_rename`, `properties_edit*`, documentación/sistema/propietario y los 4 menús globales viven en C++ | No | **HECHO** `wmOperatorType` + `MenuType` | — | `wm_context_ops.cc`, `wm_property_ops.cc`, `wm_system_ops.cc`, `wm_utility_ops.cc`, `wm_batch_rename.cc`, `wm_splash_screen.cc` | Contratos de operadores y registro UI volcados; expresiones cerradas con `BLI_expr_pylike` y proveedores nativos de URL |
| `userpref.py` | 1.307 | Instalar/activar addons, importar y exportar keymaps, gestión de paquetes CPython | Sí | **RET** la parte de addons y `pip` (cae con el intérprete); **MIG** exportar/importar keymap y activar keyconfig | 25–40 | `bl_keymap_utils/io.py` migrado | Medio: se pierde exportar el keymap si no se migra |
| `image_as_planes.py` | 1.230 | Importar imágenes como planos con material | Sí | **MIG** `wmOperatorType` sobre BKE_image + nodos | 25–40 | — | Bajo |
| `object.py` | 1.211 | Operadores de objeto (transferencia de datos, ids, etc.) | Sí | **MIG**. Carril C | 25–40 | — | Medio |
| `clip.py` | 1.098 | Ayudantes del editor de clips (seguimiento) | Sí | **MIG**, o **RET** si se retira el seguimiento de cámara | 20–35 | Decisión sobre el editor de clips | Bajo (no es capacidad de juego) |
| `presets.py` | 1.015 | 22 operadores `AddPreset*` + `ExecutePreset` + `WM_PT_operator_presets`. **Metaprogramación: genera ficheros `.py`** | Sí. `interface_template_operator_property.cc:102` busca `WM_OT_operator_preset_add` | **MIG** sobre el formato `.fpreset` (carril A, `windowmanager/preset/`, en curso) | 30–45 | `FL_preset.hpp` + `fl_preset_file.cc` | **Alto**: es el único camino por el que el editor **escribe** código Python en disco. Mientras exista, «cero Python» es falso por construcción |
| `anim.py` | 817 | Keying sets, copiar/pegar animación, borrar drivers | Sí | **MIG** | 16–27 | `keyingsets_utils.py` | Medio |
| `node.py`, `geometry_nodes.py`, `connect_to_output.py`, `node_editor/node_functions.py` | 1.591 | Operadores de nodos. `NODE_OT_connect_to_output` y `NODE_OT_viewer_shortcut_*` **están en el keymap nativo** | Sí | **MIG** | 32–53 | — | **Alto**: tres atajos del keymap C++ apuntan aquí |
| `uvcalc_lightmap.py`, `uvcalc_transform.py`, `uvcalc_follow_active.py` | 1.484 | Empaquetado de lightmaps y desplegado UV. Geometría pura | Sí | **MIG** sobre BMesh | 30–50 | — | Bajo |
| `object_quick_effects.py` | 676 | Humo/fuego/fluido/explosión rápidos | Sí | **MIG**, o **RET** con `WITH_MOD_FLUID` (que `WITH_PYTHON=OFF` ya apaga) | 14–23 | Decisión sobre Mantaflow | Bajo |
| `bone_selection_sets.py`, `sequencer.py`, `object_align.py`, `bmesh/find_adjacent.py`, `image.py`, `view3d.py`, `rigidbody.py`, `file.py`, `add_mesh_torus.py`, `mesh.py`, `screen_play_rendered_anim.py`, `vertexpaint_dirt.py`, `object_randomize_transform.py`, `world.py`, `assets.py`, `constraint.py`, `grease_pencil.py`, `spreadsheet.py`, `__init__.py` | 4.604 | Operadores acotados sobre APIs C++ ya disponibles | Sí. `MESH_OT_select_next_item/prev_item`, `OBJECT_OT_select_hierarchy`, `OBJECT_OT_subdivision_set`, `OBJECT_OT_add_modifier_menu`, `RENDER_OT_play_rendered_anim`, `SEQUENCER_OT_split_multicam`, `ARMATURE_OT_collection_show_all`, `SPREADSHEET_OT_toggle_pin` **están en el keymap nativo** | **MIG**. Carriles B y C (`object_align.cc` y `object_randomize_transform.cc` ya están en el árbol de trabajo) | 85–140 | — | **Alto** para los ocho del keymap |
| `freestyle.py` | 223 | Operadores del editor de líneas de Freestyle | Sí | **RET** con Freestyle (§2.9) | 0 | — | Se pierde Freestyle |
| `console.py` | 156 | `CONSOLE_OT_copy_as_script`, autocompletado. **`CONSOLE_OT_execute` y `copy_as_script` están en el keymap nativo** | Sí | **RET** con la consola de Python | 0 | — | Se pierde la consola |

### 2.3 `scripts/startup` — sueltos y plantillas de aplicación (7 ficheros, 903)

| Ruta | Fich | Líneas | Qué hace | Carga | Destino | h·p | Dependencias | Riesgo |
|---|---:|---:|---|---|---|---:|---|---|
| `keyingsets_builtins.py` | 1 | 686 | Los keying sets de fábrica. Su propio encabezado avisa: «*None of these Keying Sets should be removed, as these are needed by various parts of Blender*», y que **cambiar su orden rompe ficheros antiguos** | Sí | **MIG** servicio nativo sobre `KeyingSetInfo` (`editors/animation/keyingsets.cc` ya los busca por nombre) | 14–22 | `keyingsets_utils.py` | **Alto**: el orden es parte del formato `.blend` |
| `bl_app_templates_system/{2D_Animation,Game_Engine,Sculpting,Video_Editing}/__init__.py` | 4 | 163 | Manejadores `load_factory_startup_post` de cada plantilla (p. ej. Game_Engine pone el sombreado en `RENDERED`) | Sí, al elegir plantilla | **MIG** manejador nativo en `wm_files.cc` (el `.blend` de cada plantilla es dato y se queda) | 4–8 | — | Bajo |
| `nodeitems_builtins.py` | 1 | 54 | API antigua de «añadir nodo», sustituida por `node_add_menu_*` | Sí | **RET** (ya vacía de contenido real) | 0 | — | Ninguno |

### 2.4 `scripts/modules` — módulos importables (99 ficheros, 30.788)

| Ruta | Fich | Líneas | Qué hace | Carga | Destino | h·p | Dependencias | Riesgo |
|---|---:|---:|---|---|---|---:|---|---|
| `bpy/` (`__init__` 75, `path` 459, `ops` 184, `utils/__init__` 1.382, `utils/previews` 136, `utils/toolsystem` 11), `bpy_types.py` 1.487, `bpy_restrict_state.py` 50 | 8 | 3.784 | **Es** el puente CPython↔C (`from _bpy import ...`). No implementa capacidad: la expone | Sí, siempre | **RET** — no se traduce: desaparece el día que el editor no ejecute Python | 0 | Todo lo demás | Ninguno propio; es el último en caer |
| `addon_utils.py` 1.992, `_bpy_internal/addons/` 45, `_bpy_internal/extensions/` 1.341 | 9 | 3.378 | Descubrimiento y activación de addons, ruedas `pip`, uniones de directorio | Sí | **RET** — sin addons Python no hay nada que descubrir | 0 | Que caiga `bl_pkg` y los addons | Se pierde instalar addons de terceros. **Es puerta de un solo sentido** |
| `bl_i18n_utils/` | 10 | 5.254 | Herramienta *offline* de extracción y fusión de `.po` | **No**: nada la importa en ejecución | **RET** — utillaje de traducción, no capacidad | 0 | Que la traducción se extraiga del C++ (`N_()`/`IFACE_()`/`TIP_()`) | Bajo. Habrá que reponer el extractor, en C++, cuando los textos ya no estén en Python |
| `rna_manual_reference.py` | 1 | 4.272 | Tupla autogenerada: patrón RNA → URL del manual. Ni es código | Sí, vía `bpy/utils/__init__.py:1234` (`execfile`) | **DAT** — tabla binaria o texto plano leída por el `WM_OT_doc_view_manual` nativo | 6–10 | Migrar `wm.doc_view_manual` (carril B) | Bajo |
| `bgui/` | 15 | 2.391 | **Biblioteca de interfaz del juego** (widgets, cajas de texto, listas) | Sí, si el juego la importa | **MIG** — es capacidad del Player. Hoy `PLAYER-SIN-CPYTHON.md §3.3` la da por perdida: un juego sin CPython **no tiene sistema de interfaz de serie** | 45–70 | Servicio nativo de UI de juego sobre `RAS_2DFilter`/`BLF` | **Alto**: es un agujero abierto del Player de distribución, no una deuda de editor |
| `bpy_extras/` | 14 | 3.916 | `node_shader_utils` 847, `anim_utils` 757, `io_utils` 616, `mesh_utils` 464, `object_utils` 289, `image_utils` 194, `view3d_utils` 181, `progress_report` 160, `keyconfig_utils` 141, y 5 menores | Sí | **MIG** los de mate/geometría (`mesh_utils`, `object_utils`, `view3d_utils`, `bmesh_utils`, `id_map_utils`, `anim_utils`: ~1.800) como ayudantes C++; **RET** los que solo sirven a addons Python (`io_utils`, `node_shader_utils`, `image_utils`, `progress_report`, `asset_utils`, `node_utils`: ~2.116) | 30–50 | Que caigan los addons | Medio: `anim_utils` lo usa `anim.py` |
| `bl_keymap_utils/` (`io` 308, `keymap_hierarchy` 243, `versioning` 209, `platform_helpers` 55, `__init__` 9) | 5 | 824 | Importar/exportar keyconfigs, jerarquía de keymaps para el editor de teclas, versionado de configuraciones antiguas | Sí | **MIG** servicio nativo (el `platform_helpers` ya está transliterado dentro de `fl_keymap_build.cc:192`) | 16–26 | `wm_keymap.cc` | Medio: sin esto no se importa un keymap guardado |
| `bl_keymap_utils/keymap_from_toolbar.py` | 1 | 394 | Genera el keymap emergente de la barra | **Ya no**: solo lo llama el arnés `tests/flipendo/toolsystem/dump_toolbar_keymaps_gui.py` | **RET** — sustituto `fl_toolbar_keymap.cc` verificado **912/912** | 0 | Retirar el arnés | Ninguno |
| `rna_keymap_ui.py` 504, `rna_prop_ui.py` 269, `bl_rna_utils/` 76, `bl_ui_utils/` 21 | 6 | 870 | Dibujo del editor de keymaps, de las propiedades personalizadas, y ayudantes de layout | Sí | **MIG** `PanelType`/`uiLayout` nativo | 17–28 | `bl_keymap_utils` | Medio |
| `bl_previews_utils/bl_previews_render.py` | 1 | 536 | Genera previsualizaciones en lote lanzando un Blender hijo | Sí, desde `wm.previews_batch_generate` | **MIG** — `WM_OT_previews_ensure`/`_clear` **ya son C++** (`wm_operators.cc:3907,4042`); falta el modo lote | 10–18 | — | Bajo |
| `keyingsets_utils.py` | 1 | 296 | Funciones de sondeo/generación que usan los keying sets de fábrica | Sí | **MIG** con `keyingsets_builtins.py` | 6–10 | — | Alto (ver 2.3) |
| `_bpy_internal/grease_pencil/` 405, `_bpy_internal/system_info/` 423, `_bpy_internal/__init__` 3 | 6 | 831 | Operaciones de trazo de lápiz de cera; informe de sistema de `wm.sysinfo` | Sí | **MIG** | 17–28 | — | Bajo |
| `bl_text_utils/external_editor.py` | 2 | 54 | Abrir un texto en el editor externo | Sí | **MIG** (trivial) | 1–2 | — | Ninguno |
| `console_python.py` 368, `console_shell.py` 73, `bl_console_utils/` 698 | 8 | 1.139 | La consola interactiva de Python del editor y su autocompletado | Sí | **RET** — un REPL de Python «en C++» es una contradicción; se jubila con el intérprete | 0 | — | Se pierde la consola. `CONSOLE_OT_execute` y `copy_as_script` **están en el keymap nativo**: hay que quitar esos atajos a la vez |
| `rna_info.py` 952, `rna_xml.py` 422, `animsys_refactor.py` 225, `graphviz_export.py` 194, `nodeitems_utils.py` 178, `bl_app_override/` 364, `bl_app_template_utils.py` 177, `blend_render_info.py` 147, `gpu_extras/` 190 | 12 | 2.849 | Introspección de RNA para la documentación, serializado XML de presets, refactor de rutas de animación, exportación a graphviz, atajos de dibujo GPU para addons | Sí (importables), pero ninguno es capacidad de usuario | **RET** — utillaje. `rna_xml` cae con los presets XML de tema (que ya son XML, leídos como datos); `bl_app_override` solo lo usan las plantillas de aplicación | 0 | Formato `.fpreset` para el tema | Bajo. `blend_render_info.py` lo cita `writefile.cc:986` en un comentario: reponer como opción `--fl-blend-info` en C++ si se quiere conservar |

### 2.5 `scripts/presets` — 173 ficheros, 10.287

| Ruta | Fich | Líneas | Qué hace | Carga | Destino | h·p | Dependencias | Riesgo |
|---|---:|---:|---|---|---|---:|---|---|
| `presets/**` menos `keyconfig` | 171 | 1.232 | Presets de cámara, nube de puntos, ffmpeg, render, cycles, eevee, fotogramas, seguimiento… **166 de 171 solo asignan propiedades**. Los otros 5 (ffmpeg) tienen un `if is_ntsc:` | Sí, ejecutados como Python por `script.execute_preset` | **DAT** — formato `.fpreset` (`windowmanager/preset/FL_preset.hpp`, `FORMAT_VERSION = 1`), leído por `fl_preset_file.cc`. **Carril A lo está escribiendo esta noche**, con lector nativo del `.py` heredado incluido | 25–45 | Migrar `presets.py` (§2.2) | **Medio, y aquí está la trampa**: los 5 de ffmpeg **no son datos puros**. Un formato que solo asigne pierde el condicional NTSC/PAL. Salida elegida: dos entradas de datos por preset (NTSC y PAL) resueltas por `render.fps`, no un mini-lenguaje en el formato |
| `presets/keyconfig/Blender.py` + `keymap_data/blender_default.py` | 2 | 9.055 | El keymap por defecto entero | **Ya no en el arranque**: `WM_keyconfig_reload` (`wm.cc:424-450`) llama a `flipendo::keymap::register_default`. `bpy.utils.keyconfig_init()` **no tiene ningún llamador vivo**. Solo se ejecutaría al elegir un preset de keymap en Preferencias | **RET** — sustituto verificado **3.673/3.673 atajos, 248/248 keymaps, cero diferencias** | 2–4 | Quitar el desplegable de presets de keymap, que solo tiene esta entrada | Bajo. Se pierde «cambiar de preset de keymap», que hoy es un desplegable de un solo elemento |

### 2.6 `scripts/addons_core` — 22 ficheros, 24.330

| Ruta | Fich | Líneas | Qué hace | Carga | Destino | h·p | Dependencias | Riesgo |
|---|---:|---:|---|---|---|---:|---|---|
| `bl_pkg/` | 15 | 19.687 | El gestor de extensiones: cliente de repositorios, ruedas `pip`, interfaz, CLI (`blender_ext.py` 5.688) y sus propios tests (2.681) | Sí, **activado de fábrica** (`blendfile.cc:1516`). `interface_template_status.cc:425,450,474` invoca `EXTENSIONS_OT_userpref_show_online` y `_show_for_update` | **RET** — no es capacidad de Flipendo: es el mercado de addons de Python de Blender. Sin intérprete no hay extensión que instalar | 3–6 (quitar los dos puentes de `interface_template_status.cc` y la entrada de `blendfile.cc`) | — | **Puerta de un solo sentido.** Se pierde instalar extensiones de terceros, para siempre. Se justifica: la meta declarada es cero Python, y una extensión de Blender **es** Python |
| `game_engine_publishing.py` 576, `game_engine_save_as_runtime_eevee.py` 416, `game_engine_spring_bones.py` 1.301, `game_engine_add_basic_character.py` 338, `game_engine_object_camera_vertex_cull.py` 287 | 5 | 2.918 | **Capacidad de juego de UPBGE/Flipendo**: publicar el juego, guardar como ejecutable, huesos con muelle, personaje básico, descarte de vértices por cámara | Solo si el usuario los activa | **MIG** — `wmOperatorType` + `FL_Component` para los huesos con muelle. Es lo único de `addons_core` que toca el motor | 55–90 | `FL_Component` (existe) | **Alto**: «guardar como ejecutable» es la ruta de publicación de ÁNIMA. Si se retira sin sustituto, no hay cómo empaquetar el juego |
| `copy_global_transform.py` 1.261, `normal_map_to_group.py` 464 | 2 | 1.725 | Copiar/pegar transformación en mundo con «fijar al hueso»; convertir un mapa de normales en grupo de nodos | Solo si se activan | **MIG** `wmOperatorType` | 30–50 | — | Bajo |
| `bge_bricknodes/`, `bge_easyonline/`, `bge_netlogic/`, `bge_thelightmapper/` | 0 | 0 | Submódulos git **sin inicializar**: cero ficheros, cero líneas | No | **RET** — borrar los cuatro directorios y sus entradas de `.gitmodules` | 0,5 | — | Ninguno |

### 2.7 `intern/cycles/blender/addon` — 9 ficheros, 6.109

| Ruta | Líneas | Qué hace | Carga | Destino | h·p | Dependencias | Riesgo |
|---|---:|---|---|---|---:|---|---|
| `ui.py` 2.646, `properties.py` 1.971, `osl.py` 341, `version_update.py` 323, `engine.py` 267, `__init__.py` 174, `operators.py` 172, `presets.py` 137, `camera.py` 78 | 6.109 | Registra `CyclesRender` (`bl_idname = 'CYCLES'`), define **todas** las propiedades RNA de Cycles y **todos** sus paneles | Sí, **activado de fábrica** (`blendfile.cc:1514`) | **MIG** — `RE_engines_register` (`render/intern/engine.cc:84`) ya permite registrar un motor desde C++; las propiedades pasan a `makesrna` y los paneles a `PanelType` | 55–90 | `makesrna`, `FL_ui_registry` | Medio. **Alternativa descartada**: `WITH_CYCLES=OFF` (que además `WITH_PYTHON=OFF` ya fuerza en `CMakeLists.txt:1374`) hace caer estas 6.109 líneas gratis, pero deja el fork sin trazado de rayos para arte de promoción ni horneado. Se elige migrar porque retirar Cycles es una pérdida de capacidad que la doctrina prohíbe sin sustituto |
| `intern/cycles/app/io_export_cycles_xml.py` | 153 | Exportador XML del Cycles independiente | No | **RET** — utillaje de desarrollo del Cycles autónomo | 0 | — | Ninguno |

### 2.8 `scripts/templates_py`, `templates_custom_objects`, `site`, `bge` — 45 ficheros, 2.918

| Ruta | Fich | Líneas | Qué hace | Carga | Destino | h·p | Riesgo |
|---|---:|---:|---|---|---|---:|---|
| `templates_py/` | 41 | 2.791 | Plantillas de *script Python* que el editor de texto ofrece en su menú | Se leen como texto, no se ejecutan | **RET** — sin Python no hay script que plantillar | 0 | Ninguno |
| `templates_custom_objects/custom_object.py` | 1 | 18 | Plantilla de objeto personalizado | Igual | **RET** | 0 | Ninguno |
| `site/sitecustomize.py` | 1 | 63 | Solo se instala con `WITH_PYTHON_MODULE` (bpy como módulo de `pip`) | No, en el editor | **RET** | 0 | Ninguno |
| `bge/interpreter.py` | 1 | 23 | REPL de Python del Player | Sí, si se pide | **RET** — cae con el intérprete | 0 | Ninguno |

### 2.9 `scripts/freestyle` — 46 ficheros, 6.541

| Ruta | Fich | Líneas | Qué hace | Carga | Destino | h·p | Riesgo |
|---|---:|---:|---|---|---|---:|---|
| `modules/parameter_editor.py` 1.587, `modules/freestyle/` 3.588 | 8 | 5.175 | Traduce los ajustes de línea del editor a predicados de Freestyle; biblioteca de sombreadores, predicados y funciones | Sí, ejecutados por `source/blender/freestyle/intern/system/PythonInterpreter.h:46-82` (`BPY_run_filepath`/`BPY_run_text`) | **RET** | 0 | **Se pierde Freestyle entero** |
| `styles/*.py` | 38 | 1.366 | 38 módulos de estilo del «modo Python» de Freestyle | Sí, uno por capa de estilo | **RET** | 0 | Igual |

**Motivo, y es el que la doctrina exige por escrito:** Freestyle no tiene un camino de
ejecución que no sea Python. Su única interfaz de ejecución (`PythonInterpreter.h`)
ejecuta un fichero o un bloque de texto Python **por capa de estilo y por fotograma**.
Migrarlo no es transliterar 6.541 líneas: es diseñar un lenguaje de composición de
estilos y su intérprete, en C++, para un renderizador NPR *offline* que un motor de
juego no usa. Además `CMakeLists.txt:1378` **ya** fuerza `WITH_FREESTYLE=OFF` cuando
`WITH_PYTHON=OFF`: el Player de distribución no lo lleva desde el 2026-09-08 y no se ha
echado en falta. Se retira Freestyle completo (las 6.541 líneas de Python, más
`properties_freestyle.py` 1.348 y `bl_operators/freestyle.py` 223, más el
`source/blender/freestyle` de C++, que es otra decisión y otro carril).
**La alternativa** —conservar solo `parameter_editor.py` migrado a C++ y tirar los 38
estilos— deja el 76% del coste y el 100% del riesgo; se descarta.

### 2.10 `tests/` — 276 ficheros, 44.348

| Ruta | Fich | Líneas | Qué hace | Carga | Destino | h·p | Riesgo |
|---|---:|---:|---|---|---|---:|---|
| `tests/python/` (raíz 24.611, `view_layer` 8.191, `modules` 2.616, `ui_simulate` 1.707, `collada` 432, `overlay` 265, `system_python` 134) | 233 | 37.956 | La suite de pruebas de Blender escrita en `bpy` | No, es desarrollo | **RET** — regla 5d de la doctrina: los tests nuevos, en C++ | 0 (o 400–800 si se repone la cobertura) | **Alto y hay que decirlo claro**: aquí hay cobertura real del núcleo C++ (`view_layer`, `bl_keymap_validate`, `bl_keymap_completeness`, operadores, IO). Retirarla sin reponerla **baja la red de seguridad del fork** |
| `tests/utils` 2.146, `tests/performance` 1.999, `tests/coverage` 552, `tests/files` 720, `tests/blender_as_python_module` 25 | 33 | 5.442 | Arneses de rendimiento, informes de cobertura, ficheros de apoyo | No | **RET** | 0 | Se pierde el arnés de rendimiento; reponerlo en C++ es barato y está en la regla 5d |
| `tests/flipendo/toolsystem/` | 10 | 950 | **Nuestros** arneses de línea base del sistema de herramientas (416/416, 912/912, 14/14, 819 capturas) | No | **RET**, pero **el último**: siguen siendo la línea base que demuestra que el C++ es idéntico | 0 | Retirarlos antes de tiempo destruye la prueba. Se van con `space_toolsystem_*.py` |

### 2.11 `tools/`, `doc/`, `build_files/`, `release/`, codegen en `source/` — 218 ficheros, 39.277

| Ruta | Fich | Líneas | Qué hace | Carga | Destino | h·p | Riesgo |
|---|---:|---:|---|---|---|---:|---|
| `tools/` (`check_source` 6.048, `utils` 5.623, `utils_maintenance` 4.860, `utils_doc` 1.319, `modules/blendfile.py` 1.235, `utils_ide` 1.018, `triage` 977, `debug` 945, `utils_api` 497, `check_blender_release` 492, `utils_build` 218, `check_docs` 218, `git` 50) | 89 | 23.500 | Utillaje de desarrollo: comprobadores de fuente y licencias, limpieza de código, informes semanales, extensión de gdb | **No** (nada se instala) | **RET** | 0 | Bajo. `tools/modules/blendfile.py` (lector de `.blend` en Python) y `tools/utils_maintenance/code_clean.py` son útiles; reponer en C++ solo si se echan en falta |
| `doc/python_api/` (6 + 107 ejemplos) | 113 | 9.620 | Generador Sphinx de la documentación del API de Python y sus 107 ejemplos | No | **RET** — documenta un API que va a dejar de existir | 0 | Ninguno |
| `doc/blender_file_format/` 892, `doc/manpage/blender.1.py` 230 | 3 | 1.122 | Explorador del formato `.blend`; generador de la página de manual | No | **RET** | 0 | Bajo |
| `build_files/utils/` (`make_update` 672, `make_utils` 349, `make_source_archive` 318, `make_bpy_wheel` 289, `make_test_files` 66), `build_files/cmake` 98, `build_files/package_spec` 87 | 7 | 1.879 | Infraestructura de compilación y empaquetado | No | **RET** — el árbol ya no ejecuta Python en el build desde `8022eca1be4` («discover_nodes.py era lo último que el BUILD ejecutaba con Python») | 0 | Se pierde `make update`; el fork ya no sigue a upstream, así que sobra |
| `release/` (`release_notes` 1.120, `datafiles` 555, `lts` 308, `pypi` 80) | 8 | 2.063 | Notas de versión, generador de la geometría de los iconos (su salida está en el árbol), infraestructura LTS y PyPI de Blender | No | **RET** | 0 | Ninguno |
| `source/blender/gpu/vulkan/vk_to_string.py` 413, `source/blender/makesrna/rna_cleanup/` 399, `source/blender/python/rna_dump.py` 128 | 4 | 940 | Generadores y utillaje: `vk_to_string.py` genera `vk_to_string.cc`, **que ya está commiteado** | No | **RET** | 0 | Ninguno |

### 2.12 `extern/` — 18 ficheros, 1.317 — **EXTERNAL**

`extern/audaspace/bindings/python/examples` (12, 335), `extern/ceres/internal/ceres/*`
(4, 850: generadores de plantillas del propio Ceres), `extern/audaspace/src/respec`
(1, 121), `extern/mantaflow/preprocessed/python` (1, 11).
**EXT** — terceros vendorizados, se mantienen verbatim. No son código de Flipendo y su
presencia no impide `WITH_PYTHON=OFF` (nada los ejecuta).

---

## 3. Resumen numérico

| Destino | Ficheros | Líneas | % |
|---|---:|---:|---:|
| **MIGRAR-A-C++** | 182 | **99.041** | 39,7 % |
| **CONVERTIR-A-DATOS** | 172 | **5.504** | 2,2 % |
| **RETIRAR** | 659 | **143.520** | 57,6 % |
| **EXTERNAL** | 18 | **1.317** | 0,5 % |
| **Total** | **1.031** | **249.382** | 100 % |

Comprobación: 99.041 + 5.504 + 143.520 + 1.317 = 249.382 ✓ · 182 + 172 + 659 + 18 = 1.031 ✓

Desglose de MIGRAR (99.041):

| Bloque | Líneas |
|---|---:|
| `bl_ui` (75 ficheros, sin los 2 del sistema de herramientas) | 59.295 |
| `bl_operators` (36) | 18.427 |
| Addon de Cycles (9) | 6.109 |
| `bpy_extras` (14) | 3.916 |
| Addons de juego + editor de `addons_core` (7) | 4.643 |
| `bgui` (15) | 2.391 |
| Módulos varios de `scripts/modules` (21) | 3.411 |
| Keying sets de fábrica (1) | 686 |
| Plantillas de aplicación (4) | 163 |

Desglose de RETIRAR (143.520), por motivo:

| Motivo | Líneas |
|---|---:|
| Tests del propio Python y arneses (276 ficheros) | 44.348 |
| Utillaje de desarrollo: `tools/`, `build_files/`, `release/`, codegen (108) | 28.382 |
| Gestor de extensiones `bl_pkg` (15) | 19.687 |
| Documentación del API de Python (116) | 10.742 |
| Keymap por defecto ya transliterado (2) | 9.055 |
| Freestyle entero (46) | 6.541 |
| Traducción `.po` offline (10) | 5.254 |
| Sistema de herramientas ya transliterado (2) | 4.865 |
| Puente `bpy` + `bpy_types` (8) | 3.784 |
| `addon_utils` + extensiones internas (9) | 3.378 |
| Plantillas de script, `site`, REPL (44) | 2.895 |
| Utillaje importable de `scripts/modules` (12) | 2.849 |
| Consola de Python (8) | 1.139 |
| Resto (`keymap_from_toolbar`, `nodeitems_builtins`, `cycles/app`) | 601 |

### El camino crítico, ordenado

El orden no es por tamaño: es por **cuánto desbloquea cada paso**.

1. **`--fl-dump-ui` / `--fl-check-ui` verdes** *(carril D, en el árbol de trabajo sin
   commitear)*. Sin volcador de interfaz no se puede migrar `bl_ui` con evidencia, y
   `bl_ui` son 59.295 de las 99.041 líneas a migrar. **Esto es el cuello de botella de
   todo lo demás.** — desbloquea 59.295.
2. **`bl_ui/space_view3d.py` (9.381)**, porque contiene 65 de los 129 menús que el
   keymap nativo ya invoca por nombre. Mientras no esté, `WITH_PYTHON=OFF` deja la vista
   3D sin menús contextuales ni tartas. — desbloquea el 50 % de los puentes.
3. **`presets.py` + el formato `.fpreset`** *(carril A, en curso)*. Es el único punto
   donde el editor **escribe** Python en disco: mientras exista, «cero Python» es falso
   aunque no quede ni un `.py` en el árbol. — cierra 1.232 líneas de datos y 1.015 de
   operador.
4. **Los 7 `wm.context_*` que faltan + los 12 puentes de `wm.py`** *(carril B)*.
   Pequeño, muy referenciado desde C++ (pantalla de bienvenida incluida). — 3.015.
5. **Las 8 familias de `bl_operators` que el keymap nativo nombra** (`mesh.select_*_item`,
   `object.select_hierarchy`, `object.subdivision_set`, `object.add_modifier_menu`,
   `render.play_rendered_anim`, `sequencer.split_multicam`, `armature.collection_show_all`,
   `spreadsheet.toggle_pin`, más `node.connect_to_output` y `node.viewer_shortcut_*`).
   Son atajos que hoy funcionan y mañana no, en silencio.
6. **Retirada barata, hoy mismo, sin coste ni riesgo: 85.570 líneas.** `tools/` (23.500),
   `doc/` (10.742), `tests/` no propios (43.398 — con la reposición de cobertura
   anotada como deuda), `build_files/` (1.879), `release/` (2.063), codegen en `source/`
   (940), `cycles/app` (153), plantillas de script (2.918), submódulos `bge_*` vacíos.
   Ninguna se instala, ninguna es capacidad. **Es el 34 % del Python del árbol y se va
   con `git rm`.**
7. **`keymap_data/blender_default.py` + `Blender.py` (9.055) y `space_toolsystem_*.py`
   (4.865)**: sustitutos existentes y verificados. Salen en cuanto caigan sus lectores
   Python (que son `bl_ui`, paso 1).
8. **`bl_ui` restante (49.914)** en el orden: `node_add_menu_*` (tabla pura) →
   `properties_*` → `space_*` → los ayudantes.
9. **`bl_operators` restante**, **addon de Cycles**, **`bgui`**, **addons de juego**.
10. **El bloque terminal**: `bpy/`, `bpy_types.py`, `addon_utils`, `bl_pkg`, consola.
    Solo se pueden quitar cuando nada más los use. Son 27.988 líneas que caen de golpe
    el último día.

---

## 4. Los cinco subsistemas donde el C++ llama de vuelta a Python

Verificado en el árbol, no en las políticas. **La lista del encargo acierta en cuatro y
falla en el quinto**, y le faltan dos que son mayores que todos los demás juntos.

### 4.1 Keymap — **hecho, con un residuo**

`wm.cc:424-450`: `WM_keyconfig_reload` llama a `flipendo::keymap::register_default`. La
llamada a `bpy.utils.keyconfig_init()` desapareció, y `keyconfig_init` **no tiene hoy
ningún llamador vivo** (su única mención fuera de su definición es un ejemplo de la
documentación: `doc/python_api/examples/bpy.utils.register_cli_command.1.py:49`). Verificado 3.673/3.673 atajos, 248/248 keymaps,
cero diferencias. Además se ganó algo: ya no hay guarda de `G.background`, así que en
`--background` **sí** hay keymap por defecto, cosa que antes no ocurría.

**Lo que falta, y está escrito en el propio código** (`wm.cc:470-479`): la guarda
`CTX_py_init_get(C)` de `WM_keyconfig_init` **sigue ahí**, y no como «espera a Python»
sino como marca de fase: `WM_keyconfig_init` se llama dos veces y en la primera aún no
están registrados los operadores MACRO — construir el keymap ahí deja 502 líneas de
diferencia. Con `WITH_PYTHON=OFF`, `CTX_py_init_get` **nunca es cierto** y el keymap
por defecto **no se construye nunca**. Hace falta una marca de fase explícita
(`WM_INIT_FLAG_*` propia, puesta tras `ED_spacemacros_init`). Es un cambio de diez
líneas y es un bloqueante duro.

### 4.2 Sistema de herramientas — **los puentes están cortados; queda el Python muerto**

`wm_toolsystem.cc:940` y `:995` documentan que la activación ya no busca el operador
Python. Los nueve puentes declarados en `TOOLSYSTEM-A-CPP.md` están cortados y verificados
(catálogo 416/416, keymaps de barra 912/912, barra 14/14 al píxel, cabeceras con 819
capturas). Los cuatro `*_PT_tools_active` y `WM_MT_toolsystem_submenu` **los registra
C++** (`fl_toolbar_ui.cc:818-855`).

**Lo que falta:** el Python del sistema de herramientas (4.865 líneas) **sigue cargándose**
porque lo leen **otros** ficheros de `bl_ui` (`space_topbar`, `space_view3d`, `space_image`,
`space_sequencer`, `space_node`, `properties_paint_common`, `space_view3d_toolbar`) y
`bl_keymap_utils/keymap_hierarchy`, y porque su `register()` sigue siendo necesario para
convertir 362 `keymap=()` en nombres. Ya no es Python→C++: es Python→Python. **Cae
entero cuando caiga `bl_ui`, no antes.** Y hay **un puente en dirección contraria que
sigue vivo**: `fl_tool_settings_*.cc` abre `TOPBAR_PT_tool_settings_extra` con
`popover()`, y `fl_toolbar_ui.cc:678` abre `TOPBAR_PT_tool_fallback`. Los dos paneles
están en `space_topbar.py`. **Hoy el C++ del sistema de herramientas depende de Python
para dos popovers.**

### 4.3 Familia `wm.context_*` — **11 de 18, en curso**

`wm_context_ops.cc` (588 líneas) tiene 11: `context_toggle`, `set_boolean`, `set_int`,
`set_float`, `set_string`, `set_enum`, `toggle_enum`, `cycle_int`, `cycle_enum`,
`scale_float`, `scale_int`. Verificado con 131 atajos del keymap.

**Faltan 7**, todos aún en `wm.py`: `context_set_value`, `context_cycle_array`,
`context_menu_enum`, `context_pie_enum`, `context_set_id`,
`context_collection_boolean_set`, `context_modal_mouse`. Dato medido y útil: **ninguno de
los siete aparece en el keymap por defecto**; se invocan desde menús y desde el propio
C++ — `interface.cc:1580-1581` y `interface_context_menu.cc:102` nombran
`WM_OT_context_cycle_array` y `WM_OT_context_menu_enum` para el menú contextual de un
botón. Así que su ausencia no rompe atajos: rompe el clic derecho sobre propiedades.
Carril B está dentro.

### 4.4 Ejecución de presets `.py` — **en curso, y es el peor de los cinco**

`presets.py:291` hace `bpy.utils.execfile(filepath)` sobre cada preset. Los 171 presets
son 1.232 líneas de asignaciones (166) y condicionales NTSC/PAL (5). El camino XML
alternativo (`rna_xml.xml_file_run`, `presets.py:300`) sirve a los temas.

**Estado real:** carril A tiene en el árbol de trabajo, sin commitear,
`source/blender/windowmanager/preset/FL_preset.hpp` (162) y `fl_preset_file.cc` (937):
formato `.fpreset` versión 1, con gramática de valores, lector/escritor nativo **y
lector nativo del subconjunto Python heredado**. **Lo que falta:** enganchar
`script.execute_preset` y los 22 `AddPreset*` a ese lector, y decidir el condicional de
ffmpeg (propuesta: dos entradas de datos resueltas por `render.fps`, no un mini-lenguaje).

Y una cosa que ninguna política dice: **`AddPresetBase.execute` escribe un fichero
`.py`**. No basta con leer datos; hay que dejar de generar código. Mientras `presets.py`
exista, el editor **produce** Python cada vez que el usuario guarda un preset.

### 4.5 `bpy.utils.previews` — **no es un puente. La lista del encargo se equivoca aquí**

Medido: `scripts/modules/bpy/utils/previews.py` (136 líneas) hace `from _bpy import
_utils_previews` y envuelve tres funciones de `source/blender/python/intern/bpy_utils_previews.cc`.
La dirección es **Python → C++**, no al revés. Y no lo usa nadie: los únicos ficheros del
árbol que lo mencionan son dos plantillas de `templates_py` (documentación). El C++ no
llama a `bpy.utils.previews` en ningún sitio.

Lo que sí existe y probablemente se confundió con esto: `bl_previews_utils/bl_previews_render.py`
(536 líneas), que **no** lo invoca el C++ sino los operadores Python
`wm.previews_batch_generate/clear` de `bl_operators/file.py`, lanzando un Blender hijo.
Los operadores nativos hermanos (`WM_OT_previews_ensure`, `WM_OT_previews_clear`) **ya
son C++** desde antes de esta migración (`wm_operators.cc:3907,4042`).

**Destino:** `previews.py` → RETIRAR con el resto del puente `bpy/`. `bl_previews_render.py`
→ MIGRAR (modo lote del operador nativo). **Ninguno de los dos es un bloqueante.**

### 4.6 Los dos que faltaban en la lista, y son los grandes

**(a) 170 tipos de UI que el C++ nombra por cadena y solo Python registra.**
145 menús, 24 paneles y 1 lista. **129 de ellos los invoca el keymap nativo**
(`menu(km, "…")` / `menu_pie(km, "…")` en `fl_keymap_g*.cc`), repartidos así: VIEW3D 65,
IMAGE 11, SEQUENCER 8, CLIP 8, GRAPH 7, TOPBAR 5, DOPESHEET 5, NLA 4, NODE 3, y 13 más
sueltos. **Este es el mayor puente C++ → Python del árbol, y no está en ninguna política.**
Con `WITH_PYTHON=OFF`, 129 atajos del keymap nativo —que se verificó idéntico al de
Python— no abren nada y no dan error. También cuelgan de aquí la pantalla de bienvenida
(`wm_splash_screen.cc:348,355,476` busca `WM_MT_splash`, `WM_MT_splash_quick_setup`,
`WM_MT_splash_about`), el menú contextual de cualquier botón (`UI_MT_button_context_menu`,
`interface_context_menu.cc:1281`), el de cualquier lista (`UI_MT_list_item_context_menu`)
y la propia plantilla de lista (`UI_UL_list`).

**(b) 15 operadores Python que el C++ nombra por cadena.** `WM_OT_path_open`
(`interface_context_menu.cc:453,957`, `buttons_ops.cc:322`), `WM_OT_url_open`
(pantalla de bienvenida), `WM_OT_doc_view` y `WM_OT_doc_view_manual`
(`wm_operators.cc:4072-4076`, la ayuda del botón derecho), `WM_OT_drop_blend_file`
(`screen_ops.cc:6813`, soltar un `.blend` en la ventana), `WM_OT_properties_context_change`
(`MOD_ui_common.cc:432,439`), `WM_OT_operator_preset_add`
(`interface_template_operator_property.cc:102`), `WM_OT_context_cycle_array`,
`WM_OT_context_menu_enum`, `WM_OT_batch_rename`, `OBJECT_OT_geometry_nodes_move_to_nodes`,
`CONSOLE_OT_execute`, `CONSOLE_OT_autocomplete`, `CONSOLE_OT_banner`, y los dos
`EXTENSIONS_OT_userpref_show_*` de `bl_pkg` (`interface_template_status.cc:425,450,474`).

---

## 5. Qué se rompe al compilar el **editor** con `WITH_PYTHON=OFF`

El Player ya existe sin CPython (`PLAYER-SIN-CPYTHON.md`: 0 símbolos `_Py`, 536 MB
frente a 771 MB, 5/5 componentes nativos en `ArpgNative.blend` y en `T1_LaMancha.blend`).
Para el editor, apagar la opción activa 422 ocurrencias de `WITH_PYTHON` repartidas en
138 ficheros (81 de ellos en `source/gameengine`, 14 en `blenkernel`, 11 en `makesrna`,
10 en `editors`, 9 en `windowmanager`). Esto es lo que se rompe **hoy**, en orden de
arreglo.

**Bloque 1 — lo que falla en silencio (primero, porque no da error).**

1. **Los campos numéricos dejan de calcular.** `interface.cc:3462-3475` y
   `numinput.cc:270-298`: sin Python, la rama `#else` es `*r_value = atof(str)`.
   Escribir `2*3` en un campo da **2**; escribir `5cm` da **5** e ignora las unidades.
   Arreglo: `BLI_expr_pylike` (que ya existe y es C++) + `BKE_unit_replace_string`, en
   lugar de `atof`. Es la reparación más barata con más impacto: **~40 líneas**.
2. **Los drivers complejos dejan de evaluarse.** `fcurve_driver.cc:1386-1401`: primero
   se intenta `driver_try_evaluate_simple_expr` (nativo, `BLI_expr_pylike`), y **solo si
   falla** se llama a `BPY_driver_exec`. Con `WITH_PYTHON=OFF` esa rama es
   `UNUSED_VARS(...)`: el driver **conserva su valor anterior** sin avisar. Los drivers
   simples (la mayoría de un rig) siguen funcionando; los que llaman a funciones de
   Python, no. Arreglo mínimo: reportar una vez por driver, como ya se hizo con
   `SCA_PythonController::Trigger()`. Arreglo bueno: ampliar `BLI_expr_pylike`.
3. **Los 129 menús del keymap y los 15 operadores del §4.6.** No dan error: la tecla no
   hace nada.

**Bloque 2 — lo que se apaga solo, y hay que decidir qué se pierde.**
`CMakeLists.txt:1374-1378` fuerza, al apagar Python: `WITH_CYCLES=OFF`,
`WITH_DRACO=OFF`, `WITH_MOD_FLUID=OFF` y `WITH_FREESTYLE=OFF`. Es decir, un editor sin
Python hoy **no tiene trazado de rayos, ni Mantaflow, ni Freestyle, ni compresión Draco**.
De los cuatro, este inventario propone recuperar Cycles (§2.7, migrando su addon) y
retirar Freestyle por escrito (§2.9); Mantaflow y Draco quedan pendientes de decisión.

**Bloque 3 — lo que no está en esa lista y revienta la compilación.**
`WITH_USD` **no** está en `set_and_warn_dependency`, y `boost::python` de USD hace
`#include <pyconfig.h>` incondicional: el PCH de `bf_io_usd` no compila. La salida
conocida es `-DWITH_USD=OFF -DWITH_HYDRA=OFF -DWITH_MATERIALX=OFF`. Para el editor eso
sí es una pérdida (USD es intercambio de escenas), así que la reparación correcta es
añadir `set_and_warn_dependency(WITH_PYTHON WITH_USD OFF)` **y** decidir si Flipendo
quiere USD; si lo quiere, hay que compilar USD sin `boost::python`.

**Bloque 4 — el arranque.** La marca de fase `CTX_py_init_get` del §4.1: sin ella no se
construye el keymap. Y `wm_init_exit.cc:510`, `wm_files.cc:752,758,1317` y
`wm_platform.cc:77` ejecutan cadenas Python en el arranque, al cargar fichero y al
resolver la plantilla de aplicación; todas están guardadas, pero al desaparecer se van
con ellas `bl_app_template_utils.reset()` y `addon_utils.disable_all()`, que hay que
sustituir por sus equivalentes nativos (§2.3, §2.4).

**Bloque 5 — lo que desaparece y ya está decidido:** editor de texto con «ejecutar
script» (`text_ops.cc:876`), espacio Scripting entero (`script_edit.cc`,
`space_script.cc`), consola de Python, restricción `Script` de constraints
(`object_constraint.cc`, `constraint.cc`), `python_proxy.cc` (que aún mantiene el tipo
falso `FT_KX_PythonComponent` para el panel de componentes del editor).

**Orden recomendado:** 1 (campos numéricos y drivers, ~1 noche) → 4 (marca de fase,
~1 noche) → 3 (USD, ~1 noche) → 2 (decisiones de capacidad, escritas) → §4.6 (los 129
menús, que es `bl_ui`) → 5 (retirada final).

---

## 6. Calendario honesto

**Base de cálculo, medida del propio repositorio** (`git log --numstat`):

| Sesión | C++ producido | Python cubierto | Verificación |
|---|---:|---:|---|
| Noche 09-08 → 09-09 | +13.703 líneas | 9.055 (keymap) | 3.673/3.673 atajos |
| Noche 09-09 → 09-10 | +9.061 líneas | 5.259 (herramientas, fases 1-3) | 416/416 herramientas |
| Noche 09-10 (hasta `4e88d96a431`) | +7.127 líneas | ~900 (barra, fases 4a-4c) | 912/912, 14/14, 819 capturas |
| **Total 3 noches** | **+29.891 líneas C++** | **~15.200 líneas Python** | — |

Ritmo medido: **~5.000 líneas de Python cubiertas por noche**, produciendo **~10.000
líneas de C++**. Aviso importante: ese material era el más tabular del árbol (un keymap
y un catálogo de herramientas). Traducido a horas-persona-agente, sale a ~150 líneas/h·p
en material declarativo y ~50 líneas/h·p en material con lógica y verificación al píxel.

**Esfuerzo total del inventario:**

| Partida | h·p |
|---|---:|
| MIGRAR declarativo (63.912 líneas: `bl_ui` + UI/propiedades de Cycles) | 430–640 |
| MIGRAR con lógica (35.129 líneas) | 590–880 |
| CONVERTIR-A-DATOS (5.504) | 60–120 |
| Bloqueos estructurales de `WITH_PYTHON=OFF` (§5) | 200–320 |
| Arneses de verificación nuevos (uno por familia, estilo `--fl-dump-*`) | 120–200 |
| Retirada ordenada de las 143.520 líneas | 40–80 |
| **Subtotal** | **1.440–2.240** |
| Reponer en C++ la cobertura de los 44.348 de tests | 400–800 |
| **Total con la red de seguridad repuesta** | **1.840–3.040** |

**Traducido a noches como esta** (cinco carriles, agentes en paralelo, ~50–100 h·p por
sesión):

- **Optimista** (todo resulta tan tabular como el keymap): **20 noches**.
- **Realista** (declarativo al ritmo medido, la lógica a un tercio de ese ritmo, más los
  bloqueos y los arneses): **40 noches**.
- **Incómodo, que es el que hay que creerse**: **60 noches.** La experiencia de la
  cabecera de herramientas —819 capturas, la primera comparación dio 819 de 819
  distintas y ninguna era del código— dice que en `bl_ui` **el arnés cuesta más que la
  transliteración**, y `bl_ui` es el 60 % de lo que queda por migrar.

Con 60 noches: a dos por semana, **siete meses**. A una por semana, **catorce meses**.
A una al mes, **cinco años**.

### Qué es plurianual de verdad, y qué no

**No es plurianual:** llegar a cero ficheros `.py` en el árbol y a un editor que compila
con `WITH_PYTHON=OFF`. Con el ritmo demostrado son meses, no años. La afirmación de
`MIGRACION-CPP.md` §6 («el editor se mantiene EXTERNAL, su erradicación total no se
promete») y la de `BACKLOG-EDITOR-PYTHON.md` §4 («plurianual, y buena parte nunca
compensa») **están desmentidas por la medición**: se escribieron con una foto de 479.178
líneas de Python que hoy son 249.382, y antes de que existieran `FL_ui_registry`, el
keymap nativo y el sistema de herramientas nativo. **Este documento las sustituye.**

**Sí es plurianual, o directamente permanente:**

1. **La cobertura de pruebas.** 44.348 líneas de tests en `bpy` que verifican el núcleo
   C++. Reponerlas en C++ es un proyecto en sí (400–800 h·p) y compite con la migración
   por los mismos recursos. La salida realista es reponer solo lo que cubre lo migrado, y
   **anotar por escrito la cobertura perdida**, fichero a fichero.
2. **La divergencia.** Desde el momento en que `bl_ui` es C++, incorporar una mejora de
   Blender 4.6 deja de ser un `git merge` y pasa a ser una transliteración manual. Eso no
   se «termina»: es un coste por año, para siempre.
3. **Lo que no se migra porque no puede migrarse.** El API de extensión (`bpy`) y el
   ecosistema de addons de terceros. Freestyle. La consola. Los presets escritos a mano
   por usuarios. Cuando el intérprete se va, se van con él, y no vuelven. Cada una está
   marcada RETIRAR arriba con su motivo, que es lo que la doctrina exige: *borrar sin
   reemplazo solo vale si la capacidad no es de Flipendo, y se justifica por escrito.*

### La cifra, sin adornos

**99.041 líneas por migrar, 5.504 por convertir en datos, 143.520 por retirar. Entre 40 y
60 noches como esta para el editor sin Python. Más un coste permanente de divergencia. Y
tres capacidades que se pierden a propósito: extensiones de terceros, Freestyle y la
consola.**

---

*Método: `git ls-files`, `wc -l` y `grep` sobre `flipendo-main` @ `4e88d96a431`, 2026-09-11.
Los cruces de identificadores se hicieron con conjuntos extraídos del árbol, no por lectura.
Donde una decisión era discutible, están escritas las dos opciones y el motivo de la elegida:
Cycles (§2.7), Freestyle (§2.9), presets de ffmpeg (§2.5), `bpy_extras` (§2.4) y la
cobertura de tests (§2.10).*
