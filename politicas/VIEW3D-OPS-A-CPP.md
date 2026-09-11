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

## Lo que queda

- Los tres `FileHandler` (`VIEW3D_FH_empty_image`, `VIEW3D_FH_camera_background_image`,
  `VIEW3D_FH_vdb_volume`) siguen en `view3d.py`, que se queda en 63 líneas. No son
  operadores: necesitan `bke::file_handler_add()` con un `FileHandlerType` en C++, y sus
  extensiones salen de `imb_ext_image`/`imb_ext_movie`, que ya son C. Es la próxima pieza
  de este fichero.
- Comprobación manual en la interfaz de que la tecla E y sus variantes siguen extruyendo
  igual en los cuatro casos (cara suelta, varias caras, arista, vértice).
