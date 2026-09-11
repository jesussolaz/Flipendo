# `tests/python` a C++ — plan de reposición de la red de seguridad

> Medido el 2026-09-11 sobre `flipendo-main`, con `ctest -N` del árbol de
> construcción real (`dev/build`), no por lectura del `CMakeLists.txt`.
>
> **Este documento existe porque `tests/python` NO se borra.** El inventario
> (`INVENTARIO-PYTHON.md §2.10`) lo marca **RET** por la regla 5d de la doctrina
> («los tests nuevos, en C++»), y añade el riesgo: «aquí hay cobertura real del
> núcleo C++; retirarla sin reponerla **baja la red de seguridad del fork**».
> Eso es exactamente lo que no se va a hacer. Borrar la única red que hay no es
> progreso: es quedarse ciego. Se repone primero, se retira después, suite a
> suite.

---

## 0. La foto, medida

```
git ls-files 'tests/python/**/*.py' | wc -l          -> 232
… | xargs wc -l | tail -1                            -> 37.769
cd dev/build && ctest -N | grep -c 'Test *#'         -> 345   (antes de §6)
grep WITH_GTESTS dev/build/CMakeCache.txt            -> WITH_GTESTS:BOOL=OFF
```

> Las dos últimas cifras son de las 03:30. A las 03:57 se encendió `WITH_GTESTS`
> y pasaron a **407** y **`ON`**: ver §6, que es donde está la foto real de la
> red C++.

Y aquí está el primer hallazgo, que cambia el tamaño del problema:

| | Ficheros | Líneas |
|---|---:|---:|
| `tests/python` en total | 232 | 37.769 |
| **Nombrados por algún `CMakeLists.txt`** | **80** | **23.183** |
| Nunca nombrados por nadie | 152 | 14.586 |

**El 39 % de los ficheros produce el 100 % de los 345 tests registrados.** Los
otros 152 son módulos de apoyo (`modules/mesh_test.py`, `modules/render_report.py`,
`modules/io_report.py`…, que sí los importan los primeros) y, sobre todo, suites
enteras **desconectadas**:

- **`view_layer/` — 127 ficheros, 8.191 líneas, DESACTIVADA.**
  `tests/python/CMakeLists.txt:1351` tiene `# add_subdirectory(view_layer)`
  comentado, con el motivo escrito encima: *«TODO: disabled for now after
  collection unification»*. **Esta suite no corre desde upstream, no desde
  Flipendo.** Es el 22 % de las líneas de `tests/python` y cubre exactamente
  cero.
- **`collada/` — 2 ficheros, DESACTIVADA.** `tests/python/CMakeLists.txt:1341-1342`
  tiene los dos `collada_test(...)` comentados.
- **`ui_simulate/` — 5 ficheros, 1.707 líneas, NO REGISTRADA.** Ningún
  `CMakeLists` la nombra. Es el simulador de eventos de interfaz
  (`test_undo.py`, 1.033 líneas), y se lanza a mano.
- `bl_rst_completeness.py`, `bl_mesh_modifiers.py`, `bl_rna_defaults.py`,
  `pep8.py`, `rna_array.py`, `batch_import.py`, `gpu_info.py`… tampoco los
  registra nadie.

**Conclusión del recuento: la red real son 345 tests que salen de 80 ficheros y
23.183 líneas.** Planificar sobre 37.769 es planificar sobre un 38 % de aire.

### Qué cubren esos 345

| Grupo | Tests | Qué mira |
|---|---:|---|
| `geo_node_*` | 171 + 30 + 8 + 6 + 6 + 3 + 3 = **227** | Nodos de geometría, comparando mallas contra `.blend` de referencia |
| `blendfile_*` | 36 | Lectura/escritura de `.blend`, enlaces de biblioteca, *overrides* |
| `bl_*` | 16 | Animación, *drivers*, curvas F, acciones, `rigify`, esculpido, brochas |
| `script_*` | 15 | Keymap, carga de addons/módulos, API de `bpy`, operadores |
| `compositor_*` | 12 | Salida a fichero del compositor (CPU, OpenGL, Metal, Vulkan) |
| `io_*` | 9 | Alembic, USD, Collada |
| `sequencer_*`, `physics_*` | 12 | Render de secuenciador; telas, blandos, océano, partículas |
| `object_*`, `bmesh_*`, `modifiers`, `operators`, `id_management`, `imbuf_*` | 9 | Conversión de objetos, bevel/boolean/split, modificadores, operadores |

