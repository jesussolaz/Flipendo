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

## 10. Cuando se borra el andamio

`mtl_objc_compat.hh` es **temporal**. Cuando el ultimo `.mm` de `gpu/metal` pase a
`.cc`, se borra su rama `__OBJC__` y las cabeceras se quedan con los tipos de
metal-cpp a secas. Mientras tanto, la rama `__OBJC__` es lo que permite que el arbol
compile despues de cada paso en vez de estar roto veinte ficheros seguidos.
