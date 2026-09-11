# El sistema de herramientas: de Python a C++

> ~8.300 líneas de Python. El segundo bloqueo del cero-Python, después del keymap: es
> el otro sitio donde **C++ llama de vuelta al intérprete**.

## Qué hay hoy

| Pieza | Dónde | Tamaño |
|---|---|---|
| Armazón | `bl_ui/space_toolsystem_common.py` | 1.249 |
| Catálogo | `bl_ui/space_toolsystem_toolbar.py` | 3.752 — 160 herramientas, 4 espacios, 33 modos |
| Operadores | `bl_operators/wm.py:2005-2446` | 757 |
| Keymap de la barra | `bl_keymap_utils/keymap_from_toolbar.py` | 394 |

Resultado observable, congelado en `tests/flipendo/toolsystem/baseline-python.txt`:
**425 entradas de herramienta en 30 combinaciones de espacio y modo**, 124
identificadores distintos.

## No hay ningún bloqueo técnico

El estado de la herramienta activa **ya es 100% C++**: `bToolRef` y `bToolRef_Runtime`
son DNA, y `WM_toolsystem_*` tiene 30 entradas. Lo que está en Python es la **tabla** y
las **consultas** sobre ella. Por eso hoy el motor invierte el control: para activar
una herramienta, C++ busca un operador de Python y lo invoca.

Toda la API de dibujo que usa el Python existe ya en C++ y está verificada
(`operator_menu_hold` es `uiItemFullOMenuHold_ptr`, `depress` es `UI_ITEM_O_DEPRESS`…).

## Nueve puentes, no tres

C++ pregunta a Python **construyendo cadenas de código y evaluándolas**:

| Sitio | Qué pregunta |
|---|---|
| `wm_toolsystem.cc:926` | invoca `WM_OT_tool_set_by_id` |
| `wm_toolsystem.cc:985` | invoca `WM_OT_tool_set_by_brush_type` |
| `interface_query.cc:146` | cachea el `wmOperatorType` de `tool_set_by_id` |
| `interface_context_menu.cc:405` | `item_from_id(...).label` |
| `interface_region_tooltip.cc:526` | `item_from_id(...).label` |
| `interface_region_tooltip.cc:578` | `description_from_id(...)` |
| `interface_region_tooltip.cc:641` | `keymap_from_toolbar.generate(...)` — **fabrica un keymap entero en cada apertura** |
| `interface_region_tooltip.cc:703` | los `idname` del grupo |
| `interface_region_tooltip.cc:775` | `keymap_from_id(...)` |

Y además **seis módulos de UI en Python** consumen el armazón (~25 puntos), así que
migrarlo de golpe arrastra `space_view3d_toolbar.py`, `properties_paint_common.py`,
`space_topbar.py` y compañía.

## La trampa que más caro habría salido

El nombre del keymap de una herramienta **no se puede calcular**. Parece que sí: se
sintetiza con `"{prefijo} {modo}, {etiqueta}"`. Pero se guarda **mutando una lista
compartida**, así que gana **el primer modo que registra la herramienta** y los demás
heredan ese nombre.

Comprobado contra la línea base: `builtin.radius` aparece en Edit Curve, Edit Curves y
Edit Grease Pencil, y **los tres usan `"3D View Tool: Edit Curve, Radius"`**. Una
implementación que calculara el nombre por modo generaría dos keymaps que no existen, y
esas herramientas se quedarían sin atajos **sin que nada avisara**.

→ **El nombre va como literal en la tabla.** Se transliteran los que ya existen en
`source/blender/windowmanager/keymap/fl_keymap_g*.cc`. Los 119 keymaps que referencian
las herramientas ya están todos migrados.

## Lo que es dato y lo que es código

De los 82 `draw_settings`, solo **6 tienen flujo de control**. El resto son
declarativos: N × `layout.prop` sobre una o dos fuentes, con argumentos opcionales.
Van a una tabla de filas que interpreta una única función genérica.

Lo que no cabe en tabla y hay que escribir a mano:

