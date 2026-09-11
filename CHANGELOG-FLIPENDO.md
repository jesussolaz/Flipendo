# UPBGE Key — registro de versiones

Línea propia de UPBGE para **MacBookPro16,1** (i7-9750H · Radeon Pro 5300M · macOS 26.3.1, x86_64).

## Por qué existe esta línea

Blender retiró el soporte de **macOS Intel en la versión 5.0**: la 4.5 LTS es la última que
publica `macos-x64`, y de 5.0 en adelante solo hay `macos-arm64`. Como UPBGE 0.50 está basado
en Blender 5.0.1, **no existe ni existirá un build oficial de UPBGE posterior a 0.44 para Intel**.

UPBGE 0.44 (base Blender 4.4.3) es por tanto el techo oficial en este equipo, y el punto de
partida de nuestra propia línea.

---

## key-1.0 — «UPBGE Key 1.0» · 2026-09-04

Primera versión de la línea. Base: `0.44-vanilla`, sin recompilar.

**Cambios**
- Identidad propia: `CFBundleName` = `UPBGE Key`, bundle id `org.upbge.key` (player: `org.upbge.key.player`).
- Versionado propio: `CFBundleShortVersionString` = `1.0`, independiente del `4.4.3` de base.
- `NSSupportsAutomaticGraphicsSwitching = false` — fuerza explícitamente la **Radeon Pro 5300M**
  discreta. Antes funcionaba por *ausencia* de la clave; ahora es intencionado y no se puede
  perder por accidente en un build futuro.
- **Firma ad-hoc** (`codesign -s -`). El build oficial venía sin firmar. Con identidad estable,
  macOS no trata cada build modificado como una app nueva y no vuelve a pedir permisos de
  disco/cámara/micro en cada iteración.
- Cuarentena de Gatekeeper retirada.

**Verificado**
- Arranca en GUI y en `--background`.
- Backend **Metal** (es el único que soporta este build: `--gpu-backend` sólo acepta `metal`;
  OpenGL ya no está compilado).
- Escritura/lectura de `.blend` correcta.

**Sin cambios**
- Binarios y librerías idénticos al 0.44 oficial. Esto **no** es una recompilación.

---

## 0.44-vanilla — referencia prístina · protegida

UPBGE 0.44 oficial tal cual salió del DMG. Punto de retorno; no se modifica nunca.

- Origen: `upbge-0.44-macos-x86_64.dmg` (release oficial v0.44)
- SHA-512 verificado: `b943b29a…7b31a2da`
- Build upstream: 2025-05-05, rama `upbge-v0.44-release`, hash `22781af88461`

---

## Uso

```
upbge-key list                     ver versiones
upbge-key snapshot <n> -m "..."    congelar el estado actual como versión nueva
upbge-key activate <n>             instalar esa versión en /Applications
upbge-key diff <a> <b>             qué ficheros cambian entre dos versiones
upbge-key run                      abrir UPBGE Key
```

Los snapshots son **clones APFS**: cada uno cuesta ~0 bytes hasta que los ficheros difieren.

---

## Fase 0/4 del plan maestro · 2026-09-05

**Fase 0 (preparación) — casi completa**
- Material de sesión rescatado del scratchpad volátil a `dev/refs/` (112 MB: benchmarks, scripts de verificación, notas) y `addons/keyfx/`.
- Toolchain sin Homebrew en `dev/toolchain/`: CMake 3.31.6, ninja 1.12.1, git-lfs 3.7.0 (LFS inicializado). En PATH vía ~/.zshrc.
- Clones en curso: fuente UPBGE (historia completa, base fork `3c7b891a`) y libs `lib-macos_x64` rama `blender-v4.5-release`.
- PENDIENTE: remotes de GitHub (no hay `gh` instalado — requiere login del usuario).

