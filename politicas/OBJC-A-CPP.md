# De Objective-C++ a C++: el backend de Metal

> Encargo: `politicas/LENGUAJE-CPP.md`, seccion «Decision del 2026-09-11». Full C++
> incluye el Objective-C++. Este documento cubre la zona `source/blender/gpu/metal`
> y deja medido el resto para las fases siguientes.

## 1. Inventario medido (2026-09-11)

Medido con `find source intern -name '*.mm' | xargs wc -l`, sin `extern/`:
**34 ficheros, 30.536 lineas**. Por zonas:

| Zona | Ficheros | Lineas | ¿Compila hoy? | Fase |
|---|---:|---:|---|---|
| `source/blender/gpu/metal` | 20 | **20.950** | Si (`WITH_METAL_BACKEND=ON`) | **esta** |
| `intern/cycles/device/metal` + `bvh/metal.mm` | 8 | 5.060 | **NO** | aplazada |
| `intern/ghost/intern` (Cocoa) | 5 | 4.340 | Si | otra fase |
| `source/blender/blendthumb` | 1 | 186 | Si | suelta |

**Los 5.060 de Cycles no se compilan, y esto esta verificado, no supuesto:**
`WITH_CYCLES:BOOL=OFF` en `dev/build/CMakeCache.txt`, y
`grep -c 'cycles/device/metal' dev/build/build.ninja` da **0**. Migrarlos hoy no
quita ni una linea de Objective-C del binario y, peor, **no habria forma de
verificarlo**: no se puede comprobar lo que no se compila. Recomendacion: se migran
cuando (y si) se encienda Cycles. Son el 17% del recuento total y el 0% del problema
real.

## 2. La medida que de verdad importa: no son lineas, son envios de mensaje

El recuento por lineas enga&#241;a. Lo que hay que reescribir es cada `[objeto mensaje]`;
el resto del fichero ya es C++ y se queda igual. Medido en los 20 ficheros:

| Fichero | Lineas | Envios `[ ]` | Bloques `^` | Literales `@` |
|---|---:|---:|---:|---:|
| mtl_shader_log.mm | 110 | **0** | 0 | 0 |
| mtl_shader_interface.mm | 752 | 1 | 0 | 0 |
| mtl_uniform_buffer.mm | 202 | 1 | 0 | 1 |
| mtl_query.mm | 129 | 4 | 0 | 0 |
| mtl_state.mm | 715 | 2 | 0 | 0 |
| mtl_framebuffer.mm | 2.007 | 3 | 0 | 0 |
| mtl_index_buffer.mm | 566 | 3 | 0 | 1 |
| mtl_immediate.mm | 348 | 8 | 0 | 3 |
| mtl_batch.mm | 930 | 11 | 0 | 6 |
| mtl_debug.mm | 184 | 11 | 0 | 2 |
| mtl_storage_buffer.mm | 526 | 11 | 0 | 1 |
| mtl_vertex_buffer.mm | 363 | 11 | 0 | 1 |
| mtl_memory.mm | 1.121 | 14 | 0 | 2 |
| mtl_shader_generator.mm | 3.413 | 16 | 0 | 8 |
| mtl_texture_util.mm | 852 | 22 | 0 | 27 |
| mtl_backend.mm | 614 | 26 | 0 | 0 |
| mtl_command_buffer.mm | 1.089 | 54 | **1** | 3 |
| mtl_context.mm | 2.749 | 67 | **1** | 11 |
| mtl_texture.mm | 2.685 | 89 | 0 | 17 |
| mtl_shader.mm | 1.602 | 91 | 0 | 25 |

**El fichero mas grande del backend (`mtl_shader_generator.mm`, 3.413 lineas) tiene
16 envios. `mtl_framebuffer.mm`, 2.007 lineas, tiene 3.** Son generadores de texto
MSL y contabilidad de estado: C++ que estaba en un `.mm` por vecindad, no por
necesidad. Quedan **441 envios de mensaje** en los 18 ficheros pendientes. Ese es el
tama&#241;o real del trabajo, no 21.000 lineas.

## 3. La unidad de migracion NO es el fichero: es la capa de cabeceras

Esta es la leccion cara de la noche. El primer `.cc` cuesta lo que cuesta **toda la
capa de cabeceras**; a partir de ahi cada fichero es barato.

Motivo: `mtl_context.hh` **la incluyen 22 ficheros** y arrastraba `<Cocoa/Cocoa.h>` y
`intern/GHOST_ContextCGL.hh` (Objective-C puro). Mientras eso siguiera asi, ningun
fichero de `gpu/metal` podia dejar de ser `.mm`, porque un `.cc` **ni siquiera puede
leer la cabecera**. Medido: un `.cc` que solo hiciera `#include "mtl_query.hh"`
producia **36.334 errores**, todos dentro de las cabeceras del SDK de Apple.

Progresion real hasta dejarlo en cero:

| Paso | Errores |
|---|---:|
| Punto de partida | 36.334 |
| Cortadas las inclusiones ObjC (Cocoa, Metal.h, GHOST_ContextCGL) | 511 |
| A&#241;adidos los alias de tipo de metal-cpp | 261 |
| A&#241;adidos los alias `XxxPtr` | 98 |
| Corregido el generador de alias | 40 |
| Aliasado el enumerado completo | **0** |

## 4. La tecnica: un nombre de tipo que vale en los dos modos

`mtl_objc_compat.hh` declara, para cada protocolo Metal, un alias `XxxPtr`:

```cpp
en un .mm :  using MTLDevicePtr = id<MTLDevice>;
en un .cc :  using MTLDevicePtr = MTL::Device *;
```

Las cabeceras escriben `MTLDevicePtr` y dejan de tener sintaxis de Objective-C. Los
`.mm` **no cambian**: para ellos el miembro sigue siendo exactamente `id<MTLDevice>` y
sus `[obj mensaje]` siguen compilando. Asi se migra **un fichero cada vez** en lugar
de los veinte de golpe.

Es seguro porque `id<MTLDevice>` y `MTL::Device *` son **el mismo puntero**
(`objc_object *`). metal-cpp no envuelve ni copia: declara clases sin miembros de
datos cuyos metodos llaman a `objc_msgSend`. Mismo tama&#241;o, misma disposicion, misma
ABI. Un `.mm` y un `.cc` pueden discrepar en la grafia sin romper el enlazado.

## 5. Que cubre metal-cpp y que no

**Cubre** (verificado compilando y EJECUTANDO en esta maquina, Mac Intel con AMD
Radeon Pro 5300M): Foundation, Metal, QuartzCore y MetalFX. La prueba de humo abrio
el dispositivo real (`device=AMD Radeon Pro 5300M unified=0`) y reservo un buffer de
4096 B con puntero de host valido. **Importa que `hasUnifiedMemory()==false`**: el
backend tiene rutas de sincronizacion que solo se ejecutan en GPU discreta, y son
justo las que toca el piloto.

**NO cubre, y hay que resolverlo a mano cuando toque:**

- **AppKit/Cocoa.** metal-cpp no incluye NSWindow, NSView ni NSApplication. Los 4.340
  de `intern/ghost` no tienen binding oficial: o se escribe el pegamento sobre el
  runtime de C (`objc_getClass` / `sel_registerName` / `objc_msgSend`), o se quedan.
- **Bloques (`^`).** Son extension de Clang, no C++ estandar. metal-cpp los expone
  como punteros a funcion en algunos sitios, pero los manejadores de finalizacion de
  `MTLCommandBuffer` son bloques de verdad. Solo hay **2 en todo el backend**
  (`mtl_command_buffer.mm` y `mtl_context.mm`), pero son los dos ficheros mas dificiles.
- **Fabricar clases en tiempo de ejecucion** (delegados). No hace falta en `gpu/metal`;
  hara falta en ghost.
- **`@available`.** Se sustituye por `__builtin_available`, que Clang acepta en C++.
  Verificado: 1 sitio en `mtl_texture.hh`.
- **Literales `@""`.** No hay equivalente en C++ puro. Ver la trampa 5 de abajo.

## 6. Trampas medidas (cada una costo tiempo real)

1. **`id<T>` NO se puede imitar en C++.** El primer intento fue
   `template<typename T> using id = T *;` para no tocar las cabeceras. **Falla**:
   metal-cpp incluye `<objc/runtime.h>` &rarr; `<objc/objc.h>`, que ya hace
   `typedef struct objc_object *id;`. Da «redefinition of 'id' as a different kind of
   symbol». No hay atajo: hay que cambiar la grafia en las cabeceras.

