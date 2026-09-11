# Los menús que el keymap nativo abre por nombre: de Python a C++

> El puente C++ → Python más grande del árbol, y el más silencioso.

## El agujero

El keymap por defecto ya es C++ y está verificado idéntico al de Python
(`KEYMAP-A-CPP.md`: 248 keymaps, 3.673 atajos, cero diferencias). Pero un
`wmKeyMapItem` de `wm.call_menu` / `wm.call_menu_pie` / `wm.call_panel` **no
guarda el menú, guarda su nombre**: una cadena en la propiedad `name` que se
resuelve al pulsar la tecla con `WM_menutype_find()`. Y `WM_menutype_find()`
solo conoce lo que alguien haya dado de alta.

Mientras el menú solo exista como clase de Python, un editor sin intérprete
resuelve ese nombre a `nullptr`. `wm.call_menu` devuelve `OPERATOR_CANCELLED`,
la tecla no hace nada y **no hay ni un mensaje de error**. Es exactamente el
tipo de fallo que este proyecto persigue eliminar: no se ve hasta que alguien
pulsa la tecla y se encoge de hombros.

## La medida, hecha por cruce de conjuntos (no por lectura)

Tres conjuntos, extraídos del árbol:

1. **Lo que el keymap nombra.** Union de (a) los literales
   `"[A-Z][A-Z0-9_]*_(MT|PT)_…"` de `source/blender/windowmanager/keymap/*.cc`
   —hay que incluir los que pasan por las plantillas
   `template_items_context_menu()` / `template_items_context_panel()` y por los
   ternarios de `params`, que un `grep` de `item_menu(km, "…"` se deja— y (b)
   los `props={…name="…"}` de los `WM_OT_call_menu*` / `WM_OT_call_panel` de la
   línea base congelada `tests/flipendo/keymap/baseline-python.txt`. Los dos
   caminos hacen falta: (a) ve las combinaciones de `Params` que la
   configuración por defecto no activa (`VIEW3D_MT_snap` frente a
   `VIEW3D_MT_snap_pie`, `VIEW3D_MT_shading_ex_pie`…), (b) ve lo que las
   plantillas pasan por variable. **Total: 136 identificadores distintos**
   (282 `call_menu` + 90 `call_menu_pie` + 44 `call_panel` en la línea base).
2. **Lo que registra Python:** las clases `^class X(…)` de `scripts/**/*.py`.
3. **Lo que ya registra el C++:** los `STRNCPY(mt->idname, "…")` y los
   `MenuDecl` de `FL_ui_registry`.

Resultado:

| | |
|---|---|
| Nombres que el keymap nativo invoca | **136** |
| Ya nativos (no hay nada que hacer) | 3 — `LOGIC_MT_logicbricks_add`, `SCREEN_MT_user_menu`, `TOPBAR_MT_file_open_recent` |
| **Solo existen como clase de Python** | **133** |

### Corrección a `INVENTARIO-PYTHON.md` §4.6

El inventario cifra este puente en **129**. La cifra real es **133**. Los cuatro
que faltan son `GREASE_PENCIL_MT_draw_delete`, `GREASE_PENCIL_MT_layer_active`,
`GREASE_PENCIL_MT_move_to_layer` y `GREASE_PENCIL_MT_snap_pie`: el patrón de
identificador del inventario (`^[A-Z0-9]+_(MT|PT)_\w+`) no admite el guion bajo
de `GREASE_PENCIL`, así que los descarta. El reparto por espacio del inventario
(VIEW3D 65, IMAGE 11, SEQUENCER 8, CLIP 8, GRAPH 7, TOPBAR 5, DOPESHEET 5,
NLA 4, NODE 3, «y 13 más sueltos») coincide con el de aquí en todo lo demás:
13 + 4 = 17 sueltos.

### Los 133, por fichero que los define