**Todos son Python.** `WITH_GTESTS` está en **OFF** en el árbol de construcción
real, así que los 187 ficheros `*_test.cc` que hay en `source/` **ni se compilan**.
La red C++ nativa de Blender está apagada en esta build. Es el segundo hallazgo y
es importante: no se puede decir «ya lo cubre el gtest» de nada, porque hoy no
corre ningún gtest.

---

## 1. Lo que YA está repuesto en C++ esta noche

El proyecto no parte de cero. El binario lleva **40 opciones `--fl-*`** escritas en
C++, con sus líneas base congeladas en `tests/flipendo/` (46 ficheros, 210.916
líneas de volcado). Comprobado con `Blender --help | grep -o '\-\-fl-[a-z-]*'`.

| Arnés C++ | Línea base | Qué suite de `tests/python` deja sin objeto |
|---|---|---|
| `--fl-dump-keymap`, `--fl-dump-keymap-native`, `--fl-check-keymap`, `--fl-selftest-keyconfig` | `tests/flipendo/keymap/baseline-python.txt` | **`script_load_keymap`** (`bl_keymap_completeness.py`) y **`script_validate_keymap`** (`bl_keymap_validate.py`). Verificado 3.673/3.673 atajos y 248/248 keymaps |
| `--fl-dump-tools`, `--fl-check-tools`, `--fl-dump-tool-activation`, `--fl-dump-tool-queries`, `--fl-dump-toolbar-keymaps` | `tests/flipendo/toolsystem/` (416/416, 912/912, 14/14, 819 capturas) | Nada de `tests/python`: **cobertura nueva**, el sistema de herramientas no la tenía |
| `--fl-dump-operators`, `--fl-dump-optypes`, `--fl-check-optypes` | `tests/flipendo/optypes/`, `tests/flipendo/operators/` | Parte de **`script_run_operators`** (`bl_run_operators.py`) y de **`operators`** |
| `--fl-dump-ui`, `--fl-dump-ui-layout`, `--fl-check-ui` | `tests/flipendo/ui/baseline-python{,-layout}.txt` | Cobertura nueva. También sustituye a `tools/utils_api/bpy_introspect_ui.py`, ya retirado |
| `--fl-dump-manual`, `--fl-check-manual`, `--fl-convert-manual-reference` | `tests/flipendo/manual/` | Nada previo |
| `--fl-check-presets`, `--fl-convert-presets`, `--fl-dump-preset-panel` | `tests/flipendo/presets/` | Nada previo |
| `--fl-selftest-mesh-ops`, `--fl-check-mesh-ops` | `tests/flipendo/meshops/` | Parte de `bmesh_*` |
| `--fl-selftest-object-ops`, `--fl-selftest-object-select`, `--fl-check-object-select` | `tests/flipendo/objectops/`, `objectselect/` | Parte de `object_*` |
| `--fl-check-rigidbody-ops`, `--fl-selftest-rigidbody-ops` | `tests/flipendo/rigidbody/` | Parte de `physics_*` |
| `--fl-check-mirror-uv`, `--fl-selftest-mirror-uv` | `tests/flipendo/mirroruv/` | Nada previo |
| `--fl-dump-external-editor`, `--fl-check-external-editor` | `tests/flipendo/externaleditor/` | Nada previo |
| `--fl-selftest-wm-batch-rename`, `--fl-selftest-wm-owner-ops`, `--fl-selftest-wm-properties-edit`, `--fl-selftest-wm-property-ops`, `--fl-selftest-wm-system-ops`, `--fl-selftest-context-ops` | `tests/flipendo/operators/*.txt` | Parte de `operators` |
| `--fl-selftest-numinput` | `tests/flipendo/numinput/` | Nada previo |
| `--fl-dump-runtime` | — | Nada previo |

