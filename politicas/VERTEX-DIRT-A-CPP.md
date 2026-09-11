# `paint.vertex_color_dirt` a C++ — qué había, cómo se verificó, trampas

Carril C (Flipendo, noche del 2026-09-10/11). Trabajo de partida de **GitHub Copilot**,
terminado y verificado por **Claude Opus 5**.

## Qué había

`scripts/startup/bl_operators/vertexpaint_dirt.py`, 198 líneas. Un solo operador,
`paint.vertex_color_dirt` ("Dirty Vertex Colors"), con seis propiedades: `blur_strength`,
`blur_iterations`, `clean_angle`, `dirt_angle`, `dirt_only` y `normalize`. Algoritmo
original de Keith "Wahooney" Boshoff: para cada vértice compara su normal con la dirección
media hacia los vértices conectados por arista; el ángulo resultante (0 = pliegue,
π = saliente) se desenfoca por vecindad, se normaliza a [0,1] y multiplica el color del
atributo de color activo. Lo llama `VIEW3D_MT_paint_vertex` (`space_view3d.py:3291`).

## Qué hay ahora

`source/blender/editors/mesh/mesh_vertex_dirt.cc` → `PAINT_OT_vertex_color_dirt`,
declarado en `mesh_intern.hh` y registrado en `ED_operatortypes_mesh()`. Mismo `idname`,
mismos nombres/tipos/rangos/defectos/subtipos de propiedad (`clean_angle` y `dirt_angle`
siguen siendo `PROP_ANGLE`), mismas cadenas traducibles, mismos flags
`OPTYPE_REGISTER|OPTYPE_UNDO` y el mismo `poll` (objeto activo de tipo malla).

## Cómo se verificó

Arnés nuevo en C++, `--fl-selftest-mesh-ops <fichero>` y `--fl-check-mesh-ops <base>`
(`source/blender/editors/mesh/fl_mesh_ops_selftest.cc`,
`source/blender/editors/include/FL_mesh_ops_selftest.hh`), al estilo de
`--fl-dump-keymap`/`--fl-check-tools`. Construye once escenas deterministas llamando
**solo a operadores por su idname**, ejecuta `paint.vertex_color_dirt` y vuelca el
atributo de color **elemento a elemento** (bytes crudos para `BYTE_COLOR`, `%.9g` para
`FLOAT_COLOR`).

Los once casos cubren, a propósito, todas las ramas del algoritmo:

| Caso | Qué rama toca |
|---|---|
| 0-4 | Suzanne: topología irregular; defectos, `blur_iterations=0`, `blur=5/0.35`, `dirt_only`+`normalize=False`+ángulos recortados, `normalize=False` |
| 5 | Icoesfera: superficie convexa, valencias 5 y 6 |
| 6 | Esfera UV con atributo `FLOAT_COLOR` en dominio POINT (rama de coma flotante + multiplicación repetida del mismo vértice) |
| 7 | Lo mismo con `BYTE_COLOR` en POINT: acumula el redondeo a byte una vez por bucle |
| 8 | Cubo unido a cuatro vértices sueltos: única forma de llegar a `tot_con == 0` (ángulo = π/2) |
| 9 | Rejilla con máscara de pintura y una cara de cada tres seleccionada |
| 10 | Dos pasadas seguidas: la segunda parte de colores ya no uniformes |

Línea base congelada en `tests/flipendo/meshops/baseline-python.txt`, capturada contra
`/tmp/Blender-ref.app` — el binario **anterior a la migración**, que aún lleva
`vertexpaint_dirt.py` en `Contents/Resources/4.5/scripts` (comprobado:
`bpy.types.PAINT_OT_vertex_color_dirt.__module__ == 'bl_operators.vertexpaint_dirt'`).

Resultado con el binario ya migrado:

```
$ Blender -b --fl-check-mesh-ops tests/flipendo/meshops/baseline-python.txt
TOTAL 13112/13112 elementos de color identicos, 13146/13146 lineas   (rc=0)
```

**13112 de 13112 elementos idénticos, tolerancia cero**: bytes iguales en los diez casos
de color de 8 bits y `%.9g` idéntico en el caso de color flotante. No hay margen
declarado porque no hizo falta ninguno.

> **Corrección de esa misma noche (03:20): un elemento de los 13112 es inestable, y la
> culpa es del cálculo de normales, que NO es determinista.**
>
> A las 02:55 el comprobador empezó a dar 13111/13112 en vez de 13112/13112. El único
> elemento que baila es el 43 del caso 6 —la esfera UV con atributo `FLOAT_COLOR` en
> dominio POINT—, con una diferencia de 1e-5 relativo. La primera explicación que escribí
> aquí ("algo que entró en el árbol esta noche cambió el cálculo de normales") **era
> falsa**, y la desmonta el experimento más simple: ejecutar cuatro veces seguidas el
> **mismo binario de referencia, que no ha cambiado**, construir la misma esfera UV y
> sumar las normales de sus vértices:
>
> ```
> PROBE no=2.7911332559e-05
> PROBE no=2.7911332559e-05
> PROBE no=2.7911332559e-05
> PROBE no=2.80749853019e-05     <-- misma versión, misma escena, otro resultado
> ```
>
> El cálculo de normales de vértice acumula en paralelo y el orden de la suma en coma
> flotante depende de cómo repartan los hilos, así que **varía entre ejecuciones**. Las
> posiciones sí son estables (`co=4.33623790741e-05` siempre).
>
> Consecuencias, y son generales, no solo de este operador:
>
> - La suciedad por cavidad es por definición una función de la normal, así que hereda esa
>   inestabilidad. En el dominio POINT se amplifica, porque el Python multiplica el mismo
>   vértice una vez por cada bucle que lo toca.
> - **Los diez casos de color de 8 bits son inmunes**: el redondeo a byte se come 1e-5.
>   Por eso 13111 de los 13112 elementos nunca fallan.
> - Lo esperado es **13112/13112**, y un 13111/13112 con la única diferencia en el
>   elemento 43 del caso 6 **no es una regresión**: es esta inestabilidad. Cualquier otra
>   diferencia sí lo es.
> - **Aviso para otros carriles**: cualquier línea base congelada que dependa de normales
>   de vértice en coma flotante sin cuantizar tiene este mismo problema. Si hace falta una
>   comprobación estable al 100%, o se cuantiza la salida o se evita el dominio POINT con
>   color flotante.