- **7 herramientas de partículas generadas en caliente** desde
  `rna_enum_particle_edit_hair_brush_items`. Ojo con `identifier` frente a `name`: el
  primero va a `data_block` (lo leen `wm_toolsystem.cc:340` y `:671`), el segundo forma
  el `idname` (`:673`). Confundirlos rompe el viaje de ida y vuelta.
- **9 entradas dinámicas** en las listas de modo (7 lambdas + 2 llamables) que deciden
  qué herramientas se ven según el contexto.
- **7 descripciones como función**, que leen el keymap **del usuario** para meter el
  atajo dentro del texto.
- **6 `draw_cursor`** que se registran y desregistran al cambiar de herramienta.
- El **keymap de la barra**, que se fabrica entero en cada apertura.

## Decisiones tomadas

**El nombre del keymap es un literal en la tabla.** Ver arriba.

**`data_block` está vivo.** Un informe lo dio por muerto; lo usan 7 herramientas y el
motor lo lee y lo escribe en cuatro sitios.

**Puente incremental.** `space_toolsystem_common.py` queda como un cascarón de ~80
líneas que reenvía al registro nativo por RNA. Así los seis módulos de UI y los seis
`eval` de C++ siguen funcionando mientras se migran, en vez de cambiar 25 puntos a la
vez.

**El contrato cubre las cuatro consultas que C++ hace hoy a Python**
(`tool_label_for_id`, `tool_description_for_id`, `tool_group_idnames_for_id`,
`tool_keymap_for_id`). Es la diferencia entre «los operadores ya son C++» y «el
subsistema ya no llama a Python».

**Traducciones desde el primer día.** `label` y `description` envueltos en `N_`/`IFACE_`,
o el catálogo i18n pierde 425 cadenas.

**El estado de grupo activo** (qué variante de cada grupo se usó la última vez) no va a
DNA: es un `Map<idname, índice>` por espacio, no serializado, igual que el diccionario
de clase que sustituye.

## Lo que se pierde, y es decisión

- `bpy.utils.register_tool`: sin intérprete no hay herramientas de terceros. Los 160
  builtins no se ven afectados.
- La extracción de traducciones, que hoy recorre el catálogo desde Python, necesitará
  su equivalente nativo.

## Orden

1. **Contrato y registro** — la tabla de las 160 herramientas y las consultas. Todo lo
   demás cuelga de esto: sin la tabla, las piezas compilan pero el editor arranca sin
   herramientas.
2. **Operadores** — los seis, único llamante de la activación.
3. **Cortar los nueve puentes** — dejan de evaluar cadenas y llaman al registro.
4. **Dibujo de la barra** — sobre `FL_ui_registry`, que ya existe.

Se verifica contra `tests/flipendo/toolsystem/baseline-python.txt`, igual que el keymap
contra el suyo.

---

## Lo verificado al empezar la fase 1

### La línea base estaba mal, y por mi culpa

La primera versión decía 425 entradas. El editor de nodos salía con sus 9 herramientas
**dos veces**. El fallo no estaba en el catálogo sino en cómo lo enumeraba mi script:
`tools_from_context` recorre `(cls._tools[None], cls._tools.get(mode, ()))`, y el editor
de nodos no tiene modos — su tabla solo tiene la clave `None`. Preguntarle con
`mode=None` recorre la misma lista dos veces.

En uso real no pasa, porque allí el modo es `space_data.tree_type`, que nunca es clave.
Corregido: **416 entradas, 30 combinaciones, 124 idnames**.

La lección que queda en el código: un espacio sin modos **no se modela como "un modo
llamado `None`"**. En `ToolbarDecl::modes` la lista común y la del modo son entradas
distintas y la del modo se busca por nombre, así que la duplicación no puede repetirse.

### El verificador se verificó al revés

`--fl-check-tools` compara el catálogo nativo contra la línea base. Antes de confiar en
su "0 diferencias" se le pasó una línea base alterada a propósito: la caza, señala la
línea y devuelve código de salida 1. Un verificador que solo sabe decir que sí no
verifica nada.