**Ya cubiertas de verdad: las dos de keymap** (`script_load_keymap`,
`script_validate_keymap`), medidas al atajo. De las demás hay solapamiento
parcial, no equivalencia: un `--fl-selftest-object-ops` no es lo mismo que
`object_conversion` sobre un `.blend` de referencia.

---

## 2. El plan, por orden

El criterio de orden no es el tamaño: es **cuánto duele que se rompa sin que nadie
se entere**, y **cuánto cuesta reponerlo**. Lo barato y peligroso, primero.

### Fase 0 — Encender la red que ya existe — **HECHO el 2026-09-11 a las 03:57**

Estaba en `OFF`. **Ya está en `ON`** en `dev/build`, y la foto real está en §6.
No hay que volver a encenderlo: hay que arreglar lo que salió.

### Fase 1 — Retirar lo que ya no cubre nada (coste 0, ganancia 14.586 líneas)

Tres suites que **no corren** y por tanto no son red de nada:

| Suite | Ficheros | Líneas | Por qué se puede retirar sin reponer |
|---|---:|---:|---|
| `view_layer/` | 127 | 8.191 | Desactivada en `CMakeLists.txt:1351` con su motivo escrito. No la ejecuta nadie |
| `collada/` | 2 | 432 | Los dos `collada_test()` comentados en `:1341-1342` |
| `ui_simulate/` | 5 | 1.707 | Sin registro en ningún CMake |

**Pero no se retiran hoy, y el motivo es honesto:** `view_layer/` es la
descripción escrita de cómo debe comportarse la unificación de colecciones. Está
apagada porque el comportamiento cambió, no porque sobre. Antes de tirarla hay
que decidir si se repone en C++ (es evaluación de dependencias: un gtest natural)
o si se acepta perderla. **Esa decisión no es de este carril**, y se deja aquí
escrita para quien lleve el depsgraph.

### Fase 2 — Lo que el C++ ya cubre: retirar con la medición delante (coste bajo)

Solo dos suites están **verificadas idénticas** hoy:

1. `bl_keymap_completeness.py` + `bl_keymap_validate.py` (~600 líneas) ->
   `--fl-check-keymap`, 3.673/3.673 y 248/248. **Se pueden retirar en cuanto se
   quite su entrada de `tests/python/CMakeLists.txt` en el mismo cambio.**
   Trampa: `check_mypy_config.py` nombra `tests/python/bl_keymap_validate.py` en
   sus `PATHS`; hay que quitarlo a la vez o `make check_mypy` avisa.

El resto **no** está cubierto todavía, por mucho que lo parezca. Un
`--fl-selftest-*` prueba que un operador nativo hace lo que hacía el Python; no
prueba que el resultado sobre una escena real siga siendo el mismo.

### Fase 3 — Lo caro y lo que de verdad protege (400-800 h·p)

Por orden de valor:

| # | Qué reponer | Cubre hoy | Cómo, en C++ | h·p |
|---|---|---:|---|---:|
| 1 | **`blendfile_*`** (36 tests): `bl_blendfile_io`, `_liblink`, `_relationships`, `_library_overrides` | 36 | gtest sobre `BLO_read`/`BLO_write` con `.blend` de `tests/files`. Es el formato de fichero: si se rompe, se pierden datos del usuario. **Lo primero** | 80-140 |
| 2 | **`geo_node_*`** (227 tests) | 227 | Son el 66 % de los tests y comparan mallas contra `.blend` de referencia. El arnés ya es de datos: hace falta un corredor C++ que cargue el `.blend`, evalúe el árbol de nodos y compare la malla. **Trampa medida esta noche (lección de las 03:50): el cálculo de normales NO es determinista.** Cualquier comparación de mallas tiene que cuantizar las normales o declarar tolerancia, o perseguirá fantasmas | 120-200 |
| 3 | **`script_run_operators`** (`bl_run_operators.py`) | 1 | Ejecuta todos los operadores buscando caídas. El equivalente nativo es recorrer `WM_operatortype_iter()` y llamar a cada `poll`+`exec`. `--fl-dump-optypes` ya enumera; falta ejecutar | 40-70 |
| 4 | **`physics_*`, `object_*`, `modifiers`, `bmesh_*`** | 19 | Mismo patrón que (2): cargar `.blend`, aplicar, comparar con tolerancia | 80-140 |
| 5 | **`bl_animation_*`** (7) | 7 | Curvas F, acciones, NLA, *bake*, *drivers*. Comparación numérica, tolerancia declarada | 50-90 |
| 6 | **`io_*`** (9: Alembic, USD, Collada) | 9 | Depende de si Flipendo conserva esos formatos. **Decisión previa pendiente**, no es este carril | 30-90 |
| 7 | **`compositor_*`, `sequencer_*`, `imbuf_*`** (20) | 20 | Comparación de imágenes; ya existe infraestructura de comparación en `tests/` | 40-70 |
| 8 | **`script_pyapi_*`** (9) | 9 | **NO se reponen.** Prueban el API de Python: se jubilan con el intérprete. Es la única categoría que se puede tirar sin deuda |

### Fase 4 — La regla permanente

Ninguna suite de `tests/python` se retira sin que su sustituto C++ **exista, esté
registrado en `ctest` y haya reproducido la línea base**. El patrón está
establecido y funciona: congelar la línea base con el Python todavía vivo, y
exigir al C++ que la reproduzca byte a byte. Los 3.673/3.673 del keymap y los
912/912 del sistema de herramientas son la prueba de que el método aguanta.

---

## 3. Trampas, para quien ejecute este plan

1. **La red C++ está encendida pero rota por la mitad** (§6). Compilan los 187
   `*_test.cc` y pasan 448 casos, pero el corredor muere con SIGSEGV y 255 casos
   (52 suites) no se ejecutan nunca. Antes de decir «eso ya lo cubre un gtest»,
   hay que comprobar que ese gtest está entre los 449 que llegan a correr.
2. **Un tercio de `tests/python` no corre.** `view_layer/` (127 ficheros),
   `collada/` y `ui_simulate/` están desactivadas o sin registrar. Contarlas como
   cobertura infla la sensación de seguridad.
3. **Las normales de vértice no son deterministas** (medido esta noche). Toda
   comparación de mallas tiene que cuantizarlas o declarar tolerancia. Y antes de
   dar por buena una diferencia: ejecutar la línea base dos veces con el mismo
   binario. Si no se reproduce a sí misma tres veces seguidas, no es línea base.
4. **`tests/flipendo/toolsystem/` conserva arneses en Python a propósito.** Son la
   prueba de que el C++ es idéntico. Retirarlos antes de tiempo destruye la
   evidencia; se van con `space_toolsystem_*.py`.
5. **`tests/python/CMakeLists.txt` y `check_mypy_config.py` nombran ficheros por
   ruta.** Retirar un test y dejar su entrada es la versión inversa de la lección
   de las 01:05: el fichero y su lista viajan en el mismo cambio.
6. **Los tests de `bl_pkg`** (`scripts/addons_core/bl_pkg/tests/`, 2.681 líneas,
   6 ficheros) son la única red de `cli/blender_ext.py`, que se conserva (ver
   `EXTENSIONES-Y-EL-INTERPRETE.md §3`). Se retiran con él, no antes.

---

## 4. Resumen para el que tenga prisa

- No se borra `tests/python`. Eran **345 tests** registrados en `ctest`, todos
  Python, y eran **la única red que había**: los gtest estaban apagados.
  **Ya no lo están** (§6): `WITH_GTESTS=ON`, 407 tests registrados, 448 casos C++
  pasando… y el corredor cayéndose con SIGSEGV a mitad, dejando 255 sin ejecutar.
- De sus 37.769 líneas, **14.586 ya no cubren nada** (suites desactivadas o sin
  registrar). Ahí hay retirada barata, pero `view_layer/` merece una decisión
  antes de tirarla.
