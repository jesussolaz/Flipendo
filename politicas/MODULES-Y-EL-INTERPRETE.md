# `scripts/modules` y el intérprete — qué se migra, qué son datos y qué desaparece

> Medido fichero a fichero el 2026-09-11 sobre `flipendo-main`, después de que la
> poda de la noche dejara el directorio en **97 ficheros y 30.165 líneas** (el
> `INVENTARIO-PYTHON.md` decía 99 y 30.788: entre medias cayeron
> `bl_keymap_utils/keymap_from_toolbar.py` y `bpy/utils/toolsystem.py`, y
> `bpy/utils/__init__.py` adelgazó de 1.382 a 1.185).
>
> Este documento existe porque `scripts/modules` es el bloque de Python más
> confuso que queda: mezcla tres cosas que **no se tratan igual**, y tratarlas
> igual es como se pierde una noche.

---

## 0. Las tres categorías, con sus cifras

| Categoría | Ficheros | Líneas | Qué se hace con ella |
|---|---:|---:|---|
| **(a) Pegamento CPython** | 19 | **7.366** | *No se traduce.* Desaparece el día que no haya intérprete |
| **(b) Capacidad de verdad** | 77 | **18.527** | Se migra a C++ (47 fich., 8.199) o se retira con motivo escrito (30 fich., 10.328) |
| **(c) Datos disfrazados de código** | 1 | **4.272** | Convertido en datos y **el `.py` retirado** |
| **Total** | **97** | **30.165** | |

Comprobación: 7.366 + 18.527 + 4.272 = 30.165 ✓ · 19 + 77 + 1 = 97 ✓

> Tras las dos retiradas que documenta §4.2 —`bl_app_override/` y
> `bl_keymap_utils/platform_helpers.py` (419 líneas), y luego
> `bpy_extras/mesh_utils.py`, `bpy_extras/id_map_utils.py`,
> `bpy_extras/wm_utils/progress_report.py` y `graphviz_export.py` (873 líneas)—,
> todas demostradas sin una sola referencia en el árbol, el directorio queda en
> **87 ficheros y 24.544 líneas**: la migración de `bl_text_utils/` a C++
> (§4.2b), 1.292 líneas sin un solo llamador, y las **4.272** de
> `rna_manual_reference.py`, que se va porque su sustituto nativo está
> registrado y verificado (§3). Son **5.621 líneas menos, ninguna con pérdida de
> capacidad**: un 19 % del directorio en una noche.

La distinción importa porque las tres tienen **coste y riesgo distintos**, y
porque la (a) es la única que no hay que planificar: no cuesta horas, cuesta
esperar. Meterla en la columna de «migrar» infla el presupuesto con 6.961
líneas que nadie va a escribir nunca en C++.

---

## 1. Cómo se comprobó quién carga qué

No por lectura. Cuatro barridos sobre el árbol:

1. **Importadores por módulo**: para cada módulo de primer nivel, cuántos
   ficheros `.py` fuera de él escriben `import X` o `from X…`, separando los que
   **viajan en el bundle** (`scripts/`, `intern/cycles/blender/addon`) de los que
   no (`tests/`, `tools/`, `doc/`).
2. **Puentes C++ → Python**: todas las llamadas a `BPY_run_string_eval`,
   `BPY_run_string_exec`, `BPY_run_filepath`, `BPY_run_text` y
   `BPY_run_string_as_*` del árbol, leyendo su lista de `imports[]`.
3. **Carga por nombre construido**: los sitios donde el módulo se elige en
   ejecución (`"console_" + sc.language`), que un `grep` de `import` no ve.
4. **Ficheros `.py` que el editor *escribe***: el caso que más veces se olvida.

### Los cinco puentes C++ → Python que salen de `scripts/modules`

