# El pelo, a C++ — plan de las cuatro fases, y lo medido en las dos primeras

> Carril PELO, 2026-09-12. Qué había, qué se midió, qué se arregló, qué queda.
> Fase 1 (que el pelo llegue al juego): §1-§5. Fase 2 (el sombreado): §6.

## 0. Lo que Jesús quiere

Melenas de muchos cabellos que se muevan con **inercia y viento al caminar**, con
físicas casi reales, y también **pelo corto realista**. Eso no es una tarea: son
cuatro, y hasta que no está la primera las otras tres no se pueden ni probar.

| Fase | Qué es | Estado |
|---|---|---|
| 1 | Que el pelo **llegue al juego**: que el objeto exista para el motor | **hecha** (este documento) |
| 2 | **Modelo de sombreado** de pelo (reflejo primario y secundario, transmisión, tinte) | **hecha** (§6) |
| 3 | **Simulación de guías** en *compute* + interpolación de los cabellos en GPU | sin empezar, diseño fijado (§7) |
| 4 | **Autosombreado** y **niveles de detalle** | sin empezar |

---

## 1. Lo que se creía y lo que se midió

El encargo partía de tres cosas. Dos eran falsas, y se dicen aquí porque
equivocarse en la causa cuesta más que no saberla.

| Lo que se creía | Lo que dice la medición |
|---|---|
| «El objeto de pelo no se convierte, así que al pulsar P **desaparece**» | El objeto **no se convierte** (cierto) pero **sí se dibuja** (falso que desaparezca). Captura del Player adjunta en `informes/PELO-1.md`: la melena de 500 mechones sale pintada con el binario **sin** el arreglo |
| «Las curvas heredadas están dentro de `#ifdef WITH_PYTHON`, así que sin intérprete no llegan» | El `#ifdef` sólo tapa la búsqueda del *proxy* de Python. `gameobj` nace a `nullptr` y el `if (!gameobj)` de después crea el objeto nativo **pase lo que pase**. Las curvas heredadas ya funcionaban sin intérprete |
| «El `case OB_CURVES_LEGACY` está bajo `#ifdef THREADED_DAG_WORKAROUND`» | Cierto, y era **una condición falsa**: el macro se `#define`-ía siempre, en ese mismo fichero, doce líneas antes. No encendía ni apagaba nada |

### Por qué se dibuja algo que el motor no tiene

Porque **son dos listas distintas**:

- El motor tiene la suya: `KX_Scene::GetObjectList()`, poblada por
  `BL_ConvertBlenderObjects` a partir del `switch (ob->type)` de
  `BL_gameobject_from_blenderobject` (`source/gameengine/Converter/BL_DataConversion.cpp`).
- **EEVEE no mira esa lista.** El Player no tiene rasterizador propio: delega en el
  gestor de dibujo de Blender (`DRW_render.hh` en `KX_KetsjiEngine.cpp`), que recorre
  el **depsgraph** de la escena de Blender. Un objeto que el convertidor se salta
  sigue en el depsgraph, y se pinta.

De ahí el síntoma engañoso: **parece que está y no está**. Se ve, pero el juego no
puede tocarlo — no se le puede cambiar la visibilidad, ni emparentarlo a un hueso, ni
colgarle un componente, ni matarlo, ni meterlo en un nivel de detalle. Y peor para lo
que viene: **no hay dónde colgar la simulación de la fase 3**.

Incluso el movimiento engaña. Con la melena emparentada a una cabeza con física, la
cabeza cae y **el pelo la sigue** aun sin convertir: lo arrastra el emparentamiento
del depsgraph, no el motor. Medido, con captura. Lo que cambia al convertirlo es
**quién manda**: a partir del arreglo la posición del pelo sale del grafo de escena
del motor (`KX_GameObject::TagForTransformUpdate`), que es el único sitio desde el que
una simulación va a poder escribirla.

---

## 2. El arreglo de la fase 1

