# Pestañas del editor de Propiedades en C++

## Primera unidad: Effects

`scripts/startup/bl_ui/properties_data_shaderfx.py` era una unidad autónoma de 31 líneas y un
solo panel. Ahora `DATA_PT_shader_fx` se declara en
`space_buttons/fl_properties_data_shaderfx.cc` con `FL_ui_registry.hh`:

- espacio `PROPERTIES`, región `WINDOW`, contexto `shaderfx`;
- etiqueta `Effects`, bandera `PANEL_TYPE_NO_HEADER`;
- menú enum `OBJECT_OT_shaderfx_add.type` y `uiTemplateShaderFx` nativos.

El bloque de registro conserva todos sus metadatos y el bloque de diseño conserva el único
botón visible, `Add Effect`. El módulo desaparece también de `bl_ui/__init__.py`, de modo que no
se importa ni ejecuta Python para esta pestaña.

Mientras convivan paneles nativos y Python de Propiedades con `order=0`, el panel nativo se
registra al comienzo de la familia en vez de en la posición intermedia que imponía el orden de
imports Python. Es una deuda temporal de orden global, no de contexto ni de dibujo; desaparece
al migrar la familia completa o al introducir reservas de orden estables en el registro.


## Segunda unidad: la pestaña del objeto Vacío

`scripts/startup/bl_ui/properties_data_empty.py` (84 líneas, **dos** paneles) pasa entera a
`space_buttons/fl_properties_data_empty.cc`. Se migra la pestaña **completa**, nunca un panel
suelto: un panel aislado cambia de posición dentro de la región y el volcado marca diferencia
aunque no cambie ni un campo (`UI-A-CPP.md`).

- `DATA_PT_empty` — contexto `data`, etiqueta `Empty`, contexto de traducción
  `BLT_I18NCONTEXT_ID_ID`, `poll` = `ob and ob.type == 'EMPTY'`.
- `DATA_PT_empty_image` — etiqueta `Image`, `poll` con además `empty_display_type == 'IMAGE'`.

**Trampas del dibujo**, que son las que cambian el volcado:

- `depth_row.enabled = not ob.show_in_front` es `uiLayoutSetEnabled` sobre la **fila**, no sobre
  la columna: si se pone en la columna se apaga también el `Side` de debajo.
- `col = layout.column(align=False, heading="Opacity")` lleva `use_property_decorate = False`
  porque el decorador lo pone a mano `row.prop_decorator(ob, "color", index=3)`. Sin apagarlo
  salen **dos** decoradores.
- `sub.active = ob.use_empty_image_alpha` es `uiLayoutSetActive`, no `SetEnabled`: son dos
  aspectos distintos en el volcado.
- Las propiedades con índice (`empty_image_offset` 0 y 1, `color` 3) necesitan la sobrecarga de
  `prop()` que toma `PropertyRNA *` e índice; la que toma el nombre no lo admite.

**Verificado** (`--fl-check-ui` sobre una copia del binario, no sobre el instalado):

| Volcado | Resultado |
|---|---|
| Registro (`baseline-python.txt`) | **2.113 bloques, 2.112 idénticos, 1 distinto, 0 faltan, 0 sobran** |
| Diseño (`baseline-python-layout.txt`) | **2.004 bloques, 1.983 idénticos, 21 distintos, 0 faltan, 0 sobran** — y **ninguno de los 21 es de esta pestaña** |

El único bloque distinto del registro es `REGION PROPERTIES WINDOW`, y **ya lo estaba antes**:
es la deuda de orden global que documenta la primera unidad. La divergencia arranca donde los
paneles nativos se registran al principio de la familia; `DATA_PT_empty` y `DATA_PT_empty_image`
se suman a esa misma cabecera desplazada. `0 faltan, 0 sobran` es lo que importa: los dos
paneles existen con sus metadatos idénticos.

Los 21 bloques distintos del diseño son menús y cabeceras de **presets**
(`RENDER_PT_ffmpeg_presets`, `USERPREF_MT_keyconfigs`, `TEXT_MT_templates*`…): cambiaron al pasar
los presets de `.py` a `.fpreset` y al retirar el Python de las plantillas. No los toca esta
pestaña.

## Tercera unidad: Nodos de simulación (pestaña Físicas)

`scripts/startup/bl_ui/properties_physics_geometry_nodes.py` (64 líneas, **un** panel, **cero**
dependencias compartidas) pasa entera a
`space_buttons/fl_properties_physics_geometry_nodes.cc`. Es una unidad completa aunque la
pestaña `physics` tenga más paneles: el fichero entero era este panel y solo este.

- `PHYSICS_PT_geometry_nodes` — contexto `physics`, etiqueta `Simulation Nodes`,
  `PANEL_TYPE_DEFAULT_CLOSED`, `poll` = algún objeto seleccionado y editable con modificador
  `eModifierType_Nodes`.

**Detalle que se reproduce a propósito:** el texto de los botones cambia según
`len(context.selected_editable_objects) > 1`, **no** según cuántos de ellos tienen nodos. Es
una inconsistencia del original (el `poll` mira una cosa y el texto otra), y se copia tal cual:
cambiarla sería inventar comportamiento.

**Verificado** igual que la anterior, sobre copia del binario:

| Volcado | Resultado |
|---|---|
| Registro | **2.113 bloques, 2.112 idénticos, 1 distinto, 0 faltan, 0 sobran** |
| Diseño | **2.004 bloques, 1.983 idénticos, 21 distintos, 0 faltan, 0 sobran** — 0 de los 21 son de esta pestaña |

Las mismas cifras exactas que antes de añadirla: no mueve ni un bloque más.

## Lo siguiente, medido (para quien lo coja)

