# Editor sin CPython — lo que se pierde en silencio, y lo que ya no

> Hermano de `PLAYER-SIN-CPYTHON.md`. Aquel documenta el Player, que ya existe sin
> intérprete desde el 2026-09-08. Este documenta el **editor**, que todavía no puede,
> y lleva la cuenta honesta de qué se rompe al apagar `WITH_PYTHON` y con qué
> evidencia se ha comprobado cada arreglo.
>
> El inventario (`INVENTARIO-PYTHON.md` §5) ordena los daños en cinco bloques. Este
> documento es el diario del **bloque 1**: lo que falla *en silencio*, que va primero
> precisamente porque no da error.

---

## 1. Por qué el bloque 1 va primero

Un fallo ruidoso cuesta una tarde. Un fallo silencioso cuesta un `.blend` corrupto y
tres semanas de desconfianza. Al apagar `WITH_PYTHON` se activan ramas `#else` que en
un build normal **no se compilan nunca**, y por eso llevaban años sin que nadie las
mirara. Dos de ellas no fallaban: devolvían un resultado incorrecto con pinta de
correcto.

---

## 2. Los campos numéricos — ARREGLADO (2026-09-11)

### Qué pasaba

Los campos numéricos de Blender no son campos numéricos: son campos de **expresión**.
`2*3` da 6, `1/2` da 0,5 y `5cm` da 0,05 m porque el texto se pasa por
`BPY_run_string_as_number()`. Hay exactamente dos puntos de entrada:

| Fichero:línea (antes del arreglo) | Función | Para qué |
|---|---|---|
| `source/blender/editors/interface/interface.cc:3462-3475` | `ui_number_from_string()` | Teclear en un campo de la interfaz |
| `source/blender/editors/util/numinput.cc:265-298` | `user_string_to_number()` | Entrada numérica de las operaciones modales (G/R/S) y camino con unidades |

Las dos tenían la misma rama `#else`:

```c
UNUSED_VARS(C, unit, type, use_single_line_error, r_error);
*r_value = atof(str);
return true;
```

`atof("2*3")` es **2**. `atof("5cm")` es **5**. `return true` — sin error, sin aviso,
sin nada en el editor de Info. Y en `numinput.cc` el `#ifdef` envolvía la función
**entera**, así que se saltaba de paso toda la conversión de unidades: la escala de
escena, la unidad preferida y `BKE_unit_replace_string`.

### Cómo se arregló

El árbol ya traía las dos piezas:

- **`BLI_expr_pylike_eval`** (`source/blender/blenlib/intern/expr_pylike_eval.cc`):
  evaluador en C++ del subconjunto de expresiones Python que se calcula en doble
  precisión. No es código nuevo ni experimental: es el mismo que ya usan los drivers
  simples desde 2018.
- **`BKE_unit_replace_string`**: traduce `5cm` a `(5*0.01)*1` antes de evaluar.

El arreglo (`source/blender/editors/util/fl_numinput_native.cc`, con la cabecera
`source/blender/editors/include/FL_numinput_native.hh`) es el pegamento entre las dos,
con la misma semántica observable que `PyC_RunString_AsNumber()`.

**La decisión estructural que más importa**: en `numinput.cc` el `#ifdef WITH_PYTHON`
ya no envuelve la función entera, sino **solo la evaluación de la expresión**. Toda la
lógica de unidades es ahora literalmente la misma en los dos builds y solo cambia
quién hace la cuenta. Eso quita de un plumazo la clase de bug que había: una rama
`#else` que reimplementa mal lo que la otra hace bien.

### Verificación

Opción nueva `--fl-selftest-numinput <fichero>`, en el estilo de `--fl-dump-keymap`:
la batería de casos vive en C++ (`cases[]` en `fl_numinput_native.cc`) y el volcado
pasa por el punto de entrada de verdad, `user_string_to_number()`, no por el evaluador
a pelo — así comprueba de paso toda la cadena de unidades.

```
dev/build/bin/Blender.app/.../Blender --background --factory-startup \
    --fl-selftest-numinput tests/flipendo/numinput/baseline-python.txt
dev/build-nopy/bin/Blender.app/.../Blender --background --factory-startup \
    --fl-selftest-numinput /tmp/numinput-nopy.txt
diff tests/flipendo/numinput/baseline-python.txt /tmp/numinput-nopy.txt
```

