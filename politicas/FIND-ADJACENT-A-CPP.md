# `mesh.py` cerrado: `find_adjacent` y los dos operadores de selección por orden

Carril C (Flipendo, 2026-09-11). Con esta pieza **desaparecen del árbol
`scripts/startup/bl_operators/mesh.py` y el paquete entero
`scripts/startup/bl_operators/bmesh/`**.

## Qué había

`bl_operators/bmesh/find_adjacent.py` (341 líneas) y las dos clases de `mesh.py` que
tiraban de él:

| idname | C++ |
|---|---|
| `mesh.select_next_item` | `MESH_OT_select_next_item` |
| `mesh.select_prev_item` | `MESH_OT_select_prev_item` |

Todo en `source/blender/editors/mesh/editmesh_find_adjacent.cc`.

El algoritmo: a partir de los **dos últimos elementos que el usuario seleccionó**
(`bm.select_history`), mide la profundidad topológica de los vértices del destino respecto
al origen por **dos caminos distintos** —saltando de arista a arista por vértices
compartidos, y saltando por caras— y busca otro elemento con exactamente el mismo perfil
de profundidades. Si hay varios, se queda con el de mayor variación de profundidad (al
cuadrado, para que unos pocos valores altos ganen a muchos bajos) y, si aún empatan, con
el más lejano al origen. Si tras eso siguen empatando, compara la "huella topológica" del
entorno (`ele_uuid`) con cuatro criterios cada vez más laxos: tupla exacta, conjunto, suma
del conjunto y longitud.

## Cómo se verificó

Arnés `--fl-selftest-find-adjacent` / `--fl-check-find-adjacent` (en
`editors/mesh/fl_mesh_ops_selftest.cc`). Trece escenas: rejilla, cubo y esfera UV, en los
tres modos de selección (vértice, arista, cara), con historiales de selección conocidos —
adyacentes y separados—, más los casos degenerados (historial vacío y con un solo
elemento, en los dos operadores).

```
$ Blender -b --fl-check-find-adjacent tests/flipendo/findadjacent/baseline-python.txt
TOTAL 65/65 elementos identicos, 92/92 lineas   (13/13 casos, rc=0)
```

Repetido **tres veces seguidas con el mismo resultado**. Se vuelca, por caso, la lista de
vértices, aristas y caras seleccionados, el historial de selección completo (tipo + índice)
y la cara activa.

Y `--fl-check-optypes`: **38/38**, 15 operadores.

## El caso catorce, retirado con evidencia: el original no es determinista

La batería tenía un caso más —**esfera UV en modo cara, historial `F0, F1`**— y está
retirado a propósito. **El operador original en Python da tres resultados distintos en tres
ejecuciones seguidas del mismo binario con la misma escena**:

```
md5 del caso 7, tres capturas contra /tmp/Blender-ref.app:
  ae32caf290299005fa763e49b07d5b9d
  5559633547cc2b468e343e41bc75dc65
  6dd5a4749d3abe7ba86a35f7ab9a9b2e
```

Los otros trece casos, en cambio, dan el **mismo md5 las tres veces**.

La causa está en el algoritmo: `find_next()` y `elems_depth_search()` recorren `set` de
Python de elementos de BMesh, cuyo hash es el **puntero** del objeto. Cuando varios
candidatos empatan —y en una esfera, por simetría, empatan— el que gana depende de en qué
orden los recorrió el conjunto, y ese orden cambia entre ejecuciones porque cambian las
direcciones de memoria. En una rejilla o un cubo no se nota porque los empates se resuelven
antes.

Por eso el puerto en C++ **itera en orden de inserción** (un `Vector` en paralelo a cada
`Set`): el resultado es el mismo que el del Python en todos los casos en que el Python es
estable, y además se reproduce a sí mismo, que es lo que exige el reglamento de una línea
base. Donde el Python era una moneda al aire, el C++ da una respuesta fija — cuál de las
tres es, no está definido por el original.

**Queda pendiente**: decidir si ese comportamiento merece un desempate explícito y estable
(por ejemplo, por índice de elemento más bajo). Hoy el C++ es determinista pero su elección
en caso de empate es la que caiga del orden de inserción, no una regla declarada.

## Trampas

1. **`if not e.is_wire` mira la arista de partida, no la vecina**, y además está dentro del
   bucle interior de `other_edges_over_edge()`. Leído rápido parece un filtro sobre
   `e_other`; no lo es. Se conserva tal cual.
2. **`select_set(False)` no toca el historial de selección.** `BM_vert_select_set()` y
   compañía solo mueven la bandera y el contador. Importa en `select_prev`, que
   deselecciona el último y después vuelve a leer el historial: la posición 1 sigue siendo
   la misma de antes.
3. **`vert_depths.setdefault(v, depth)`**: gana la PRIMERA profundidad a la que se alcanza
   un vértice, no la última. Es `Map::add()`, no `add_overwrite()`.
4. El desempate por `ele_uuid` no descarta la lista cuando un criterio no deja a nadie: si
   un filtro deja cero candidatos, se pasa al siguiente criterio **con la lista anterior**,
   no con la vacía.

## Lo que queda de esta familia

Nada de `mesh.py`. `scripts/startup/bl_operators/` se queda sin el paquete `bmesh/`.
