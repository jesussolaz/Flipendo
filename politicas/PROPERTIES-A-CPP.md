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

