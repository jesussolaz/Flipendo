# La captura de pantalla del motor — qué la rompe y cómo se sabe

> Carril CAPTURA-NOPY, 2026-09-12. Qué se denunció, qué se midió, qué era falso, qué se
> arregló y qué queda. Informe con las cifras: `informes/CAPTURA-NOPY.md`.

## 1. Por qué existe este documento

La configuración **sin CPython** (`WITH_PYTHON=OFF`) es la que Jesús envía. Si ahí no se
pueden tomar capturas, no se puede verificar visualmente **nada**: ni el pelo, ni el
render a textura, ni la interfaz de juego. `informes/PELO-1.md` §6 dejó escrito que
«el Blenderplayer sin CPython escribe la captura completamente en blanco», con un
control sobre el cubo de fábrica. Es un bloqueo de toda la verificación visual del
producto real, así que se midió.

**Era falso.** El Player sin CPython captura perfectamente. Lo que falla es otra cosa,
le pasa **igual a los dos binarios**, y —esto es lo grave— fallaba **en silencio**:
el motor escribía un PNG válido, entero a cero, y decía por el log que había capturado.

## 2. Lo que de verdad pasa

`RAS_OpenGLRasterizer::MakeScreenshot()` pedía medio mega con `malloc()` y se lo pasaba
a `GPU_framebuffer_read_color()` sobre el buffer trasero de la ventana. Dos hechos que
se juntan mal:

1. **Un `malloc()` grande llega del sistema con las páginas a cero.** No es basura: es
   un negro perfecto, con alfa 0.
2. **La lectura puede volver sin escribir ni un byte, y sin decirlo.** En el camino de
   Metal, `MTLFrameBuffer::read()` (caso `GPU_COLOR_BIT`) hace `return` sin `else` si el
   framebuffer no tiene adjunto en la ranura, y `gpu::MTLTexture::read_internal()` hace
   `return` si la textura no está *baked*; el aviso va con `MTL_LOG_WARNING`, que sólo
   se imprime con `--debug-gpu`.

Resultado: una captura PNG de 400x300 RGBA, válida, con los cuatro canales a cero. Al
mirarla se ve **blanca** —porque el alfa es 0 y el visor pinta transparente sobre
blanco—, y de ahí el diagnóstico equivocado de «sale en blanco».

**El disparador medido**: tener **dos Blenderplayer abiertos a la vez**. Con uno solo la
captura sale siempre; con dos, en 5 de 6 intentos **uno de los dos** (no siempre el
mismo) lee el buffer vacío. No depende de que las ventanas se solapen —se reprodujo con
ventanas separadas— ni de cuál tenga el foco: robarle el foco a un Player con otra
aplicación cualquiera (Calculadora) **no** rompe la captura. Es una carrera entre los dos
procesos por el recurso de ventana de Metal.

## 3. La regla

> **Una captura que no pudo leer la GPU no es una captura: no se escribe y se dice.**

Es la regla de `politicas/ARNES-A-PRUEBA.md` («si no pudo comparar, es fallo, y lo dice
con el motivo») aplicada al sitio donde se fabrica la evidencia. Cómo se cierra:

| Pieza | Dónde |
|---|---|
| Byte centinela `0xCD` con el que se rellena el destino **antes** de leer | `RAS_Rasterizer::SCREENSHOT_UNREAD_BYTE` |
| Relleno antes de la lectura | `RAS_OpenGLRasterizer::MakeScreenshot()` |
| Guarda «no se leyó nada» → no se escribe fichero | `RAS_ICanvas::SaveScreeshot()`, devuelve `false` |
| Reintento acotado (8 fotogramas), con aviso en cada uno, y error final con `ARNES:` | `RAS_ICanvas::FlushScreenshots()` |
| Capturas que se quedan en la cola al cerrar el juego → `ARNES:` | `~RAS_ICanvas()` |
| Gancho para probar la guarda al revés | `FL_SHOT_FORCE_EMPTY=1` |

La última fila cierra la trampa que `politicas/UI-JUEGO-NATIVA.md` §11 tenía escrita desde
hace dos noches: «pedirla y salir en el mismo tic la deja sin escribir **mientras el log
dice que se ha capturado**». Estaba documentada, pero el código seguía callándose porque
`~RAS_ICanvas()` vaciaba la cola sin mirarla.

Por qué un centinela y no «comprobar si está todo a cero»: una pantalla **negra de
verdad** también es todo ceros en RGB, y hay escenas que la tienen. Lo que distingue
«negro» de «no leído» es que la lectura de verdad escribe alfa 255. El centinela lo hace
explícito y no depende de esa suposición.

Por qué el umbral del aviso parcial es «más de la mitad del buffer» y no «más de cero»:
una captura de verdad trae por azar uno de cada 256 bytes con el valor `0xCD` —unos
1.900 bytes de 480.000 en 400x300—, así que `unread > 0` avisaría en **todas**.

