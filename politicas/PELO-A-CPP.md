# El pelo, a C++ — plan de las cuatro fases y lo medido en la primera

> Carril PELO, 2026-09-12. Qué había, qué se midió, qué se arregló, qué queda.

## 0. Lo que Jesús quiere

Melenas de muchos cabellos que se muevan con **inercia y viento al caminar**, con
físicas casi reales, y también **pelo corto realista**. Eso no es una tarea: son
cuatro, y hasta que no está la primera las otras tres no se pueden ni probar.

| Fase | Qué es | Estado |
|---|---|---|
| 1 | Que el pelo **llegue al juego**: que el objeto exista para el motor | **hecha** (este documento) |
| 2 | **Modelo de sombreado** de pelo (reflejo primario y secundario, transmisión, tinte) | sin empezar |
| 3 | **Simulación de guías** en *compute* + interpolación de los cabellos en GPU | sin empezar, diseño fijado (§6) |
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

## 6. Fase 3: guías explícitas e interpolación en GPU (decisión de Jesús, 2026-09-12)

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

## 7. Por dónde seguir

1. **Fase 2, sombreado.** El camino de EEVEE para curvas ya está enchufado en el
   Player: existen `eevee_geom_curves_vert.glsl` y `eevee_attributes_curves_lib.glsl`,
   y el Player dibuja por `DRW_shgroup_*`. Lo que falta es el modelo (reflejo primario
   y secundario, transmisión) y poder pedirlo desde el material.
2. **Fase 3**, con el diseño del §6. Necesita un objeto de juego propio para el pelo
   —una clase C++ derivada de `KX_GameObject`, no un `KX_EmptyObject`— donde vivan las
   guías, el estado de la simulación y el enganche al *compute*. El `case` del §2 es
   el sitio exacto donde se cambiará esa línea.
3. **Fase 4**, niveles de detalle: el motor ya tiene `KX_LodManager` y
   `AddObjToLodObjList`, hoy sólo para `OB_MESH`.
4. Y antes que nada, cerrar los dos huecos del §5: la escena con pelo **pegado a la
   superficie**, y la bandera `--fl-game-start` para poder medir el motor empotrado.