**133 casos cubiertos, 133 idénticos, `diff` vacío.** Nueve casos más se declaran
divergentes por escrito (tabla en `tests/flipendo/numinput/README.md`); ninguno de
ellos devuelve un número equivocado callando: o coinciden o se rechaza la entrada.

### Lo que NO se cubre, y por qué

No se cubren los literales `0x10` ni `1_000`, ni las listas separadas por comas
(`10km, 2m`), que CPython lee como tupla y **suma**. `round()` de un `.5` exacto
difiere: C redondea hacia afuera y CPython al par. Y `lerp`, `clamp` y `smoothstep`
divergen al revés: existen en `BLI_expr_pylike` y no en el `math` de CPython.

`**`, `%` y `//` **sí se cubren desde el 2026-09-11**: ver §6.

### La trampa que costó una vuelta de medición

CPython **rechaza** una expresión que empieza por espacio o tabulador
(`IndentationError: unexpected indent`); `BLI_expr_pylike` se los come sin rechistar.
Medido, no supuesto: con Python, teclear `␣␣42␣␣` en un campo **no** funciona. El
evaluador nativo replica el rechazo, porque el objetivo es *igualar* lo que acepta el
campo, no *ampliarlo*. El espacio final sí vale en los dos.

---

## 3. Los drivers — YA NO ES SILENCIOSO (2026-09-11)

### Qué pasaba

`source/blender/blenkernel/intern/fcurve_driver.cc`, `evaluate_driver_python()`.
Primero se intenta `driver_try_evaluate_simple_expr()` (nativo, `BLI_expr_pylike`) y
**solo si falla** se llama a `BPY_driver_exec()`. Sin Python la rama era
`UNUSED_VARS(anim_rna, anim_eval_context)`, es decir: nada.

**Corrección al inventario**, medida en el código: `INVENTARIO-PYTHON.md` §5 dice que
el driver «conserva su valor anterior». No es exacto. `driver_try_evaluate_simple_expr()`
hace `*result = 0.0f` **antes** de intentar nada, y `result` es `&driver->curval`. Así
que el driver se quedaba clavado en **0.0**, no en su valor previo. Da igual para el
usuario —sigue siendo silencioso y falso— pero conviene que la política diga la verdad.

### Qué subconjunto se evalúa de forma nativa, decidido y escrito

| Tipo de driver | Sin CPython | Dónde |
|---|---|---|
| `DRIVER_TYPE_AVERAGE`, `SUM`, `MIN`, `MAX` | **Funciona** | `evaluate_driver_sum/minmax`, C++ puro |
| Variables de driver (`driver_f-curve`, transformaciones, distancias, contexto) | **Funciona** | `driver_get_variable_value`, C++ puro |
| `DRIVER_TYPE_PYTHON` con expresión dentro de `BLI_expr_pylike` | **Funciona** | `driver_try_evaluate_simple_expr` |
| `DRIVER_TYPE_PYTHON` con `**`, `%`, `//`, `bpy.*`, `self`, indexaciones, funciones propias | **Avisa y vale 0.0** | rama nueva |

Es decir: **la mayoría de un rig sigue funcionando**. Lo que cae fuera es la expresión
que necesita un intérprete de verdad, y para ésa la doctrina es la de siempre — que se
note.

### Cómo deja de ser silencioso

Siguiendo el precedente ya establecido del proyecto —`SCA_PythonController::Trigger()`
avisa una vez por controlador en el build sin CPython en vez de callar— la rama nueva:

1. **Avisa una vez por driver**, no una por fotograma. La clave del registro es
   `<nombre del ID> + <propiedad RNA> + <expresión>`, en un `blender::Set<std::string>`
   con cerrojo, porque los drivers se evalúan desde varios hilos del depsgraph.
2. El aviso dice **qué driver** es y **qué hacer**: la lista de lo que sí admite el
   evaluador nativo, `pow(a,b)` en vez de `a**b`, `fmod(a,b)` en vez de `a%b`, y que
   la lógica de verdad se pasa a una variable de driver o a un componente nativo.
