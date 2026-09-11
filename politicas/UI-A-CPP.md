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