## 4. Cómo se verifica visualmente el Player sin intérprete

Funciona, y da lo mismo que el Player con intérprete. El procedimiento:

```
FL_UI_SHOT=<salida.png> FL_UI_SHOT_FRAME=<frame> FL_UI_EXIT=1 \
  <Blenderplayer> -w <ancho> <alto> [<izq> <arriba>] <escena.blend>
```

Y luego `Blender --background --factory-startup --fl-compare-png <a> <b>`.

Las cuatro condiciones que hacen que el número signifique algo:

1. **Un Player a la vez.** Es el disparador del fallo. Nada de lanzar la captura de las
   dos configuraciones en paralelo para ir más rápido: es justo lo que la rompe.
2. **Mide el suelo de ruido primero**, con el mismo binario y la misma escena. En una
   escena determinista (el cubo de fábrica, sin física) el suelo es **cero**: las
   capturas salen byte a byte iguales, y entonces se puede comparar por `md5`. En una
   escena con física el suelo sube mucho (el carril PELO midió 14,43 % en la suya) y hay
   que darlo antes de afirmar cualquier diferencia.
3. **Espera antes de salir.** La captura se encola y el motor la vuelca en `EndFrame`;
   con el reintento, hasta 8 fotogramas más. `FL_UI_EXIT` espera 15, que sigue bastando.
4. **Comprueba que el fichero existe.** Si la lectura falló, ahora **no hay fichero** y
   el log trae una línea `ARNES:`. `--fl-compare-png` sale con 1 si falta un fichero, así
   que el rojo llega solo.

## 5. Trampas pagadas

1. **«Sale en blanco» era «sale a cero con alfa 0».** Un PNG RGBA todo a cero se ve
   blanco en cualquier visor. Antes de diagnosticar el color de una captura, mira los
   bytes: `sips -s format tiff` y un histograma, o el propio `--fl-compare-png` contra
   una captura buena (delta medio 71,78 y máximo 177 contra el cubo es exactamente «la
   otra imagen es negra», no «es blanca»).
2. **Un `malloc()` grande no da basura, da ceros.** Es la razón de que el fallo pareciera
   un render negro legítimo. Con `calloc()` habría sido igual de indistinguible.
3. **Comparar dos configuraciones lanzándolas a la vez se sabotea solo.** Es la trampa
   que costó el diagnóstico falso de `PELO-1.md` §6.
4. **`m_frame` y `m_mousestate` de `RAS_ICanvas` no se inicializaban.** `m_frame` lo lee
   `BLI_path_frame()` para resolver los `#` de la ruta de la captura: con una ruta con
   `#` el número era basura. Arreglado de paso.
5. **El aviso de Metal existe pero no se ve.** `MTL_LOG_WARNING` está detrás de
   `G.debug & G_DEBUG_GPU`. Si alguna vez sospechas de una lectura de GPU, arranca con
   `--debug-gpu` antes de leer código.

## 6. Lo que queda

- **No se arregla la causa de fondo**, que está en el camino de Metal de Blender
  (`MTLFrameBuffer::read()` vuelve en silencio cuando no hay adjunto). Lo que se arregla
  es que **el motor no pueda mentir**: o hay captura, o hay rojo. Que dos Players a la
  vez puedan capturar los dos sigue sin estar resuelto, y la regla operativa es no
  hacerlo.
- **No se sabe si el reintento de 8 fotogramas salva el caso de los dos Players**, y no se
  va a dar por bueno sin medirlo: repetido el experimento con los binarios arreglados, el
  fallo no se reprodujo ninguna de las 6 veces, pero esos binarios pintaban un fotograma
  negro (trabajo de GPU casi nulo) por un estorbo de otro carril, mientras que las 6
  repeticiones que sí fallaron 5 veces pintaban un fotograma real de EEVEE. Hay que
  repetirlo cuando el árbol vuelva a renderizar. Detalle en `informes/CAPTURA-NOPY.md` §4.
- **`--fl-compare-png` debería rechazar también una imagen uniforme** («no pudo
  comparar»). Es fichero del carril PELO (`source/blender/editors/flipendo/fl_image_compare.cc`)
  y no se toca desde aquí: queda dicho para su dueño.
- **El motor empotrado del editor (pulsar P)** no se ha medido: sigue haciendo falta la
  bandera `--fl-game-start` que ya pedía `PELO-1.md`.
- **Sólo hay un `MakeScreenshot`** (el de `RAS_OpenGLRasterizer`, que sirve a los tres
  backends a través de `GPU_framebuffer_read_color`). Si algún día hay uno propio de
  Vulkan o Metal, el centinela tiene que ir también ahí.
