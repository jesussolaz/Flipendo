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
