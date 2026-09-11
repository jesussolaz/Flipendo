# Estado de Flipendo — medido, no escrito

> **FICHERO GENERADO. NO SE EDITA A MANO.** Cualquier cambio que escribas aquí
> lo borra la siguiente regeneración. Se genera con
> `tools/flipendo_metrics/flipendo_metrics.cpp`, que es C++ y mide el árbol:
> ninguna cifra de este documento está copiada de otro documento.
>
> **Medido el 2026-09-12 00:07 sobre el commit `fe49217aabe`** (2026-09-12 00:02 — «Interfaz: el volcado y el comprobador daban resultados distintos sobre el mismo binario»).
>
> Regenerar:
> ```sh
> c++ -std=c++17 -O2 -o ~/Flipendo/bin/flipendo-metrics \
>     ~/Flipendo/dev/upbge/tools/flipendo_metrics/flipendo_metrics.cpp
> flipendo-metrics --estado ~/Flipendo/dev/upbge
> ```

Por qué existe: `politicas/METRICAS.md` publicó el 8 de septiembre una cifra de
Python medida el 5 —entre medias habían caído 109.442 líneas sin registrarse— y el
backlog describía zonas que el árbol ya no tenía. Un documento a mano envejece en
horas cuando hay siete carriles trabajando. La solución no es escribir mejor: es
**generar**.

---

## 1. Cómo se mide cada cifra

- **A un commit, nunca al árbol de trabajo.** Con varios carriles vivos, medir el
  disco no es reproducible ni por quien lo hizo: se ven ficheros sin versionar y no
  se ve lo que otro acaba de borrar. El árbol se lee con `git ls-tree -r` y un solo
  `git cat-file --batch`, al commit de la cabecera.
- **Una línea es un `\n`**, como `wc -l`.
- **Propio contra vendorizado**: los prefijos no están escritos en la herramienta,
  se leen de `.gitattributes` (todo patrón marcado `linguist-vendored`). Hoy son: `extern/`, `lib/`.
- **Compilado o no** sale de `build.ninja` y `CMakeCache.txt` del árbol de
  compilación, no de lo que diga una política.
- **La evolución** sale del historial de git: se mide de verdad cada commit
  muestreado, no se copia de ninguna tabla.
- **La batería** se ejecuta: cada verificador del binario, con su línea base, y el
  código de salida es el veredicto.

Ficheros versionados al commit medido: **19.903**. De ellos, **11.624 son de código**:
9.262 propios y 2.362 vendorizados. El resto (8.279) son datos, assets, textos y
configuración, que por doctrina no son código.

---

## 2. Composición: código propio

| Lenguaje | Ficheros | Líneas | % |
|---|---:|---:|---:|
| C++ | 4.345 | 2.574.866 | 75,3 % |
| .hh | 1.854 | 292.944 | 8,6 % |
| .hpp | 306 | 36.846 | 1,1 % |
| .h heredada | 1.388 | 248.731 | 7,3 % |
| Python | 611 | 189.766 | 5,6 % |
| GLSL | 745 | 68.391 | 2,0 % |
| Objective-C++ | 8 | 5.060 | 0,1 % |
| MSL | 4 | 1.722 | 0,1 % |
| Metal | 1 | 832 | 0,0 % |
| **TOTAL propio** | **9.262** | **3.419.158** | 100 % |

- Familia C/C++ contando las `.h` heredadas: **3.153.387 → 92,2 %**.
- C++ en sentido estricto (`.cc`/`.cpp`/`.hh`/`.hpp`): **2.904.656 → 85,0 %**.
- Todo lo que no es C++ (Python + GLSL + ObjC++ + MSL + Metal + C + shell):
  **265.771 → 7,8 %**.

### Terceros vendorizados — no son de Flipendo

Se mantienen verbatim y se actualizan desde upstream; reescribirlos a mano no es
propiedad del código, es asumir el mantenimiento de bibliotecas ajenas
(`LENGUAJE-CPP.md`, decisión del 2026-09-11).

