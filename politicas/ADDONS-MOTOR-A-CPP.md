# Los addons del MOTOR, a C++ — qué había, cómo se verificó, qué queda

> Medido y ejecutado el 2026-09-11 sobre `flipendo-main`. Carril J.
> Doctrina: `politicas/LENGUAJE-CPP.md`, `politicas/MIGRACION-CPP.md`.
> Contexto: `politicas/PLAYER-SIN-CPYTHON.md` y `politicas/INVENTARIO-PYTHON.md` §2.6.

## 0. Por qué este bloque no admitía excusas

Flipendo distingue el Python **heredado** de Blender (el editor, que se migra por
fases) del Python **propio**, el que hace lo que el motor vende. `scripts/addons_core/`
tenía cinco addons `game_engine_*` que son exactamente eso: publicar el juego,
exportarlo como ejecutable, crear un personaje jugable, descartar vértices por cámara
y dar muelle a los huesos. **2.918 líneas.**

Y había una contradicción viva: el Player de distribución ya se compila **sin CPython**
(`PLAYER-SIN-CPYTHON.md`), de modo que Flipendo exportaba juegos sin intérprete usando
una herramienta que necesitaba intérprete.

## 1. La foto, medida antes de tocar nada

Lo primero fue **activar los cinco** en el binario de referencia y mirar qué pasaba:

```
addon_utils.enable(<módulo>, default_set=False, persistent=False)
```

| Addon | Líneas | ¿Registra en 4.5? | Evidencia |
|---|---:|---|---|
| `game_engine_publishing.py` | 576 | **NO** | `AttributeError: module 'bpy.utils' has no attribute 'register_module'` (línea 565). `register_module()` lo borró Blender en la 2.80 |
| `game_engine_save_as_runtime_eevee.py` | 416 | Sí | 9 propiedades, operador `wm.save_as_runtime` |
| `game_engine_add_basic_character.py` | 338 | **NO** | `RuntimeError: Error: Registering panel class: 'addcharacter' has category 'Add Character'`. Como el panel se registraba el primero, tampoco llegaban `character.gen` ni `fly_camera.gen` |
| `game_engine_object_camera_vertex_cull.py` | 287 | Sí | panel `PLS_PT_camera_vertex_cull` + `Object.camera_cull_props` |
| `game_engine_spring_bones.py` | 1.301 | Sí | 3 operadores, 3 paneles, 25 propiedades, 3 manejadores |

**Dos de los cinco (914 líneas, el 31%) llevaban muertos desde 2019.** No se migró un
comportamiento observable —no había ninguno—: se **repuso** la capacidad que prometían.

## 2. Lo entregado

Todo vive en un directorio nuevo, `source/blender/editors/flipendo/`, con su cabecera
pública en `source/blender/editors/include/FL_game_runtime.hh`. Se registra desde
`ED_spacetypes_init()` (`space_api/spacetypes.cc`) con una sola llamada,
`flipendo::game::operatortypes_register()`.

| Fichero C++ | Líneas | Sustituye a | Estado |
|---|---:|---|---|
| `fl_game_runtime.cc` | 631 | `game_engine_save_as_runtime_eevee.py` | **Verificado idéntico** |
| `fl_game_publishing.cc` | 550 | `game_engine_publishing.py` | **Repuesto y verificado** |
| `fl_game_character.cc` | 294 | `game_engine_add_basic_character.py` | **Repuesto y verificado** |
| `fl_game_camera_cull.cc` | 441 | `game_engine_object_camera_vertex_cull.py` | **Verificado idéntico** |

Identificadores conservados: `wm.save_as_runtime`, `wm.publish_platforms`,
`character.gen`, `RENDER_PT_publish`, `PLS_PT_camera_vertex_cull`.

### 2.1 El verificador: `--fl-dump-runtime`

Siguiendo el patrón del proyecto (`--fl-dump-keymap`, `--fl-dump-tools`, `--fl-dump-ui`),
el binario lleva ahora:

```
--fl-dump-runtime <bundle.app> [informe]
```

Vuelca, en orden estable: estructura del bundle, número de ficheros, enlaces y
directorios, bytes totales, el `.blend` empotrado con su tamaño, y **un renglón por
objeto con propiedad de juego `fl_component`**, con su recuento. Es lo que hace
comparable un entregable con otro.

Cómo se usa para verificar una migración del exportador:

