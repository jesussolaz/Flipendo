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

---

## 2. El editor **escribía** Python

### Qué pasaba

Dos sitios generaban código Python en el disco del usuario:

1. **Guardar un preset.** `AddPresetBase.execute` escribía un `.py` con
   asignaciones. **Ya estaba arreglado** por el carril de presets: hoy escribe
   `.fpreset` con `WM_OT_preset_write`, un escritor nativo, y lee los `.py`
   antiguos sin intérprete. Ver `PRESETS-A-DATOS.md`.
2. **Exportar una configuración de teclado.**
   `PREFERENCES_OT_keyconfig_export` llamaba a
   `bl_keymap_utils/io.py: keyconfig_export_as_data()`, que escribía un `.py`
   con una lista literal `keyconfig_data` y, al final, este pie:

   ```python
   from bl_keymap_utils.io import keyconfig_import_from_data
   keyconfig_import_from_data(..., keyconfig_data, **keywords)
   ```

   O sea: el editor escribía un **programa**, y volver a importarlo era
   *ejecutarlo*. Esto es lo que quedaba abierto, y es peor que un `.py` en el
   árbol: un `.py` del árbol se puede borrar, pero los que fabrica el editor
   están en el disco de cada usuario.

### Cómo se arregló

`source/blender/windowmanager/keymap/fl_keyconfig_io.cc`:

- **Formato `.fkeyconfig`**, texto plano, una línea por elemento:
  `keymap "3D View" space=VIEW_3D region=WINDOW`, `item "…" type=… value=…` con
  los modificadores que estén puestos, y `prop "ruta" valor`. Se lee de un
  vistazo y `diff` es concluyente.
- **`PREFERENCES_OT_keyconfig_export` y `PREFERENCES_OT_keyconfig_import` son
  nativos**, con los mismos `idname` y las mismas propiedades (`all`,
  `keep_original`, el selector de ficheros). Las clases Python se retiraron de
  `bl_operators/userpref.py`.
- `AddPresetKeyconfig` pasa de `preset_ext = ".py"` a `".fkeyconfig"`, y cargar
  un preset de teclado ya no es `bpy.utils.keyconfig_set()` (que ejecutaba el
  fichero) sino el importador nativo.

**Decisiones que se tomaron, con su motivo:**

- Las propiedades de un atajo se leen y escriben **desde su `IDProperty`**, no
  pasando por el `OperatorProperties` de RNA. En un `IDProperty` «estar puesta»
  *es* estar en el grupo, que es justo lo que el Python comprobaba con
  `is_property_set()`; y además es exacto, porque no pasa por un `repr()` de
  Python y de vuelta, que era donde se podía perder precisión. Los grupos
  anidados se escriben con ruta con puntos (`prop "sub.valor" 3`).
- Los flotantes se escriben con la **representación más corta que vuelve a
  leerse como el mismo `float`**, la misma regla de `.fpreset` y la que tenía
  `repr_f32()` en el Python.
- Los tipos de evento, los valores y las direcciones se escriben como
  **identificadores de enumeración** (`MIDDLEMOUSE`, `CLICK_DRAG`), no como
  números: es lo que escribía el Python y no se rompe si un valor de DNA se
  mueve.

### Verificado

```
Blender --background --factory-startup --fl-check-keyconfig-io <informe>
keyconfig: 248 keymaps y 3674 atajos escritos en roundtrip.fkeyconfig
keyconfig: 248 keymaps y 3674 atajos escritos en roundtrip2.fkeyconfig
keyconfig: ciclo exportar-importar-exportar: 6681 lineas, 0 distintas
```

Exportar → importar → volver a exportar da **el mismo fichero, línea a línea**:
**248 keymaps, 3.674 atajos y 2.754 propiedades, 0 diferencias en 6.681 líneas**.
Si el ciclo perdiera un atajo, un modificador o una propiedad, el informe lo
diría. La línea base está en `tests/flipendo/keyconfig/baseline-nativo.fkeyconfig`.

Y el ciclo del usuario, de verdad, con el operador:

| Caso | Resultado |
|---|---|
| Exportar y volver a importar un `.fkeyconfig` | `{'FINISHED'}`, aparece la configuración nueva en `wm.keyconfigs` |
| Importar un `.py` exportado por el Python de antes | `{'FINISHED'}`, aparece la configuración — ver la nota de abajo |

### La compatibilidad hacia atrás: **leyendo**, nunca escribiendo

El editor **no vuelve a escribir ni un `.py`**. Lo que ya esté escrito se sigue
leyendo, y ahí hay una deuda declarada:

> El analizador nativo del `.py` heredado (`read_legacy_python_text`) **no está
> terminado**: se atraganta con el tercer elemento de un atajo que lleva
> propiedades (`{"properties": [...]}`), y da
> `linea 14: se esperaba una tupla de atajo` sobre un fichero exportado por el
> propio Blender. El fallo está localizado en el consumo de ese bloque y **no**
> en la estructura general (los keymaps, los argumentos de evento y los atajos
> sin propiedades se leen bien).

Mientras eso no esté cerrado, importar un `.py` heredado **no pierde nada**: el
lector nativo rechaza el fichero **en voz alta**, con fichero, línea y el texto
que no supo leer, y se cae al intérprete, exactamente igual que el último
recurso de los cinco presets de FFmpeg (`PRESETS-A-DATOS.md`). Es ruidoso a
propósito.

Lo que eso significa, dicho claro: **la escritura de Python está cortada; la
lectura del Python heredado todavía usa el intérprete para un caso.** Cuando el
analizador esté terminado, ese `#ifdef WITH_PYTHON` se va y con él el último
puente de esta familia.

### Lo que queda

1. Terminar `read_legacy_python_text` (arriba).
2. `--fl-check-keyconfig-io` acepta un segundo argumento con un `.py` heredado y
   compara su lectura nativa contra la exportación nativa. Hoy ese modo sale en
   rojo por lo mismo, y es justo el arnés que cerrará la deuda.
3. `bl_keymap_utils/io.py` (308 líneas) sigue en el árbol: ya no lo usa el
   editor para exportar, pero lo siguen leyendo `bl_ui/space_toolsystem_common.py`
   y `bpy_extras/keyconfig_utils.py`. Cae con `bl_ui`.
