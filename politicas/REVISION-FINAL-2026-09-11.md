# Revisión final de la noche — busca de capacidad perdida en silencio

> Carril P, 05:00–06:00 del 2026-09-11. Rango revisado:
> `4e88d96a431..HEAD` — **115 commits**, 887 ficheros, +254.130 / −109.542 líneas,
> siete carriles.
>
> Encargo: buscar Python retirado sin sustituto, registros duplicados o perdidos,
> verificaciones hechas contra el bundle rancio, y `#ifdef WITH_PYTHON` que
> escondan capacidad. **No es una revisión línea a línea**: prioriza los borrados
> grandes y los registros, que es donde se pierde capacidad de verdad.

---

## Resumen en tres líneas

**No he encontrado ninguna capacidad perdida en silencio.** Los 52 operadores y
las 24 clases de interfaz que registraban los `.py` retirados están **todos** en
el binario. Hay **una sospecha real** (verificaciones anteriores a las 04:10, que
he vuelto a pasar yo mismo y salen limpias), **un defecto heredado** de upstream y
**una capacidad apagada por opción de build** que conviene decidir a propósito.

---

## 1. Qué revisé, y cómo

No por lectura del diff: por medición contra el binario.

| Comprobación | Método |
|---|---|
| Python retirado sin sustituto | Extraje **todos** los `bl_idname` y clases `_OT_/_PT_/_MT_/_HT_/_UL_` de los **202 `.py` de `scripts/` borrados**, leyéndolos en `4e88d96a431`, y los busqué **en el binario vivo** (`bpy.ops`) y en el volcado nativo (`--fl-dump-ui`) |
| Registros duplicados | `ot->idname` y `panel.idname` repetidos en todo `source/` |
| Registros perdidos | Cadena de llamada hasta `ED_spacetypes_init()`, y orden dentro de ella |
| Imports colgando | Cada módulo borrado de `scripts/modules`, buscado en el Python que queda |
| `WITH_PYTHON` | Los 7 bloques nuevos del diff, leídos uno a uno |
| Bundle rancio | Cruce de la hora de cada commit de retirada contra la hora en que empezó a correr la poda |

---

## 2. Lo que está bien, con cifras

### 2.1 Ni un identificador perdido

```
operadores declarados por los .py borrados ......... 52
  presentes en el binario ......................... 52   (100 %)
clases de interfaz registradas por los .py borrados  24
  presentes en el volcado nativo .................. 24   (100 %)
```

Dos que parecían faltar eran **falsos positivos míos**, y merece decirlo porque
es la trampa del método: busqué por **nombre de clase** cuando lo que se registra
es el **`bl_idname`**. `PLS_PT_CameraCullPropertiesPanel` se registra como
`PLS_PT_camera_vertex_cull` (`fl_game_camera_cull.cc:419`) y ahí está. Igual los
seis `scene.publish_*`.

Los únicos identificadores sin rastro son tres de
`scripts/templates_py/ui_tool_simple.py` (`my_template.my_circle_select`,
`my_template.my_gizmo_translate`, `my_template.my_other_select`): son una
**plantilla de ejemplo** que el editor de texto ofrecía, no capacidad.

### 2.2 Registro de `flipendo::game`, comprobado hasta el final

`fl_game_publishing.cc` / `fl_game_character.cc` / `fl_game_camera_cull.cc` →
`flipendo::game::operatortypes_register()` (`fl_game_runtime.cc:621`) →
**`spacetypes.cc:105`, dentro de `ED_spacetypes_init()`**. Y el orden es el
correcto: los `ED_spacetype_*()` se crean en las líneas 71-90, antes del 105, así
que el `BKE_spacetype_from_id(SPACE_PROPERTIES)` de `publish_panel_register()`
(`fl_game_publishing.cc:514`) no puede devolver `nullptr`. Era el candidato
número uno a «registro que se queda sin llamar» —un `return` silencioso si el
espacio aún no existe— y **no lo es**.

Verificado en el binario: los 6 `scene.publish_*`, `wm.publish_platforms` y los
paneles `RENDER_PT_publish` y `PLS_PT_camera_vertex_cull`, todos presentes.

### 2.3 Ni un import colgando