2. **63 nombres `MTL*` son de Blender, no de Apple.** `MTLBuffer`, `MTLTexture`,
   `MTLSamplerState`, `MTLFence`, `MTLVertexDescriptor`, `MTLContext`... En
   Objective-C no chocan con los protocolos hom&#243;nimos porque los protocolos tienen su
   propio espacio de nombres; **en C++ si chocan**, y dentro de `namespace
   blender::gpu` gana la clase de Blender. Traducir `id<MTLBuffer>` a ciegas daria
   `blender::gpu::MTLBuffer *`: **compila, y es silenciosamente el tipo equivocado**.
   Caso real cazado: `MTLBufferRange` es una struct de `mtl_memory.hh` y el generador
   le puso un alias a un `MTL::BufferRange` de Apple que ni existe (es
   `MTL4::BufferRange`). Por eso los alias excluyen los 63 nombres propios y esos se
   escriben explicitos (`MTL::Buffer *`).

3. **Aliasar «solo lo que se usa» no basta.** `mtl_shader.hh` fabrica nombres como
   `MTLVertexFormatCharNormalized` pegando tokens dentro de una macro
   (`RESIZE_TYPE(Char, Normalized)`). Esos identificadores **no existen en el texto** y
   ninguna busqueda por patron los encuentra. Dejaban 92 errores inexplicables. Hay
   que aliasar el enumerado completo: 327 tipos + 868 constantes.

4. **Las cabeceras `MTL4*.hpp` son otra API.** Son Metal de nueva generacion y viven
   en el namespace `MTL4`, no en `MTL`. Colarlas daba 89 errores «no type named
   'Archive' in namespace 'MTL'». Y **`CAMetalLayer` es una CLASE, no un protocolo**:
   escribir `id<CAMetalLayer>` rompio los 19 `.mm` de golpe.

5. **`@""` no tiene equivalente en C++ puro.** En Objective-C es un literal inmortal
   creado por el compilador; fabricar un `NS::String` en el inicializador de un miembro
   reservaria memoria en cada construccion. Se usa `nullptr` mediante
   `MTL_NSSTRING_EMPTY`. **No es equivalente al 100%**: mandar un mensaje a `nil`
   devuelve 0/nil sin fallar, asi que `[s length]` da 0 en ambos casos, pero
   `[array addObject:s]` si distingue. Hoy solo afecta a los seis `NSString *` de
   `mtl_shader.hh`, que unicamente toca `mtl_shader.mm` (aun Objective-C++). **Cuando
   se migre ese fichero hay que resolverlo de verdad y verificarlo, no darlo por hecho.**

6. **`[x release]` sobre nulo cambia de significado.** En Objective-C es un no-op
   legal; en C++ `x->release()` sobre `nullptr` revienta. El helper `mtl_release()`
   comprueba el nulo **en las dos ramas** a proposito: sin esa guarda, el cambio de
   modo convierte un no-op silencioso en un fallo de segmentacion.

7. **metal-cpp necesita UN traductor que materialice sus simbolos.** Exactamente un
   `.cc` debe definir `NS_PRIVATE_IMPLEMENTATION` / `MTL_PRIVATE_IMPLEMENTATION` /
   `CA_PRIVATE_IMPLEMENTATION`. En ninguno: simbolos indefinidos de `NS::Private`. En
   dos: simbolos duplicados. Es `metal/mtl_cpp_impl.cc` y no debe contener nada mas.

## 7. Version de metal-cpp: por que no la ultima

Vendorizado en `extern/metal-cpp/` el tag `release/metal-cpp_macOS26.4_iOS26.4`
(5c00949), **no** el ultimo (`macOS27`). El SDK de esta maquina es 26.5
(`xcrun --show-sdk-version`). metal-cpp compila contra cualquier SDK porque es solo un
envoltorio de `objc_msgSend`, pero unas cabeceras mas nuevas que el SDK dejarian
llamar selectores que en este sistema no existen, y **ese fallo aparece en ejecucion,
no en compilacion**. Se fija el tag inmediatamente inferior al SDK.

Apple ya no publica el zip en `developer.apple.com/metal/cpp`: esa pagina solo enlaza
a `github.com/apple/metal-cpp`.

## 8. Orden recomendado para los 18 que quedan

La capa de cabeceras ya esta pagada, asi que el coste de cada fichero es ahora
proporcional a sus envios de mensaje. Orden propuesto, de menos riesgo a mas:

**Tanda 1 — casi gratis (10 envios o menos, 4.591 lineas, ~20 envios en total).**
`mtl_shader_interface.mm` (1), `mtl_uniform_buffer.mm` (1), `mtl_state.mm` (2),
`mtl_framebuffer.mm` (3), `mtl_index_buffer.mm` (3), `mtl_immediate.mm` (8).
Son dos tercios de un dia de trabajo para quitar 4.591 lineas de `.mm`.

**Tanda 2 — mecanicos (11-26 envios).** `mtl_batch.mm`, `mtl_debug.mm`,
`mtl_storage_buffer.mm`, `mtl_vertex_buffer.mm`, `mtl_memory.mm`,
`mtl_shader_generator.mm`, `mtl_texture_util.mm`, `mtl_backend.mm`.
Ojo con `mtl_texture_util.mm`: 27 literales `@`, casi todos nombres de kernel.

**Tanda 3 — los dificiles, al final.** `mtl_texture.mm` (89), `mtl_shader.mm` (91),
`mtl_command_buffer.mm` (54 + **bloque**), `mtl_context.mm` (67 + **bloque** + el
unico acceso a GHOST). Aqui hay que resolver de verdad los bloques de los manejadores
de finalizacion y el `@""` de la trampa 5.

**Coste real por fichero, medido en el piloto:** un fichero de la tanda 1 son minutos
(cambiar N envios y la entrada del CMakeLists). Lo que costo horas fue la capa de
cabeceras, y ya no se vuelve a pagar.

## 9. Estado

- `extern/metal-cpp/` vendorizado con procedencia y licencia.
- Las 24 cabeceras de `gpu/metal`, sin sintaxis de Objective-C. 0 errores desde C++.
- `mtl_objc_compat.hh`: el puente, con las trampas escritas dentro.
- **Migrados 2 de 20**: `mtl_shader_log.cc` (tenia 0 envios: era C++ disfrazado de
  `.mm`) y `mtl_query.cc` (el piloto con metal-cpp, 4 envios).
- Quedan **18 ficheros y 441 envios** en `gpu/metal`.

## 9-bis. Como se verifica esto, que es la parte delicada

Al pasar de `.mm` a `.cc` **el compilador deja de comprobar los selectores**: un
`objc_msgSend` mal casteado compila igual y falla en ejecucion. Por eso aqui hace
falta MAS evidencia que en una migracion de interfaz, no menos. Lo hecho en el piloto:

1. **Build verde**: `nb install` -> rc=0, 0 errores.
2. **Los ficheros entraron de verdad en el build** (trampa de las 04:20 del
   REGLAMENTO): `grep -c <fichero> dev/build/build.ninja` da 3 para cada `.cc` nuevo
   y **0** para los `.mm` retirados; los `.o` existen con fecha de la build.
3. **Player**: `Blenderplayer ArpgNative.blend` da
   `Flipendo: componentes nativos atados 5/5 en la escena 'Scene'`, sigue vivo y
   dibuja la escena.
4. **Editor en modo grafico**: arranca, sigue vivo a los 15 s, **0 errores**.
5. **Comparacion de pixeles.** Render EEVEE de ArpgNative a 1920x1080, dos
   ejecuciones del mismo binario, PNG decodificado a mano (esta maquina no tiene PIL):

   | Medida | Valor |
   |---|---:|
   | Pixeles distintos | **0 de 2.073.600 (0,000%)** |
   | Diferencia maxima por canal | **0/255** |
   | Pixeles con color | 100% |
   | Luminancia media | 62,93/255 |

   **El suelo de ruido de este motor es CERO**: el render es exactamente
   reproducible. Eso es mejor arnes del que tuvo el carril de VideoTexture, que tuvo
   que comparar contra el ruido del propio motor. Consecuencia practica: esta imagen
   sirve de **linea base congelada** para los 18 ficheros que quedan, y cualquier
   diferencia futura sera real y no ruido. Conviene repetir el render despues de cada
   fichero migrado y exigir 0 pixeles de diferencia.

   **Aviso honesto:** esto NO es un antes/despues. El binario anterior a la migracion
   lo sobrescribio la propia build y no se reconstruyo por presupuesto de bateria. Lo
   demostrado es que el motor rinde identico consigo mismo; el antes/despues real ya
   se puede hacer contra esta cifra.

Trampa del arnes: **macOS no trae `timeout`**. El primer intento de lanzar el Player
dio rc=127 y parecia un fallo del binario; era el `timeout` inexistente.

## 11. LA RESTRICCION QUE MANDA SOBRE TODAS: el mangling de C++