| Fichero | Menús |
|---|---|
| `scripts/startup/bl_ui/space_view3d.py` | 66 |
| `scripts/startup/bl_ui/space_image.py` | 11 |
| `scripts/startup/bl_ui/space_sequencer.py` | 8 |
| `scripts/startup/bl_ui/space_clip.py` | 8 |
| `scripts/startup/bl_ui/space_graph.py` | 7 |
| `scripts/startup/bl_ui/space_dopesheet.py` | 5 |
| `scripts/startup/bl_ui/space_topbar.py` | 4 |
| `scripts/startup/bl_ui/space_nla.py` | 4 |
| `scripts/startup/bl_ui/properties_grease_pencil_common.py` | 4 |
| `scripts/startup/bl_ui/space_node.py` | 3 |
| `scripts/startup/bl_ui/space_filebrowser.py` | 3 |
| `scripts/startup/bl_ui/space_outliner.py` | 2 |
| `scripts/startup/bl_ui/space_userpref.py` | 1 |
| `scripts/startup/bl_ui/space_text.py` | 1 |
| `scripts/startup/bl_ui/space_info.py` | 1 |
| `scripts/startup/bl_ui/space_console.py` | 1 |
| `scripts/startup/bl_ui/properties_mask_common.py` | 1 |
| `scripts/startup/bl_ui/properties_data_armature.py` | 1 |
| `scripts/startup/bl_ui/anim.py` | 1 |
| `scripts/startup/bl_operators/wm.py` | 1 |

Ojo: 66 en `space_view3d.py` pero 65 con prefijo `VIEW3D_`. El que sobra es
`TOPBAR_MT_edit_curve_add`, que vive en `space_view3d.py` aunque se llame
`TOPBAR_`. Por eso el inventario, que contaba por prefijo, decía 65.

## Cómo se migra uno

Igual que un panel (`UI-A-CPP.md`), con dos particularidades:

1. **El menú C++ y la clase de Python no pueden convivir.** `rna_Menu_register`
   (`makesrna/intern/rna_ui.cc:1022-1040`) busca el `idname`, y si lo encuentra
   intenta desregistrarlo; si el `MenuType` que hay es nativo no tiene
   `rna_ext.srna`, así que falla con `RPT_ERROR` «is built-in». Eso hace saltar
   una excepción en `bpy.utils.register_class` que **aborta el bucle de registro
   de todo `bl_ui`**. Es decir: el C++ y la retirada del Python van en el mismo
   cambio, sin fase intermedia.
2. **Traducción.** `rna_uiItemR` traduce el texto con el contexto por defecto
   (el bloque que usaría el de la propiedad lleva años dentro de un `#if 0`,
   `rna_ui_api.cc:74-82`) y `rna_uiItemO` con el de la `srna` del operador, que
   es `BLT_I18NCONTEXT_DEFAULT_BPYRNA` (`"*"`), y `BLT_is_default_context()`
   normaliza `"*"` al contexto por defecto. Conclusión práctica: `IFACE_()` es
   el equivalente exacto en los dos casos, y solo hace falta `CTX_IFACE_()`
   donde el Python pasaba un `text_ctxt=` explícito.

## Orden de ataque

Por familias, empezando por la vista 3D, que es donde están 66 de los 133 y
donde cuelgan los atajos de uso diario.

### Primera tanda: 29 menús de la vista 3D (2026-09-11)

