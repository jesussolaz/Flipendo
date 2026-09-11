# `rna_xml.py` a C++ — el XML de los temas, y qué falta para cerrar el quinto puente

Carril OPS-3 (Flipendo, 2026-09-12). `scripts/modules/rna_xml.py` (422 líneas) vuelca un
árbol RNA a XML y lo vuelve a leer. **La mitad de introspección ya es C++ y está
verificada**; lo que queda escrito abajo es el diagnóstico de lo que falta y cuánto vale.

---

## La medición: qué es de verdad y quién lo llama

Lo primero, porque cambia la forma de atacarlo:

| Pregunta | Respuesta medida |
|---|---|
| ¿Quién lo importa? | **Un solo fichero**: `bl_operators/presets.py`, en **tres** sitios. Nada más en todo el árbol — ni `bl_ui`, ni `scripts/modules`, ni `addons_core`, ni `tests`. |
| ¿Para qué? | **Sólo temas de la interfaz.** Los tres sitios pasan el mismo `preset_xml_map`: `preferences.themes[0]` y `preferences.ui_styles[0]`. |
| ¿Cuánto es serialización XML? | Poco: el escritor son ~40 líneas de formateo de texto y el lector usa `xml.dom.minidom`. |
| ¿Cuánto es introspección de RNA? | **El grueso.** Recorrer propiedades, decidir si cada una es atributo, sub-puntero o colección, y coaccionar tipos al leer. |
| ¿Se puede reproducir al byte? | **Sí, y está hecho.** La salida es determinista (mismo md5 tres veces) y no usa ninguna de las construcciones inestables. |

Y lo que de verdad importaba saber: **los temas son capacidad viva**. El gestor de
extensiones instala temas además de add-ons, así que esto no se puede tirar, hay que
migrarlo.

### Los tamaños

```
scripts/modules/rna_xml.py                 422 líneas
scripts/presets/interface_theme/Blender_Light.xml   1.675 líneas
scripts/presets/interface_theme/Blender_Dark.xml        6 líneas (vacío a propósito)
El tema de fábrica, serializado                     1.709 líneas
```

`Blender_Dark.xml` **está vacío a propósito**, y es el detalle que más conviene no pasar
por alto: el tema oscuro *es* el de fábrica, así que aplicarlo consiste únicamente en
disparar el gancho de reinicio. Si alguien migra el aplicador sin el gancho, elegir
«Blender Dark» dejará de hacer nada y la prueba de que funciona será que no cambia
nada — que es exactamente lo que parecerá si está roto.

### El contenido real del XML de un tema, contado

| Forma | Apariciones |
|---|---:|
| Color en hexadecimal (`#rrggbb[aa]`) | **983** |
| Número suelto | 106 |
| Booleano (`TRUE`/`FALSE`) | 40 |
| Enumeración simple (una cadena) | 1 |
| Enumeración de **banderas** (`{A,B}`) | **0** |
| Puntero nulo (`NONE`) | **0** |
| Referencia (`Tipo::nombre`) | **0** |

Los tres ceros son buenas noticias y hay que decirlos: el subconjunto que los temas usan
de verdad es pequeño. Aun así el módulo nativo implementa **todos** los casos, porque el
`preset_xml_map` es un dato y mañana puede apuntar a otra cosa.

---

## Lo que ya está hecho y verificado (commit `6c299d8988d`)

`source/blender/windowmanager/preset/fl_rna_xml.cc`, con las dos mitades:

- **Leer**: `pugixml`, que **ya venía en el árbol** (lo usa la exportación SVG de lápiz
  de grasa) y se engancha igual, tras `WITH_PUGIXML`.
- **Escribir**: a mano, sin biblioteca. No es cabezonería: el formato del original **no
  es XML canónico** sino un texto con una sangría muy concreta, y hay que reproducirlo
  tal cual. El detalle que lo delata: el `>` que cierra la etiqueta de apertura va con la
  sangría de los **atributos**, no con la del elemento. Ninguna biblioteca escribe eso.

### La verificación, y por qué está bien montada

```
$ Blender -b --fl-check-theme-xml tests/flipendo/themexml/baseline-python.txt
  TOTAL 6839/6839 elementos identicos, 6844/6844 lineas
```

