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

## Verificación

El contrato general sigue congelado en
`tests/flipendo/operators/baseline-python.txt`; después de migrar estos tipos,
`--fl-dump-operators` debe seguir siendo byte a byte idéntico (2.408 tipos, 107.568
líneas, 3.406.563 bytes).

`--fl-selftest-wm-system-ops` compara contra
`tests/flipendo/operators/system-python.txt`: cuatro normalizaciones de URL, ocho
resoluciones de documentación y las ramas sin efectos externos de `wm.path_open`.
La línea base se obtuvo del `Blender-python.app` congelado antes de retirar las clases.

## Estado y deuda explícita

Son nativos `wm.url_open`, `wm.path_open` y `wm.doc_view`.

Quedan dos piezas relacionadas que no se deben borrar a medias:

- `wm.url_open_preset`: sus ocho destinos incluyen el informe de error con datos de
  plataforma/GPU y el enum era ampliable modificando la clase Python.
- `wm.doc_view_manual`: consulta 4.253 patrones generados en
  `rna_manual_reference.py` y también mapas registrados por extensiones mediante
  `bpy.utils.register_manual_map`. Migrar sólo el operador dejando esa consulta en
  Python no reduciría la dependencia; quitarla perdería extensibilidad.