| Familia | Fichero C++ | Menús | Teclas |
|---|---|---|---|
| 1 · Radiales y `snap` | `editors/space_view3d/fl_view3d_menus.cc` | 15 | Ctrl-Tab, `` ` ``, Z, `.`, `,`, Shift-S, Shift-O, A, Alt-A, Shift-Alt-A, Alt-W, K |
| 2 · Borrado en edición | `editors/space_view3d/fl_view3d_menus_edit.cc` | 7 | X, M, Y |
| 3 · Submenús de malla | `editors/space_view3d/fl_view3d_menus_mesh.cc` | 7 | Ctrl-V, Ctrl-E, Ctrl-F, Alt-E, Alt-N, Shift-G |

Quedan **104** de los 133.

## Deuda con nombre y apellidos

Menús migrados que siguen apoyándose en código Python. **No se fuerzan**: el
`idname` del operador se resuelve en tiempo de dibujo, así que el menú C++ ya
está bien y funcionará solo en cuanto su dependencia sea nativa.

| Depende de | Es | Menús afectados | De quién es |
|---|---|---|---|
| `view3d.transform_gizmo_set` | operador Python, `bl_operators/view3d.py:220` | `VIEW3D_MT_transform_gizmo_pie` | `bl_operators/view3d.py` no está asignado a ningún carril |
| `view3d.edit_mesh_extrude_move_normal` | ídem, `:83` | `VIEW3D_MT_edit_mesh_extrude`, `VIEW3D_MT_edit_mesh_faces` | ídem |
| `view3d.edit_mesh_extrude_move_shrink_fatten` | ídem, `:173` | ídem | ídem |
| `view3d.edit_mesh_extrude_manifold_normal` | ídem, `:189` | `VIEW3D_MT_edit_mesh_extrude` | ídem |

Menús de la lista de 133 que **todavía no se pueden migrar** porque su contenido
sale de código Python que aún no existe en C++:

| Menú | Por qué |
|---|---|
| `VIEW3D_MT_bone_options_toggle` / `_enable` / `_disable` | Se generan recorriendo `bpy.types.Bone.bl_rna.properties` y llaman a `wm.context_collection_boolean_set`, que sigue siendo Python. Es del carril B (`bl_operators/wm.py`). |
| `WM_MT_region_toggle_pie` | Vive en `bl_operators/wm.py`; carril B. |

## Cómo se verificó la primera tanda

Con el volcador del carril D, congelando la línea base con el Python vivo y los
`.cc` nuevos compilados pero **sin llamar a `menus_register()`** — porque con los
menús no hay fase de convivencia.

| | |
|---|---|
| Registro (`--fl-check-ui`) | 2.069 bloques, **2.069 idénticos**, 0 distintos, 0 faltan, 0 sobran |
| Dibujo (`--fl-dump-ui-layout`, `cmp`) | **idéntico byte a byte**, 4.660.312 bytes |
| Los 29 menús migrados | 29/29 bloques de dibujo idénticos, 269 botones |

Y el verificador se probó al revés: cambiada a mano una etiqueta de la línea base,
la señaló (5 menús con `label='Pivot Point'`, uno de ellos migrado).

### Dos avisos para quien siga

1. **El volcado de dibujo tiene hoy un bloque no determinista**:
   `OBJECT_PT_levels_of_detail`. Dos volcados seguidos del mismo binario salen
   idénticos, pero una tercera corrida lo marca distinto. Mientras siga así,
   `--fl-check-ui` sobre el diseño no puede dar un `0 distintos` limpio. Es del
   carril D (volcador y paneles de juego).
2. **La escena de fábrica está en modo objeto.** Los menús que solo se llenan en
   modo edición se comparan en su rama vacía. Es la misma rama en Python y en
   C++, pero no prueba la llena: `VIEW3D_MT_edit_mesh_extrude` sale con 3 de sus
   9 botones posibles. Lo que no se verifica, se dice.

### Segunda y tercera tanda: 16 menús más (2026-09-11)

| Familia | Fichero C++ | Menús | Teclas |
|---|---|---|---|
| 4 · Objeto, pose y UV | `editors/space_view3d/fl_view3d_menus_object.cc` | 8 | Ctrl-A, U, Ctrl-L, Ctrl-H, Ctrl-G, Alt-P |
| 5 · Contextuales cortos y lápiz de cera | `editors/space_view3d/fl_view3d_menus_context.cc` | 8 | botón derecho en retícula, metabola, texto y curvas; U |

Quedan **88** de los 133.

## Tres trampas más, todas cazadas por el volcado y ninguna por la vista

1. **`layout.operator_menu_enum(op, prop)` sin `text=` no es `""`.** El Python
   pasa `None`, `rna_translate_ui_text` lo convierte en `std::nullopt`, y es ese
   `nullopt` el que hace que la etiqueta salga del nombre del operador. Con `""`
   el botón sale mudo. En C++ hay que ir por
   `uiItemMenuEnumFullO_ptr(..., std::nullopt, ...)`, porque el atajo
   `uiItemMenuEnumO()` exige una cadena. Pasó en `VIEW3D_MT_make_links` (lo
   marcó el volcado) y en los cinco `operator_menu_enum` de `VIEW3D_MT_hook`
   (esos **no** los marcó, porque la escena de fábrica no tiene ningún
   modificador de gancho: se arreglaron por analogía).
2. **Propiedades de enumeración dinámica.** `grease_pencil.set_material` declara
   `slot` como `RNA_def_enum` con `RNA_def_enum_funcs(..., material_enum_itemf)`.
   Asignarla con `RNA_string_set` **se lleva el proceso por delante**. Va con
   `RNA_enum_set_identifier(C, ...)`, que necesita el contexto para resolver los
   items. Sin el volcado, esto se descubre el día que alguien pulsa la tecla.
3. **`bl_translation_context` del menú.** `VIEW3D_MT_edit_curves_add` lleva
   `i18n_contexts.operator_default`; en el `MenuDecl` es
   `BLT_I18NCONTEXT_OPERATOR_DEFAULT`. Lo canta el volcado de **registro**, no el
   de dibujo.

## Y una advertencia de método

La línea base caduca. Mientras se migraban estas familias, otros carriles
añadieron (`RENDER_PT_publish`) y quitaron (`VIEW3D_PT_gpencil_brush_presets`)
paneles. Por eso el parte de cada tanda compara **los bloques de los menús
migrados** — que tienen que salir idénticos, sin excusa — y además da el número
global diciendo qué bloques cambiaron y de quién son. Un `0 distintos` global en
un árbol que comparten cuatro carriles a la vez no es alcanzable, y fingir que
sí lo es sería peor que no medir.

## Antes de retirar un menú: mirar quién le añade filas

Un menú de Python puede tener filas que **no están en su `draw()`**: cualquier
código puede hacer `bpy.types.<MENU>.append(func)` o `.prepend(func)` y la fila
aparece igual. Al pasar el menú a C++ esas filas **desaparecen sin avisar**,
porque desde C++ no hay forma de insertar en un menú de Python ni al revés.

Por eso, antes de borrar la clase:

```
grep -rnE "\.(append|prepend)\(" scripts --include='*.py' | grep -E "_MT_|_PT_|_HT_"
```

Hecho para las siete familias de esta migración (51 menús): **ninguno de los 51
tiene un añadidor vivo**. Los únicos `append` del árbol sobre menús son los de
`scripts/templates_py/` —plantillas de script, texto que no se ejecuta— y los de
`addons_core/bl_pkg` sobre menús de Preferencias (`USERPREF_*`), que no están en
esta lista.

### La fila que sí hay que reponer, y a quién le toca

El carril J migró los addons del motor a C++ y dejó una deuda apuntada
(`politicas/ADDONS-MOTOR-A-CPP.md` §4.3): el addon del *runtime* añadía
**File › Export › «Save as game runtime»** con
`bpy.types.TOPBAR_MT_file_export.append()`. El operador ya es nativo
(`WM_OT_save_as_runtime`), pero la fila del menú no existe hoy.

`TOPBAR_MT_file_export` **no está entre los 133** de esta política (el keymap no
lo abre por nombre), pero vive en `space_topbar.py`, que sí tiene cuatro de los
133. Así que queda escrito aquí también: **el día que `TOPBAR_MT_file_export`
pase a C++, su `draw()` tiene que terminar con**

```cpp
layout->op("WM_OT_save_as_runtime", IFACE_("Save as game runtime"), ICON_NONE);
```

Ninguno de los 51 menús migrados hasta ahora es de Archivo ni de Exportar, así
que no hay nada que reponer hacia atrás.

### Cuarta a séptima tanda: 32 menús más, y salida de la vista 3D (2026-09-11)

| Familia | Fichero C++ | Menús |
|---|---|---|
| 8 · `VIEW3D_MT_add` (Shift-A) | `space_view3d/fl_view3d_menu_add_root.cc` | 1 |
| 9 · Editor de curvas | `space_graph/fl_graph_menus.cc` | 7 |
| 10 · Hoja de exposición | `space_action/fl_action_menus.cc` | 5 |
| 11 · NLA | `space_nla/fl_nla_menus.cc` | 4 |
| 12 · Editor de imagen y UV | `space_image/fl_image_menus.cc` | 11 |
| 13 · Editores pequeños (texto, consola, info, esquema) | cuatro `fl_*_menus.cc` | 4 |

**83 de los 133.** Quedan 50.

## Cuatro trampas más

1. **`is_extended()` no se puede preguntar desde C++, y no hace falta.**
   `VIEW3D_MT_add` mira `VIEW3D_MT_armature_add.is_extended()` —
   `len(cls.draw._draw_funcs) > 1`, o sea «¿le ha metido filas algún addon?».
   Sin intérprete no hay addons, así que es estructuralmente falso: se escribe
   la rama «no extendido». Es una **decisión**, no una copia, y por eso va
   escrita en la cabecera del fichero.
2. **`operator_menu_enum` sin `text=`** (ya estaba) y su primo:
   `layout.operator(...)` cuya propiedad es una **enumeración dinámica**
   (`grease_pencil.set_material.slot`) necesita `RNA_enum_set_identifier(C, ...)`;
   con `RNA_string_set` el proceso se cae.
3. **El guion que quita las clases de Python tiene que cortar en CUALQUIER línea
   a columna cero.** La primera versión buscaba la siguiente `class`/`def`/
   `# ***`/`classes = (`, y en `space_image.py` se llevó por delante un
   `from bl_ui.properties_mask_common import (...)` que venía detrás de un
   comentario `# ----`. El módulo dejó de cargar (`NameError: MASK_PT_mask`) y
   con él **todo el registro de `bl_ui`**: el volcado pasó de 2.069 bloques a
   389, con 1.683 ausencias. Lo cantó el verificador a la primera. Auditoría
   después del arreglo: cero líneas de columna cero borradas que no fueran
   `class X(` en ninguno de los ficheros tocados.