| Lenguaje | Ficheros | Líneas | % |
|---|---:|---:|---:|
| C++ | 773 | 298.165 | 32,4 % |
| .hpp | 155 | 65.383 | 7,1 % |
| .h heredada | 1.385 | 495.082 | 53,9 % |
| Python | 19 | 1.588 | 0,2 % |
| C | 24 | 58.187 | 6,3 % |
| shell | 6 | 792 | 0,1 % |
| **TOTAL vendorizado** | **2.362** | **919.197** | 100 % |

### Shell: cero `.sh` no es cero shell

Buscando por *shebang* (`git grep -l -E '^#!.*(bash|/bin/sh|zsh|ksh)'`) y no
por extensión, fuera de los vendorizados aparecen **2 ficheros** que ninguna
tabla por extensión ve:

- `release/darwin/scripts/blender-system-info.sh.in`
- `tools/flipendo_cli/flipendo.bash.legacy`

---

## 3. El Python que queda, por zonas

Las zonas no son una lista escrita a mano: son los directorios del árbol. Si una
se vacía, desaparece sola de esta tabla.

| Zona | Ficheros | Líneas | % del Python |
|---|---:|---:|---:|
| `scripts/startup/bl_ui` | 65 | 51.087 | 26,9 % |
| `tests/python` | 232 | 37.769 | 19,9 % |
| `scripts/modules` | 87 | 24.544 | 12,9 % |
| `scripts/addons_core` | 17 | 22.703 | 12,0 % |
| `scripts/startup/bl_operators` | 27 | 11.765 | 6,2 % |
| `scripts/freestyle` | 46 | 6.541 | 3,4 % |
| `intern/cycles` | 10 | 6.262 | 3,3 % |
| `tools/check_source` | 15 | 5.745 | 3,0 % |
| `tools/utils_maintenance` | 6 | 3.642 | 1,9 % |
| `tools/utils` | 9 | 2.705 | 1,4 % |
| `tests/utils` | 4 | 2.146 | 1,1 % |
| `tests/performance` | 14 | 1.999 | 1,1 % |
| `build_files/utils` | 5 | 1.694 | 0,9 % |
| `tests/flipendo` | 13 | 1.631 | 0,9 % |
| `tools/modules` | 1 | 1.235 | 0,7 % |
| `release/release_notes` | 1 | 1.120 | 0,6 % |
| `source/blender` | 4 | 940 | 0,5 % |
| `doc/blender_file_format` | 2 | 892 | 0,5 % |
| `tests/files` | 9 | 720 | 0,4 % |
| `scripts/startup/keyingsets_builtins.py` | 1 | 686 | 0,4 % |
| `release/datafiles` | 3 | 555 | 0,3 % |
| `tests/coverage` | 5 | 552 | 0,3 % |
| `tools/check_blender_release` | 11 | 492 | 0,3 % |
| `tools/utils_ide` | 2 | 438 | 0,2 % |
| `doc/python_api` | 4 | 362 | 0,2 % |
| `release/lts` | 3 | 308 | 0,2 % |
| `doc/manpage` | 1 | 230 | 0,1 % |
| `tools/check_docs` | 1 | 218 | 0,1 % |
| `tools/utils_doc` | 1 | 174 | 0,1 % |
| `scripts/startup/bl_app_templates_system` | 4 | 163 | 0,1 % |
| `build_files/cmake` | 1 | 98 | 0,1 % |
| `build_files/package_spec` | 1 | 87 | 0,0 % |
| `release/pypi` | 1 | 80 | 0,0 % |
| `scripts/site` | 1 | 63 | 0,0 % |
| `scripts/startup/nodeitems_builtins.py` | 1 | 54 | 0,0 % |
| `tests/blender_as_python_module` | 1 | 25 | 0,0 % |
| `scripts/bge` | 1 | 23 | 0,0 % |
| `scripts/templates_custom_objects` | 1 | 18 | 0,0 % |
| **TOTAL** | **611** | **189.766** | 100 % |

---

## 4. Lo que el binario COMPILA y lo que no

Medido sobre `/Users/jesussolaz/Flipendo/dev/build/build.ninja` (5.252 aristas de objeto) y `/Users/jesussolaz/Flipendo/dev/build/CMakeCache.txt`.

La diferencia importa: **código que no entra en el binario no es trabajo
pendiente, es código muerto**, y ninguna tabla por extensión la hacía.