Criterio: pestaña completa, y primero las que **no** dependen de los mixins compartidos
(`PropertyPanel` de `rna_prop_ui` y `PropertiesAnimationMixin` de `bl_ui.space_properties`).
**Los dos mixins ya están extraídos** (`FL_properties_ui.hpp` +
`space_buttons/fl_properties_ui.cc`): `draw_custom_properties` es el `PropertyPanel` de
`rna_prop_ui` y `draw_action_and_slot_selector` es el `PropertiesAnimationMixin` de
`bl_ui.space_properties`. Ya no hay que reescribirlos en cada pestaña: se incluye la cabecera y
se llaman. **Ésa era la señal de salida**; a partir de aquí las demás salen casi solas.

| Fichero | Líneas | Paneles | Dependencias compartidas |
|---|---:|---:|---|
| `properties_animviz.py` | 123 | 0 (solo mixins de dibujo) | ninguna — es una **biblioteca**, no una pestaña; cae con sus usuarios |
| ~~`properties_data_metaball.py`~~ | 137 | 6 | **hecha** (sexta unidad) |
| ~~`properties_data_speaker.py`~~ | 156 | 6 | **hecha** (séptima unidad) |
| `properties_data_pointcloud.py` | 175 | 3 | **BLOQUEADA**: `UIList` con `filter_items()` propio |
| `properties_workspace.py` | 193 | 3 | **BLOQUEADA**: `UIList` con `filter_items()` propio |
| `properties_data_curves.py` | 219 | 5 | **BLOQUEADA**: `UIList` con `filter_items()` propio |
| ~~`properties_data_volume.py`~~ | 239 | 8 | **hecha** (octava unidad) |
| `properties_view_layer.py` | 300 | 10 | **la siguiente**: `PropertyPanel`, 1 menú, 1 `UIList` **sin** `filter_items` |
| ~~`properties_data_lightprobe.py`~~ | 413 | 13 | **hecha** (novena unidad) |

Quedan **24.038 líneas** en `scripts/startup/bl_ui/properties_*.py` (eran 25.412 y 43
ficheros; ahora **35**).

**La regla de las listas, dicha de una vez.** Una pestaña con un `UIList` que trae
`filter_items()` propio **no se coge**: el volcado de diseño no ejecuta ni el `draw_item()`
ni el `filter_items()` de una lista (deuda del propio volcador, `UI-A-CPP.md` §D4.2), así
que esa parte se migraría sin evidencia. Una lista que **solo** tiene `draw_item` sí se
puede coger, acotando por escrito lo que no queda medido —es lo que se hizo con
`VOLUME_UL_grids`—, porque lo que sí prueba el volcado de registro es que la lista existe
con sus callbacks exactos.

El desbloqueo de las tres marcadas es **una sola pieza, y es del volcador**: ejecutar el
`draw_item` de una lista con los datos que la escena ya tenga, en vez de inventarse un
elemento. Con `--fl-ui-scene` ya hay escenas con datos de verdad; lo que falta es que
`fl_ui_dump.cc` los recorra.

Las gordas —`properties_particle` 2.312, `properties_paint_common` 1.965,
`properties_constraint` 1.900, `properties_physics_fluid` 1.629, `properties_freestyle` 1.348—
no son candidatas a una sola sesión, y `properties_freestyle` además cae entera con Freestyle
(`INVENTARIO-PYTHON.md §2.9`).

## Cuarta unidad: la pestaña Colección, y los mixins ya extraídos

Dos cosas en un cambio, porque la segunda no se sostiene sin la primera.

**(a) Los dos mixins salen de `fl_world_buttons.cc`.** Estaban escritos en C++ pero `static`
dentro del fichero donde hicieron falta por primera vez. Ahora viven en
`space_buttons/fl_properties_ui.cc`, declarados en `FL_properties_ui.hpp`:

- `draw_custom_properties()` — el `PropertyPanel` de `rna_prop_ui`.
- `draw_action_and_slot_selector()` — el `PropertiesAnimationMixin` de `bl_ui.space_properties`.

**Es una extracción, no una reescritura**: no se tocó ni una línea del cuerpo de las dos
funciones, solo el `static` y el espacio de nombres. Por eso no mueve ni un bloque del volcado.
`fl_world_buttons.cc` era de Codex, que se quedó sin cuota a las 04:05; pasa a este carril.

**(b) `properties_collection.py`** (146 líneas, 6 paneles y 1 menú) pasa entera a
`space_buttons/fl_properties_collection.cc`, y es la primera que **usa** los mixins en vez de
copiarlos. `lineart_make_line_type_entry()` no se migra: está definida en el fichero y **no la
llama nadie**, ni allí ni en el resto del árbol.

**Las dos trampas que cazó el volcado**, y que leyendo no se veían:

1. **`toggle=False` no es el valor por defecto ni sobra.** `hide_select` tiene icono
   (`RESTRICT_SELECT_OFF`), y una booleana con icono se dibuja como `ICON_TOGGLE` salvo que se
   fuerce lo contrario. En C++ eso es `UI_ITEM_R_ICON_NEVER`. Sin él, el volcado daba
   `type=ICON_TOGGLE icon=RESTRICT_SELECT_OFF text=''` donde el Python daba
   `type=CHECKBOX icon=NONE text='Selectable'`. Afecta a `hide_select`, `hide_render`,
   `exclude`, `holdout` e `indirect_only`.
2. **El mixin `PropertyPanel` lleva `bl_order = 1000`**: las propiedades personalizadas van
   siempre al final de la pestaña. Sin ese `order`, el panel salía con `order=0` y el volcado del
   **registro** lo cantaba.

**Verificado:**

| Volcado | Resultado |
|---|---|
| Registro | **2.113 bloques, 2.112 idénticos, 1 distinto, 0 faltan, 0 sobran** |
| Diseño | **2.004 bloques, 2.003 idénticos, 1 distinto, 0 faltan, 0 sobran** |

El único distinto del diseño es `TOPBAR_MT_templates_more`, que **no es de esta pestaña** (es el
menú de plantillas de aplicación). El único distinto del registro sigue siendo
`REGION PROPERTIES WINDOW`, la deuda de orden global. **Los seis paneles y el menú de Colección
se dibujan idénticos al Python.**