Descubierto al enlazar la primera tanda, y **cambia el orden de migracion entero**.
Lo anterior de este documento sigue valiendo, pero esta seccion manda sobre la §8.

La §4 decia que `id<MTLDevice>` y `MTL::Device *` son el mismo puntero y que por eso
un `.mm` y un `.cc` pueden discrepar en la grafia. **Eso es cierto para los datos y
FALSO para las funciones.** El nombre mangleado de una funcion en C++ incluye los
tipos de sus parametros, asi que:

    declarada en la cabecera comun, definida en un .mm, llamada desde un .cc
      -> el .mm exporta  _ZN...set_labelEP8NSString
      -> el .cc  pide    _ZN...set_labelEPN2NS6StringE
      -> dos simbolos distintos: NO ENLAZA

Compila perfectamente en los dos lados y revienta en el enlazado. Cifras reales de la
primera tanda: los 5 ficheros compilaron sin un solo error y el enlace fallo con
**9 simbolos indefinidos** en 4 grupos:

| Simbolo | Definido en | Llamado desde |
|---|---|---|
| `MTLShaderInterface::insert_argument_encoder` | mtl_shader_interface**.cc** | mtl_context.mm |
| `MTLCommandBufferManager::encode_signal_event` / `encode_wait_for_event` | mtl_command_buffer.mm | mtl_state**.cc** |
| `MTLBuffer::set_label(NSString*)` | mtl_memory.mm | mtl_index_buffer**.cc** |
| `MTLShader::set_*_function_name`, `shader_source_from_msl`, `shader_compute_source_from_msl` | mtl_shader.mm | mtl_shader_generator**.cc** |

### La regla que sale de aqui

> **Un `.cc` solo puede llamar a funciones con tipos Metal en la firma si quien las
> define tambien es `.cc`.** La migracion sigue el GRAFO DE LLAMADAS, no la densidad
> de Objective-C.

Es decir: la §8 ordenaba por coste de traduccion y **eso era necesario pero no
suficiente**. Un fichero barato de traducir puede ser imposible de enlazar si depende
de un fichero caro que sigue en `.mm`. Hay que mirar las dos cosas.

### Las dos salidas cuando aparece un simbolo indefinido

1. **Migrar tambien al que define** (lo correcto cuando se puede). `mtl_index_buffer`
   necesita `mtl_memory`; `mtl_shader_generator` necesita `mtl_shader`. Van juntos o
   no van.
2. **Usar un tipo NEUTRAL en la firma**: `id` pelado es `objc_object *` en los DOS
   modos, asi que mangla igual y cruza la frontera sin problema. Es lo que se hizo con
   `MTLShaderInterface::insert_argument_encoder(int, id)`, que la llama
   `mtl_context.mm` (bloqueado por GHOST y por tanto intocable a corto plazo). Se paga
   con perdida de tipado: usar solo cuando el definidor no se puede migrar.

Lo que **no** vale es cambiar la firma a `XxxPtr` y confiar: eso es justo lo que
produjo los 9 simbolos indefinidos.

### 11-bis. La salida buena: quitar el tipo Metal de la firma

Las dos salidas de arriba (migrar al definidor, o usar `id` pelado) son parches. Hay
una tercera que es mejor que las dos y que conviene preferir siempre que la funcion lo
permita: **que la firma no tenga ningun tipo de Metal**.

Caso real de esta tanda. `gpu::MTLBuffer::set_label()` recibia `NSString *` y la
llaman ficheros de los dos lados. Con `NSString *` cada lado generaba un simbolo
distinto (`...set_labelEP8NSString` frente a `...set_labelEPN2NS6StringE`) y no
enlazaba. La solucion no fue castear: fue cambiar la firma a

    void set_label(const char *str);

Un tipo del lenguaje base se escribe igual en los dos modos, asi que la frontera
**deja de existir** para esa funcion: ya da igual si quien la llama es `.mm` o `.cc`,
ahora y despues. Y de paso es mejor API, porque la etiqueta es texto de depuracion y
nunca tuvo que ser un objeto de Metal. La conversion a `NSString` se hace dentro, una
sola vez, donde toca.

Orden de preferencia al encontrarse un simbolo indefinido:

1. **Quitar el tipo Metal de la firma** (`const char *`, `int`, un enum propio...).
   Resuelve el problema para siempre y suele dejar mejor API.
2. **Migrar tambien al definidor**, si se puede y el grafo no explota.
3. **`id` pelado**, solo cuando al definidor no se le puede tocar (bloqueado por
   GHOST). Se paga con perdida de tipado y un `reinterpret_cast` en el `.cc`.

### 11-quater. El comprobador de dependencias tambien se equivoca

Hay un script (`deps.py`) que antes de migrar dice que funciones con tipo Metal
cruzarian la frontera. Ahorra builds, pero **no es una autoridad**: cada vez que me fie
de un «0 bloqueos» sin mirar, el enlace me corrigio. Tres fallos suyos, ya conocidos:

1. **Unia las lineas de continuacion ANTES de quitar comentarios**, asi que el `*/` del
   comentario de arriba quedaba pegado a la declaracion y la expresion regular no la
   reconocia. Se salto `MTLContext::ensure_render_pipeline_state`. Corregido: limpia
   comentarios primero.
2. **Casa por NOMBRE de funcion, sin clase.** Hay dos `blit()`: `MTLFrameBuffer::blit`
   (solo `uint`, inofensiva) y `MTLTexture::blit(MTLBlitCommandEncoderPtr, ...)`. El
   script aviso de «blit», yo mire la de MTLFrameBuffer, la di por falso positivo y el
   enlace fallo por la otra. **Ante un aviso, hay que mirar CADA sobrecarga.**
3. **Confunde los `MTL*` de Blender con los de Apple.** `gpu::MTLBuffer *` en una firma
   no es un tipo de Objective-C y no rompe nada, pero el patron `MTL[A-Z]\w+` lo marca.
   De ahi salen falsos positivos como `set_visibility_buffer` o `MTLStorageBuf`.

Resumen util: el script sirve para **encontrar candidatos**, y el enlazador es el unico
que dictamina. Un `nb install` fallido no es un fracaso del metodo: es la verificacion
funcionando.

### 11-ter. Comprobar el grafo ANTES de compilar, y en TODAS las cabeceras

Error propio que costo una build entera: comprobe las firmas con tipos Metal de
`mtl_memory.hh` (el fichero que migraba) y di el cluster por cerrado. Pero
`mtl_storage_buffer` no fallaba por `mtl_memory`: fallaba por `MTLComputeState::bind_pso`
y `MTLCommandBufferManager::encode_signal_event`, que estan en **`mtl_context.hh`** y
los define codigo bloqueado por GHOST.

La comprobacion util no es «que funciones con tipo Metal declara MI cabecera» sino
**«a que funciones con tipo Metal llama el fichero que quiero migrar, esten donde
esten, y quien las define»**. Se hace en segundos con grep y ahorra una build de
varios minutos.

## 12. Los tres ficheros que GHOST bloquea

Medido compilando cada `.mm` como C++ (metrica objetiva, seccion 13): tres ficheros
dan ~36.000 errores cada uno mientras el resto se queda por debajo de 360. No es que
tengan mas Objective-C: es que **incluyen la capa Cocoa de GHOST**, que no es de este
carril.

| Fichero | Lineas | Que arrastra | ¿Se puede quitar? |
|---|---:|---|---|
| `mtl_backend.mm` | 614 | `<Cocoa/Cocoa.h>` | **Si**: solo usa NSString y NSProcessInfo, y metal-cpp cubre los dos (`NS::ProcessInfo`). Quitar la inclusion deberia bajarlo al rango normal. |
| `mtl_command_buffer.mm` | 1.089 | `intern/GHOST_ContextCGL.hh` | Solo por la constante `GHOST_ContextCGL::max_command_buffer_count`. Hace falta una forma de leerla sin Cocoa. |
| `mtl_context.mm` | 2.749 | `intern/GHOST_ContextCGL.hh` | **No**: hace `dynamic_cast<GHOST_ContextCGL *>`, necesita el tipo completo. Va con la fase de GHOST, no antes. |

`mtl_context.mm` es ademas quien llama a `insert_argument_encoder`, asi que arrastra a
`mtl_shader_interface` a usar firma neutral. **Es el tapon de la migracion**, y esta
fuera de este carril por decision del encargo.

## 13. La metrica buena: compilar el `.mm` como C++ y contar errores

Las dos metricas anteriores (lineas, y envios `[ ]`) se quedaron cortas. La de envios
**no ve los accesos a propiedad con punto**, que son sintaxis de Objective-C igual de
real: `buffer.storageMode`, `descriptor.depthAttachment.texture = ...`. Medidos en el
backend: ~128 accesos de ese tipo que la cuenta de 441 envios NO incluia. Caso
flagrante: `mtl_framebuffer.mm` parecia tener **3 envios** y al compilarlo como C++
da **136 errores**, casi todos cadenas de propiedades.

