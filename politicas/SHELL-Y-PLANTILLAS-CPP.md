# Shell propio y plantillas de usuario → C++ (2026-09-11)

Cierra dos de los objetivos de `LENGUAJE-CPP.md`, "Decisión del 2026-09-11": **cero
shell propio** y la parte de **cero Python** que tocaba a las plantillas del editor
de texto.

## 1. Shell propio: 383 líneas, cinco ficheros, cero migraciones

Quedaban cinco `.sh` fuera de `extern/` y `lib/`. Los cinco se **retiraron**; ninguno
se migró a C++, y esa es la conclusión, no un atajo: **ninguno implementaba capacidad
de Flipendo**. La comprobación fue la misma en todos: `grep` del nombre del fichero en
todo el árbol (excluyendo `.git` y `lib/`), más búsqueda explícita en `CMakeLists.txt`,
`*.cmake`, `GNUmakefile`, `tests/` y `doc/`. **Cero invocaciones en los cinco casos.**

| Fichero | Líneas | Destino | Razón, en una línea |
|---|---|---|---|
| `intern/libmv/bundle.sh` | 204 | RETIRAR | Resincroniza libmv con upstream: capacidad que el proyecto abandonó por escrito |
| `intern/libmv/mkfiles.sh` | 9 | RETIRAR | Solo genera `files.txt`, cuyo único consumidor era `bundle.sh` |
| `source/blender/makesrna/rna_cleanup/rna_update.sh` | 18 | RETIRAR | Campaña de renombrado de RNA que upstream declaró muerta en 2010 |
| `tests/files/imbuf_io/multilayer/generate.sh` | 116 | RETIRAR | Genera 3 `.exr` que ya están commiteados y los consume un test C++ |
| `tests/files/sequence_editing/ffmpeg/media/build_test_movies.sh` | 36 | RETIRAR | Genera 22 `.mp4`/`.webm` que ya están commiteados |

### Las trampas que hacían falta para decidir bien

- **Un guion sin invocador puede seguir siendo capacidad viva.** Por eso no bastó el
  `grep`: en los cuatro casos de datos hubo que demostrar además que *la salida* existe
  y quién la consume. El caso mejor es `generate.sh`: sus tres `.exr` los carga
  `source/blender/blenkernel/intern/image_test.cc` (líneas 315, 343, 379). Es decir, la
  capacidad viva (el test de regresión de EXR multicapa) **ya estaba en C++**; el guion
  era andamio.
- **Varios eran literalmente inejecutables**, lo que confirma que llevaban años muertos:
  - `bundle.sh` clona de `repo="/home/sergey/Developer/libmv"`, el portátil de Sergey
    Sharybin, con la URL real comentada encima.
  - `bundle.sh` genera además plantillas de **SCons** (`src += env.Glob(...)`), y SCons
    se eliminó de Blender en la 2.8: no queda un solo `SConscript` en el árbol.
  - `rna_update.sh` invoca `./blender.bin`, el nombre del binario en Linux; en el árbol
    de Jesús es `Blender.app/Contents/MacOS/Blender`.
  - `generate.sh` lee de un subdirectorio `passes/` que **no está versionado** (su propia
    cabecera dice que no hace falta commitearlo) y exige "Blender 4.0".
  - Ninguna de las herramientas externas que necesitaban está instalada en la máquina:
    `oiiotool`, `exrmultipart`, `exrinfo` y `ffmpeg` dan las cuatro "no encontrado".
- **Un fichero de datos puede declararse muerto a sí mismo.** `rna_properties.txt`, el
  fichero que mantenía toda la cadena de `rna_update.sh`, dice en su única línea:
  *"currently this isn't in use"*. Lo dice upstream, no Flipendo.

### Por qué RETIRAR y no MIGRAR, en general

La capacidad de los cinco es "invocar herramientas de terceros (git, ffmpeg, oiiotool)
para refabricar algo que ya existe en el repo, o para resincronizar con un upstream del
que Flipendo ya divergió". Reescribir eso en C++ no da propiedad del código: añade
superficie para envolver binarios que no están instalados. Doctrina, regla 1: los
assets no son código, y estos multimedia y `.exr` son assets.

### Excepción que se mantiene

El shell de `extern/` (6 ficheros) y `lib/` (0) **no se toca**: son terceros
vendorizados, se actualizan verbatim desde upstream y están marcados
`linguist-vendored` en `.gitattributes`. Es la única excepción doctrinal de
`LENGUAJE-CPP.md`.

