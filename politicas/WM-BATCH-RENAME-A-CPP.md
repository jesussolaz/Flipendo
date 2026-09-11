# Renombrado por lotes: de Python a C++

## Contrato

`wm.batch_rename` y el tipo auxiliar `BatchRenameAction` se registran ahora desde
`wm_batch_rename.cc`. Conservan el mismo `idname`, tres propiedades de operador,
20 tipos de dato y las cuatro acciones: establecer/prefijo/sufijo, retirar
caracteres, buscar/reemplazar y cambiar mayúsculas/minúsculas/título.

La colección `actions` sigue siendo RNA y por ello el diálogo puede añadir, quitar y
reordenar pasos sin guardar estado paralelo. Los destinos se recogen como
`PointerRNA`: objetos, datos de objeto, materiales, colecciones, acciones, escenas y
pinceles usan el mismo camino que nodos, huesos y tiras. La escritura de `name` pasa
por RNA y ejecuta sus actualizaciones.

## Expresiones regulares

El motor usa `std::regex` en modo ECMAScript. Convierte referencias Python `\1` a
`$1` y escapa el destino cuando el usuario no activa «Regular Expression Replace».
La búsqueda de texto normal implementa reemplazo global y sensibilidad de
mayúsculas por separado, sin construir una expresión regular artificial.

## Verificación

- `WM_OT_batch_rename` es **byte a byte idéntico** al bloque de la línea base Python:
  callbacks, `UNDO`, enum de 23 entradas contando separadores y colección con tipo
  fijo `BatchRenameAction`.
- `--fl-selftest-wm-batch-rename` ejecuta tres acciones encadenadas sobre los tres
  objetos de fábrica. Resultado: `FINISHED`, 3/3 y nombres
  `[PRE-CUBE,PRE-C_MER_,PRE-LIGHT]`, idéntico a
  `tests/flipendo/operators/batch-rename-native.txt`.
- `sc-codex` y `nb-codex` completos terminan con `rc=0`.

## Trampas

El tipo auxiliar tiene que registrarse antes de declarar la colección del operador;
si también se registra la clase Python con el mismo identificador, el registro de
`bl_operators` falla entero. Por eso ambos nombres se retiran de la tupla Python en
el mismo cambio.

El cambio de caso es byte a byte para ASCII. Los nombres UTF-8 se conservan, pero la
conversión de caja de caracteres no ASCII queda como deuda explícita hasta conectar
las tablas Unicode del subsistema de texto; no se corrompen bytes ni se ejecuta
Python como reserva.
