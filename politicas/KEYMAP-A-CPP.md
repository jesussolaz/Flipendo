# El keymap por defecto: de Python a C++

> Fue uno de los bloqueos reales del "cero Python". Desde B4, tanto la construcción
> inicial como la activación desde Preferencias y sus opciones integradas son C++.

## Por qué bloquea las dos cosas

Históricamente, `WM_keyconfig_reload` ejecutaba `bpy.utils.keyconfig_init()`. No era
solo cosa del editor: el Player standalone también inicializaba el keymap y arrancaba
CPython para sus atajos. Ese camino fue sustituido por
`flipendo::keymap::register_default()`; B4 ha cerrado el segundo camino que quedaba,
`preferences.keyconfig_activate`.

## Qué hay que reproducir

| Pieza | Dónde | Tamaño |
|---|---|---|
| Datos del keymap | antes `keymap_data/blender_default.py`; hoy `source/blender/windowmanager/keymap/` | 8.669 líneas Python retiradas, 235 constructores `km_*` transliterados |
| Parámetros | `FL_keymap_params.hpp` y `BlenderKeyConfigPreferences` RNA | 17 decisiones de usuario + derivados |
| Pase de macOS | andamiaje C++ de `flipendo::keymap` | duplica Ctrl como Cmd |

El resultado son **248 keymaps y 3.673 atajos**.

## Cómo se verifica — esto es lo importante

No por lectura. El binario lo comprueba solo:

```
Blender --fl-dump-keymap  <fichero>    # vuelca el keymap activo (hoy: el de Python)
Blender --fl-dump-keymap-native <f>    # vuelca el que construye el C++
Blender --fl-check-keymap <linea-base> # compara y da el parte
```

`tests/flipendo/keymap/baseline-python.txt` es la línea base congelada: lo que genera
Python hoy. El C++ tiene que producir un volcado **idéntico**.

Dos detalles del formato de volcado, ambos deliberados: los keymaps se ordenan por
`(idname, space, region)` para no depender del orden de registro, pero los **elementos
dentro de cada keymap conservan su orden**, porque es semántico — gana el primero que
casa. Y las propiedades se serializan con las claves ordenadas alfabéticamente, para
que un cambio de orden de inserción no se confunda con un cambio de conducta.

> Nota: en `--background` el keymap por defecto no se carga (`WM_keyconfig_reload` se
> salta a sí mismo con `G.background`), así que ahí solo hay 12 atajos. La comparación
> se hace en modo gráfico.

## Decisiones de diseño

**Los identificadores se resuelven por RNA, no con una tabla propia.** Las teclas y
valores se pasan como las mismas cadenas que usaba el Python (`'N'`, `'PRESS'`,
`'VIEW_3D'`) y se resuelven con `rna_enum_event_type_items` y hermanos — la misma tabla
que usaba el camino de Python. Un mapeo paralelo de 200 constantes escrito a mano
habría sido una fuente de errores silenciosos.

**Transliteración, no tabulación.** El keymap no se compila a tablas de datos: se
escribe como código C++ que se ejecuta al arrancar. La razón es la combinatoria: 16
preferencias booleanas más un enum de tres valores no caben en tablas precalculadas sin
perder opciones.

**Las preferencias no se colapsan.** Se podría haber horneado la configuración de
fábrica y el volcado habría coincidido igual, pero eso borraría 17 opciones que hoy
existen. `FL_keymap_params.hpp` conserva la struct y los `km_*` conservan sus
condicionales.

**El gemelo de Cmd se añade en el andamiaje, no a mano.** El pase de macOS inserta,
antes de cada atajo con Ctrl, una copia con Cmd — salvo `Ctrl-H/M/SPACE/W/ACCENT_GRAVE/PERIOD/TAB`
sin Alt ni Shift, y `Ctrl-Alt-Q` sin Shift. Como Flipendo es Mac-only está siempre
activo. Se hace al añadir el atajo, no como transformación posterior: da el mismo orden
y no se puede olvidar. Como las propiedades se ponen después por encadenado, `Item`
guarda los dos punteros y cada setter escribe en ambos — si no, el gemelo se quedaría
sin propiedades.

