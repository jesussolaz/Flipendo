# Las extensiones y el intérprete — qué se retira, qué se conserva, qué se repone en C++

> Medido en el árbol el 2026-09-11 sobre `flipendo-main`, con el binario en la mano.
> Encargo: demostrar si `scripts/addons_core/bl_pkg` (19.930 líneas, 18 ficheros) se
> queda sin objeto en un editor sin Python, y ejecutar lo que salga de la evidencia.
>
> **La conclusión no es la que se esperaba, y por eso este documento existe.**
> `INVENTARIO-PYTHON.md §2.6` clasificó `bl_pkg` como **RET** entero, con el motivo
> «es el mercado de addons de Python de Blender». Ese motivo es cierto para el 90 %
> del código y **falso para el resto**. Al medirlo salieron dos cosas que cambian la
> decisión: el gestor también instala **temas**, que son datos y no código; y
> retirarlo hoy degrada el editor de Jesús sin que exista sustituto. Donde este
> documento discrepa del inventario, manda este documento.

---

## 0. La foto, medida

```
git ls-files scripts/addons_core/bl_pkg | xargs wc -l   ->  19.930 (18 ficheros)
```

| Fichero | Líneas | Qué es |
|---|---:|---|
| `cli/blender_ext.py` | 5.688 | El gestor de paquetes propiamente dicho: formato del manifiesto, validación, empaquetado, cliente del repositorio, ruedas `pip`. No importa `bpy`: es un programa independiente |
| `bl_extension_ops.py` | 4.101 | 32 operadores `extensions.*` |
| `bl_extension_ui.py` | 2.385 | El dibujo de Preferencias › Extensiones y › Add-ons, 6 clases de UI |
| `bl_extension_utils.py` | 2.356 | Envoltorio del CLI: subproceso, hilos, caché de repositorios |
| `tests/` (6) | 2.681 | Las pruebas del CLI y del lado Blender |
| `bl_extension_cli.py` | 934 | `blender --command extension build|validate|server-generate` |
| `__init__.py` | 807 | Registro, preferencias del addon, manejadores de arrastrar-y-soltar, menú de temas |
| `bl_extension_notify.py` | 628 | Hilo que consulta actualizaciones al arrancar |
| `Makefile` | 206 | Objetivos de conveniencia: mypy/ruff/pylint/flake8 y lanzar las pruebas |
| `extensions_map_from_legacy_addons.py` | 97 | Tabla de datos: nombre de addon de Blender 4.1 → id de extensión |
| `example_extension/` (3) | 47 | Paquete de ejemplo, solo lo nombra el `Makefile` |

---

## 1. Qué instala hoy de verdad — la pregunta que cambia la conclusión

**Dos tipos de extensión, no uno.** `cli/blender_ext.py:1749`:

```python
def pkg_manifest_validate_field_type(value: str, strict: bool) -> str | None:
    # NOTE: add "keymap" in the future.
    value_expected = {"add-on", "theme"}
```

Confirmado en otros tres sitios: `blender_ext.py:1576` y `:1603` (las listas de
etiquetas válidas se llevan por separado para `add-on` y para `theme`),
`bl_extension_ops.py:173` (`"THEME": "theme"`) y el filtro de la interfaz
`bl_extension_ui.py:912` (`show_themes = filter_by_type in {"", "theme"}`).

**Un tema no es código Python.** Es un `theme.xml` dentro del paquete:
`bl_extension_ops.py:497-510` lo localiza con `pkg_theme_file_list()` y lo activa
con `bpy.ops.script.execute_preset(filepath=..., menu_idname="USERPREF_MT_interface_theme_presets")`,
el mismo camino que un preset de tema de fábrica. Y `__init__.py:748` **cuelga el
listado de temas instalados del menú de temas del editor**:

```python
from bl_ui.space_userpref import USERPREF_MT_interface_theme_presets
USERPREF_MT_interface_theme_presets.append(theme_preset_draw)
```