3. Marca `DRIVER_FLAG_INVALID` **en la copia evaluada** (no en el original, que se
   guardaría en el `.blend`), igual que ya hace `driver_evaluate_simple_expr()` con una
   división por cero. Así la interfaz lo pinta en rojo en vez de fingir que va bien.

---

## 4. Lo que queda del bloque 1, y lo que se sabe del resto

- **Bloque 1 punto 3** — los 129 menús del keymap y los 15 operadores que hoy son
  Python: la tecla no hace nada y tampoco avisa. Es `bl_ui`, no se arregla aquí.
- **Bloque 4 — el arranque**: la marca de fase `CTX_py_init_get` de
  `WM_keyconfig_init` (`wm.cc:470-479`). Sin ella el keymap por defecto no se
  construye nunca en un build sin Python. Ver §5.
- **Bloques 2, 3 y 5**: siguen tal cual los describe `INVENTARIO-PYTHON.md`.

---

## 5. La marca de fase del keymap — HECHA (2026-09-11), y una corrección

`WM_keyconfig_init()` se llama dos veces durante el arranque y solo la segunda vale:
en la primera todavía no se han registrado los operadores MACRO
(`ED_spacemacros_init`, `wm_init_exit.cc:316`) y construir el keymap ahí deja 502
líneas de diferencia. La guarda `CTX_py_init_get(C)` servía de marca de fase **por
casualidad**: `CTX_py_init_set(C, true)` está en `wm_init_exit.cc:344`, entre
`ED_spacemacros_init()` y la segunda llamada.

### La corrección al inventario

`INVENTARIO-PYTHON.md` §4.1 da esto por «bloqueante duro» y dice que con
`WITH_PYTHON=OFF` el keymap por defecto **no se construye jamás**. Para el **editor
no es cierto**, y se ha comprobado en el código, línea a línea: la llamada
`CTX_py_init_set(C, true)` de `wm_init_exit.cc:344` **no está dentro de ningún
`#ifdef WITH_PYTHON`**. El editor sin Python también ponía la marca.

Donde sí era cierto es en el **Player**: en `GPG_ghost.cpp:1704` ese
`CTX_py_init_set()` sí vive dentro de `#ifdef WITH_PYTHON`, así que el Blenderplayer
sin CPython llegaba a `WM_keyconfig_init()` (línea 1731) con la marca en falso y se
quedaba sin mapa de teclado por defecto. Ése sí era un fallo real, y silencioso.

### Lo que se hizo

Sustituir la guarda por una marca de fase **explícita**
(`WM_keyconfig_init_phase_ready_set()`, declarada en `WM_keymap.hh`), puesta en el
punto exacto donde ya se llamaba a `CTX_py_init_set()` — para que el build con Python
quede en el mismo instante y el volcado salga idéntico — y **fuera** del `#ifdef` en
el arranque del Player. Es estado de **proceso**, un `static bool` en `wm.cc`, no un
`WM_INIT_FLAG_*` en `wm->init_flag`: ese campo se reinicia al leer un `.blend` y la
fase de arranque no.

### Verificación

- `--fl-check-keymap` con Python: 248 keymaps transliterados, 248 idénticos, 0 con
  diferencias. Igual que antes.
- `--fl-dump-keymap` —que vuelca la configuración **activa**, la que solo existe si
  `WM_keyconfig_init` ha corrido de verdad— con Python: `keymaps=248 items=3673`,
  idéntico byte a byte al binario de referencia guardado antes de tocar nada.
- El mismo `--fl-dump-keymap` sin Python: `keymaps=248 items=3673`.

Ojo con la diferencia entre las dos opciones: `--fl-check-keymap` construye el keymap
nativo en una configuración **aparte** (`FL_keyconfig_check_native`, `wmKeyConfig`
nuevo + `flipendo::keymap::register_default`), así que pasa igual con la guarda y sin
ella: **no** prueba este cambio. La que lo prueba es `--fl-dump-keymap`.

### De regalo: la primera medición del keymap del editor sin CPython

