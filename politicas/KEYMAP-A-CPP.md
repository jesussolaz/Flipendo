# El keymap por defecto: de Python a C++

> Es el bloqueo real del "cero Python". No es una pieza de UI más: mientras el keymap
> salga de un script, **ni el editor ni el Player** pueden prescindir del intérprete.

## Por qué bloquea las dos cosas

`WM_keyconfig_reload` (`source/blender/windowmanager/intern/wm.cc:422-430`) ejecuta
literalmente `bpy.utils.keyconfig_init()`. Y no es solo cosa del editor: el Player
standalone hace `CTX_py_init_set(C, true)` y llama a `WM_keyconfig_init`
(`source/gameengine/GamePlayer/GPG_ghost.cpp:1704` y `:1724`), así que arranca CPython
para sus atajos aunque el juego entero sea C++ nativo.

## Qué hay que reproducir

| Pieza | Dónde | Tamaño |
|---|---|---|
| Datos del keymap | `scripts/presets/keyconfig/keymap_data/blender_default.py` | 8.669 líneas, 235 funciones `km_*` |
| Parámetros | la clase `Params` del mismo fichero | 17 preferencias de usuario + derivados |
| Pase de macOS | `bl_keymap_utils/platform_helpers.py`, llamado desde `presets/keyconfig/Blender.py:379` | duplica Ctrl como Cmd |

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

Ver el parte que da `--fl-check-keymap`. Cuando llegue a 248/248 idénticos, se
sustituye la llamada a Python de `WM_keyconfig_reload` por
`flipendo::keymap::register_default`, y con ella cae el último motivo por el que el
Player carga un intérprete.

---

## Estado: hecho, con una deuda concreta

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

**Deuda abierta:** no hay panel nativo para cambiarlas. Se leen y se aplican, pero la
interfaz de Preferencias › Teclado está vacía hasta que ese panel se migre con
`FL_ui_registry`. Es una regresión de interfaz, no de comportamiento, y está aquí
escrita para que no se olvide.

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
