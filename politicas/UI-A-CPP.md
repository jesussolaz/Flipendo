# La interfaz del editor: de `bl_ui` a C++

> El macizo que queda. `scripts/startup/bl_ui` son **77 ficheros y 64.160 líneas**, con
> más de mil paneles y menús. Es, con diferencia, el bloque de Python más grande que le
> queda a Flipendo, y hasta ahora no había forma de migrarlo **sin fiarse de la vista**.

## Lo que ya existía, y lo que faltaba

Existía la **capacidad**: `rna_Panel_register()` (`makesrna/intern/rna_ui.cc`) no hace
nada que C++ no pueda hacer — rellena un `PanelType` y lo mete en la lista de la
región, exactamente igual que `graph_buttons.cc`. El bucle de dibujo
(`ED_region_panels_layout_ex`) no sabe de dónde vino el tipo.

Existía la **ergonomía**: `source/blender/editors/interface/FL_ui_registry.{hh,cc}`
reduce cada panel, menú o cabecera a una struct literal y centraliza lo que antes solo
hacía el registro de Python (orden de inserción, categoría de reserva, enlace de
sub-paneles con su padre, alta en el registro global).

Existía el **piloto**: `source/blender/editors/space_logic/logic_ui.cc`, que sustituyó
a `space_logic.py` (145 líneas) cubriendo los tres mecanismos.

Faltaba lo único que permite hacerlo **a escala y en paralelo**: una forma automática
de demostrar que el panel migrado es el mismo panel. Sin eso, migrar mil paneles es
tirar una moneda mil veces.

## Los dos volcados

Mismo patrón que `--fl-dump-keymap` y `--fl-dump-tools`: el binario escribe en C++ el
estado observable, se congela la línea base **con el Python todavía vivo**, y el C++
tiene que reproducirla. Están en `source/blender/editors/interface/fl_ui_dump.cc`,
contrato en `source/blender/editors/include/FL_ui_dump.hpp`.

```
Blender --factory-startup -b --fl-dump-ui        tests/flipendo/ui/baseline-python.txt
Blender --factory-startup    --fl-dump-ui-layout tests/flipendo/ui/baseline-python-layout.txt
Blender --factory-startup -b --fl-check-ui       tests/flipendo/ui/baseline-python.txt
Blender --factory-startup    --fl-check-ui       tests/flipendo/ui/baseline-python-layout.txt
```

### Las cifras de hoy

Con el Python de `bl_ui` vivo, sobre `--factory-startup`:

| Volcado | Bloques | Detalle |
|---|---:|---|
| Registro | **2.072** | 1.396 paneles, 592 menús, 25 cabeceras y 59 regiones |
| Dibujo | **2.013** | **1.096 dibujados**, 917 no cubiertos, **0 fallos** |

Los 917 no cubiertos, por motivo: 640 `poll` (el panel dice que no se dibuja en una
escena con un cubo, una cámara y una luz — su comportamiento correcto), 270
`instanciado`, 6 `sin-region` (`OPERATOR_PT_redo`, que solo existe si hay un operador
recién ejecutado) y 1 `datos-de-la-build`.

Las cifras se mueven según lo que vayan migrando los demás carriles: la línea base es
una foto de un commit, no una constante. Cuando la interfaz cambia a propósito, se
regenera y se dice en el commit.

### 1. `--fl-dump-ui` — el registro

Un bloque por región con la **lista ordenada** de sus paneles y cabeceras, y luego un
bloque por tipo: cada `PanelType`, `MenuType` y `HeaderType` dado de alta, con `idname`, etiqueta,
espacio, región, categoría, contexto de la pestaña de Propiedades, `parent_id`, orden,
banderas, opciones, `owner_id`, contexto de traducción, y **qué callbacks tiene**
(`draw`, `draw_header`, `draw_header_preset`, `poll`).

Prueba que el panel migrado **existe donde tiene que existir**. Corre en
`--background`, así que vale para CI.

Se recorre por espacios y regiones (`BKE_spacetypes_list()`), no por el mapa global de
`WM_paneltype_add`: el mapa global no dice en qué región vive el panel, y un panel que
está en el mapa pero en ninguna región **no lo dibuja nadie**.

### 2. `--fl-dump-ui-layout` — el dibujo

Ejecuta el `draw()` (y el `draw_header()`, y el `draw_header_preset()`) de cada panel,
menú y cabecera sobre la escena de fábrica, y serializa el árbol de `uiLayout` que sale:
la secuencia de filas, columnas, cajas, splits, grid-flows, paneles de layout y
botones, con el texto dibujado, el icono, la propiedad RNA (`Struct.prop[índice]`), el
operador **con los argumentos que le puso el panel** y las propiedades que modifican
cada nivel (`align`, `enabled`, `activo`, `emboss`, `alignment`, `prop_sep`,
`prop_decorate`, `heading`, escalas y unidades).

Esto es lo que de verdad prueba que un panel C++ dibuja lo mismo. El registro solo
prueba que está.

Necesita **modo gráfico**: sin ventana no hay región, y sin región no hay contexto, ni
`poll`, ni `CTX_data_*`.

### El formato

El fichero es una lista de **bloques**:

```
=== PANEL VIEW3D_PT_transform
category='Item'
context=''
...
```

El bloque es la unidad que compara `--fl-check-ui`, y por eso el parte puede decir
"1.632 bloques, 1.630 idénticos, 2 distintos, y éstos son". Los bloques se ordenan por
`(espacio, región, categoría, idname)` y las claves de dentro van en orden alfabético.
Nada de punteros, tiempos ni contadores: el volcado se reproduce a sí mismo byte a byte.

`--fl-check-ui` acepta las dos líneas base y decide cuál tiene delante por la marca de
su primera línea (`# FL-UI-DUMP registro v1` / `# FL-UI-DUMP diseno v1`).

## Cómo se migra un panel

1. **Declararlo** con `FL_ui_registry`. Los campos que no se ponen quedan a cero, que es
   el valor correcto para todos:

   ```cpp
   static const flipendo::PanelDecl panels[] = {
       {
           /*idname*/ "GAME_PT_game_object",
           /*label*/ N_("Game Object"),
           /*category*/ nullptr,
           /*context*/ "game",
           /*parent_id*/ nullptr,
           /*description*/ nullptr,
           /*translation_context*/ nullptr,
           /*draw*/ game_object_draw,
           /*draw_header*/ nullptr,
           /*draw_header_preset*/ nullptr,
           /*poll*/ game_object_poll,
           /*flag*/ 0,
           /*order*/ 1000,
       },
   };
   flipendo::panels_register(art, SPACE_PROPERTIES, {panels, ARRAY_SIZE(panels)});
   ```

   Los menús van a `flipendo::menus_register()` (registro global) y las cabeceras a
   `flipendo::headers_register()`.