La metrica objetiva es empirica: **copiar el `.mm` a `.cc` y contar los errores**.

    clang++ -x c++ -ferror-limit=0 <flags reales del build> -fsyntax-only fichero.cc | grep -c error:

| Fichero | Lineas | Errores como C++ | Estado |
|---|---:|---:|---|
| mtl_shader_log | 110 | 0 | **migrado** |
| mtl_query | 129 | (piloto) | **migrado** |
| mtl_uniform_buffer | 202 | 1 | **migrado** |
| mtl_state | 715 | 2 | traducido; **bloqueado** por mtl_command_buffer (GHOST) |
| mtl_shader_interface | 752 | 5 | **migrado** (firma neutral `id`) |
| mtl_shader_generator | 3.413 | 6 | **migrado** |
| mtl_immediate | 348 | 8 | **migrado** |
| mtl_index_buffer | 566 | 12 | **migrado** |
| mtl_texture_util | 852 | 12 | **migrado** |
| mtl_batch | 930 | 29 | **migrado** |
| mtl_debug | 184 | 29 | **migrado** |
| mtl_memory | 1.121 | 32 | **migrado** |
| mtl_storage_buffer | 526 | 33 | traducido; **bloqueado** por GHOST |
| mtl_vertex_buffer | 363 | 33 | **migrado** |
| mtl_shader | 1.602 | 52 | **migrado** |
| mtl_framebuffer | 2.007 | 136 | **migrado** |
| mtl_texture | 2.685 | 357 | bloqueado por mtl_command_buffer (GHOST) |
| mtl_backend | 614 | 35.734 -> ~20 | **migrado** (era solo `<Cocoa/Cocoa.h>`) |
| mtl_command_buffer | 1.089 | 35.851 | GHOST |
| mtl_context | 2.749 | 36.019 | GHOST — el tapon |

**Dato que reordena el trabajo:** `mtl_shader_generator.mm`, 3.413 lineas (el fichero
mas grande del backend), da **6 errores**. Ya esta traducido y verificado como C++;
solo espera a que `mtl_shader.mm` (52) lo desbloquee. Los dos juntos son 5.015 lineas.

### Ese orden ya se ejecuto

Se siguio tal cual y funciono: memoria+indices+vertices, luego el cluster del shader,
luego los sueltos, luego framebuffer y backend. Lo unico que cambio sobre el plan es que
`mtl_storage_buffer` resulto estar bloqueado por GHOST (usa `MTLComputeState`) y que
`mtl_texture` tambien lo esta. Estado final y motivos, en la seccion 16.

## 14. El convertidor mecanico: lo que automatiza y donde MIENTE

Hay un convertidor en el scratchpad (`conv.py`, `fixdot.py`, `unfix.py`) que hace los
idiomas repetitivos. Automatiza bien:

  [obj sel]                -> obj->sel()
  [obj sel:a otra:b]       -> obj->sel(a, b)        (metal-cpp usa el PRIMER trozo del
                                                     selector y el resto por posicion)
  obj.prop                 -> obj->prop()
  obj.prop = v             -> obj->setProp(v)
  id<X>                    -> XPtr
  @"texto"                 -> mtl_string("texto")
  NSMakeRange / MTLSizeMake-> NS::Range::Make / MTL::Size::Make

**Y se equivoca de cuatro formas, todas vistas de verdad en `mtl_shader`:**

1. **Confunde metodos de CLASE con envios a instancia.** `[NSString stringWithFormat:]`
   y `[[MTLCompileOptions alloc] init]` no son mensajes a un objeto existente. Salia
   `NSString->stringWithFormat(...)`, que no compila. Hay que hacerlos a mano.
2. **Se come las comparaciones.** El filtro para no confundir el setter `.prop = v` con
   otra cosa tambien capturaba `.prop == v`. Convirtio
   `if (current_attribute.format == MTLVertexFormatInvalid)` en
   `if (current_attribute->setFormat(= MTLVertexFormatInvalid)`. Corregido con un
   lookahead que distingue `=` de `==`, pero conviene revisar los `==` a mano.
3. **Toca COMENTARIOS y cadenas.** Convirtio el comentario «require Metal 3.1» en
   «require Metal 3->1()». Solo hubo uno, pero es el fallo mas peligroso del lote
   porque **compila**. Comprobacion barata que conviene repetir siempre:
   `grep -nE '[0-9]->[0-9A-Za-z_]+\(\)' fichero.cc`
   y diff de los literales de cadena contra el `.mm` original.
4. **Aplica la conversion a structs que NO son de Metal.** `MTLContextGlobalShaderPipelineState`
   es una struct de Blender, no un objeto de Objective-C: `ctx->pipeline_state.active_shader = x`
   acabo como `->setActive_shader(x)`. El compilador lo caza
   («member reference type ... is **not** a pointer»), y de ahi salio `unfix.py`, que
   deshace justo esos.

**Lo que el convertidor NO puede garantizar, y por eso el render es obligatorio:** que
el nombre de metal-cpp sea el mismo que el del selector. No siempre lo es:

  newBufferWithLength:options:            -> newBuffer(...)
  newFunctionWithName:constantValues:     -> newFunction(...)
  newLibraryWithSource:options:           -> newLibrary(...)
  newComputePipelineStateWithDescriptor:  -> newComputePipelineState(...)
  newRenderPipelineStateWithDescriptor:   -> newRenderPipelineState(...)
  objectAtIndex:                          -> object(...)
  UTF8String                              -> utf8String()

Todos estos dan **error de compilacion**, no comportamiento indefinido, porque ya son
llamadas a metodo de C++: el compilador comprueba que el metodo exista. El riesgo real
que queda es el **orden de los argumentos** y la **semantica**, y contra eso solo vale
el render contra la linea base.

Dos diferencias de API que hay que saber:

- `NSArray<MTLArgument *> *` no existe: `NS::Array` **no es una plantilla**. Se escribe
  `NS::Array *` y se recupera el tipo con `static_cast` al sacar cada elemento.
- Los arrays de descriptores (`colorAttachments[i]`, `vertexDescriptor.layouts[i]`) no
  admiten `[]`: se accede con `->object(i)`.

## 15. Dos cosas de Objective-C que NO tienen equivalente y como se resolvieron

**`@autoreleasepool { ... }`** pasa a una guarda RAII, `MTLAutoreleasePoolScope`, que se
declara al principio del bloque. El pool se drena al salir del ambito, tambien por
excepcion, igual que la construccion original. Ojo: `NS::AutoreleasePool` se libera con
`release()`, no con `drain()`; sin recoleccion de basura son lo mismo y metal-cpp solo
expone el primero.

**`@""`** era un literal inmortal creado por el compilador. Primero se puso `nullptr`, y
**era un fallo latente**: `mtl_shader.hh` inicializa seis `NSString *` con ese valor y
`mtl_shader` hace `[x length]` sobre ellos. En Objective-C mandar un mensaje a `nil`
devuelve 0 sin fallar; en C++ puro `nullptr->length()` es un cierre por violacion de
segmento. Se resolvio con `mtl_empty_string()`: un `NS::String` vacio creado una sola
vez y retenido para siempre, no nulo y de longitud 0, que reproduce el comportamiento
observable del original. **Es el ejemplo perfecto de por que aqui hace falta mas
verificacion: el codigo compilaba igual con `nullptr`.**

## 16. [SUPERADA por la seccion 17] Donde se paro cuando GHOST aun bloqueaba

**15 de 20 ficheros de `gpu/metal` migrados.** En el arbol entero, el Objective-C++
baja de **30.536 a 17.362 lineas**. Los 5 que quedan NO son los mas dificiles de
traducir: son los que dependen de GHOST/Cocoa, directa o indirectamente.

| Fichero | Lineas | Menciones a GHOST | Situacion |
|---|---:|---:|---|
| mtl_context.mm | 2.753 | 18 | Hace `dynamic_cast<GHOST_ContextCGL *>`. **El tapon.** |
| mtl_command_buffer.mm | 1.092 | 4 | Usa `GHOST_ContextCGL::max_command_buffer_count`. |
| mtl_texture.mm | 2.688 | 1 | Bloqueado por las 3 funciones de `MTLComputeState` que define mtl_command_buffer. |
| mtl_state.mm | 715 | 0 | **Ya traducido y compilando como C++**; espera a `encode_signal_event`. |
| mtl_storage_buffer.mm | 528 | 0 | **Ya traducido**; espera a `MTLComputeState` y `encode_signal_event`. |