**Fase 4 (Key Assistant) — MVP funcional**
- Addon `key_assistant` v0.1.0 (386 líneas) instalado y habilitado de forma persistente en UPBGE 4.4.
- Panel en Vista 3D > N > "Key Assistant": chat, adjuntar imagen/URL, captura de viewport ("ojos"), confirmación antes de ejecutar (por defecto), autosave + undo por cada ejecución, lista negra de llamadas peligrosas, autocorrección de errores (hasta 3 intentos).
- Backend: binario `claude` de la extensión VS Code (auth existente del usuario), `-p --output-format json` + `--resume` para continuidad de sesión.
- `keyfx` instalado como módulo (`scripts/modules/keyfx.py`).
- **Verificado ciclo completo headless**: petición "crea una esfera TEST_KA…" → Claude → bloque bpy-exec → ejecución → esfera con material toon en escena. CICLO_COMPLETO_OK.

## Fase 5.2 — Plantilla "Key ARPG" funcional · 2026-09-05

- `game/template/components/arpg_core.py`: lógica pura testeada fuera del motor (combos con ventanas de encadenado y buffer de input, lock-on por cono, salud con i-frames, cerebro enemigo). 5/5 tests unitarios.
- `game/template/components/arpg.py` (297 líneas): componentes UPBGE — PlayerController (WASD relativo a cámara, dash, salto, combo x3, aplicar golpes por alcance+ángulo), ThirdPersonCamera (órbita con ratón, colisión por raycast, encuadre lock-on, autosanado de cámara activa), EnemyAI (percibir→perseguir→atacar, knockback, muerte).
- `game/template/KeyARPG.blend` generado por script (GUI + autocierre): arena, jugador CHARACTER, 3 enemigos, componentes registrados, 60fps.
- **7/7 tests de runtime en blenderplayer**: componentes viven, enemigo muere al golpe, jugador recibe daño, cámara activa y siguiendo, enemigo persigue.
- Bug conocido de UPBGE esquivado: registro de componentes/sensores crashea en `--background`; el generador corre en GUI con salida por timer.

## key-2.0 — 2026-09-05
Motor propio compilado desde fuente en este Mac (rama `key-fase3-filtros`, base Blender 4.5).
- Filtros 2D funcionando en Metal: presets y custom con sintaxis nueva (`fragColor`, `texture()`, `bgl_TexCoord`). Verificado con capturas.
- `bge.texture`/VideoTexture restaurado: ImageRender a textura de material, probado con escena CCTV (pantalla que muestra otra cámara).
- InitTextures por árbol de nodos: adiós al "Texture is not available".
- Pendiente: traductor de sintaxis GLSL vieja; segfault al cerrar player con Texture activa; feedback cámara-objeto descartado por Metal (limitación documentada).

## key-2.1 — 2026-09-05
- Filtros 2D custom aceptan la sintaxis GLSL antigua (`gl_FragColor`, `texture2D()`, `gl_TexCoord[0]` con `.st`/`.xy`): los tutoriales pre-0.50 funcionan tal cual.
- Arreglado el segfault al cerrar el player con `ImageRender` activo (use-after-free en el GC final de Python). 3/3 cierres limpios.

## flipendo-2.1 — 2026-09-05 (Rebautizo integral)
Capa de identidad renombrada de "UPBGE Key" a **Flipendo**:
- Apps: `Flipendo.app` y `Flipendo Player.app` (bundle `org.flipendo.flipendo` / `org.flipendo.player`).
- Carpeta de trabajo: `~/Flipendo` (antes `~/UPBGE-Key`); todas las rutas absolutas corregidas (symlink de libs, build cache, .zshrc, .active).
- Gestor de versiones: comando `flipendo` (antes `upbge-key`).
- Motor interno (ejecutable, `--version`, config) sigue siendo Blender/UPBGE: es funcional y lo exige la GPL. Pendiente opcional: título de ventana y splash propios (requiere recompilar).

---

## Del 6 al 10 de septiembre · el hueco que faltaba en este registro

Este registro saltaba del 5 de septiembre a la noche del 10. En esos cinco días (32
commits) pasaron cuatro cosas que conviene tener fechadas, porque la noche del 10 se
apoya en ellas:

- **`KX_PythonComponent` eliminado** (08-09, 21:09). `FL_Component` —componentes
  nativos en C++— queda como el **único** sistema de componentes del motor.