**Consecuencia, y es la del encargo escrita al revés:** «instalar un tema descargado
y elegirlo en el desplegable de temas» es **capacidad de Flipendo**, no mercado de
Python. Datos que entran, datos que se pintan. Se conserva y se repone en C++.

**Lo que NO admite:** bibliotecas de assets ni paquetes de datos. El único otro tipo
está en un comentario (`# NOTE: add "keymap" in the future`), sin implementación. Las
bibliotecas de assets de Blender no pasan por `bl_pkg`: son rutas del catálogo, otra
cosa. Así que el reparto real de las 19.930 líneas es: **temas, unos cientos;
addons de Python, el resto**.

### Los repositorios de fábrica

No los define `bl_pkg`: **ya están en C++**, en
`source/blender/blenkernel/intern/preferences.cc:218-250`
(`BKE_preferences_extension_repo_add_defaults_all`). Son tres, y se comprobaron
volcándolos del binario en ejecución:

| Módulo | Directorio | `remote_url` |
|---|---|---|
| `blender_org` | `…/extensions/blender_org` | `https://extensions.blender.org/api/v1/extensions/` |
| `user_default` | `…/extensions/user_default` | *(ninguna: local)* |
| `system` | `<bundle>/4.5/extensions/system` | *(ninguna: local)* |

El modelo de datos (`bUserExtensionRepo` en DNA, su RNA, el panel
`USERPREF_PT_extensions_repos` y las rutas `BLENDER_USER_EXTENSIONS` /
`BLENDER_SYSTEM_EXTENSIONS` de `BKE_appdir.hh:164,169`) es **C++ y `bl_ui`, no
`bl_pkg`**. `bl_pkg` es solo el cliente que habla con esos repositorios. Es decir:
**la mitad de abajo del gestor de extensiones ya está migrada**, y nadie lo había
dicho.

De los tres, **uno solo es el mercado de Python**: `blender_org`. Los otros dos son
carpetas locales del usuario.

---

## 2. Qué se rompe si desaparece — medido, no razonado

Se midió moviendo `bl_pkg` fuera del bundle instalado, arrancando el binario con una
copia de la configuración real de Jesús (`BLENDER_USER_RESOURCES` a un duplicado, con
su `userpref.blend` y su directorio `extensions/`), y devolviéndolo después.

### 2.1 Lo que se rompe

```
Traceback (most recent call last):
  File ".../scripts/modules/addon_utils.py", line 1214, in _initialize_extensions_compat_data
  File ".../scripts/modules/addon_utils.py", line 1135, in _initialize_extensions_compat_ensure_up_to_date
  File ".../scripts/modules/addon_utils.py", line 1061, in _extension_compat_cache_create
    import bl_pkg
ModuleNotFoundError: No module named 'bl_pkg'
Extension: unexpected error detecting cache, this is a bug!
Add-on not loaded: "bl_pkg", cause: No module named 'bl_pkg'
```

Tres roturas, en tres ficheros de **tres carriles distintos**:

1. **`scripts/modules/addon_utils.py:1061`** (carril M) hace `import bl_pkg` para
   validar la compatibilidad de las ruedas `pip` de **cada extensión activada**. Sin
   `bl_pkg`, excepción en el arranque, en cada arranque.
2. **`scripts/startup/bl_ui/space_userpref.py:2328`** (carriles D/G) declara
   `USERPREF_PT_extensions` con `def draw(self, context): pass`. Es una **cáscara
   vacía** que `bl_pkg` rellena. Sin `bl_pkg`, la pestaña Preferencias › Extensiones
   existe y está **en blanco**. La pestaña Add-ons no se rompe: `:2450` comprueba
   `self.is_extended()` y, si nadie la ha extendido, dibuja la lista clásica con
   `preferences.addon_install` / `preferences.addon_refresh` (operadores de
   `bl_operators/userpref.py`, que siguen).
3. **Cinco llamadas desde C++ a operadores que dejarían de existir**:
   `interface_template_status.cc:425,450,474` (`EXTENSIONS_OT_userpref_show_for_update`
   ×2 y `EXTENSIONS_OT_userpref_show_online`) y `userpref_ops.cc:793-798`
   (`extensions.package_install`, `extensions.package_install_files`,
   `extensions.userpref_allow_online_popup`: el arrastrar-y-soltar de un `.zip` o de
   una URL sobre la ventana).