4. **`CMakeLists.txt` del editor puede no depender de la traducción.**
   `space_console` no dependía de `bf::blentranslation` porque no dibujaba
   ningún texto desde C++; el primer `IFACE_()` rompe la compilación con
   `'BLT_translation.hh' file not found`. Hay que añadir la dependencia.

## Lo que queda bloqueado por algo ajeno

| Menú | Bloqueado por | De quién |
|---|---|---|
| `VIEW3D_MT_bone_options_toggle` / `_enable` / `_disable` | `wm.context_collection_boolean_set` (ya es C++ en `noche/codex`, pendiente de integrar en `main`) | carril B |
| `WM_MT_region_toggle_pie` | vive en `bl_operators/wm.py` | carril B |
| `POSE_MT_selection_sets_select` | `pose.selection_set_select` sigue siendo operador de Python | sin asignar |
| `OUTLINER_MT_context_menu` | llama a `OUTLINER_MT_collection_new.draw_without_context_menu()`, un método de clase distinto de `draw()`: `uiItemMContents()` no sirve | este carril, cuando toque migrar también ese trozo |

### Octava a décima tanda: clips, secuenciador y editores pequeños (2026-09-11)

| Familia | Fichero C++ | Menús |
|---|---|---|
| 14 · Editor de clips | `space_clip/fl_clip_menus.cc` | 8 |
| 15 · Secuenciador (6 de 8) | `space_sequencer/fl_sequencer_menus.cc` | 6 |