Dos propiedades que no tenía el del keymap:

- **Es incremental.** Compara solo las secciones ya trasladadas y lista aparte las que
  faltan, así que sirve desde la primera herramienta en vez de dar rojo hasta el final.
- **Corre en `--background`.** La generación con Python no puede: allí el campo `keymap`
  sigue siendo un objeto función hasta que arranca el modo gráfico. En el catálogo
  nativo es un literal. Eso lo hace apto para CI.

### La deuda de ajustes no se puede perder sola

`draw_settings` a `nullptr` es indistinguible de "esta herramienta no tiene ajustes".
Así es exactamente como se pierde una capacidad sin que nadie se entere. Por eso existe
`ToolDecl::settings_pending`: marca las que sí tienen ajustes en el Python pero aún no
se han trasladado, y el verificador las lista en cada pasada. La fase de dibujo
comprobará que no quede ninguna puesta.

Las tres primeras son las de anotación, y el motivo es real, no pereza: dentro dibujan
un popover al panel `TOPBAR_PT_annotation_layers`, que todavía es un panel de Python.
Dependen de la migración de `bl_ui`, no de esta.

### Generar en vez de copiar

`generate_from_enum_ex` se usa en un solo sitio: los siete pinceles del modo de
partículas, sacados de `ParticleEdit.tool`. Se generan desde la enumeración RNA en vez
de copiarse a mano, porque una tabla copiada se desincroniza en silencio si la
enumeración cambia.

La enumeración usa sus dos campos para cosas distintas, y la línea base lo confirma:

| campo | de dónde sale | ejemplo |
|---|---|---|
| `idname` | prefijo + `enum.name` | `builtin_brush.Comb` |
| `icon` | prefijo + `identifier` en minúsculas | `brush.particle.comb` |
| `data_block` | `enum.identifier` | `COMB` |

Confundir `name` con `identifier` da idnames que no existen y rompe la sincronización
con el pincel activo, que el motor lee y escribe.

### Los siete `lambda` caben en el contrato

Los filtros de contexto del Python son todos de la forma
`lambda context: (herramientas) if poll(context) else ()`, que es exactamente
`ToolEntry::poll`. El único matiz: un bloque filtrado puede contener varias entradas
(cursor, separador y las de transformación, en `PAINT_WEIGHT`), y entonces el mismo
filtro se repite en cada una, porque se aplica por entrada.

---

## Fase 2: la activación, en C++

Cambiar de herramienta ya no pasa por Python. Antes el motor buscaba el operador
`wm.tool_set_by_id` de Python y lo invocaba; el operador calculaba once argumentos y se
los devolvía al motor por `tool.setup()`, que ya era C++. Ahora esos once argumentos se
calculan en `toolsystem/fl_toolsystem_activate.cc`, y los tres operadores
(`tool_set_by_id`, `tool_set_by_index`, `tool_set_by_brush_type`) son C++
(`intern/wm_tool_ops.cc`), con el mismo identificador, propiedades y banderas, porque
los nombran 148 atajos y cada botón de la barra.

Dos de los nueve puentes quedan cortados: `wm_toolsystem.cc:926` y `:985`.

### Cómo se verificó

Tres niveles, cada uno cubriendo lo que el anterior no ve:

1. **El cálculo**, contra el Python real. `tests/flipendo/toolsystem/activation-python.txt`
   se sacó interceptando `tool.setup()` en el `_activate_by_item` de verdad, para cada
   herramienta de cada modo, más las 58 activaciones como reserva. El nativo la
   reproduce **474/474, byte a byte** (`--fl-dump-tool-activation`).
2. **La aplicación**, diferencial. `smoke_tools_gui.py` activa cada herramienta por el
   camino nativo y por el Python antiguo —que sigue cargado— sobre el mismo motor y el
   mismo estado, y compara el resultado real: **0 diferencias** en las 20 herramientas
   del modo objeto y las 43 de edición de malla.