`source/gameengine/Converter/BL_DataConversion.cpp`. Un `case` para los cuatro tipos
de geometría que el `switch` no contemplaba, **sin `#ifdef WITH_PYTHON`**:

```cpp
    case OB_CURVES_LEGACY:
    case OB_CURVES:
    case OB_POINTCLOUD:
    case OB_VOLUME: {
      ...
      if (!gameobj) {
        gameobj = new KX_EmptyObject();
      }
      break;
    }
```

`KX_EmptyObject` es lo mismo que ya usaban `OB_MBALL`, `OB_SURF` y `OB_GREASE_PENCIL`:
un objeto de juego con transformación y sin malla propia. La geometría la sigue
pintando EEVEE; lo que se añade es el dueño.

Nativo **a propósito**: el pelo es código nuevo y no nace atado al intérprete. El
*proxy* de Python (`custom_object`) no se extiende a estos tipos; cuando el pelo
necesite una clase de objeto de juego propia (fase 3) será C++.

`OB_POINTCLOUD` y `OB_VOLUME` entran en el mismo `case` porque les faltaba
exactamente lo mismo (`OB_TYPE_IS_GEOMETRY` en `DNA_object_types.h` los agrupa con
`OB_CURVES`). **No se han verificado con una escena**: van declarados, no dados por
buenos.

---

## 3. Cómo se verifica el pelo (y por qué hacen falta DOS evidencias)

Una captura dice si algo **se pinta**. No dice si el motor **lo tiene**. En el pelo
esas dos cosas se separan de verdad, así que la evidencia del carril son dos:

### 3.1 `FL_GAME_DUMP` — la lista de objetos del juego

`source/gameengine/Flipendo/FL_GameDumpProbe.{hpp,cpp}`. Sonda nativa, se compila
siempre, no hace nada si no está su variable de entorno:

```
FL_GAME_DUMP=<fichero> FL_GAME_DUMP_FRAME=90 FL_UI_EXIT=1 \
  Blenderplayer -w 800 600 100 100 tests/flipendo/pelo/escena-pelo.blend
```

Una línea por objeto, ordenadas por nombre para poder comparar con `diff`, con el tipo
del objeto de Blender del que salió y la posición **cuantizada a dos decimales** (se
vuelca con la física corriendo y la última cifra de un float acumulado no se
reproduce — lección de las 03:50 del REGLAMENTO).

### 3.2 `--fl-compare-png` — las capturas, con su suelo de ruido

`source/blender/editors/flipendo/fl_image_compare.cc`. Píxeles distintos, porcentaje,
delta medio y máximo, y **la caja donde están las diferencias** — sin la caja,
«cambian 40.000 píxeles» no distingue «salió el pelo» de «cambió el ruido de toda la
pantalla».

> **La regla que trae puesta, y que es la más importante de este documento:**
> dos ejecuciones del **mismo** binario sobre la **misma** escena **no dan el mismo
> PNG**. EEVEE acumula muestras y el ruido temporal cambia entre arranques. Medido
> aquí: **14,43 % de los píxeles** difieren entre dos ejecuciones idénticas, con delta
> máximo 91. Una diferencia sólo significa algo **por encima de ese suelo**, así que
> se mide el suelo **antes** de afirmar nada. Es la técnica que estrenó el carril de
> VideoTexture comparando el render contra el ruido del propio motor consigo mismo.

Y en este caso el suelo se comió el resultado, que es justo para lo que sirve: antes
contra después dio **9,16 %**, *menos* que el suelo. Conclusión honesta: **el arreglo
no cambia un solo píxel de esta escena**, porque el pelo ya se pintaba. Lo que cambia
lo dice el otro volcado, 4 objetos → 5.

### 3.3 La escena, construida con el binario

