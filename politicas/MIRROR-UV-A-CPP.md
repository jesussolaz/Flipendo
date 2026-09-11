# `mesh.faces_mirror_uv` a C++ — qué había, cómo se verificó, dos trampas y un bug del original

Carril C (Flipendo, 2026-09-11). Primera de las tres piezas de
`scripts/startup/bl_operators/mesh.py`.

## Qué había

La clase `MeshMirrorUV` (líneas 15-193 de `mesh.py`), operador `mesh.faces_mirror_uv`
("Copy Mirrored UV Coords"), con dos propiedades: `direction` (`POSITIVE`/`NEGATIVE`) y
`precision` (1-16, defecto 3). Empareja vértices por su posición espejada en X —
redondeada a `precision` decimales—, empareja caras por el conjunto de vértices
emparejados, y copia las UV de una cara a su espejo invirtiéndolas en U
(`-(u - 0.5) + 0.5`).

## Qué hay ahora

`source/blender/editors/mesh/mesh_faces_mirror_uv.cc` → `MESH_OT_faces_mirror_uv`,
declarado en `mesh_intern.hh` y registrado en `ED_operatortypes_mesh()`. Mismo idname,
mismas propiedades, mismos textos (incluidos los tres avisos con su orden de comprobación)
y el mismo `poll`.

## Cómo se verificó

Arnés `--fl-selftest-mirror-uv` / `--fl-check-mirror-uv`, añadido a
`editors/mesh/fl_mesh_ops_selftest.cc`. Seis escenas deterministas: una rejilla simétrica
en X con UV artificiales y distintas en cada bucle, en las dos direcciones y con dos
precisiones; una malla **sin capa UV** (tiene que avisar y no romper); y un cubo.

```
$ Blender -b --fl-check-mirror-uv tests/flipendo/mirroruv/baseline-python.txt
TOTAL 216/216 elementos identicos, 236/236 lineas   (rc=0)
```

Repetido **tres veces seguidas con el mismo resultado**, como manda el reglamento desde lo
de las normales no deterministas.

## Trampas

1. **`uv.select` vale `False` cuando no hay capa de selección de UV.** El getter de RNA es
   literalmente `return select ? select[loop_index] : false`
   (`rna_MeshUVLoop_select_get`). Como el operador solo copia cuando **todas** las UV de
   las dos caras están seleccionadas (`puvsel[i] = (False not in ...)`), en una malla
   recién creada **no copia nada**. Tratar la ausencia de capa como "todo seleccionado"
   —que es lo que parece razonable— es exactamente lo contrario de lo que hace el
   original. Lo cazó el arnés: 100/216 antes de corregirlo, 216/216 después.
2. **El cero negativo.** La clave del emparejamiento es la posición redondeada, y la
   búsqueda espejada niega la X. En Python `(-0.0,) == (0.0,)` y además tienen el mismo
   hash, así que el diccionario los trata como la misma clave; en C++ hay que normalizar
   el `-0.0` a `0.0` o los vértices del plano X=0 no se emparejan consigo mismos.
3. Las UV de destino se leen de una **copia** tomada antes de escribir nada: una cara y su
   espejo pueden emparejarse la una con la otra y cada una tiene que leer las UV
   originales de la otra, no las ya reescritas.
4. `v1.index(v2[k])` de Python devuelve el **primer** índice que coincide, no cualquiera.

## Bug del original que impide verificar una rama (medido, no supuesto)

La rama con **selección de UV activada** —la única en la que el operador copia algo— **no
se ha podido congelar**, porque el operador *original en Python* revienta al ejecutarlo
repetidamente con UV seleccionadas:

- Una ejecución de la batería abortó con **SIGABRT (rc=134)** al llegar al séptimo caso.
- Otra, con la misma escena y el mismo binario, se **colgó indefinidamente** (más de cinco
  minutos para una malla de 48 bucles) en el segundo caso.

Dos síntomas distintos para el mismo código y la misma entrada apuntan a corrupción de
memoria en el original, no a una diferencia de la migración. Un caso aislado sí funciona
(`select` + operador + operador, instantáneo); es la repetición con `purge()` de por medio
la que lo tumba.

**Qué queda pendiente, concretamente**: verificar la rama de copia real. Dos caminos, en
orden de preferencia:

1. Capturar la línea base **una escena por proceso** (un `Blender -b` por caso), que evita
   la acumulación que tumba al original. Es lento pero directo.
2. Si se quiere averiguar el bug del original: el sospechoso es que `puvs[i]` guarda
   `mathutils.Vector` *envueltos sobre memoria RNA* de la malla, y la malla se libera en
   `purge()` mientras esas referencias siguen vivas en el intérprete.

Mientras tanto, lo verificado (216/216) cubre el emparejamiento de vértices y caras, las
dos direcciones, dos precisiones, la malla sin capa UV y la decisión de no copiar —que es,
por la trampa 1, el comportamiento real del operador en una malla recién creada.

## Lo que queda de `mesh.py`

Los otros dos operadores, `mesh.select_next_item` y `mesh.select_prev_item`, que tiran de
`scripts/startup/bl_operators/bmesh/find_adjacent.py` (341 líneas). No se puede cerrar
`mesh.py` sin migrar también ese módulo.