2. **Traducir el `draw()`** a la API C++ de `uiLayout`, que es la misma que usa Python
   por debajo: `layout->row(align)`, `->column()`, `->box()`, `->split(pct, align)`,
   `->prop(ptr, "nombre", flags, texto, icono)`, `->op("IDNAME", texto, icono)`,
   `->label()`, `->separator()`, `->menu()`. Los textos traducibles pasan por `N_()` en
   las tablas y por `IFACE_()` en el dibujo, igual que el resto del árbol.

3. **Verificar antes de retirar el Python.** El orden importa:
   - Congela la línea base **con el Python vivo** (si no está ya congelada).
   - Escribe el C++, **sin quitar el Python todavía**: los dos registros con el mismo
     `idname` chocan, así que en esta fase se comprueba solo lo que se pueda.
   - Quita el Python.
   - `--fl-check-ui` contra las dos líneas base. Solo cuando los bloques del panel salen
     **idénticos** el trabajo está hecho.

4. **Actualizar esta política** con lo que aprendiste.

## Las trampas que ya están encontradas

### El verificador hay que verificarlo, y en los dos sentidos

El proyecto ya se llevó ese susto con el catálogo de herramientas (ver
`TOOLSYSTEM-A-CPP.md`, "El verificador se verificó al revés"). Aquí se hicieron las dos
comprobaciones antes de fiarse de ningún "0 diferencias":

- **Que se reproduce**: dos volcados seguidos, `cmp` byte a byte.
- **Que detecta**: se cambió a mano una etiqueta de un panel de Python, se volcó, y el
  verificador la señaló, con el bloque y la línea. Después se deshizo el cambio.

Un verificador que solo sabe decir que sí no verifica nada.

### Migrar un panel puede cambiarlo de sitio sin cambiar ningún campo

`PanelType::order` es una **prioridad, no una posición**: entre paneles con el mismo
`order` manda el orden de registro. Y ahí está la trampa — el Python se registra al
cargar los scripts, mucho después de que `ED_spacetypes_init()` haya registrado todo lo
nativo. Pasar un panel a C++ le adelanta el registro, así que un panel con `order` 0
que en Python quedaba el último de su pestaña, en C++ queda **el primero**, sin que
ninguno de sus campos haya cambiado y sin que el volcado de campos se entere.

Por eso el volcado de registro lleva además un bloque por región:

```
=== REGION PROPERTIES WINDOW
panel=OBJECT_PT_context_object
panel=OBJECT_PT_transform
...
```

La lista va **en orden de lista, no alfabético**: la lista *es* el resultado. Un panel
que cambia de sitio sale como diferencia de ese bloque y de ningún otro.

La salida cuando pasa: subir el `order` del panel migrado hasta donde estaba, o bajarlo,
según el caso — y escribirlo, porque un `order` que no venía del Python es una decisión,
no una copia.

### La pestaña del editor de Propiedades

`properties_*.py` es el mayor bloque de `bl_ui`, y sus paneles **no pasan el `poll`** si
el editor no está en su pestaña: `context.material`, `context.particle_system` y
compañía salen de la ruta de contexto que el editor calcula para la pestaña activa
(`buttons_context_compute`). Sin ponerla, el volcado de dibujo solo cubriría la pestaña
de Objeto.

Por eso el volcador pone la pestaña que pide `pt->context` antes de dibujar cada panel,
con `ED_buttons_context_tab_set()` — añadida a `ED_buttons.hh` para esto. Un panel sin
contexto se dibuja siempre con la pestaña de Objeto, para que el resultado no dependa
del panel anterior.

### Los espacios que el fichero de fábrica no abre

Un `draw()` necesita una región viva. El fichero de fábrica abre unos pocos editores, y
los paneles de los demás quedarían sin cubrir. El volcador recorre los espacios de uno
en uno y, para el que no esté abierto, **cambia de tipo un área de reserva** con
`ED_area_newspace` + `ED_area_init` — exactamente lo que hace el desplegable de tipo de
editor. Al terminar con un espacio pasa al siguiente, porque cambiar de tipo destruye
las regiones del anterior.

La barra superior y la de estado son la excepción: no son editores y no se pueden abrir
en un área normal. O están vivas en la ventana — el volcador también mira
`win->global_areas` — o se listan como no cubiertas.

### Los tipos de item de layout son privados

`uiButtonItem`, `uiLayoutItemSplit`, `uiLayoutItemGridFlow` y la propia enumeración
`blender::ui::ItemType` viven dentro de `interface_layout.cc`; el header público solo
los declara. Duplicar esas structs en el volcador es la manera clásica de que se
desincronicen en silencio, así que `interface_layout.cc` abre un **puente mínimo y de
solo lectura** (`flipendo::ui_dump::item_type_name`, `item_button`, …) declarado en
`FL_ui_dump.hpp`, y el resto sigue siendo privado.

### El binario no lee el Python del repo

La primera vez que se le pasó una etiqueta cambiada, el verificador dijo **"2.069
idénticos, 0 distintos"**. No era un fallo suyo: `nb install` **copia** `scripts/` a
`Blender.app/Contents/Resources/4.5/scripts/`, y el binario carga esa copia. Cambiar el
`.py` del repo no cambia nada hasta reinstalar.

Es la trampa perfecta para creerse un "idéntico" que no significa nada. Al comprobar
contra Python hay que hacer una de dos cosas, y decir cuál:

- reinstalar (`nb install`) después de tocar el `.py`, o
- tocar la copia instalada y restaurarla al terminar — que es lo que se hizo aquí,
  porque reinstalar entero por una etiqueta es media hora de reloj.

Con la copia instalada cambiada, los dos verificadores la cazaron a la primera y
devolvieron código de salida 1:

```
DIFIERE PANEL PROPERTIES WINDOW GAME_PT_game_object
  linea 9
    base:  label='Game Object'
    ahora: label='Game Object CAMBIADO'
```