## Quinta unidad: la Rejilla, y la prueba de que la extracción paga

`scripts/startup/bl_ui/properties_data_lattice.py` (105 líneas, 4 paneles) pasa entera a
`space_buttons/fl_properties_data_lattice.cc`. Es la primera que usa **los dos** ayudantes
compartidos, que es justo para lo que se extrajeron: los paneles `DATA_PT_lattice_animation` y
`DATA_PT_custom_props_lattice` son dos llamadas y sus metadatos, nada más.

Los metadatos de los mixins, que no están en el fichero de la pestaña y hay que ir a buscar:

| Mixin | `bl_label` | `bl_options` | `bl_order` |
|---|---|---|---:|
| `PropertiesAnimationMixin` | `Animation` | `DEFAULT_CLOSED` | **999** (`PropertyPanel.bl_order - 1`) |
| `PropertyPanel` | `Custom Properties` | `DEFAULT_CLOSED` | **1000** |

**Verificado:**

| Volcado | Resultado |
|---|---|
| Registro | **2.113 bloques, 2.112 idénticos, 1 distinto, 0 faltan, 0 sobran** |
| Diseño | **2.004 bloques, 2.004 idénticos, 0 distintos, 0 faltan, 0 sobran** |

**Cero diferencias de diseño en todo el editor.** El único bloque distinto del registro sigue
siendo `REGION PROPERTIES WINDOW`, la deuda de orden global.

## Sexta unidad: el Metaball, y el volcado de diseño que no medía nada

`scripts/startup/bl_ui/properties_data_metaball.py` (137 líneas, 6 paneles) pasa entera a
`space_buttons/fl_properties_data_metaball.cc`.

| Panel | Etiqueta | Banderas | `order` |
|---|---|---|---:|
| `DATA_PT_context_metaball` | (vacía) | `NO_HEADER` | 0 |
| `DATA_PT_metaball` | `Metaball` | — | 0 |
| `DATA_PT_mball_texture_space` | `Texture Space` | `DEFAULT_CLOSED` | 0 |
| `DATA_PT_metaball_element` | `Active Element` | — | 0 |
| `DATA_PT_metaball_animation` | `Animation` | `DEFAULT_CLOSED` | 999 |
| `DATA_PT_custom_props_metaball` | `Custom Properties` | `DEFAULT_CLOSED` | 1000 |

### Lo importante de esta unidad no es la pestaña: es que el verificador estaba mudo

Las cinco pestañas de anoche se dieron por buenas con «diseño 2.004 de 2.004, cero
diferencias». Hay que leer esa cifra con cuidado. La escena de fábrica tiene un cubo, una
cámara y una luz: **no tiene rejilla, ni vacío con imagen, ni metaball**. Así que en la
línea base de diseño los seis bloques de esta pestaña son, literalmente:

```
=== PANEL PROPERTIES WINDOW DATA_PT_metaball
NO-CUBIERTO motivo=poll
```

Reproducir eso en C++ es trivial —basta con que el `poll` siga diciendo que no— y no
prueba **ni un botón** del `draw()`. Es exactamente la trampa que `UI-A-CPP.md` describe
para `PresetPanel` («en instalación de fábrica la carpeta de presets no existe»), solo que
aquí afectaba a toda la familia `DATA_PT_*` de tipos de objeto que la escena de fábrica no
trae: rejilla, vacío, metaball, altavoz, nube de puntos, volumen, curvas.

O sea: el volcado de diseño certificaba **la ausencia**, no el dibujo. Lo que verificaba de
verdad esas migraciones era el volcado de **registro**, que sí es completo.

### Cómo se ha cerrado el agujero, sin regenerar ninguna línea base

El mismo volcador, sobre una escena que **sí** tiene el dato, dos veces: con el Python vivo
y con el C++ ya puesto, comparando solo los bloques de la pestaña.

```
Blender --factory-startup --python-expr "<añade el metaball y entra en edición>" \
        --fl-dump-ui-layout <salida>
```

Las dos líneas base congeladas de `tests/flipendo/ui/` **no se tocan**: siguen siendo la
foto de la escena de fábrica y siguen dando lo que daban. Esto es una medición **añadida**,
no una línea base regenerada porque saliera roja.

Se hizo con las **cinco** formas de metaelemento, porque el `draw()` del elemento activo
tiene cuatro ramas y hay que pisarlas todas:

| Escena | Bloques de la pestaña | Resultado |
|---|---:|---|
| Metaball `BALL` | 173 líneas | **idéntico** |
| Metaball `CUBE` | 203 líneas | **idéntico** |
| Metaball `CAPSULE` | 183 líneas | **idéntico** |
| Metaball `PLANE` | 193 líneas | **idéntico** |
| Metaball `ELLIPSOID` | 203 líneas | **idéntico** |

Byte a byte, con `diff`, contra el volcado del Python tomado antes de retirarlo.

**Lo que este andamio tiene de malo, y queda escrito:** monta la escena con `--python-expr`,
porque hoy no hay forma en C++ de decirle al volcador «dibuja con un metaball delante». El
día que no haya intérprete, esta comprobación deja de poder repetirse. La cura durable es
una opción `--fl-ui-scene <tipo>` en C++ (crear el objeto, activarlo y seguir con el
volcado normal), y es el primer trabajo de quien siga con las pestañas de datos: sin ella,
cada pestaña de esta familia se migra a ciegas o con muleta de Python.

### Las trampas de esta pestaña

1. **`elements.active` es `MetaBall::lastelem`, y solo existe en modo edición.**
   `ED_mball_editmball_make()` lo asigna al entrar en edición y `ED_mball_editmball_free()`
   lo borra al salir; además `mball.cc` lo pone a `nullptr` al leer un `.blend`. Por eso
   `DATA_PT_metaball_element` **no se dibuja nunca en modo objeto**, y por eso el andamio
   tiene que entrar en edición: con un metaball en modo objeto, el panel sigue saliendo
   `NO-CUBIERTO` y uno se cree que lo ha verificado. Es el comportamiento del original, no
   un efecto de la migración.
