# Métricas de composición del árbol de Flipendo

> ## ⚠️ El dato vivo ya no está aquí: está en [`ESTADO.md`](ESTADO.md)
>
> **Las tablas de este documento son historia**, congeladas al commit que dice cada
> una. Sirven para saber qué se midió, cuándo, y qué se creyó por error; **no** para
> saber cómo va el proyecto ahora. Para eso está [`ESTADO.md`](ESTADO.md), que no se
> escribe a mano: **se genera** desde el árbol con `flipendo-metrics --estado`
> (C++, en [`../tools/flipendo_metrics/`](../tools/flipendo_metrics/)) y lleva en la
> cabecera la fecha y el commit de su medición.
>
> El motivo está escrito abajo con nombres y cifras: este documento publicó el 8 de
> septiembre una cifra medida el 5 y se perdió por el camino una caída de 109.442
> líneas ([§6.1](#61-la-cifra-de-python-llevaba-dos-días-caducada)). Con siete carriles
> commiteando a la vez, **un documento a mano envejece en horas**. La solución no es
> escribir mejor; es generar.

> **Última medición a mano: 2026-09-11, commit `87ce606a318`.** La anterior era del 8
> de septiembre y llevaba tres días dando cifras que el árbol ya desmentía: qué decía,
> qué era falso y por qué, en el [§6](#6-qué-decía-la-versión-anterior-y-por-qué-estaba-mal).
> Lo que se midió después está en el [§7](#7-lo-que-se-midió-el-11-por-la-noche-y-no-cuadraba).

---

## 1. Cómo se mide, y por qué el método ha cambiado

**Se mide a un commit nombrado, nunca al árbol de trabajo.**

```sh
cd ~/Flipendo/dev/upbge
REV=$(git rev-parse --short HEAD)
# ficheros
git ls-tree -r --name-only $REV | grep -vE '^(extern|lib)/' | grep -cE '\.py$'
# líneas
git grep -c '' $REV -- '*' | sed "s|^$REV:||" | grep -vE '^(extern|lib)/' \
  | grep -E '\.py:[0-9]+$' | awk -F: '{s+=$NF} END{print s}'
```

Tres decisiones de método, cada una con su motivo:

1. **Al commit, no al *working tree*.** Mientras se escribía este documento, el carril
   METAL tenía cinco `.mm` borrados del disco y sus `.cc` todavía sin commitear
   (`mtl_state`, `mtl_shader_generator`, `mtl_shader_interface`, `mtl_index_buffer`,
   `mtl_uniform_buffer`). Una medición del árbol de trabajo con varios carriles vivos
   **no es reproducible ni por quien la hizo**: `wc -l` fallaba con
   `No such file or directory` sobre ficheros que `git ls-files` sí listaba. Se mide a
   un hash y el hash se escribe.
2. **Propio = todo salvo `extern/` y `lib/`.** Es la frontera que fija
   `LENGUAJE-CPP.md` en su «Decisión del 2026-09-11» («única excepción, y es
   doctrinal: `extern/` y `lib/` son terceros vendorizados») y que ya está en el
   árbol: `.gitattributes:107-108` los marca `linguist-vendored`. La versión anterior
   de este documento solo separaba `extern/`, así que metía `lib/` en el saco de
   Flipendo.
3. **Una línea es un `\n`**, como `wc -l`. Ver el §6.2: el contador del proyecto no
   lo hacía y sumaba una línea de más **por fichero**.

---

## 2. Flipendo — código propio (commit `87ce606a318`, 2026-09-11)

17.326 ficheros versionados fuera de `extern/` y `lib/`. De ellos, código:

| Lenguaje | Ficheros | Líneas | Doctrina |
|---|---:|---:|---|
| **C++** (`.cpp`/`.cc`/`.cxx`) | 4.313 | **2.540.538** | el estándar del árbol |
| `.hh` (cabecera C++) | 1.850 | 292.099 | C++; convive con `.hpp` |
| `.h` (cabecera heredada) | 1.389 | 248.745 | **deuda**: a `.hpp` al reescribir su subsistema |
| `.hpp` (cabecera propia) | 305 | 36.679 | la convención nueva |
| **Python** | 613 | **191.224** | **deuda**: objetivo 0 |
| **GLSL** | 745 | **68.391** | **deuda desde el 11**: cuerpos escritos a mano |
| **Objective-C++** (`.mm`) | 32 | **30.304** | **deuda desde el 11**: objetivo 0 |
| MSL (`.msl`) | 4 | 1.722 | pegamento del backend Metal |
| Metal (`.metal`) | 1 | 832 | kernel de Cycles, **no se compila** (`WITH_CYCLES=OFF`) |
| **C** (`.c`) | **0** | **0** | ✅ objetivo cumplido el 5 de septiembre |
| **shell** (`.sh`) | **0** | **0** | ✅ objetivo cumplido — **con el matiz de abajo** |
| Objective-C (`.m`), HLSL, Lua, JS | **0** | **0** | nunca existieron o ya no existen |

**Total de código propio: 3.410.534 líneas.**

- Familia C/C++ (con las `.h` heredadas dentro): **3.118.061 → 91,4 %**.
- Contando las `.h` heredadas como deuda, que es lo que dice la decisión del 11:
  **2.869.316 → 84,1 %**.
- No-C++ en sentido estricto (Python + GLSL + ObjC++ + MSL + Metal): **292.473 → 8,6 %**.

El `.metal` de Cycles (832 líneas) **no lo contaba el contador del proyecto**, que
solo conoce la extensión `.msl`. Aparece aquí por primera vez. No cambia nada
práctico —`WITH_CYCLES:BOOL=OFF` en `dev/build/CMakeCache.txt`, no entra en el
binario— pero una métrica que ignora una extensión entera no es una métrica.

### El matiz del shell: cero `.sh` no es cero shell

Las dos filas de cero de arriba son ciertas **por extensión**. Buscando por *shebang*
en vez de por extensión aparecen dos ficheros que ninguna tabla de este documento ve:

```sh
git grep -l -E '^#!.*(bash|/bin/sh|zsh|ksh)' HEAD -- '*' \
  | sed 's|^HEAD:||' | grep -vE '^(extern|lib)/'
```

- `tools/flipendo_cli/flipendo.bash.legacy` (127 líneas): el gestor de versiones en
  bash, **conservado a propósito** como registro de lo que se migró a
  `flipendo_cli.cpp`. No se ejecuta ni se instala.
- `release/darwin/scripts/blender-system-info.sh.in` (16 líneas): plantilla que CMake
  configura para el informe de sistema del bundle de macOS. Heredada de Blender.

**143 líneas, ninguna en el motor ni en el juego.** El objetivo «cero shell propio» se
da por cumplido sabiendo esto, no por no haber mirado — que es exactamente la
diferencia entre una métrica y un titular. Registradas en
[`REGISTRO-LEGACY.md`](REGISTRO-LEGACY.md).

---

## 3. Terceros vendorizados (`extern/` + `lib/`) — NO son de Flipendo

2.564 ficheros versionados. **Se mantienen verbatim y se actualizan desde upstream**;
reescribirlos a mano no es propiedad del código, es asumir el mantenimiento de
bibliotecas ajenas (`LENGUAJE-CPP.md`, decisión del 2026-09-11).

| Lenguaje | Ficheros | Líneas |
|---|---:|---:|
| `.h` | 1.385 | 495.108 |
| C++ | 773 | 298.200 |
| `.hpp` | 155 | 65.387 |
| C | 24 | 58.188 |
| Python | 19 | 1.593 |
| shell | 6 | 792 |

**Total vendorizado: 919.268 líneas.** Las 58.188 de C y las 792 de shell son las
únicas de esos dos lenguajes que quedan en todo el repositorio: **fuera de `extern/`
no hay ni una**. Por eso las filas de C y de shell del §2 valen cero y no es un error
de medida.

Aquí dentro está, desde esta mañana, `extern/metal-cpp`: el binding oficial de Apple
que hace posible el objetivo «cero Objective-C++» (ver `OBJC-A-CPP.md`).

---

## 4. Evolución medida

Todas las cifras de esta sección están medidas con el método del §1 sobre el commit
indicado. **No están copiadas de ningún documento anterior**, y por eso no coinciden
exactamente con las que circulan por el repo: ver §6.2.

### Python propio

| Commit | Fecha | Ficheros | Líneas | Qué pasó |
|---|---|---:|---:|---|
| `d2a1ca3c2b7` | 05 sep 17:56 | 1.389 | **492.366** | línea base, antes de la poda Mac-only |
| `291e35d2bd4` | 05 sep 20:01 | 1.008 | **357.084** | poda Mac-only (20 lotes) + `svn_rev_map` a dato binario |
| `59a58f78996` | 09 sep 23:47 | 1.003 | **247.642** | día 9: retirada que **nunca se registró aquí** (−109.442) |
| `4e88d96a431` | 10 sep 23:19 | 1.013 | **248.065** | arranque de la noche del 10 al 11 |
| `bb2bcb1852e` | 11 sep 06:25 | 655 | **195.611** | **la noche: −52.454 líneas, −358 ficheros** |
| `87ce606a318` | 11 sep 15:46 | 613 | **191.224** | mañana del 11 (plantillas, `templates_py`) |

De 492.366 a 191.224 en seis días: **−301.142 líneas, el 61,2 %**. La noche del 10 al
11 aporta 52.454 de ellas; el resto es poda previa y el día 9.

### El resto

| Lenguaje | 05 sep 17:56 | 10 sep 23:19 | 11 sep 15:46 | Nota |
|---|---:|---:|---:|---|
| C++ | 2.510.771 | 2.497.382 | **2.540.538** | baja con la poda, sube con lo que sustituye al Python |
| `.mm` | 30.813 (37 f.) | 30.536 (34 f.) | **30.304 (32 f.)** | los dos primeros ficheros del backend Metal, ya en C++ puro |
| GLSL | 68.391 | 68.391 | **68.391** | **intacto**: la decisión del 11 abre este frente, no lo ha tocado nadie todavía |
| `.h` heredadas | 269.005 | 248.745 | **248.745** | intacto desde la poda del 5 |
| `.hpp` propias | 33.880 | 35.019 | **36.679** | +2.799 en seis días |
| C | 5.457 (22 f.) | 0 | **0** | cerrado el 5 de septiembre |

Que GLSL y `.h` lleven seis días clavadas no es un descuido: eran deuda *aceptada*
hasta la decisión del 11 de la mañana. Desde hoy son deuda *abierta*, y esta tabla
es el sitio donde se verá si se mueven.

---

## 5. Deuda abierta, según la decisión del 2026-09-11

`LENGUAJE-CPP.md` fija el objetivo: cero Python, cero C propio (✅), cero
Objective-C++, cero shell propio (✅), cero GLSL a mano, y las `.h` heredadas a
`.hpp` según se reescriba su subsistema.

| Frente | Hoy | Documento | Estado |
|---|---:|---|---|
| Python | 191.224 | `BACKLOG-EDITOR-PYTHON.md`, `PLAYER-SIN-CPYTHON.md` | en marcha; el Player de distribución ya no lleva intérprete |
| `.h` heredadas | 248.745 | `MIGRACION-CPP.md` | sin empezar |
| GLSL | 68.391 | `MIGRACION-CPP.md` §4 (derogado) | sin empezar; `source/gameengine` solo aporta 183 |
| Objective-C++ | 30.304 | `OBJC-A-CPP.md` | en marcha; el trabajo real son **441 envíos de mensaje**, no 30.000 líneas |
| C propio | **0** | — | ✅ cerrado 2026-09-05 |
| shell propio | **0** | `SHELL-Y-PLANTILLAS-CPP.md` | ✅ cerrado |

---

## 6. Qué decía la versión anterior y por qué estaba mal

Este documento se escribió el 5 de septiembre y se actualizó por última vez el 8. Se
corrige aquí en vez de borrarlo, porque quien lo leyó tomó decisiones con él.

### 6.1 La cifra de Python llevaba dos días caducada

Decía: *«Snapshot 2026-09-08 … Python 1008 358088»*, y de ahí colgaba todo
(«Python mantenido: 493.742 → editor Blender», «el editor son ~293k líneas»).

**Medido:** ese 357.084 real corresponde al commit `291e35d2bd4`, del **5 de
septiembre a las 20:01**, no al 8. El 8 de septiembre por la noche el árbol ya
estaba en 247.999, y el 9 en 247.642. Es decir: **el documento fechado el 8 daba una
cifra de tres días antes y se perdió una caída de 109.442 líneas** que nadie anotó.
Consecuencia práctica: `BACKLOG-EDITOR-PYTHON.md` calificó de «plurianual» un trabajo
cuyo tamaño ya era un 30 % menor cuando se escribió la frase.

### 6.2 El contador suma una línea de más por fichero

`tools/flipendo_metrics/flipendo_metrics.cpp` hacía:

```cpp
while (f.get(c)) { any = true; if (c == '\n') ++n; }
return n + (any ? 1 : 0);      // ← +1 en todo fichero no vacío
```

Un fichero que termina en `\n` —o sea, todos— tiene sus líneas **ya contadas**; ese
`+1` es de más. **Comprobado, no deducido:** un fichero de tres líneas hecho a mano
devolvía `4`. Y sobre el árbol entero, contra `wc -l`:

| Lenguaje | `flipendo-metrics` (antes) | `wc -l` | Diferencia | Ficheros |
|---|---:|---:|---:|---:|
| Python | 191.834 | 191.224 | +610 | 610 no vacíos |
| GLSL | 69.136 | 68.391 | +745 | 745 |
| `.hpp` | 36.984 | 36.679 | +305 | 305 |
| Objective-C++ | 30.336 | 30.304 | +32 | 32 |
| MSL | 1.726 | 1.722 | +4 | 4 |

La diferencia **es exactamente el número de ficheros**, en las cinco filas. Por eso
todas las cifras históricas del repo van ligeramente altas (493.742 frente a 492.366;
358.088 frente a 357.084; 249.382 frente a 248.065 — esa última además sumaba el
Python de `extern/`).

**Corregido hoy** en el contador, junto con dos fallos más de la misma familia:

- **No excluía `lib/`**, solo `/extern/`, así que contra la doctrina del 11 metía
  librerías precompiladas en el saco de Flipendo. (En la práctica no se notaba
  porque `lib/macos_x64` es un enlace simbólico y `recursive_directory_iterator` no
  lo sigue — pero el criterio estaba mal escrito.)
- **No conocía `.metal` ni `.sh`**: el kernel de Cycles (832 líneas) y cualquier
  script de shell eran invisibles. Un objetivo doctrinal («cero shell propio») que el
  contador no sabe medir no se puede dar por cumplido.

El contador no está en ningún `CMakeLists.txt` —es una herramienta suelta— así que se
compila a mano:

```sh
c++ -std=c++17 -O2 -o ~/Flipendo/bin/flipendo-metrics \
    ~/Flipendo/dev/upbge/tools/flipendo_metrics/flipendo_metrics.cpp
flipendo-metrics ~/Flipendo/dev/upbge
```

**Aun así, el contador no era la fuente de este documento.** Recorría el disco, así
que veía ficheros sin versionar y no veía lo que otro carril acababa de borrar. Las
tablas de arriba salen de `git`, a un commit; el contador servía para una
comprobación rápida.

**Eso cambió la noche del 11.** El contador ya no recorre el disco: mide con `git` a
un commit nombrado —`git ls-tree -r` y un solo `git cat-file --batch`, sin pasar por
el shell— que es exactamente el método del §1 de este documento, y además genera
[`ESTADO.md`](ESTADO.md) entero. O sea: el método que este documento describía a mano
**es ahora código**, y lo que antes había que copiar a una tabla se regenera con un
comando. El modo viejo sigue disponible con `--disco`, con su aviso.

### 6.3 El objetivo «C → 0» seguía descrito como pendiente en la cabecera

La tabla de la línea base decía «**C** 22 / 5.457» y el texto de objetivos vivos
«migrar los compilados en Mac (auditoría en curso)», mientras que 60 líneas más abajo
el snapshot del 8 ya decía que era 0. **Medido:** 0 desde `291e35d2bd4`, el 5 de
septiembre a las 20:01. El documento se contradecía a sí mismo dentro de la misma
página.

### 6.4 Lo que decía de `extern/` sigue siendo cierto

La única parte que ha aguantado: C++ 773 ficheros, C 24 ficheros / 58.211 líneas
(58.188 con `wc -l`), Python 18-19 ficheros. Ni se ha tocado ni se va a tocar.

---

## 7. Lo que se midió el 11 por la noche y no cuadraba

Al escribir el generador hubo que medir de verdad cosas que hasta ahora se afirmaban
de palabra. Tres no cuadraban. Se dejan aquí con el mismo formato que el §6: qué
decía, qué mide el árbol, cómo se comprobó.

### 7.1 «46 verificadores» eran 32 comprobadores y 14 volcadores

**Decía** el `README.md`, en una insignia: `verificadores-46`.

**Mide el árbol:** cuando se midió (commit `35309693a44`), en
`source/creator/creator_args.cc` había **50 banderas `--fl-*`** registradas con
`BLI_args_add`. De ellas, **32** daban veredicto (16 `--fl-check-*` +
16 `--fl-selftest-*`), **14** eran volcadores `--fl-dump-*` y **4** conversores o
preparadores de escena (`--fl-convert-presets`, `--fl-convert-manual-reference`,
`--fl-make-ui-scene`, `--fl-ui-scene`). El 46 salía de sumar los comprobadores y los
volcadores.

**Y ya no son esas cifras**, que es justo el problema de escribirlas a mano: al cerrar
la noche son 61 banderas, 41 comprobadores y 16 volcadores. Por eso la insignia del
`README.md` se corrige cuando cambia, pero **el recuento vivo está en
[`ESTADO.md`](ESTADO.md)**, que lo cuenta solo en cada pasada y avisa si la insignia
se ha quedado atrás.

**Por qué importa y no es una pedantería:** un volcador **no se puede poner en
verde**. Escribe estado y sale con 0 haga lo que haga. Decir «46 verificadores» y
preguntar «¿cuántos están en verde?» son dos frases que no se pueden juntar sin
contar 14 verdes que nadie ha comprobado.

**Cómo se comprobó:** contando las cadenas `"--fl-..."` del fichero al commit medido.
Lo hace el generador en cada pasada y lo vuelve a decir si vuelve a descuadrar. La
insignia del `README.md` ya está corregida a `comprobadores-32` + `volcadores-14`.

### 7.2 Un `--fl-selftest-*` no es una prueba: es un volcador que siempre sale con 0

**Se venían contando** los 16 `--fl-selftest-*` junto a los `--fl-check-*` como si
fueran pruebas.

**Mide el árbol —ejecutándolos—:** los dieciséis piden un fichero de salida y, si no
se lo das, imprimen `Error: falta el fichero de salida después de '--fl-selftest-X'`
y **salen con 0**. Un arnés que mire el código de salida los da todos por verdes sin
haber comprobado nada. Y dos de ellos (`--fl-selftest-context-ops` y
`--fl-selftest-wm-property-ops`) **no escriben nada en `--background`**: necesitan un
editor real, así que en modo consola el fichero se queda vacío y el verde es doblemente
falso.

**Cómo se comprobó:** ejecutando los 16 con y sin argumento, en `--background` y en
modo gráfico, y comparando cada volcado byte a byte contra todos los `.txt` de
`tests/flipendo/`. Así se identificó además con qué línea base va cada uno (por
ejemplo `--fl-selftest-wm-system-ops` →
`tests/flipendo/operators/system-python.txt`). El generador hace justo eso: el
veredicto de un selftest sale de comparar su volcado, no de su código de salida.

> **ARREGLADO la misma noche, y no por este carril.** El carril ARNES tiró del hilo y
> midió que el agujero era mucho mayor: **58 de las 59 opciones `--fl-*` salían con 0
> sin haber comprobado nada**. Están cerradas (commits `8cc31792ac6`, `ab66896ec20`,
> `2dd540193dc`), con sus reglas escritas en
> [`ARNES-A-PRUEBA.md`](ARNES-A-PRUEBA.md): un comprobador tiene que fallar cuando
> debe, y comparar cero líneas dejó de ser un aprobado. Este apartado se queda como
> registro de por dónde salió, no como estado actual: **el estado actual está en
> [`ESTADO.md`](ESTADO.md)**, que ejecuta la batería entera y publica el recuento.

### 7.2.bis Las declaraciones: tolerancia y divergencias deliberadas

De arreglar el arnés salieron dos ficheros que acompañan a una línea base y que
**cualquier arnés que mida tiene que honrar** o publicará rojos falsos:

- `<linea-base>.tolerancia` — hoy solo
  `tests/flipendo/meshops/baseline-python.txt.tolerancia` (2e-5 relativa). Es la
  respuesta medida a la inestabilidad que este carril había localizado en la línea
  10866 (`0.451384932` contra `0.451389611`): no se cuantizó porque a 5 decimales los
  valores siguen difiriendo y a 4 se tirarían cifras válidas de los 13.111 estables.
- `<linea-base>.divergencias` — hoy solo
  `tests/flipendo/operators/execution-python.txt.divergencias`, donde se declara que
  `wm.context_cycle_array` da `[2,3,1]` **a propósito**, porque el C++ corrige un
  fallo del Python al rotar tuplas. La línea base del Python no se toca: es la prueba.

`flipendo-metrics` los lee y los honra desde el commit `35a7605177c`, y publica en
`ESTADO.md` cuántas hay, en qué fichero, quién las usa y **en cuántas pasadas de
cuántas hizo falta cada una**. Una declaración escondida es una excusa; una declarada
y contada es una decisión de ingeniería — y una que deja de hacer falta sale en el
informe, porque significa que algo cambió por debajo.

### 7.3 «`intern/cycles` tiene 0 referencias en `build.ninja`» es falso; lo cierto es «0 objetos»

**Decía** `OBJC-A-CPP.md:1076`: las 5.060 líneas de `.mm` de `intern/cycles` tienen
«**0 referencias en `build.ninja`**».

**Mide el árbol:** `grep -c 'intern/cycles' dev/build/build.ninja` da **202**. Son
rutas `-I` dentro de las líneas `INCLUDES` de otros objetivos (todas a
`intern/cycles/blender`), no compilaciones. Lo que sí es cero, y es lo que importa,
son los **objetos**: ninguna arista `build ... .o:` de las 5.247 del fichero tiene
como entrada un fuente de `intern/cycles`, y `WITH_CYCLES:BOOL=OFF` en
`CMakeCache.txt`. La conclusión del documento —esas 5.060 líneas no entran en el
binario— **es correcta**; la prueba que daba, no. El grep estrecho que sí da 0 es el
que usa `REGISTRO-LEGACY.md`: `grep -c 'cycles/device/metal' dev/build/build.ninja`.

**Cómo se comprobó:** el generador parsea `build.ninja`, separa «citado en cualquier
sitio» de «es entrada de una arista de objeto», y publica las dos columnas. La
diferencia entre las dos es literalmente la diferencia entre «queda trabajo» y «queda
código muerto».

**No se ha corregido `OBJC-A-CPP.md` en el sitio**: es de otro carril y tocarlo con el
índice compartido arriesga arrastrar su trabajo a medias (la lección de las 01:05 y la
de las 04:50). La corrección queda aquí y en [`ESTADO.md`](ESTADO.md) §4, que lo mide
en cada pasada.

### 7.4 Y el método del §1 de este documento tampoco es exacto del todo

El §1 manda contar con `git grep -c ''` y a la vez dice «una línea es un `\n`, como
`wc -l`». **No son lo mismo**: en un fichero cuya última línea no acaba en `\n`,
`git grep -c ''` cuenta esa línea y `wc -l` no. El generador implementa la semántica
declarada (`wc -l`, contar `\n`), no la del comando de ejemplo.

**Comprobado midiendo el mismo commit `87ce606a318` de la cabecera con la herramienta
nueva**, y el resultado deja el alcance exacto del desliz:

| | §2 (a mano, `git grep -c ''`) | generador (`wc -l`) | diferencia |
|---|---:|---:|---:|
| Código **propio**, las diez filas | 3.410.534 | **3.410.534** | **0** |
| Vendorizado, total | 919.268 | 919.197 | 71 |

En el código propio **no hay ni una línea de diferencia**: las diez filas del §2
—C++ 2.540.538, `.hh` 292.099, `.h` 248.745, `.hpp` 36.679, Python 191.224, GLSL
68.391, ObjC++ 30.304, MSL 1.722, Metal 832, C 0— salen idénticas. O sea que **ningún
fichero propio se queda sin salto de línea final**, y las dos mediciones, la de la
mano y la de la máquina, se confirman la una a la otra. Las 71 líneas de diferencia
están todas en `extern/` y `lib/` (26 en las `.h`, 35 en el C++, 4 en `.hpp`, 5 en
Python, 1 en C), que es código ajeno y no se toca.