```
DIFIERE PANEL PROPERTIES WINDOW OBJECT_PT_levels_of_detail
  linea 4
    base:        BUTTON ... text='Distance Factor: 1.000' ...
    ahora:       BUTTON ... text='Distance Factor CAMBIADO: 1.000' ...
```

### El `idname` de un panel no es único

`OPERATOR_PT_redo` está registrado en la región HUD de **seis** editores: seis
`PanelType` distintos con el mismo `idname`. La primera versión del verificador usaba
`PANEL <idname>` como clave y perdía cinco de los seis sin decir nada — el fallo más
silencioso posible en un verificador. La clave lleva ahora espacio y región:
`PANEL PROPERTIES WINDOW OBJECT_PT_transform`.

El registro global de paneles (`WM_paneltype_add`) tampoco los distingue: es un
`VectorSet` por `idname`, así que solo guarda el primero. Otra razón para recorrer por
regiones y no por el mapa global.

### La red debajo del dibujo, y por qué el proceso sale sin cerrar

Ejecutar mil `draw()` fuera de su sitio natural encuentra, tarde o temprano, uno que da
por hecho algo que la escena de fábrica no tiene y se lleva el proceso por delante. La
primera pasada murió en `NODE_MT_category_GEO_OUTPUT` sin escribir nada.

Hay una barrera de señales (`SIGSEGV`, `SIGBUS`, `SIGFPE`, `SIGILL`) alrededor de cada
`draw()`: atrapa, anota el tipo como `NO-CUBIERTO motivo=fallo` y sigue. Dos reglas que
la hacen honesta:

- Lo que cae en la barrera **nunca cuenta como idéntico**. Se lista en el parte.
- Si la barrera actuó aunque sea una vez, el proceso sale con `_exit()` **sin el cierre
  ordenado**: volver de un `SIGSEGV` deja el montón en un estado que nadie puede dar por
  bueno, y el cierre de Blender lo recorre entero liberando. El fichero ya está escrito;
  cerrar bonito solo serviría para caerse al final y mentir en el código de salida.

Hoy la barrera **no actúa ni una vez** — las dos causas reales (menús de otro editor y
sub-paneles instanciados) están arregladas de raíz. Sigue puesta porque la siguiente
tanda de migraciones volverá a encontrar alguna.

### El árbol de layout se libera al resolverlo

`UI_block_layout_resolve()` llama a `ui_layout_free()`. Si el volcador dejara que la
región dibujase normalmente, para cuando fuera a mirar el árbol ya no habría árbol. Por
eso monta el bloque él mismo, llama al `draw()`, **serializa**, y solo después libera
con `UI_block_layout_free()` + `UI_block_free()` — sin resolver nunca.

### Un panel no se dibuja con sus hijos

`UI_paneltype_draw()` dibuja también los sub-paneles de `pt->children`. El volcador no
lo usa: cada bloque es exactamente lo que produce **el `draw()` de ese tipo**, y los
sub-paneles tienen su propio bloque. Es la unidad correcta para verificar una
migración panel a panel.

### El volcado de dibujo depende de la escala de la interfaz

Los anchos que se pasan a `UI_block_layout` van en `UI_UNIT_X`, que depende del DPI y de
la escala de la interfaz del usuario. La línea base y la comprobación tienen que
generarse con la misma escala; por eso las dos se lanzan con `--factory-startup`.

## Qué falta

- El resto de `bl_ui`. La lista, por tamaño, está en `BACKLOG-EDITOR-PYTHON.md`.
- Los paneles que el volcado de dibujo marca `NO-CUBIERTO`: los que no pasan el `poll`
  en una escena de fábrica (necesitan un modificador, un sistema de partículas, un
  hueso…) y los de espacios que no se pueden abrir. Se listan en cada pasada y en el
  informe: lo que no se verifica, se dice.
- `bpy.utils.register_class` para paneles de terceros: sin intérprete no hay paneles de
  add-on. No afecta a los de `bl_ui`.

### Medir sobre una copia, no sobre la carpeta de compilación compartida

Tres volcados seguidos dieron 1.700, 1.701 y 2.011 bloques. No era el volcador: otro
carril estaba haciendo `nb install`, que **reescribe** `Blender.app` —binario y
scripts— mientras corrían. Un arranque que pilla el directorio de scripts a medias
importa menos módulos de `bl_ui` y registra menos paneles.

Con una copia privada (`cp -R .../Blender.app /tmp/Blender-D1.app`) los tres volcados
salieron **idénticos byte a byte**. Cuando haya más de un carril compilando, el
verificador se ejecuta sobre esa copia, no sobre `dev/build`.

De paso, esa misma carrera fabricó un falso positivo: un menú de lápiz de cera apareció
como `NO-CUBIERTO motivo=fallo` en una pasada y dibujado en la siguiente. Estuvo a punto
de quedarse vetado "por inestable". Sobre la copia limpia dibuja perfectamente las tres
veces, así que el veto se quitó. **Una excepción escrita a partir de una medición sucia
es peor que no tener excepción**: esconde un tipo que sí se puede verificar.

### La línea base es una foto, no una constante

Mientras otros carriles migran interfaz, el número de bloques sube. Eso no es una
regresión: es la migración avanzando. La línea base se regenera a propósito, en el
commit que cambia la interfaz, y se dice allí. Lo que nunca debe pasar es regenerarla
"porque salía rojo".

## Lo que enseñó la primera migración completa (`properties_game.py`)

14 paneles y 2 menús, 899 líneas de Python, a
`source/blender/editors/space_buttons/fl_game_buttons.cc`. Verificado con los dos
volcados contra la línea base congelada:

- **Los 16 tipos salen idénticos**, campo a campo y botón a botón. Cero diferencias en
  el registro y cero en el dibujo.
- **Queda una diferencia, y es la que la política predecía**: el bloque
  `REGION PROPERTIES WINDOW`. Los 14 paneles se han movido dentro de la lista de la
  región, de la posición ~200 a la ~27, sin que ninguno de sus campos cambie.

El motivo es exactamente el de la trampa de `order`: `properties_game.py` se registraba
al cargar los scripts, y el C++ se registra en `ED_spacetypes_init()`, mucho antes. Los
paneles vecinos son todos de Python con `order` 0, así que el desempate lo decide el
momento del registro, y ese momento ha cambiado.