2. **La subcolumna del elemento se crea SIEMPRE.** El Python hace
   `sub = col.column(align=True)` **antes** del `if type in {...}`, así que con un `BALL`
   queda una columna vacía en el árbol. Crearla solo dentro de las ramas quita un
   `LAYOUT_COLUMN` del volcado y se nota.
3. **Los nombres del enum no son los del DNA.** `'CAPSULE'` del RNA es `MB_TUBE`, y
   `'ELLIPSOID'` es `MB_ELIPSOID` (con una sola `L`, como está escrito en
   `DNA_meta_types.h`). Los `MB_TUBEX/Y/Z` están marcados obsoletos, no salen en el enum y
   el Python no los nombra: caen al caso vacío, igual que `BALL`.

### El `draw()` del mixin de animación, también compartido

Hasta ahora `FL_properties_ui.hpp` exponía `draw_action_and_slot_selector()`, que es el
**método** del `PropertiesAnimationMixin`. Faltaba su **`draw()`**, que es lo que usan las
pestañas que no lo sobrescriben —metaball, altavoz, volumen, cámara…—: una columna
alineada con `use_property_split` y sin decorador, y dentro el selector.

Ahora está, como `draw_animation_panel()`. La trampa que trae escrita: la separación y el
decorador van **en la columna, no en el layout raíz**. En el volcado se ve enseguida —la
raíz queda `prop_sep=no prop_decorate=si` y la columna `prop_sep=si prop_decorate=no`—, y
ponerlos en la raíz cambia los dos bloques. Las pestañas que **sí** sobrescriben el `draw()`
(malla, rejilla) siguen llamando directamente al selector con su propia disposición.

### Verificado

| Volcado | Resultado |
|---|---|
| Registro (`baseline-python.txt`) | **2.113 bloques, 2.110 idénticos, 3 distintos, 0 faltan, 0 sobran** |
| Diseño (`baseline-python-layout.txt`) | **2.004 bloques, 2.004 idénticos, 0 distintos, 0 faltan, 0 sobran** |
| Diseño con metaball delante (5 escenas) | **5 de 5 idénticas**, byte a byte |

De los **3** bloques distintos del registro, **ninguno es de esta pestaña**:

- `REGION PROPERTIES WINDOW` es la deuda de orden global de siempre. Se comprobó que **es
  solo orden**: las 723 líneas que difieren son **el mismo conjunto** de paneles
  (`diff` de las dos listas ordenadas: vacío). Los seis de Metaball pasan de las posiciones
  ~40 y ~1029 a las ~4, 58, 61, 64, 97 y 127, que es justo lo que predice la trampa de
  `order` de `UI-A-CPP.md`.
- `REGION NODE_EDITOR UI` y `REGION PROPERTIES HEADER` **no son de este carril**: aparecen
  porque el árbol compartido tenía a esa hora la migración de `space_node.py` a medias
  (`fl_node_ui.cc` sin commitear), que adelanta `NODE_PT_node_color_presets`,
  `NODE_PT_annotation` y `NODE_WORLD_PT_viewport_display`. Con el árbol limpio de anoche el
  registro daba 1 distinto, no 3; la diferencia son esos dos bloques ajenos.

## Séptima unidad: el Altavoz, y las dos ramas de `muted`

`scripts/startup/bl_ui/properties_data_speaker.py` (156 líneas, 6 paneles) pasa entera a
`space_buttons/fl_properties_data_speaker.cc`.

| Panel | Etiqueta | Banderas | `order` |
|---|---|---|---:|
| `DATA_PT_context_speaker` | (vacía) | `NO_HEADER` | 0 |
| `DATA_PT_speaker` | `Sound` | — | 0 |
| `DATA_PT_distance` | `Distance` | `DEFAULT_CLOSED` | 0 |
| `DATA_PT_cone` | `Cone` | `DEFAULT_CLOSED` | 0 |
| `DATA_PT_speaker_animation` | `Animation` | `DEFAULT_CLOSED` | 999 |
| `DATA_PT_custom_props_speaker` | `Custom Properties` | `DEFAULT_CLOSED` | 1000 |

Es la primera de esta familia con **`COMPAT_ENGINES`**: los seis paneles declaran el mismo
conjunto (`BLENDER_RENDER`, `BLENDER_EEVEE_NEXT`, `BLENDER_WORKBENCH`), así que basta un
`poll` que mire `scene->r.engine`, donde lo miraba el Python y donde ya lo mira
`fl_game_buttons.cc`.

### Las trampas

1. **El `template_ID` va ANTES de `use_property_split`.** En `DATA_PT_speaker` el Python
   dibuja el selector de sonido y *después* pone `layout.use_property_split = True`. Si en
   C++ se pone la separación primero, el selector se dibuja partido y el volcado lo canta.
   El orden de las llamadas es parte del dibujo, no un detalle de estilo.
2. **`open="sound.open_mono"` es el operador de ABRIR, el tercero de la firma C++**
   (`newop`, `openop`, `unlinkop`), no el segundo. Ponerlo de `newop` cambia el botón.
3. **`active` no cae siempre en el mismo sitio.** En `DATA_PT_speaker` es
   `col.active = not speaker.muted` (sobre la columna, para que el `Mute` de arriba siga
   encendido); en `DATA_PT_distance` y `DATA_PT_cone` es `layout.active` (sobre la raíz del
   panel). Son tres `uiLayoutSetActive` en tres niveles distintos y el volcado distingue
   los tres.
4. **`slider=True` es `UI_ITEM_R_SLIDER`**, en `volume`, `volume_min`, `volume_max` y
   `cone_volume_outer` — y **no** en `attenuation`, `pitch` ni los dos ángulos del cono.

### Verificado

Medido sobre una copia del bundle recién instalado, sin huérfanos:

| Volcado | Resultado |
|---|---|
| Registro (`baseline-python.txt`) | **2.113 bloques, 2.110 idénticos, 3 distintos, 0 faltan, 0 sobran** |
| Diseño (`baseline-python-layout.txt`) | **2.004 bloques, 2.004 idénticos, 0 distintos, 0 faltan, 0 sobran** |
| Diseño con un altavoz delante, `muted=False` | **148 líneas, 6 de 6 paneles dibujados: idéntico** |
| Diseño con un altavoz delante, `muted=True` | **148 líneas, 6 de 6 paneles dibujados: idéntico** |

Los 3 bloques distintos del registro son los mismos tres de la unidad anterior y ninguno es
de esta pestaña: `REGION PROPERTIES WINDOW` (la deuda de orden global) y
`REGION NODE_EDITOR UI` + `REGION PROPERTIES HEADER`, que vienen de la migración de
`space_node.py` que otro carril tenía a medias en el árbol compartido.

**Las dos escenas hacían falta**: entre `muted=False` y `muted=True` cambian 6 líneas del
volcado, en 3 bloques. Medir solo el caso por defecto habría dejado sin probar los tres
`uiLayoutSetActive`, que es justo donde estaba el riesgo.

## `--fl-ui-scene`: el andamio que hace medible a toda esta familia

Las dos unidades anteriores dejaron el problema señalado: para las pestañas de datos, el
volcado de diseño sobre la escena de fábrica **certifica la ausencia, no el dibujo**. Y la
medición que lo arreglaba montaba la escena con `--python-expr`, o sea con la muleta que
este proyecto está quitando.

`source/blender/editors/space_buttons/fl_properties_ui_scene.cc` la sustituye:

```
Blender --factory-startup --fl-ui-scene METABALL:CUBE --fl-dump-ui-layout <salida>
Blender --factory-startup --fl-ui-scene SPEAKER:MUTED --fl-dump-ui-layout <salida>
```

Familias: `METABALL[:BALL|CAPSULE|PLANE|ELLIPSOID|CUBE]`, `SPEAKER[:MUTED]`, `LATTICE`,
`VOLUME`, `CURVES`. Se declaran en `FL_properties_ui.hpp` y la opción se registra en
`ARG_PASS_FINAL`, así que corre **antes** del volcado si va antes en la línea de órdenes.

**No toca el volcador.** `fl_ui_dump.cc` es de otro carril y no hace falta cambiarlo: el
andamio solo deja la escena en el estado en que esos `poll` dicen que sí, y el volcado de
siempre hace el resto.

### Las tres decisiones que lo hacen honesto

1. **Monta el dato llamando a los mismos operadores que llamaría el usuario**
   (`OBJECT_OT_metaball_add` + `OBJECT_OT_editmode_toggle`, `OBJECT_OT_speaker_add`,
   `OBJECT_OT_add(type='LATTICE')`, `OBJECT_OT_volume_add`,
   `OBJECT_OT_curves_empty_hair_add`), **no fabricándolo a mano**.
   `ED_mball_add_primitive()` escala el elemento por el diámetro de la vista, así que un
   metaball construido con `BKE_mball_element_add()` daría números distintos y la
   comparación byte a byte fallaría por el andamio, no por el código.
2. **Grita y sale con error** si el operador no existe, no termina, o la familia no se
   reconoce. Un andamio que falla en silencio deja la pestaña sin cubrir y el volcado
   vuelve a decir `NO-CUBIERTO motivo=poll`: un falso verde indistinguible del bueno.
3. **Se verificó contra lo que sustituye.** El criterio no es «funciona», es «da
   exactamente lo mismo que el andamio de Python que ya se había usado para certificar
   Metaball y Altavoz».

### Y ya se ha cobrado su primera pieza

La primera versión usaba `CTX_data_active_object()` para comprobar que el objeto había
quedado activo. En `--background` funcionaba; **en modo gráfico devolvía `nullptr`**, porque
cuando corre esta opción todavía no hay área activa en el contexto y esa ruta pasa por el
callback de contexto de la pantalla. O sea: el caso que importa —el volcado de diseño, que
necesita modo gráfico— era justo el que fallaba, y el barato de probar era el que pasaba.

Sin la regla 2 esto habría salido como un volcado sin los bloques de la pestaña, que al
compararlo contra un extracto vacío habría dado «0 diferencias». Con ella salió como
`fl-ui-scene: el altavoz no quedo activo` y código de salida 1. La cura: mirar la capa de
vista (`BKE_view_layer_synced_ensure` + `BKE_view_layer_active_object_get`), que no depende
de la pantalla.

**Lección para el resto del árbol:** un arnés probado solo en `--background` no está
probado. En modo gráfico el contexto tiene ventana, pantalla y área, y en la línea de
órdenes esas tres cosas no están todas puestas todavía.

### Verificado, contra el andamio que sustituye

Siete escenas, comparando los bloques de la pestaña byte a byte contra el volcado que había
producido el andamio de `--python-expr`:

| Escena | Bloques | Resultado |
|---|---:|---|
| `METABALL:BALL` | 173 líneas | **idéntico** |
| `METABALL:CUBE` | 203 líneas | **idéntico** |
| `METABALL:CAPSULE` | 183 líneas | **idéntico** |
| `METABALL:PLANE` | 193 líneas | **idéntico** |
| `METABALL:ELLIPSOID` | 203 líneas | **idéntico** |
| `SPEAKER` | 148 líneas | **idéntico** |
| `SPEAKER:MUTED` | 148 líneas | **idéntico** |

Y que **detecta**: con una familia inventada (`--fl-ui-scene DESCONOCIDA`) escribe la lista
de familias conocidas y sale con código 1, en vez de seguir y volcar una escena de fábrica
que parecería correcta.

Prueba de humo de las otras tres familias en `--background`: `LATTICE`, `VOLUME` y `CURVES`
dejan su objeto activo (tipos 22, 29 y 27). **Lo que no está medido y se dice**: sus
pestañas todavía no se han comparado contra nada, porque todavía no se han migrado; el
andamio está listo para cuando se haga.

