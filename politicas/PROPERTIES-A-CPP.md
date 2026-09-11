# Pestañas del editor de Propiedades en C++

## Primera unidad: Effects

`scripts/startup/bl_ui/properties_data_shaderfx.py` era una unidad autónoma de 31 líneas y un
solo panel. Ahora `DATA_PT_shader_fx` se declara en
`space_buttons/fl_properties_data_shaderfx.cc` con `FL_ui_registry.hh`:

- espacio `PROPERTIES`, región `WINDOW`, contexto `shaderfx`;
- etiqueta `Effects`, bandera `PANEL_TYPE_NO_HEADER`;
- menú enum `OBJECT_OT_shaderfx_add.type` y `uiTemplateShaderFx` nativos.

El bloque de registro conserva todos sus metadatos y el bloque de diseño conserva el único
botón visible, `Add Effect`. El módulo desaparece también de `bl_ui/__init__.py`, de modo que no
se importa ni ejecuta Python para esta pestaña.

Mientras convivan paneles nativos y Python de Propiedades con `order=0`, el panel nativo se
registra al comienzo de la familia en vez de en la posición intermedia que imponía el orden de
imports Python. Es una deuda temporal de orden global, no de contexto ni de dibujo; desaparece
al migrar la familia completa o al introducir reservas de orden estables en el registro.


## Segunda unidad: la pestaña del objeto Vacío

`scripts/startup/bl_ui/properties_data_empty.py` (84 líneas, **dos** paneles) pasa entera a
`space_buttons/fl_properties_data_empty.cc`. Se migra la pestaña **completa**, nunca un panel
suelto: un panel aislado cambia de posición dentro de la región y el volcado marca diferencia
aunque no cambie ni un campo (`UI-A-CPP.md`).

- `DATA_PT_empty` — contexto `data`, etiqueta `Empty`, contexto de traducción
  `BLT_I18NCONTEXT_ID_ID`, `poll` = `ob and ob.type == 'EMPTY'`.
- `DATA_PT_empty_image` — etiqueta `Image`, `poll` con además `empty_display_type == 'IMAGE'`.

**Trampas del dibujo**, que son las que cambian el volcado:

- `depth_row.enabled = not ob.show_in_front` es `uiLayoutSetEnabled` sobre la **fila**, no sobre
  la columna: si se pone en la columna se apaga también el `Side` de debajo.
- `col = layout.column(align=False, heading="Opacity")` lleva `use_property_decorate = False`
  porque el decorador lo pone a mano `row.prop_decorator(ob, "color", index=3)`. Sin apagarlo
  salen **dos** decoradores.
- `sub.active = ob.use_empty_image_alpha` es `uiLayoutSetActive`, no `SetEnabled`: son dos
  aspectos distintos en el volcado.
- Las propiedades con índice (`empty_image_offset` 0 y 1, `color` 3) necesitan la sobrecarga de
  `prop()` que toma `PropertyRNA *` e índice; la que toma el nombre no lo admite.

**Verificado** (`--fl-check-ui` sobre una copia del binario, no sobre el instalado):

| Volcado | Resultado |
|---|---|
| Registro (`baseline-python.txt`) | **2.113 bloques, 2.112 idénticos, 1 distinto, 0 faltan, 0 sobran** |
| Diseño (`baseline-python-layout.txt`) | **2.004 bloques, 1.983 idénticos, 21 distintos, 0 faltan, 0 sobran** — y **ninguno de los 21 es de esta pestaña** |

El único bloque distinto del registro es `REGION PROPERTIES WINDOW`, y **ya lo estaba antes**:
es la deuda de orden global que documenta la primera unidad. La divergencia arranca donde los
paneles nativos se registran al principio de la familia; `DATA_PT_empty` y `DATA_PT_empty_image`
se suman a esa misma cabecera desplazada. `0 faltan, 0 sobran` es lo que importa: los dos
paneles existen con sus metadatos idénticos.