| Lenguaje | Ficheros | Líneas | Con objeto en el build | Líneas que compilan |
|---|---:|---:|---:|---:|
| C++ | 4.345 | 2.574.866 | 3.549 | **2.340.830** |
| Objective-C++ | 8 | 5.060 | 0 | **0** |
| Metal | 1 | 832 | (no produce `.o`) | 0 lineas en 0 ficheros citados en el build |
| MSL | 4 | 1.722 | (no produce `.o`) | 1.722 lineas en 4 ficheros citados en el build |
| GLSL | 745 | 68.391 | (no produce `.o`) | 67.013 lineas en 729 ficheros citados en el build |

### Objective-C++, fichero a fichero

| Fichero | Líneas | ¿Produce objeto? |
|---|---:|---|
| `intern/cycles/device/metal/device_impl.mm` | 1.463 | no |
| `intern/cycles/device/metal/bvh.mm` | 1.398 | no |
| `intern/cycles/device/metal/kernel.mm` | 919 | no |
| `intern/cycles/device/metal/queue.mm` | 868 | no |
| `intern/cycles/device/metal/util.mm` | 182 | no |
| `intern/cycles/device/metal/device.mm` | 155 | no |
| `intern/cycles/device/metal/graphics_interop.mm` | 52 | no |
| `intern/cycles/bvh/metal.mm` | 23 | no |

**5.060 líneas de Objective-C++ propio, de las cuales 0 COMPILAN y 5.060 no.**

- `intern/cycles`: 5.060 líneas sin compilar. `WITH_CYCLES` está en **OFF** en `CMakeCache.txt` y el build no genera **ningún** objeto de `intern/cycles` (0 objetos, 0 ficheros fuente citados).
  Ojo con cómo se dice: `build.ninja` **sí** nombra `intern/cycles/blender`, pero
  como `-I` en las líneas de `INCLUDES` de otros objetivos. «Cero referencias» es
  falso; lo cierto y lo que importa es **cero objetos compilados**.

---

## 5. Evolución, medida commit a commit

Cada fila de estas tablas se ha medido ejecutando la misma medición sobre ese
commit. No hay ni una cifra copiada de ningún documento anterior.

Un aviso de método que cambia las cifras viejas: el `.gitattributes` que marca
`extern` y `lib` como vendorizados se añadió a media historia. Para que la serie
sea comparable consigo misma se imponen a TODAS las filas los prefijos
vendorizados del commit de la cabecera; si no, los commits anteriores contarían
las bibliotecas ajenas como código de Flipendo y la caída parecería mayor de lo
que es.

### Por días (último commit de cada día)

| Commit | Fecha | Python (f.) | Python | C++ | ObjC++ | GLSL | `.h` | `.hpp` | C |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| `3da0e2d745d` | 2026-09-05 13:24 | 1.389 | **492.365** | 2.509.212 | 30.813 | 68.324 | 297.026 | 5.546 | 6.326 |
| `291e35d2bd4` | 2026-09-05 20:01 | 1.008 | **357.084** | 2.471.626 | 30.536 | 68.391 | 248.745 | 33.880 | 0 |
| `421883e9b37` | 2026-09-08 23:55 | 1.003 | **247.999** | 2.483.802 | 30.536 | 68.391 | 248.745 | 34.280 | 0 |
| `59a58f78996` | 2026-09-09 23:47 | 1.003 | **247.642** | 2.484.707 | 30.536 | 68.391 | 248.745 | 34.328 | 0 |
| `4e88d96a431` | 2026-09-10 23:19 | 1.013 | **248.065** | 2.497.382 | 30.536 | 68.391 | 248.745 | 35.019 | 0 |
| `e713bc10e22` | 2026-09-11 23:30 | 611 | **189.766** | 2.574.808 | 5.060 | 68.391 | 248.731 | 36.846 | 0 |
| `fe49217aabe` | 2026-09-12 00:02 | 611 | **189.766** | 2.574.866 | 5.060 | 68.391 | 248.731 | 36.846 | 0 |

De 492.365 a 189.766 líneas de Python propio: **−302.599 líneas, 61,5 %** desde `3da0e2d745d` (2026-09-05 13:24).