**No se ha "arreglado" inventando un `order`.** Los `bl_order` se han copiado tal cual
estaban (1000 en las pestañas de Juego y Física, 0 en Escena y Objeto): poner un 1000
donde el Python tenía 0 sería inventarse un dato para que el verificador calle. La
diferencia se queda escrita aquí y en el commit.

**Desaparece sola cuando migren los vecinos**: en cuanto las pestañas de Propiedades
sean C++, todos se registran en el mismo sitio y el orden vuelve a ser el de `order`.
Mientras tanto, el usuario ve estos paneles en su misma pestaña, con su misma etiqueta y
su mismo contenido, antes que los de Python en vez de después.

La lección para el resto de `bl_ui`: **migrar por pestañas o por editores completos, no
panel suelto a panel suelto.** Un editor entero migrado de una vez no tiene vecinos
Python con los que desempatar, y el orden sale exacto.

## D3: los dos editores de ÁNIMA, medidos antes de tocarlos

La norma que salió de D2 —migrar editores completos, no paneles sueltos— obliga a medir
el editor entero antes de empezar. Medidos los dos que pide el flujo de ÁNIMA:

| | `space_node.py` | `space_image.py` |
|---|---:|---:|
| Líneas | 1.209 | 1.585 |
| Tipos que registra | 30 | 60 |
| Cabeceras | 1 | 2 |
| Menús | 11 | 9 |
| Paneles | 18 | 46 |
| **UIList** | 0 | **2** |
| **AssetShelf** | 0 | **1** |
| Superficie ajena a reimplementar | ~285 líneas | ~1.965 líneas compartidas |

### El editor de nodos **sí** cabe entero

Los 30 tipos son todos `Panel`, `Menu` o `Header`, que es exactamente lo que
`FL_ui_registry` sabe declarar. Todas las llamadas de dibujo tienen equivalente C++
comprobado: `uiTemplateID`, `uiTemplateHeader`, `uiTemplateNodeTreeInterface`,
`uiItemPopoverPanel`, `uiItemMenuEnumO`, `uiItemMContents` (el `menu_contents` del
Python) y `uiItemSpacer` (el `separator_spacer`).

Su superficie ajena son **7 paneles clonados** con la factoría `node_panel()`, que copia
una clase de las pestañas de Propiedades y la re-registra como `NODE_<nombre>` en la
barra lateral, categoría "Options": `EEVEE_NEXT_MATERIAL_PT_settings` (+`_surface`,
+`_volume`), `MATERIAL_PT_viewport`, `WORLD_PT_viewport_display`, `DATA_PT_light` y
`DATA_PT_EEVEE_light` — 187 líneas en total — más `AnnotationDataPanel` (98).

**Dónde está la línea de propiedad:** los originales viven en las pestañas de
Propiedades y son de otro carril; los clones `NODE_*` son del editor de nodos y por
tanto de este. Reimplementar sus siete `draw()` aquí duplica lógica a sabiendas, y hay
que escribirlo: cuando las pestañas de Propiedades sean C++, los dos sitios se unifican
en una función compartida. Duplicar sin decirlo es como se desincronizan las cosas.

Lo único que se pierde de verdad es la rama `nodeitems_utils` de `NODE_MT_add`, que
dibuja las categorías de nodos de add-ons de terceros. Sin intérprete no hay add-ons,
así que esa capacidad ya estaba condenada; los cuatro árboles de nodos integrados
(geometría, composición, sombreado y textura) van por `menu_contents` y no dependen de
ella.

### El editor de imagen/UV **no** cabe, y no es cuestión de tamaño

Dos obstáculos duros, ninguno resoluble escribiendo más rápido:

1. **`FL_ui_registry` no sabe declarar `UIList` ni `AssetShelf`.** `space_image.py`
   registra `IMAGE_UL_render_slots`, `IMAGE_UL_udim_tiles` y `IMAGE_AST_brush_paint`.
   El motor tiene `uiListType` y `AssetShelfType`, así que es posible, pero hay que
   añadir `UIListDecl` y `AssetShelfDecl` al registro **antes** de intentar el editor.
   Es el primer trabajo del que lo coja.
2. **14 llamadas a `properties_paint_common`** (1.965 líneas): `brush_settings`,
   `brush_texture_settings`, `brush_basic_texpaint_settings`, `draw_color_settings` y
   los mixins `UnifiedPaintPanel`, `BrushSelectPanel`, `ClonePanel`, `StrokePanel`,
   `FalloffPanel`, `DisplayPanel`… Ese módulo lo comparten los modos de pintura de la
   vista 3D y las pestañas de Propiedades, o sea **otros dos carriles**. Migrarlo desde
   aquí sería pisar dos dominios a la vez, y migrar solo el trozo que usa el editor de
   imagen lo dejaría duplicado.

**Orden recomendado para el editor de imagen/UV**, cuando se acometa:

1. Añadir `UIListDecl` y `AssetShelfDecl` a `FL_ui_registry`, con su volcado
   correspondiente en `--fl-dump-ui` (hoy no salen en el registro: el volcador recorre
   `paneltypes` y `headertypes`, y los `uiListType` viven en otro sitio). **Esto es
   deuda del propio verificador y conviene saberla**: un `UIList` migrado hoy no lo
   vería nadie.
2. Migrar `properties_paint_common` a C++ como módulo compartido, coordinado con el
   carril de la vista 3D y el de Propiedades.
3. Entonces, y solo entonces, el editor de imagen/UV entero: 46 paneles, 9 menús,
   2 cabeceras, 2 listas y una estantería de recursos.

### El tercer obstáculo, que no es de ninguno de los dos editores: `PresetPanel`

Al bajar al detalle del editor de nodos apareció un bloqueo que no estaba en la primera
medición y que es **transversal a todo `bl_ui`**.

`NODE_PT_node_color_presets` hereda de `PresetPanel` (`bl_ui/utils.py`), cuyo `draw()`
llama a `Menu.draw_preset()` (`bpy_types.py`): enumera los ficheros de una carpeta de
presets y dibuja un elemento por cada uno, más el campo de nombre y el botón de añadir.

El carril de presets migró a C++ **los datos** —leer, aplicar, escribir y capturar un
preset (`source/blender/windowmanager/preset/`)— pero **no el dibujo del menú**: no hay
API C++ que recorra el directorio y pinte la lista. Sin ella, `PresetPanel` no se puede
migrar.