`--fl-make-hair-scene <fichero>` (`source/blender/editors/flipendo/fl_hair_scene.cc`),
mismo patrón que `--fl-make-ui-scene`: un `.blend` en el repo es un asset, pero un
asset que nadie sabe regenerar es un dato opaco. La escena sale del código, es
reproducible y no depende del intérprete.

Lleva suelo con colisión, una cabeza (esfera UV de radio 0,95) con física de cuerpo
rígido que cae desde z=2, y una melena `Curves` de **500 mechones de 8 puntos**
emparentada a la cabeza. El pelo lo pone `OBJECT_OT_curves_random_add`, que es el
único operador del árbol que crea un `Curves` **con geometría dentro**
(`OBJECT_OT_curves_empty_hair_add` lo crea vacío, y un objeto vacío no prueba que se
dibuje nada).

**Determinismo comprobado**: tres generaciones seguidas dan el mismo volcado
(`--fl-dump-hair`), byte a byte. El generador de `primitive_random_sphere` se
construye con la semilla por defecto, así que los 500 mechones salen iguales siempre.

### 3.4 El arnés, probado al revés

`politicas/ARNES-A-PRUEBA.md` es tajante: **un comprobador que no puede fallar no es un
comprobador**. Las tres banderas nuevas se han ejecutado en las condiciones malas y
salen distintas de 0; y en la buena, verde.

| Condición | Bandera | Salida |
|---|---|---:|
| Sin argumento | `--fl-compare-png` | **1** |
| Con una sola captura de las dos | `--fl-compare-png` | **1** |
| Con una captura que no existe | `--fl-compare-png` | **1** |
| Con dos capturas de **tamaños distintos** (800×600 contra 400×300) | `--fl-compare-png` | **1** |
| Con las dos capturas de verdad | `--fl-compare-png` | 0 |
| Sin argumento | `--fl-dump-hair` | **1** |
| Sobre una escena **sin ningún `Curves`** (el cubo de fábrica) | `--fl-dump-hair` | **1** |
| Sobre la escena de pelo | `--fl-dump-hair` | 0 |
| Sin argumento | `--fl-make-hair-scene` | **1** |

La cuarta fila es la que más importa y es propia de este carril: comparar dos capturas
de tamaños distintos es la forma silenciosa de «no pudo comparar» que más fácil sería
dejar pasar recortando a la menor. No se recorta: se falla y se dice.

---

## 4. Trampas pagadas en la fase 1

1. **La captura de pantalla se encola.** `RAS_ICanvas::MakeScreenShot` sólo apunta la
   petición; el motor la vuelca en `EndFrame`. Pedirla y salir en el mismo tic la deja
   sin escribir **mientras el log dice que se ha capturado**. Heredada de
   `politicas/UI-JUEGO-NATIVA.md`, y sigue siendo cierta.
2. **El `.blend` manda sobre la cámara.** El Player dibuja a través del `View3D`
   guardado en el fichero: si su vista no está en `RV3D_CAMOB`, no usa la cámara de la
   escena. En `--background` no hay ventana, así que el constructor recorre las
   pantallas de `bmain` y toca el `RegionView3D` a mano (10 vistas en la plantilla de
   fábrica).
3. **Una condición falsa es peor que ninguna.** `#define THREADED_DAG_WORKAROUND` doce
   líneas antes del `#ifdef` que lo consulta hacía creer que las curvas se convertían
   «sólo a veces». Se ha quitado. Si un `#ifdef` no puede estar apagado, no es un
   `#ifdef`: es ruido que despista al siguiente.
4. **El pelo nace en el radio 1.** `primitive_random_sphere` planta las raíces
   exactamente sobre la esfera unidad del origen del objeto. Por eso la cabeza es de
   radio 0,95 y el pelo va en la misma posición que ella: con la cabeza más grande, las
   raíces quedan dentro de la malla y la melena se ve a trozos.
5. **Emparentar a mano sale distinto.** `parentinv` lo calcula `OBJECT_OT_parent_set`;
   ponerlo a mano da otro resultado que el camino del usuario. Va por operador, como
   todo lo demás de la escena.