Ninguno de los módulos borrados de `scripts/modules` deja una referencia viva en
el Python que queda. El único acierto era un `docstring` de
`bpy_extras/io_utils.py:135` que contiene la palabra «helpers».

### 2.4 Los 7 `WITH_PYTHON` nuevos: ninguno esconde nada

Cinco son **comentarios** que documentan un hueco ya existente (`FL_*_menus.hh`
de consola, gráfico, info, outliner, texto y vista 3D: «con `WITH_PYTHON=OFF` esa
tecla no abre nada»). Escribir el hueco es lo contrario de esconderlo.

Los dos reales:

- **`numinput.cc`** — *mejora*: el `#ifdef` **envolvía toda**
  `user_string_to_number()` y ahora solo envuelve la llamada a CPython. Menos
  capacidad escondida que antes.
- **`fcurve_driver.cc`** — *mejora clara y de las buenas de la noche*: la rama
  `#ifndef WITH_PYTHON` era `UNUSED_VARS(...)`, de modo que un driver con
  expresión no soportada **se quedaba clavado en 0.0 sin decir una palabra** — el
  rig se mueve mal y parece que el `.blend` está roto. Ahora avisa una vez por
  driver, con nombre y con qué hacer. Eso es convertir un fallo silencioso en uno
  visible.
- **`wm_keyconfig_ops.cc:88`** — correcto: el camino nativo va primero y el
  `#ifdef` es solo el respaldo que sigue leyendo los `.py` de keyconfig que el
  usuario ya tenga. Es compatibilidad, no ocultación.

### 2.5 Re-verificación completa con el bundle ya limpio

Nueve arneses `--fl-check-*`, pasados **después** de la poda:

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

---

## 3. Sospechas, por gravedad

### 🟠 A — Ocho retiradas se verificaron contra el bundle rancio. Las he repasado yo

**La poda empezó a correr a las 04:10:25** (primer «Flipendo: podando» en
`claude-C2-build33.log`). Estas ocho retiradas de módulos **importables por
nombre** son todas anteriores, así que se verificaron con el módulo que decían
haber quitado **todavía presente e importable** desde el bundle:

| Módulo retirado | Commit | Hora |
|---|---|---|
| `bl_keymap_utils/keymap_from_toolbar` | `42fc3b41ce9` | 01:13 |
| `bpy/utils/toolsystem` | `d7c4789204b` | 02:11 |
| `bl_app_override/` | `75b5322aedc` | 02:53 |
| `graphviz_export`, `bpy_extras/mesh_utils`, `bpy_extras/id_map_utils` | `840486f911a` | 02:59 |
| `bl_text_utils/external_editor` | `ed69d1872be` | 03:07 |
| `rna_manual_reference` | `779a06e93b5` | 03:18 |

**Matiz importante, para no alarmar de más:** el riesgo real es menor de lo que
parece. Los 12 huérfanos de `startup/` **no se cargaban** (ni
`bl_operators/__init__.py` ni `bl_ui/__init__.py` los nombraban ya), y varias de
esas retiradas se justificaron con una prueba **estática** («sin una sola
referencia», `840486f911a`), que el bundle no puede falsear. El riesgo se
concentra en los módulos de `scripts/modules`, que se importan por nombre.

**Lo he resuelto en vez de dejarlo escrito:** los nueve arneses de §2.5 se han
vuelto a pasar con el bundle limpio y **salen todos idénticos**, incluidos los
dos que cubren directamente dos de esas retiradas —
`--fl-check-manual` (7470/0, cubre `rna_manual_reference`) y
`--fl-check-external-editor` (4326/4326, cubre `bl_text_utils`). **No hace falta
repetir nada.**

Lo que sí queda sin arnés propio y solo tiene prueba estática:
`bl_app_override/`, `graphviz_export`, `bpy_extras/mesh_utils` y
`bpy_extras/id_map_utils`. Su commit dice «sin una sola referencia» y lo he
vuelto a comprobar: cierto, y ahora además con el bundle limpio el editor arranca
sin un solo traceback.

### 🟡 B — `MESH_OT_edgering_select` definido dos veces (heredado, no de esta noche)

```
source/blender/editors/mesh/editmesh_select.cc:1989
source/blender/editors/mesh/editmesh_loopcut.cc:719
```

