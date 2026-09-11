# Métricas de composición del árbol de Flipendo

> **Última medición: 2026-09-11, commit `87ce606a318`.** La anterior era del 8 de
> septiembre y llevaba tres días dando cifras que el árbol ya desmentía: qué decía,
> qué era falso y por qué, en el [§6](#6-qué-decía-la-versión-anterior-y-por-qué-estaba-mal).

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
| **shell** (`.sh`) | **0** | **0** | ✅ objetivo cumplido |
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

**Aun así, el contador no es la fuente de este documento.** Recorre el disco, así que
ve ficheros sin versionar y no ve lo que otro carril acaba de borrar. Las tablas de
arriba salen de `git`, a un commit. El contador sirve para una comprobación rápida;
el `git` es el que se cita.

### 6.3 El objetivo «C → 0» seguía descrito como pendiente en la cabecera

La tabla de la línea base decía «**C** 22 / 5.457» y el texto de objetivos vivos
«migrar los compilados en Mac (auditoría en curso)», mientras que 60 líneas más abajo
el snapshot del 8 ya decía que era 0. **Medido:** 0 desde `291e35d2bd4`, el 5 de
septiembre a las 20:01. El documento se contradecía a sí mismo dentro de la misma
página.

### 6.4 Lo que decía de `extern/` sigue siendo cierto

La única parte que ha aguantado: C++ 773 ficheros, C 24 ficheros / 58.211 líneas
(58.188 con `wc -l`), Python 18-19 ficheros. Ni se ha tocado ni se va a tocar.