- **El Player compila, arranca y juega sin CPython** (08-09, 22:01): 0 símbolos de
  Python, 536 MB frente a 771 MB. La salida no fue inventar un target nuevo, sino
  **dos configuraciones de build del mismo árbol** (`dev/build` con `WITH_PYTHON=ON`
  para trabajar, `dev/build-nopy` con `OFF` para empaquetar).
- **`svn_rev_map`**, el mayor bloque de Python del árbol, convertido en asset binario
  con lector C++: **−108.812 líneas**.
- **El keymap del arranque ya no lo genera Python** (09-09, 00:52): 3.673 atajos
  idénticos, y después **248 keymaps con cero diferencias**.

Efecto medido sobre el árbol (Python propio, sin `extern/` ni `lib/`): **357.084 → 247.642 líneas**.

---

## Flipendo sin Python en el paquete · noche del 10 al 11 de septiembre de 2026

> **No es una versión congelada.** No se ha hecho `flipendo snapshot`: no hay una
> carpeta en `~/Flipendo/versions/` con este estado. Es un hito del árbol de código,
> y se registra aquí porque cambia lo que Flipendo *es*.

Once trabajadores en paralelo (Claude en varias pestañas y agentes, Codex, GitHub
Copilot) dirigidos por una sesión de Claude que hizo de jefe de proyecto, de las
20:51 del día 10 a las 06:25 del 11. **154 commits** en ese tramo; 192 contando la
mañana del 11.

Una sola regla, y es la que explica por qué esto se puede creer: **nada se retira de
Python hasta que su sustituto en C++ demuestra, con un volcado idéntico, que hace
exactamente lo mismo.**

### Cambios

- **El paquete de distribución no contiene ni un `.py`.** El Player que se envía
  —`dev/build-nopy`, `WITH_PYTHON=OFF`— no lleva intérprete: **0 símbolos `^_Py`**
  (antes 2.267), `libpython` no enlazada, y el bundle pasa de **772 a 537 MB**. El
  juego se **crea** con el editor (`WITH_PYTHON=ON`) y se **envía** con esta segunda
  configuración; el `.blend` es el mismo a los dos lados.
- **Sistema de herramientas, en C++** (5.606 líneas): catálogo, activación y sus
  atajos. Se van `space_toolsystem_toolbar.py` (3.779), `space_toolsystem_common.py`
  (1.086), `bpy/utils/toolsystem` y `keymap_from_toolbar.py` (394).
- **Los operadores del Window Manager, en C++.** `bl_operators/wm.py` (3.015 líneas)
  deja de existir: 24 operadores migrados con el mismo `idname` y las mismas
  propiedades.
- **El keymap deja de ser código.** `keymap_data/blender_default.py` (8.669) y
  `keyconfig/Blender.py` (386) se convierten en un dato (`Blender.fpreset`) que lee
  C++.
- **Los presets, también datos.** `scripts/presets/` pasa de **173 ficheros `.py` /
  10.287 líneas** a **0 `.py` y 172 `.fpreset`**, con lector, escritor y conversor en
  C++.
- **52 operadores de objeto, malla, cuerpo rígido y selección** reescritos en C++.
- **El editor de nodos entero, en C++**: `space_node.py` cae de 1.201 a **84** líneas
  y de 34 clases registradas a 7. Las 7 que quedan son paneles que este editor
  *clona* de Propiedades, y no se reimplementan a propósito: esperan a que su
  original esté en C++ y exponga su `draw()` en una cabecera compartida.
- **Los paneles de juego de Flipendo, nativos**: `properties_game.py` (899) fuera; el
  editor de lógica es C++ (`source/blender/editors/space_logic/`).
- **Vuelven en C++ dos capacidades del motor**: el **render a textura**
  (VideoTexture) y una **interfaz de juego** nativa (4 de 9 widgets), comparadas
  píxel a píxel contra el camino antiguo.