---

## 6. Los puentes que quedan de C++ a Python

No son líneas de Python: son **llamadas reales desde el C++** al intérprete. Miden
lo que todavía depende de CPython aunque el `.py` ya no exista.

| Puente | Ocurrencias | Ficheros | Qué es |
|---|---:|---:|---|
| `BPY_run_` | 59 | 18 | ejecutar código Python desde C++ (`BPY_run_*`) |
| `PyImport_ImportModule` | 23 | 11 | importar un módulo de Python desde C++ |
| `PyRun_` | 19 | 9 | la API cruda de CPython (`PyRun_*`) |
| `Py_Initialize` | 2 | 2 | arrancar el intérprete |
| `BPY_python_start` | 8 | 4 | arrancar el intérprete (envoltorio de Blender) |
| **TOTAL de llamadas** | **111** | **32** | |

Además, **557 líneas de preprocesador** (`#if*` con `WITH_PYTHON`) en **259
ficheros** esconden capacidad detrás de la compilación con Python: son los
sitios donde el binario sin intérprete hace menos cosas. `WITH_PYTHON` aparece
817 veces en total en el código propio.

Los ficheros que más puentes concentran (ahí es donde está el trabajo):

| Fichero | Llamadas | ¿Lo compila el binario? |
|---|---:|---|
| `source/blender/python/intern/bpy_interface_run.cc` | 20 | sí |
| `source/blender/python/BPY_extern_run.hh` | 15 | no |
| `source/blender/python/generic/py_capi_utils.cc` | 7 | sí |
| `source/blender/python/intern/bpy_interface.cc` | 7 | sí |
| `source/blender/python/intern/bpy_driver.cc` | 5 | sí |
| `source/creator/creator_args.cc` | 5 | sí |
| `source/gameengine/Ketsji/KX_PythonInit.cpp` | 5 | sí |
| `intern/mantaflow/intern/MANTA_main.cpp` | 4 | sí |
| `source/blender/freestyle/intern/system/PythonInterpreter.h` | 4 | no |
| `source/blender/windowmanager/intern/wm_init_exit.cc` | 4 | sí |
| `source/gameengine/GamePlayer/GPG_ghost.cpp` | 4 | sí |
| `source/blender/editors/space_script/script_edit.cc` | 3 | sí |
| `source/blender/windowmanager/intern/wm_files.cc` | 3 | sí |
| `source/blender/editors/interface/interface.cc` | 2 | sí |
| `source/blender/editors/space_script/space_script.cc` | 2 | sí |

---

## 7. La batería de verificación, ejecutada

Los verificadores se descubren leyendo las banderas `"--fl-..."` registradas en
`source/creator/creator_args.cc`: **61 banderas `--fl-*` en total**, de las cuales
**41** son comprobadores (`--fl-check-*` 22 + `--fl-selftest-*` 19), **16**
son volcadores `--fl-dump-*` (no dan veredicto: escriben estado) y **4** son
conversores o preparadores de escena. Aquí se ejecutan **42** comprobaciones,
porque `--fl-check-ui` admite dos líneas base (registro y dibujo) y son dos
comprobaciones distintas.

### Contra qué binario se ha medido

- Binario: `/Users/jesussolaz/Flipendo/dev/build/bin/Blender.app/Contents/MacOS/Blender`
- Construido del commit `c64b3c11c6f8` — **OJO: no es el commit medido (`fe49217aabe`)**, el 2026-09-11 21:57:00.
- **El árbol de trabajo tenía 1 ficheros versionados modificados sin
  commitear cuando se midió**, de los cuales **0 pueden cambiar el binario**
  (fuente, cabeceras, scripts instalados o ficheros de compilación) y 1 no.
  **Ninguno de los que pueden cambiar el binario**: el veredicto de abajo es del
  commit medido, no de trabajo a medias de nadie.
  Los otros 1, que no entran en el binario, por directorio: `politicas/ESTADO.md` 1.