- **Ya repuesto y verificado: las dos suites de keymap.** El resto de los 40
  arneses `--fl-*` añade cobertura nueva o solapa parcialmente; no equivale.
- Lo siguiente, por este orden: **encender `WITH_GTESTS`** (coste ~0),
  **`blendfile_*`** (el formato de fichero, lo que más duele perder) y
  **`geo_node_*`** (el 66 % de los tests).
- Lo único que se puede jubilar sin deuda son los **9 `script_pyapi_*`**: prueban
  el intérprete que se va.

---

## 5. El bundle instalado no se limpiaba — **arreglado el 2026-09-11**

> Medido a las 03:50, arreglado a las 04:15. **Afectaba a la verificación de
> todos los carriles**, y por eso está aquí y no en una nota al pie.

### El defecto

`cmake --install` **copia, pero no borra**. Cuando un carril migraba un `.py` a
C++ y lo retiraba del árbol, el fichero **seguía dentro del bundle**, en
`sys.path`, y se seguía importando.

| Medida (03:50) | Antes |
|---|---:|
| `.py` huérfanos en `Blender.app` | **406** |
| `.py` huérfanos en `Blenderplayer.app` | **578** |
| Módulos retirados esa noche todavía importables | **8 de 8** |

Los ocho, comprobados importando desde el binario: `rna_manual_reference`,
`graphviz_export`, `bl_app_override`, `bl_text_utils.external_editor`,
`bl_keymap_utils.keymap_from_toolbar`, `bpy.utils.toolsystem`,
`bpy_extras.mesh_utils`, `bpy_extras.id_map_utils`.

**El riesgo era el peor que hay en una migración:** retirar el Python, compilar,
arrancar, ver que todo sigue funcionando y dar la migración por buena — cuando lo
que funciona es el Python viejo que quedó en el bundle. La evidencia diría
«idéntico» porque literalmente es el mismo código.

### La causa, y una sorpresa

El Player **ya tenía** un paso de limpieza, desde upstream, con el motivo
correcto escrito al lado:

```cmake
# important to make a clean  install each time, else old scripts get loaded.
install(CODE "file(REMOVE_RECURSE ${PLAYER_TARGETDIR_VER})")
```

**Nunca borró nada.** `${PLAYER_TARGETDIR_VER}` es una ruta **relativa**
(`Blenderplayer.app/Contents/Resources/4.5`) y `file(REMOVE_RECURSE)` la resolvía
contra el directorio de trabajo del script de instalación, es decir contra
`dev/build/Blenderplayer.app/…`, que no existe. Upstream diagnosticó el problema
y escribió una cura que no curaba. Blender.app no tenía ni eso.

### El arreglo

Opción `WITH_INSTALL_PRUNE_SCRIPTS` (por defecto **ON**), declarada en
`CMakeLists.txt`, con un paso de poda en los dos bundles:

- `source/creator/CMakeLists.txt`, **fuera de `if(WITH_PYTHON)`** y antes de
  todas las reglas que escriben en `scripts/`. Va fuera del `if` a propósito: los
  presets y las plantillas de aplicación se instalan en `scripts/` **también sin
  intérprete**, así que la poda tiene que valer en `dev/build` y en
  `dev/build-nopy`.
- `source/blenderplayer/CMakeLists.txt`, sustituyendo la limpieza rota.

**Se poda solo `scripts/`.** Ni `datafiles/`, ni `python/`, ni `extensions/`:
`extensions/system/` lo pueden poblar «users or administrators» (lo dice el
comentario de su propia regla de instalación), y borrarlo sería tirar algo que no
viene del árbol. Comprobado además que **MPFB2 no está en el bundle** (0
coincidencias): vive en `~/Library/Application Support/UPBGE/4.5/extensions/
user_default/mpfb`, que la poda no toca.

### Verificado

| | Antes | Después |
|---|---:|---:|
| `.py` huérfanos en `dev/build/Blender.app` | 406 | **0** (de 292) |
| `.py` huérfanos en `dev/build/Blenderplayer.app` | 578 | **0** (de 298) |
| `.py` huérfanos en `dev/build-nopy/Blender.app` | 0 | **0** (de 0) |
| Módulos retirados importables desde el binario | 8/8 | **0/8** |

