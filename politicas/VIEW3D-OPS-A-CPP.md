# Los operadores de `bl_operators/view3d.py` a C++

Carril C (Flipendo, 2026-09-11). Petición del carril de menús: estos cinco operadores en
Python bloqueaban migraciones de menús de la vista 3D.

| idname | C++ | Fichero |
|---|---|---|
| `view3d.edit_mesh_extrude_individual_move` | `VIEW3D_OT_edit_mesh_extrude_individual_move` | `editors/mesh/view3d_edit_mesh_extrude.cc` |
| `view3d.edit_mesh_extrude_move_normal` | `VIEW3D_OT_edit_mesh_extrude_move_normal` | idem |
| `view3d.edit_mesh_extrude_move_shrink_fatten` | `VIEW3D_OT_edit_mesh_extrude_move_shrink_fatten` | idem |
| `view3d.edit_mesh_extrude_manifold_normal` | `VIEW3D_OT_edit_mesh_extrude_manifold_normal` | idem |
| `view3d.transform_gizmo_set` | `VIEW3D_OT_transform_gizmo_set` | `editors/object/view3d_transform_gizmo_set.cc` |

## Trampa de nombres (la primera y la más tonta)

En el Python, **el nombre de la clase y el `bl_idname` no coinciden** en dos de ellos:

```python
class VIEW3D_OT_edit_mesh_extrude_move(Operator):
    bl_idname = "view3d.edit_mesh_extrude_move_normal"        # ¡no "..._extrude_move"!
class VIEW3D_OT_edit_mesh_extrude_shrink_fatten(Operator):
    bl_idname = "view3d.edit_mesh_extrude_move_shrink_fatten"  # ¡no "..._shrink_fatten"!
```

Lo que manda es el `bl_idname`: hay atajos de teclado y menús que lo usan (el keymap
nativo llama a `VIEW3D_OT_edit_mesh_extrude_move_normal` con la tecla E en edición de
malla). Copiar el nombre de la clase habría roto la tecla E en silencio.

## Qué hacen y por qué viven donde viven