- **ÁNIMA se publica desde el Flipendo nuevo**: `.zip` de 269 MB que arranca.
- **La instalación dejó de mentir.** Se descubrió que el `install` copiaba pero no
  borraba: **406 `.py` huérfanos** seguían cargándose desde el paquete, de modo que
  una migración podía darse por buena con el Python viejo todavía vivo. Ahora el
  árbol de `scripts/` instalado se poda.

### Verificado, y con qué cifras

Nada de esto se aceptó por lectura del código. El patrón del proyecto es que el
binario lleve una opción `--fl-dump-*` escrita en C++ que vuelca el estado
observable, se congela la línea base con el Python aún vivo, y el C++ tiene que
reproducirla. **Antes de la noche había 8 de esas opciones; hoy hay 50.**

```
--fl-check-keymap           248 keymaps, 0 por transliterar
--fl-check-tools            30 secciones, 0 diferencias
--fl-check-optypes          44/44 identicos
--fl-check-ui               2113/2113 identicos, 0 faltan, 0 sobran
--fl-check-manual           7470 identicas, 0 distintas
--fl-check-external-editor  4326/4326 identicos
--fl-check-object-select    152/152 identicos
--fl-check-mesh-ops         13112/13112 identicos
--fl-check-rigidbody-ops    63/63 identicos
```

Y una revisión final buscando **capacidad perdida en silencio**, que es el riesgo real
de una noche así: se extrajeron todos los `bl_idname` y clases registradas de los
**202 `.py` de `scripts/` borrados** y se buscaron en el binario vivo. **52 de 52
operadores y 24 de 24 clases de interfaz, presentes.** Los únicos identificadores sin
rastro eran tres de una plantilla de ejemplo.

Python propio del árbol: **248.065 → 195.611 líneas** en la noche (−52.454; −358
ficheros), y 191.224 al cerrar la mañana del 11.

### Lo que NO se hizo, dicho claro

- **Flipendo no es «full C++» todavía.** Al ritmo medido, faltan entre **40 y 60
  noches** como esta. Quedan 191.224 líneas de Python, 248.745 de cabeceras `.h`
  heredadas, 68.391 de GLSL escrito a mano y 30.304 de Objective-C++.
- **Dos trabajadores agotaron su cuota** a mitad de faena: Copilot a la 01:20 y Codex
  a las 04:05. No se compraron créditos.
- **El fork no tiene trazado de rayos**: `WITH_CYCLES=OFF`. No es de esta noche, pero
  se descubrió midiendo, y el inventario de Python había dicho expresamente que
  retirar Cycles era una pérdida de capacidad que no debía hacerse sin sustituto.
  **Debería ser una decisión, no un `OFF` heredado que nadie ha mirado.**
- **La suite de tests C++ no corre entera**: entre 448 y 703 casos según el momento;
  el corredor muere con un doble *free* y 255 casos no llegan a ejecutarse.
- **44 de los 52 operadores** migrados tienen verificación propiedad a propiedad; los
  otros ocho se verificaron por comportamiento. La doctrina pide lo primero para
  todos.
- **El menú *Plantillas* del editor de texto queda vacío**: se retiraron los 41
  ficheros de `scripts/templates_py`. Es coherente (sin intérprete no hay script que
  plantillar), pero el usuario lo notará.
- **El fork ya no importa ni exporta glTF**, y no exporta FBX: esos addons Python se
  retiraron. El importador FBX sigue, en C++.

### Tres lecciones que se pagaron caras y quedan en vigor

1. **Hay resultados que no son deterministas.** El cálculo de normales de vértice da
   valores distintos entre ejecuciones del mismo binario con la misma escena, porque
   la acumulación va en paralelo y en coma flotante. **Si un volcado no se reproduce a
   sí mismo tres veces seguidas, no es una línea base**: dos arneses dieron una
   diferencia en la primera pasada y ninguna en las siguientes.
2. **Se mide sobre un paquete recién instalado, nunca sobre uno acumulado.** Es la
   otra cara de los 406 `.py` huérfanos.
3. **Regenerar una línea base porque sale roja** es exactamente como un verificador se
   convierte en un sello de goma. Cada diferencia se explica antes de absorberse.

