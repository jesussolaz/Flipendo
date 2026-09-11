# Los niveles de detalle, a C++ — y setenta líneas que no ejecutaba nadie

Carril OPS-3 (Flipendo, 2026-09-11). Los tres operadores de niveles de detalle que
quedaban en `scripts/startup/bl_operators/object.py` pasan a
`source/blender/editors/object/object_lod.cc`, **junto a los dos que ya eran C**.

| idname | C++ | Verificado |
|---|---|---|
| `object.lod_by_name` | `OBJECT_OT_lod_by_name` | ✅ conducta + registro |
| `object.lod_clear_all` | `OBJECT_OT_lod_clear_all` | ✅ conducta + registro |
| `object.lod_generate` | `OBJECT_OT_lod_generate` | ✅ conducta + registro, salvo dos ramas rotas (abajo) |

`object.py` baja de **849 a 620 líneas** (−229). De esas 229, **70 eran código muerto**:
ver la sección siguiente.

Esto no es una pieza del editor: el sistema de niveles de detalle decide **qué malla
dibuja el motor según la distancia a la cámara**. `LodLevel` es DNA, tiene su RNA, y
`lod_add` / `lod_remove` siempre fueron C (`object_lod.cc`, herencia de UPBGE). Lo único
que estaba en Python era la parte que un artista usa: montar los niveles por nombre,
limpiarlos y generarlos con el modificador de diezmado.

---

## Setenta líneas muertas, medidas y no supuestas

`object.py` declaraba **`LodByName` y `LodClearAll` DOS VECES**, en las líneas 569-638 y
otra vez en las 769-838. En Python la segunda definición tapa a la primera, y la tupla
`classes` sólo registra un nombre: las **70 primeras líneas no las ejecutaba nadie**.

Comprobado, no leído: los dos bloques son **byte a byte idénticos** una vez recortados
los espacios de los extremos. Es una duplicación de una fusión mal resuelta del fork, no
dos versiones distintas de nada. Se retiran junto con las clases migradas; no había
capacidad que perder porque no había capacidad que ganar.

---

## Cómo se verificó

Dos arneses nuevos, con línea base congelada contra `/tmp/Blender-ref-ops3.app` — una
copia del binario anterior, con los `.py` dentro de su *bundle*:

```
$ Blender -b --fl-check-lod-ops     tests/flipendo/lod/baseline-python.txt
  TOTAL 59/59 elementos identicos, 71/71 lineas      (11 casos)

$ Blender -b --fl-check-lod-optypes tests/flipendo/lod/optypes-python.txt
  TOTAL 6/6 elementos identicos, 10/10 lineas        (3 operadores)
```

Las dos capturas se repitieron **tres veces con el mismo md5** antes de darlas por
buenas, como manda el reglamento desde lo de las normales no deterministas.

Las once escenas se construyen llamando **sólo a operadores por su idname**
(`object.select_all`, `object.delete`, `mesh.primitive_cube_add`) y renombrando por RNA,
de modo que la batería en Python y la batería en C++ parten exactamente del mismo sitio.
Se vuelca, por caso: el resultado del operador, **todos** los objetos de `bmain` con su
selección, si son el activo, su posición y sus modificadores (nombre, tipo y `ratio`), y
la lista de niveles del objeto activo con su objeto, distancia, histéresis y banderas.

Los casos cubren, a propósito: la rama del prefijo (`LOD0Mesh`), la del sufijo
(`MeshLOD0`), minúsculas con un hueco en medio (`obj_lod0`, `obj_lod1`, `obj_lod3` — se
tiene que parar en el 2), un nombre sin `lod0` (tiene que devolver `CANCELLED` sin tocar
nada), la llamada **dos veces seguidas** (no debe duplicar niveles), un `lod0` suelto sin
más niveles, `clear_all` con y sin niveles, y `lod_generate` con tres combinaciones de
`count`/`target` y las tres formas de nombre.

Sin olvidar la regresión: `--fl-check-optypes` 44/44, `--fl-check-preset-optypes` 93/93,
`--fl-check-object-select` 152/152, `--fl-check-object-misc` 42/42 y
`--fl-check-dupli-face` 28/28 siguen idénticos.

---

## Trampas

1. **No se deselecciona nada antes de duplicar, y no es un descuido del original.**
   `OBJECT_OT_duplicate` copia lo **seleccionado**, y después de la primera vuelta lo
   seleccionado es el nivel anterior — que ya lleva el modificador de diezmado. Por eso a
   partir de la segunda vuelta el original coge `lod.modifiers[-1]` en vez de crear otro:
   el duplicado **lo hereda**. Añadir un `deselect_all` «para limpiar» rompe la cadena y
   el operador deja de duplicar. Lo cazó el arnés: 3 casos de 11 en `CANCELLED`.