### Cómo se registra la opción, y cómo acabó entrando

`source/creator/creator_args.cc` tenía, cuando se escribió el andamio, trabajo a medias de
otro carril (un `--fl-make-ui-scene` propio). Con el índice compartido, commitear ese
fichero habría metido el trabajo ajeno en el commit del andamio con el mensaje equivocado,
que es el accidente de las 01:05. Así que el alta se dejó escrita y verificada en el árbol
de trabajo pero **fuera** del commit `ca64307f2e5`.

**Acabó entrando media hora después, en `69270a8f633`** («Interfaz: la escena rica del arnés,
y el defecto que ha destapado»), porque el otro carril commiteó ese fichero y se llevó estas
tres piezas dentro. Es el riesgo que se había anotado: con el índice compartido, lo que
dejas en un fichero ajeno lo commitea quien pase después. **No es un problema de contenido**
—estaba probado antes de dejarlo ahí— pero sí de autoría, y por eso queda dicho. Las tres
piezas, para quien las busque:

```cpp
#  include "FL_properties_ui.hpp"                       /* con los demás FL_*. */

/* ...el manejador, junto a arg_handle_fl_dump_ui... */
static int arg_handle_fl_ui_scene(int argc, const char **argv, void *data)
{
  bContext *C = static_cast<bContext *>(data);
  if (argc > 1) {
    if (!flipendo::properties_ui::scene_setup(C, argv[1])) {
      fprintf(stderr, "\nError: --fl-ui-scene no pudo montar '%s'.\n", argv[1]);
      WM_exit(C, EXIT_FAILURE);
    }
    return 1;
  }
  fprintf(stderr, "\nError: falta la especificacion de escena despues de '%s'.\n", argv[0]);
  return 0;
}

/* ...y el alta, en ARG_PASS_FINAL, antes de la de --fl-dump-ui: */
BLI_args_add(ba, nullptr, "--fl-ui-scene", CB(arg_handle_fl_ui_scene), C);
```

**Y cómo convive con `--fl-make-ui-scene`**, que otro carril estaba escribiendo a la vez: no
son lo mismo y no se estorban. Aquél construye **una** escena rica (cámara, luz, curva,
texto, vacío, esqueleto en pose, partículas, el cubo en edición) y la guarda en un `.blend`.
Éste parametriza **por tipo de objeto** y lo deja **activo**, que es lo que exigen las
pestañas de datos: `context.meta_ball`, `context.speaker` y compañía salen del objeto
activo, y activo solo puede haber uno. Una escena rica no puede tener a la vez el metaball y
el altavoz de activos; por eso hacen falta las dos herramientas.

## Aviso al cerrar el turno: la línea base de diseño lleva dentro los ficheros recientes

La comprobación final del turno, sobre el árbol ya entregado, dio **dos** diferencias de
diseño que no existían hora y media antes:

```
DIFIERE MENU TOPBAR_MT_file_open_recent
DIFIERE MENU WM_MT_splash
Interfaz (diseno): 2004 bloques, 2002 identicos, 2 distintos, 0 faltan, 0 sobran.
```

**No son de ninguna migración.** Los dos bloques dibujan la **lista de ficheros recientes**
del usuario, con sus rutas absolutas dentro del argumento del operador:

```
BUTTON ... op='bpy.ops.wm.open_mainfile(filepath="/Users/jesussolaz/Flipendo/game/anima/T1_LaMancha.blend", ...)' text='T1_LaMancha.blend' ...
```

Y esa lista vive en `~/Library/Application Support/UPBGE/4.5/config/recent-files.txt`, que
**`--factory-startup` no reinicia**. Cualquier Blender que abra o guarde un `.blend` en esta
máquina —el de otro carril, o el propio arnés de alguien— empuja tres entradas nuevas
arriba y desplaza las demás. Comprobado: el fichero se tocó a las 07:16 y ahora encabeza con
tres `rica-*.blend` que no estaban cuando se congeló la línea base.

Es **el mismo defecto que D6** («la línea base no puede llevar dentro dónde está instalado el
programa») y que la fecha de compilación del menú «Acerca de»: *lo que no es del árbol, no
entra en la línea base*. Aquí lo que se cuela no es la ruta de instalación sino el historial
del usuario, que además cambia solo.

**Consecuencia práctica, y por eso queda escrito:** quien mida a partir de ahora se va a
encontrar dos diferencias en rojo que no ha causado él. La tentación es regenerar la línea
base, y eso taparía el problema en vez de arreglarlo —el sello de goma otra vez—. La cura es
la misma que se aplicó a las rutas de presets: que el volcado **sustituya la lista de
recientes por una marca**, o que estos dos menús se declaren dependientes del entorno y se
excluyan diciéndolo. El fichero del volcador (`interface/fl_ui_dump.cc`) es de otro carril,
así que aquí queda el diagnóstico con su prueba, no el parche.

## Octava unidad: el Volumen, y las variantes que hacen medible cada `if`

`scripts/startup/bl_ui/properties_data_volume.py` (239 líneas, 8 paneles y la lista
`VOLUME_UL_grids`) pasa entera a `space_buttons/fl_properties_data_volume.cc`.

| Panel | Etiqueta | Banderas | `order` |
|---|---|---|---:|
| `DATA_PT_context_volume` | (vacía) | `NO_HEADER` | 0 |
| `DATA_PT_volume_grids` | `Grids` | — | 0 |
| `DATA_PT_volume_file` | `OpenVDB File` | — | 0 |
| `DATA_PT_volume_viewport_display` | `Viewport Display` | — | 0 |
| `DATA_PT_volume_viewport_display_slicing` | (vacía), hija de la anterior | — | 0 |
| `DATA_PT_volume_render` | `Render` | — | 0 |
| `DATA_PT_volume_animation` | `Animation` | `DEFAULT_CLOSED` | 999 |
| `DATA_PT_custom_props_volume` | `Custom Properties` | `DEFAULT_CLOSED` | 1000 |