Comparar `--fl-dump-keymap` de los dos binarios da la primera lista **medida** de qué
atajos pierde el editor sin Python. El keymap por defecto es idéntico en cuenta
(248 / 3.673) y las únicas diferencias son dos, las dos por operadores que hoy son
Python:

1. **`OBJECT_OT_select_hierarchy` pierde su propiedad `direction`** en sus cuatro
   atajos (`[` `]` con y sin Mayús), en las tres configuraciones. El operador vive en
   `bl_operators`, así que sin Python su RNA no existe y la propiedad enumerada no se
   resuelve; `extend` sobrevive. Es la confirmación empírica del punto 5 del camino
   crítico del inventario: «atajos que hoy funcionan y mañana no, en silencio».
2. **La configuración `addon` se queda vacía** (de 2 keymaps y 5 atajos a 0): son los
   de la biblioteca de poses (`POSELIB_OT_apply_pose_asset`,
   `POSELIB_OT_blend_pose_asset`), que registra un add-on de Python. La configuración
   `user` baja de 3.674 a 3.673 atajos por el mismo motivo.

Nada más. Todo lo demás del mapa de teclado del editor ya es nativo.


---

## 6. `BLI_expr_pylike` aprende `**`, `%` y `//` (2026-09-11)

Era lo único que quedaba fuera del subconjunto por un motivo de gramática, no de
diseño: el tokenizador no reconocía los operadores de dos caracteres y el analizador no
tenía nivel de potencia. Tres operadores, 25 líneas de evaluador.

### Por qué se hizo aparte, y con qué red

Ese fichero es **también el camino rápido de los drivers en el build con Python**:
`driver_try_evaluate_simple_expr()` lo intenta antes que CPython. Ampliar la gramática
no puede romper nada que hoy funcione —lo que antes era error de sintaxis ahora se
evalúa, nunca al revés— pero **sí cambia quién evalúa** un driver con `**`: antes
CPython, ahora C++. Por eso el listón no era «que compile» sino «que dé exactamente el
mismo número que CPython», y para eso ya existía el arnés.

### Lo que no es obvio: la semántica de Python no es la de C

Copiar `fmod` y `pow` a secas habría sido otro fallo silencioso, justo la clase de cosa
que este trabajo está quitando. Medido caso a caso contra el binario con CPython:

| Expresión | CPython | `fmod`/`pow` de C a secas | Lo que se implementó |
|---|---:|---:|---|
| `-7 % 3` | 2 | −1 | módulo con **suelo**: el signo es el del divisor |
| `7 % -3` | −2 | 1 | ídem |
| `-7 // 2` | −4 | −3 | `floor(a/b)`, no truncar |
| `-2**2` | −4 | 4 | `**` liga más que el menos unario de su **izquierda** |
| `2**-1` | 0,5 | — | ...y menos que el de su **derecha** |
| `2**3**2` | 512 | 64 | asociativo por la **derecha** |
| `2*3**2` | 18 | 36 | `**` liga más que `*` |

La precedencia sale sola escribiendo la gramática de CPython tal cual
(`power ::= primary ["**" u_expr]`, `u_expr ::= power | "-" u_expr`), que es lo que se
hizo: `parse_unary()` se partió en `parse_primary()` + `parse_power()` + `parse_unary()`.

### Verificación

25 casos nuevos en la batería de `--fl-selftest-numinput`, incluidos los seis de
precedencia de la tabla, los cuatro signos de `%` y de `//`, y tres de error
(`7%0`, `7//0`, `0**-1`, `(-8)**(1/3)`, que CPython convierte en complejo).
**133 casos cubiertos, 133 idénticos, `diff` vacío** entre los dos binarios. Y de los
108 casos que ya estaban antes, **ninguno cambió de valor**: las únicas tres filas que
se mueven son precisamente `2**3`, `7%3` y `7//2`, que pasan de la sección de
divergencias declaradas a la de cubiertos.

### Lo que sigue fuera

`int ** int` con resultado enorme (`3**500`): CPython lo calcula exacto con enteros de
precisión arbitraria y redondea al final; aquí es `pow()` en doble desde el principio.
En el caso medido los dos dan el mismo doble, pero por suerte, no por construcción, así
que se queda declarado divergente.