1. Congelar el Player: `cp -R dev/build/bin/Blenderplayer.app /tmp/Blenderplayer-ref.app`.
   **Sin esto la comparación es falsa**: entre dos exportaciones el Player se recompila y
   cambian su tamaño y los `.pyc` de su árbol `scripts/` (se midió: 54 bytes por `.pyc`
   y 73.736 en el binario).
2. Exportar por los dos caminos contra ESE Player.
3. `--fl-dump-runtime` sobre los dos bundles y `diff`.

### 2.2 Cifras de la verificación (juego real, no maqueta)

Exportado `~/Flipendo/game/anima/T1_LaMancha.blend` (mapa de ÁNIMA) y
`~/Flipendo/game/template/ArpgNative.blend` por el camino Python y por el C++:

```
T1_LaMancha   4.912 / 4.912 líneas de volcado IDÉNTICAS
ArpgNative    4.912 / 4.912 líneas de volcado IDÉNTICAS

ambos: 4.383 ficheros, 516 directorios, 0 enlaces
T1_LaMancha: 805.814.626 bytes, game.blend 7.579.023, COMPONENTES 5/5
ArpgNative:  798.656.934 bytes, game.blend   421.331, COMPONENTES 5/5
```

Los cinco componentes de cada escena son `PlayerController`, `ThirdPersonCamera` y tres
`EnemyAI`: los tres tipos que registra `FL_RegisterBuiltinComponents()`
(`FL_ArpgComponents.cpp:340-342`). Ninguno se queda sin factoría.

Los dos ejecutables producidos por el C++ **arrancan y siguen vivos a los 18 segundos**,
con cero líneas de error.

Publicación completa de ÁNIMA (`wm.publish_platforms`) con assets y archivo:

```
bin/ANIMA.app   4.383 ficheros, 805.814.626 bytes, componentes 5/5
bin/assets/     copiado
bin.zip         269.194.903 bytes, 4.939 entradas, 812.372.511 sin comprimir
                unzip -t -> "No errors detected"
                el Player dentro conserva -rwxr-xr-x
descomprimido a mano (792 MB) y arrancado -> el juego CORRE
```

Descarte por cámara, contra el addon de Python, misma escena determinista
(rejilla 20×20, 441 vértices; cámara en (0,−8,4), rotación (1.1,0,0)):

```
margen 0.00                    Python 372 / C++ 372 marcados
margen 0.00 + distancia 10.0   Python 417 / C++ 417 marcados
margen 0.25, LISTA de índices  Python 259 / C++ 259, listas IDÉNTICAS
```

Personaje básico: `character.gen` produce `Player` (MESH, physics CHARACTER, actor,
bounds CONE, 17 vértices, `fl_component=PlayerController`) y `camPlayer` (CAMERA,
`fl_component=ThirdPersonCamera`, cámara de escena). Exportado: **componentes 2/2**;
el ejecutable arranca.

## 3. Las trampas, para que no se paguen dos veces

1. **`RNA_def_string()` guarda el PUNTERO del valor por defecto, no una copia**
   (`makesrna/intern/rna_define.cc:2215`). Con un buffer de pila, la propiedad queda
   apuntando a memoria muerta y leer su `default` desde fuera revienta. El buffer
   tiene que ser `static`.
2. **`ob->body_type` no es la fuente de verdad de la física de juego.** El getter de
   RNA (`rna_object.cc:1552-1594`) lo recalcula en cada lectura desde `ob->gameflag`.
   Hay que encender `OB_CHARACTER`. Y el setter de `physics_type` **apaga `OB_ACTOR`**
   al pasar a personaje (`:1631`): por eso el addon ponía `use_actor = True` después.
3. **La cabecera del directorio central de un ZIP son 46 bytes exactos.** Un
   `zip_put32(f, 0)` de más corrió cuatro bytes los atributos externos y el
   desplazamiento; `unzip` leyó como desplazamiento el modo Unix
   (`bad zipfile offset (lseek): 1106051072`, que es `040755 << 16`). Costó un `.zip`
   roto de 269 MB antes de verse.
4. **`cp -R src dst` con `dst` ya existente copia DENTRO.** El addon hacía
   `os.system('cp -R ...')` y producía un `Juego.app/Blenderplayer.app` que no arranca.
   El C++ borra el destino primero.
5. **El `.blend` del juego no es «el `.blend` más grande del bundle».** El volcador
   elegía `Contents/Resources/4.5/scripts/addons_core/bge_mixer/blender_data/tests/
   test_data.blend` (881.212 bytes) en vez del juego de ArpgNative (421.331). La ruta
   buena es siempre `Contents/Resources/game.blend`, que es la que busca
   `GPG_ghost.cpp:604`.