| Puente | Dónde | Estado |
|---|---|---|
| `bpy.utils.load_scripts_extensions()` | `wm_init_exit.cc:516` | Vivo. Es el arranque del registro entero |
| `bpy.utils._on_exit()` | `wm_init_exit.cc:636` | Vivo |
| `bl_app_template_utils.reset()` | `wm_files.cc:752` | Vivo |
| `addon_utils.disable_all()` / `.reset_all()` | `wm_files.cc:758,1317`, `bpy_interface.cc:556` | Vivo |
| `bl_text_utils.external_editor.open_external_editor()` | `text_ops.cc:3987` | **Cortado.** Era el único que implementaba capacidad de usuario; hoy es `flipendo::text::open_external_editor()` (§4.2b) |

Y uno **muerto que conviene saber que está muerto**: `wm_platform.cc:64` importa
`_bpy_internal.freedesktop`, **que no existe en el árbol**. No rompe nada porque
está bajo el `#else` de `#elif defined(__APPLE__)`, o sea que en un fork solo para
Mac esa rama no se compila nunca. Queda anotado para que nadie lo cuente como
puente vivo.

---

## 2. (a) El pegamento CPython — 19 ficheros, 7.366 líneas. **NO SE TOCA**

Esto **es** el puente CPython↔C: `from _bpy import …`. No implementa capacidad,
la expone. Traducirlo a C++ no significa nada: sería escribir en C++ un envoltorio
de C++ para que lo llame un intérprete que no va a existir.

| Fichero | Líneas | Qué es |
|---|---:|---|
| `bpy/utils/__init__.py` | 1.185 | `load_scripts`, `register_class`, rutas de recursos, `execfile`, `manual_map` |
| `bpy_types.py` | 1.491 | Las clases base de Python (`Operator`, `Panel`, `Menu`…) y sus mixins |
| `addon_utils.py` | 1.992 | Descubrimiento, activación y desactivación de addons |
| `_bpy_internal/extensions/` (6) | 1.341 | Ruedas `pip`, ficheros obsoletos, uniones de directorio, etiquetas, permisos |
| `bpy/path.py` | 459 | Rutas al estilo `bpy` (`abspath`, `basename`, `display_name`) |
| `bpy/ops.py` | 184 | El objeto `bpy.ops` |
| `bpy/utils/previews.py` | 136 | Envoltorio de `bpy_utils_previews.cc`. **La dirección es Python → C++** |
| `_bpy_internal/grease_pencil/` (2) | 405 | Rebanadas de trazo (`drawing.strokes[n].points[i]`). **Es azúcar del API de Python sobre `attributes`, no capacidad**: su propio docstring dice que no se use para nada que importe el rendimiento y que se use `GreasePencilDrawing.attributes`, que ya es C++ |
| `bpy/__init__.py` | 75 | El paquete |
| `bpy_restrict_state.py` | 50 | Bloquea `bpy.context` mientras se registran las clases |
| `_bpy_internal/addons/` (2), `_bpy_internal/__init__.py` | 48 | CLI de `--addons` |

**Por qué desaparece en vez de migrarse, y no es una excusa:** la capacidad que
este código presta es *«que exista un API de Python»*. Esa capacidad se retira a
propósito y está justificada por escrito en `INVENTARIO-PYTHON.md §6`: sin
intérprete no hay addons de terceros, y ése es el precio declarado del fork.
Todo lo demás que estos ficheros hacen —cargar el registro, activar addons,
resolver rutas— ya tiene o tendrá su equivalente nativo, y entonces estas 7.366
líneas se van de golpe, el último día, sin escribir ni una línea de C++ a cambio.

**Es el último bloque en caer.** Retirar cualquiera de estos ficheros antes de
que caiga todo lo que los usa deja el editor sin arrancar.

---

## 3. (c) Datos disfrazados de código — 1 fichero, 4.272 líneas. **HECHO**

`rna_manual_reference.py`: 4.272 líneas de las que 4.253 son una tupla
autogenerada de pares (patrón de ruta RNA → sufijo de URL del manual). Ni es
código ni debería ejecutarse; el editor la cargaba con `execfile()` **entera y de
nuevo** en cada consulta.