Ninguno de esos tres ficheros es de este carril.

### 2.2 Lo que NO se rompe — MPFB2, y hay que decirlo claro

**Retirar `bl_pkg` NO impide a Jesús seguir usando MPFB2.** Medido:

| Escenario | `bl_ext.user_default.mpfb` cargado | Clases `MPFB*` registradas |
|---|---|---|
| Con `bl_pkg` | sí | **237** |
| Sin `bl_pkg`, caché de compatibilidad borrada | sí | **237** |

El motivo es que **`bl_pkg` no carga extensiones: las administra.** Quien carga
`bl_ext.user_default.mpfb` es `addon_utils.py` con el paquete sintético `bl_ext`
(`addon_utils.py:1420`, `_bpy_internal/extensions/junction_module.py`), y ese camino
no pasa por `bl_pkg`. La única cita de `bl_pkg` en `addon_utils` es la validación de
ruedas `pip` — y **MPFB2 no trae ruedas**: su `blender_manifest.toml`
(`type = "add-on"`, `version = "2.0.17"`) no declara `wheels` y su directorio no tiene
carpeta `wheels/`. Por eso la excepción se traga el fallo y MPFB2 registra sus 237
clases igual.

Lo que sí pierde Jesús sin `bl_pkg`: ver MPFB2 en Preferencias › Extensiones,
actualizarlo desde ahí y desinstalarlo desde ahí. Seguiría activándose y
desactivándose desde la pestaña Add-ons, que es la lista clásica.

**Y lo que pierde de verdad el día que no haya intérprete: MPFB2 entero.** MPFB2 es
`type = "add-on"`, 237 clases de Python. No lo salva conservar `bl_pkg`; lo mata el
día que caiga CPython. Eso es una decisión ya tomada del proyecto
(`EDITOR-SIN-CPYTHON.md`) y ÁNIMA depende de ella: los personajes que MPFB2 genera
son **mallas y armaduras en el `.blend`**, no scripts; lo que se pierde es la
herramienta de generación, no los personajes ya hechos. Está anotado en el informe.

---

## 3. La decisión, y por qué

> **No se retira el runtime de `bl_pkg` esta noche.**

Tres razones, en orden de peso:

1. **Admite un tipo que no es Python.** El encargo lo decía: «si admite tipos que NO
   son código Python —temas o assets son datos, y eso sí es capacidad que Flipendo
   podría querer— entonces la conclusión cambia: hay que conservar esa parte y
   reponerla en C++, no borrarla». Admite temas. Conclusión cambiada.
2. **No hay sustituto y la rotura cae en tres carriles ajenos.** Retirarlo hoy deja
   un *traceback* en cada arranque que solo se arregla en `addon_utils.py` (carril M),
   una pestaña de Preferencias en blanco que solo se arregla en `bl_ui` (carriles
   D/G), y cinco llamadas C++ a operadores inexistentes. La regla de la noche es
   clara: si necesitas un fichero de otro carril, buscas otra vía.
3. **Doctrina.** «Migrar, no borrar»: el original se elimina **cuando su sustituto
   C++ existe, está registrado y está verificado**. Aquí no existe ninguno.

Y una razón que no cuenta pero conviene escribir: es una **puerta de un solo
sentido**. 19.930 líneas que nadie va a volver a escribir. Lo barato es esperar a
tener el sustituto; lo caro es equivocarse.

### Lo que sí se retira ahora

| Qué | Líneas | Evidencia de que no es capacidad |
|---|---:|---|
| `Makefile` | 206 | `source/creator/CMakeLists.txt:492` lo **excluye explícitamente** de la instalación. Lanza mypy/ruff/pylint/flake8 sobre Python: utillaje de linting de una plataforma que se deprecia, con herramientas que este Mac no tiene instaladas |
| `example_extension/` (3 fich.) | 47 | `source/creator/CMakeLists.txt:494` lo excluye de la instalación. Su único nombrador en todo el árbol es `Makefile:63`, que se va en el mismo cambio |