6. **En macOS, siete de las nueve propiedades de `wm.save_as_runtime` no hacían nada.**
   `WriteAppleRuntime()` salía por `return` antes de copiar Python, DLLs, librerías,
   scripts, datafiles, módulos y la licencia. Se registran igual, con sus mismos
   valores por defecto, para no romper ningún `.blend` ni script que las pase.
7. **Para comparar dos bundles hay que congelar el Player** (ver §2.1).

## 4. Deuda, con nombre y apellidos

### 4.1 `game_engine_spring_bones.py` — 1.301 líneas, SIGUE EN PYTHON

**No se retira**, porque no tiene sustituto y la doctrina prohíbe borrar sin él.
Es el único de los cinco que funciona de verdad hoy, y está medido:

```
addon_utils.enable("game_engine_spring_bones")            -> OK
bpy.ops.sb.spring_bone_frame() sobre un hueso con muelle  -> {'FINISHED'}
el hueso se mueve: (0,0,0) -> (0,0,5.0007) en 28 fotogramas
crea una restricción COPY_LOCATION llamada 'spring' y dos vacíos
  (Bone_spring, Bone_spring_tail)
```

Qué hay dentro, medido:

| Parte | Tamaño | Qué impide migrarla hoy |
|---|---:|---|
| Guion de Python EMPOTRADO (líneas 24-346) | 323 | Se escribe como bloque de texto en el `.blend` y se ata a un **controlador Python** (`spring_bone_gamestart`, líneas 399-410). En el Player sin CPython **no se ejecuta**: `SCA_PythonController::Trigger()` ya avisa de ello. Su sustituto es un `FL_Component` nativo, y eso vive en `source/gameengine/Flipendo/`, que no es este carril |
| Solucionador del editor | ~600 | Vector matemático con colisionadores de malla y de hueso, proyección punto-triángulo y restricciones/vacíos por hueso |
| Interfaz y estado | ~380 | **25 propiedades**: 9 en `Scene` (incluida una `PointerProperty` a un objeto y dos `CollectionProperty`), 12 en `PoseBone` y 4 en `Object`. Un equivalente nativo pide **DNA nueva en `bPoseChannel`, `Scene` y `Object`**, es decir tocar el formato del `.blend`: otro carril y otra decisión |

**Además ya está roto en parte**: `sb.select_bone` recorre `data_bone.layers` y
`armature.layers`, que Blender **quitó en la 4.0** (las sustituyeron las colecciones de
huesos). Comprobado: `'layers' in bpy.types.Bone.bl_rna.properties` da **False**.

**Plan de migración, en tres fases y por este orden:**

1. **`FL_SpringBones`, componente nativo** en `source/gameengine/Flipendo/`, atado por
   `fl_component = "SpringBones"`, que reproduzca el solucionador del guion empotrado
   (líneas 24-346). Con eso el muelle funciona **en el juego exportado**, que es donde
   hoy no funciona, y deja de haber un controlador Python en el `.blend`.
2. **DNA de los ajustes**: `sb_*` sobre `bPoseChannel` y `Object`, más los dos
   `ListBase` de la escena. Es un cambio de formato de `.blend` y necesita versionado.
3. **Solucionador y paneles del editor** en C++, con
   `BKE_callback_add(CB_EVT_FRAME_CHANGE_POST, ...)` en lugar de
   `bpy.app.handlers.frame_change_post`, y los tres `idname` intactos
   (`sb.spring_bone`, `sb.spring_bone_frame`, `sb.select_bone`).

Verificación propuesta: `--fl-dump-springbones <blend> <informe>` que vuelque la matriz
de cada hueso con muelle a lo largo de N fotogramas, congelando la línea base con el
Python aún vivo. Es el mismo patrón que se usó aquí.

### 4.2 Retirado sin sustituto, y por qué

De `game_engine_publishing.py` (que **no registraba nada**, §1):

