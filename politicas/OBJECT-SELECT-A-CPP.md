# Los tres operadores de selección de `bl_operators/object.py` a C++

Carril C (Flipendo, 2026-09-11). Petición del carril de menús: son los tres operadores de
`object.py` que salen en `VIEW3D_MT_select_object` y bloqueaban la migración de ese menú.

| idname | C++ | Líneas de Python retiradas |
|---|---|---|
| `object.select_pattern` | `OBJECT_OT_select_pattern` | 20-110 |
| `object.select_camera` | `OBJECT_OT_select_camera` | 111-146 |
| `object.select_hierarchy` | `OBJECT_OT_select_hierarchy` | 147-215 |

Todo en `source/blender/editors/object/object_select_extra.cc`, dentro de
`namespace blender::ed::object` (ver la trampa 1). `object.py` baja de 1173 a 1016 líneas;
los otros dieciséis operadores del fichero siguen en Python.

## Cómo se verificó

Arnés nuevo `--fl-selftest-object-select` / `--fl-check-object-select`
(`editors/object/fl_object_select_selftest.cc`,
`editors/include/FL_object_select_selftest.hh`). Diecinueve escenas deterministas
construidas llamando solo a operadores por idname; se vuelca, para **cada** objeto de la
escena, nombre, selección, si es el activo, si está oculto y quién es su padre.

```
$ Blender -b --fl-check-object-select tests/flipendo/objectselect/baseline-python.txt
TOTAL 152/152 elementos identicos, 178/178 lineas   (19/19 casos)

$ Blender -b --fl-check-optypes tests/flipendo/optypes/baseline-python.txt
TOTAL 33/33 elementos identicos, 46/46 lineas       (12 operadores)
```

Los casos cubren, a propósito:

- **`select_pattern`** (10 casos): `Cube*`, `*phere`, `?lane`, `[CP]*` (las tres formas de
  comodín que promete la descripción), `cube*` con y sin distinguir mayúsculas, `*.001`,
  un nombre exacto y `*`; con `extend` a los dos lados. En todos ellos hay un objeto
  **oculto** (`Plane`) para comprobar que solo se miran los objetos visibles.
- **`select_camera`** (3 casos): con cámara de escena y sin ella, y con `extend`.
- **`select_hierarchy`** (6 casos): una jerarquía de tres niveles
  (`Cube` → `Cube.001` → `Icosphere`, y `Cube` → `Sphere`), en las dos direcciones, con
  `extend` a los dos lados, empezando por una hoja, por la raíz y por un objeto suelto sin
  padre ni hijos (que tiene que devolver `CANCELLED` y no tocar nada).

Línea base congelada en `tests/flipendo/objectselect/baseline-python.txt`, capturada
contra `/tmp/Blender-ref.app` (binario anterior a la migración, con los `.py` dentro). La
captura se hizo con un guion Python **efímero** fuera del árbol; al repositorio entran solo
el `.txt` y el comprobador, que es C++.

## Trampas

1. **El espacio de nombres.** `object_ops.cc` registra desde dentro de
   `namespace blender::ed::object`, así que la definición del operador tiene que estar en
   ese mismo namespace o el enlazador no encuentra el símbolo. Esto ya costó esta noche
   que **ni `blender` ni `blenderplayer` enlazaran** con
   `view3d_transform_gizmo_set.cc`, que se definió en el espacio global. Y lo peor:
   `nb bf_editor_object` daba rc=0 igualmente. **Compilar tu biblioteca no basta; hay que
   pasar `nb install` entero.**
2. **`maxlen=64` de Python no es 64 en RNA.** `bpy_props` guarda `maxlen + 1` para dejar
   sitio al nulo, así que la propiedad registrada declara **65**. Poner 64 en
   `RNA_def_string()` cambia el contrato. Lo cazó `--fl-check-optypes` (32/33 → 33/33). El
   búfer local para leer el patrón también tiene que ser de 66, no de `MAX_NAME` (64).
3. **`obj.children` no es RNA.** Está en `scripts/modules/bpy_types.py:227` y recorre
   `bpy.data.objects` filtrando por padre: o sea, el orden de `bmain->objects`. No hay
   lista de hijos en el DNA.
4. **El orden del `sort` es de cadena, no "natural".**
   `select_new.sort(key=lambda o: o.name)` de Python compara cadenas tal cual, así que
   `Cube.10` va **antes** que `Cube.2`. Usar `BLI_strcasecmp_natural()` habría dado otro
   objeto activo.
5. **El objeto activo se puede quedar en nada.** En dirección `PARENT`, si el padre del
   objeto activo no es visible, `act_new` se queda sin asignar y Python hace
   `view_layer.objects.active = None`. La capa de vista se queda **sin objeto activo**. Es
   raro, pero es el comportamiento y se conserva (`view_layer->basact = nullptr`).
6. **`use_local_camera` es `!v3d->scenelock`**, no una bandera propia; y fuera de una vista
   3D (por ejemplo en segundo plano) manda la cámara de la escena.
7. **`fnmatch` frente a `fnmatchcase(a.upper(), b.upper())`.** Se usa
   `fnmatch(pattern, name, FNM_CASEFOLD)` cuando no se pide distinguir mayúsculas.
   `FNM_CASEFOLD` es ASCII y `str.upper()` de Python conoce Unicode: coinciden en cualquier
   nombre de objeto normal y difieren solo con nombres no ASCII. Queda dicho.

## Lo que queda

- El cuarto operador de `object.py` que sale en menús de la vista 3D,
  **`object.make_dupli_face`** (líneas 588-669): construye mallas a mano con
  `vertices.add`/`loops.add`/`polygons.add` y `foreach_set`. Ojo con `SCALE_FAC = 0.01` y
  con `instance_faces_scale = 1/SCALE_FAC`, que tienen que ir tal cual o las instancias
  salen con otro tamaño.
- Los otros quince operadores de `object.py` no aparecen en menús de la vista 3D:
  `subdivision_set`, `join_uvs`, `isolate_type_render`, `hide_render_clear_all`,
  `transforms_to_deltas`, `anim_transforms_to_deltas`, `assign_property_defaults` y los
  `lod_*` no salen en ningún `bl_ui`; `shape_key_transfer` está en
  `properties_data_mesh.py` y los `instance_offset_*` en `properties_object.py` y
  `properties_collection.py`.