El volcado de cada caso **es** el XML del tema, así que una sola comparación cubre las
dos mitades a la vez: el **escritor** byte a byte (caso 0, escribir el tema de fábrica) y
el **lector** por su efecto (casos 1-3: leer un tema y volver a escribirlo; si el lector
se dejara un valor, el XML de salida lo cantaría).

Y no es sólo que el comprobador diga que sí: **el md5 del volcado en C++ es el mismo que
el del volcado en Python** (`fc366658526f1ba52bc27a65ddca1e30`), con las dos capturas
repetidas tres veces.

Los cuatro casos: el tema de fábrica; `Blender_Light.xml` leído y reescrito;
`Blender_Dark.xml` (el vacío); y el claro leído con `secure_types` restringido a
`{Theme}`, que tiene que saltarse todos los sub-tipos y dejar el tema como estaba.

### Trampas encontradas

1. **`RNA_property_update(nullptr, ...)` tumba el proceso.** `setattr` de Python llama a
   la actualización con el contexto de verdad, y esa función acaba en `CTX_data_main(C)`.
   Pasar `nullptr` —que es lo primero que uno escribe cuando no tiene el contexto a
   mano— se cae al leer el primer tema. Hay que enhebrar el `bContext` hasta el fondo del
   lector.
2. **Un arnés que no vacía el búfer miente sobre dónde falla.** La caída del punto 1
   truncó el volcado a mitad del caso 1, y como el caso 0 se había quedado en el búfer,
   *parecía* que el escritor estaba mal cuando ya salía idéntico. Media hora buscando en
   el sitio equivocado. Ahora hay un `fflush` por caso.
3. **`int(v * 255)` de Python trunca hacia cero, no redondea.** 0.5 → 127, no 128. Con
   redondeo, 983 colores del tema salen distintos.
4. **El `>` con la sangría de los atributos** (ver arriba).

### Una diferencia deliberada

Una enumeración de **banderas** se escribía en Python como `",".join(list(conjunto))`, y
el orden de iteración de un `set` de cadenas depende del hash, que Python aleatoriza por
proceso: **el original no era determinista para ese caso**. El nativo emite en el orden de
la tabla de RNA, que sí lo es. No afecta a nada del árbol — medido: cero enumeraciones de
banderas en los dos temas distribuidos.

### Lo que no se trasladó, y por qué

El modo `'DATA'` de `rna2xml`, que recorría `bpy.data` entero. **No lo llamaba nadie**: su
único cliente, `xml_file_write`, usa siempre `'ATTR'`. Trasladar un camino sin usar es
escribir código que no se puede verificar.

---

## Lo que falta, medido, con su plan

`rna_xml.py` **sigue en el árbol** y `presets.py` lo sigue llamando desde sus tres sitios.
Lo que falta para retirarlo son **cuatro operadores**:

| idname | Qué hace con el XML |
|---|---|
| `script.execute_preset` | rama `.xml` → `xml_file_run`, más los ganchos `reset_cb`/`post_cb` |
| `wm.interface_theme_preset_add` | rama XML de `AddPresetBase.execute` → `xml_file_write` |
| `wm.interface_theme_preset_remove` | sin XML propio; su `post_cb` reinicia el tema |
| `wm.interface_theme_preset_save` | `execute` propio → `xml_file_write` sobre el fichero activo |

### El diagnóstico que ahorra el camino: los ganchos no son un mecanismo

`AddPresetBase` y `ExecutePreset` tienen un sistema de ganchos con pinta de ser
extensible: `pre_cb`, `reset_cb`, `post_cb`, `add`, `remove`, y `_call_preset_cb` con su
detección de aridad y su aviso de obsolescencia. **Contado en todo `scripts/`: hay
exactamente un `reset_cb` y un `post_cb`, y los dos son de la misma clase**
(`USERPREF_MT_interface_theme_presets`). Más un `add`/`pre_cb`/`post_cb` de keyconfig, ya
migrados.

O sea que eso **no es un mecanismo: es una fila**. Quien lo migre no tiene que construir
un registro de ganchos; tiene que escribir una tabla con un renglón:

```
menu_idname               = "USERPREF_MT_interface_theme_presets"
reset_op                  = "PREFERENCES_OT_reset_default_theme"
set_theme_filepath        = true
rna_map / secure_types    = los de los temas
```

