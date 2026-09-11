# Operadores de sistema de `wm.py`: de Python a C++

## Alcance

Esta familia integra la interfaz con macOS y con la documentación en línea. No es
correcto sustituirla por llamadas a `system()`: las rutas y URL proceden de archivos,
extensiones y botones, y una shell convertiría datos válidos en código.

## Camino nativo

`source/blender/windowmanager/intern/wm_system_ops.cc` concentra los operadores. En
macOS abre URL y rutas con `posix_spawn("/usr/bin/open", argv)` y espera el proceso
corto del lanzador. No hay shell ni intérprete.

`wm.url_open` conserva el comportamiento de `urllib.parse` que se veía desde fuera:

- añade `https://` si falta esquema;
- sólo añade `utm_source` a `blender.org` y sus subdominios;
- decodifica y vuelve a codificar la consulta como `parse_qs` + `urlencode`, incluido
  espacio como `+`, y sustituye un UTM anterior;
- usa `blender-<version>` en minúsculas y con espacios convertidos en guiones.

`wm.path_open` expande `//` contra el `.blend`, normaliza, comprueba existencia y llama
al lanzador con un argumento separado.

`wm.doc_view` reproduce `_wm_doc_get_id` por RNA: distingue clases, propiedades,
operadores en sintaxis Python o `SOME_OT_name`, sube hasta la clase que declaró una
propiedad heredada y conserva el destino genérico de propiedades personalizadas.

`wm.url_open_preset` publica los mismos ocho destinos mediante un enum RNA dinámico.
Las URL dependientes de versión se construyen con `BLENDER_VERSION`; el manual respeta
el idioma activo, y el informe de error incorpora la versión de macOS, el backend GPU y
los datos de compilación antes de pasar por la misma normalización segura de URL.

## Verificación

El contrato general sigue congelado en
`tests/flipendo/operators/baseline-python.txt`; después de migrar estos tipos,
`--fl-dump-operators` debe seguir siendo byte a byte idéntico (2.408 tipos, 107.568
líneas, 3.406.563 bytes).

`--fl-selftest-wm-system-ops` compara contra
`tests/flipendo/operators/system-python.txt`: cuatro normalizaciones de URL, los siete
presets que no abren un informe dependiente de una GPU gráfica, ocho resoluciones de
documentación y las ramas sin efectos externos de `wm.path_open`.
La línea base se obtuvo del `Blender-python.app` congelado antes de retirar las clases.

## Contrato nativo del manual (B6, 2026-09-11)

`wm.doc_view_manual` ya no recorre callbacks Python. `WM_manual.hpp` publica un
registro de proveedores C++ (`provider_register`/`provider_unregister`): se consultan
del último al primero, por lo que una extensión nativa puede sustituir la referencia
integrada sin parchear el operador. El proveedor base conserva destinos semánticos
para las páginas especiales que usa Flipendo y envía el resto de identificadores RNA
válidos al buscador versionado del manual oficial. Es el contrato sustituto elegido
para la API extensible de `bpy.utils.manual_map()`.

También son nativos `wm.sysinfo`, `wm.blenderplayer_start` y
`wm.operator_cheat_sheet`. El informe de sistema se escribe directamente desde los
datos de build/GPU; el Player se guarda como copia temporal y se lanza con
`posix_spawn`, sin shell; la chuleta enumera el registro real de `wmOperatorType` y
sus propiedades RNA en `OperatorList.txt`.

Los cuatro contratos de operador se compararon bloque a bloque con la línea base
Python: **4/4 idénticos**. La build completa terminó con `rc=0` y la autoprueba previa
de sistema siguió en `cmp=0`.

## Estado y deuda explícita

Son nativos `wm.url_open`, `wm.url_open_preset`, `wm.path_open`, `wm.doc_view`,
`wm.doc_view_manual`, `wm.sysinfo`, `wm.blenderplayer_start` y
`wm.operator_cheat_sheet`.

El antiguo `preset_items` de la clase Python era mutable, pero no existe en el árbol
ningún consumidor que lo ampliase. El contrato efectivo son los ocho valores expuestos
por el callback RNA nativo; el volcado sigue observando un enum dinámico, igual que antes.

La tabla Python generada `rna_manual_reference.py` deja de ser una dependencia del
operador. Mientras el resto del editor siga cargando Python permanece como dato
heredado; el contrato extensible de Flipendo es ya el registro C++.