Sustituto: `release/datafiles/manual/rna_manual_reference.txt` (datos) +
`source/blender/windowmanager/manual/` (lector nativo). Ver el commit
«Manual en línea: la tabla de 4.253 rutas RNA, como datos y con lector en C++»
para el formato, las trampas y las cifras.

**Qué se midió antes de escribir nada**, porque el formato depende de ello:

- 4.253 entradas; todas encajan en `("patrón", "url"),`; ningún patrón usa `?`
  ni `[`; los 4.253 usan `*`, 17 con el `*` en medio; cero mayúsculas, cero
  tabuladores, cero no-ASCII, cero duplicados.
- La tabla está **ordenada por longitud de patrón descendente** y se comprobó
  (0 violaciones). Gana la primera que casa: el orden **es** parte del dato.

**Decisión, con su motivo:** no se reutiliza el formato `.fpreset` que el carril
A escribió esta noche. Aquél describe asignaciones a propiedades RNA con su
gramática de valores; esto es una tabla de dos columnas donde lo único
estructural es el orden. Un formato general para un caso que no lo necesita es
deuda, no reutilización.

**Y una trampa que costó encontrar, con el operador ya nativo.** Cuando el carril
B migró `WM_OT_doc_view_manual` a `wm_system_ops.cc`, su proveedor integrado
(`builtin_manual_provider`) se quedó con **cinco** entradas escritas a mano y una
reserva que manda todo lo demás a `search.html?q=<ruta>`. Su comentario lo llama
«contrato sustituto de la enorme tabla Python generada». No lo es: el Python
tenía **4.253** entradas y llevaba al **párrafo concreto del manual, con su
ancla**. Un buscador no es el mismo comportamiento observable, y el usuario no
ve ningún error — es exactamente el tipo de pérdida silenciosa que la doctrina
persigue.

Arreglado sin tocar el fichero del otro carril, usando su propio punto de
extensión (`WM_manual.hpp`): `provider_register_builtin_table()` registra la
tabla de datos como proveedor desde `WM_init()`. Como los proveedores se
consultan en orden inverso al registro, la tabla se consulta **antes** que el
integrado; si la ruta está en la tabla se devuelve la URL exacta y si no se
devuelve `false` para que la reserva del buscador siga funcionando. Los dos
conviven y ninguno se estorba. `--fl-check-manual` comprueba ahora **las dos
cosas**: la tabla y el camino real del operador (`url_lookup()`), que es donde
puede fallar el registro y el orden.

**Queda anotado para el carril B, y no lo toco:** su `manual_language_code()`
usa `BLT_lang_get()`, que devuelve el *locale activo* y con la traducción apagada
vale `"en_US"`. El Python leía `preferences.view.language` —el identificador del
enum, `DEFAULT` incluido, con caída a `$LANG`—. Con un idioma elegido pero la
traducción desactivada, las dos cosas no coinciden y el prefijo del manual sale
distinto. El prefijo que verifica `--fl-check-manual` es el nuestro, que sí
reproduce el del Python.

**El `.py` ya no está.** `scripts/modules/rna_manual_reference.py` (4.272) se
retiró en cuanto se cumplieron las tres condiciones de la doctrina: el sustituto
existe (`datafiles/manual/rna_manual_reference.txt` + el lector), está
**registrado** (`provider_register_builtin_table()` desde `WM_init()`) y está
**verificado idéntico** (7.470/7.470 por el camino del operador). Con él se fue
`tests/python/bl_rna_manual_reference.py` (187), que era su test y no podía
sobrevivirle. Y en `bpy/utils/__init__.py` desapareció `_blender_default_map()`:
`_manual_map` arranca ahora vacío y queda solo como enganche para lo que aún se
escriba en Python. El editor arranca limpio y `bpy.utils.manual_map()` devuelve
una lista vacía, que es lo correcto: la tabla integrada ya no vive en Python.

Para reimportar la tabla de upstream el camino es el mismo de siempre más un
paso: `tools/utils_doc/rna_manual_reference_updater.py` escribe un `.py` (fuera
del árbol) y `--fl-convert-manual-reference` lo convierte en el fichero de datos.