**Un `--fl-selftest-*` no es una prueba, es un volcador**: escribe un fichero y su
código de salida no es un veredicto. Contarlos como «verdes» por ese código sería un
verde falso. Aquí el veredicto de un selftest se saca comparando su volcado con su
línea base —honrando las declaraciones, ver más abajo—, que es lo que hace su
`--fl-check-*` hermano cuando existe. Y los que en su propia ayuda dicen «Solo
VUELCA: el veredicto lo da `--fl-check-X`» se marcan **volcador** y no se cuentan ni
verdes ni rojos: quien firma es el comprobador.

Y un rojo no se firma a la primera: **todo rojo se repite hasta tres veces**, porque
el REGLAMENTO ya midió que hay resultados no deterministas (el cálculo de normales
acumula en paralelo y en coma flotante). Si alguna pasada sale verde, el veredicto
es **INESTABLE**, que no es lo mismo que un fallo ni que un aprobado. Excepción
declarada: un verificador que tarde más de 60 s no se repite —triplicarlo se comería
la batería— y su fila dice que va con una sola pasada.

**Resultado: 39 en verde, 1 en rojo, 0 inestables, 1 sin línea base con
la que comparar, 1 que su propia ayuda declara volcadores (no dan veredicto).** Tardó 254 segundos en total; el más lento, 124 s.

### Grupo 1 — valen en `--background`

