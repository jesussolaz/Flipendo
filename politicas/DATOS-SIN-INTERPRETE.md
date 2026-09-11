# Los datos tienen que viajar aunque no viaje el intérprete

> Escrita el 2026-09-11 después de destapar dos agujeros estructurales que no
> daban ningún error: uno de instalación y otro de escritura.
>
> Los dos tienen la misma forma, y por eso están en el mismo documento: **una
> cosa que dejó de ser código y pasó a ser dato seguía tratada como código**.

---

## 1. `scripts/` solo se instala con `WITH_PYTHON=ON`

### Qué pasaba

`source/creator/CMakeLists.txt:463` abre un `if(WITH_PYTHON)` y dentro instala el
árbol `scripts/` **entero**. Es razonable: son scripts de Python. Pero esta
noche `scripts/presets` dejó de ser Python: son **167 ficheros `.fpreset`**, que
son datos, más 2 temas `.xml` y 5 `.py` de FFmpeg que aún no tienen sustituto.

Consecuencia, medida antes de arreglarla: un editor compilado con
`WITH_PYTHON=OFF` **no llevaba ni un preset**. Y no habría dado ningún error: el
menú de presets habría salido vacío y nadie lo habría sabido hasta abrirlo.
Lo mismo para los `.blend` de las plantillas de aplicación, que son el dato de la
plantilla, no su código.

La tabla del manual (`datafiles/manual/rna_manual_reference.txt`) se libró por un
pelo, y no por casualidad: se puso a propósito fuera de `scripts/` justo por esto
(ver `MODULES-Y-EL-INTERPRETE.md §3`).

### Cómo se arregló

Un bloque de instalación nuevo, **fuera de cualquier condición de Python**, en
`source/creator/CMakeLists.txt`, y `PATTERN "presets" EXCLUDE` en la instalación
del árbol de scripts para que no se instale dos veces. `scripts/presets` es el
único directorio con ese nombre en todo el árbol de scripts (comprobado con
`find scripts -type d -name presets`), así que el patrón no se lleva nada por
delante.

### La regla, que es lo que hay que recordar

> **Todo lo que deje de ser código y pase a ser dato se instala en el bloque de
> datos, no dentro de `if(WITH_PYTHON)`.** Si el dato vive bajo `scripts/` por
> historia, se excluye de la instalación de scripts y se añade al bloque de
> datos.

Y el corolario, que es el que de verdad duele: **un dato que no viaja no da
error**. Da un menú vacío. Por eso esto se verifica compilando de verdad, no
leyendo el `CMakeLists`.

### Verificado, con el binario sin Python en la mano

```
PATH=~/Flipendo/dev/toolchain/bin:$PATH \
  cmake --build ~/Flipendo/dev/build-nopy --target install -- -j5     # rc=0
```

Dentro de `build-nopy/bin/Blender.app/Contents/Resources/4.5/`:

| Qué | Cuántos |
|---|---:|
| `datafiles/manual/rna_manual_reference.txt` | 1 (481.199 bytes) |
| `scripts/presets/**/*.fpreset` | **167** |
| `scripts/presets/**/*.xml` (temas) | 2 |
| `scripts/presets/**/*.py` (los 5 de FFmpeg, a propósito) | 5 |
| `scripts/startup/**/*.blend` (plantillas) | 5 |
| Ficheros `.py` en **todo** el bundle | **5** |

Los 5 `.py` de FFmpeg se instalan a propósito: sin intérprete el lector nativo
los rechaza con un error visible, y un error visible es mejor que un preset que
desaparece del menú sin decir nada.

Y no basta con que estén: **el binario sin intérprete los encuentra y los usa**.

```
build-nopy/.../Blender --background --factory-startup \
  --fl-check-manual tests/flipendo/manual/baseline-python.txt
manual: 8023 rutas comparadas, 8024 identicas, 0 distintas (prefijo incluido)
manual: por url_lookup() (el camino del operador): 7470 identicas, 0 distintas,
        553 a la reserva del buscador

build-nopy/.../Blender --background --factory-startup \
  --fl-check-presets <bundle>/scripts/presets /tmp/informe.txt
aplicados sin error 151 de 167 (1 con error, 15 sin contexto)
```

Y esas cifras de presets son **exactamente** las mismas que da el binario **con**
Python sobre el árbol (`151 de 167, 1 con error, 15 sin contexto`): el camino
nativo no depende del intérprete ni un poco.

*Detalle de paso:* `nm -gU` sobre el binario sin Python da 72 aciertos de `_Py`,
y ninguno es CPython: son `_RNA_PythonController`, `_RNA_PythonProxy` y
`CTX_py_state_push/pop`, que son nombres del motor y del contexto, no símbolos
del intérprete.