Los cuatro de extrusión no extruyen nada: miran el modo de selección y cuánto hay
seleccionado, y despachan a la macro `MESH_OT_extrude_*` que toque, en
`WM_OP_INVOKE_REGION_WIN` para que el transform arranque modal bajo el ratón. Por eso
están en `editors/mesh`, al lado de `ED_operatormacros_mesh()`, que es donde se definen
esas macros. Devuelven siempre `FINISHED` sin mirar lo que conteste la macro: la macro se
queda en `RUNNING_MODAL` y, si se propagara, este operador no llegaría a liberarse (es el
motivo que comentaba el Python citando #24671).

`view3d.transform_gizmo_set` está en `editors/object` y no en `editors/space_view3d`
porque esa noche el `CMakeLists.txt` de `space_view3d` tenía trabajo sin commitear del
carril de menús, y meter ahí un fichero nuevo habría arrastrado su trabajo a medias al
commit. Es donde se registra y donde están las banderas que toca (`show_gizmo_object_*`).
Moverlo cuando el árbol esté tranquilo es cosmética.

## Cómo se verificó

`--fl-check-optypes` contra `tests/flipendo/optypes/baseline-python.txt`, capturada del
binario con Python: compara para cada operador el nombre visible, la descripción, el
contexto de traducción y **cada propiedad** con su tipo, subtipo, longitud de array,
nombre y descripción visibles, valor por defecto, rango duro, rango blando e items de
enum. Es exactamente lo que la doctrina exige comprobar ("mismos identificadores...
mismas propiedades") y se puede hacer en `--background`.

Más `--fl-check-keymap` en modo gráfico: el keymap nativo referencia
`VIEW3D_OT_edit_mesh_extrude_move_normal`, así que si el idname no resolviera, saltaría.

**Lo que NO está verificado por ejecución, y por qué**: los cuatro de extrusión llaman a
sus macros en `INVOKE_REGION_WIN`, que necesita una región de vista 3D viva y deja un
transform modal corriendo; `transform_gizmo_set` necesita un `ScrArea` de tipo VIEW_3D.
Nada de eso existe en `--background`, y en modo gráfico con `-P` el guion corre antes de
que el contexto tenga área. Ni el original en Python se puede invocar desde un guion. La
lógica de despacho (qué macro y con qué propiedades según `select_mode`, `total_face_sel`
y `total_edge_sel`) se trasladó rama a rama y está comentada en el C++ junto a la
condición delYthon original, incluidos los dos casos con comentario propio: con una sola
arista NO se fija `orient_type` (#61637) ni se restringe el eje.

## Lo que queda: los tres `FileHandler`, medidos y planificados

`view3d.py` se queda en 63 líneas con `VIEW3D_FH_empty_image`,
`VIEW3D_FH_camera_background_image` y `VIEW3D_FH_vdb_volume`. **No se migran esta noche a
propósito**: el trabajo de escribirlos es pequeño, pero no tengo forma de verificarlos con
el método del carril, y no voy a retirar Python a cambio de código no verificado.

### El camino ya está trazado (no hay que inventar nada)

El árbol **ya tiene** el patrón en C++, en `source/blender/editors/io/`. Por ejemplo
`io_alembic.cc:726`:

```cpp
auto fh = std::make_unique<blender::bke::FileHandlerType>();
STRNCPY(fh->idname, "IO_FH_alembic");
STRNCPY(fh->import_operator, "WM_OT_alembic_import");
STRNCPY(fh->label, "Alembic");
STRNCPY(fh->file_extensions_str, ".abc");
fh->poll_drop = poll_file_object_drop;
bke::file_handler_add(std::move(fh));
```

Y se registran desde `ED_operatortypes_io()` en `io_ops.cc`. Lo mismo vale para estos tres:

| idname | `import_operator` | extensiones | `poll_drop` |
|---|---|---|---|
| `VIEW3D_FH_empty_image` | `OBJECT_OT_empty_image_add` | imagen + vídeo | espacio VIEW_3D y `rv3d->persp` en (PERSP, ORTHO) |
| `VIEW3D_FH_camera_background_image` | `VIEW3D_OT_camera_background_image_add` | imagen + vídeo | espacio VIEW_3D y `rv3d->persp == RV3D_CAMOB` |
| `VIEW3D_FH_vdb_volume` | `OBJECT_OT_volume_import` | `.vdb` | espacio VIEW_3D |

Las extensiones de imagen y vídeo **ya son C**: `imb_ext_image` e `imb_ext_movie`, que es
justo de donde las saca el Python (`bpy.path.extensions_image` se construye en
`bpy_path.cc:41` con `PyC_FrozenSetFromStrings(imb_ext_image)`).

### Por qué no se cierran hoy: no se pueden verificar con este método

1. El original las junta con `";".join((*extensions_image, *extensions_movie))`, y esos son
   **frozensets**: el orden de la cadena resultante es el del hash de Python. No es
   reproducible ni comparable, aunque para el emparejamiento de extensiones dé igual.
2. Un `FileHandler` no es un operador: no lo ve `--fl-check-optypes`. Y los registrados
   desde C++ con `bke::file_handler_add()` **no crean tipo RNA**, así que el binario de
   referencia (donde son Python) y el migrado no se pueden interrogar por el mismo camino.
   Habría que escribir un `--fl-dump-filehandlers` en C++ **y** un equivalente en el
   binario de referencia que lea `bpy.types.FileHandler.__subclasses__()`; son dos fuentes
   distintas, no una línea base.
3. `poll_drop` depende de una región de vista 3D viva y del modo de perspectiva: no se
   puede ejercitar en `--background`, igual que pasa con los cuatro operadores de
   extrusión.

### Qué haría falta para cerrarlo con evidencia

Un volcado en C++ de `bke::file_handlers()` (idname, etiqueta, operador de importación y
lista de extensiones **ordenada**, para esquivar el problema 1), y capturar la línea base
en el binario de referencia recorriendo `bpy.types.FileHandler.__subclasses__()` con el
mismo formato y el mismo orden. Es media hora de trabajo; no cabía esta noche.
- Comprobación manual en la interfaz de que la tecla E y sus variantes siguen extruyendo
  igual en los cuatro casos (cara suelta, varias caras, arista, vértice).
