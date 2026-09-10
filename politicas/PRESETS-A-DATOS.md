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

**Decisión: no se tocan.** No son presets de datos —no hay ninguna asignación de
propiedad que convertir, hay 235 funciones que fabrican keymaps— y su sustituto
nativo ya existe: `flipendo::keymap::register_default()`. Lo que falta es cambiar
el operador de activación para que llame a ese registro en vez de ejecutar el
script, y eso es código del carril del keymap (`fl_keymap_*`), no de este. Queda
escrito abajo como deuda con nombre.

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

1. **`scripts/presets/keyconfig/Blender.py` y
   `keyconfig/keymap_data/blender_default.py` (9.055 líneas).** Siguen en Python.
   No son datos: fabrican keymaps, y su sustituto nativo
   (`flipendo::keymap::register_default`) ya existe y ya es quien construye el
   keymap por defecto. Lo que falta es que `preferences.keyconfig_activate` llame
   al registro nativo en vez de ejecutar el script. Es trabajo del carril del
   keymap; este no toca `fl_keymap_*`.

2. **Los cinco presets de FFmpeg con condicional NTSC/PAL**
   (`DVD_(note_colon__this_changes_render_resolution).py`, `H264_in_MP4.py`,
   `H264_in_Matroska.py`, `Ogg_Theora.py`, `Xvid.py`). Deciden `gopsize` 18 o 15
   según `scene.render.fps != 25`, que es estado de la escena en el momento de
   aplicar. No es representable como asignación plana y **no se convierte**:
   hornear una de las dos ramas cambiaría el comportamiento para la mitad de los
   usuarios. Salida prevista: o un guardián declarativo en el formato (`when
   <ruta> != <literal>`), o cinco proveedores de preset nativos. Mientras tanto
   siguen en `.py` y siguen funcionando.

3. **`scripts/presets/text_editor/Visual_Studio_Code.py`.** Su `match
   platform.system()` colapsa en Flipendo, que es solo macOS, a la rama `_` →
   `"code"`. Es el mismo razonamiento que el pase de macOS del keymap, que está
   siempre activo. Convertido con esa rama fija y anotado en el fichero.

4. **`context.particle_system` y `context.active_operator` no se montan en el
   arnés**, así que `hair_dynamics/Default.py` (1) y los dos presets de
   `operator/wm.collada_export` (2) se convierten pero **no se comparan**. Son
   3 de 165. Para el primero haría falta un sistema de partículas real sobre un
   objeto; para los otros, una instancia de operador viva.

5. **El escritor nativo y el operador.** `scripts/startup/bl_operators/presets.py`
   sigue siendo el que registra `script.execute_preset` y la familia
   `AddPreset*`, porque están atados a `bl_ui` (los menús `Menu`/`PresetPanel` y
   su `bl_label`, que un operador nativo no puede escribir). Lo que sí deja de
   ser Python es el **trabajo**: leer, aplicar y escribir presets. Ver el estado
   exacto en el informe.

6. **Punteros a datos.** El escritor de Python guardaba un puntero a un ID con
   `repr()`, que daba `bpy.data.images['Foo']` — Python válido que al reejecutarse
   volvía a resolver. El formato de datos solo entiende `none` para un puntero, y
   el lector nativo rechaza `bpy.data.…` con un error claro. Afecta a
   `gpencil_material` (`stroke_image`, `fill_image`) y a presets de operador con
   propiedades de puntero. En el árbol no hay **ninguno**: los tres presets de
   material de lápiz de grasa guardan `None`. La salida es un valor
   `data("images", "Foo")` en el formato, resuelto contra `bpy.data` con RNA.
   Hasta entonces: error visible, nunca pérdida silenciosa.

7. **Los presets de tema son XML**, no Python-como-datos, y siguen pasando por
   `rna_xml.py`. Es otra familia y otro formato; no entra en esta migración.

---

## Cambios fuera de los ficheros de este carril

Uno solo, y es una línea: el conjunto de extensiones por defecto que lista un
menú de presets, en `scripts/modules/bpy_types.py` (`Menu.preset_extensions`),
pasa de `{".py", ".xml"}` a `{".fpreset", ".py", ".xml"}`. Sin eso los menús no
verían ni un preset. Es un **valor por defecto**: cualquier clase que ya lo
sobrescriba se queda como estaba.
