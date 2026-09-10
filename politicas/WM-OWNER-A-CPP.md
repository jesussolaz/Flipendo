# Etiquetas de propietario del workspace: de Python a C++

`wm.owner_enable` y `wm.owner_disable` manipulan directamente la lista
`WorkSpace.owner_ids`. Conservan los mismos idnames, propiedades RNA y notificación
`NC_WINDOW`; no necesitan importar ni ejecutar código Python.

La inserción admite las mismas etiquetas repetidas que `workspace.owner_ids.new`.
La eliminación busca la primera coincidencia, como el acceso por clave de la colección,
y cancela con error si la etiqueta no existe.

`--fl-selftest-wm-owner-ops` ejecuta ambos tipos por idname sobre el workspace real y
compara con `tests/flipendo/operators/owner-python.txt`. El contrato general continúa
byte a byte idéntico a `tests/flipendo/operators/baseline-python.txt`: 2.408 tipos,
107.568 líneas y 3.406.563 bytes.