2. **`lod_name[-3:-1]` son DOS caracteres, no tres.** En `lod_generate`, la rama del
   sufijo hace `lod_suffix = lod_name[-3:-1]` y `lod_name = lod_name[:-3]`, que es
   **asimétrico** respecto de la rama del prefijo. Con `Genlod0` sale `lod_name="Genl"` y
   `lod_suffix="od"`, y los niveles se llaman `Genlod1`, `Genlod2`… Parece un error y
   produce el nombre correcto; se conserva tal cual.
3. **El dígito del nivel no se limita a uno.** `prefix = prefix[:3] + str(level)`: a
   partir del nivel 10 el nombre buscado tiene una letra más (`LOD10Mesh`). Es lo que
   hacía el original.
4. **`bpy.props` sin `options=` deja `ANIMATABLE`, con `options=` no.** Estas tres
   propiedades no declaran `options`, así que **sí** son animables — justo al revés que
   las de los operadores de presets, que llevan `options={'SKIP_SAVE'}` y por tanto
   pierden el defecto `{'ANIMATABLE'}`. Poner o quitar
   `RNA_def_property_clear_flag(prop, PROP_ANIMATABLE)` por inercia cambia el contrato.
   Lo canta `--fl-check-lod-optypes`.
5. **`SEL_SELECT` es 1, no 2** (2 es `DESELECT`). Con el 2 la batería «limpiaba» la
   escena sin borrar nada y los objetos se acumulaban entre casos. Es un fallo del arnés,
   no del operador, pero cuesta el mismo rato.
6. **`context.view_layer.objects.active = ob` no es `base_activate()`.** Lo que hace RNA
   (`rna_LayerObjects_active_object_set`) es buscar la base y apuntar `basact`, sin más;
   `base_activate()` además puede salir del modo actual. Se replica la versión de RNA.

---

## Las dos ramas de `lod_generate` que ya estaban rotas, medidas

No son una regresión de esta migración: **el original ya fallaba**, y está comprobado
ejecutándolo contra el binario de referencia.

### `package=True`

```
AttributeError: Calling operator "bpy.ops.object.group_link" error, could not be found
```

`bpy.ops.object.group_link` y `bpy.ops.group.create` **no existen desde Blender 2.8**: los
grupos pasaron a ser colecciones. El código llegó al 4.5 sin portar. Además, más abajo,
`level.object.hide` tampoco existe (hoy es `hide_viewport` / `hide_set()`). Comprobado:
`hasattr(bpy.types, "OBJECT_OT_group_link")` es `False` y la llamada levanta la excepción
de arriba **antes de crear nada**, así que la escena queda intacta.

El nativo **devuelve `CANCELLED` con un mensaje claro** en vez de dejar una traza de
Python en la consola. El estado observable es el mismo (nada cambia); lo que cambia es
que el fallo se explica.

### `count` menor que 2

```
ZeroDivisionError: float division by zero      (count = 1)
```

`step = (1.0 - target) / (count - 1)`. Y con `count = 0` el bucle no corre y
`lod.select_set(False)` usa una variable sin asignar. La propiedad **no tiene mínimo**
declarado, así que el caso es alcanzable desde el panel de rehacer. El nativo lo rechaza
con «Count must be 2 or more» y `CANCELLED`; la escena tampoco cambia, igual que antes.

Las dos ramas **no entran en la batería congelada**: no hay conducta que congelar cuando
el original revienta. Están medidas aquí, que es donde tienen que estar.

---

## Deuda con nombre y apellidos

1. **`package=True` no hace lo que promete su etiqueta** («Package into Group»). Para
   recuperar la capacidad hay que reescribirla con **colecciones**: crear o reutilizar
   una colección con el nombre base, enlazar cada nivel, emparentarlos al objeto y
   ocultar los niveles con `hide_viewport` / `hide_render`. No se ha hecho aquí porque
   sería **inventar comportamiento**, no migrarlo: no hay original que reproducir, porque
   el original lleva roto desde 2.8. Quien lo haga, que lo verifique contra una escena
   nueva y lo escriba aquí.
2. **`count` sigue sin mínimo declarado** para no cambiar la superficie de registro
   (`--fl-check-lod-optypes` compara `min=-2147483648`). La defensa está en el `exec`. Si
   algún día se decide que el contrato puede cambiar, lo suyo es `min=2` y quitar la
   comprobación.
3. **Sin verificar por ejecución**: nada. Los tres operadores corren en `--background`.

---

## Lo que queda de `object.py`

620 líneas y seis clases: `SubdivisionSet` (`object.subdivision_set`), `ShapeTransfer`
(`object.shape_key_transfer`), `JoinUVs` (`object.join_uvs`), `TransformsToDeltas` y
`TransformsToDeltasAnim`, y `OBJECT_OT_assign_property_defaults`.

De `TransformsToDeltas` hay medición previa en
[`OBJECT-SELECT-A-CPP.md`](OBJECT-SELECT-A-CPP.md): el puerto está escrito y sale
idéntico en localización y escala, y falla por **1e-8 relativo** sólo en la rama de
rotación. Ahí sigue, con las tres pistas de dónde seguir buscando.