**97 de los 133.** Quedan 36.

## Los 36 que quedan, y qué hace falta para cada uno

| Menús | Qué falta |
|---|---|
| `VIEW3D_MT_object_context_menu` (229 líneas), `VIEW3D_MT_edit_mesh_context_menu` (209), `VIEW3D_MT_greasepencil_edit_context_menu` (106), `SEQUENCER_MT_add` (63), `SEQUENCER_MT_context_menu` (121), `NODE_MT_context_menu` (100) | Nada especial: son largos. Una tanda cada uno o de dos en dos |
| Los 9 `VIEW3D_PT_*_context_menu` y `VIEW3D_PT_snapping`, `USERPREF_PT_ndof_settings`, `TOPBAR_PT_name`, `TOPBAR_PT_name_marker` | **Son paneles, no menús**: van con `flipendo::panels_register()` sobre su región, no con `menus_register()`. Es otro mecanismo y merece su propia tanda |
| `TOPBAR_MT_file_context_menu`, `TOPBAR_MT_file_new` | `space_topbar.py`. Al tocar ese fichero hay que reponer la fila `wm.save_as_runtime` en `TOPBAR_MT_file_export` (ver más arriba) |
| `NODE_MT_add`, `NODE_MT_view_pie`, `FILEBROWSER_MT_context_menu`, `FILEBROWSER_MT_view_pie`, `ASSETBROWSER_MT_context_menu`, `MASK_MT_add`, `ANIM_MT_keyframe_insert_pie`, los 3 `GREASE_PENCIL_MT_*` | Cortos. `MASK_MT_add` y `ANIM_MT_keyframe_insert_pie` no tienen editor propio: hay que elegirles un punto de registro (`editors/mask`, `editors/animation`) |
| `VIEW3D_MT_bone_options_*` (3), `WM_MT_region_toggle_pie`, `POSE_MT_selection_sets_select`, `OUTLINER_MT_context_menu` | Bloqueados por trabajo ajeno o por código Python que aún no existe en C++ (ver la tabla anterior) |

## Una laguna de la línea base que hay que tener presente

La línea base se congela sobre la **escena de fábrica**, y eso deja sin cubrir el
dibujo de todo menú cuyo `poll` dependa de datos que ahí no hay. En la tanda del
editor de clips, cinco de los ocho salieron `NO-CUBIERTO motivo=poll` porque no
hay ningún clip cargado. De esos queda verificado el **registro** —idname,
etiqueta, banderas, contexto de traducción y que tienen `draw` y `poll`— pero no
el dibujo.

No es un fallo de la migración ni del volcador: es el alcance de la línea base.
Para cerrarlo haría falta una segunda línea base sobre un `.blend` con clip,
máscara, partículas y modo edición. Mientras no exista, cada parte dice qué
bloques quedaron sin cubrir y por qué.

## Los doce que son paneles: por qué no se pueden migrar de uno en uno