3. **En ejecución**: ciclo, posición, reserva, tipo de pincel, y reabrir un fichero
   guardado (`check_reopen_gui.py`). Además se abrieron Molino, MolinoInterior y
   T1_LaMancha de ÁNIMA.

### Tres cosas que la línea base sola no habría enseñado

**La reserva se borra, y es correcto.** Tras activar, 11 de 20 herramientas quedaban
con `idname_fallback` vacío aunque el cálculo decía `builtin.select`. No era un fallo:
`WM_toolsystem_ref_set_from_runtime` borra la reserva de toda herramienta que no use el
keymap de reserva (ni opción `KEYMAP_FALLBACK` ni gizmo con esa bandera). El fallo era
de la prueba, que comparaba el valor final en DNA con el argumento de `setup()`. Por
eso la prueba buena es la diferencial: compara lo que queda de verdad.

**La memoria de grupos estaba partida.** En el Python quien anota «qué variante de cada
grupo se usó» es el *dibujo* de la barra, que sigue siendo Python y escribe en su
diccionario. La activación nativa leía un mapa que nadie actualizaba, y el efecto era
visible: con Select Box activa, la W saltaba a Tweak en vez de pasar a Select Circle.
La prueba en ejecución falló exactamente ahí antes del arreglo — se ejecutó a propósito
contra el binario sin arreglar, porque una prueba que nunca ha fallado no demuestra
nada. Ahora la activación hace lo que hacía el dibujo. Diferencia, a favor: el Python
solo lo anotaba con la barra a la vista.

**El arranque cambia.** Antes, en el primer instante del arranque
`WM_toolsystem_ref_set_by_id_ex` devolvía nulo porque el operador de Python aún no
existía. Ahora la herramienta se activa desde el principio. Comprobado que abrir un
fichero restaura herramienta *y* runtime (gizmo, keymap), no solo el nombre.

### Lo que se replica a propósito, aunque parezca un error

- `activate_by_id_or_cycle` recibe `as_fallback` y lo ignora, como el Python.
- El tipo de pincel se busca con `context.mode` en todos los espacios, también en el
  editor de imagen, cuyos modos se llaman de otra forma; allí solo ve las comunes.
- La reserva se localiza usando una posición *dentro del grupo* como posición *de
  entrada*. Funciona porque el grupo de selección es siempre la primera entrada.

Arreglar cualquiera de las tres cambiaría qué herramienta o qué teclas obtiene el
usuario, y eso no es una migración.

### Quedan

Siete puentes: las cinco consultas del tooltip y del menú contextual (fase 3), el keymap
de la barra que el tooltip fabrica en cada apertura, y el dibujo de la barra (fase 4).
Hasta la fase 4 el dibujo sigue en Python, y con él `space_toolsystem_common.py`.

---

## Fase 3: las consultas, en C++

El tooltip y el menú contextual ya no le preguntan nada a Python. Antes cada tooltip de
herramienta hacía **cuatro evaluaciones de cadenas de código**: etiqueta, descripción,
grupo y keymap. Ahora son llamadas al catálogo nativo.

| Puente | Estado |
|---|---|
| `wm_toolsystem.cc:926`, `:985` | cortados en la fase 2 |
| `interface_query.cc:146` | disuelto: compara con un operador que ya es C++ |
| `interface_context_menu.cc:405` — etiqueta | cortado |
| `interface_region_tooltip.cc:526` — etiqueta | cortado |
| `interface_region_tooltip.cc:578` — descripción | cortado |
| `interface_region_tooltip.cc:703` — grupo | cortado |
| `interface_region_tooltip.cc:775` — keymap | cortado |
| `interface_region_tooltip.cc:641` — keymap de la barra | **queda**, aislado en su `#ifdef` |

**Ocho de nueve.** El que queda depende de `keymap_from_toolbar.py` y del operador
`wm.toolbar`, que van con el dibujo de la barra.

### La consulta delicada era la descripción