---

## 5. Lo que queda de la fase 1

- **`OB_POINTCLOUD` y `OB_VOLUME` no se han probado con una escena.** Comparten el
  `case` con `OB_CURVES`, así que el camino es el mismo, pero eso es una lectura, no
  una medida.
- **No está comprobado el motor empotrado del editor (pulsar P).** `VIEW3D_OT_game_start`
  necesita un `View3D` real y no hay forma de lanzarlo sin intérprete desde la línea de
  órdenes; haría falta una bandera `--fl-game-start` que llame al operador con el
  contexto apañado. La verificación de este carril está hecha con el **Blenderplayer de
  las dos configuraciones**, que es el que se envía.
- **El pelo PEGADO a una superficie: probado a medias, y hay que decir hasta dónde.**
  `--fl-make-hair-scene <fichero> PEGADA` monta la segunda escena
  (`tests/flipendo/pelo/escena-pelo-pegada.blend`) con lo mismo que pone
  `OBJECT_OT_curves_empty_hair_add`: objeto de superficie, mapa UV de enganche, el nodo
  *Deform Curves on Surface* (por la propia función del árbol,
  `ed::curves::ensure_surface_deformation_node_exists`) y la marca
  `OB_MODIFIER_FLAG_ADD_REST_POSITION` en la cabeza.

  Medido: **el pelo se sigue viendo** y el motor lo sigue teniendo (5 objetos, la
  `Melena` dentro). Contra la escena de pelo suelto, la captura difiere en **8,55 %**
  de los píxeles — otra vez **por debajo del 14,43 % del suelo de ruido**, o sea la
  misma imagen.

  **Lo que ESO no prueba**, y es importante: los mechones de esta escena salen de
  `primitive_random_sphere`, no de esculpirlos sobre la cabeza, así que **no llevan el
  atributo `surface_uv_coordinate`** con el que el nodo ancla cada raíz a un punto de
  la malla. Sin ancla, el nodo de deformación es la identidad. Queda probado que la
  cadena de modificadores **no rompe** el pelo en el juego; **no** queda probado que un
  pelo de verdad anclado a la superficie siga a la malla cuando el motor toma el
  control de ella (`BL_ConvertMesh`). Para eso hace falta una escena con pelo esculpido
  —o generar el atributo de anclaje— y es lo siguiente que hay que medir aquí.

---

## 6. Fase 2 — el sombreado: qué se implementó, dónde se aproxima y qué no cubre

> Carril PELO-2, 2026-09-12. Capturas y cifras: `informes/PELO-2.md` e `informes/pelo-2/`.

### 6.1 El punto de partida, con cita

EEVEE **no tenía sombreado de pelo**. No es una opinión: está escrito por los propios
desarrolladores de Blender en `source/blender/gpu/shaders/material/gpu_shader_material_hair.glsl`,
dentro de un `#if 0`:

> NOTE(fclem): This is the way it should be. But we don't have proper implementation
> of the hair closure yet. For now fall back to a simpler diffuse surface so that we
> have at least a color feedback.

Los **dos** nodos de pelo (`Hair BSDF` y `Principled Hair BSDF`) caían en el mismo
`#else`: un `ClosureDiffuse` con el color del zócalo. Y
`closure_eval(ClosureHair)` en `eevee_nodetree_lib.glsl` era literalmente
`/* TODO */ return Closure(0);`. Una melena se pintaba como **superficie mate**: sin
banda de brillo recorriendo el mechón, sin luz atravesándolo por detrás, sin el
segundo reflejo teñido. Eso es exactamente lo que hace que el pelo en tiempo real
parezca plástico.

### 6.2 El modelo: Marschner de campo lejano, en tres cierres