### Un fallo que cazó `--fl-check-optypes` después

El subtipo de los dos ángulos estaba mal en la primera versión: se puso
`RNA_def_property_subtype(prop, PROP_ANGLE)` y el Python declaraba `unit='ROTATION'`. **No
es lo mismo**: `subtype` y `unit` comparten campo de bits, `PROP_ANGLE` es
`16 | PROP_UNIT_ROTATION`, y `unit='ROTATION'` a secas deja el índice de subtipo en 0.
Python lo reporta como subtipo vacío y el C++ lo reportaba como `ANGLE`. Corregido a
`PropertySubType(PROP_UNIT_ROTATION)`; `--fl-check-optypes` pasó de 22/24 a 24/24.

> Nota de proceso: la captura de la línea base se hizo con un guion Python **efímero**,
> fuera del árbol (en el scratchpad de la sesión). Al repositorio solo entra el `.txt`
> resultante y el comprobador, que es C++. Es una mejora consciente sobre lo que se hizo
> en `tests/flipendo/objectops/`, donde el guion de captura sí se dejó en el árbol: la
> doctrina dice "nada nuevo en Python", y un binario congelado se puede interrogar sin
> dejar Python detrás.

## Trampas (las tres que costaban el resultado)

1. **`.color` de un `BYTE_COLOR` no son los bytes.** El getter de RNA
   (`rna_ByteColorAttributeValue_color_get`) decodifica de sRGB a lineal, y el setter
   vuelve a codificar. O sea, `col[0] = tone * col[0]` multiplica **en espacio lineal**.
   Multiplicar el byte directamente (`(col.r/255) * tone * 255`, que es lo que parece al
   leer el .py) da colores visiblemente distintos: en el caso 0, 128 donde el Python da
   182. Esta era la diferencia que Copilot estaba persiguiendo cuando se quedó sin cuota;
   iba mirando visitas duplicadas de bucles y el mapeo RNA de `active_color`, y la causa
   estaba un nivel más abajo, en el espacio de color del getter.
2. **Cada canal hace el viaje completo.** `col[k] = ...` sobre un `bpy_prop_array` pasa
   por `RNA_property_float_set_index()`, que hace *get-array → modifica un índice →
   set-array*. Los tres canales hacen por tanto tres viajes bytes → lineal → bytes
   independientes, no uno. Se replica canal a canal.
3. **La mezcla float/double de mathutils y `array.array("f")`.**
   `Vector.normalized()` (`normalize_vn`) calcula el módulo al cuadrado en double sumando
   **de la última componente a la primera** y multiplica por el recíproco en float;
   `Vector.dot()` (`dot_vn_vn`) acumula en double productos hechos en float, también de z
   a x; `vec /= n` es `mul_vn_fl(vec, n, 1.0f/n)`, multiplicación por recíproco y no
   división; y en el desenfoque cada `vert_tone[j] += ...` **trunca a float32 al guardar**
   aunque el producto se haya hecho en double. Sin reproducir esto hay deriva de última
   cifra que, tras 40 iteraciones de desenfoque, se ve en el byte.

Y una cuarta, del arnés más que del operador, que costó cinco casos:

4. **`WM_operator_name_call()` arrastra las propiedades de la última ejecución.** El
   operador es `OPTYPE_REGISTER`, así que `WM_operator_last_properties_init()` rellena las
   propiedades no puestas con las de la llamada anterior. `bpy.ops...()` no hace eso. Un
   arnés que deje propiedades sin poner compara peras con manzanas: el caso N hereda los
   ángulos del caso N-1. El arnés declara **siempre las seis**.

## Divergencias deliberadas (dos, ambas en casos en los que el Python se rompía)

- `math.acos()` lanza `ValueError: math domain error` si el producto escalar se sale de
  [-1,1] por error de redondeo, y con ello moría el operador entero con traza. El C++
  recorta a [-1,1], que es lo que hace el resto del C++ de Blender. Solo cambia el caso
  degenerado.
- `min(vert_tone)` sobre una malla de cero vértices es otro `ValueError`. El C++ devuelve
  `OPERATOR_CANCELLED`.

## Lo que queda

Nada de este operador. `scripts/startup/bl_operators/vertexpaint_dirt.py` retirado y
`"vertexpaint_dirt"` fuera de `_modules` en `bl_operators/__init__.py`.
`scripts/startup/bl_ui/space_view3d.py` sigue llamándolo por idname sin cambio alguno,
que es justamente la prueba de que la migración es invisible.