Casi ninguna herramienta tiene texto propio: su tooltip sale del **primer atajo activo
de su keymap de usuario**. Un matiz mal replicado habría degradado cientos de tooltips
sin que nada avisara. Se verificó igual que todo lo demás: `queries-python.txt` sale de
las funciones reales (`item_from_id`, `description_from_id`, `item_group_from_id`), modo
a modo, y el nativo la reproduce **416/416 byte a byte**, en modo gráfico porque los
keymaps de usuario solo existen ahí. Incluye las siete descripciones calculadas, con los
atajos del usuario dentro, y las **45 herramientas que no tienen ninguna**: un tooltip
inventado sería una diferencia igual que uno perdido.

### Una diferencia deliberada

Manteniendo Mayúsculas sobre una herramienta, el tooltip debía enseñar su keymap. En el
Python **no salía nunca**: `keymap_from_id` devuelve el *nombre* del keymap, no el
keymap, y `getattr(nombre, 'as_pointer', lambda: 0)()` daba siempre 0. El nativo pide el
keymap de verdad, que es lo que el código C pretendía. Está escrito en el código.

---

## Una regresión mía de la fase 2, encontrada y cerrada

Al fijar la reserva en C++, el Python dejó de enterarse. El popover y la tarta de
reserva resaltan su opción con la memoria de grupos, y la etiqueta «Drag: …» de la
cabecera también; las tres leían un diccionario *Python* que ya nadie escribía. Tras
cambiar la reserva a Select Lasso seguían diciendo Tweak. Las pruebas de la fase 2 no
lo vieron porque no miraban esos dibujos.

Arreglo: la memoria de grupos es **una sola, la del motor**. `WindowManager` expone
`tool_group_active_get/set` por RNA y el armazón Python lee y escribe ahí. Es un puente
en la dirección que se puede quedar mientras se migra `bl_ui`: Python llama a C++.

`check_fallback_memory_gui.py` lo comprueba, y se ejecutó contra los dos binarios: con
el de antes falla (resalta Tweak), con el de después pasa.

## La barra tiene línea base visual

`capture_toolbar_gui.py` recorta la región de la barra en 14 combinaciones de editor y
modo y guarda su huella (`toolbar-capturas.txt`). Si el dibujo nativo es el mismo que el
de Python, las huellas tienen que coincidir píxel a píxel.

Dos avisos sobre esa prueba:

- **Tuvo que hacerse determinista.** La primera versión dio anchos de 112 y 113 píxeles
  en dos ejecuciones del mismo binario: capturaba antes de que las regiones se
  asentaran. Ahora espera a que el tamaño no cambie en dos redibujados seguidos, y dos
  ejecuciones dan exactamente lo mismo.
- **Las huellas dependen de la máquina** (resolución, escala, GPU). Sirven para comparar
  antes y después en el mismo equipo, no como referencia absoluta.

---

## Fase 4a: la barra, dibujada en C++

Los cuatro paneles `*_PT_tools_active` y el menú `WM_MT_toolsystem_submenu` (el que se
abre al mantener pulsado un grupo) son C++ (`editors/interface/fl_toolbar_ui.cc`), con
los mismos identificadores. La barra ya no ejecuta Python en cada redibujado.

**Verificación: 14 de 14 capturas idénticas píxel a píxel** a la línea base del Python
(vista 3D en seis modos, editor de imagen en tres, nodos, secuenciador en tres). Y que
sea la nativa la que pinta no es una suposición: un panel registrado desde Python
aparece en `bpy.types`, y este ya no aparece.

Dos cosas del Python que había que reproducir para acertar al píxel:

- Las columnas son **corrutinas** (`yield`/`send`), no bucles. En modo de dos columnas el
  cierre solo rellena la última fila si el índice cae en la última columna; si no, el
  generador da una vuelta más y puede abrir una fila vacía. Se reproduce tal cual.
- El ancho se compara con 80 y 120 en `double`, como el Python.

Y una que habría roto el píxel: el Python pasa `text=""` a los botones de solo icono. En
C++, pasar «sin texto» en vez de «texto vacío» hace que `op()` ponga el nombre del
operador.

### Pegamento transitorio