| Bin | Cierre | Lóbulo | Qué es |
|---|---|---|---|
| 0 | `ClosureTranslucent` | **TT** | la luz que **atraviesa** la fibra. Teñida por una travesía del pigmento |
| 1 | `CLOSURE_BSDF_HAIR_REFLECTION_ID` | **R** | reflejo primario en la cutícula. **Acromático** (nunca entra en la fibra) |
| 2 | `CLOSURE_BSDF_HAIR_REFLECTION_ID` | **TRT** | segundo reflejo. **Teñido**, más ancho, desplazado al otro lado |

- **Longitudinal `M_p`**: gaussiana de **área unidad** sobre el ángulo `theta_h`,
  desplazada por la inclinación de la cutícula (`-alpha` para R, `+3·alpha/2` para
  TRT). Que los desplazamientos tengan **signos opuestos** es lo que separa los dos
  brillos en vez de superponerlos: es el rasgo que se reconoce como «pelo».
  Al ser normalizada, subir la rugosidad **baja el pico** en vez de añadir energía, y
  eso es comprobable (§6.5).
- **Azimutal `N_p`**: las formas cerradas baratas de la literatura de tiempo real
  desde Marschner 2003, en vez de resolver el cilindro de Bravais:
  `N_R = 0.25·cos(phi/2)` (ancha — la que hace que el brillo **recorra** el mechón y
  no sea un punto) y `N_TRT = exp(17·cos(phi) − 16.78)` (estrecha, hacia el
  observador: el destello de color).
- **Fresnel** de queratina (n = 1,55) con el denominador `1/cos²(theta_d)` de
  Marschner. Pesos `F` para R y `(1−F)²·F` para TRT.
- **Pigmento**: melanina/feomelanina y `sigma_a_from_reflectance` **copiados de
  Cycles** (`intern/cycles/kernel/closure/bsdf_hair_principled.h`), para que el mismo
  material signifique el mismo pelo en los dos motores. Transmitancia de una travesía
  `A = exp(−2·sigma_a)`; tinte de TT = `A`, tinte de TRT = `A³`.
- **Variación por mechón** (`Random Color`, `Random Roughness`), con las fórmulas de
  Cycles: sin ella todos los cabellos tienen el mismo color, y eso se ve.

El código: `source/blender/draw/engines/eevee/shaders/eevee_bxdf_hair_lib.glsl` (los
lóbulos) y `gpu_shader_material_hair.glsl` (el nodo, el pigmento y el reparto en
cierres). Las dos cabeceras llevan escritas sus aproximaciones.

### 6.3 Las tres decisiones de arquitectura, y su precio

**1. El cierre de pelo guarda el TANGENTE en la ranura de la normal.**
El gbuffer codifica ahí un vector unidad en octaedro y le da igual si es una normal o
una tangente. Gracias a eso el modo nuevo `GBUF_HAIR` reusa **el reparto exacto** de
`GBUF_REFLECTION` (color + una palabra de datos + una normal) y no hace falta ni una
capa nueva de textura. Donde hace falta una normal de verdad —atenuación por
encaramiento, sesgo de sombra, sondas— se reconstruye con `bxdf_hair_normal(T, V)`,
que es la normal del cilindro que mira al observador.
*Precio*: la textura de cierres es `GPU_RGB10_A2`, o sea **UNORM**. La inclinación
(con signo) va remapeada a [0,1] y el identificador de lóbulo va en los **dos bits**
del alfa, donde 0 y 1 sí son exactos.

**2. R y TRT en bins SEPARADOS.**
Un solo bin no puede llevar dos tintes (R es blanco, TRT lleva el pigmento) ni dos
anchuras. Caen en la ranura *glossy* y en la de *coat*, así que un material de pelo
pide los **mismos tres bins** que un Principled con barniz. Por eso el nodo declara
`GPU_MATFLAG_TRANSLUCENT | GLOSSY | COAT` en vez del viejo `DIFFUSE | GLOSSY`.