**253 líneas.** Es poco, y se dice sin adornos: en `bl_pkg` lo que sobra de verdad
está *entretejido* con lo que no, y separarlo a las 03:30 sin arnés de verificación
es cómo se rompe un editor. La retirada grande va cuando exista el sustituto de §4.

### Lo que se conserva y por qué, uno a uno

- **`tests/` (2.681)** — son la **única red de regresión de `cli/blender_ext.py`**,
  que se conserva. Retirar las pruebas de lo que conservas es quedarse ciego, que es
  exactamente lo que el encargo prohíbe para `tests/python`. Se van con el CLI, no
  antes. Entran en el plan de `TESTS-A-CPP.md`.
- **`cli/blender_ext.py` (5.688)** — contiene el **formato del manifiesto y el
  empaquetado**, que es lo que hay que leer para escribir el lector C++ de temas. Es
  la especificación.
- **`extensions_map_from_legacy_addons.py` (97)** — tabla de datos de la migración
  Blender 4.1 → 4.2, sin valor para Flipendo, pero **código vivo**:
  `bl_extension_ui.py:327,760` y `bl_extension_ops.py:3240` la leen. Quitarla obliga
  a operar `bl_extension_ui.py` en caliente, sin arnés, tocando el panel de
  Preferencias que este mismo documento se compromete a no romper. No compensa por
  97 líneas. Se va con el bloque.
- **El resto del runtime** — por §3.1-3.3.

---

## 4. Qué se repone en C++, y en qué orden

La mitad de abajo ya está (§1). Falta la de arriba. Tres piezas, de menor a mayor:

### 4.1 Temas instalados como datos (la capacidad que hay que salvar) — 12-20 h·p

Hoy: `theme_preset_draw` (`__init__.py:639-670`) recorre los repositorios, busca los
paquetes con `type == "theme"`, lista sus `*.xml` y los añade al menú
`USERPREF_MT_interface_theme_presets`; `extension_theme_enable()` los activa con
`script.execute_preset`.

En C++: al construir el menú de temas, recorrer los directorios de
`BKE_preferences_extension_repo_*`, leer el `blender_manifest.toml` de cada paquete
(un TOML mínimo: `id`, `name`, `type`, `version`), quedarse con los de `type =
"theme"` y añadir sus `.xml` al menú. **Cero red.** Encaja con el trabajo de
`PRESETS-A-DATOS.md` (formato `.fpreset`, `fl_preset_file.cc`): un tema de extensión
es un preset más, en otro directorio.

Identificadores a conservar sin cambios (doctrina, regla 3):
`EXTENSIONS_OT_package_theme_enable` / `_disable`, con sus propiedades `pkg_id`
(string) y `repo_index` (int).

### 4.2 Instalar desde disco — 20-35 h·p

`extensions.package_install_files`: descomprimir un `.zip`, validar el manifiesto,
copiarlo al repositorio local. Sin red y sin `pip`. Es lo que hace falta para que
arrastrar un tema a la ventana siga funcionando (`userpref_ops.cc:793` ya llama a ese
idname desde C++: el puente está puesto, falta el otro lado).

### 4.3 El cliente remoto — **no se repone**

Sincronizar con `extensions.blender.org`, descargar, resolver versiones, instalar
ruedas `pip`, avisar de actualizaciones. Es el mercado de add-ons de Python de
Blender: 100 % de lo que sirve ahí es Python, y un Flipendo sin intérprete no puede
ejecutar ni uno. **Se retira con el bloque, y el repositorio `blender_org` de
`preferences.cc:220` se retira con él** (no es de este carril: queda anotado).

### Orden

`4.1` → retirar `bl_extension_notify.py` y el camino remoto → `4.2` → retirar
`bl_extension_ops/ui/utils` → retirar `cli/blender_ext.py` y sus `tests/` → quitar
los cinco puentes C++ (`interface_template_status.cc` ×3, `userpref_ops.cc` ×2) y la
entrada `"bl_pkg"` de `blendfile.cc:1515`.