Las clases Python de la barra siguen existiendo —su catálogo lo leen aún la cabecera, la
reserva, el keymap de la barra y el editor de keymaps— pero ya no se registran como
paneles. Su `register()`, sin embargo, sigue haciendo falta: convierte cada
`keymap=()` en el *nombre* de su keymap, que es lo que esos lectores esperan. Lo dispara
un menú Python que nunca se muestra (`WM_MT_toolsystem_catalog_keymaps`). Comprobado:
362 de 362 keymaps convertidos. Se va con el fichero.

### KEYMAP_TOOL no era una regresión

Se sospechó que la migración del keymap había dejado sin la bandera `KEYMAP_TOOL` a los
keymaps de herramienta. Es cierto, pero no importa: esa bandera **no la lee nadie** en
el motor. Las herramientas encuentran su keymap por nombre desde el runtime.

---

## Fases 4b y 4c: ajustes, cabecera, reserva, keymap de la barra y sus operadores

Todo lo que dibujaba el sistema de herramientas desde Python pasa a C++:

- **Los 36 `draw_settings` que faltaban**, transliterados por diez agentes (uno por familia)
  y revisados línea a línea por otros diez. Sobre una capa (`FL_tool_settings_ui.hh`) que
  reproduce `layout.prop/label/operator/popover/row` tal como los recibe RNA, traducción
  incluida, para que ninguna transliteración reinvente `text=None` frente a `text=""`.
- **La cabecera de herramienta, el panel "Active Tool", el popover y la tarta de reserva**
  (`fl_toolbar_ui.cc`). Las cabeceras de `bl_ui`, que siguen en Python, los llaman por
  cuatro plantillas RNA (`template_tool_*`): Python llamando a C++.
- **El dibujo sobre la vista** (el círculo bajo el ratón) de las seis herramientas que lo
  tienen. Una diferencia a propósito: el Python capturaba el `bToolRef` al activar y lo
  usaba en cada dibujo; aquí se busca por su clave, así que no puede quedar colgando.
- **El keymap del popup de la barra** (`keymap_from_toolbar.py`, 394 líneas) y los tres
  operadores que lo usan: `wm.toolbar`, `wm.toolbar_fallback_pie`, `wm.toolbar_prompt`.
  Verificado **912/912** contra el Python real.

**Los nueve puentes C++ → Python del sistema de herramientas están cortados.** `wm.py` ya no
contiene nada de él, y la deuda declarada es **cero**: ninguna herramienta con ajustes ni
dibujo pendiente.

### Una diferencia deliberada de aspecto

`wm.toolbar_prompt` pintaba su aviso en la barra de estado **sustituyendo el método de
dibujo** de la cabecera de estado. Eso no tiene equivalente en C++; lo idiomático es
`WorkspaceStatus`, que es lo que usan todos los modales nativos. Mismo contenido (tecla
inicial, y para cada herramienta sus modificadores, su tecla y su nombre); cambian la caja y
el espaciado.

### La verificación visual de la cabecera, y lo que enseñó

819 capturas (cabecera y panel lateral de cada herramienta en los 30 espacios y modos).
La primera comparación dio **819 de 819 distintas**, y no era el código:

1. **La línea base de referencia no se reproducía a sí misma.** Una segunda ejecución de la
   misma app de referencia salía distinta de la primera. Se descarta y se regenera, y ahora
   se exige que dos ejecuciones coincidan antes de usarla.
2. **El ratón.** El resaltado depende de dónde está el cursor real al abrirse la ventana.
   El arnés ahora lo fija fuera de las regiones capturadas.
3. Con eso aislado, en modo objeto quedaban **3 diferencias reales**, y eran mías: las filas
   de modo de selección de la vista 3D no tenían su fila propia sin división, porque la
   expresión regular que las corrigió usaba `[a-z_.]` y `view3d` lleva un dígito. Corregido.

### Y cómo se cerró: comparando el binario contra sí mismo