**Cobertura de pruebas que hay que reponer:** `tests/python/bl_rna_manual_reference.py`
(187 líneas) validaba la tabla desde dentro: que el dato tiene la forma correcta,
que **toda** ruta RNA tiene entrada y que **todo** patrón se usa alguna vez, y
que la tabla de idiomas está completa. Ese test cae con el resto de
`tests/python`. Su barrido de rutas RNA (`rna_info.BuildRNAInfo()`) es el que se
reutiliza para congelar la línea base del lector nativo. La comprobación de
«ningún patrón sobra» **no** está repuesta todavía: queda anotada como deuda.

*Detalle que confirma la trampa 1 del commit:* ese test empareja con
`fnmatchcase` directo, **sin** la comprobación de prefijo literal que hace
`wm.py`. O sea que el test y el editor no se comportan igual con un patrón que
empiece por comodín. El comportamiento que manda es el del editor.

**Dónde vive el dato, y por qué no en `scripts/`:** `source/creator/CMakeLists.txt`
instala el árbol `scripts/` **entero dentro de `if(WITH_PYTHON)`**. Un dato que
tiene que sobrevivir a `WITH_PYTHON=OFF` no puede vivir ahí. Va a
`datafiles/manual/`, como las fuentes y los estudios de iluminación. *Esto es una
trampa que afecta también a los `.fpreset`: hoy están en `scripts/presets/` y con
`WITH_PYTHON=OFF` no se instalarían.*

---

## 4. (b) Capacidad de verdad — 79 ficheros, 18.932 líneas

### 4.1 Lo que se migra a C++ — 45 ficheros, 8.145 líneas

(Eran 49 y 8.604: `bl_text_utils/` ya está migrado —§4.2b— y
`_bpy_internal/grease_pencil/` resultó ser pegamento, no capacidad —§2.)

| Bloque | Fich | Líneas | Por qué es capacidad | Quién la usa hoy |
|---|---:|---:|---|---|
| `bgui/` + `gpu_extras/` | 18 | 2.581 | **El sistema de interfaz del juego.** `gpu_extras` no es utillaje suelto: lo importan `bgui/frame.py`, `image.py` y `progress_bar.py` | El juego, si lo importa |
| `bpy_extras/` (los 9 con llamador vivo) | 9 | 2.211 | `anim_utils` 757, `io_utils` 616, `object_utils` 289, `image_utils` 194, `keyconfig_utils` 141, `node_utils` 88, `bmesh_utils` 56, `asset_utils` 50, `__init__` 20 | `bl_operators` y `bl_ui` (§4.3) |
| `bl_keymap_utils/` | 5 | 826 | Importar/exportar keyconfigs y la jerarquía del editor de teclas | §4.4 |
| `rna_keymap_ui`, `rna_prop_ui`, `bl_rna_utils/`, `bl_ui_utils/` | 6 | 870 | Dibujo del editor de keymaps (504), de las propiedades personalizadas (269, **31 importadores**), rutas de datos (76) y el `operator_context` de layout (21, 4 importadores) | `bl_ui`, `bl_operators` |
| `_bpy_internal/system_info/` | 3 | 423 | El informe de sistema de `wm.sysinfo` | `bl_operators/wm.py` |
| `bl_previews_utils/` | 1 | 536 | Generación de previsualizaciones en lote | `bl_operators/file.py` |
| `keyingsets_utils.py` | 1 | 296 | Sondeo y generación de los keying sets de fábrica | `keyingsets_builtins.py`, `bl_operators/anim.py` |
| `bl_app_template_utils.py` | 1 | 177 | Activar/desactivar la plantilla de aplicación | **Puente C++ vivo** (`wm_files.cc:752`) |
| `animsys_refactor.py` | 1 | 225 | Reescribir rutas de animación al renombrar propiedades | `bl_operators/anim.py:390` (`ANIM_OT_update_animated_transform_constraints`) |

### 4.2 Lo que se retira, con el motivo escrito — 30 ficheros, 10.328 líneas