`mtl_state` y `mtl_storage_buffer` estan traducidos, compilan como C++ con 0 errores y
**no enlazan**: los revierte a `.mm` una sola dependencia cada uno. No es trabajo
pendiente de traduccion, es trabajo pendiente de GHOST.

### La decision: no se siguen neutralizando firmas

Para llegar hasta aqui hubo que neutralizar **8 firmas** (a `id` o `uint64_t`) porque
su definidor esta bloqueado por GHOST. Cada una es un trozo de tipado que se pierde y
una nota de «revertir cuando migre X». Seguir con `mtl_texture` costaria **tres mas**, y
las tres sobre la API de enlace de computacion (`bind_pso`, `bind_compute_buffer`,
`bind_compute_texture`), que es camino caliente y central del backend.

Ahi la cuenta deja de salir: se cambiaria tipado real del nucleo por avanzar un fichero
que, de todas formas, no se puede terminar de limpiar hasta que caiga GHOST. **Lo
sensato es que los 5 restantes vayan con la fase de GHOST/Cocoa, no antes.** Cuando
`intern/ghost` deje de ser Objective-C, los cinco caen casi de golpe y ademas se
revierten las 8 neutralizaciones, que estan todas anotadas en su sitio con la condicion
exacta para deshacerlas.

### Las 8 firmas neutralizadas y cuando revertirlas

En `mtl_memory.hh`, `mtl_context.hh`, `mtl_shader.hh` y `mtl_texture.hh`, todas con
comentario en el sitio:

  MTLBufferPool::init(id)                          <- revertir con mtl_context
  MTLShaderInterface::insert_argument_encoder(id)  <- revertir con mtl_context
  MTLRenderPassState::bind_vertex_buffer(id, ...)  <- revertir con mtl_command_buffer
  MTLShader::bake_current_pipeline_state(uint64_t) <- revertir con mtl_context
  MTLContext::ensure_render_pipeline_state(uint64_t)  <- revertir con mtl_context
  MTLContext::ensure_depth_stencil_state(uint64_t)    <- revertir con mtl_context
  MTLTexture::blit(id, ...)                        <- revertir con mtl_texture
  mtl_format_supports_blending / get_mtl_format_bytesize /
  get_mtl_format_num_components (uint64_t)         <- revertir con mtl_texture

La excepcion es `MTLBuffer::set_label(const char *)`, que **no** hay que revertir: ahi
no se neutralizo un tipo, se quito de la firma un tipo que nunca debio estar. Es el
patron preferente (§11-bis).

## 17. CERRADO: `gpu/metal` es C++ puro, y las 8 firmas estan revertidas

El carril de GHOST quito Cocoa de `GHOST_ContextCGL.hh` y con eso cayo el tapon. Los
cinco ficheros que quedaban pasaron de 35.851 y 36.019 errores a 152 y 305, con **cero
dentro de cabeceras del SDK**: lo que parecia imposible eran 836 errores normales en
7.773 lineas.

**`source/blender/gpu/metal`: 21 ficheros, todos `.cc`. Cero `.mm`.**

### Las 8 firmas neutralizadas, revertidas

Era deuda contraida a regañadientes y la condicion para deshacerla —que GHOST dejara de
arrastrar Cocoa— se cumplio. Como ya no queda ningun `.mm` en `gpu/metal`, la frontera
interna desaparecio y todas vuelven a su tipo real:

| Firma | Vuelve a |
|---|---|
| `MTLBufferPool::init` | `MTLDevicePtr` |
| `MTLShaderInterface::insert_argument_encoder` | `MTLArgumentEncoderPtr` |
| `MTLRenderPassState::bind_vertex_buffer` | `MTLBufferPtr` |
| `MTLShader::bake_current_pipeline_state` | `MTLPrimitiveTopologyClass` |
| `MTLContext::ensure_render_pipeline_state` | `MTLPrimitiveType` |
| `MTLContext::ensure_depth_stencil_state` | `MTLPrimitiveType` |
| `MTLTexture::blit` | `MTLBlitCommandEncoderPtr` |
| `get_mtl_format_bytesize` / `get_mtl_format_num_components` / `mtl_format_supports_blending` | `MTLPixelFormat` |

Con ellas se van los `reinterpret_cast` que las acompañaban. `MTLBuffer::set_label` NO
se revierte y es deliberado: ahi no se neutralizo un tipo, se quito de la firma un tipo
que nunca debio estar (§11-bis). `const char *` es la firma correcta.

### La unica que NO se puede revertir, y por que

`blender::gpu::present(id, id, id, id)`. Es el callback de presentacion que **registra
GHOST**, asi que su firma la fija `GHOST_ContextCGL::GHOST_MetalPresentCallback`, no
este backend. Mientras `GHOST_WindowCocoa.mm` siga siendo Objective-C++, esa firma cruza
modos de verdad, y en un parametro la grafia entra en el simbolo:

    id<MTLTexture>  ->  PU21objcproto10MTLTexture11objc_object
    MTL::Texture *  ->  PN3MTL7TextureE
    id              ->  P11objc_object   (en los DOS modos)

Los tipos se recuperan dentro de `present()` con `reinterpret_cast` y una advertencia
escrita: ahi el compilador no comprueba nada, y si GHOST cambiara el ORDEN de los
argumentos esto seguiria compilando.

**ACTUALIZACION (mismo dia, tras cerrar Metal):** esa condicion YA SE CUMPLE. El carril
de GHOST migro `GHOST_ContextCGL.cc` y `GHOST_WindowCocoa.cc`, y medido despues:
**ningun `.mm` del arbol usa el callback**. El unico `.mm` que queda en GHOST
(`GHOST_SystemCocoa.mm`) incluye la cabecera pero no toca el tipo (0 referencias); todos
los usuarios son `.cc`. La frontera de enlazado ha desaparecido y el typedef puede
volver a tipos reales, con lo que `present()` perderia sus cuatro `reinterpret_cast`.
No se hace desde aqui porque el typedef vive en `intern/ghost`, que es de otro carril.

### El andamio `mtl_objc_compat.hh`

Su condicion de borrado («cuando el ultimo `.mm` de gpu/metal pase a `.cc`») **ya se
cumple**, y nadie fuera de `gpu/metal` lo incluye de verdad (solo se menciona en
comentarios de GHOST). Se deja de momento a proposito: retirar la rama `__OBJC__` es un
cambio que no aporta comportamiento y que conviene hacer cuando GHOST termine, para no
tocar dos veces. Queda como unica tarea de limpieza pendiente del backend.

### Verificacion final (build verde, con las 8 firmas ya revertidas)

| Comprobacion | Resultado |
|---|---|
| `nb install` | **rc=0**, 0 errores, 0 simbolos indefinidos |
| Ficheros de `gpu/metal` | **21/21 `.cc`**, 0 errores, 0 avisos nuevos |
| Render EEVEE 1920x1080 vs linea base | **0 pixeles distintos de 2.073.600**, max 0/255 |
| Player `ArpgNative.blend` | **5/5 componentes** |
| Editor en modo grafico | vivo, **0 errores** |

VERIFICACION FINAL hecha DESPUES de revertir las firmas, no antes: revertir tipos no
deberia cambiar comportamiento, pero «no deberia» no es una verificacion.

### El bloque `^`, que era el miedo

`[cmdbuf addCompletedHandler:^(id<MTLCommandBuffer>){...}]` paso a lambda de C++:
metal-cpp acepta `MTL::HandlerFunction`, que es un `std::function`. **La trampa son las
capturas**: un bloque de Objective-C captura los locales por valor implicitamente y una
lambda no captura nada si no se lo dices. Capturar por referencia habria sido un
uso-despues-de-liberar, porque el manejador corre cuando la GPU termina, con el marco de
pila ya destruido. Las cuatro variables se capturan explicitamente por valor.

## 10. Cuando se borra el andamio

`mtl_objc_compat.hh` es **temporal**. Cuando el ultimo `.mm` de `gpu/metal` pase a
`.cc`, se borra su rama `__OBJC__` y las cabeceras se quedan con los tipos de
metal-cpp a secas. Mientras tanto, la rama `__OBJC__` es lo que permite que el arbol
compile despues de cada paso en vez de estar roto veinte ficheros seguidos.

---

# Parte II — GHOST y AppKit (carril GHOST, 2026-09-11)

> La parte I cubre `source/blender/gpu/metal`, donde hay binding oficial (metal-cpp).
> Aqui no lo hay: **metal-cpp no cubre AppKit**. Esta parte no reescribe nada de la
> anterior; anade lo que cambia cuando el binding no existe.

## 17. La regla que hace barata la frontera: los punteros a CLASE manglan igual

