# `object.make_dupli_face` a C++

Carril C (Flipendo, 2026-09-11). El cuarto y último de los operadores de
`bl_operators/object.py` que el carril de menús necesitaba.

`source/blender/editors/object/object_make_dupli_face.cc` →
`OBJECT_OT_make_dupli_face`. Mismo idname, mismo nombre visible ("Make Instance Face"),
misma descripción, mismos flags y sin propiedades, igual que el original.

## Qué hace

Agrupa los objetos seleccionados **por dato compartido** (la malla, o la colección si es un
*empty* de instancia de colección), y por cada grupo:

1. construye una malla nueva con **una cara cuadrada diminuta por objeto**, colocada y
   orientada por la matriz de mundo de ese objeto;
2. crea un objeto con esa malla, en modo instancia por caras y con la escala compensada;
3. cuelga de él un único objeto con el dato original;
4. saca los objetos originales de todas sus colecciones.

## Cómo se verificó

Arnés `--fl-selftest-dupli-face` / `--fl-check-dupli-face` (en
`editors/object/fl_object_select_selftest.cc`). Cuatro escenas: tres cubos con mallas
distintas (tres grupos), tres objetos que **comparten** malla (un solo grupo con tres
caras), un objeto suelto, y una selección sin ningún candidato válido (una cámara).

```
$ Blender -b --fl-check-dupli-face tests/flipendo/dupliface/baseline-python.txt
TOTAL 28/28 elementos identicos, 37/37 lineas   (4/4 casos, rc=0)
```

Repetido **tres veces seguidas con el mismo resultado**. Se vuelca, por caso, todos los
objetos de la escena (nombre, tipo, selección, padre, tipo de instancia, si usa escala de
instancia y su valor) y todas las mallas (nombre, recuento de vértices, aristas, caras y
bucles, número de usuarios y una suma de comprobación de las posiciones).

Y `--fl-check-optypes`: **39/39**, 16 operadores.

## Trampas

1. **`SCALE_FAC = 0.01` y `instance_faces_scale = 1 / SCALE_FAC` van juntos.** El cuadrado
   base mide medio centímetro de lado y la escala de instancia lo compensa; tocar una sola
   de las dos constantes cambia el tamaño de todas las instancias generadas.
2. **La agrupación conserva el orden de primera aparición.** `linked` es un
   `defaultdict(list)` y en Python los diccionarios mantienen el orden de inserción, así
   que el orden de los grupos —y por tanto los nombres `Cube_dupli`, `Cube.001_dupli`…—
   depende de él. Se reproduce con un `Vector` de claves en paralelo al índice.
3. **`matrix.to_3x3()` incluye la escala**, no es solo rotación: el cuadrado de cada
   objeto sale escalado como el objeto.
4. **El nombre del objeto nuevo es el de la MALLA, no el del dato de origen**
   (`bpy.data.objects.new(mesh.name, mesh)`), y la malla ya se llama `<dato>_dupli`. De ahí
   que el objeto resultante se llame `Cube_dupli` y no `Cube`.
5. `ob.parent = x` de RNA no es una asignación: pasa por
   `blender::ed::object::parent_set()`, que además arregla la matriz de padre inverso.

## Lo que queda de `object.py`

Los cuatro que bloqueaban menús están hechos (`select_pattern`, `select_camera`,
`select_hierarchy` y este). Quedan quince operadores más en el fichero, ninguno de ellos
usado por menús de la vista 3D: `subdivision_set`, `join_uvs`, `isolate_type_render`,
`hide_render_clear_all`, `transforms_to_deltas`, `anim_transforms_to_deltas`,
`assign_property_defaults`, los `lod_*`, `shape_key_transfer` (en
`properties_data_mesh.py`) y los `instance_offset_*` (en `properties_object.py` y
`properties_collection.py`).
