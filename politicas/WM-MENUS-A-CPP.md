# Menús globales de `wm.py`: de Python a C++

## Estado

Los cuatro `MenuType` que cerraban `scripts/startup/bl_operators/wm.py` son nativos:

- `WM_MT_splash_quick_setup`
- `WM_MT_splash`
- `WM_MT_splash_about`
- `WM_MT_region_toggle_pie`

Se registran justo después de `WM_menutype_init()`, mediante `FL_ui_registry.hh`. Esto es
anterior al registro de interfaz Python, pero posterior a la creación del registro global.
`wm.py` y su entrada en `bl_operators/__init__.py` se eliminan: no queda un módulo vacío ni
clases Python desregistradas como deuda oculta.

## Contrato conservado

- El splash enumera General y plantillas de aplicación, limita a cinco los ficheros recientes,
  conserva las ramas de primeros pasos/offline y los enlaces web.
- Quick Setup sigue exponiendo idioma, tema, keyconfig y las preferencias nativas del keyconfig,
  además de guardar o importar preferencias cuando el operador de importación da `poll`.
- About usa directamente los símbolos de información de compilación.
- Region Toggle distribuye por alineación las regiones reales del área y mantiene el mismo
  algoritmo de desbordamiento de la tarta.

`--fl-check-ui` confirma los cuatro contratos de registro sin diferencias. La línea base de
diseño conserva una fotografía anterior al keyconfig nativo y por eso no contiene aún
`select_mouse`/`spacebar_action`; no se amputan esos controles funcionales para hacer coincidir
una captura obsoleta.

## Resultado

La última retirada elimina 2.085 líneas de `wm.py`. Sumada a los commits anteriores de este
carril, el fichero completo (operadores de contexto, sistema, propiedades, propietario,
documentación, `properties_edit*`, `batch_rename` y menús) deja de formar parte del arranque.