No es un detalle del editor de nodos: **17 clases de 12 ficheros de `bl_ui` heredan de
`PresetPanel`** — cámara, áreas seguras, materiales de lápiz, fluidos, formato y FFmpeg
de salida, balance de blancos, trazado de rayos, dinámica de pelo, color de nodo, tres
del editor de clips, tela, editor de texto de preferencias y pinceles de la vista 3D.

**Y ojo con la trampa que casi cuela**: en una instalación de fábrica la carpeta de
presets no existe, así que el volcado de la línea base para estos paneles es un
`* Missing Paths *` y poco más. Reproducir *eso* en C++ haría pasar al verificador
mientras se pierde la capacidad entera. Sería exactamente la verificación que no
verifica nada. El trabajo de verdad es la API de enumeración, y su sitio natural es el
módulo de presets, no el fichero de cada editor.

**Prerrequisito, por tanto, para cerrar el editor de nodos**: una función C++ que
enumere una carpeta de presets y dibuje el menú, en `source/blender/windowmanager/preset/`.
Con ella, el editor de nodos queda cerrable de una sentada: los otros 29 tipos no tienen
más obstáculo que escribirlos.

## D4.1: el panel de presets, en C++ — desbloqueadas 17 clases

`source/blender/windowmanager/preset/FL_preset_ui.{hpp,cc}`: el equivalente nativo de
`Menu.path_menu()` + `Menu.draw_preset()` (`bpy_types.py`) y de `PresetPanel`
(`bl_ui/utils.py`). Se apoya en el sistema de presets que ya estaba en C++
(`FL_preset.hpp`, `PRESETS-A-DATOS.md`): eso migró los **datos**, esto migra el
**dibujo**, que era lo que faltaba.

Un panel de presets queda ahora en una declaración:

```cpp
static const flipendo::preset::ui::MenuSpec node_color = {
    /*subdir*/ "node_color",
    /*op*/ "SCRIPT_OT_execute_preset",
    /*menu_idname*/ "NODE_PT_node_color_presets",
    /*add_op*/ "NODE_OT_node_color_preset_add",
};
/* ...y en el draw del PanelDecl: */
flipendo::preset::ui::draw_panel(C, panel->layout, node_color);
```

### Cómo se verificó, y por qué no vale hacerlo de la forma fácil

La trampa estaba señalada de antemano: **en instalación de fábrica la carpeta de presets
no existe**, así que lo que dibuja el Python es un `* Missing Paths *` y poco más.
Escribir una función que pinte esa etiqueta habría pasado el verificador con la
capacidad entera perdida.

Así que se verificó con una carpeta **llena de verdad**, con seis ficheros elegidos para
que cada uno probara una regla distinta:

| Fichero | Qué prueba |
|---|---|
| `Azul_frio.fpreset` | el guion bajo pasa a espacio y **no** se pone en mayúsculas (`title_case=False`) |
| `rojo_intenso.fpreset` | el orden es insensible a mayúsculas |
| `Verde 3.fpreset` y `Verde 10.fpreset` | el **orden natural**: 3 antes que 10, no alfabético |
| `amarillo_viejo.py` | los presets `.py` que el usuario guardó antes de la migración se siguen listando |
| `.oculto.fpreset` | los ocultos no se listan |
| `ignorame.txt` | las extensiones que no son de preset no se listan |

Resultado, comparando el árbol de `uiLayout` del Python contra el del C++ con el mismo
serializador (`--fl-dump-ui-layout` frente a `--fl-dump-preset-panel`):

- **Carpeta llena: 23 de 23 líneas idénticas.** Mismo orden, mismos nombres, mismas
  rutas, los dos operadores por fila con sus argumentos (`remove_name=True`), el
  separador, el campo `preset_name` con su cambio de relieve y el botón de añadir.
- **Carpeta vacía: 9 de 9 líneas idénticas**, incluido el `* Missing Paths *`.

`--fl-dump-preset-panel` se queda en el árbol: es el arnés que permite repetir esta
comprobación cuando alguien toque el listado.

### Lo que esto desbloquea

Las 17 clases de 12 ficheros que heredaban de `PresetPanel` ya no tienen impedimento.
Y con ellas, el editor de nodos: era su único bloqueo.

## D4.2: listas y estanterías — cerrada la deuda del propio verificador

Hasta ahora `--fl-dump-ui` recorría `paneltypes` y `headertypes` de cada región, más el
registro global de menús. Los `uiListType` y los `AssetShelfType` **viven en registros
aparte**, así que no salían: una lista migrada a C++ no la habría visto nadie, y el
verificador habría dicho "0 diferencias" con una capacidad entera perdida. Era deuda
mía, y la peor clase: la que hace que una herramienta de verificación mienta.

Crecen las dos cosas a la vez, que es la única forma de que esto no vuelva a pasar:

- **`FL_ui_registry`**: `UIListDecl` + `uilists_register()` y `AssetShelfDecl` +
  `asset_shelves_register()`.
- **El volcado**: bloques `UILIST <idname>` (con qué callbacks tiene: `draw_item`,
  `draw_filter`, `filter_items`, `listener`) y `ASSETSHELF <idname>` (espacio, operador
  de activación, banderas, tamaño de previsualización y sus cuatro callbacks).
- Dos iteradores que no existían: `WM_uilisttypes_registered_get()` y
  `ed::asset::shelf::types_all()`.

Cifras: el volcado de registro pasa de **2.072 a 2.123 bloques** — aparecen **41 listas
y 11 estanterías** que antes eran invisibles.

**Verificado que detecta**, que es lo único que importa en un verificador: se renombró a
mano `IMAGE_UL_render_slots` en la copia instalada y la comprobación lo cazó,

```
SOBRA   UILIST IMAGE_UL_render_slots_CAMBIADO (no esta en la linea base)
FALTA   UILIST IMAGE_UL_render_slots (esta en la linea base y ya no se registra)
Interfaz (registro): 2123 bloques, 2122 identicos, 0 distintos, 1 faltan, 1 sobran.
```

con código de salida 1. Deshecho el cambio, vuelve a 2.123/2.123.

Queda pendiente, y se dice: el volcado de **dibujo** no ejecuta el `draw_item` de una
lista ni el contenido de una estantería. Para una lista habría que fabricar un elemento
de datos que dibujar, y eso es inventarse el dato — el mismo motivo por el que los
paneles instanciados salen `NO-CUBIERTO`. Lo que sí queda cubierto es que la lista
**existe, con los callbacks que debe tener**, que es donde estaba el agujero.

## Estado del camino crítico, tras D4