| Bloque | Fich | Líneas | Importadores que viajan en el bundle | Motivo |
|---|---:|---:|---:|---|
| `bl_i18n_utils/` | 10 | 5.227 | **0** | Herramienta *offline* de extracción y fusión de `.po`. Nada la importa en ejecución. Habrá que reponer el extractor en C++ cuando los textos ya no estén en Python: es deuda anotada, no capacidad perdida |
| `bpy_extras` sin llamador (`node_shader_utils` 847, `mesh_utils` 464, `view3d_utils` 181, `wm_utils/progress_report` 160, `id_map_utils` 53) | 5 | 1.705 | **0** | Ver §4.3: son API de addons, y el C++ equivalente ya existe |
| `rna_info` 952, `rna_xml` 422, `graphviz_export` 194, `nodeitems_utils` 178, `blend_render_info` 147 | 5 | 1.893 | 0–1 | Introspección de RNA para la documentación (solo `tests/`, `tools/` y `doc/`), serializado XML de presets de tema, exportación a graphviz y el API antiguo de «categorías de nodo» (que sin addons nunca tiene categorías). Utillaje, no capacidad de usuario |
| `console_python` 368, `console_shell` 73, `bl_console_utils/` 698 | 8 | 1.139 | 2 | Un REPL de Python «en C++» es una contradicción. Se jubila con el intérprete. **`CONSOLE_OT_execute` y `copy_as_script` están en el keymap nativo: hay que quitar esos atajos a la vez** |
| `bl_app_override/` | 2 | 364 | **0 en TODO el árbol** | Armazón de «sobreescritura de aplicación» para plantillas. **Cero llamadores**, ni en `scripts/`, ni en `tests/`, ni en `tools/`, ni en C++. Código muerto heredado |

**Trampa de `console_shell.py`:** un `grep import` dice que no lo importa nadie,
y es mentira. `bl_operators/console.py:18` construye el nombre en ejecución
(`"console_" + sc.language`), así que el módulo se carga por el valor de una
propiedad. Los módulos que se eligen por nombre construido no aparecen en un
barrido de importadores: hay que buscarlos aparte.

#### Lo retirado en la segunda tanda, con su prueba

| Fichero | Líneas | Referencias en TODO el árbol | El C++ que ya hace lo mismo |
|---|---:|---|---|
| `bpy_extras/mesh_utils.py` | 464 | **una**, y es su propio nombre en el `__all__` de `bpy_extras/__init__.py` | `BLI_polyfill_calc` (teselado de n-gon), `BMW_EDGELOOP` (bucles de aristas), `uvedit_islands.cc` (islas UV) |
| `bpy_extras/id_map_utils.py` | 53 | **una**, la del `__all__` | `BKE_library_foreach_ID_link` |
| `bpy_extras/wm_utils/progress_report.py` | 160 | **ninguna**, ni siquiera en el `__all__` | Barra de progreso para importadores Python, que no habrá |
| `graphviz_export.py` | 194 | **ninguna** | Exportar el grafo de dependencias a graphviz: utillaje de desarrollo |

Barrido hecho sobre `scripts/`, `source/`, `tests/`, `tools/`, `intern/`, `doc/`,
`build_files/` y `release/`, incluyendo `.txt` y `.cmake` para cazar referencias
desde el sistema de compilación. Comprobado después: el editor arranca limpio y
`bpy_extras.__all__` queda consistente con los ficheros que hay.

**Los que NO se van todavía, y por qué**, que es la otra mitad del resultado:

- `bpy_extras/node_shader_utils.py` (847) lo importa `tests/python/modules/io_report.py`.
- `bpy_extras/view3d_utils.py` (181) lo importa `scripts/templates_py/operator_modal_view3d_raycast.py`.
- `blend_render_info.py` (147) no lo importa nadie, pero `writefile.cc:986` lo cita
  como documentación del bloque de rango de fotogramas. Retirarlo deja el
  comentario colgando y pierde una utilidad de desarrollo sin reponerla; la
  salida escrita en el inventario es reponerla como `--fl-blend-info`.