La poda imprime la ruta **absoluta** que limpia, y se la vio disparar en los tres
destinos y en los logs de otros tres carriles:

```
-- Flipendo: podando scripts instalados en …/dev/build/bin/Blender.app/…/4.5/scripts
-- Flipendo: podando scripts instalados en …/dev/build/bin/Blenderplayer.app/…/4.5/scripts
-- Flipendo: podando scripts instalados en …/dev/build-nopy/bin/Blender.app/…/4.5/scripts
```

`nb install` rc=0. Arranque limpio tras la poda, tres veces en `--background` y
una en modo gráfico: 248 keymaps, panel de Extensiones relleno, 32 operadores
`extensions.*`, cero tracebacks.

### Efecto colateral que hay que conocer

La poda **se llevó `bl_ui/space_toolsystem_common.py` del bundle**, que era
huérfano y que importan los **siete** arneses `tests/flipendo/toolsystem/*_gui.py`.
Se recupera del histórico en un comando:

```
git show 231333893fc^:scripts/startup/bl_ui/space_toolsystem_common.py \
  > <bundle>/4.5/scripts/startup/bl_ui/space_toolsystem_common.py
```

Y si hiciera falta desactivarla del todo:
`cmake -S dev/upbge -B dev/build -DWITH_INSTALL_PRUNE_SCRIPTS=OFF`.

### La regla que sale de aquí

Una migración solo está verificada si se ha comprobado **con el bundle limpio**.
«Compila y arranca» no probaba nada mientras el `.py` retirado siguiera ahí.
Ahora lo prueba.

---

## 6. La foto real de los gtests, encendidos el 2026-09-11 a las 03:57

Se activaron así, tomando antes el mismo cerrojo que usa `nb` para no pisar a los
seis carriles que compilaban a la vez:

```
cmake -S dev/upbge -B dev/build -DWITH_GTESTS=ON     # configure rc=0, 17 s
~/Flipendo/dev/noche/nb blender_test                 # build rc=0, 298 pasos
```

**Compilan los 187.** Ni un error. El enlace produce
`dev/build/bin/tests/blender_test`, 197 MB.

### Qué añade al `ctest`

| | Antes | Después |
|---|---:|---:|
| Tests registrados en `ctest -N` | 345 | **407** |

Los **62 nuevos** son: `atomic`, `guardedalloc`, 32 `libmv_*`,
`intern_opensubdiv`, y 27 entradas que corresponden a las suites dentro del
binario único (`BLI`, `blenkernel`, `bmesh`, `depsgraph`, `gpu`, `nodes`,
`animrig`, `windowmanager`, `interface`, `blenloader`, `blenfont`, `imbuf`,
`function`, `asset_system`, `bf_geometry_tests`, `editor_animation`,
`editor_curves`, `editor_grease_pencil`, `editor_sculpt_paint`…).

### Qué pasa al ejecutarlo — y aquí está lo importante

```
./blender_test --test-assets-dir dev/upbge/tests/files
```

| Medida | Cifra |
|---|---:|
| Casos que el binario declara | **704**, en 117 suites |
| Casos que llegan a ejecutarse | **449** |
| **Pasan** | **448** |
| **Fallan** | **1** |
| **Nunca se ejecutan** | **255** (52 suites) |
| Código de salida | **139 = SIGSEGV** |

**El corredor se cae a la mitad**, y no por un solo motivo: son **al menos cuatro
bloqueadores encadenados** (§7). El primero, después de
`LibRemapTest.never_null_usage_storage_requested_on_remap`, imprime:

```
Memoryblock source/blender/blenlib/BLI_vector.hh:1126: double free
Memoryblock free: pointer not in memlist
```