> **Corregido el 2026-09-11 a las 06:15.** Lo que sigue describe bien el mecanismo,
> pero la conclusión que sacaba —«la unidad de migración es la región entera»— era
> más dura de lo necesario, y uno de sus datos era falso. La versión buena está en
> «La regla del prefijo», al final de este documento. Se deja el texto original
> porque la medida del mecanismo sigue siendo correcta y es la que hay que entender
> antes de leer la corrección.


De los 133, **doce son `Panel`, no `Menu`**. El keymap los abre con
`wm.call_panel`, que los busca en el registro global con `WM_paneltype_find()`,
así que a primera vista bastaría con `flipendo::panels_register()`. No basta, y
conviene saber por qué antes de empezar.

Un `PanelType` no vive solo en el registro global: vive **en la lista de su
región**, y el volcado de registro compara esa lista **en orden de lista, no
alfabético** (es la trampa que ya está escrita en `UI-A-CPP.md`). Y el orden lo
decide el orden de alta, porque `PanelType::order` vale 0 en los doce:
`panel_insert_ordered()` inserta detrás del último con `order <= 0`, o sea añade
al final entre iguales.

El C++ se registra en `ED_spacetypes_init()`; el Python, al cargar los scripts,
mucho después. Conclusión: **cualquier panel que pase a C++ salta a la cabeza de
la lista de su región**, y el volcado lo marca como diferencia aunque el panel
sea idéntico. Medido en la línea base de esta noche:

| Región | Paneles en la lista | De los 133 | Ajenos que quedarían detrás |
|---|---|---|---|
| `VIEW_3D WINDOW` | 11 | 8 | 3 (`VIEW3D_PT_curves_sculpt_*`, de `space_view3d_toolbar.py` — carril A2) |
| `TOPBAR HEADER` | 15 | 3 (posiciones 5, 6 y 7) | 12 |
| `VIEW_3D HEADER` | 58 | 1 (`VIEW3D_PT_snapping`) | 57 |

O sea: migrar `VIEW3D_PT_snapping` solo cambia su posición de la 1 a la 58 en una
lista de 58, y el parte lo cantaría. **No es un fallo: es que la unidad de
migración de los paneles es la región entera, no el panel.**

Las dos salidas, y las dos hay que escribirlas antes de tomarlas:

1. **Migrar la región completa.** Para `VIEW_3D WINDOW` son once paneles, de los
   que tres son de `space_view3d_toolbar.py` (carril A2): hay que coordinarlo,
   no se puede hacer desde aquí sin pisar a otro carril.
2. **Dar `order` explícito** a los migrados para que caigan donde caían. Es
   posible, pero `order` es una prioridad y no una posición, y con todo a 0 no
   se puede intercalar nativo y Python de forma estable. Sería una decisión, no
   una copia, y habría que justificarla por escrito.

Mientras tanto los doce **están medidos y no tocados**. Migrarlos «porque son
cortos» habría metido cinco diferencias de región en el parte de otro carril.

## La regla del PREFIJO de región (medida el 2026-09-11, 06:15)

La sección anterior mide bien el mecanismo: con `order` a cero en todos,
`panel_insert_ordered()` añade al final entre iguales, así que **el orden de la lista
de una región es el orden de alta**, y el C++ se da de alta en `ED_spacetypes_init()`,
mucho antes de que carguen los scripts. De ahí concluía que había que migrar la región
entera.

No hace falta la región entera. Hace falta un **prefijo** de la región:

> Si lo que se migra son los **N primeros** paneles de la lista, y se dan de alta en el
> mismo orden relativo, quedan en las N primeras posiciones —que es donde ya estaban— y
> el Python sigue añadiendo los suyos detrás. La lista sale **idéntica**.

Lo que rompe el orden no es «migrar menos de la región»: es **migrar uno de en medio**.

Medido, no razonado: con los cuatro primeros de `VIEW_3D WINDOW` en C++ y los otros
siete todavía en Python, el volcado de registro devolvió la lista de la región
**idéntica a la línea base**, con los cuatro migrados en las posiciones 1 a 4.

### Por qué `order` no sirve para intercalar (y no hay que volver a intentarlo)

El bucle de inserción recorre la lista **hacia atrás** desde el último y corta en el
primero con `order <= pt->order`. Con todos los demás a `order = 0`:

| `order` del panel nuevo | Dónde cae |
|---|---|
| `0` | detrás del último que ya hay — o sea, a la cabeza, porque el C++ llega antes |
| `> 0` | al **final** de la región, detrás de todos los de `order = 0` |
| `< 0` | a la cabeza de todo |

No hay valor que lo deje en medio. `order` es una prioridad, no una posición: por eso
la unidad es el prefijo y no el panel suelto.