- `bl_i18n_utils/` (5.227) no lo importa nadie en ejecución, pero **es la única
  forma que hay hoy de extraer y fusionar los `.po`**. Retirarlo antes de que
  exista el extractor en C++ es perder capacidad sin sustituto, que es
  exactamente lo que la doctrina prohíbe.

### 4.2b `bl_text_utils/external_editor.py` — **migrado a C++**

Eran 54 líneas y **uno de los cinco puentes C++ → Python vivos**, el único de
ellos que implementaba capacidad de usuario: `TEXT_OT_jump_to_file_at_point`
(`text_ops.cc`) construía una expresión Python, escapaba la ruta byte a byte en
hexadecimal y llamaba a `open_external_editor(filepath, line, column)`.

Sustituto: `source/blender/editors/space_text/fl_external_editor.cc` +
`source/blender/editors/include/FL_external_editor.hh`. Hubo que reproducir
**cuatro** semánticas, y ninguna es «llamar a `system()`»:

1. Los tres mensajes de error del Python, literales (`RPT_`): sin
   `text_editor_args`, sin `$filepath` dentro, y los dos «Exception …».
2. `shlex.split()` en modo POSIX. **La trampa está en las comillas dobles:**
   dentro de `"…"` la barra invertida escapa **solo** `"` y `\`; ante cualquier
   otro carácter la barra **se queda**. Fuera de comillas escapa lo que sea. Las
   comillas simples no escapan nada. Comilla sin cerrar o barra final →
   `ValueError`, con su texto exacto.
3. `string.Template.substitute()` con cinco variables (`$filepath`, `$line` y
   `$column` en base 1, `$line0` y `$column0` en base 0), la forma `${nombre}`,
   `$$` y los errores de marcador inválido y de clave que falta. El mensaje de
   marcador inválido lleva línea y columna, y se calculan como las calcula
   Python: sobre `template[:i]` donde `i` es la posición **siguiente** al `$`,
   porque el grupo `invalid` de su expresión regular es de anchura cero.
4. `subprocess.run(args, check=True)` = `fork` + `execvp` + `waitpid`, con el
   código de salida convertido en mensaje. El ayudante que ya existía en
   `fileops_c.cc:1245` **no valía**: está dentro del camino de papelera de Linux
   (`kioclient5`/`gio`), no es un API general.

**Nota de historia, porque el commit no lo cuenta:** el registro de las dos
opciones en `source/creator/creator_args.cc` **no entró en el commit de esta
migración**. Ese fichero lo tocan todos los carriles a la vez y el índice de git
es compartido: las dos entradas acabaron dentro de `c82746c6cbf` («Selección de
objetos: los tres operadores que bloqueaban los menús, en C++»), que es de otro
carril. El árbol quedó consistente y las dos opciones funcionan, pero si alguien
busca de dónde salieron, están ahí.

**Verificado:** `--fl-dump-external-editor` y `--fl-check-external-editor`
vuelcan y comparan el `argv` resultante sin lanzar ningún proceso, contra una
línea base congelada con el `shlex` y el `string.Template` de Python
(`tests/flipendo/externaleditor/baseline-python.txt`): **4.326 casos, 4.326
idénticos, 0 distintos**, incluyendo los `repr()` de las excepciones. Y ocho
casos extremo a extremo con proceso real (`/bin/echo`, `/usr/bin/false`, un
ejecutable que no existe), con la ruta con espacios llegando como **un solo**
argumento.

**Lo que cambia a propósito, y se dice:** el mensaje de `CalledProcessError`
lleva solo el código de salida (`CalledProcessError(1)`), no la lista de
argumentos que Python metía en su `repr`. Reproducir el `repr` de una lista de
Python en un mensaje de error no aporta nada y ata el C++ a una sintaxis ajena.

### 4.3 `bpy_extras`: la sorpresa de la medición

El `INVENTARIO-PYTHON.md` proponía migrar a C++ «los de mate/geometría
(`mesh_utils`, `object_utils`, `view3d_utils`, `bmesh_utils`, `id_map_utils`,
`anim_utils`: ~1.800)». **La medición dice otra cosa, y hay que decirla:**

| Fichero | Líneas | Importadores en el bundle | El C++ equivalente |
|---|---:|---|---|
| `mesh_utils.py` | 464 | **ninguno** | `ngon_tessellate` ≈ `BLI_polyfill_calc`; `edge_loops_from_edges` ≈ `BMW_EDGELOOP`; `mesh_linked_uv_islands` ≈ `uvedit_islands.cc` |
| `view3d_utils.py` | 181 | ninguno (una plantilla y un test) | Las cuatro funciones son envoltorios de `ED_view3d_win_to_vector`, `ED_view3d_win_to_3d`, `ED_view3d_win_to_ray` y `ED_view3d_project`, **que ya son C++** |
| `id_map_utils.py` | 53 | **ninguno** | Recorrido de referencias entre IDs; `BKE_library_foreach_ID_link` |
| `wm_utils/progress_report.py` | 160 | **ninguno** | Barra de progreso para importadores Python |
| `node_shader_utils.py` | 847 | ninguno (un test) | Envoltorio PBR para importadores/exportadores de terceros |

**Conclusión, que ahorra trabajo:** ninguno de los ayudantes de mate y geometría
de `bpy_extras` necesita un helper nuevo en C++. O no lo usa nadie, o el C++ al
que envuelve ya está en el árbol. **La tarea no es «pasar `bpy_extras` a C++»:
es, cuando un carril migre el operador que lo usaba, apuntarlo al C++ que ya
existe.** Escribir helpers C++ nuevos para estas funciones sería duplicar el
árbol.

Los que sí llevan lógica que no está duplicada, y por eso están en §4.1:
`anim_utils.py` (757, horneado de acciones y transferencia de curvas; lo usa
`bl_operators/anim.py` y `keyingsets_utils.py`), `io_utils.py` (616, del que
`path_reference` y `axis_conversion` son lógica real y los mixins
`ImportHelper`/`ExportHelper` son API de addons: **se parte, no se migra entero**)
y `object_utils.object_data_add` (que sí tiene cousin C++,
`blender::ed::object::add_type` + `add_generic_get_opts`, pero con
comportamiento propio que hay que comparar antes de dar por buena la
sustitución).

### 4.4 `bl_keymap_utils`: qué está muerto de verdad y qué no

Con el keymap nativo verificado (248/248 keymaps, 3.673/3.673 atajos) parecía
que buena parte de estos 5 ficheros debía estar muerta. **Se comprobó llamante a
llamante y casi nada lo está.**

| Fichero | Líneas | Llamadores vivos | Veredicto |
|---|---:|---|---|
| `io.py` | 308 | `bl_operators/userpref.py` (`keyconfig_export_as_data`), `bl_ui/space_toolsystem_common.py` (`keymap_init_from_data`, `_init_properties_from_data`), `rna_keymap_ui.py` (`keyconfig_merge`), `bpy_extras/keyconfig_utils.py` | **Vivo.** Cae con `bl_ui` y con el operador nativo de exportar keymap |
| `keymap_hierarchy.py` | 246 | `rna_keymap_ui.py`, `bpy_extras/keyconfig_utils.py`, `bl_i18n_utils` | **Vivo.** Es el árbol del editor de teclas |
| `versioning.py` | 209 | Solo `io.py:288`, dentro de `keyconfig_import_from_data`, que **no tiene ningún llamador en el árbol** | **Vivo igualmente. Ver la trampa de abajo** |
| `platform_helpers.py` | 55 | **Ninguno.** Su único llamador era `scripts/presets/keyconfig/Blender.py`,  **que la rama de Codex ya retiró** | **Muerto.** Ya está transliterado en `fl_keymap_build.cc:192` y citado en `fl_keymap_g21.cc:175` |
| `__init__.py` | 8 | — | Con el paquete |

**La trampa, y es de las gordas:** `versioning.py` parece código muerto porque su
único llamador (`keyconfig_import_from_data`) no se llama desde ningún sitio del
árbol. Pero `keyconfig_export_as_data` (`io.py:218-224`) **escribe un fichero
`.py`** cuyo pie es literalmente:

```python
from bl_keymap_utils.io import keyconfig_import_from_data
keyconfig_import_from_data(..., keyconfig_data, **keywords)
```

O sea: **el llamador de `versioning.py` está en el disco del usuario**, en cada
keymap que haya exportado alguna vez. Retirarlo rompe importar un keymap
guardado, en silencio y sin error.

Y la consecuencia mayor: **exportar un keymap es el segundo sitio donde el editor
genera código Python**, junto a `AddPresetBase.execute` (`bl_operators/presets.py`).
Mientras los dos existan, «cero Python» es falso por construcción aunque no
quede ni un `.py` en el árbol. Los dos están en ficheros de otros carriles y
quedan anotados aquí para que no se pierdan.

---

## 5. El orden en que cae `scripts/modules`

No por tamaño: por dependencias.

1. **`rna_manual_reference.py` (4.272)** — hecho: el dato y el lector nativo ya
   están. El `.py` se borra en cuanto `wm.doc_view_manual` sea nativo; sólo
   necesita `flipendo::manual::url_from_rna_id(C, rna_id)`.
2. **`bl_app_override/` (364)** — cero llamadores en todo el árbol. **Retirado**
   en el commit «Módulos: retirar el armazón de sobreescritura…».
3. **`bl_keymap_utils/platform_helpers.py` (55)** — **Retirado** en el mismo
   commit: la rama de Codex ya se llevó `scripts/presets/keyconfig/Blender.py`,
   que era su único llamador.
4. **`bl_i18n_utils/` (5.227)** — utillaje offline; se va con la decisión de cómo
   extraer los textos del C++.
5. **`bpy_extras` sin llamador (1.705)** — se van cuando se acepte por escrito
   que el API de addons no continúa.
6. **`bl_text_utils/` (54)** — **hecho**: migrado a C++ y retirado (§4.2b). Era
   el candidato más barato a migración real que quedaba aquí, y el único puente
   C++ → Python de este directorio que implementaba capacidad de usuario.
7. **`bgui/` (2.391) + `gpu_extras/` (190)** — capacidad del Player, no del
   editor. Es un agujero abierto: sin CPython, un juego distribuido **no tiene
   sistema de interfaz de serie**.
8. **Lo demás de (b)** — cae detrás de `bl_ui` y de `bl_operators`, que son de
   otros carriles.
9. **El pegamento (6.961)** — el último día, de golpe.

---

## 6. Lo que este documento corrige del `INVENTARIO-PYTHON.md`

El inventario es de esta misma noche y es bueno; estas cuatro cosas se midieron
más fino y mandan sobre él:

1. **`scripts/modules` son 97 ficheros y 30.165 líneas**, no 99 y 30.788.
2. **Los ayudantes de mate/geometría de `bpy_extras` no hay que migrarlos**: el
   C++ ya existe (§4.3). El inventario presupuestaba ~1.800 líneas de migración
   que no hacen falta.
3. **`bl_app_override/` (364) tiene cero llamadores**, no «solo lo usan las
   plantillas de aplicación».
4. **`_bpy_internal/grease_pencil/` (405) es pegamento, no capacidad**: es el
   azúcar que hace funcionar `drawing.strokes[n].points[i]` desde Python sobre
   el API de `attributes`, que ya es C++. Cae con el intérprete, no se migra.
5. **`bl_keymap_utils` no está muerto** pese al keymap nativo: solo lo está
   `platform_helpers.py` (55 de 826), y por una razón que no es el keymap sino
   la retirada del preset `Blender.py`.

Y añade uno que no estaba en ninguna política: **exportar un keymap escribe
Python en el disco del usuario** (§4.4).

---

*Método: barridos de `grep` e importadores sobre el árbol de trabajo, no lectura.
Las cifras de las tres categorías suman exactamente 30.165 y 97, y esa
comprobación es parte de la medición.*
