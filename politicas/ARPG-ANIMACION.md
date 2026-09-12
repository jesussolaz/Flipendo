# ARPG: animación del personaje, cámara orbital y verificación sin manos

> Carril CPP de la noche de ÁNIMA (2026-09-12). Código: `source/gameengine/Flipendo/`
> (`FL_ArpgComponents.cpp`, `FL_AnimaComponents.cpp`, `FL_GameDumpProbe.cpp`).
> Escena de prueba y volcados de referencia: `tests/flipendo/arpg/`.
> Informe con las capturas: `~/Flipendo/dev/noche/informes/anima-cpp.md`.

## Qué

Hasta esta noche el `PlayerController` nativo movía una cápsula y llevaba el combo,
pero el personaje no **animaba**: no había ninguna vía C++ que llamara a
`KX_GameObject::PlayAction`. Ahora:

1. **Máquina de estados de animación** en `FL_PlayerController`: Idle / Andar / Correr
   en bucle, Ataque1-3 / Salto / Golpe de un tiro, sobre el armature hijo del Player.
2. **Cámara orbital** en `FL_ThirdPersonCamera`: el ratón orbita (yaw libre, pitch
   entre -10° y 60°), con suavizado, sin invertir.
3. **Componentes de escena** `Flotar` y `Girar` (`FL_AnimaComponents.cpp`).
4. **Volcado v2** (`FL_GAME_DUMP`): `accion=<nombre>@<frame>` por objeto que anima y
   `combo=… hp=…` en el Player.
5. **`FL_ARPG_AUTOPLAY`**: pulsaciones programadas por frame, para que el Player
   demuestre solo que anda, corre y pega.

## Por qué así

- **Cero Python en el juego** (`LENGUAJE-CPP.md`): los `.blend` solo llevan
  propiedades; el motor las resuelve a componentes C++.
- **El animador no cuadra números**: los ataques duran lo que dice `HitSpec` en
  `FL_ArpgCore.hpp` (0,52 / 0,50 / 0,85 s). El componente escala la velocidad de
  reproducción para que la acción encaje **exactamente** en esa duración, así que lo
  único que importa en el `.blend` es la proporción entre frames.
- **Sin rig, nada cambia**: un Player sin hijo `fl_rig` se mueve y pega como antes.
- **Verificación con evidencia, nunca por lectura**: el volcado dice qué acción y en
  qué frame; la captura dice qué se ve. Las dos juntas.

## Convención `fl_anim_*` (la de `NOCHE-ANIMA.md`)

```
Player                 EMPTY, física CHARACTER, fl_component = "PlayerController"
 └─ Alonso_Rig         ARMATURE, hijo (a cualquier profundidad). Propiedades string:
                         fl_rig          = "1"
                         fl_anim_idle    = "Idle:1:72"      Accion:inicio:fin
                         fl_anim_walk    = "Andar:1:32"
                         fl_anim_run     = "Correr:1:24"
                         fl_anim_attack1 = "Ataque1:1:16"
                         fl_anim_attack2 = "Ataque2:1:16"
                         fl_anim_attack3 = "Ataque3:1:24"
                         fl_anim_jump    = "Salto:1:30"     opcional
                         fl_anim_hit     = "Golpe:1:14"     opcional
```

- El rig se busca en `Start()`: el propio objeto o el **primer descendiente**
  (`GetChildrenRecursive`) con `fl_rig`. Se registra por consola lo que se encontró:
  `Flipendo: PlayerController con rig 'Alonso_Rig' — idle=Idle[1..72] walk=…`.
- Una propiedad ausente = ese estado no anima. Una mal formada (`fin <= inicio`, sin
  dos `:`) se dice por consola (`CM_Error`) y se ignora.
- Escena y acciones **a 30 fps**. El motor convierte frames en segundos con
  `GetAnimFrameRate()` (el fps de la escena), no con un 30 escrito a mano.

### Estados y prioridades

| Estado | Modo | Prioridad | Blend-in | Cuándo |
|---|---|---|---|---|
| Idle | LOOP | 1 | 8 | parado |
| Andar | LOOP | 1 | 5 | moviéndose |
| Correr | LOOP | 1 | 5 | moviéndose con SHIFT izq. (velocidad ×1,7 = 11,9 m/s) |
| AtaqueN | PLAY | **0** | 3 | al arrancar/encadenar el golpe N del combo |
| Salto | PLAY | 1 | 4 | al saltar (si existe) |
| Golpe | PLAY | 0 | 4 | al recibir daño, si no está atacando (si existe) |

