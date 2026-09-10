# Línea base de los campos numéricos

`baseline-python.txt` es lo que devuelve el binario **con** CPython al evaluar 119
cadenas de entrada de campo numérico. Es el objetivo que tiene que reproducir el
binario **sin** CPython, byte a byte.

## Por qué existe

Los campos numéricos de Blender no son campos numéricos: son campos de expresión.
Escribir `2*3` da 6 y escribir `5cm` da 0,05 m porque el texto se pasa por
`BPY_run_string_as_number()`. Al compilar con `WITH_PYTHON=OFF`, las dos llamadas que
lo hacían —`interface.cc:3462` y `numinput.cc:265`— caían a una rama `#else` que era
`*r_value = atof(str)`: `2*3` daba **2**, `1/2` daba **1** y `5cm` daba **5**, sin
error y sin aviso. Un número equivocado con pinta de bueno es peor que un fallo.

## Cómo se genera y cómo se comprueba

```
# línea base (binario CON Python)
dev/build/bin/Blender.app/Contents/MacOS/Blender --background --factory-startup \
    --fl-selftest-numinput tests/flipendo/numinput/baseline-python.txt

# comprobación (binario SIN Python)
dev/build-nopy/bin/Blender.app/Contents/MacOS/Blender --background --factory-startup \
    --fl-selftest-numinput /tmp/numinput-nopy.txt
diff tests/flipendo/numinput/baseline-python.txt /tmp/numinput-nopy.txt
```

`diff` vacío = los dos binarios entienden lo mismo.

La batería de casos vive en **C++**, no aquí: `cases[]` en
`source/blender/editors/util/fl_numinput_native.cc`. Este fichero es solo el resultado
congelado. Los ajustes de unidades son fijos (métrico, `scale_length` 1, sin división
en varias unidades, rotación en grados) para que el resultado no dependa del fichero
de arranque.

El volcado pasa por el punto de entrada de verdad, `user_string_to_number()`, no por
el evaluador a pelo: así comprueba de paso toda la cadena de unidades
(`BKE_unit_string_contains_unit` → `BKE_unit_replace_string` →
`BKE_unit_apply_preferred_unit`), que es la mitad del problema.

## Las dos secciones del fichero

- **`[cubiertos]`**: casos que los dos binarios tienen que resolver igual. Incluye los
  de error a propósito (`1/0`, `foo`, `2+`, `(`): que los dos **rechacen** también es
  parte del contrato — el fallo tiene que ser ruidoso en los dos lados.
- **`[no-cubiertos]`**: casos que divergen a sabiendas. Se listan con su motivo pero
  **sin** su resultado, para que el `diff` del resto siga siendo concluyente. El valor
  real de cada binario se imprime por salida estándar al ejecutar el volcado.

## Las cifras

**108 casos cubiertos, 108 idénticos**, `diff` vacío entre el binario con Python
(`dev/build`) y el binario sin Python (`dev/build-nopy`). Once casos más, declarados
divergentes a sabiendas, abajo.

## Las divergencias medidas, una a una

| Entrada | Con Python | Sin Python | Por qué |
|---|---|---|---|
| `2**3` | 8 | error | `BLI_expr_pylike` no tokeniza `**`; se escribe `pow(2,3)` |
| `7%3` | 1 | error | tampoco `%`; se escribe `fmod(7,3)` |
| `7//2` | 3 | error | tampoco `//`; se escribe `floor(7/2)` |
| `0x10` | 16 | error | literales hexadecimales: solo CPython |
| `1_000` | 1000 | error | separador de miles del literal: solo CPython |
| `round(2.5)` | 2 | 3 | CPython redondea al par (bancario); `round()` de C redondea hacia afuera. El resto de valores coincide |
| `lerp(0,10,0.25)` | error | 2,5 | divergencia **al revés**: `lerp` existe en `BLI_expr_pylike` y no en el `math` de CPython |
| `clamp(5,0,1)` | error | 1 | ídem |
| `smoothstep(0,1,0.5)` | error | 0,5 | ídem |
| `2,5` | 7 | error | CPython lo lee como tupla `(2,5)` y `PyC_RunString_AsNumber` **suma** las tuplas |
| `10km, 2m` | 10002 | error | el mismo caso, con unidades |

Ninguna de ellas devuelve un número equivocado sin avisar: o coinciden, o el binario
sin Python rechaza la entrada con un mensaje que dice qué se admite.

## Una trampa que costó una vuelta de medición

CPython **rechaza** una expresión que empieza por espacio o tabulador
(`IndentationError: unexpected indent`), y `BLI_expr_pylike` se los come sin
rechistar. Medido: teclear `␣␣42␣␣` en un campo de Blender con Python **no** funciona.
El evaluador nativo replica el rechazo (`fl_numinput_native.cc`) para no *ampliar* lo
que acepta el campo. El espacio final sí vale en los dos.
