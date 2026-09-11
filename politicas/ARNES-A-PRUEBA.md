# El arnés puesto a prueba — que ninguna comprobación pueda pasar sin comprobar

> Carril ARNÉS, 2026-09-11. Qué había, qué se midió, qué se arregló, qué queda.

## 1. Por qué existe este documento

Todo el proyecto se sostiene sobre una regla: **nada se da por migrado hasta que un
comprobador demuestra que el C++ hace lo mismo que hacía el Python**. Esa regla vale
exactamente lo que valga el comprobador.

Dos noches seguidas se descubrió que no valía nada:

- El 10, el verificador de interfaz era **ciego a las listas**: comparaba y no miraba.
- El 11, `politicas/METRICAS.md` §7.2 midió que **los dieciséis `--fl-selftest-*` sin
  argumento imprimían el error y salían con código 0**. Dieciséis pruebas que no podían
  fallar. Y dos de ellas (`--fl-selftest-context-ops`, `--fl-selftest-wm-property-ops`)
  **no escribían nada en `--background`**: el fichero quedaba vacío y el verde era doble
  mentira.

Un arnés que no falla nunca no es un arnés, es un sello de goma, y envenena todo lo que se
haya dado por verificado con él. Este documento fija las reglas para que no vuelva a pasar,
y deja escrito cómo se demostró cada una.

## 2. La regla

> **Si no pudo comparar, es fallo, y lo dice con el motivo.**

Un comprobador sale con 0 **solamente** si comparó algo y lo comparado coincidió. Las cinco
formas de «no pudo comparar», y dónde se cierra cada una:

| Forma | Guarda | Dónde vive |
|---|---|---|
| Falta el argumento obligatorio | `arg_missing()` | `source/creator/creator_fl_harness.hh` |
| La línea base no existe, no se lee o está vacía | `baseline_required()` | ídem |
| El directorio de entrada no existe | `dir_required()` | ídem |
| El volcado no se escribió o quedó vacío | `dump_written()` | ídem |
| Hace falta un editor real y se pidió en `--background` | `gui_required()` | ídem |
| Se compararon **cero** elementos | guarda al final de cada comparador | `FL_selftest_compare.hh` y los seis comprobadores propios |

Todas salen con `WM_exit(C, EXIT_FAILURE)` —nunca `exit()`, para que se limpien los
temporales— y todas imprimen una línea que empieza por `ARNES:` con el motivo, para poder
leer un rojo sin abrir el código.

## 3. Cómo se demuestra que un comprobador falla cuando debe

Un comprobador nuevo no está terminado hasta que se ha ejecutado **en las condiciones
malas** y se ha enseñado que sale distinto de 0. Las cuatro que hay que pasar:

1. **Sin argumento.** `Blender --background --factory-startup --fl-check-X`
2. **Con una línea base que no existe.** `... --fl-check-X /no/existe.txt`
3. **Con una línea base vacía (0 bytes).**
4. **Con la línea base alterada a mano**, cambiando una línea de datos.

Y, para que no sea un rojo permanente disfrazado, una quinta: **contra la línea base de
verdad tiene que salir verde**, con sus cifras.

Medido el 2026-09-11 sobre las 38 banderas `--fl-check-*` / `--fl-selftest-*` del binario:

| Condición | Antes | Ahora |
|---|---|---|
| Sin argumento (38 banderas) | 38 salían con **0** | 37 salen con **1**; la 38ª, `--fl-check-keymap-menus`, sale con 0 **porque su fichero es opcional y sí comprueba** (136 nombres, 0 ausentes) |
| Línea base inexistente (14 comprobadores) | salían con 0 | 14/14 con **1** y el motivo |
| Línea base vacía (14) | salían con 0 | 14/14 con **1** y el motivo |
| Línea base alterada (14) | — | 14/14 con **1**, nombrando qué difiere |
| Línea base de verdad (14) | — | 13 verdes; 1 rojo real (`--fl-check-ui`, de otro carril) |
| Los dos que no escriben en `--background` | salían con 0 y fichero vacío | salen con **1** diciendo que necesitan modo gráfico |

## 4. Resultados que NO son deterministas: tolerancia declarada, no redondeo

El REGLAMENTO ya midió (lección de las 03:50) que el cálculo de normales de vértice no es
determinista. Se manifiesta en `--fl-check-mesh-ops`: 1 línea de 13.146, siempre la misma
(10866, `paint.vertex_color_dirt uvsphere floatcolor point`), que salta entre dos valores
separados 4,679e-6.

La decisión del proyecto es **declarar una tolerancia**, no redondear el volcado:

