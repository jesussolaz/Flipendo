# Líneas base de la interfaz del editor

Dos ficheros, dos niveles de prueba, los dos congelados **con el Python de `bl_ui`
todavía vivo**. Son el objetivo que tiene que reproducir la interfaz migrada a C++,
igual que `tests/flipendo/keymap/baseline-python.txt` lo fue para el mapa de teclado y
`tests/flipendo/toolsystem/baseline-python.txt` para el catálogo de herramientas.

| Fichero | Qué prueba | Modo |
|---|---|---|
| `baseline-python.txt` | El **registro**: que cada panel, menú y cabecera existe donde tiene que existir, con los mismos campos, y en la misma posición dentro de su región | `--background` |
| `baseline-python-layout.txt` | El **dibujo**: que el `draw()` produce el mismo árbol de `uiLayout` — mismas filas, columnas, etiquetas, propiedades, operadores con sus argumentos e iconos | gráfico |

## Cómo se generan

```
Blender --factory-startup -b --fl-dump-ui        tests/flipendo/ui/baseline-python.txt
Blender --factory-startup    --fl-dump-ui-layout tests/flipendo/ui/baseline-python-layout.txt
```

## Cómo se comprueban

```
Blender --factory-startup -b --fl-check-ui tests/flipendo/ui/baseline-python.txt
Blender --factory-startup    --fl-check-ui tests/flipendo/ui/baseline-python-layout.txt
```

Un solo comando para las dos: `--fl-check-ui` decide qué línea base tiene delante por
la marca de su primera línea. Devuelve 0 solo si no hay ninguna diferencia, y lista
por nombre los bloques que difieren, los que faltan y los que sobran.

## Por qué el volcado de dibujo no cubre todo, y por qué eso está bien

La escena de fábrica tiene un cubo, una cámara y una luz. No tiene material, ni
modificador, ni sistema de partículas, ni armadura, así que **cientos de paneles no
pasan su propio `poll`** — y ese es su comportamiento correcto, no un fallo del
volcador. Cada tipo no cubierto queda en el fichero con su motivo:

| Motivo | Qué significa |
|---|---|
| `poll` | El panel dijo que no se dibuja en esta escena. Es el caso normal y mayoritario |
| `sin-region` | No hay región viva de ese espacio y no se pudo abrir |
| `instanciado` | El panel (o su padre) es de los que se repiten por elemento de una lista y leen `panel->runtime->custom_data`: dibujarlo en seco sería inventarse el dato |
| `fallo` | Su `draw()` se llevó el proceso por delante y lo atrapó la red. **Nunca cuenta como idéntico** |
| `sin-draw` | El tipo no tiene `draw()` |

Un tipo que estaba cubierto en la línea base y deja de estarlo sale como **diferencia**,
que es justo lo que hace falta: la cobertura también es parte del contrato.

## El método completo

En [`politicas/UI-A-CPP.md`](../../../politicas/UI-A-CPP.md): cómo se declara un panel
con `FL_ui_registry`, cómo se verifica con estos volcados, y las trampas encontradas.

## Cuidado con la escala de la interfaz

Los anchos que se pasan al motor de layout van en `UI_UNIT_X`, que depende del DPI y de
la escala de la interfaz. Las dos líneas base y las comprobaciones se lanzan con
`--factory-startup` por eso: para que la escala sea siempre la misma.

## El binario no lee el Python de este repo

`nb install` **copia** `scripts/` dentro de `Blender.app`. El binario carga esa copia,
así que tocar un `.py` del repo no cambia nada hasta reinstalar. Es la forma más fácil
de creerse un "0 diferencias" que no significa nada: verificar contra el Python exige
reinstalar, o tocar la copia instalada y restaurarla después.