**3. TT va por el cierre translúcido que ya existía.**
Ese cierre ya trae puesta toda la maquinaria de «la luz viene de detrás»: su bin de
gbuffer, su término de sombra y su pasada `light_eval_transmission`. Reimplementarla
para un lóbulo habría sido duplicarla.
*Precio, dicho*: TT sale como **coseno ancho** en vez del cono estrecho hacia adelante
del modelo real. A cambio hace también de sustituto de la dispersión múltiple entre
mechones, que es lo único que impide que el pelo oscuro sea una silueta negra.

### 6.4 La trampa gorda: los lóbulos de pelo NO tienen ajuste LTC

Toda la iluminación de EEVEE pasa por `light_ltc()`, que integra el BSDF sobre la
forma de la luz con una matriz de **cosenos transformados linealmente**. Para el pelo
no existe esa matriz, y ajustarla es un trabajo aparte.

En vez de inventarla, `light_eval_single_closure` usa el LTC con un **coseno apuntado
a la luz** —que devuelve el ángulo sólido que cubre— y lo multiplica por el BSDF
evaluado en la dirección del centro de la luz. Es **el mismo truco** que
`light_eval_single` ya jugaba para `LIGHT_TRANSLUCENT_WITH_THICKNESS`.

> **Consecuencia real**: una luz de área grande suaviza el brillo por su ángulo sólido,
> pero **no lo estira** como haría una integral de área de verdad. Quien monte una
> escena con un panel grande verá el brillo más pequeño de lo que debería.

### 6.5 Cómo se verificó, y por qué aquí NO vale «reproducir byte a byte»

En este repo casi todo se verifica reproduciendo lo que hacía el Python. **Aquí no hay
nada que reproducir**: la capacidad no existía. Así que la verificación cambia de forma,
y tiene que ser igual de dura.

**El truco que hace posible el A/B con un solo binario**: el modo `PLANO` de
`--fl-make-hair-shading-scene` monta un `Diffuse BSDF` con **exactamente** el color por
defecto del zócalo `Color` del nodo de pelo, que es lo que el `#else` metía en su
`ClosureDiffuse`. No es «un material parecido»: es la reproducción del camino viejo. Sin
eso habría hecho falta reconstruir el binario anterior para cada medida.

**El suelo de ruido cambia respecto a la fase 1, y es una buena noticia.** La fase 1
midió **14,43 %** de píxeles distintos entre dos ejecuciones idénticas — pero eso era
el **visor** del Player, que acumula muestras en el tiempo. Un **render por lotes** con
muestras fijas (64) es **determinista**: tres ejecuciones del mismo binario sobre la
misma escena dan **0 píxeles distintos de 480.000 = 0,000 %**. (Los md5 de los PNG sí
cambian, por metadatos; los píxeles no.) Con suelo cero, cualquier diferencia es señal.

| Medida | Cifra |
|---|---|
| Suelo de ruido (3 ejecuciones) | **0 / 480.000 = 0,000 %** |
| A/B luz frontal (90°) | **164.187 = 34,206 %**, delta medio 31,13, máx 138 |
| A/B a contraluz (270°) | **158.792 = 33,082 %**, delta medio 36,95, máx 255 |

**Transmisión** (el punto 3 del encargo), luz detrás de la cabeza:

| | luminancia media | máx | píxeles negros |
|---|---:|---:|---:|
| ANTES (difuso) | 0,000323 | 0,004472 | **91,76 %** |
| DESPUÉS (Marschner) | **0,027525** | 1,000000 | 61,35 % |

El fotograma de antes es **negro** (máximo 0,0045 = un paso de 8 bits). El de ahora es
**85 veces más luminoso** de media. Capturas `03-` y `04-` de `informes/pelo-2/`.

**Energía contra rugosidad** (misma luz, misma escena):