Es el hallazgo central de esta fase y **deroga la impresion, razonable pero falsa, de
que sin binding hay que neutralizar firmas**. Medido con `nm` sobre dos objetos
compilados con las flags reales del arbol:

    .mm :  @class NSView;  void Foo::take(bool, NSView *, CAMetalLayer *, int)
    .cc :   class NSView;  void Foo::take(bool, NSView *, CAMetalLayer *, int)

    los DOS exportan  __ZN3Foo4takeEbP6NSViewP12CAMetalLayeri

Un puntero a clase de Objective-C y un puntero a una clase C++ **declarada y no
definida con el mismo nombre** producen el mismo simbolo. Consecuencia practica: para
las clases de Cocoa basta con declarar el mismo nombre de las dos formas segun el modo
y **la frontera de enlazado desaparece sin perder tipado y sin castear nada**.

Es mejor de lo que se pudo hacer en `gpu/metal`, donde el choque era `id<MTLDevice>`
contra `MTL::Device *`: dos nombres DISTINTOS, y por eso alli hubo que neutralizar 8
firmas. Aqui no hizo falta neutralizar ninguna por ese motivo.

### Donde NO vale: los protocolos

    id<MTLTexture>  ->  PU21objcproto10MTLTexture11objc_object
    MTL::Texture *  ->  PN3MTL7TextureE
    id              ->  P11objc_object      (en los DOS modos)

El protocolo entra en el simbolo mangleado. Un `id<T>` en un PARAMETRO que cruce la
frontera compila en los dos lados y no enlaza. Salidas, por orden de preferencia:

1. Quitar el tipo de la firma (§11-bis), que sigue siendo lo mejor.
2. `id` pelado, que mangla igual en los dos modos.

### Y donde da igual: los tipos de RETORNO

El ABI de Itanium **no mangla el tipo de retorno** de las funciones que no son
plantilla. Por eso un alias de doble modo en el retorno es gratis: `GHOST_ObjCCompat.hh`
deja `id<MTLTexture>` para los `.mm` que quedan y `MTL::Texture *` para los `.cc`, y
ninguno de los dos lados pierde tipado. **En los parametros, jamas.**

## 18. Trampa: los nombres `MTL*` a secas ya estan ocupados en modo C++

`mtl_objc_compat.hh` aliasa cientos de nombres al espacio de metal-cpp
(`using MTLDevice = MTL::Device;`). Declarar en otra cabecera una clase opaca global con
ese mismo nombre revienta en cuanto un `.cc` incluye las dos —que es justo el caso que se
queria desbloquear— con «redefinition of 'MTLDevice' as a different kind of symbol».
Por eso `GHOST_ObjCCompat.hh` **no declara ningun `MTL*` global**: en modo C++ los
declara dentro de `namespace MTL` (donde metal-cpp los pone) y en modo Objective-C como
los `@protocol` que realmente son. De paso se retiro el apaño heredado de Blender de
escribir `@class MTLDevice;`, que era una clase FALSA con el nombre de un protocolo.

## 19. Llamar a AppKit desde C++: `GHOST_ObjCRuntime.hh`

El runtime de Objective-C es una biblioteca de C, asi que cualquier `.cc` puede mandar
cualquier mensaje. Lo que hay que escribir a mano es el reparto de `objc_msgSend`, que
en x86_64 **no es una sola funcion**:

- Agregado de **mas de 16 bytes** → `objc_msgSend_stret`, con puntero oculto al hueco
  del resultado como PRIMER argumento. `NSRect` son 32 bytes y cae aqui. `NSPoint`,
  `NSSize` y `NSRange` son 16 y NO caen: vuelven en registros con el envio normal.
- `long double` → `objc_msgSend_fpret`. **Aqui se discrepa de metal-cpp a proposito:**
  metal-cpp manda por `fpret` todo lo que sea coma flotante, y en x86_64 eso no hace
  falta para `float` ni `double` (vuelven en `xmm0`); la documentacion de Apple reserva
  `fpret` para `long double`.
- Todo lo demas → `objc_msgSend`.

Equivocarse no da error de compilacion: da basura. Comprobado empiricamente con una
`NSView` real: `convertSizeToBacking:(37,11)` devuelve `74x22`, el factor 2x de esta
pantalla.

**`@autoreleasepool` NO se traduce con `NSAutoreleasePool`.** Mirando los simbolos
indefinidos de un `.mm` original se ve que el compilador emite
`objc_autoreleasePoolPush`/`Pop`: esa pareja es la traduccion literal. El otro camino da
el mismo resultado observable por otro mecanismo, y en una migracion eso no vale.

## 20. Las constantes escritas a mano son el punto ciego, y tienen cura

Al quitar `<Foundation/Foundation.h>` hay que escribir los valores de los enumerados.
**Un 13 en vez de un 14 devuelve la carpeta equivocada sin un solo aviso.** Cura barata:
una sonda `.mm` con `static_assert` contra los simbolos reales del SDK, que falla al
compilar si Apple cambia un valor. En esta fase: **19 de 19 comprobados** (8
`NSSearchPathDirectory`, 2 mascaras de dominio, `NSASCIIStringEncoding`,
`NSOrderedAscending`, y los tamanos de `NSUInteger`, `BOOL`, `NSComparisonResult`,
`NSRect`, `NSSize`, `NSPoint` y `NSRange`, de los que depende el reparto de msgSend).

Y para los selectores, que es lo otro que el compilador deja de mirar: una sonda que
crea los objetos REALES y pregunta `respondsToSelector:` por cada uno. En
`GHOST_ContextCGL`: **54 comprobados, 0 fallos**, contra un dispositivo Metal de verdad.

Otras trampas medidas en esta fase:

- `[NSString stringWithFormat:]` es **variadico** y su ABI no se puede describir con una
  firma fija de `objc_msgSend`. Se formatea con `snprintf` y se crea la cadena ya hecha.
- `desc.colorAttachments[0]` **no es un array de C**: es
  `[[desc colorAttachments] objectAtIndexedSubscript:0]`.
- `MTLClearColor` son 32 bytes que se pasan **por valor**.
- `CAEdgeAntialiasingMask` es `unsigned int`, **no** `NSUInteger`: pasar 8 bytes donde el
  metodo espera 4 deja basura en la parte alta del registro.
- `MIN(...)` llegaba por `<Foundation/Foundation.h>` → `<sys/param.h>`. Al quitar
  Foundation desaparece.
- CoreFoundation y CoreGraphics **son C puro**: se incluyen desde un `.cc` sin arrastrar
  AppKit. Conviene usar sus simbolos de verdad (`kCFBundleVersionKey`) en vez de escribir
  la cadena a mano: si Apple los cambia, lo dice el enlazador.

## 21. Recibir mensajes: fabricar clases en tiempo de ejecucion. PROBADO

Es lo que bloquea `GHOST_WindowCocoa` y `GHOST_SystemCocoa`: ahi GHOST deja de mandar
mensajes y pasa a **recibirlos**. Se hizo un piloto y **funciona: 5 de 5 devoluciones de
llamada desde C++ puro**.

    Class c = objc_allocateClassPair(objc_getClass("NSObject"), "FlipendoWindowDelegate", 0);
    class_addMethod(c, sel_registerName("windowDidResize:"),   (IMP)imp_resize, "v@:@");
    class_addMethod(c, sel_registerName("windowShouldClose:"), (IMP)imp_close,  "c@:@");
    if (Protocol *p = objc_getProtocol("NSWindowDelegate")) class_addProtocol(c, p);
    objc_registerClassPair(c);

Lo que hay que tener presente:

- La implementacion es una funcion de C normal, pero **los dos primeros parametros
  ocultos (`self`, `_cmd`) se escriben a mano**: en Objective-C los pone el compilador.
- La **codificacion de tipo** describe el retorno y TODOS los parametros, empezando por
  `self` (`@`) y `_cmd` (`:`). `v@:@` = void con un objeto. `c@:@` = `BOOL` con un
  objeto. Para `drawRect:`, que recibe un `NSRect` por valor, la que funciona es
  **`"v@:{CGRect={CGPoint=dd}{CGSize=dd}}"`**. Escribirla mal **no da error**.
- Funciona igual para **subclasear** (`NSWindow` → `canBecomeKeyWindow`, `NSView` →
  `drawRect:`, `keyDown:`), no solo para delegados.
- `class_addProtocol` hace que `conformsToProtocol:` devuelva SI, que es lo que consulta
  parte de AppKit.

Verificado: `windowDidResize:` 1, `windowDidMove:` 1, `windowShouldClose:` 1,
`canBecomeKeyWindow` 4, `drawRect:` 1, sobre una `NSWindow` real movida y redimensionada.

## 22. Como se verifica GHOST, que NO se parece a verificar el backend de Metal