Ese pase se descubrió comparando: el piloto daba 6 atajos y la línea base 7.

## Estado

Hecho: `--fl-check-keymap` da 248/248 idénticos y 3.673 atajos. Tanto
`WM_keyconfig_reload` como la selección de «Blender» usan el constructor C++.

---

## Estado B4: construcción, selección y panel nativos

El keymap por defecto **ya no lo genera Python**. `WM_keyconfig_reload` llama a
`flipendo::keymap::register_default()`, y el volcado de la configuración activa del
editor es byte a byte idéntico a la línea base: 248 keymaps, 3.673 atajos.

### Lo que las preferencias enseñaron

Al quitar el preset de Python se perdió, sin que lo pareciera, la capacidad de
**cambiar** las opciones de teclado. Los datos nunca fueron Python — se guardan como
`IDProperty` en `UserDef` y el propio motor los escribe
(`BKE_keyconfig_pref_set_select_mouse`, usado por el versionado de ficheros
antiguos). Lo que era Python es el **tipo RNA y el panel**, que vivían en
`presets/keyconfig/Blender.py`.

`params_from_preferences()` ya los lee de ahí, así que la preferencia vuelve a tener
**efecto**: con `select_mouse` a la derecha el keymap se construye distinto, como
antes.

El tipo `BlenderKeyConfigPreferences`, sus propiedades, el callback de reconstrucción
y su método `draw` viven ahora en `rna_wm.cc`. El panel ya no queda vacío. Se
conservan las 18 propiedades almacenadas históricas, que son **17 decisiones
efectivas** porque `use_alt_tool` y `use_alt_cursor` son alternativas según el botón
de selección. No se hornea ni colapsa ninguna.

Solo hay una configuración integrada, «Blender». El fichero `Blender.fpreset` es un
marcador de datos para que el menú existente pueda descubrirla; el operador nativo
reconoce el nombre y llama a `WM_keyconfig_reload`. Configuraciones externas `.py`
del usuario conservan un puente de compatibilidad: se pueden importar y activar,
pero ningún fichero de teclado distribuido por Flipendo ejecuta el intérprete.

### Evidencia de B4

`--fl-selftest-keyconfig` recorre el mismo operador del desplegable y el mismo
callback RNA del panel. Con selección izquierda obtiene 248 keymaps/3.673 atajos;
con `select_mouse=RIGHT`, 248/3.591 y `VIEW3D_OT_select` cambia de clic izquierdo a
presión derecha; al restaurar vuelve a 248/3.673. El tipo devuelve las 18 propiedades
y un método `draw` nativo. La comparación previa de los caminos Python y C++ fue
idéntica tras ignorar exclusivamente `KEYMAP_TOOL`, bit de estado que añade después
el sistema de herramientas a 83 keymaps y no forma parte de sus datos.

### Situación de las dos guardas históricas

1. **La espera a que Python arrancase ya puede desaparecer por completo.** El camino
   actual de `WM_keyconfig_reload` ya no contiene la guarda `CTX_py_init_get`, y B1
   elimina la última premisa que se le atribuía: los 18 operadores `wm.context_*`
   están registrados en C++. El verificador confirma que construir el keymap así
   conserva exactamente 248 keymaps y 3.673 atajos. La diferencia de arranque que se
   había observado al probar una fase más temprana no procedía de Python, sino de las
   macros `MESH_OT_`, `NODE_OT_` y `OBJECT_OT_`, registradas después por
   `ED_spacemacros_init`; si se adelanta otra vez la construcción habrá que expresar
   esa fase de forma explícita, no volver a usar el estado del intérprete como señal.

2. **El respaldo a `IDProperty` sigue siendo una red general**, no una dependencia de
   `wm.context_*`. Reproduce el `setattr` de `bl_keymap_utils/io.py:241` cuando el
   keymap menciona un operador o una macro todavía no registrados y evita perder
   propiedades silenciosamente. Su retirada requiere demostrar que todos los tipos
   referidos existen ya en la fase concreta donde se construye el mapa; B1, por sí
   solo, no demuestra eso para las macros tardías.