Como Altavoz, lleva `COMPAT_ENGINES` en los ocho paneles, así que basta un `poll`. **Ojo
a la diferencia con la Sonda de Luz**: aquí el Python mira `context.scene.render.engine`
**crudo**; allí mira `context.engine`, que no es lo mismo (ver la novena unidad).

### Las trampas

1. **`VolumeGrids` usa el propio `Volume` como dato** (`RNA_def_struct_sdna(srna,
   "Volume")`), igual que `SpaceUVEditor` con `SpaceImage`. El `volume.grids` del
   `template_list` es `RNA_pointer_create_discrete(&volume->id, &RNA_VolumeGrids, volume)`.
2. **`use_slice` es una prueba de bit, no una comparación.** Su
   `RNA_def_property_boolean_sdna` lo saca de `axis_slice_method` con la máscara
   `VOLUME_AXIS_SLICE_SINGLE`.
3. **La separación de propiedad se enciende DENTRO del `if volume.filepath`.**
   Encenderla siempre cambia el `prop_sep` del `LAYOUT_ROOT` y el bloque entero sale
   distinto — justo en el caso normal, que es el volumen vacío.
4. **Los defectos de `template_list` hay que ir a buscarlos a `rna_ui_api.cc`**:
   `rows=3` pero `maxrows=5` y `columns=9`. Poner `columns=0` cambia el árbol.
5. **`layout.active` y `sub.active` caen en niveles distintos**: en el panel de corte va
   en la **raíz**; en la malla de alambre va en la **fila**, no en la columna (en la
   columna apagaría también el `wireframe_type` de arriba).

### Verificado

| Volcado | Resultado |
|---|---|
| Registro (`baseline-python.txt`) | **2.113 bloques, 2.112 idénticos, 1 distinto, 0 faltan, 0 sobran** |
| Diseño (`baseline-python-layout.txt`) | **2.004 bloques, 2.001 idénticos, 3 distintos, 0 faltan, 0 sobran** |
| Diseño con un volumen delante, 4 escenas | **4 de 4 idénticas**, byte a byte |

Las cuatro escenas (`VOLUME`, `:SLICE`, `:WIRE_NONE`, `:SEQUENCE`) hacían falta: sin las
dos primeras quedaban sin probar los dos `active`, y sin `:SEQUENCE` las tres ramas del
panel de fichero. **`VOLUME:SEQUENCE` es la que más paga**: con una ruta fija que no
existe, `BKE_volume_load` deja un mensaje de error, así que un solo volcado cubre ruta
vacía, secuencia y error sin meter ningún `.vdb` en el repositorio.

### Lo que NO queda verificado, y se dice

`VOLUME_UL_grids.draw_item` no lo ejecuta ningún volcado, y además un volumen sin `.vdb`
no tiene ni una rejilla que dibujar. Lo que sí queda probado es el bloque
`UILIST VOLUME_UL_grids` del registro con sus callbacks (`draw_item=si`, `draw_filter=no`,
`filter_items=no`, `listener=no`), exactamente los del Python. Y la rama de Cycles de
`DATA_PT_volume_render`, que sin el addon no pisa nadie: se copia tal cual en vez de
borrarla.

## Novena unidad: la Sonda de Luz, y `layout.prop()` no es `uiLayout::prop()`

`scripts/startup/bl_ui/properties_data_lightprobe.py` (413 líneas, 13 paneles, **cero**
listas y **cero** menús) pasa entera a `space_buttons/fl_properties_data_lightprobe.cc`.
Es la pestaña completa más grande sin lista ni menú que quedaba, y su única dependencia
compartida —`PropertiesAnimationMixin`— ya estaba extraída.

### La trampa gorda, que es general y le va a tocar a más pestañas

**`layout.prop()` del Python y `uiLayout::prop()` del C++ NO hacen lo mismo cuando la
propiedad no existe en el struct:**

- el Python (`rna_uiItemR`) avisa por consola y **no dibuja nada**;
- el C++ (`uiLayout::prop`) dibuja una **etiqueta deshabilitada con el identificador
  crudo** (`ui_item_disabled`).

Y aquí pasa de verdad. `LightProbe` tiene `RNA_def_struct_refine_func`, así que el puntero
se refina a `LightProbeSphere`, `LightProbePlane` o `LightProbeVolume` — y los cuatro
subpaneles de horneado piden propiedades que **solo existen en el de volumen**
(`capture_distance`, `resolution_x`, `clamp_direct`, `surface_bias`…) mientras su `poll`
**no mira el tipo**. Con una esfera delante, el Python dibuja las columnas y ni un botón;
el primer C++ metía **13 etiquetas deshabilitadas de más** en cada escena de esfera y de
plano.

La cura es un `prop_py()` local de tres líneas que comprueba `RNA_struct_find_property` y
calla si no está — o sea, la semántica del Python. Las 65 llamadas del fichero pasan por
él.

**Quien migre cualquier pestaña con `RNA_def_struct_refine_func` detrás se va a encontrar
esto** (materiales, luces, curvas…), y **sin el volcado de diseño sobre una escena que
tenga el dato no lo vería**: con la escena de fábrica los cuatro paneles salen
`NO-CUBIERTO` y las 13 etiquetas de más no aparecen en ninguna cifra.

### Las otras trampas

1. **`context.engine` no es `scene.render.engine`.** Es
   `CTX_data_engine_type(C)->idname`, o sea `RE_engines_find(scene->r.engine)`, que **cae a
   EEVEE** si el nombre guardado no corresponde a ningún motor registrado. Son dos `poll`
   distintos y cada fichero usa el suyo. (`bf_editor_space_buttons` gana `../../render` en
   su `INC` solo por la cabecera `RE_engine.h`; no gana dependencia de enlace.)