`gpu/metal` se verificaba con un render a fichero. **Eso no vale aqui**: el render en
`--background` no abre ventana, asi que no ejecuta ni una linea de `GHOST_ContextCGL`.
La verificacion de este carril es la interfaz de verdad, y tiene dos trampas que costaron
una hora:

1. **`screencapture -l<ventana>` NO incluye el contenido de una `CAMetalLayer`.** Devuelve
   la ventana vacia: solo marco, botones y titulo. Todas las comparaciones daban «0
   pixeles distintos» y parecia que el teclado no llegaba y que Blender no dibujaba. La
   captura de **PANTALLA COMPLETA** si compone la capa Metal. **Nunca verificar una
   ventana Metal con captura por ventana.**
2. **Un `kill -9` sobre Blender envenena el arranque siguiente.** macOS abre su dialogo
   «la ultima vez se cerro inesperadamente, ¿reabro las ventanas?», que es MODAL y **se
   queda con todo el teclado**. Mientras estuvo delante, ninguna tecla llego a Blender.
   **Cerrar siempre con SIGTERM o Cmd+Q.**

**La regla general que sale de aqui, y vale para cualquier carril:** ante un observable
que no cambia, lo primero es probar el arnes contra algo que se sabe que funciona. Aqui
se probaron las mismas teclas sinteticas sobre TextEdit, que cambio 19.843 pixeles. El
arnes funcionaba; lo que fallaba era el arnes.

Bateria minima para dar por bueno un cambio en GHOST:

| Comprobacion | Como |
|---|---|
| Dibuja | captura de PANTALLA COMPLETA con la vista 3D visible |
| Teclado | `T` y `N` sobre la vista cambian pixeles (medido: 68.737 y 466.996) |
| Raton | la rueda sobre la vista cambia pixeles (medido: 2.833.074) |
| Ventana | mover, redimensionar, minimizar/restaurar, pantalla completa |
| Cerrar | `Cmd+Q` termina el proceso |
| Player | `ArpgNative.blend` ata 5/5 y dibuja |
| Regresion del motor | render EEVEE 1920x1080, 0 pixeles contra la linea base |

## 23. Estado de `intern/ghost`

| Fichero | Lineas | Estado |
|---|---:|---|
| `GHOST_ContextCGL.hh` | — | **cabecera legible desde C++**; desbloquea los 5 `.mm` de Metal |
| `GHOST_ObjCCompat.hh` | nuevo | puente de doble modo para las cabeceras |
| `GHOST_ObjCRuntime.hh` | nuevo | `msg<>`, `msg_super`, `AutoreleasePool`, cadenas, retain/release |
| `GHOST_SystemPathsCocoa` | 127 | **migrado** |
| `GHOST_NDOFManagerCocoa` | 280 | **migrado** |
| `GHOST_ContextCGL` | 434 | **migrado** |
| `GHOST_WindowCocoa.mm` | 1.300 | pendiente: 3 clases, 32 metodos, 111 envios |
| `GHOST_WindowViewCocoa.hh` | 534 | pendiente: 1 clase con `NSTextInputClient`, 37 metodos |
| `GHOST_SystemCocoa.mm` | 2.199 | pendiente: 1 clase, 20 metodos, 129 envios |

Objective-C++ en `intern/ghost`: **4.340 → 3.499**. En todo el arbol: **17.362 → 16.518**.

**Los tres pendientes van juntos o no van:** la ventana, su vista y su delegado se crean
a la vez y no se pueden partir. Son 89 metodos con codificacion de tipo escrita a mano
en el camino de entrada del programa. La tecnica ya esta probada (§21); lo que queda es
volumen y cuidado.

---

# Parte III — Recibir mensajes: delegados, vistas y el final de `intern/ghost`

> La parte II cubria MANDAR mensajes desde C++. Esta cubre lo contrario: que Cocoa nos
> LLAME. Es lo que hacia falta para `GHOST_WindowCocoa`, `GHOST_WindowViewCocoa` y
> `GHOST_SystemCocoa`, los tres ultimos `.mm` de `intern/ghost`. Estan cerrados.

## 24. La regla que hace esto seguro: NO escribir las codificaciones de tipo

`class_addMethod` necesita una cadena que describe el tipo de retorno y el de todos los
parametros, empezando por los dos ocultos `self` (`@`) y `_cmd` (`:`). Escribir a mano
las 68 que hacian falta en `intern/ghost` habria sido la parte mas peligrosa de toda la
migracion: **una mal puesta no da error de compilacion ni de registro**, corrompe la
pila de argumentos dentro del manejador de eventos.

**No hay que escribirlas: el runtime ya las sabe.**

    class_getInstanceMethod(superclase, sel) -> method_getTypeEncoding   (sobreescrituras)
    protocol_getMethodDescription(protocolo, sel, requerido, instancia)  (protocolos)

`ghost_objc::ClassBuilder` las pide y **aborta con un mensaje claro si nadie conoce el
selector**. Eso convierte el fallo silencioso mas probable de esta migracion —una errata
en un nombre de selector— en un fallo ruidoso y temprano, en el registro de la clase,
antes de que se dibuje un pixel. De las 68, solo **3** se escriben a mano (metodos
propios, firma `v@:@`), y para esas el constructor comprueba ademas que no contradigan al
runtime si este las conoce.

Lo que devuelve el runtime, y que nadie deberia intentar adivinar:

    drawRect:                                v48@0:8{CGRect={CGPoint=dd}{CGSize=dd}}16
    firstRectForCharacterRange:actualRange:  {CGRect=...}40@0:8{_NSRange=QQ}16^{_NSRange=QQ}32
    draggingEntered:                         Q24@0:8@16

**Orden obligatorio:** declarar los protocolos ANTES de anadir los metodos, o la
busqueda no los encuentra.

## 25. Declarar un protocolo obliga a implementarlo ENTERO

Medido en el piloto: se declaro conformidad con `NSTextInputClient` implementando solo
dos metodos y AppKit mato la aplicacion con «unrecognized selector» en
`validAttributesForMarkedText`. Antes de declarar un protocolo hay que mirar cuantos
metodos OBLIGATORIOS tiene:

    protocol_copyMethodDescriptionList(p, /*requerido*/ true, /*instancia*/ true, &n)

De los cuatro de esta fase: `NSWindowDelegate`, `NSDraggingDestination` y
`NSApplicationDelegate` tienen **0 obligatorios**; `NSTextInputClient` los tiene todos y
se implementan los 11.

## 26. De 37 metodos, Cocoa solo llama a 24

El `@implementation` de la vista tenia 37 metodos. La mayoria eran ayudas internas
(`composing_free`, `processImeEvent`, `convertNSString`...) que estaban en Objective-C
por vecindad, no porque nadie las llamara por selector. **Solo hay que registrar lo que
Cocoa invoca**; el resto pasa a ser funciones de C++ normales. Menos superficie y ningun
selector mas que pueda estar mal escrito.

Lo mismo con los inicializadores propios (`initWithSystemCocoa:...`): se llama al
inicializador designado de la superclase —cuya codificacion conoce el runtime— y los
punteros se escriben desde C++ con `ivar_set`. Dos metodos menos que registrar.

**Y una ventaja que no se esperaba:** fabricar la clase en ejecucion elimina el apaño de
la herencia multiple. El original incluia `GHOST_WindowViewCocoa.hh` DOS VECES con macros
distintas para generar `CocoaOpenGLView : NSOpenGLView` y `CocoaMetalView : NSView`,
porque Objective-C no tiene herencia multiple. Ahora las MISMAS funciones se registran en
dos clases con superclase distinta: ni macros, ni doble inclusion, ni codigo duplicado.

## 27. Bloques (`^`) desde C++ ESTANDAR, sin `-fblocks`

Hay APIs que solo aceptan un bloque. En `intern/ghost` habia exactamente una:
`NSColorSampler showSamplerWithSelectionHandler:`.

Clang acepta bloques en C++ con `-fblocks`, pero eso es una extension. **No hace falta:**
un bloque es una struct con una disposicion publicada (Block ABI de Clang) y se puede
construir a mano. `ghost_objc::Block` lo hace en ~40 lineas.

Probado ANTES de usarlo, que es la parte que importa: se construye, se invoca en la pila,
**sobrevive a `_Block_copy` al monticulo** —lo que hace toda API que guarde el bloque— y
Objective-C lo ejecuta al pasarlo a un metodo de verdad.

Limitacion deliberada: **captura UN SOLO PUNTERO**. Con una captura POD no hacen falta
las ayudas de copia y destruccion (`BLOCK_HAS_COPY_DISPOSE`), que es donde estan las
complicaciones. Si hay que capturar mas, se mete todo en una struct y se captura su
direccion. Eso ademas sustituye a `__block`, que es otra extension.