| rugosidad | máx | media | píxeles brillantes |
|---:|---:|---:|---:|
| 0,05 | 0,7011 | 0,07158 | 5.517 |
| 0,30 | 0,5719 | 0,07439 | 8.631 |
| 0,60 | 0,4026 | 0,07709 | 19.469 |
| 1,00 | 0,2858 | 0,07622 | 52.974 |

El pico **baja un 59 %** mientras el conjunto brillante crece ×9,6 y la media se mueve
un 6,5 %: el lóbulo **reparte** la misma energía, no la crea. Cero píxeles saturados.

**Cordura numérica**: en EXR de coma flotante —que es donde el NaN sobrevive, porque en
un PNG de 8 bits ya se ha convertido en un número cualquiera— **nan = 0, negativos = 0**
en las tres escenas medidas.

**Banda contra mancha, y aquí va la lectura honesta.** Al girar la luz se mueven **los
dos** centroides, el del pelo y el del control difuso (el difuso **más**, porque lo que
barre es el hemisferio iluminado entero). Decir «se mueve, luego hay lóbulo» habría sido
firmar un verde falso. Lo que de verdad los separa son otras dos cifras:

- **Concentración**: al mismo umbral el pelo enciende 2.623–8.631 píxeles (0,5–1,8 % del
  cuadro) y el difuso 17.944–115.288 (3,7–24 %). Siete a trece veces más disperso.
- **Pico**: el máximo del difuso es **prácticamente constante** en todos los ángulos
  (0,1647–0,1725: puro albedo por coseno), mientras el del pelo va de 0,57 a 1,00 según
  la geometría del lóbulo. En radiancia lineal (EXR) el pico del pelo es 0,3043 contra
  0,0308 del difuso: **×9,9**.

**Las dos tuberías.** El mismo sombreado se compila **dos veces**: la diferida lo guarda
en el gbuffer y lo ilumina en otra pasada; la de adelante lo ilumina en el propio
fragmento. Probar solo una habría dejado a la mitad de los usuarios con un fallo de
compilación que nadie midió. Medido: media 0,074385 contra 0,074926 (0,7 %), pico 0,5719
contra 0,5649 (1,2 %), 0 NaN en las dos. El 17,77 % de píxeles distintos con delta medio
3,02 es la cuantización del gbuffer — y de paso demuestra que **el tangente sobrevive al
ida y vuelta** por la codificación en octaedro.

### 6.6 Lo que NO cubre este sombreado

1. **Sin campo cercano**: los destellos azimutales de una fibra suelta no se resuelven.
2. **Sin dispersión múltiple entre mechones**; el lóbulo translúcido hace de apaño.
3. **Sin sección elíptica**: `Aspect Ratio` y el modelo **Huang** se ignoran.
4. **`Coat` y `Radial Roughness`** solo entran por el pigmento, no como lóbulo propio.
5. **Sin trazado de rayos en pantalla** para el pelo: se declara rugosidad aparente 1,0
   **a propósito** para que el módulo de rayos lo salte y caiga en las sondas.
6. **Sin ajuste LTC** (§6.4): las luces de área no estiran el brillo.
7. **Una singularidad medida y dejada a la vista**: a contraluz quedan **13 píxeles de
   480.000 (0,003 %)** con radiancia 78,98 en el EXR. Es la singularidad
   `1/cos²(theta_d)` del modelo de campo lejano cuando la luz cae casi **a lo largo** del
   mechón. Está acotada por `BXDF_HAIR_MAX_EVAL` (que nunca llegó al tope) y en 8 bits
   son 39 píxeles saturados (0,01 %). Se deja medida y sin tocar: retocar el recorte sin
   volver a medir el modelo entero sería cambiarlo a ciegas.

### 6.7 El arnés que queda para las fases 3 y 4

```
--fl-make-hair-shading-scene <fichero> <PELO|PLANO> [grados] [rugosidad] [ADELANTE]
--fl-stats-png <imagen> [cuantil]
```