Los temas, además, **no usan el `bl_label` del menú para saber cuál está activo**, sino
`preferences.themes[0].filepath`. Por eso su `post_cb` existe.

### Coste estimado, con la parte ya escrita

Se escribió y **compila** (`nb install` rc=0), pero **no se verificó**, así que se ha
revertido: en este proyecto nada se retira ni se da por bueno sin línea base. El código
queda fuera del árbol, en `/tmp/.../etapa2-sin-verificar/` (`etapa2.patch`,
`fl_preset_theme.cc`, `fl_theme_ops_selftest.cc`), como referencia y no como entrega.

| Pieza | Líneas | Estado |
|---|---:|---|
| `fl_preset_theme.cc` (la tabla de una fila + ayudantes de tema) | 167 | escrito, compila, **sin verificar** |
| `SCRIPT_OT_execute_preset` en `fl_preset_ops.cc` | ~105 | escrito, compila, **sin verificar** |
| Las tres filas de tema en `fl_preset_add_ops.cc` | ~100 | escrito, compila, **sin verificar** |
| Arnés `--fl-*-theme-ops` | 270 | escrito, **con un fallo conocido** (ver abajo) |

**El fallo conocido del arnés**, para que nadie lo repita: volcaba
`view_3d.space.gradients` como si fuera un array de flotantes y **no lo es** — es un
puntero a `ThemeGradientColors`. El valor bueno para un resumen es
`view_3d.space.gradients.high_gradient` (3 flotantes) o `view_3d.grid` (4).

Lo que le falta a esa mitad es exactamente esto, y en este orden:

1. Corregir el atributo del resumen del arnés.
2. Capturar la línea base contra un binario que todavía tenga el Python. **Aviso
   importante**: `/tmp/Blender-ref-ops3.app` es el que vale, y es una copia en `/tmp`, o
   sea que no sobrevive a un reinicio. Si ya no está, hay que reconstruirlo desde el
   commit anterior a `4211271e494`.
3. Retirar de `presets.py`: `ExecutePreset`, las tres clases de tema, `AddPresetBase`,
   `_call_preset_cb` y `_is_path_readonly`. Quedan ~90 líneas (los dos menús de presets
   de operador y `_operator_path`).
4. Retirar `scripts/modules/rna_xml.py`.
5. Comprobar que `--fl-check-preset-optypes` sigue dando **93/93** — la línea base ya
   incluye los cuatro operadores, así que sirve tal cual y no hay que recapturar nada.

### La decisión que hay que tomar al hacerlo, y está tomada a medias

Migrar `script.execute_preset` a C++ **se lleva por delante el último recurso
`bpy.utils.execfile`** para un `.py` heredado que el lector nativo no sepa convertir. Es
la deuda 2 de [`PRESETS-A-DATOS.md`](PRESETS-A-DATOS.md), que decía que era «lo último que
se quita». Las condiciones para quitarlo **ya se cumplen**: el árbol tiene **cero**
presets `.py` y el lector nativo entiende los cinco de FFmpeg con su guardián. Lo que se
pierde es ejecutar código arbitrario de un `.py` escrito a mano por el usuario, y el
sustituto es el rechazo con el motivo a la vista — el mismo contrato que eligió
`wm.properties_edit` para su antiguo `eval()`. **Hay que escribirlo en el mensaje del
commit, no dejarlo pasar de largo.**

---

## Estado

| Qué | Estado |
|---|---|
| Serialización XML nativa (leer y escribir) | ✅ verificada, 6839/6839 |
| Introspección de RNA nativa | ✅ verificada |
| `rna_xml.py` retirado | ❌ **no**: sus tres llamadas siguen vivas |
| `script.execute_preset` en C++ | ❌ no (escrito y revertido por no estar verificado) |
| Los tres operadores de tema en C++ | ❌ no (ídem) |
| El quinto puente de C++ a Python | **abierto**, pero ya sin su bloqueo técnico |

Dicho de otra manera: lo que bloqueaba esto era no tener con qué leer y escribir el XML
de un tema sin intérprete. **Eso ya no bloquea.** Lo que queda es fontanería de
operadores con una línea base que capturar, y está medido arriba pieza por pieza.
