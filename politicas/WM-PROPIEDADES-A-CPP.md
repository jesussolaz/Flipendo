# Operadores de propiedades personalizadas: de Python a C++

## Camino nativo

`source/blender/windowmanager/intern/wm_property_ops.cc` concentra la familia. Las
rutas que antes se evaluaban como `eval("context." + data_path)` se resuelven ahora
por RNA: primero sobre `RNA_Context` y después, para miembros dinámicos como
`object` o `bone`, mediante `CTX_data_pointer_get`.

Son nativos:

- `wm.properties_add`: elige `prop`, `prop1`, etc. sin colisionar con propiedades
  RNA ni personalizadas, crea un `IDP_DOUBLE` de valor 1.0 y reproduce sus límites,
  paso, precisión y valor por defecto de interfaz;
- `wm.properties_remove`: dispara la actualización RNA antes de liberar la
  `IDProperty` y conserva la prohibición sobre overrides enlazados;
- `wm.properties_context_change`: cambia el enum `SpaceProperties.context` por RNA,
  de modo que también se ejecuta su callback de actualización.
- `wm.properties_edit`: conserva sus 25 propiedades RNA, el diálogo dinámico, las
  conversiones entre escalares/arrays, los metadatos UI, la marca de override y el
  renombrado de rutas animadas;
- `wm.properties_edit_value`: dibuja directamente los tipos soportados y usa el
  evaluador cerrado para los restantes.

## Sustituto de `eval()`

El modo cuyo identificador histórico sigue siendo `PYTHON` ya no ejecuta Python.
Acepta aritmética mediante `BLI_expr_pylike` —incluidos `**`, `%` y `//`—, `True`,
`False`, `None`, cadenas, arrays numéricos homogéneos y grupos vacíos. Una estructura
arbitraria no reconocida devuelve `CANCELLED` y deja intacta la propiedad: el fallo es
ruidoso y no convierte datos a medias. Éste es el contrato sustituto deliberado para
la antigua evaluación arbitraria.

Las longitudes RNA nativas incluyen el byte nulo. Por ello `data_path`,
`property_name` y `context` se declaran con 1025, 64 y 65 bytes para conservar los
`maxlen` Python observables de 1024, 63 y 64 caracteres.

## Verificación

El contrato de tipos sigue siendo byte a byte idéntico a
`tests/flipendo/operators/baseline-python.txt`: 2.408 operadores, 107.568 líneas y
3.406.563 bytes.

`--fl-selftest-wm-property-ops` espera a que exista interfaz gráfica y ejecuta los
tres operadores por idname sobre el cubo y el editor de Propiedades reales. Su salida
debe coincidir con `tests/flipendo/operators/properties-python.txt`: prueba la colisión
`prop`/`prop1`, el valor 1.0, el borrado y el cambio `OBJECT` a `WORLD`.

`--fl-selftest-wm-properties-edit` prueba además una conversión y renombrado
`DOUBLE -> INT_ARRAY[3]` y la expresión `2**3 + 5%2 + 9//2`. Su resultado coincide
con `tests/flipendo/operators/properties-edit-native.txt`: **2/2 operaciones
FINISHED**, array `[3,3,3]` y resultado `13.0`. Los contratos de los dos operadores
son byte a byte idénticos a la línea base Python.

## Deuda explícita

La reevaluación de `autoflags` de F-Curves al cambiar entre entero y flotante aún no
tiene prueba diferencial propia; el renombrado de rutas sí pasa por
`BKE_animdata_fix_paths_rename_all_ex`. El evaluador cerrado no acepta diccionarios o
arrays de propiedades no vacíos: los rechaza sin mutar, porque admitirlos exige un
formato de datos nativo explícito, no reintroducir ejecución arbitraria.