Los dos obstáculos que impedían cerrar editores enteros **ya no están**:

| Obstáculo | Estado |
|---|---|
| `PresetPanel` sin dibujo en C++ (17 clases, 12 ficheros) | **Resuelto** (D4.1) |
| `FL_ui_registry` sin `UIList` ni `AssetShelf`, e invisibles al volcado | **Resuelto** (D4.2) |
| `properties_paint_common` compartido entre tres carriles | Sigue en pie |
| Los 7 clones `node_panel()` de las pestañas de Propiedades | Dependencia: funciones compartidas que escribirá el carril de Propiedades |

### El editor de nodos está listo para que lo cojan

Queda **medido, con las APIs verificadas una a una y sin bloqueos propios**: 30 tipos —
1 cabecera, 11 menús, 18 paneles—, 1.209 líneas de Python. Lo único que espera son las
funciones compartidas de los 7 paneles clonados; hasta que existan, **no se escriben los
clones**: la decisión del proyecto es función compartida, no copia, porque dos copias se
desincronizan y aquí se busca propiedad total del código, no duplicado.

El resto del fichero no depende de eso y puede escribirse ya.

### El editor de imagen/UV, después

Le queda un solo obstáculo de fondo, `properties_paint_common` (1.965 líneas, 14
llamadas desde `space_image.py`), que hay que migrar como módulo compartido de acuerdo
con los carriles de la vista 3D y de Propiedades. Sus dos listas y su estantería ya se
pueden declarar y ya se ven en el volcado.
## B8.1: la pestaña Mundo completa, en C++

Los 11 paneles que registraba `properties_world.py` viven ahora en
`source/blender/editors/space_buttons/fl_world_buttons.cc`. Se retiraron juntos del
registro Python, incluida la animacion, las propiedades personalizadas, la superficie
y el volumen por nodos y todos los subpaneles de ajustes de EEVEE.

Verificacion contra las dos lineas base congeladas con el Python activo:

- Registro: los 11 bloques de Mundo son identicos; ninguno falta ni sobra. El bloque
  global `REGION PROPERTIES WINDOW` cambia de orden porque el registro nativo ocurre
  antes que el de los paneles Python restantes, la misma diferencia transitoria ya
  documentada para `properties_game.py`.
- Dibujo: los 11 de 11 arboles `uiLayout` son identicos; ninguno falta ni sobra.
  El parte global fue 1.972 de 2.004 bloques identicos, con 32 diferencias y 9
  ausencias, todas ajenas a Mundo y procedentes de otras migraciones ya integradas.
- Build completa `nb-codex install`: enlace verde.

`WORLD_PT_viewport_display` tiene ademas un clon en el editor de nodos. Su dibujo C++
se expone desde `FL_properties_ui.hpp`, para que la futura migracion de `space_node.py`
lo reutilice sin duplicar logica. Hasta que ese editor se migre, queda en
`properties_world.py` solo la declaracion Python que importa `space_node.py`; el modulo
ya no figura en `bl_ui.__init__` y por tanto no registra ningun panel de la pestaña
Mundo.

Durante la integracion aparecieron dos opciones de linea de comandos de Mirror UV que
llamaban a `dump_mirror_uv()` y `check_mirror_uv()`, nombres inexistentes en
`FL_mesh_ops_selftest.hh`. Las unicas funciones reales, `dump()` y `check()`, prueban
`paint.vertex_color_dirt` y no son equivalentes. Las opciones se dejaron desactivadas
con un `TODO(Carril C)` explicito hasta que exista el arnes especifico; conectarlas al
test equivocado habria producido evidencia falsa.

## D5: las líneas base, sobre un bundle limpio

`cmake --install` **copia pero no borra**. El bundle instalado arrastraba **403 ficheros
`.py` huérfanos**: 700 en `Blender.app` frente a 304 en el repo. Entre ellos, seis
add-ons que la poda a Mac-only ya había retirado del árbol —`io_anim_bvh`,
`io_curve_svg`, `io_mesh_uv_layout`, `io_scene_fbx`, `io_scene_gltf2` y `pose_library`—
y que el arranque de fábrica **seguía activando**, porque el binario los encontraba.

Las líneas base se habían congelado contra ese bundle rancio. Es decir: parte de lo que
certificaban **no era capacidad del árbol actual**, sino restos de instalaciones
anteriores. Un verificador que mide contra un bundle sucio certifica ficción.

### Qué desaparece, y por qué

Diez bloques, todos de los seis add-ons retirados — comprobado uno a uno buscando su
clase en el bundle antes de borrarlo:

| Bloque | Venía de |
|---|---|
| `DOPESHEET_PT_asset_panel`, `ASSETBROWSER_MT_asset`, `VIEW3D_MT_pose_modify`, `VIEW3D_AST_pose_library` | `pose_library` |
| `BVH_PT_import_main`, `BVH_PT_import_transform`, `BVH_PT_import_animation`, `BVH_PT_export_transform`, `BVH_PT_export_animation` | `io_anim_bvh` |
| `SCENE_UL_gltf2_filter_action` | `io_scene_gltf2` |

Y con ellos, dos bloques `REGION` que solo pierden esas entradas de su lista
(`DOPESHEET_EDITOR UI` y `FILE_BROWSER TOOL_PROPS`).

**No es una regresión: es dejar de contar lo que ya no existe.**

### La tercera diferencia, que no era de los add-ons

`REGION PROPERTIES WINDOW` también salía distinta, y la causa es otra: el **mismo
conjunto** de 748 paneles (ni uno de más ni de menos) en **otro orden**. Los paneles de
Mundo —`WORLD_PT_context_world`, los siete `EEVEE_WORLD_PT_*`, `WORLD_PT_viewport_display`,
`WORLD_PT_animation` y `WORLD_PT_custom_props`— saltaron de la posición ~664 a la ~3.

Es el efecto de `order` documentado en D2, esta vez sobre la migración de otro carril
(`3b52ebbe95b`, «Propiedades: la pestaña Mundo pasa a C++»): al registrarse en
`ED_spacetypes_init()` en vez de al cargar los scripts, se adelantan. El bloque `REGION`
lo cazó, que es exactamente para lo que se añadió.

**Se comprobó antes de absorberla.** Regenerar una línea base «porque salía rojo» es la
forma de convertir un verificador en un sello de goma; cada una de las trece diferencias
tiene aquí su causa por escrito.