Los 21 bloques distintos del diseño son menús y cabeceras de **presets**
(`RENDER_PT_ffmpeg_presets`, `USERPREF_MT_keyconfigs`, `TEXT_MT_templates*`…): cambiaron al pasar
los presets de `.py` a `.fpreset` y al retirar el Python de las plantillas. No los toca esta
pestaña.

## Tercera unidad: Nodos de simulación (pestaña Físicas)

`scripts/startup/bl_ui/properties_physics_geometry_nodes.py` (64 líneas, **un** panel, **cero**
dependencias compartidas) pasa entera a
`space_buttons/fl_properties_physics_geometry_nodes.cc`. Es una unidad completa aunque la
pestaña `physics` tenga más paneles: el fichero entero era este panel y solo este.

- `PHYSICS_PT_geometry_nodes` — contexto `physics`, etiqueta `Simulation Nodes`,
  `PANEL_TYPE_DEFAULT_CLOSED`, `poll` = algún objeto seleccionado y editable con modificador
  `eModifierType_Nodes`.

**Detalle que se reproduce a propósito:** el texto de los botones cambia según
`len(context.selected_editable_objects) > 1`, **no** según cuántos de ellos tienen nodos. Es
una inconsistencia del original (el `poll` mira una cosa y el texto otra), y se copia tal cual:
cambiarla sería inventar comportamiento.

**Verificado** igual que la anterior, sobre copia del binario:

| Volcado | Resultado |
|---|---|
| Registro | **2.113 bloques, 2.112 idénticos, 1 distinto, 0 faltan, 0 sobran** |
| Diseño | **2.004 bloques, 1.983 idénticos, 21 distintos, 0 faltan, 0 sobran** — 0 de los 21 son de esta pestaña |

Las mismas cifras exactas que antes de añadirla: no mueve ni un bloque más.

## Lo siguiente, medido (para quien lo coja)

Criterio: pestaña completa, y primero las que **no** dependen de los mixins compartidos
(`PropertyPanel` de `rna_prop_ui` y `PropertiesAnimationMixin` de `bl_ui.space_properties`).
Los dos ya tienen equivalente C++ escrito como `static` dentro de `fl_world_buttons.cc`
(`draw_custom_properties` y `draw_action_and_slot_selector`): **el primer paso de la siguiente
tanda es sacarlos a `FL_properties_ui.hpp`**, y a partir de ahí las demás salen casi solas.

| Fichero | Líneas | Paneles | Dependencias compartidas |
|---|---:|---:|---|
| `properties_animviz.py` | 123 | 0 (solo mixins de dibujo) | ninguna — es una **biblioteca**, no una pestaña; cae con sus usuarios |
| `properties_collection.py` | 146 | 6 | `PropertyPanel` |
| `properties_data_lattice.py` | 105 | 4 | `PropertyPanel`, `PropertiesAnimationMixin` |
| `properties_data_metaball.py` | 137 | 6 | `PropertyPanel`, `PropertiesAnimationMixin` |
| `properties_data_speaker.py` | 156 | 6 | `PropertyPanel`, `PropertiesAnimationMixin` |
| `properties_data_pointcloud.py` | 175 | 3 | `PropertyPanel`, `UIList` |
| `properties_workspace.py` | 193 | 3 | `PropertyPanel`, `UIList` |
| `properties_data_curves.py` | 219 | 5 | `PropertyPanel`, `PropertiesAnimationMixin`, `UIList` |
| `properties_data_volume.py` | 239 | 8 | `PropertyPanel`, `PropertiesAnimationMixin`, `UIList` |
| `properties_view_layer.py` | 300 | 10 | `PropertyPanel`, `UIList` |

Quedan **25.263 líneas** en `scripts/startup/bl_ui/properties_*.py` (eran 25.412 y eran 43
ficheros; ahora 41). Las gordas —`properties_particle` 2.312, `properties_paint_common` 1.965,
`properties_constraint` 1.900, `properties_physics_fluid` 1.629, `properties_freestyle` 1.348—
no son candidatas a una sola sesión, y `properties_freestyle` además cae entera con Freestyle
(`INVENTARIO-PYTHON.md §2.9`).
