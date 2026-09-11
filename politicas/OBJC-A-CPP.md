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
| mtl_state | 715 | 2 | traducido; **bloqueado** por mtl_command_buffer |
| mtl_shader_interface | 752 | 5 | **migrado** (firma neutral `id`) |
| mtl_shader_generator | 3.413 | 6 | traducido; **bloqueado** por mtl_shader |
| mtl_immediate | 348 | 8 | pendiente |
| mtl_index_buffer | 566 | 12 | traducido; **bloqueado** por mtl_memory |
| mtl_texture_util | 852 | 12 | pendiente |
| mtl_batch | 930 | 29 | pendiente |
| mtl_debug | 184 | 29 | pendiente |
| mtl_memory | 1.121 | 32 | **desbloquea a mtl_index_buffer** |
| mtl_storage_buffer | 526 | 33 | pendiente |
| mtl_vertex_buffer | 363 | 33 | pendiente |
| mtl_shader | 1.602 | 52 | **desbloquea a mtl_shader_generator** |
| mtl_framebuffer | 2.007 | 136 | pendiente |
| mtl_texture | 2.685 | 357 | pendiente |
| mtl_backend | 614 | 35.734 | quitar `<Cocoa/Cocoa.h>` primero |
| mtl_command_buffer | 1.089 | 35.851 | GHOST |
| mtl_context | 2.749 | 36.019 | GHOST — el tapon |

**Dato que reordena el trabajo:** `mtl_shader_generator.mm`, 3.413 lineas (el fichero
mas grande del backend), da **6 errores**. Ya esta traducido y verificado como C++;
solo espera a que `mtl_shader.mm` (52) lo desbloquee. Los dos juntos son 5.015 lineas.

### Orden recomendado (revisado, con el grafo de llamadas)

1. `mtl_memory` + `mtl_index_buffer` juntos (definidor + llamador).
2. `mtl_shader` + `mtl_shader_generator` juntos. 5.015 lineas de una sentada.
   Ojo: al migrar `mtl_shader` hay que resolver de verdad el `@""` de la trampa 5.
3. Sueltos sin dependencias cruzadas: `mtl_immediate`, `mtl_texture_util`, `mtl_debug`,
   `mtl_batch`, `mtl_storage_buffer`, `mtl_vertex_buffer`.
4. `mtl_backend` tras quitarle `<Cocoa/Cocoa.h>`.
5. `mtl_framebuffer` (136) y `mtl_texture` (357).
6. `mtl_command_buffer` y `mtl_context`: **solo despues de la fase de GHOST**.

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

## 10. Cuando se borra el andamio

`mtl_objc_compat.hh` es **temporal**. Cuando el ultimo `.mm` de `gpu/metal` pase a
`.cc`, se borra su rama `__OBJC__` y las cabeceras se quedan con los tipos de
metal-cpp a secas. Mientras tanto, la rama `__OBJC__` es lo que permite que el arbol
compile despues de cada paso en vez de estar roto veinte ficheros seguidos.