2. **`RNA_pointer_create_discrete()` ya refina.** El volcado escribe
   `rna='LightProbeSphere.influence_type[0]'`, no `LightProbe.…`, y sale solo. Conviene
   saberlo antes de perseguirlo.
3. **`subset = 'ACTIVE'` vale 2, no 0** (`ALL`=0, `SELECTED`=1). Se pone por identificador
   con `RNA_enum_set_identifier`, que además no necesita contexto porque el enum es
   estático. Escribir el número a ojo deja ahí un `subset='ALL'` que hornea la escena entera.
4. **El último `sub = col.column(align=True)` de `DATA_PT_lightprobe` está FUERA del `if`**
   y cuelga de la columna que haya creado la rama tomada. Meterlo en las ramas cambia el
   árbol.
5. **El orden del array de `PanelDecl` es el de la tupla `classes`**, no el del fichero: en
   la sonda, `_bake_clamping` va **antes** que `_bake_offset` en `classes` y al revés en el
   texto. Ese orden es el que desempata dentro de la región.

### Tres paneles que no dibuja nadie, y se migran igual

`DATA_PT_lightprobe`, `DATA_PT_lightprobe_visibility` y `DATA_PT_lightprobe_display`
declaran `COMPAT_ENGINES = {'BLENDER_RENDER'}`, y **`BLENDER_RENDER` no existe** como
motor en 4.5: los únicos registrados son `BLENDER_EEVEE_NEXT` y `BLENDER_WORKBENCH`. Su
`poll` dice que no en cualquier escena, con Python y con C++. Se migran línea a línea y con
el mismo `COMPAT_ENGINES` —la doctrina es migrar, no decidir por el original que su código
sobra—, pero **su `draw()` no lo cubre ningún volcado y eso queda escrito** en vez de dado
por bueno. Lo que sí queda probado de ellos es el registro.

### Verificado

| Volcado | Resultado |
|---|---|
| Registro (`baseline-python.txt`) | **2.113 bloques, 2.112 idénticos, 1 distinto, 0 faltan, 0 sobran** |
| Diseño (`baseline-python-layout.txt`) | **2.004 bloques, 2.001 idénticos, 3 distintos, 0 faltan, 0 sobran** |
| Diseño con una sonda delante, 6 escenas | **6 de 6 idénticas**, byte a byte (10 de 13 paneles dibujados) |

El único distinto del registro es `REGION PROPERTIES WINDOW`, y **se comprobó que es solo
orden antes de absorberlo**: las dos listas tienen 748 entradas y son el mismo conjunto.
Los 3 distintos del diseño se probaron ajenos midiendo el **mismo fichero base con el
bundle anterior, con el Python todavía vivo**: salen los mismos.

## Dos avisos para quien mida después

### La carpeta de presets del usuario también se cuela en la línea base

Tercer trozo de estado del usuario que aparece dentro del volcado de diseño, después de la
ruta de instalación y de los ficheros recientes: `RENDER_PT_format_presets` enumera
**también** `~/Library/Application Support/UPBGE/4.5/scripts/presets/`, y
`--factory-startup` no la reinicia. Con los `.fpreset` de prueba de otro carril dentro, ese
bloque sale rojo; cuando se quitaron, volvió a salir limpio. **No está en el árbol**: el
`presets/render/` del repositorio y el del bundle tienen los mismos 14 ficheros.

### `STATUSBAR_HT_header` NO se reproduce a sí mismo

Tres pasadas seguidas del mismo verificador, con el **mismo binario** y la **misma
escena**:

| Pasada | Resultado |
|---|---|
| 1 | 2.004 bloques, **3 distintos** |
| 2 | 2.004 bloques, **4 distintos** — aparece `STATUSBAR_HT_header` |
| 3 | 2.004 bloques, **3 distintos** |

La barra de estado dibuja la memoria usada, que cambia entre ejecuciones. Por la regla del
REGLAMENTO —«si un volcado no se reproduce a sí mismo tres veces seguidas, no es una línea
base»—, **ese bloque no es línea base**: es ruido, y quien mida se va a encontrar un rojo
aleatorio que no ha causado.

**No se ha regenerado nada.** La cura es la misma que ya se aplicó a las rutas de
instalación y a los recientes: que el volcador elida el valor y conserve la forma. El
fichero del volcador (`interface/fl_ui_dump.cc`) es de otro carril, así que aquí queda el
diagnóstico con su prueba, no el parche.

## La siguiente candidata, para quien retome esto

**`properties_view_layer.py`** — 300 líneas, 10 paneles, 1 menú (`VIEWLAYER_MT_lightgroup_sync`)
y 1 lista (`VIEWLAYER_UL_aov`) **sin `filter_items()` propio**, o sea del tipo que sí se
puede coger acotando por escrito lo que no queda medido.

Lo que le hace falta, y no lo tiene todavía:

- **No necesita andamio nuevo**: sus paneles pollan sobre `context.view_layer`, que la
  escena de fábrica ya tiene, así que el volcado de diseño **ya los cubre**. Compruébalo
  antes de nada extrayendo sus bloques de `tests/flipendo/ui/baseline-python-layout.txt`:
  si salen `DRAW` y no `NO-CUBIERTO`, la línea base congelada ya es la evidencia y no hay
  que montar nada.
- Sus tres ayudantes (`ViewLayerAOVPanelHelper`, `ViewLayerCryptomattePanelHelper`,
  `ViewLayerLightgroupsPanelHelper`) son **locales al fichero**, no mixins compartidos: no
  hay nada que extraer antes.
- `VIEWLAYER_PT_layer_custom_props` hereda `PropertyPanel` a secas (sin
  `ViewLayerButtonsPanel`), así que su `poll` es el del mixin y su `_context_path` es
  `"view_layer"`. Cuidado al copiar el `poll` de los otros nueve.
- Ojo con `COMPAT_ENGINES`: los paneles `_eevee_next_` y `_workbench_` declaran motores
  distintos, y aquí el Python mira `context.engine` (como la sonda), no
  `scene.render.engine` (como el volumen).