La escena de sombreado **no tiene física**, tiene el **mundo negro** y **una sola luz de
sol** cuya dirección es un argumento (90 = de frente, 0 = cenital, 270 = a contraluz).
`--fl-stats-png` da luminancia media/mínima/máxima, cuántos píxeles son NaN, negativos,
negros o saturados, y el centroide del brillo; lee **OpenEXR**, que es donde el NaN se
puede contar de verdad.

**Trampa nueva, y va a doler en la fase 3**: si la vista 3D guardada en el `.blend` no
está en modo **RENDERIZADO**, el editor y el Player dibujan con **Workbench** y **no
pasan por EEVEE**. La escena se ve perfectamente y no prueba ni una línea del sombreado.
Es la hermana de la trampa 2 del §4 (el `.blend` manda sobre la cámara). El generador
fuerza `v3d->shading.type = OB_RENDER` en todas las vistas.

**Arnés probado al revés** (`politicas/ARNES-A-PRUEBA.md`), diez condiciones: sin
argumentos, sin modo, con un modo inventado, sobre un fichero que no existe y con dos
imágenes de tamaños distintos → **rc=1** las cinco; los cinco casos buenos → rc=0.

---

## 7. Fase 3: guías explícitas e interpolación en GPU (decisión de Jesús, 2026-09-12)

**Decidido antes de empezar la fase, para que no se replantee desde cero cuando toque.**

Se simulan **100-200 guías** con física de verdad —restricciones de longitud, rigidez,
amortiguación, colisión contra unas cápsulas atadas a cabeza, cuello y hombros, más
inercia del movimiento y viento— y los cientos o miles de cabellos visibles se
**interpolan de esas guías en la GPU** cada fotograma, por cercanía en el cuero
cabelludo.

**1. No es un atajo, es como se hace.** Blender ya tiene el concepto:
`node_geo_interpolate_curves.cc` habla literalmente de «curvas base entre las que se
interpolan las nuevas», y el sistema de pelo antiguo tenía `child_percent` y
`PART_CHILD_PARTICLES` — pelos padre y pelos hijo. TressFX y Unreal hacen lo mismo.
Nadie simula veinte mil cabellos, ni en cine.

**2. La trampa de interpolar, y es requisito de la fase, no un detalle.** «Los cabellos
cercanos se mueven parecido» es cierto casi siempre, y el pelo se ve vivo justo en el
*casi*. Interpolar demasiado limpio da aspecto de **casco sólido**, que es el fallo
clásico. Hace falta variación controlada encima —separación en mechones, pelos
sueltos— o parecerá una peluca.

**3. Anotado como posible mejora futura, no como plan.** Precalcular la dinámica en
subespacio: análisis de componentes principales sobre simulaciones fuera de línea, o
una red que aprenda «movimiento de cabeza → forma del pelo». Es real y más barato en
ejecución, pero **sólo responde bien a movimientos parecidos a los que aprendió**. Se
monta encima de las guías si algún día hace falta, sin tirar nada de lo anterior.

---

## 8. Por dónde seguir

1. ~~**Fase 2, sombreado.**~~ **Hecha**: §6. Lo que queda de ella está en §6.6, y lo
   más goloso es el **ajuste LTC** de los lóbulos, para que una luz de área estire el
   brillo en vez de solo suavizarlo.
2. **Fase 3**, con el diseño del §7. Necesita un objeto de juego propio para el pelo
   —una clase C++ derivada de `KX_GameObject`, no un `KX_EmptyObject`— donde vivan las
   guías, el estado de la simulación y el enganche al *compute*. El `case` del §2 es
   el sitio exacto donde se cambiará esa línea.
3. **Fase 4**, niveles de detalle: el motor ya tiene `KX_LodManager` y
   `AddObjToLodObjList`, hoy sólo para `OB_MESH`.
4. Y antes que nada, cerrar los dos huecos del §5: la escena con pelo **pegado a la
   superficie**, y la bandera `--fl-game-start` para poder medir el motor empotrado.