### Lo que queda de shell, y no es `.sh`

El censo por extensión da cero, pero el censo **por shebang** encuentra dos que hay que
dejar escritos porque no están cerrados:

1. **`release/darwin/scripts/blender-system-info.sh.in` — shell VIVO que se instala.**
   `source/creator/CMakeLists.txt:1834-1840` lo configura e instala en el paquete de
   macOS (y el gemelo de `release/freedesktop/` en las líneas 1092-1098). No es un
   guion de desarrollo: **viaja en el producto**. Su cuerpo entero existe para lanzar el
   Python empotrado sobre `url_prefill_startup.py`, así que **muere con el intérprete**:
   lo correcto es que lo retire quien quite la instalación de Python, no este carril, y
   que la información de sistema pase a una opción C++ del binario. Pendiente declarado.

2. **`tools/flipendo_cli/flipendo.bash.legacy` — 127 líneas de bash propio, NO retirable
   todavía.** Su sustituto C++ existe y está en uso: `tools/flipendo_cli/flipendo_cli.cpp`,
   compilado en `~/Flipendo/bin/flipendo` (94 KB) y en el `PATH`. Pero la migración de la
   Fase A (commit `ecdb8bcb19d`) **está incompleta** y conviene que se sepa:

   | Subcomando del bash | ¿En el C++? |
   |---|---|
   | `list`/`ls`, `snapshot`/`snap`, `activate`/`use`, `info`, `status` | Sí |
   | `svn` | Añadido nuevo en C++ |
   | **`diff`** | **No** |
   | **`delete`/`rm`** | **No** |
   | **`run`** | **No** |

   Comprobado sobre el despacho de `flipendo_cli.cpp:267-277`. Retirar el bash ahora
   perdería tres capacidades, que es justo lo que prohíbe la regla 2 de la doctrina
   ("migrar, no borrar"). **Se conserva hasta que esos tres subcomandos estén en C++**, y
   entonces se retira. No es una excepción permanente: es una migración a medias.

## 2. `scripts/templates_py` → `FL_TemplateComponents.cpp`

40 ficheros, 2.700 líneas de plantillas de Python que el editor de texto ofrecía en
**Text → Templates → Python**.

### La capacidad, separada de su forma

La capacidad no era "tener 40 `.py`": era **"que el editor me dé un ejemplo que
funciona, para copiarlo y empezar"**. Esa capacidad es real y hay que conservarla. Su
forma en Python, no: un ejemplo de `bpy`/`bge` sin intérprete no es un punto de partida,
es texto muerto. De las 40, **37 son ejemplos genéricos de addon de Blender** (operadores,
gizmos, paneles, listas, nodos, bmesh, drivers) y **3 son controladores de ladrillo
lógico de la BGE** (`gamelogic.py`, `gamelogic_module.py`, `gamelogic_simple.py`).
Ninguna de las 40 conserva sentido alguno sin `bpy`/`bge`.

Por eso: **se retiran las 40 y se sustituyen**, que es lo que pide la regla 2.

### Ya estaban fuera del producto

`scripts/` solo se instala dentro de `if(WITH_PYTHON)` (`source/creator/CMakeLists.txt`
:492 y :503), y Flipendo ya añadió `WITH_INSTALL_PRUNE_SCRIPTS` (:475-490), que borra el
`scripts/` instalado por completo. `templates_py` **no** está en ninguna de las reglas
de instalación incondicionales (sí lo están `scripts/presets` y
`bl_app_templates_system`). En una build sin Python no llegaban a ninguna parte.

### El sustituto

`source/gameengine/Flipendo/FL_TemplateComponents.cpp`: cinco plantillas de componente
nativo, cubriendo los arquetipos que cubrían las de Python, traducidos al mundo del motor.

| Plantilla | Sustituye a | Enseña |
|---|---|---|
| `TemplateHello` | `operator_simple.py`, `bmesh_simple.py` | El esqueleto mínimo: `Start()` + `Update(dt)` |
| `TemplateKeyInput` | los 3 `gamelogic*.py` | Entrada de teclado sin ladrillos lógicos; `ACTIVE` vs `JUSTACTIVATED` |
| `TemplateTimer` | `operator_modal_timer.py` | Acción periódica acumulando `dt`, sin temporizador del motor |
| `TemplateProperty` | la familia `ui_panel.py`/`ui_list.py` | Ajustes editables sin recompilar, vía propiedades de juego |
| `TemplateFollow` | `custom_nodes.py`, `operator_mesh_*.py` | Leer la escena y reaccionar a otro objeto |