### Cifras, ya sobre bundle limpio

| | Antes (bundle rancio) | Ahora (limpio) |
|---|---:|---:|
| `.py` en el bundle | 700 (403 huérfanos) | **297 (0 huérfanos)** |
| Registro | 2.123 bloques | **2.113** |
| Dibujo | 2.012 bloques | **2.004** (1.095 dibujados, 909 no cubiertos) |

Las dos comprobaciones: **0 distintos, 0 faltan, 0 sobran**. Los dos volcados,
reproducibles byte a byte. Y sigue detectando: cambiada a mano una etiqueta de
`space_image.py`, salieron sus 3 bloques como diferencia y el código de salida fue 1;
deshecho, vuelve a 2.113/2.113.

### La regla que queda

**El verificador se mide sobre un bundle recién instalado**, no sobre el que lleva
semanas acumulando. Antes de congelar una línea base: borrar el árbol de scripts del
bundle, `nb install`, y comprobar que no quedan huérfanos
(`comm -23 <bundle .py> <repo .py>` tiene que salir vacío).

## D6: la línea base no puede llevar dentro dónde está instalado el programa

La batería final, pasada desde una copia del bundle en otra ruta, dio **21 bloques
distintos** sin que nada hubiera cambiado. La causa: los menús de presets y de
plantillas meten en el argumento del operador la **ruta completa** del fichero, que
empieza por donde esté `Blender.app`:

```
base:  ...script.execute_preset(filepath="/tmp/Blender-D5.app/.../presets/camera/1_inch.fpreset"...)
ahora: ...script.execute_preset(filepath="/tmp/Blender-FINAL.app/.../presets/camera/1_inch.fpreset"...)
```

Con la ruta dentro, **la línea base solo valía desde el directorio exacto donde se
congeló**. Es el mismo defecto que la fecha de compilación del menú «Acerca de», y se
arregla con el mismo criterio: *lo que no es del árbol, no entra en la línea base*.

El volcado sustituye ahora el prefijo de la carpeta de scripts por `<SCRIPTS>` — 203
rutas en el volcado de dibujo. Lo comparado sigue siendo lo que importa (que el fichero
es ese, con ese nombre y en ese orden) y deja de depender de la instalación.

**Verificado desde tres rutas distintas** —dos copias en `/tmp` y el propio
`dev/build/bin/Blender.app`—: las dos comprobaciones dan **0 distintos, 0 faltan, 0
sobran** en las tres, y los volcados se reproducen byte a byte entre bundles distintos.

## Cifras finales de la noche

Sobre bundle recién instalado (291 `.py`, **0 huérfanos**):

| | Bloques | Resultado |
|---|---:|---|
| Registro (`--fl-dump-ui`) | **2.113** | 1.390 paneles · 589 menús · 25 cabeceras · 59 regiones · 40 listas · 10 estanterías |
| Dibujo (`--fl-dump-ui-layout`) | **2.004** | **1.095 dibujados**, 909 no cubiertos, 0 fallos |

`--fl-check-ui`: **2.113/2.113** y **2.004/2.004**, cero diferencias, desde cualquier
ruta. Y sigue detectando: cambiada a mano la etiqueta de `NODE_PT_backdrop`, sale el
bloque con su línea y salida 1; deshecho, vuelve a cero.

## El editor de nodos: entrega para quien lo coja

**No se migró**, y la razón es de reloj, no de capacidad: a las 04:25, con la cabecera
—190 líneas de Python con ramas por tipo de árbol, `template_ID` con operadores `new=`,
popovers y `separator_spacer`— aún sin empezar, no había margen para escribirlo **y**
verificarlo antes de las 06:00. Un editor a medias aborta el registro de todo `bl_ui`,
así que se paró con el árbol limpio en vez de a mitad.

Queda leído entero y desmenuzado. Lo que necesita quien siga:

**29 tipos propios**, todos con su equivalente C++ ya comprobado:

| Construcción del Python | C++ |
|---|---|
| `layout.template_header()` | `uiTemplateHeader(layout, C)` |
| `layout.template_ID(ptr, "prop", new="op")` | `uiTemplateID(layout, C, &ptr, "prop", "op", nullptr, nullptr)` |
| `layout.menu_contents("X")` | `uiItemMContents(layout, "X")` |
| `layout.separator_spacer()` | `uiItemSpacer(layout)` |
| `row.popover(panel="X")` | `uiItemPopoverPanel(row, C, "X", ...)` |
| `layout.operator_menu_enum("op", "prop")` | `uiItemMenuEnumO(layout, C, "OP_OT_x", "prop", name, icon)` |
| `layout.template_node_tree_interface(...)` | `uiTemplateNodeTreeInterface(layout, C, &ptr)` |
| `layout.template_node_inputs(node)` | `uiTemplateNodeInputs(layout, C, &ptr)` |
| `layout.panel("id")` | `layout->panel(C, "id", default_closed)` |
| `self.bl_label = ...` en `draw_header` | `UI_panel_drawname_set(panel, nombre)` |
| `Menu.draw_collapsible()` | el patrón de `logic_ui.cc` |
| `NODE_PT_node_color_presets` | `flipendo::preset::ui::draw_panel()` (D4.1) |

Campos del DNA ya localizados: `SpaceNode::tree_idname` (el `tree_type` del Python es
una cadena, no una enumeración), `edittree`, `nodetree` y `flag & SNODE_BACKDRAW`.

**8 tipos que NO deben escribirse aquí**, porque son de otro dominio y la decisión del
proyecto es función compartida, no copia:

- Los **7 clones** de `node_panel()` desde las pestañas de Propiedades.
- **`NODE_PT_annotation`**, que hereda `AnnotationDataPanel` (98 líneas compartidas con
  la vista 3D, el editor de clips, el de imagen y el de secuencias). Su sitio es una
  función pública de anotaciones, no el fichero de cada editor.

Mientras esas ocho no existan como función compartida, `space_node.py` se queda
reducido a ellas: sin colisión de `idname` y con el resto en C++.

Lo único que se pierde de verdad al migrar: la rama `nodeitems_utils` de `NODE_MT_add`,
que dibuja categorías de nodos de add-ons de terceros. Sin intérprete no hay add-ons.

## T2: el editor de nodos, cerrado — 27 de sus 34 tipos en C++

