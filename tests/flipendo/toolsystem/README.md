# Línea base del catálogo de herramientas

`baseline-python.txt` es el catálogo que define hoy el Python
(`scripts/startup/bl_ui/space_toolsystem_toolbar.py`): **416 entradas de herramienta
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

## El editor de nodos y la doble iteración

`tools_from_context` recorre `(cls._tools[None], cls._tools.get(mode, ()))`: primero
las herramientas comunes del espacio y luego las del modo. El editor de nodos **no
tiene modos** — su `_tools` solo tiene la clave `None` — así que si se le pregunta con
`mode=None` las dos vueltas devuelven *la misma lista* y el catálogo sale duplicado.

No es una hipótesis: la primera versión de esta línea base decía `tools=18` para 9
herramientas, cada una dos veces. En uso real no ocurre, porque el modo del editor de
nodos es `space_data.tree_type` (`ShaderNodeTree`, `GeometryNodeTree`…), que nunca es
clave de `_tools` y deja la segunda vuelta vacía.

Para el catálogo nativo esto significa dos cosas:

1. El editor de nodos tiene **9 herramientas**, no 18.
2. Un espacio sin modos no debe modelarse como "un modo llamado `None`". En
   `FL_toolsystem.hpp` las herramientas comunes y las del modo son listas distintas
   (`ToolbarDecl::modes`), y un espacio sin modos tiene una sola entrada; no hay forma
   de que la misma lista se recorra dos veces.
