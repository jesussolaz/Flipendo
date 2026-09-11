# Los presets: de código Python a datos que lee C++

> Uno de los cinco sitios donde el C++ del editor todavía llamaba de vuelta al
> intérprete. No porque los presets hicieran nada complicado, sino porque estaban
> *escritos* como scripts: `bpy.context.scene.render.fps = 30` en un `.py` que el
> editor ejecutaba con `bpy.utils.execfile`. Eso es dato disfrazado de código.

## Qué había: la auditoría de los 173 ficheros

`scripts/presets` tenía **173 ficheros `.py` y 10.287 líneas**. Medido, no
estimado. La cifra engaña, porque dos ficheros se llevan el 88 % de las líneas:

| Grupo | Ficheros | Líneas | Qué son |
|---|---:|---:|---|
| **(a) Asignaciones RNA puras** | **165** | **1.125** | `ruta = literal`, con alias locales (`camera = bpy.context.edit_movieclip.tracking.camera`). Nada más. |
| **(b) Con lógica** | 6 | 107 | 5 de FFmpeg con un condicional NTSC/PAL, y `text_editor/Visual_Studio_Code.py` con `import platform` y un `match`. |
| **(b') keyconfig** | 2 | 9.055 | `keymap_data/blender_default.py` (8.669) y `keyconfig/Blender.py` (386). No son presets de propiedades: **construyen** el keymap. |
| **(c) Sin uso en Flipendo** | 0 | 0 | Ninguno. Hasta `framerate/Custom.py`, que solo contiene `import bpy`, sigue vivo: es la entrada «Custom» del menú de fotogramas. |

Cómo se separaron: se descartó toda línea vacía, comentario, `import bpy` y
`nombre = ...`, y se miró lo que quedaba. Salieron exactamente 18 líneas
sospechosas en 6 ficheros — cinco `if is_ntsc:` con su `else:`, un `import
platform`, un `match` y sus dos `case`. El resto son 165 ficheros que no hacen
otra cosa que asignar.

Las familias, por raíz de contexto (esto importa para verificar, ver abajo):

| Raíz | Familias | Ficheros |
|---|---|---:|
| `context.scene` | render, framerate, ffmpeg, pixel_density, safe_areas, color_management/white_balance, cycles/*, eevee/raytracing | 83 |
| `context.preferences` | text_editor | 2 |
| `context.camera` | camera | 31 |
| `context.edit_movieclip` | tracking_camera, tracking_settings, tracking_track_color | 39 |
| `context.cloth` | cloth | 5 |
| `context.fluid` | fluid | 3 |
| `context.object` | gpencil_material | 3 |
| `context.particle_system` | hair_dynamics | 1 |
| `context.active_operator` | operator/wm.collada_export | 2 |

### La familia keyconfig, que era la delicada

El aviso era justo: el keymap por defecto ya es C++
([`KEYMAP-A-CPP.md`](KEYMAP-A-CPP.md)) y podía haber solapamiento. Lo hay, y esto
es lo que queda vivo de verdad:

- `bpy.utils.keyconfig_init()` **no lo llama nadie**. Era el único cliente de
  `presets/keyconfig/Blender.py` al arrancar, y `WM_keyconfig_reload` dejó de
  invocarlo cuando el keymap pasó a C++.
- Pero `bpy.utils.keyconfig_set(filepath)` **sí** sigue vivo, desde tres sitios:
  `preferences.keyconfig_activate` (el menú `USERPREF_MT_keyconfigs` de
  Preferencias › Teclado), `preferences.keyconfig_import` y el propio
  `AddPresetKeyconfig`. Es decir: si el usuario elige «Blender» en ese menú, hoy
  todavía se ejecutan las 9.055 líneas de Python y se construye una segunda
  configuración de teclado, en paralelo a la nativa.

**B4 lo ha cerrado.** No se fingió que fueran presets de propiedades: los 235
constructores ya transliterados siguen en `flipendo::keymap`, y
`preferences.keyconfig_activate` es ahora un operador C++ que reconstruye «Blender»
con ese registro. `Blender.fpreset` es únicamente el marcador de datos que descubre
el menú. Las 17 decisiones de teclado continúan vivas mediante RNA nativo. Los dos
scripts distribuidos y sus 9.055 líneas se han retirado.

---

## El formato: `.fpreset`

Un fichero de texto UTF-8, una operación por línea. Se eligió así, y no JSON ni
XML, por tres motivos concretos: se lee de un vistazo, se compara bien en `git`,
y el escritor cabe en veinte líneas — porque el editor tiene que poder
**escribirlo** cada vez que el usuario guarda un preset suyo.

```
# Cámara de 1 pulgada
fpreset 1
subdir camera
set context.camera.sensor_width 13.2
set context.camera.sensor_height 8.8
set context.camera.sensor_fit "HORIZONTAL"
```

**Cabecera.** La primera línea que no sea blanca ni comentario tiene que ser
`fpreset <versión>`. `subdir` es opcional y recuerda a qué familia pertenece.
Los comentarios son `#` a principio de línea, y solo ahí: dentro de una cadena un
`#` no tiene nada de especial y no queríamos un escape más.

**Las rutas RNA.** Siempre con raíz `context.`, que es literalmente lo que
escribía el Python (`bpy.context.`) menos el `bpy.`, que en un editor sin
intérprete no significa nada. Detrás va una ruta RNA normal, con la sintaxis que
`RNA_path_resolve` ya entiende: `foo.bar`, `foo["nombre"].bar`, `foo[0].bar`.

El primer componente se resuelve **en el mismo orden que usaba el intérprete**
(`pyrna_struct_getattro`): primero como propiedad RNA del tipo `Context`, y solo
si no existe, por los callbacks de contexto (`CTX_data_pointer_get`). El orden no
es un detalle: `scene` y `preferences` son propiedades de `Context`, mientras que
`camera`, `object`, `cloth` o `edit_movieclip` no lo son y salen de los
callbacks. Invertirlo resolvería alguna ruta a otra cosa sin avisar.

**Los tipos no se escriben.** Un preset no dice si `13.2` es un `float` o un
`double`, ni si `"HORIZONTAL"` es una cadena o un identificador de enumeración:
lo decide la propiedad RNA de destino. Es la misma razón por la que el keymap
resuelve sus identificadores con las tablas de RNA y no con una tabla propia — un
tipo escrito en el fichero es una segunda fuente de verdad, y las segundas
fuentes de verdad se desincronizan. La gramática del literal basta para llevar el
valor; RNA basta para saber qué significa.

| Literal | Se lee como |
|---|---|
| `true` · `false` | booleano (o entero, si la propiedad es entera) |
| `-42` | entero (o booleano, o índice de enumeración) |
| `13.2` · `1e-3` · `inf` · `nan` | flotante |
| `"texto"` | cadena, o identificador de enumeración |
| `none` | puntero nulo |
| `[1.0, 0.5, 0.0]` | vector RNA; la longitud tiene que coincidir |
| `{"A", "B"}` | enumeración de banderas |

Escapes dentro de una cadena: `\\`, `\"`, `\n`, `\r`, `\t` y `\xNN`. Nada más.

**Los flotantes se imprimen con la representación más corta que vuelve a leerse
como el mismo número**, y para una propiedad de precisión simple se exige que
vuelva el mismo `float`, no el mismo `double`. Por eso
`options.screen_trace_thickness = 0.20000000298023224`, que era lo que escribía
el `repr()` de Python al promover un `float` a `double`, queda como
`set context.scene.eevee.ray_tracing_options.screen_trace_thickness 0.2`
sin perder un bit. Se comprueba imprimiendo con precisión creciente y releyendo
hasta que coincide.

**Colecciones.** El escritor de Python sabía volcar colecciones de `IDProperty`
(`ruta.clear()` y `item_sub_1 = ruta.add()`), así que el formato también:

```
clear context.scene.algo.lista
add context.scene.algo.lista
  set .nombre "x"
end
```

Las rutas dentro de un bloque `add` empiezan por `.` y se resuelven contra el
elemento recién creado. Los bloques anidan. Hoy **ningún** preset del árbol usa
esto; está porque un preset de operador del usuario podría.

---

## La compatibilidad, que es capacidad viva

Un usuario tiene presets suyos en
`~/Library/Application Support/Blender/4.5/scripts/presets/**/*.py`. Los escribió
el propio Blender. Perderlos no es una opción, y «que los reconvierta a mano»
tampoco.

**El lector nativo acepta los dos formatos.** Cuando la extensión es `.py`, el
fichero se analiza **sin intérprete**, con un analizador propio del subconjunto
exacto que generaba `AddPresetBase`:

- `import bpy` y una cadena de documentación inicial: se ignoran.
- `nombre = <expresión de ruta>`: es un alias, un `preset_defines`. Se anota y se
  expande; en el formato nativo los alias desaparecen y toda ruta queda completa.
- `<ruta> = <literal>`: una asignación. Literales de Python: `True`/`False`/`None`,
  enteros, flotantes, cadenas con comilla simple o doble, tuplas, listas y
  conjuntos. Y el producto de enteros (`buffersize = 224 * 8`), que aparece escrito
  a mano en los presets de FFmpeg.
- `<ruta>.clear()` y `nombre = <ruta>.add()`: colecciones.

Cualquier otra cosa —un `if`, un `for`, un `import` que no sea `bpy`, una llamada
a un operador— **se rechaza con fichero, línea y motivo**. No se aplica a medias
y no se ignora en silencio: un preset que hacía algo y deja de hacerlo sin avisar
es peor que un error.

Cuando el usuario vuelve a guardar ese preset, se escribe ya como `.fpreset`.
El `.py` original se queda donde estaba; nadie borra ficheros del usuario.

---

## Cómo se verifica — esto es lo importante

No por lectura. El binario lo comprueba solo, con dos opciones nuevas escritas en
C++, al estilo de `--fl-dump-keymap` y `--fl-dump-tools`:

```
Blender --background --fl-convert-presets <dir>            # .py -> .fpreset
Blender --background --fl-check-presets  <dir> <informe>   # compara los dos caminos
```

`--fl-check-presets` hace, para cada preset:

1. captura el estado de todas las rutas que el preset toca;
2. ejecuta el `.py` **con el intérprete**, por el mismo camino que usa el editor
   hoy (`bpy.utils.execfile`), y vuelve a capturar;
3. restaura el estado de partida;
4. aplica el `.fpreset` con el lector nativo, y vuelve a capturar;
5. compara las dos capturas línea a línea.

La captura es el volcado: `ruta = valor` con el mismo formateo determinista del
formato, así que dos estados iguales dan dos textos idénticos byte a byte.

**La trampa era el contexto.** `bpy.context.camera` no existe en `--background`,
y sin él 82 de los 165 presets no se pueden aplicar por ningún camino. La salida
no fue fabricar una escena de mentira, sino empujar los datos que hacen falta al
**almacén de contexto** (`CTX_store_set`), que es lo primero que mira
`ctx_data_get` — y por ahí pasan igual el intérprete y el resolvedor nativo, de
modo que los dos ven exactamente el mismo objeto. El arnés monta una cámara, un
clip de vídeo con una pista activa, un objeto con modificador de tela y otro de
fluido, y los ata a `camera`, `edit_movieclip`, `cloth`, `fluid` y `object`.

Un detalle que costó encontrarlo: el puntero de un modificador tiene que llevar
el ID del **objeto** como propietario, no `nullptr`. Las llamadas de
actualización de RNA (`rna_cloth_update`) desreferencian `ptr->owner_id` sin
comprobarlo, y con un puntero suelto el proceso se cae.

### Lo que se hace igual que el intérprete, a propósito

Al asignar, el aplicador reproduce lo que hacía `setattr` sobre un `bpy_struct`:
comprueba `RNA_property_editable_flag` antes de escribir y, después, dispara
`RNA_property_update` si `RNA_property_update_check` lo pide. Sin esa segunda
parte, propiedades con callback (`ffmpeg.format`, que reajusta el códec) dejarían
un estado distinto y la comparación lo cantaría.

Y una diferencia deliberada: `float` → `int` **se rechaza**. Python también lo
rechazaba; convertirlo redondeando en silencio sería inventar comportamiento.

---

## Estado

Ver la sección de deuda. Las cifras de la comprobación están en el mensaje del
commit que las obtuvo y en `~/Flipendo/dev/noche/informes/F1-presets.md`.

## Deuda abierta, con nombre y apellidos

1. ~~**Los cinco presets de FFmpeg con condicional NTSC/PAL.**~~ **CERRADO.**
   Decidian `gopsize` (18 o 15) y, el del DVD, `resolution_y` (480 o 576) segun
   `scene.render.fps != 25`. No se horneo ninguna rama: el formato tiene ahora un
   **guardian declarado**, y solo eso:

   ```
   when context.scene.render.fps != 25
   set context.scene.render.ffmpeg.gopsize 18
   otherwise
   set context.scene.render.ffmpeg.gopsize 15
   endwhen
   ```

   Una comparacion de una ruta RNA contra un literal, con `==` o `!=`, y las
   operaciones que protege. Sin bucles, sin expresiones. **No va a crecer**: es
   justo lo que hacia falta para no perder la unica decision que tomaban estos
   cinco ficheros.

   Verificado comparando el estado que deja cada camino **en las dos ramas**:
   se aplica el `.py` con el interprete y el `.fpreset` con el lector nativo
   sobre un estado de partida limpio, con `fps=30` y con `fps=25`, y se leen
   todas las rutas que el preset toca. **116 propiedades comparadas en 10 casos
   (5 presets x 2 ramas), 0 distintas.** Con `fps=30` sale `gopsize 18` (y
   `resolution_y 480` en el DVD); con `fps=25`, `gopsize 15` y `resolution_y 576`
   — que es exactamente lo que hacia el Python.

   Con esto **`scripts/presets` queda a cero ficheros `.py`**: 172 `.fpreset` y
   2 temas `.xml`.

2. **`scripts/presets/text_editor/Visual_Studio_Code.py`.** Su `match
   platform.system()` colapsa en Flipendo, que es solo macOS, a la rama `_` →
   `"code"`. Es el mismo razonamiento que el pase de macOS del keymap, que está
   siempre activo. Convertido con esa rama fija y anotado en el fichero.

3. **`context.particle_system` y `context.active_operator` no se montan en el
   arnés**, así que `hair_dynamics/Default.py` (1) y los dos presets de
   `operator/wm.collada_export` (2) se convierten pero **no se comparan**. Son
   3 de 165. Para el primero haría falta un sistema de partículas real sobre un
   objeto; para los otros, una instancia de operador viva.

4. **El escritor nativo y el operador.** `scripts/startup/bl_operators/presets.py`
   sigue siendo el que registra `script.execute_preset` y la familia
   `AddPreset*`, porque están atados a `bl_ui` (los menús `Menu`/`PresetPanel` y
   su `bl_label`, que un operador nativo no puede escribir). Lo que sí deja de
   ser Python es el **trabajo**: leer, aplicar y escribir presets. Ver el estado
   exacto en el informe.

5. **Punteros a datos.** El escritor de Python guardaba un puntero a un ID con
   `repr()`, que daba `bpy.data.images['Foo']` — Python válido que al reejecutarse
   volvía a resolver. El formato de datos solo entiende `none` para un puntero, y
   el lector nativo rechaza `bpy.data.…` con un error claro. Afecta a
   `gpencil_material` (`stroke_image`, `fill_image`) y a presets de operador con
   propiedades de puntero. En el árbol no hay **ninguno**: los tres presets de
   material de lápiz de grasa guardan `None`. La salida es un valor
   `data("images", "Foo")` en el formato, resuelto contra `bpy.data` con RNA.
   Hasta entonces: error visible, nunca pérdida silenciosa.

6. **Los presets de tema son XML**, no Python-como-datos, y siguen pasando por
   `rna_xml.py`. Es otra familia y otro formato; no entra en esta migración.

---

## Cambios fuera de los ficheros de este carril

Uno solo, y es una línea: el conjunto de extensiones por defecto que lista un
menú de presets, en `scripts/modules/bpy_types.py` (`Menu.preset_extensions`),
pasa de `{".py", ".xml"}` a `{".fpreset", ".py", ".xml"}`. Sin eso los menús no
verían ni un preset. Es un **valor por defecto**: cualquier clase que ya lo
sobrescriba se queda como estaba.

---

## Estado, con cifras

| Qué | Cifra |
|---|---:|
| Presets `.py` de partida | 173 (10.287 líneas) |
| Presets de propiedades convertidos a `.fpreset` | **171** (165 por la herramienta, 1 a mano, 5 de FFmpeg con guardian) |
| Marcadores de keyconfig `.fpreset` | **1** («Blender», construcción C++) |
| Mismo estado por los dos caminos | **148 de 166 comparados, 0 distintos** |
| `.fpreset` que leen y aplican sin un error | **156 de 172** (1 con error, 15 sin contexto) |
| `.py` retirados | **168** (incluye los 2 de keyconfig, 9.055 líneas) |
| `.py` que quedan | **0** |

Los artefactos están en `tests/flipendo/presets/`:
`comparacion-python-cpp.txt` (la comparación de los dos caminos, congelada
antes de retirar el Python — el día de la retirada esa prueba se queda sin
material) y `aplicacion-nativa.txt`, que **sigue valiendo**: cada `.fpreset`
se lee y se aplica, y caza lo que de verdad se rompe con el tiempo — un
fichero mal escrito a mano o una ruta RNA que desaparece.

Se reproduce con:

```
Blender --background --factory-startup \
  --fl-check-presets scripts/presets tests/flipendo/presets/aplicacion-nativa.txt
```

### El puente que queda, y se ve

En el arbol ya no hay ni un preset `.py`, asi que **el ultimo recurso de
`ExecutePreset` no lo usa nada de lo que distribuimos**. Se conserva igualmente,
y a proposito: un usuario puede tener en su carpeta un preset `.py` escrito a
mano con logica que el lector nativo no entienda, y perderselo sin avisar seria
justo lo que esta migracion existe para evitar. Cuando salta, imprime el motivo
del rechazo y «Preset no convertible a datos, se ejecuta como script». Se va con
`bl_operators`.


---

## El envoltorio de Python, cerrado casi entero (carril OPS-3, 2026-09-11, 21:50)

El plan de cuatro pasos de más abajo se ha ejecutado en el orden recomendado (2 → 1),
y con él **`presets.py` baja de 1.022 líneas a 443** (−579, el 57 %).

### Qué se llevó a C++

| Qué | Dónde | Cómo se verificó |
|---|---|---|
| Los **20** `*_preset_add` / `*_preset_remove` no-tema | `preset/fl_preset_add_ops.cc`, **una fila por familia** | `--fl-check-preset-optypes` 93/93 · `--fl-check-preset-ops` 89/89 |
| `wm.operator_presets_cleanup` | el mismo fichero | `--fl-check-preset-optypes` |
| `WindowManager.preset_name` | `register_add_preset_types()` | `--fl-dump-preset-panel`, idéntico |
| El `bl_label` del menú | `flipendo::preset::ui::menu_label_set/get` | `--fl-check-preset-ops`, casos 0-6 y 8, 12 |

Las 298 líneas de tabla son ahora 20 filas de una estructura. Las 24 subclases eran
datos: nunca fueron código.

### Los dos arneses nuevos

```
Blender -b --fl-check-preset-optypes tests/flipendo/presetops/baseline-python.txt
  TOTAL 93/93 elementos identicos, 117/117 lineas      (23 operadores)

BLENDER_USER_SCRIPTS=<carpeta vacía> \
Blender -b --fl-check-preset-ops tests/flipendo/presetops/comportamiento-python.txt
  TOTAL 89/89 elementos identicos, 103/103 lineas      (13 casos)
```

El primero compara la **superficie de registro**; el segundo, la **conducta**: escribe
presets de verdad, los aplica, los borra y vuelca el contenido de los ficheros byte a
byte. Las dos líneas base se capturaron con un guion Python efímero contra una copia del
binario anterior (`/tmp/Blender-ref-ops3.app`, con los `.py` dentro de su *bundle*), y
**las dos se repitieron tres veces con el mismo md5** antes de darlas por buenas.

El arnés de conducta se puede ejecutar en `--background` porque redirige la carpeta de
scripts del usuario con `BLENDER_USER_SCRIPTS`. Sin eso escribiría en los presets de
verdad del usuario — que es exactamente lo que pasó la primera vez, por la trampa 1.

### Las cuatro trampas, medidas

1. **`bpy.utils.user_resource(create=True)` NO es `BKE_appdir_folder_id_create()`.**
   Ésta prueba primero con `BKE_appdir_folder_id()`, que **exige que la carpeta exista**,
   y sólo si no existe cae al camino del usuario; aquélla va siempre al camino del
   usuario (`folder_id_user_notest`) y crea lo que falte. Con `BLENDER_USER_SCRIPTS`
   puesto pero su `presets/<familia>` todavía sin crear, y la carpeta por defecto ya
   creada por otro carril, la versión de C escribía en la **por defecto**. Lo cazó el
   arnés dejando siete presets en la carpeta real del usuario.
2. **Una propiedad declarada desde Python no es animable.** `bpy_props` limpia
   `PROP_ANIMATABLE` salvo que se pida; `RNA_def_boolean()` lo deja puesto. Sin
   `RNA_def_property_clear_flag(prop, PROP_ANIMATABLE)` el contrato cambia sin que se
   note. Por eso el volcado nuevo incluye **banderas**, que el v1 no llevaba.
3. **`RNA_def_collection()` con el tipo por nombre sólo vale en el preproceso.** En
   ejecución imprime «`".properties": only during preprocessing`» y deja la colección
   **sin tipo de elemento**: existe, se registra igual, y no se puede recorrer. En
   ejecución se pone con el puntero (`RNA_def_property_struct_runtime`). El volcado daba
   «idéntico» con la colección rota hasta que se le añadió `srna=`.
4. **`maxlen=64` se registra como 65.** Ya estaba escrito en `OBJECT-SELECT-A-CPP.md`;
   aquí afecta a `name` y a `operator`.

### Una corrección y una ampliación, las dos deliberadas

- **`wm.keyconfig_preset_remove` estaba muerto.** Su `invoke` buscaba el fichero con
  `ext=".py"` cuando el carril B4 ya había cambiado la extensión de la familia a
  `.fkeyconfig`: nunca encontraba nada y siempre decía «Built-in keymap configurations
  cannot be removed». Ahora busca con la extensión de la familia y después con `.py`.
  Es un fallo **introducido por esta migración**, no de Blender.
- **`wm.operator_presets_cleanup` sólo limpiaba `.py`**, y los presets de operador se
  escriben hoy como `.fpreset`: el operador existía y no limpiaba nada de lo nuevo. El
  nativo hace las dos cosas, con el mismo filtro de líneas y el mismo `\b` tras el
  nombre de la propiedad (sin él, quitar `files` se llevaría `files_extra`).

### Deuda nueva, con nombre y apellidos

1. **El `bl_label` de un menú de presets que todavía sea Python.** La etiqueta vive
   ahora en un registro nativo, y `menu_label_set` escribe además el `label` del
   `MenuType`/`PanelType` registrado. Lo que **no** se puede tocar sin intérprete es el
   atributo de clase de Python, que es lo que leen estos doce ficheros de `bl_ui` cuando
   dibujan el botón con `text=CLS.bl_label`: `properties_output.py`,
   `properties_data_camera.py`, `properties_render.py`, `properties_physics_cloth.py`,
   `properties_physics_fluid.py`, `properties_particle.py`,
   `properties_material_gpencil.py`, `space_clip.py`, `space_userpref.py`,
   `space_view3d_toolbar.py` y `bl_ui/utils.py` (`PresetPanel.draw_menu`). Consecuencia
   concreta: en esos paneles el botón sigue diciendo lo que decía antes de guardar. Se
   cierra solo según migre `bl_ui`; los paneles nativos ya leen el registro.
2. **Los tres operadores de tema y la rama `.xml` de `script.execute_preset`.** Siguen
   atados a `rna_xml.py` — **pero ya no por falta de herramienta**: desde el commit
   `6c299d8988d` hay un `rna_xml` nativo, verificado con 6839/6839 elementos idénticos y
   con el mismo md5 que el volcado de Python. Lo que queda es fontanería de operadores,
   medida pieza por pieza en [`RNA-XML-A-CPP.md`](RNA-XML-A-CPP.md), incluido el aviso de
   que quitar `script.execute_preset` de Python se lleva por delante el último recurso
   `bpy.utils.execfile` (la deuda 2 de este documento), cuyas condiciones de retirada
   **ya se cumplen**.
3. **`tests/flipendo/presets/aplicacion-nativa.txt` está desfasado** respecto del árbol:
   dice 166 presets y hoy hay 172, y no incluye el `ILEGIBLE` de
   `keyconfig/Blender.fpreset` (que es un marcador, no un preset de propiedades).
   Comprobado que el binario de referencia da **exactamente la misma salida** que el de
   hoy, así que la diferencia es anterior a este trabajo. Toca recapturarlo a quien lleve
   los presets como datos.
4. **Sin verificar por ejecución**: la rama de añadir/quitar de la familia `keyconfig`
   (llama a `preferences.keyconfig_export`/`import`, que necesitan una configuración de
   teclado viva) y los `invoke` con diálogo (necesitan ventana). De los dos está
   verificado que **se registran igual**.

### Lo que queda de `presets.py` (443 líneas, 7 clases)

`AddPresetBase` (sólo ya para las tres de tema), `ExecutePreset`,
`AddPresetInterfaceTheme` / `RemovePresetInterfaceTheme` / `SavePresetInterfaceTheme`,
`WM_MT_operator_presets` y `WM_PT_operator_presets` (dos clases de interfaz, se van con
`bl_ui`) y la función `_operator_path`, que es la copia mínima de
`AddPresetOperator.operator_path` que esas dos necesitan.

**El paso 3 del plan (`script.execute_preset` a C++) está bloqueado**, y por una razón
concreta y no por falta de tiempo: su rama `.xml` aplica temas con `rna_xml.py`, y sus
dos únicos ganchos `reset_cb`/`post_cb` del árbol —contados, son dos, los de
`USERPREF_MT_interface_theme_presets`— también son de temas. Migrarlo hoy significaría o
perder los temas o dejar una llamada de C++ a Python nueva. **Lo que lo desbloquea es un
lector/escritor de temas nativo** (`rna_xml.py`, 422 líneas, formato XML), que es la
deuda 6 de este documento y una pieza propia.

---

## Auditoría de `bl_operators/presets.py` (carril C, 2026-09-11, 04:30)

El encargo lo describía como "el más redundante que queda". **Medido, no lo es**: no hay
ni una línea muerta. Lo que hay es un **envoltorio fino de Python sobre un núcleo que ya es
C++**. Las cifras, sobre 1.023 líneas:

| Categoría | Líneas |
|---|---:|
| Vacías | 200 |
| Comentario | 61 |
| **Tablas de datos** (`preset_values`, `preset_defines`, `preset_subdir`, `preset_menu`, `bl_idname`…) | **298** |
| Código real | ~464 |

26 clases: `AddPresetBase`, `ExecutePreset` y **24 envoltorios** que no hacen otra cosa que
declarar qué rutas RNA toca su familia.

### Nada está muerto: los 23 idnames están vivos

Se comprobó uno a uno buscando referencias en `scripts/startup/bl_ui`, `scripts/modules` y
`source/blender`:

- **`script.execute_preset`**: referenciado desde **12 ficheros de `bl_ui` y 3 de C++**
  (`fl_ui_dump.cc`, `FL_preset_ui.hpp`, `fl_preset_ui.cc`). Es el operador que el **panel
  nativo de presets emite** para aplicar uno. Es el puente vivo C++ → Python.
- Los **22 `*_preset_add` / `*_preset_remove` / `*_preset_save`**: cada uno referenciado
  desde su panel de `bl_ui`. `node.node_color_preset_add` y `wm.operator_preset_add`
  además desde C++.

Retirar el fichero hoy **perdería capacidad**, que es justo lo que la doctrina prohíbe.

### Lo que ya NO hace (y por eso parece redundante)

Ni `ExecutePreset` ni `AddPresetBase` hacen ya el trabajo de verdad:

- Aplicar un preset es `bpy.ops.wm.preset_apply(filepath=...)` → `WM_OT_preset_apply`, C++.
- Escribirlo es `bpy.ops.wm.preset_write(...)` → `WM_OT_preset_write`, C++, con la tabla de
  rutas en `fl_preset_spec.cc`.

### Lo que SÍ sigue haciendo en Python, que es lo que hay que migrar

1. **`ExecutePreset`** (67 líneas): actualizar el `bl_label` del menú al nombre elegido,
   validar la extensión y sus mensajes de error, invocar los ganchos `reset_cb` y `post_cb`
   de la clase de preset, la rama `.xml` (temas, vía `rna_xml`) y el **último recurso
   `bpy.utils.execfile`** para un `.py` heredado que el lector nativo no sepa convertir —la
   deuda 2 de este documento.
2. **`AddPresetBase`** (~168 líneas): nombre → nombre de fichero, resolución de la ruta de
   usuario, actualización del `bl_label`, la rama de borrado (que busca con `.fpreset` y
   además con `.py`, para poder borrar presets guardados antes de la migración), la rama
   XML y los ganchos `pre_cb` / `add` / `remove`.
3. Las **298 líneas de tabla**, que son exactamente el tipo de dato que ya vive en C++ para
   la escritura (`fl_preset_spec.cc`) pero todavía no para el registro de los operadores.

### Plan para cerrarlo (no cabía en la noche, queda escrito)

1. Un `AddPresetBase` genérico en C++: un `wmOperatorType` parametrizado por
   `(preset_menu, preset_subdir, tipo)`, que ya puede apoyarse en `WM_OT_preset_write`.
2. Las 24 familias como **una tabla más** junto a `fl_preset_spec.cc`, no como 24 clases.
   Las 298 líneas de tabla se convierten en filas.
3. `script.execute_preset` en C++, con tres cosas que no se pueden olvidar: el `bl_label`
   del menú, los ganchos `reset_cb`/`post_cb` (**comprobar antes cuántos existen de
   verdad**; si son cero, el gancho desaparece con el Python) y la rama `.xml` de temas.
4. El `execfile` de último recurso es lo **último** que se quita, y solo cuando el lector
   nativo acepte los cinco FFmpeg con su condicional NTSC/PAL. Hasta entonces, quitarlo
   pierde capacidad.

Orden recomendado: 2 → 1 → 3 → 4. La tabla primero, porque es la que quita 298 líneas sin
riesgo y deja los 24 envoltorios reducidos a una fila cada uno.