El detalle de cada pieza está en `politicas/` (34 documentos) y el resumen de la noche
en `politicas/NOCHE-2026-09-11.md`. Las cifras del árbol, medidas y con su método, en
`politicas/METRICAS.md`.

---

## Decisión del 11 de septiembre por la mañana · «full C++» significa todo

Con el coste sobre la mesa, Jesús decidió que **full C++ incluye también el
Objective-C++ y los cuerpos de shader**. Hasta entonces la política del proyecto los
daba por aceptables: el ObjC++ como «plataforma legítima» y el GLSL como «inviable de
golpe».

- **Objetivo:** cero Python, cero C propio (ya cumplido), cero Objective-C++, cero
  shell propio (ya cumplido), cero GLSL escrito a mano, y las cabeceras `.h`
  heredadas a `.hpp` según se reescriba su subsistema.
- **Única excepción, y es doctrinal:** `extern/` y `lib/` son terceros vendorizados.
  Se mantienen verbatim y se actualizan desde upstream. Quedan marcados
  `linguist-vendored` para que ni GitHub se los atribuya a Flipendo.
- **Por qué el Objective-C++ sí se podía**, contra lo que decía la política anterior:
  el runtime de Objective-C es una biblioteca de C, y cualquier C++ puede llamar a
  Cocoa a través de él. La prueba es que `metal-cpp`, el binding oficial de Apple, es
  exactamente eso. Está vendorizado en `extern/metal-cpp` desde esa mañana.
- **Y el tamaño real del trabajo no son las líneas**: los 30.304 de `.mm` contienen
  **441 envíos de mensaje**. `mtl_shader_log.mm` tenía cero —era C++ disfrazado de
  `.mm`— y el fichero más grande del backend, con 3.413 líneas, tiene 16. Ordenar
  este trabajo por líneas habría sido ordenarlo al revés de como conviene.

Esta decisión **deroga** dos secciones de `politicas/MIGRACION-CPP.md` (§3.3 y §4),
que quedan marcadas en su sitio con la fecha y el motivo en lugar de borrarse.

---

## El día 11 de septiembre · el Objective-C++ compilado baja a cero

La decisión de la mañana se ejecutó el mismo día. **Flipendo ya no compila una sola
línea de Objective-C++.**

| | Antes | Después |
|---|---:|---:|
| ObjC++ que el binario compila | 25.476 | **0** |
| ObjC++ en el árbol | 30.536 | 5.060 — todos de Cycles, que está apagado |
| `source/blender/gpu/metal` | 20 `.mm` | **21 `.cc`, ningún `.mm`** |
| `intern/ghost/intern` | 4.340 líneas | **0** |

La ventana, el teclado, el ratón, el portapapeles, los menús y el backend gráfico
entero hablan con macOS desde C++ puro.

**Cómo se hizo sin perder la red.** El backend de Metal fue con `metal-cpp`, el binding
oficial de Apple, vendorizado en `extern/`. Para AppKit, que no tiene binding, se usa
el runtime de Objective-C —que es una biblioteca de C— y las clases que macOS necesita
llamar (delegados de ventana y de aplicación) **se fabrican en tiempo de ejecución**.

La parte peligrosa eran las codificaciones de tipo: una errata en un selector no da
error de compilación, revienta en ejecución. La solución fue **no escribir ninguna**:
de las 68 que hacían falta, 65 se le piden al propio runtime y el constructor de clases
**aborta si nadie conoce el selector**. Eso convierte el fallo silencioso más probable
en un fallo ruidoso antes de dibujar un píxel.

**Tres hallazgos técnicos que abarataron el trabajo:**

- Un puntero a clase de Objective-C y una clase C++ declarada-no-definida con el mismo
  nombre **manglan igual** (`P6NSView` en los dos, comprobado con `nm`). La frontera de
  enlazado desaparece sin castear y sin perder tipado. No vale con protocolos.