La línea base de referencia se abandonó. El problema no era regenerarla mejor: es que dos
ejecuciones distintas **no comparten estado** —escala de ventana, tema, orden en que se
asientan las regiones, posición del ratón—, y ninguna de esas cosas tiene que ver con el
dibujo que se quiere comparar.

El arnés pasa a ser **diferencial**, como ya lo era `smoke_tools_gui.py` en la fase 2:
`capture_tool_headers_gui.py` lleva dentro el cuerpo literal de
`draw_active_tool_header` del commit 14294531f85, y para cada herramienta dibuja la misma
región **dos veces seguidas**, en la misma ejecución y sobre el mismo estado: una con la
plantilla nativa y otra con el Python de entonces. Misma región, misma posición, mismo
tamaño, mismo instante: si la huella cambia, cambió el dibujo y nada más.

El arnés se protege además de aprobar por accidente: marca cuando el oráculo Python ha
dibujado de verdad, y si la interfaz deja de pasar por él —porque ya llame a la plantilla
nativa directamente— lo dice y captura solo el lado nativo, en vez de comparar el dibujo
nativo consigo mismo y salir verde.

**819 huellas** (cabecera y panel lateral de cada herramienta en los 30 espacios y modos).
La comparación dio **38 diferencias**, y se repartieron así:

**22 eran del arnés, no del código.** La forma las delató: cada huella nativa era
exactamente la huella *Python de la herramienta anterior*. La captura salía con la
herramienta previa todavía pintada. Esperar a que la **geometría** de las regiones se
asiente —lo que hacía— no dice nada del **contenido**. Ahora se exige que dos capturas
seguidas den los mismos píxeles, que es lo único que no puede ir con retraso.

**16 eran reales, y ninguna se veía en el volcado del catálogo**, porque las tres son
fallos de *dibujo* sobre datos correctos:

1. **Los ajustes que viven dentro de una macro no se pintaban.** Cinco filas del catálogo
   apuntan a un sub-operador (`MESH_OT_loopcut.number_cuts`,
   `MESH_OT_polybuild_face_at_cursor.create_quads`, `MESH_OT_rip.use_fill`,
   `TRANSFORM_OT_shrink_fatten.use_even_offset`, `TRANSFORM_OT_edge_slide.correct_uv`).
   `RNA_struct_find_property` no resuelve rutas con punto: devolvía nulo y la fila se
   saltaba. **Loop Cut, Poly Build, Rip Region y Extrude Along Normals salían con la
   cabecera vacía.** Ahora esas rutas se resuelven con `RNA_path_resolve`.

2. **`layout.row(...)` no estaba modelado del todo.** `PROP_ROW_OWN_ROW` abría siempre una
   fila *sin alinear*, y seis filas ni siquiera lo llevaban. Afectaba a Spin, Cloth Filter,
   Blade y a las tres Extrude que comparten `VIEW3D_GGT_xform_extrude`: sus enumeraciones
   expandidas salían sueltas en vez de pegadas. Se añade `PROP_ROW_ALIGN`.

3. **Dos propiedades en la misma fila.** El `builtin.trim` del lápiz de grasa pinta dos
   casillas en una fila sin división. Estaba escrito en el código como pendiente
   («`PropRow` todavía no lo expresa»), que es la forma honrada de dejar una deuda… y la
   comparación visual es la que la cobra. Se añade `PROP_ROW_SAME_ROW`.

Y una cuarta, que la comparación **no** vio pero quedó a la vista al buscarlas:
`space_shows_toolbar` preguntaba por `show_region_toolbar` al tipo RNA **base** `Space`,
donde esa propiedad no existe —solo está en cada tipo derivado—. La rama nunca se
cumplía, así que el icono que sustituye a la barra cuando está oculta no se pintaba jamás.
Ahora se lee la región `RGN_TYPE_TOOLS` directamente, igual que hace el captador de RNA.

> Lección, y es la misma de siempre en esta migración: **el volcado no ve el dibujo**. Los
> nueve campos de la línea base decían que `builtin.loop_cut` tenía sus dos ajustes, y era
> cierto: estaban en la tabla. Solo que no se pintaban.