- Redondear a 5 decimales **no arregla nada**: los dos valores siguen siendo distintos.
- Redondear a 4 sí los junta, pero tira cinco cifras significativas de los 13.111 valores
  que **sí** se reproducen bit a bit. Un comprobador que deja de mirar la quinta cifra deja
  de poder cazar una regresión en la quinta cifra.
- Y redondear es una lotería: solo estabiliza si el valor inestable no cae cerca del límite
  de redondeo. Una tolerancia no depende de eso.

Cómo se declara: un fichero **al lado de la línea base**, `<linea-base>.tolerancia`, con el
motivo escrito y una línea `tolerancia-relativa <valor>`. Sin ese fichero la comparación
sigue siendo exacta byte a byte. El comparador **dice en voz alta** cada línea que tolera y
con qué desviación, y el total dice cuántas fueron: si mañana son diez en vez de una, se ve.

La línea base **no se retoca**: es la prueba de lo que hacía el Python.

## 5. Diferencias deliberadas: se declaran, no se esconden ni se dejan en rojo

Cuando el C++ corrige a propósito un fallo del Python, la comparación contra la línea base
queda roja **para siempre**. Y un rojo permanente acaba en que nadie mira los rojos.

Tampoco vale reescribir la línea base sin más: se borraría la evidencia de lo que hacía el
Python.

Regla: se declara en `<linea-base>.divergencias`, con la línea, el valor del Python, el
valor **exigido** al C++ y el motivo. El comprobador entonces es **más** estricto que antes:

- el C++ da el valor declarado → verde, y lo imprime con su motivo;
- el C++ vuelve al valor del Python → **ROJO**, «REGRESION»;
- el C++ da otra cosa → **ROJO**;
- la divergencia ya no ocurre → **ROJO**, porque la declaración sobra y hay que quitarla.

Caso de referencia: `wm.context_cycle_array`. El Python hacía `array[:]` + `append`/`pop`,
pero el corte devuelve una tupla, así que levantaba `AttributeError` y **no rotaba nada**
aunque devolviera `OPERATOR_FINISHED`. El C++ rota de verdad: `[1,2,3] → [2,3,1]`. Lo
comprueba `--fl-check-context-ops`, en modo gráfico.

## 6. Trampas encontradas, para que no cuesten dos veces

- **`git commit -- <ruta>` no vale para un fichero nuevo**: git responde `did not match any
  file(s) known to git`. Hace falta registrarlo antes con `git add -N <ruta>` (intención de
  añadir, sin contenido) y commitear acto seguido con `--`. Es lo mínimo que toca el índice
  compartido.
- **Un argumento que parece línea base y es de salida.** `--fl-check-keymap-menus <lista>` y
  `--fl-check-keyconfig-io <informe>` **escriben** ese fichero. Exigirles que exista es
  romperlos. Antes de poner una guarda hay que leer qué hace el comprobador con su argumento.
- **`parse_dump()` del keymap cortaba la línea en `flag=`** al normalizar `KEYMAP_TOOL`, y se
  tragaba en silencio todo lo que viniera detrás, en la línea base y en el volcado a la vez.
  Hoy `flag=` es el último campo y no se perdía nada real, pero un campo nuevo al final de esa
  cabecera no se habría comparado nunca.
- **Probar con un sufijo al final de una línea no siempre prueba nada**: si la normalización
  corta ahí, la prueba sale verde y parece que el comprobador funciona. Hay que alterar un
  campo del medio.

## 7. Lo que queda

- El veredicto de los `--fl-selftest-*` lo saca el generador de `ESTADO.md` comparando byte a
  byte por su cuenta; **no lee `<linea-base>.tolerancia` ni `.divergencias`**. Mientras no lo
  haga, `--fl-selftest-mesh-ops` seguirá saliendo INESTABLE ahí aunque
  `--fl-check-mesh-ops` esté verde, y `--fl-selftest-context-ops` seguirá sin casar con la
  línea base del Python. Para eso está congelado
  `tests/flipendo/operators/execution-cpp-verificado.txt`. Es trabajo del carril del generador.
- Las guardas de «cero secciones» (`--fl-check-tools`) y «cero bloques» (`--fl-check-ui`) no
  se pueden disparar desde la línea de órdenes: harían falta cero secciones o bloques
  **nativos**. Van como red de seguridad, no como caso probado, y se dice.
- `--fl-check-ui (registro)` y `--fl-check-presets` siguen en rojo por trabajo de otros
  carriles, no por el arnés. El detalle, en `informes/ARNES-1.md`.