- En cambio, **el mangling sí manda en las funciones**: mismo puntero no es misma firma,
  así que una función declarada en cabecera común, definida en un `.mm` y llamada desde
  un `.cc` genera dos símbolos, compila en los dos lados y no enlaza. La migración sigue
  el grafo de llamadas, no la densidad de Objective-C.
- La mejor salida no es castear sino **quitar el tipo de la firma**: `set_label(NSString*)`
  pasó a `set_label(const char*)` y la frontera desapareció para siempre.

**Tres fallos reales que cazó la verificación y que ningún compilador habría dado:** un
selector partido en dos líneas dentro de una macro sale con un espacio dentro y mata la
aplicación al cambiar el cursor; una clase que nunca llegaba a registrarse habría dejado
la ventana sin delegado; y `NSBitmapFormatFloatingPointSamples` no es `1<<3` sino `1<<2`.

**Verificado:** 307 selectores comprobados contra las 26.293 clases cargadas, 74
constantes con `static_assert` contra el SDK, render EEVEE 1920×1080 con **0 píxeles
distintos de 2.073.600**, Player atando 5/5 componentes, y la ventana probada de verdad
—abrir, mover, redimensionar, minimizar, pantalla completa y cerrar con Cmd+Q—.

### Y el arnés, que era un sello de goma

El mismo día se midió la batería de verificación completa por primera vez, y el
resultado obligó a parar: **58 de las 59 opciones `--fl-*` salían con código 0 sin haber
comprobado nada**. El patrón era idéntico en todas — al faltar un argumento devolvían
`return 0`, que para el analizador significa «he consumido cero argumentos», y el
programa terminaba en verde. Peor aún, **seis comprobadores salían verdes habiendo
comparado cero casos**, porque contaban diferencias y cero elementos comparados dan cero
diferencias.

Y una ceguera real: `--fl-check-keymap` reconstruía una línea cortando por `flag=`, así
que cualquier campo nuevo al final de esa cabecera **no se habría comparado jamás**.

Todo corregido, con cinco guardas en `source/creator/creator_fl_harness.hh` y la regla
**si no pudo comprobar, es fallo, y dice por qué**. Demostrado ejecutándolos en las
condiciones malas: línea base inexistente 14/14 en rojo, vacía 14/14 en rojo, alterada a
mano 14/14 en rojo nombrando qué difiere. Las reglas quedan en
`politicas/ARNES-A-PRUEBA.md`.

**Dos soluciones que merecen constar.** La diferencia deliberada de
`wm.context_cycle_array` —el C++ corrige un fallo del Python al rotar tuplas— no se tapó
tocando la línea base, que es la prueba: se declara aparte y **se exige en las dos
direcciones**, de modo que volver al valor del Python sale como REGRESIÓN. Y la
inestabilidad de `--fl-check-mesh-ops` se resolvió con una **tolerancia declarada** de
2e-5 (4,3× la desviación medida) en vez de cuantizando, porque a 5 decimales los valores
siguen difiriendo y a 4 se tirarían cifras válidas de los 13.111 estables.

### El estado del proyecto deja de escribirse a mano

`politicas/ESTADO.md` lo **genera** `tools/flipendo_metrics` (C++) midiendo el árbol a un
commit nombrado, nunca al disco. Existe porque `METRICAS.md` publicó el 8 de septiembre
una cifra medida el 5 —entre medias habían caído 109.442 líneas sin registrarse— y
porque el contador del proyecto sumaba una línea de más por fichero, lo que inflaba todas
las cifras históricas. Un documento a mano envejece en horas cuando hay siete carriles
trabajando.

### Lo demás del día

`presets.py` de 1.022 a 443 líneas · `rna_xml` nativo, con el mismo md5 que el volcado de
Python en 6.839 casos · los tres operadores de nivel de detalle a C++, y con ellos 70
líneas que estaban declaradas dos veces y no ejecutaba nadie · las pestañas de Volumen y
Sonda de luz del editor de Propiedades · y las instrucciones de compilar del README, que
**no funcionaban**: mandaban clonar las librerías en `lib/macos_x64`, que git trata como
submódulo y convierte en directorio vacío en cuanto toca el índice.