| Verificador | Veredicto | Tiempo | Cifras |
|---|---|---:|---|
| `--fl-check-dupli-face` | verde | 1.3 s | fl-check-dupli-face: TOTAL 28/28 elementos identicos, 37/37 lineas |
| `--fl-check-external-editor` | verde | 1.5 s | editor externo: 4326 casos comparados, 4326 identicos, 0 distintos |
| `--fl-check-find-adjacent` | verde | 1.4 s | fl-check-find-adjacent: TOTAL 65/65 elementos identicos, 92/92 lineas |
| `--fl-check-keyconfig-io` | verde | 1.9 s | keyconfig: ciclo exportar-importar-exportar: 6680 lineas, 0 distintas |
| `--fl-check-keymap-menus` | verde | 1.7 s | FL-KEYMAP-MENUS nombres=136 (defecto=127, solo-con-preferencia=9) configuraciones=22 elementos=4569 call_menu=3105 call_menu_pie=981 call_panel=483 re... |
| `--fl-check-lod-ops` | verde | 1.8 s | fl-check-lod-ops: TOTAL 59/59 elementos identicos, 71/71 lineas |
| `--fl-check-lod-optypes` | verde | 1.5 s | fl-check-lod-optypes: TOTAL 6/6 elementos identicos, 10/10 lineas |
| `--fl-check-manual` | verde | 1.7 s | manual: por url_lookup() (el camino del operador): 7470 identicas, 0 distintas, 553 a la reserva del buscador |
| `--fl-check-mesh-ops` | verde | 4.8 s | fl-check-mesh-ops: 1 linea(s) dentro de la tolerancia declarada 2e-05, no identicas byte a byte |
| `--fl-check-mirror-uv` | verde | 1.5 s | fl-check-mirror-uv: TOTAL 216/216 elementos identicos, 236/236 lineas |
| `--fl-check-object-misc` | verde | 1.9 s | fl-check-object-misc: TOTAL 42/42 elementos identicos, 55/55 lineas |
| `--fl-check-object-select` | verde | 2.0 s | fl-check-object-select: TOTAL 152/152 elementos identicos, 178/178 lineas |
| `--fl-check-optypes` | verde | 1.9 s | fl-check-optypes: TOTAL 44/44 elementos identicos, 66/66 lineas |
| `--fl-check-preset-ops` | verde | 1.8 s | fl-check-preset-ops: TOTAL 89/89 elementos identicos, 103/103 lineas |
| `--fl-check-preset-optypes` | verde | 1.7 s | fl-check-preset-optypes: TOTAL 93/93 elementos identicos, 117/117 lineas |
| `--fl-check-presets` | **ROJO** | 3.8 s | aplicados sin error 156 de 172 (1 con error, 15 sin contexto)  [reproducido en las 3 pasadas] |
| `--fl-check-rigidbody-ops` | verde | 2.0 s | fl-check-rigidbody-ops: TOTAL 63/63 elementos identicos, 69/69 lineas |
| `--fl-check-theme-xml` | verde | 2.1 s | fl-check-theme-xml: TOTAL 6839/6839 elementos identicos, 6844/6844 lineas |
| `--fl-check-tools` | verde | 2.0 s | Herramientas: 30 secciones comprobadas, 0 diferencias, 0 secciones sin trasladar. |
| `--fl-check-ui (registro)` | verde | 2.4 s | Interfaz (registro): 2113 bloques, 2113 identicos, 0 distintos, 0 faltan, 0 sobran. |
| `--fl-selftest-dupli-face` | verde | 9.9 s | 37 lineas identicas a tests/flipendo/dupliface/baseline-python.txt |
| `--fl-selftest-find-adjacent` | verde | 2.0 s | 92 lineas identicas a tests/flipendo/findadjacent/baseline-python.txt |
| `--fl-selftest-keyconfig` | sin línea base | 2.3 s | informe de 4 lineas; no hay linea base congelada en el arbol |
| `--fl-selftest-lod-ops` | verde | 1.9 s | 71 lineas identicas a tests/flipendo/lod/baseline-python.txt |
| `--fl-selftest-mesh-ops` | verde | 6.6 s | 13146 lineas identicas a tests/flipendo/meshops/baseline-python.txt |
| `--fl-selftest-mirror-uv` | verde | 2.3 s | 236 lineas identicas a tests/flipendo/mirroruv/baseline-python.txt |
| `--fl-selftest-numinput` | verde | 2.4 s | 150 lineas identicas a tests/flipendo/numinput/baseline-python.txt |
| `--fl-selftest-object-misc` | verde | 2.7 s | 55 lineas identicas a tests/flipendo/objectmisc/baseline-python.txt |
| `--fl-selftest-object-ops` | verde | 2.7 s | 31 lineas identicas a tests/flipendo/objectops/run-cpp-verified.txt (NO a la preferente tests/flipendo/objectops/baseline-python.txt: linea 5: obtenido `  C    ... |
| `--fl-selftest-object-select` | verde | 3.1 s | 178 lineas identicas a tests/flipendo/objectselect/baseline-python.txt |
| `--fl-selftest-preset-ops` | verde | 3.4 s | 103 lineas identicas a tests/flipendo/presetops/comportamiento-python.txt |
| `--fl-selftest-rigidbody-ops` | verde | 3.4 s | 69 lineas identicas a tests/flipendo/rigidbody/baseline-python.txt |
| `--fl-selftest-theme-xml` | verde | 4.1 s | 6844 lineas identicas a tests/flipendo/themexml/baseline-python.txt |
| `--fl-selftest-wm-batch-rename` | verde | 3.8 s | 1 lineas identicas a tests/flipendo/operators/batch-rename-native.txt |
| `--fl-selftest-wm-owner-ops` | verde | 5.3 s | 2 lineas identicas a tests/flipendo/operators/owner-python.txt |
| `--fl-selftest-wm-properties-edit` | verde | 3.3 s | 2 lineas identicas a tests/flipendo/operators/properties-edit-native.txt |
| `--fl-selftest-wm-system-ops` | verde | 2.9 s | 20 lineas identicas a tests/flipendo/operators/system-python.txt |

### Grupo 2 — necesitan modo gráfico

En `--background` no se carga el keymap por defecto y los volcados de interfaz no
tienen ventana: estos hay que pasarlos con pantalla, y por eso van aparte.

| Verificador | Veredicto | Tiempo | Cifras |
|---|---|---:|---|
| `--fl-check-context-ops` | verde | 14.6 s | Prueba real de context ops: 7 operadores -> /var/folders/m3/7n9l8vtx109fvnck478f7yxr0000gn/T/blender_JB0lzv/fl-check-context-ops-actual.txt |
| `--fl-check-keymap` | verde | 4.3 s | linea base: 248 keymaps  /  quedan por transliterar: 0 |
| `--fl-check-ui (dibujo)` | verde | 123.7 s | Interfaz (diseno): 2004 bloques, 2004 identicos, 0 distintos, 0 faltan, 0 sobran. |
| `--fl-selftest-wm-property-ops` | verde | 3.4 s | 3 lineas identicas a tests/flipendo/operators/properties-python.txt |
| `--fl-selftest-context-ops` | volcador | 2.9 s | volcado de 7 lineas; su ayuda dice que el veredicto lo da su comprobador hermano |

- `--fl-check-context-ops` necesita pantalla: lo dice su propia ayuda: necesita modo grafico.
- `--fl-check-keymap` necesita pantalla: el keymap por defecto no se carga en --background.
- `--fl-check-ui (dibujo)` necesita pantalla: el volcado de dibujo necesita una ventana real.
- `--fl-selftest-wm-property-ops` necesita pantalla: en --background no escribe nada: necesita un editor real.
- `--fl-selftest-context-ops` necesita pantalla: lo dice su propia ayuda en creator_args.cc.

### Lo que no está en verde, con su detalle

- **`--fl-check-presets`** — ROJO (rc=1, modo background):
  aplicados sin error 156 de 172 (1 con error, 15 sin contexto)  [reproducido en las 3 pasadas]
- **`--fl-selftest-keyconfig`** — SIN LINEA BASE (rc=0, modo background):
  informe de 4 lineas; no hay linea base congelada en el arbol

### Declaraciones honradas

Junto a las líneas base hay **1 tolerancia(s)** y **1 fichero(s) de
divergencias deliberadas**. Esta batería **los lee y los honra**: un valor
dentro de la tolerancia declarada no es un rojo, y una divergencia declarada y
cumplida tampoco. Al revés también: si la divergencia deja de ocurrir, es
ROJO, porque entonces la declaración sobra.

| Fichero | Declara | Línea base | ¿Quién la usa? | ¿Hizo falta? |
|---|---|---|---|---|
| `tests/flipendo/meshops/baseline-python.txt.tolerancia` | tolerancia relativa 2e-05 | existe | `--fl-check-mesh-ops`, `--fl-selftest-mesh-ops` | **sí**, en 1 de 6 pasadas |
| `tests/flipendo/operators/execution-python.txt.divergencias` | 1 divergencia(s) deliberada(s), linea 2 | existe | `--fl-check-context-ops` | **sí**, en 3 de 3 pasadas |

- `tests/flipendo/operators/execution-python.txt.divergencias`: El Python de wm.context_cycle_array hacia array[:] y luego append/pop, pero en esta version bpy_prop_array y mathutils.Vector devuelven una TUPLA al cortar: levantaba AttributeError y no rotaba nada, ...

Todas las declaradas hicieron falta en esta pasada: ninguna sobra hoy.

Cómo se pasan todos de una vez:

```sh
flipendo-metrics --bateria ~/Flipendo/dev/upbge          # los dos grupos
flipendo-metrics --bateria --sin-grafico ~/Flipendo/dev/upbge  # solo --background
```

Sale con 0 si no hay ningún rojo, así que vale de guardián antes de un push.

---

## 8. Discrepancias entre lo que dicen los documentos y lo que mide el árbol

### politicas/METRICAS.md (cabecera)

- **Decía:** medido al commit `87ce606a318`
- **Mide el árbol:** ese commit va **58 commits por detrás** del medido aquí (`fe49217aabe`). Entre uno y otro: Python 191.224 → 189.766 (−1.458), C++ 2.540.538 → 2.574.866 (+34.328), Objective-C++ 30.304 → 5.060 (−25.244), `.hpp` 36.679 → 36.846 (+167)
- **Cómo se comprobó:** `git rev-list --count` entre los dos commits y una medición completa de cada uno

---

## 9. Qué falta para «cero Python»

Quedan **189.766 líneas de Python propio** en 611 ficheros, **111 llamadas** desde C++ al
intérprete y **557 guardas `#if*` con `WITH_PYTHON`**. El documento que ordena el
trabajo por zonas es [`BACKLOG-EDITOR-PYTHON.md`](BACKLOG-EDITOR-PYTHON.md); la
doctrina, [`LENGUAJE-CPP.md`](LENGUAJE-CPP.md); la historia de las mediciones,
[`METRICAS.md`](METRICAS.md).

<!-- generado por tools/flipendo_metrics/flipendo_metrics.cpp a fe49217aabe3c527c5883a8f8b1c1e7ceddfbf43 -->