**En `BL_Action`, número menor = más prioridad**: una petición de prioridad 1 se rechaza
mientras una de prioridad 0 no ha terminado. Es lo que impide que el bucle de andar
pise el ataque. Además el componente **bloquea los pies** mientras el combo está vivo
(solo si hay animación de ataque; sin rig se mueve como siempre).

Velocidad de un ataque: `playback_speed = (fin - inicio) / fps / (startup + active +
recovery)`. Con la convención: Ataque1 = 15/30/0,52 = **0,9615**; Ataque2 = 15/30/0,50
= **1,0**; Ataque3 = 23/30/0,85 = **0,902**.

## Cámara orbital

- `MOUSEX/MOUSEY` respecto al centro de la ventana; sensibilidad 0,0025 rad/px (un
  barrido de 1280 px = 183°); ajustable con `fl_cam_sens`. Distancia `fl_cam_dist`
  (7 m) y altura `fl_cam_altura` (2,6 m) en la cámara.
- Suavizado `k = 1 - e^(-14·dt)` (0,21 a 60 Hz) para yaw/pitch y `1 - e^(-10·dt)`
  (0,15) para la posición.
- Solo se lee el ratón en los frames con evento (`m_values.size() > 1`) y después se
  recentra el cursor (`SetMousePosition`); en macOS el recentrado empuja él mismo un
  evento al centro, así que el siguiente desplazamiento sale limpio.
- **Con `FL_UI_EXIT` no se toca el puntero** (ni se esconde ni se recentra): `jugar`
  lo pone siempre, y si no, cada verificación de la noche secuestraría el ratón de
  quien esté usando el ordenador.

## `FL_ARPG_AUTOPLAY`

```
FL_ARPG_AUTOPLAY="w:70-150;shift:100-150;j:170;j:200;j:230;d:260-300" \
  ~/Flipendo/dev/noche/jugar escena.blend captura.png 210 volcado.txt
```

`tecla:frame` es un toque en ese frame; `tecla:desde-hasta` es mantenida (inclusive).
Teclas: `w a s d shift j space`. El frame es el **tic lógico** desde que arrancó el
componente, el mismo contador que `FL_GAME_DUMP_FRAME` y `FL_UI_SHOT_FRAME`. Se suma al
teclado real (OR). Sin la variable no existe. Tecla o frame inválidos se dicen por
consola y se ignoran.

## Volcado v2

```
# FL_GAME_DUMP v2 escena=ArpgAnim frame=210
OBJ nombre=Alonso_Rig tipoblender=ARMATURE visible=1 pos=(0.00,13.62,-0.31) accion=Ataque2@5.5
OBJ nombre=Player tipoblender=EMPTY visible=1 pos=(0.00,13.62,0.71) combo=active:1 hp=100
```

`accion=` solo si hay acción **en marcha** en la capa 0 (`GetActionManagerNoCreate` +
`!IsActionDone`: la sonda no le cuelga un gestor a nadie). `combo=`/`hp=` en el objeto
con `fl_component = "PlayerController"`. Las líneas sin animación son idénticas a v1.

## Cifras de verificación (escena `tests/flipendo/arpg/escena-arpg-anim.blend`)

Guion `w:70-150;shift:100-150;j:170;j:200;j:230;d:260-300`, tic 60 Hz, siete volcados.
Todos los valores se reprodujeron **idénticos** en dos ejecuciones distintas.

| Frame | Esperado | Volcado |
|---|---|---|
| 60 | parado, Idle | `pos=(0,0,0.71) accion=Idle@32.7 combo=idle:-1` |
| 90 | 20 tics andando a 7 m/s = **2,33 m** | `pos=(0,2.33,0.71) accion=Andar@10.5` |
| 120 | 3,5 m + 20 tics a 11,9 m/s = **7,47 m** | `pos=(0,7.47,0.71) accion=Correr@10.5` |
| 150 | 3,5 + 50 × 0,1983 = **13,42 m** | `pos=(0,13.42,0.71) accion=Correr@2.5` |
| 178 | Ataque1 desde 170: 1 + 7/60·30·0,9615 = **4,4** | `accion=Ataque1@4.4 combo=active:0` |
| 210 | Ataque2 desde 200: 1 + 9/60·30·1,0 = **5,5** | `accion=Ataque2@5.5 combo=active:1` |
| 245 | Ataque3 desde 230: 1 + 14/60·30·0,902 = **7,3** | `accion=Ataque3@7.3 combo=active:2` |

Los pies quietos durante el combo: `pos` del Player fijo en 13,62 de 178 a 245.

Componentes de escena, en los mismos volcados:

- `Flotar` (cubo en (3,2,1), amp 0,25, periodo 3, fase = |3·0,7 + 2·1,3 + 1·0,4| mod 2π
  = 5,1): z esperada 1 + 0,25·sin(2π·t/3 + 5,1) → **1,19 / 1,23 / 1,04 / 0,81 / 0,76 /
  0,96 / 1,22**; volcados: 1.19, 1.23, 1.04, 0.81, 0.76, 0.96, 1.22.
- `Girar` (12°/s sobre Y local): el testigo hijo a (1,0,0) local queda en
  `(-3 + cos a, 2, 1 - sin a)`; a t = 0,983 s (a = 11,8°) esperado (-2,021, 0,795) →
  volcado `(-2.02,2.00,0.80)`; a t = 4,067 s (a = 48,8°) esperado (-2,341, 0,248) →
  volcado `(-2.34,2.00,0.25)`.
- Cámara: `(0,-6.66,5.47)` = Player + (0, 0, 2,6) + (0, -7·cos 18°, 7·sin 18°).
- `EnemyAI` persigue a 4 m/s: de (-12,-6) a (-8.48,-4.24) en 0,983 s = 3,93 m.

## Trampas (todas medidas esta noche)

1. **Un EMPTY no tiene bounding box.** `BKE_object_boundbox_get` devuelve `nullopt` para
   `OB_EMPTY` y `CcdPhysicsEnvironment` usa extents 1,0: la cápsula del Player queda con
   **radio 1, alto 0** (una esfera), ignorando `game.radius`. Reposa con el origen a
   z = 0,71. Para la cápsula 0,35 × 1,75 del contrato hará falta o un Player MESH
   invisible o que el motor lea `game.radius`/`empty_drawsize` (pendiente, fuera de
   este carril).
2. **Todo lo que cuelga del Player debe ser NO_COLLISION.** Con el STATIC por defecto,
   la malla del cuerpo es una pared dentro de la esfera del CHARACTER: Bullet la
   empuja fuera, la malla la sigue (es hija) y el Player sale disparado (medido:
   `pos=(11.15,48.25,32.48)` en el frame 60 sin tocar una tecla).
3. **El visor 3D guardado debe estar en vista de cámara.** El Blenderplayer pinta EEVEE
   desde el `RegionView3D` del `.blend`; si no está en `RV3D_CAMOB`, el motor crea
   `__default__cam__` con la vista libre y la captura sale desde (14.7,-6.5,8) aunque el
   volcado diga que `GameCamera` sigue al Player. `ArpgNative.blend` de la plantilla
   tiene este defecto. En bpy: `space.region_3d.view_perspective = 'CAMERA'` en todos
   los `VIEW_3D`.
4. **`BL_Action::Play` se niega a relanzar** «la misma acción con los mismos parámetros»
   mientras no ha terminado: un combo nuevo que empieza justo al acabar el Ataque1
   anterior se quedaría sin animación. Los de un tiro hacen `StopAction(0)` antes; el
   blend-in parte de la pose actual del armature, así que no hay salto.
5. **Las acciones sin usuario no se guardan** (`use_fake_user = True` en el generador). El
   motor las encuentra por nombre entre todas las de `bmain`, no hace falta asignarlas
   al armature. Con acciones «slotted» (4.4+) el motor usa `first_slot_handle`: cada
   acción necesita un slot del armature, que `keyframe_insert` crea solo.
6. **La animación va por reloj, la lógica por tics.** `BL_Action` avanza con
   `GetFrameTime()`; el combo con `1/ticrate`. Bajo carga (arranque del Player) el frame
   de un bucle se adelanta a lo que dan los tics: Idle@32.7 en el frame 60 cuando los
   tics dan 30,0. Por eso los ataques se comprueban en ventanas cortas.
7. **`bpy.ops.object.transform_apply(scale=True)` dejó el origen en (0,0,0)** con la
   geometría desplazada, en este Blender y con el objeto recién creado: el enemigo
   nació encima del Player. Escalar la malla (`mesh.transform`) no tiene ese problema.
8. **No hay `game.properties.new()`** en bpy: es `bpy.ops.object.game_property_new(type,
   name)` con el objeto activo.

## Pendiente

- Cápsula del EMPTY (trampa 1): decidir si el motor lee `game.radius` o si el Player
  pasa a ser una malla invisible.
- El Golpe recibido no interrumpe el combo (decisión, no descuido): habría que reiniciar
  la máquina de estados de `FL_ArpgCore` para hacerlo bien.
- Encadenar el yaw de la cámara al movimiento (que se recoloque sola detrás del
  personaje al andar sin ratón), como en KH1.