`scripts/startup/bl_ui/space_node.py` pasa de **1.063 líneas y 34 clases
registradas a 84 líneas y 7**. Los 27 tipos propios viven en
`source/blender/editors/space_node/fl_node_ui.cc`: `NODE_HT_header`, 8 menús, los
6 paneles emergentes de la cabecera, los 11 de la barra lateral y
`NODE_PT_node_color_presets`. Con `fl_node_menus.cc` (los tres del keymap) el
editor queda entero salvo los clones de otro dominio.

### Las cifras

Sobre una **copia privada** del bundle recién instalado (`ditto`, 4.193 ficheros,
0 `.py` huérfanos respecto al repo):

| Volcado | Bloques | Resultado |
|---|---:|---|
| Registro (`--fl-dump-ui`) | **2.113** | 2.110 idénticos, **3 distintos**, 0 faltan, 0 sobran |
| Diseño (`--fl-dump-ui-layout`) | **2.004** | **2.004 idénticos, 0 distintos**, 0 faltan, 0 sobran |

Y los 27 tipos migrados, comparados uno a uno: **27/27 idénticos en el registro y
27/27 en el diseño**. Los dos volcados se reproducen byte a byte. El verificador
sigue detectando: cambiada a mano la etiqueta de `NODE_PT_backdrop` en una copia de
la línea base, la caza con su bloque y su línea y sale con código 1.

Las **3 diferencias del registro son bloques `REGION`**, o sea listas que cambian
de orden sin que cambie ningún campo —la trampa de `order` ya documentada—, y una
de ellas ni siquiera es de este cambio:

1. `REGION NODE_EDITOR UI`: mismo conjunto, dos intercambios. `NODE_PT_annotation`
   baja del puesto 10 al 11 (sigue siendo Python, así que se registra después) y
   `NODE_WORLD_PT_viewport_display` adelanta a `NODE_MATERIAL_PT_viewport` (los dos
   con `order` 10). **No cumple la regla del prefijo** de
   `MENUS-DEL-KEYMAP-A-CPP.md`: lo que queda en Python no es el sufijo de la lista,
   está intercalado. Desaparece cuando se migren los 7 que faltan.
2. `REGION PROPERTIES HEADER`: `NODE_PT_node_color_presets` pasa del puesto 15 al 1
   entre 18 paneles de preset que siguen siendo Python.
3. `REGION PROPERTIES WINDOW`: **ajena**. Viene de tres commits del carril de
   Propiedades posteriores a la línea base (`3b961a50532`, `dfdaeebe94c`,
   `d94d9da8b97`). Se comprobó mirando los commits, no suponiéndolo.

### La función compartida se usó en cuanto existió

De los 8 tipos que la medición de D3 dejó fuera «porque son de otro dominio»,
**uno ya tenía su función compartida escrita** y por eso entra:
`NODE_WORLD_PT_viewport_display` llama a
`flipendo::properties_ui::world_viewport_display_draw()`, que dejó el carril de
Propiedades al migrar la pestaña Mundo. Ni una línea duplicada. Es la prueba de que
la regla «función compartida, nunca copia» no es un freno: en cuanto la función
existe, el clon se migra en cuatro líneas.

Los otros **7 siguen en Python** —sin colisión de `idname`, porque el C++ no
declara ninguno—: los 4 de `properties_material.py`, los 2 de
`properties_data_light.py` y `NODE_PT_annotation`, que hereda `AnnotationDataPanel`
de `properties_grease_pencil_common.py`. Reimplementarlos aquí serían ~285 líneas
duplicadas a sabiendas.

Efecto lateral que conviene saber: **`properties_world.py` ya no lo importa nadie**.
Solo lo hacía `space_node.py`, para ese clon. No se borra desde aquí porque es
fichero de otro carril.

### Trampas nuevas, las que costaron tiempo

**Hay `draw()` de Python que revientan a medias, y la línea base recoge el medio
dibujo.** En la escena de fábrica `snode.node_tree` es `None`, así que
`NODE_PT_geometry_node_tool_object_types` y `..._mode` dejaban dibujada la columna
y nada más (la excepción salta en `col.active = group.is_tool`, después de crear la
columna), `..._options` no dibujaba nada, y
`NODE_MT_node_tree_interface_context_menu` salía vacío. **El C++ tiene que cortar
exactamente donde cortaba la excepción**, ni antes ni después. Sin eso el volcado de
diseño no habría dado 2.004/2.004.

Esto no es «codificar un fallo» en el sentido de `OUTLINER_MT_context_menu`: ahí el
dibujo se corta por un artefacto del volcador; aquí se corta porque la escena no
tiene los datos, y con datos no se corta. El corte está marcado en cada sitio del
`.cc` con su motivo.

**`'GPENCIL'` ya no existe.** El conjunto `types_that_support_material` de la
cabecera lo lleva, pero el identificador de RNA en 4.5 es `'GREASEPENCIL'`: hoy los
objetos de lápiz de cera **no** están en ese conjunto. Se copia tal cual,
comparando identificadores de RNA, para no cambiar el comportamiento a escondidas
de una migración. Arreglarlo es otro cambio, con su línea base regenerada.

**Un panel de presets vive en el editor de Propiedades.** `PresetPanel` tiene
`bl_space_type = 'PROPERTIES'` y `bl_region_type = 'HEADER'`, así que
`NODE_PT_node_color_presets` —y los otros 16 herederos— se registran en la cabecera
del editor de Propiedades, no en la de su editor. Y `ED_spacetype_node()` corre
**antes** que `ED_spacetype_buttons()`, así que esa región todavía no existe: hay
que registrarlo desde `ED_spacetypes_init()` después del segundo. Quien migre otro
`PresetPanel` se encontrará lo mismo.

**`operator_menu_enum` sin texto no cabe en `uiItemMenuEnumO()`**, que exige un
`StringRefNull`. Para que el nombre lo ponga el operador (el "Lasso Select" de
`NODE_MT_select`) hay que bajar a `uiItemMenuEnumFullO_ptr()` con `std::nullopt`.

**La regla de medir sobre una copia vale también para el `cp`.** Un `cp -R` del
bundle lanzado mientras otro carril instalaba produjo una copia **incompleta** —sin
el binario y sin `addons_core`— y con ella el verificador dio un falso «4 distintos,
6 faltan» de `bl_pkg`. Con `ditto` y comprobando que origen y copia tienen los
mismos 4.193 ficheros, limpio. **Antes de creerse una diferencia, cuenta los
ficheros de la copia.**
