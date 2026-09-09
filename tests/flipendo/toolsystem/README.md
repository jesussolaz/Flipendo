# Línea base del catálogo de herramientas

`baseline-python.txt` es el catálogo que define hoy el Python
(`scripts/startup/bl_ui/space_toolsystem_toolbar.py`): **425 entradas de herramienta
en 30 combinaciones de espacio y modo**, 124 identificadores distintos.

Es el objetivo que tiene que reproducir el catálogo nativo, igual que
`tests/flipendo/keymap/baseline-python.txt` lo fue para el mapa de teclado.

## Cómo se generó, y por qué en modo gráfico

Enumerando `ToolSelectPanelHelper.tools_from_context()` por cada subclase de espacio y
cada modo, y volcando los campos del `ToolDef`.

**Tiene que hacerse en modo gráfico, no en `--background`.** En segundo plano el campo
`keymap` de cada herramienta sigue siendo un *objeto función* — el volcado sale con
direcciones de memoria, distintas en cada ejecución. En modo gráfico ya se ha ejecutado
la inicialización que llama a esas funciones y sustituye cada una por el nombre del
keymap que generó.

Eso no es un detalle del volcado: es la propiedad más importante del subsistema. Los
keymaps de herramienta **no son datos, los genera código al arrancar**, y el
equivalente en C++ tiene que hacer lo mismo o los 196 keymaps de herramienta que hoy
salen resueltos aparecerán vacíos.