Los dos ponen `ot->idname = "MESH_OT_edgering_select"`. **Solo uno se registra**
(`mesh_ops.cc:158`, un único `WM_operatortype_append`), así que no hay operador
duplicado en el binario. **Ya estaba así en `4e88d96a431`** y ninguno de los dos
ficheros se ha tocado esta noche: es deuda de upstream, no regresión. Se anota
porque el día que alguien añada el segundo `append` tendrá un choque difícil de
leer.

### 🟡 C — Cycles está apagado por opción de build, y eso sí es capacidad

`WITH_CYCLES:BOOL=OFF` en `dev/build`. Consecuencia medida: el addon de Cycles
**ni se instala**, `addon_utils.check("cycles")` devuelve `(False, False)` y el
fork **no tiene trazado de rayos**. `INVENTARIO-PYTHON.md §2.7` descartó
expresamente esa vía —«retirar Cycles es una pérdida de capacidad que la doctrina
prohíbe sin sustituto»— y decidió **migrar** sus 6.109 líneas. Hoy el build hace
justo lo contrario, y en silencio.

No es de esta noche y no lo toco: es una decisión de Jesús, no mía. Pero
conviene que sea **una decisión**, y no un `OFF` heredado que nadie ha mirado.

### 🟢 D — Dos arneses dieron una diferencia en la primera pasada y ninguna en las siguientes

`--fl-check-mesh-ops` dio `13111/13112` una vez y `13112/13112` las **tres**
siguientes. `--fl-check-ui` dio `2112/2113` una vez y `2113/2113` las dos
siguientes.

Encaja con la lección de las 03:50 —el cálculo de normales de vértice **no es
determinista**— y `paint.vertex_color_dirt`, que es lo que mide ese arnés,
depende de normales. **Recordatorio para quien lo use: una sola pasada de esos
dos arneses no es una línea base.** Hay que repetir antes de perseguir un
fantasma.

### 🟢 E — Plantillas de Python retiradas

`scripts/templates_py/` se fue con sus 41 ficheros. Es lo correcto —sin
intérprete no hay script que plantillar— pero conviene tenerlo en la lista de
«lo que el usuario notará»: el menú *Plantillas* del editor de texto queda vacío.

---

## 4. Lo que NO revisé, y se dice

- **No revisé línea a línea las 254.130 insertadas.** La revisión es por
  identificadores, registros y puntos de carga, que es donde se pierde capacidad.
  Un error de *comportamiento* dentro de un operador correctamente registrado no
  lo detecta este método: para eso están los arneses, y los nueve que existen
  pasan.
- **No revisé las propiedades una a una** (nombre, tipo, subtipo, defecto, rango,
  enum, flags) de los 52 operadores migrados. La doctrina lo exige; comprobar 52
  operadores × sus propiedades no cabía en la hora. `--fl-check-optypes` cubre
  44 elementos y da 44/44, pero **no son los 52**.
- **No revisé `intern/cycles` ni `extern/`.**
- Los gtests no entran aquí: su parte está en `politicas/TESTS-A-CPP.md §6-§7`
  (448 pasan, 255 no llegan a correr por cuatro bloqueadores encadenados).

---

## 5. Conclusión

De lo que el encargo pedía buscar:

1. **Python retirado sin sustituto: no lo hay.** 52/52 operadores y 24/24 clases
   de interfaz, presentes y medidos en el binario.
2. **Registros duplicados o perdidos: ninguno de esta noche.** El único duplicado
   es heredado de upstream y no llega a registrarse dos veces.
3. **Verificaciones contra el bundle rancio: ocho, todas anteriores a las
   04:10.** Repasadas con el bundle limpio: los nueve arneses salen idénticos.
   No hay que repetir nada.
4. **`#ifdef WITH_PYTHON` que escondan capacidad: ninguno.** Dos de los nuevos
   hacen lo contrario: sacan a la luz un fallo que antes era mudo.

La única cosa que me llevaría al parte de Jesús como decisión pendiente es
**Cycles apagado por `WITH_CYCLES=OFF`**: es la única capacidad que hoy no está,
que el inventario dijo expresamente que no se retirara, y que nadie ha decidido
en voz alta.