**Ninguno de esos pasos se da sin su `--fl-dump-*` delante.** El arnés que falta y
hay que escribir primero: `--fl-dump-extensions`, que vuelque por repositorio los
paquetes instalados, su tipo, su versión y si están activos, para congelar la línea
base con el Python todavía vivo. Es el patrón de `--fl-check-tools` y
`--fl-check-keymap`, y sin él esto no se toca.

---

## 5. Qué pierde el usuario

| Cuándo | Qué pierde |
|---|---|
| **Hoy** (este cambio) | Nada. Se van 253 líneas de andamiaje de desarrollo que CMake ya excluía del bundle |
| Al llegar §4.3 | Instalar extensiones de terceros desde `extensions.blender.org`. Avisos de actualización |
| El día sin CPython | **MPFB2 y cualquier add-on de Python**, `bl_pkg` conservado o no. Los personajes de ÁNIMA ya generados no se pierden: son datos del `.blend`. La herramienta de generación, sí |
| Nunca | Los **temas**. Es la capacidad que este documento rescata de la papelera |

---

## 6. Trampas encontradas, para quien siga

1. **`BLENDER_SYSTEM_SCRIPTS` no sustituye la ruta de scripts del bundle: la añade.**
   La primera medición «sin `bl_pkg`» dio falso negativo por esto: el binario lo
   seguía cargando del bundle. Verificado imprimiendo `bl_pkg.__file__` y
   `bpy.utils.script_paths()`, que devolvía **las dos** rutas. Para medir de verdad
   hay que mover el directorio fuera del bundle.
2. **La caché de compatibilidad esconde el fallo.** `…/extensions/.cache/compat.dat`
   hace que `_extension_compat_cache_create()` —la función que importa `bl_pkg`— no se
   ejecute. Sin borrarla, el arranque sin `bl_pkg` parece limpio. Hay que borrarla
   antes de cada medición.
3. **`INVENTARIO-PYTHON.md` cifra `bl_pkg` en 15 ficheros y 19.687 líneas; son 18 y
   19.930.** La diferencia son los tres del `example_extension/`. Sin importancia,
   pero que conste para que las sumas cuadren.
4. **`doc/python_api` no era solo documentación del intérprete.** Ver §7.

---

## 7. Nota sobre `doc/python_api` (mismo encargo)

Se retiró el generador Sphinx y los 103 ejemplos y 20 `.rst` del lado `bpy`
(138 ficheros, 15.601 líneas). **No se retiraron 111 ficheros (12.608 líneas)**, y
por un motivo que no estaba en el inventario: `rst/bge_types/**` (85 ficheros) y
`rst/bge.*.rst` (7) son la **única especificación escrita** de `KX_GameObject`,
`KX_Scene`, los 60 ladrillos lógicos `SCA_*`, `BL_ArmatureObject` y `KX_2DFilter`
—clases **C++ de `source/gameengine/`** que Flipendo conserva—, y están escritas a
mano, no generadas. `rst/bgui/**` (16) documenta la biblioteca de interfaz del juego
que `INVENTARIO-PYTHON.md §2.4` marca **MIG** y `PLAYER-SIN-CPYTHON.md §3.3` da por
perdida: es el pliego de condiciones de esa reposición.

Queda pendiente: esos `.rst` ya no tienen constructor. Cuando el motor exponga su
modelo de objetos desde C++ habrá que decidir si se generan desde el C++ o se
mantienen como texto. **No se borran mientras no haya sustituto.**

Muerto por este cambio y anotado para quien lleve la infraestructura (no es de este
carril): el objetivo `doc_py` de `GNUmakefile:621-628` y la acción de GitHub
`.github/workflows/publish-api-and-stubs.yml` (compila UPBGE en ubuntu-24.04 los
domingos y publica en los dominios de UPBGE; ya era inaplicable a un fork Mac sin
Python).
