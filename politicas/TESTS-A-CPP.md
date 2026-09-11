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
cd dev/build && ctest -N | grep -c 'Test *#'         -> 345
grep WITH_GTESTS dev/build/CMakeCache.txt            -> WITH_GTESTS:BOOL=OFF
```

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

### Fase 0 — Encender la red que ya existe (coste ~0, hay que hacerlo ya)

`WITH_GTESTS=OFF`. Hay **187 ficheros `*_test.cc`** en `source/` que no se
compilan. Es cobertura C++ ya escrita, ya pagada, apagada por un interruptor.
Antes de escribir un solo test nuevo: `WITH_GTESTS=ON`, ver qué falla, arreglarlo
o anotarlo. Es el mejor rendimiento por hora de toda esta lista.

**Salvedad medida:** encenderlo alarga la build y esta noche hay cuatro carriles
compartiendo el cerrojo de `nb`. Se deja para cuando la noche cierre, no ahora.

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

1. **No hay red C++ encendida.** `WITH_GTESTS=OFF`: los 187 `*_test.cc` no se
   compilan. Cualquier frase del tipo «eso ya lo cubre un gtest» es falsa hoy.
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

- No se borra `tests/python`. Hoy son **345 tests** registrados en `ctest`, todos
  Python, y **son la única red que hay**: los gtest están apagados.
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