**Decisión de diseño: no son un menú, son código compilado.** Las de Python vivían en un
menú del editor de texto porque se ejecutaban en el intérprete. Un componente nativo no
se escribe así: se escribe en un `.cpp`, se añade a `Ketsji/CMakeLists.txt` y se
recompila. Ponerlas en el árbol, compiladas y registradas, tiene dos ventajas sobre un
menú de plantillas:

- **No pueden pudrirse.** Las compila el compilador en cada build. Los 40 `.py` podían
  quedarse desfasados durante años sin que nadie se enterara, y varios lo estaban.
- **Son utilizables tal cual.** Están registradas, así que el usuario puede atar
  `TemplateHello` a un objeto con la propiedad `fl_component` y darle a P sin escribir
  una línea. Una plantilla de texto no hace eso.

El fichero lleva en su cabecera las instrucciones de las dos cosas: cómo probar una
plantilla y cómo partir de ella para escribir un componente propio (incluido el paso
del `CMakeLists.txt`, que es donde se tropieza).

### Trampas encontradas al escribirlas

- **`KX_GameObject` no tiene `IsBeingDestroyed()`.** Es el error natural al guardar un
  puntero a otro objeto en `Start()`. El idioma real del motor es preguntar a la lista de
  la escena: `scene->GetObjectList()->SearchValue(obj)`, que es exactamente lo que hace
  `FL_ComponentManager::Tick` para soltar componentes huérfanos. La plantilla
  `TemplateFollow` lo enseña con el comentario puesto.
- **Registro explícito, no el macro.** `FL_Component.hpp` define
  `FL_REGISTER_COMPONENT`, pero **no tiene ni un solo uso en el árbol** y no debe usarse:
  con inicializadores estáticos en una biblioteca estática, el enlazador descarta la
  unidad de traducción entera si nadie la referencia, y los componentes desaparecen sin
  aviso. Se sigue el patrón de `FL_RegisterBuiltinComponents`: una función llamada desde
  `FL_ComponentManager::AttachScene`, y esa llamada es la referencia que mantiene el
  fichero vivo.

### Consecuencias declaradas (no son regresiones)

1. **El menú Templates → Python queda vacío.** `TEXT_MT_templates_py`
   (`scripts/startup/bl_ui/space_text.py:283`) enumera el directorio con
   `bpy.utils.script_paths(subdir="templates_py")`; sin directorio, muestra
   "* Missing Paths *". **Ya hay precedente en el árbol**: `TEXT_MT_templates_osl` (:306)
   y `TEXT_MT_templates_py_components` (:294) apuntan desde hace tiempo a directorios que
   no existen. Ese menú es Python y desaparece con el intérprete; no se toca
   `space_text.py`, que no es de este carril.
2. **Divergencia declarada contra la línea base de interfaz.**
   `tests/flipendo/ui/baseline-python.txt` y `baseline-python-layout.txt` (carril D)
   contienen las 40 entradas del submenú, congeladas con Python aún activo. Al retirar el
   directorio, esa sección ya no se reproduce. **Es divergencia intencionada, no
   regresión**: la línea base registra lo que hacía Python, y esta capacidad se ha
   retirado y sustituido a propósito. Queda escrito aquí para que quien pase
   `--fl-check-ui` no lo persiga como un fallo. Los ficheros de línea base son del carril
   D y no se han tocado.
3. `tools/check_source/check_licenses.py:552` lista `./scripts/templates_py` entre los
   directorios que recorre. Es un `.py` de utillaje, ya clasificado para retirada en
   `INVENTARIO-PYTHON.md`; recorrer una ruta que no existe no rompe nada. No es de este
   carril.

## Estado

- Shell propio en el árbol: **0 ficheros `.sh`** fuera de `extern/`.
- Shell por shebang pendiente: **2**, ambos documentados arriba con su condición de
  salida (`blender-system-info.sh.in` muere con el intérprete;
  `flipendo.bash.legacy` cuando `diff`/`delete`/`run` estén en C++).
- Plantillas de usuario: de 40 `.py` no instalables a 5 componentes C++ compilados,
  registrados y utilizables.