y muere con SIGSEGV. Por eso 255 casos no se ejecutan nunca. Las 52 suites que
se quedan sin correr incluyen cosas que a este proyecto le importan:
`GPUMetalTest`, `GPUMetalWorkaroundsTest`, `NodeTest`, `BlendfileLoadingTest`,
`BMainMergeTest`, `VolumeTest`, `bmesh_core`, `deg_builder_rna`, `field`,
`multi_function`, `lazy_function`, `imbuf_scaling`, `imbuf_transform`, los siete
`blf_*`, los exportadores `OBJ/PLY/STL/USD/Alembic` y `path_templates`.

El único fallo real es `AssetCatalogTest.read_write_unicode_filepath`.

### Trampa que costó una vuelta: el flag de los datos

Sin `--test-assets-dir`, **62 tests de `asset_system` fallan** y `ImageTest.multilayer`
también, todos con el mismo mensaje:

```
tests/gtests/testing/testing_main.cc:17: Failure
Pass the flag --test-assets-dir and point to the tests/files directory.
```

No son fallos: es un argumento que falta. Con el flag pasan los 62. **Quien
ejecute `blender_test` a mano tiene que pasarlo**; `ctest` lo pasa solo.

### Qué hay que hacer con esto, y qué NO se hizo

**No se arregló nada de lo que falla**, a propósito: no da tiempo y no es de este
carril. Lo valioso es la foto y que quede encendida. Por orden:

1. **El SIGSEGV del corredor** es lo primero: mientras esté, 255 casos (el 36 %)
   son invisibles y la red vale un tercio menos de lo que parece. **Ya está
   localizado**, ver §7.
2. `AssetCatalogTest.read_write_unicode_filepath`, el único fallo real.
3. Ejecutar las 52 suites que no se alcanzan, con `--gtest_filter`, para saber
   cuántas de esas 255 pasan de verdad. **Hasta entonces, la cifra honesta de
   cobertura C++ verificada es 448, no 704.**

### Y una consecuencia para el resto del plan

La §2 decía «encender `WITH_GTESTS` es la mejor relación valor/hora». Sigue
siendo cierto —448 comprobaciones que antes no corría nadie—, pero con el
matiz que da la medición: **la red C++ que creíamos tener entera está rota por
la mitad**, y eso refuerza, no debilita, la decisión de no borrar `tests/python`.

---

## 7. El SIGSEGV del corredor, localizado

Lo pidió el jefe de proyecto: dejarlo escrito con precisión, porque es trabajo de
la próxima noche. Esto es lo que se midió.

### Dónde está

**`source/blender/blenkernel/intern/lib_remap_test.cc:70-93`**, la clase
`LibRemapTest` y sus `SetUpTestSuite()` / `TearDownTestSuite()`.

### Cómo se acotó

Ejecutando la suite sola:

```
./blender_test --test-assets-dir … --gtest_filter='LibRemapTest.*'
  [  PASSED  ] 11 tests.
  Error: Not freed memory blocks: 4294967293, total unfreed memory 0.000000 MB
  EXIT=134   (SIGABRT)
```

**Los 11 casos pasan** y aun así el proceso aborta. Y después, caso por caso, los
once por separado:

```
LibRemapTest.embedded_ids_can_not_be_remapped              EXIT=134  -> 4294967293
LibRemapTest.embedded_ids_can_not_be_deleted               EXIT=134  -> 4294967293
LibRemapTest.delete_when_remap_to_self_not_allowed         EXIT=134  -> 4294967293
LibRemapTest.users_are_decreased_when_not_skipping_never_null  EXIT=134  -> 4294967293
LibRemapTest.users_are_same_when_skipping_never_null       EXIT=134  -> 4294967293
LibRemapTest.do_not_delete_when_cannot_unset               EXIT=134  -> 4294967293
LibRemapTest.force_never_null_usage                        EXIT=134  -> 4294967293
LibRemapTest.never_null_usage_flag_not_requested_on_delete EXIT=134  -> 4294967293
LibRemapTest.never_null_usage_storage_requested_on_delete  EXIT=134  -> 4294967293
LibRemapTest.never_null_usage_flag_not_requested_on_remap  EXIT=134  -> 4294967293
LibRemapTest.never_null_usage_storage_requested_on_remap   EXIT=134  -> 4294967293
```

**Los once dan exactamente lo mismo.** Por tanto **no es ningún caso: es el
fixture.**