### Corrección de un dato de la tabla anterior

La tabla decía que tres de los once paneles de `VIEW_3D WINDOW` eran de
`space_view3d_toolbar.py` (carril A2) y que por eso no se podía cerrar la región sin
pisar a otro carril. **Es falso.** Los tres `VIEW3D_PT_curves_sculpt_add_shape`,
`_parameter_falloff` y `_grow_shrink_scaling` están en `space_view3d.py`. Lo que vive
en `space_view3d_toolbar.py` es `VIEW3D_PT_curves_sculpt_symmetry` y
`_symmetry_for_topbar`, que son **otros paneles y de otra región** (`TOPBAR HEADER`).
Los once de `VIEW_3D WINDOW` salen los once de `space_view3d.py`.

### Undécima y duodécima tanda: `VIEW_3D WINDOW` entera (2026-09-11)

| Familia | Fichero C++ | Paneles | Commit |
|---|---|---|---|
| 28 · Contextuales de pintado y escultura (1-4) | `space_view3d/fl_view3d_panels_paint.cc` | 4 | `5a4e77847e0` |
| 29 · Escultura de curvas y lápiz de cera (5-11) | `space_view3d/fl_view3d_panels_curves_gp.cc` | 7 | `fb58a65b3c4` |

Con ellos se estrena `editors/include/FL_paint_common.hh`: los ayudantes de
`UnifiedPaintPanel` (`prop_unified`, `prop_unified_color`, `prop_unified_color_picker`,
`get_brush_mode`, `paint_settings`) y `brush_basic_grease_pencil_weight_settings`. Van
en `editors/include/` porque es el mixin de interfaz más reutilizado del árbol y la
decisión del jefe de proyecto de las 03:40 prohíbe escribirlo dos veces.

**127 de 133.** La región `VIEW_3D WINDOW` queda entera en C++: ya no hay ni un
`PanelType` de Python detrás.

### El contrato de los fallos del Python, escrito de una vez

El `draw()` de estos once empieza leyendo `context.tool_settings.<modo>.brush`. En la
escena de fábrica no hay ningún modo de pintado iniciado, así que el Python lanza
`AttributeError`/`TypeError` y **el dibujo se corta en esa línea**. La línea base tiene
por eso los once bloques a medio dibujar (26 líneas y 2 botones entre los once).

Eso **no** es el caso de `OUTLINER_MT_context_menu`, y la diferencia importa:

| | `OUTLINER_MT_context_menu` | Los once paneles |
|---|---|---|
| Qué corta el dibujo | una enumeración dinámica sin items **en el contexto del volcador** | que la escena no tiene los datos |
| ¿Pasa cuando el usuario lo abre? | **No**: es un artefacto del arnés | **Sí**: es el comportamiento correcto |
| Qué hacer | revertir y volver a congelar el bloque | transliterar el corte |

Por eso aquí sí se reproduce el corte, y se reproduce **en su sitio**: `prop_unified`
devuelve `nullptr` —y `prop_unified_color*`, `false`— en el punto exacto de la
excepción, y quien la llama hace `return` ahí. Adelantar la comprobación del pincel al
principio del `draw()` deja el panel vacío; es la misma trampa que ya costó dos menús
en las tandas anteriores, y en paneles es más fácil caer porque la rama corta *parece*
la correcta.

### Cómo se verificó

| | |
|---|---|
| Región `VIEW_3D WINDOW` | 11 paneles, **lista idéntica** a la línea base |
| Registro (`--fl-check-ui`) | **2.113 bloques, 2.112 idénticos, 1 distinto, 0 faltan, 0 sobran** |
| El único distinto | `REGION PROPERTIES WINDOW`, de otro carril: un panel suyo sí saltó a la cabeza de su región |
| Registro de los once | **11/11 idénticos** byte a byte |
| Dibujo (volcado completo, bloque a bloque) | **2.004 bloques, 0 distintos, 0 faltan, 0 sobran** |
| Dibujo de los once | **11/11 idénticos**, 26 líneas, 2 botones |

Lo que **no** queda verificado, dicho: esos 2 botones son todo lo que la escena de
fábrica dibuja de los once. La rama llena pide una segunda línea base sobre un `.blend`
con los modos de pintado, escultura de curvas y un lápiz de cera con capas.

## 127 de 133. Los seis que quedan, y quién los bloquea

Recuento hecho por cruce de conjuntos sobre el árbol y sobre el volcado de registro del
binario recién instalado, no por lectura:

- de los 133, **presentes en el registro: 133** (ninguno resuelve a `nullptr`);
- de los 133, **sin clase de Python viva: 127**;
- quedan **6**, y **ninguno** está bloqueado ya por el mecanismo de los paneles.

| Qué queda | Dónde vive | Qué lo bloquea |
|---|---|---|
| `VIEW3D_PT_snapping` | `space_view3d.py` | Es el **37.º** de `VIEW_3D HEADER` (58 paneles). Su prefijo son 36 paneles y el **segundo** es `VIEW3D_PT_brush_asset_shelf_filter`, de `properties_paint_common.py` |
| `TOPBAR_PT_name`, `TOPBAR_PT_name_marker`, `USERPREF_PT_ndof_settings` | `space_topbar.py`, `space_userpref.py` | Son el 5.º, 6.º y 7.º de `TOPBAR HEADER`. Su prefijo pasa por `IMAGE_PT_uv_sculpt_options` y `IMAGE_PT_uv_sculpt_curve`, de `space_image.py` |
| `POSE_MT_selection_sets_select` | `properties_data_armature.py` | El menú C++ y la clase de Python no pueden convivir, y ese fichero no es de este carril. (El operador `pose.selection_set_select` sigue siendo de Python, pero eso **no** bloquea: el idname se resuelve al dibujar, como el resto de la «deuda con nombre y apellidos») |
| `OUTLINER_MT_context_menu` | `space_outliner.py` | Su bloque de la línea base está contaminado; ver abajo |

Los cinco primeros están bloqueados por **propiedad de ficheros**, no por el mecanismo:
el día que `properties_paint_common.py`, `space_image.py` y `properties_data_armature.py`
estén en el mismo carril que sus regiones, salen con la regla del prefijo y sin
sorpresas. Los dos prefijos que hacen falta están medidos:

| Región | Paneles del prefijo | Líneas de Python | Fichero que lo bloquea (posición) |
|---|---|---|---|
| `VIEW_3D HEADER` → `VIEW3D_PT_snapping` | 36 + él | ~1.400 | `properties_paint_common.py` (2.ª) |
| `TOPBAR HEADER` → los tres | 4 + ellos | ~200 | `space_image.py` (2.ª y 3.ª) |

### `OUTLINER_MT_context_menu`: intentado, medido y retirado

Es corto, pero arrastra una cadena: su `draw()` llama a
`OUTLINER_MT_collection_new.draw_without_context_menu()`, y `collection_new`,
`OUTLINER_MT_collection` y `OUTLINER_MT_object` llaman los tres a
`OUTLINER_MT_context_menu.draw_common_operators()`. Son **métodos de clase**, no
`draw()`, así que `uiItemMContents()` no sirve: migrar uno obliga a migrar los
cuatro, y tres de ellos no están entre los 133.

Se hizo, compiló, y entonces el volcado enseñó algo mejor que un fallo mío:

```
< ... op='bpy.ops.outliner.id_operation()'                    text='Unlink'   ← Python
> ... op='bpy.ops.outliner.id_operation(type='<UNKNOWN ENUM>')' text='Unlink'  ← C++
```

…y **el bloque de Python terminaba ahí**, con quince filas menos que el mío.
El motivo: `outliner.id_operation.type` es una enumeración **dinámica**, y en el
contexto del volcador no tiene items; `props.type = 'UNLINK'` **lanza una
excepción en Python** y el `draw()` se corta a media faena. O sea:

> **la línea base, en ese bloque, no es el menú: es el menú a medio dibujar
> porque el Python se cayó.**

Reproducir eso en C++ sería codificar un fallo. No reproducirlo deja una
diferencia permanente en el parte. Ninguna de las dos es aceptable a la ligera,
así que se **revirtió el cambio entero** (los cuatro menús y sus borrados de
Python) y se deja escrito. Lo que hace falta para cerrarlo bien:

1. Migrar los cuatro a la vez, con `RNA_enum_set_identifier(C, ...)` —con
   contexto, para que la enumeración dinámica resuelva de verdad—, y
2. **volver a congelar la línea base de ese bloque**, porque la actual está
   contaminada por la excepción. Eso es trabajo del carril D, que es quien manda
   sobre la línea base.

### Y una lección que vale para toda la migración

Un bloque de la línea base puede ser más corto de lo que el menú debería tener,
porque el Python se cayó al dibujarlo. Salió dos veces esta noche
(`VIEW3D_MT_greasepencil_material_active`, que el carril D acabó marcando como
inestable, y este). Antes de dar por buena una diferencia, hay que mirar si el
bloque de la línea base **termina donde debería terminar**.
