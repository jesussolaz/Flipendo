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