| Retirado | Motivo |
|---|---|
| `scene.publish_download_platforms` | Descargaba builds de Blender de `download.blender.org` con `urllib` y las descomprimía. No es capacidad de Flipendo, que es un fork de un solo objetivo (Mac Intel, Metal); pide red; y la URL que construye no existe para esta versión |
| `scene.publish_auto_platforms` | Rastreaba una carpeta `lib/` buscando `blenderplayer` de otras plataformas. Sin plataformas ajenas no hay nada que rastrear |
| `scene.publish_add_platform`, `scene.publish_remove_platform` | Editaban la lista de plataformas |
| `scene.publish_add_assetpath`, `scene.publish_remove_assetpath` | Editaban la lista de assets. Hoy `wm.publish_platforms` acepta **una** ruta de assets; una lista pide colección en DNA de escena |
| `RENDER_UL_platforms`, `RENDER_UL_assets`, `PUBLISH_MT_platform_specials` | Interfaz de esas dos listas |

De `game_engine_add_basic_character.py` (que **tampoco registraba nada**):

| Retirado | Motivo |
|---|---|
| `fly_camera.gen` | Cámara libre armada con actuador MOTION en modo `OBJECT_CHARACTER` + ratón. No hay componente nativo equivalente. Reponerlo es escribir `FL_FlyCamera` en `source/gameengine/Flipendo/`: otro carril |
| `key_sensitive`, `mouse_sensitive`, `character_keys`, `character_jump` | Configuran teclas y sensibilidad de los ladrillos. El `FL_PlayerController` nativo tiene **WASD, espacio y J fijos en el código** (`FL_ArpgComponents.cpp:112,140,168-169`). Para honrarlas, el componente tiene que leer propiedades de juego: `source/gameengine/Flipendo/`, otro carril |

### 4.3 Deuda que toca a otros carriles

1. ~~**Entrada de menú `File > Export > "Save as game runtime"`.**~~ **PAGADA el
   2026-09-11 a las 07:25, commit `318f306309c`.** El addon la añadía con
   `bpy.types.TOPBAR_MT_file_export.append()`; ese menú lo dibujaba
   `scripts/startup/bl_ui/space_topbar.py` y desde C++ no hay API para insertar en un
   menú de Python, así que el operador quedó registrado e invocable pero sin entrada de
   menú. Ahora `TOPBAR_MT_file_export` es C++
   (`editors/space_topbar/fl_topbar_menus.cc`) y la fila vuelve a estar, **la última**,
   que es donde `append()` la ponía.

   Dos cosas que esta deuda enseñó y conviene no perder:

   - **La línea base de interfaz había congelado la ausencia.** Se congeló después de
     perder la fila, así que decía «idéntico» sobre un menú al que le faltaba una
     capacidad. Hubo que **corregirla** —primera vez en esta migración— y está
     justificado en el commit y en `MENUS-DEL-KEYMAP-A-CPP.md`. Una línea base puede
     tener **de menos** algo que el programa debería tener, no solo de más.
   - **Se verificó exportando de verdad**, no solo con el volcado: `ArpgNative.blend`
     sale como bundle de 3.642 ficheros y 786.534.791 bytes, con los cinco componentes
     nativos dentro, y el ejecutable arranca y sigue vivo a los 8 s.
2. **La línea base de interfaz se mueve.** Retirar cuatro `.py` de `addons_core/` quita
   cuatro filas de Preferencias > Complementos, así que
   `tests/flipendo/ui/baseline-python-layout.txt` (**carril D**) deja de cuadrar. Es la
   consecuencia buscada de «cero Python», no un fallo. No se toca ese fichero desde aquí.
3. **Recálculo automático del descarte por cámara.** El addon lo hacía en
   `frame_change_post` y `depsgraph_update_post`. El equivalente nativo es
   `BKE_callback_add(CB_EVT_FRAME_CHANGE_POST, ...)` (`BKE_callback.hh`); hoy el
   recálculo es por operador.
4. **Camino de ejecutable suelto (`BRUNTIME`).** Está escrito en
   `fl_game_runtime.cc:write_runtime_single()` y compila, pero **no se ha podido
   verificar**: Flipendo solo construye Player para macOS, donde el Player es un `.app`.
5. **El `.zip` no lleva Zip64.** Por encima de 4 GB sin comprimir haría falta. El bundle
   de hoy son 812 MB.

## 5. Cuenta de líneas

```
antes:  scripts/addons_core/game_engine_*.py     2.918 líneas de Python
después: game_engine_spring_bones.py             1.301 líneas de Python  (deuda §4.1)
         source/blender/editors/flipendo/*.cc    1.916 líneas de C++
         source/blender/editors/include/FL_game_runtime.hh  72 líneas

retirado de Python:  1.617 líneas  (55% del bloque)
de ellas MUERTAS antes de tocarlas: 914 (publishing 576 + character 338)
```