### Qué significa `4294967293`

Es `2^32 − 3`, es decir **−3 leído como sin signo**: el contador de bloques de
`guardedalloc` acabó **negativo**. Hubo **tres liberaciones más que reservas**.
Eso es un doble free, y encaja con el mensaje que da la ejecución completa:
`Memoryblock source/blender/blenlib/BLI_vector.hh:1126: double free` (esa línea
es el `allocator_.allocate(...)` del crecimiento de `blender::Vector`, es decir
el bloque que se libera dos veces es un búfer de `Vector`).

### La hipótesis que hay que comprobar primero

`SetUpTestSuite()` llama a `CLG_init`, `BKE_idtype_init`, `RNA_init`,
`node_system_init`, `BKE_appdir_init`, `IMB_init`, `BKE_materials_init`; y
`TearDownTestSuite()` los cierra todos, más `GHOST_DisposeSystemPaths()`.

Son **inicializaciones globales de proceso**. En el corredor único
(`WITH_TESTS_SINGLE_BINARY=ON`, que es como está configurado) ese estado lo
comparten las 117 suites: `LibRemapTest` desmonta subsistemas que otras suites ya
habían montado —o que se vuelven a montar después—, y por eso el contador queda
en −3 y el proceso muere **en cuanto termina esa suite**, arrastrando a las 52
suites que venían detrás (la siguiente en el orden de registro es
`BMainMergeTest`).

Dicho de otro modo: **el fallo probablemente no está en el código que se prueba,
sino en que una suite hace init/exit global dentro de un binario compartido.**

### No es uno: son al menos CUATRO bloqueadores encadenados

Esto es lo que más importa saber antes de empezar. Excluyendo el primero aparece
el segundo, y así. Medido:

| Filtro | Casos ejecutados | Pasan | Salida | Dónde se para |
|---|---:|---:|---:|---|
| *(ninguno)* | 449 | 448 | 139 | tras `LibRemapTest` (aborta al desmontar) |
| `-LibRemapTest.*` | **494** | 493 | 139 | **`BlendfileLoadingTest.CanaryTest`** |
| `…:-BlendfileLoadingTest.*` | **573** | **572** | 139 | **`NodeTest.tree_iterator_1mat_3scenes`** |
| `…:-NodeTest.*` | 570 | 569 | 139 | **`AbstractHierarchyIteratorTest.ExportSubsetTest`** |

Los tres últimos son distintos del primero: **el caso empieza (`[ RUN ]`) y nunca
termina** — no imprime ni `[ OK ]` ni `[ FAILED ]`. Eso es una caída dentro de la
prueba, no un desmontaje sucio.

**La mejor cifra alcanzada en una sola ejecución es 573 ejecutados / 572 pasando**
(excluyendo `LibRemapTest` y `BlendfileLoadingTest`). El único fallo real sigue
siendo `AssetCatalogTest.read_write_unicode_filepath`, en todas las
combinaciones.

Y un dato más: `--gtest_repeat=2` sobre `LibRemapTest` no da −6, da **EXIT=138**
(otra señal distinta). Es decir, repetir el init/exit global empeora el estado en
vez de acumularlo linealmente, lo que refuerza que el problema es estado global
compartido, no una fuga contable.

### Por dónde empezar

1. `LibRemapTest`: mover las inicializaciones globales de `SetUpTestSuite` al
   entorno global del corredor (`tests/gtests/runner/blender_test.cc`), o aislar
   la suite en su propio ejecutable. `WITH_TESTS_SINGLE_BINARY=OFF` lo hace para
   todas: más caro de enlazar, pero aísla los cuatro bloqueadores de golpe y
   probablemente sea la forma más rápida de ver la foto completa.
2. Con eso, volver a medir: la cifra real de cobertura C++ está en algún punto
   entre **572** y **703**, y hoy nadie sabe dónde.
3. Mientras tanto, para trabajar:
   `./blender_test --test-assets-dir … --gtest_filter='-LibRemapTest.*:BlendfileLoadingTest.*'`
   da 573 casos útiles.
