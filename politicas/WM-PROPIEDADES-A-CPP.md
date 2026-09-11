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

## Deuda explícita

Siguen en Python `wm.properties_edit` y `wm.properties_edit_value`. Ambos construyen
diálogos dinámicos para todos los tipos de `IDProperty`; además, su modo `PYTHON`
acepta expresiones arbitrarias. No se deben retirar hasta que el editor nativo cubra
conversiones, metadatos UI, curvas de animación, renombrado de rutas y un sustituto
del evaluador con contrato definido.
