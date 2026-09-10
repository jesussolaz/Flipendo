# Previsualizaciones: qué hay de Python aquí, y qué no

> Este encargo venía con una premisa: que `bpy.utils.previews` es uno de los
> puentes donde el C++ llama de vuelta al intérprete. **Se midió, y no lo es.**
> Lo que sigue es lo que hay de verdad, medido en el árbol.

## La premisa, comprobada

```
grep -rn "utils.previews\|_utils_previews" --include="*.cc" --include="*.cpp" \
     --include="*.hh" --include="*.h" source/ \
  | grep -v source/blender/python/intern/bpy_utils_previews \
  | grep -v source/blender/python/intern/bpy.cc
```

Cero resultados. El C++ **no nombra `bpy.utils.previews` en ningún sitio**. Las
dos únicas menciones que se excluyen del grep son el propio módulo de enlace y
su registro (`bpy.cc:777`, `PyModule_AddObject(mod, "_utils_previews", …)`).

La dirección es la contraria: `bpy_utils_previews.cc` (203 líneas de C++)
**expone** tres funciones —`new`, `load`, `release`— a Python, y
`scripts/modules/bpy/utils/previews.py` (136 líneas) las envuelve en una clase
para addons. Es Python → C++, no C++ → Python. No bloquea nada.

Y no lo usa nadie: los únicos ficheros del árbol que lo mencionan son dos
plantillas de documentación (`templates_py/ui_previews_dynamic_enum.py` 141,
`ui_previews_custom_icon.py` 76). Ni el editor ni el Player lo tocan.

**Destino de `previews.py`: RETIRAR con el resto de la capa `bpy/`.** No se
traduce, porque no implementa capacidad: la expone. Cae el día que el editor no
ejecute Python. Es la misma clasificación que ya le daba
`BACKLOG-EDITOR-PYTHON.md` a toda la capa de enlace.

Probablemente lo que se confundió con esto es lo de abajo, que sí es Python de
verdad y sí tiene trabajo.

## Lo que sí hay: la generación por lotes

| Pieza | Líneas | Qué es | Estado |
|---|---:|---|---|
| `WM_OT_previews_ensure` (`wm_operators.cc:3867`) | — | Previsualizaciones «internas»: materiales, texturas, imágenes, mundos, luces | **Ya es C++** |
| `WM_OT_previews_clear` (`wm_operators.cc`) | — | Borrado por tipo de ID | **Ya es C++**, con un hueco (abajo) |
| `bl_operators/file.py`, dos operadores | 247 | `wm.previews_batch_generate` y `wm.previews_batch_clear`: selector de ficheros y, por cada `.blend`, lanzan un Blender hijo | Python |
| `bl_previews_utils/bl_previews_render.py` | 536 | El script que corre ese hijo | Python |

El hijo se lanza así:

```
blender --background --factory-startup [--enable-autoexec] <fichero.blend> \
        --python bl_previews_render.py -- [--clear] [--no_scenes] …
```

Es decir: **el editor arranca un segundo Blender solo para ejecutar Python.**

## Dos cosas que la medición sacó a la luz

### 1. El camino de borrado no necesita renderizar nada

`do_clear_previews` (líneas 439-457 del script hijo) hace tres cosas: llamar a
`bpy.ops.wm.previews_clear(id_type={'SHADING'})` —que **ya es C++**—, poner
`preview.image_size = (0, 0)` en objetos, colecciones y escenas no enlazadas, y
guardar. Ni una imagen se renderiza.

Por tanto `wm.previews_batch_clear` es **migrable entero sin tocar el
renderizador**: un operador nativo con el mismo `idname` y las mismas
propiedades que, por cada `.blend` seleccionado, lo carga, limpia y guarda. Es
el trozo con mejor relación esfuerzo/resultado de esta área.

### 2. `wm.previews_clear` no borra las de escena, y su enumeración dice que sí

`previews_clear_exec` recorre esta lista:

```
&bmain->objects, &bmain->collections, &bmain->materials,
&bmain->worlds, &bmain->lights, &bmain->textures, &bmain->images
```

**`&bmain->scenes` no está.** Pero `preview_id_type_items` sí ofrece
`{PREVIEW_FILTER_SCENE, "SCENE", …, "Scenes"}` y `PREVIEW_FILTER_GEOMETRY`
promete «scenes, collections and objects». Así que hoy
`wm.previews_clear(id_type={'SCENE'})` no hace nada y no avisa.

No es un fallo de esta migración: viene de Blender. Y explica por qué el script
hijo borra las de escena a mano — es el apaño. **Se arregla añadiendo
`&bmain->scenes` a la lista**, y entonces el apaño sobra. No lo toco en este
commit porque `wm_operators.cc` lo está editando otro carril esta noche; queda
aquí anotado para que no se pierda.

## Lo que queda, con su tamaño

**El renderizador de previsualizaciones de geometría** (`do_previews`, líneas
58-438 del script hijo, 380 líneas) es el trabajo de verdad: monta una escena
de previsualización, crea cámara y luces, calcula la caja envolvente de los
objetos para encuadrarlos, renderiza fuera de pantalla y mete el resultado en
el `PreviewImage` del ID. Eso no es pegamento: es un pequeño motor de
previsualización.

Plan por trozos, en orden de menor a mayor riesgo:

1. **`wm.previews_batch_clear` nativo** — sin renderizar. Incluye arreglar el
   hueco de las escenas. Verificable comparando el `.blend` resultante por los
   dos caminos.
2. **La orquestación de `wm.previews_batch_generate` nativa**, con el hijo
   invocado por una opción propia (`--fl-previews-render`) en vez de
   `--python <script>`.
3. **El renderizador**, que es el grueso y necesita línea base de imagen: hay
   que volcar el `PreviewImage` de cada ID y compararlo, porque «se ve
   parecido» no es una verificación.

## Cómo habrá que verificarlo

Con el patrón del proyecto, y hace falta una pieza nueva:
**`--fl-dump-previews <fichero.blend> <salida>`**, que recorra los IDs con
previsualización y vuelque, por cada uno, nombre, tamaño y una huella del búfer
de píxeles. La línea base se congela con el Python todavía activo y el C++ tiene
que reproducirla. Sin ese volcado, la migración del renderizador no se puede
cerrar con evidencia y no se debe intentar.

## Estado

**Auditado, no migrado.** La conclusión con valor es la primera: la premisa del
encargo era incorrecta, `bpy.utils.previews` no bloquea nada y no hay que
reescribirlo — se retira con la capa de enlace. El trabajo real de esta área son
las 783 líneas del generador por lotes, y su primer trozo (el borrado) está
descrito arriba con suficiente detalle para acometerlo sin volver a investigar.