Dos cosas mas que ahorran bloques:
- Muchas APIs de `dispatch` tienen variante con PUNTERO A FUNCION: `dispatch_after_f`,
  `dispatch_async_f`. Usarlas evita el bloque entero.
- **Trampa de propiedad:** el compilador retiene las capturas de objeto de un bloque al
  copiarlo. Fabricandolo a mano, NO. Si el objeto se usa despues de que acabe la llamada
  (aqui, 0,1 s despues), hay que retenerlo a mano o es un uso despues de liberar.

## 28. Un selector con un ESPACIO dentro compila y mata la aplicacion

Caso real de esta fase. Un selector muy largo partido en dos lineas dentro de una macro
que lo estringiza:

    GHOST_SEL(initWithBitmapDataPlanes:pixelsWide:...:
                                      hasAlpha:...)

La operacion `#` del preprocesador **convierte la secuencia de espacios en UN espacio**,
asi que se registra un selector que no existe. No da error; mata la aplicacion la primera
vez que se usa. Un selector largo va SIEMPRE como literal de una sola pieza (los
literales adyacentes de C++ se concatenan sin espacio y eso si vale).

## 29. El escaner de selectores: la comprobacion que sustituye al compilador

Lo que el compilador dejo de comprobar se recupera con un programa de 40 lineas: extrae
del codigo fuente TODOS los selectores usados y, contra las **26.293 clases** cargadas en
un proceso con AppKit, Metal, QuartzCore y Carbon, comprueba que al menos una los
implemente. Un nombre con una errata no lo implementa nadie.

Dos detalles sin los que da falsos positivos:
- hay que buscar tambien en TODOS los protocolos (`objc_copyProtocolList`): los metodos
  opcionales de `NSWindowDelegate` no los implementa ninguna clase hasta que alguien lo
  hace;
- los metodos PROPIOS (los que registramos nosotros) no los conoce nadie por definicion,
  y hay que sacarlos de la lista a mano.

Resultado en `intern/ghost`: **307 selectores comprobados, 0 con espacio, 306
implementados**, y el unico restante es nuestro.

## 30. Traducir un fichero de 2.000 lineas: no reescribirlo

En `GHOST_SystemCocoa.mm` (2.199 lineas) la forma de equivocarse no es traducir mal un
envio: es **transcribir mal las 1.400 lineas que no habia que tocar**. Asi que se
tradujeron solo las construcciones de Objective-C y el resto se dejo literal, y eso se
midio:

    las 307 lineas de `convertButton` + `convertKey`   IDENTICAS byte a byte
    del resto del fichero, 1.202 de 1.668 lineas       LITERALES (72%)

Se declaran `NSPoint`, `NSSize`, `NSRect`, `unichar` y `NSTimeInterval` como alias de sus
equivalentes de CoreGraphics (en 64 bits SON los mismos tipos, y hay `static_assert` que
lo comprueba) para no tener que tocar el codigo heredado.

Los otros dos idiomas que quedaban:
- **Enumeracion rapida** (`for (x in coleccion)`): sobre un NSArray, recorrido por indice
  con `count` y `objectAtIndex:`. Exactamente equivalente.
- **Literales**: `@[a]` es `[NSArray arrayWithObjects:a, nil]`; `@YES` es
  `[NSNumber numberWithBool:YES]`; y **ojo con el diccionario**: `@{k : v}` es
  `[NSDictionary dictionaryWithObjectsAndKeys:v, k, nil]`, o sea VALOR primero. Invertirlo
  compila y da un diccionario al reves.

## 31. Como se verifica todo esto

A las comprobaciones de la parte II se anaden dos que resultaron decisivas:

- **El menu de la aplicacion, leido por accesibilidad.** Verifica de una vez toda la
  traduccion del `NSMenu` (11 entradas con sus `@selector`), que de otro modo solo se ve
  mirando la pantalla: **37 entradas**, con las 5 propias de UPBGE y las 4 de Window.
- **Las cifras de pixeles como serie, no como valor suelto.** Medir `T`, `N` y la rueda
  ANTES de migrar y repetirlo en cada tanda. La rueda dio **2.833.074 pixeles en las
  cuatro medidas**, al pixel. Una cifra suelta no dice nada; la misma cifra cuatro veces
  seguidas sobre codigo distinto dice que el camino de entrada no ha cambiado.

Y la sonda de `static_assert` **cazo un error real** en esta fase:
`NSBitmapFormatFloatingPointSamples` no es `1 << 3`, es `1 << 2`. Escrito mal, al pegar
una imagen del portapapeles se habrian aceptado bitmaps de muestras en coma flotante que
hay que rechazar. 74 constantes y tipos comprobados, 1 fallo encontrado.

## 32. Estado final

**`intern/ghost` no tiene una sola linea de Objective-C++.** En todo el arbol:

| | Al empezar | Ahora |
|---|---:|---:|
| Objective-C++ total | 30.536 | **5.246** |
| `gpu/metal` | 20.950 | **0** |
| `intern/ghost` | 4.340 | **0** |
| `intern/cycles` (APAGADO, 0 en build.ninja) | 5.060 | 5.060 |
| `blendthumb` (extension del Finder, no el motor) | 186 | 186 |

Lo que queda no lo compila el motor: 5.060 lineas de Cycles, que esta apagado
(comprobado fichero a fichero en `build.ninja`), y 186 de la extension de miniaturas del
Finder, que es un bundle aparte.

**Recomendacion para Cycles:** migrarlo hoy no quita ni una linea del binario y, peor, no
habria forma de verificarlo, que es justo lo que dijo el carril de Metal en la parte I y
sigue siendo cierto. Se migra cuando (y si) se encienda Cycles. `blendthumb` si es
cerrable y es pequeno: 186 lineas, sin delegados, y con toda la infraestructura de este
documento ya escrita.

## 33. Coda: la extension del Finder, y el caso de la clase que instancia EL SISTEMA

`blendthumb/thumbnail_provider.mm` era el ultimo Objective-C++ que el arbol compilaba.
Tiene una dificultad que no tenia ningun otro fichero y que conviene conocer, porque
volvera a aparecer en cualquier plugin o extension:

**La clase no la instancia nuestro codigo, la instancia macOS.** El `Info.plist` del
bundle dice `NSExtensionPrincipalClass = ThumbnailProvider` y el sistema la busca **por
nombre antes de que corra una sola linea nuestra**. Una clase fabricada dentro de una
funcion llegaria tarde: el bundle cargaria y no encontraria su clase principal.

La salida es registrarla en un **constructor de carga**:

    __attribute__((constructor)) void register_thumbnail_provider_class() { ... }

que dyld ejecuta al inicializar la imagen, antes del punto de entrada del bundle
(aqui `-e _NSExtensionMain`). Regla general: **si quien instancia la clase es el sistema,
el registro no puede ser perezoso**.

Los otros dos detalles de este fichero:

- Un bloque que CREAMOS (`currentContextDrawingBlock:`) y otro que RECIBIMOS (el
  manejador de finalizacion) y hay que LLAMAR. Llamar un bloque desde C++ es trivial: se
  invoca su puntero `invoke` pasandole el propio bloque como primer argumento.
- El bloque que creamos se ejecuta DESPUES de que termine la funcion que lo creo, asi que
  su captura no puede apuntar a la pila: va a una struct del monticulo que el propio
  bloque libera al final.

### La verificacion, que aqui si se puede hacer de verdad

`qlmanage -t -x fichero.blend` dispara la extension igual que el Finder, y produce un PNG
comparable. Antes de migrar hubo que descartar un falso negativo que habria hecho creer
que la extension estaba rota: con `ArpgNative.blend` decia «No thumbnail created», pero
**el motivo era que ese fichero no lleva miniatura embebida** (sin bloque `TEST` en la
cabecera), no que la extension fallara. Buscando entre 235 ficheros aparecieron tres que
si la llevan.

Linea base congelada con el binario anterior y comprobada contra si misma tres veces.
Resultado tras migrar: **0 pixeles distintos de 16.384**, tres veces seguidas, y otros dos
`.blend` generan su miniatura. Eso prueba la cadena entera: el sistema encontro la clase
fabricada en la carga, llamo al metodo, QuickLook ejecuto el bloque de dibujo y el
manejador de finalizacion se invoco bien.

### Estado, ahora si, final

**CERO Objective-C++ compilado en Flipendo.** Lo unico que queda en el arbol son las
5.060 lineas de `intern/cycles`, que tienen **0 referencias en `build.ninja`** porque
Cycles esta apagado. Migrarlas hoy no quitaria ni una linea del binario y, peor, no
habria forma de verificarlas: no se puede comprobar lo que no se compila. Se migran
cuando (y si) se encienda Cycles, y para entonces todo lo que hace falta esta escrito en
este documento.
