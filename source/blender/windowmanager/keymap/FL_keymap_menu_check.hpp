/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * El puente mas silencioso del arbol, con una alarma.
 *
 * Un `wmKeyMapItem` de `wm.call_menu` / `wm.call_menu_pie` / `wm.call_panel` **no
 * guarda el menu, guarda su nombre**: una cadena que se resuelve al pulsar la tecla con
 * `WM_menutype_find()` / `WM_paneltype_find()`. Si nadie ha dado de alta ese nombre, la
 * busqueda devuelve `nullptr`, el operador devuelve `OPERATOR_CANCELLED`, **la tecla no
 * hace nada y no se imprime ni un mensaje de error**.
 *
 * Mientras los menus estaban en Python el agujero se abria solo con compilar sin
 * interprete. Ahora que 127 de los 133 son nativos el agujero se puede abrir al reves:
 * basta con que alguien renombre un `idname` en C++, o borre una clase de Python sin
 * poner su sustituto, para que una tecla deje de funcionar en silencio. Un fallo que no
 * se ve hasta que un usuario pulsa la tecla y se encoge de hombros no es aceptable en
 * este proyecto, asi que se comprueba.
 *
 * Esto lo mide: construye el keymap NATIVO en una configuracion aparte —igual que
 * `FL_keyconfig_dump_native`, y por eso funciona en `--background`, donde no se carga el
 * keymap del editor—, recoge todos los nombres que nombra, y los resuelve uno a uno.
 *
 * Uso:  Blender --background --fl-check-keymap-menus [fichero]
 *
 * Con `fichero` escribe ademas la lista ordenada de los nombres que ve, para poder
 * cruzarla con las clases de Python vivas sin volver a adivinar el `grep`.
 *
 * Las 22 configuraciones, y por que no vale con una
 * -------------------------------------------------
 * El keymap depende de 17 preferencias, y algunas cambian **que menu abre una tecla**:
 * `VIEW3D_MT_snap` frente a `VIEW3D_MT_snap_pie`, `VIEW3D_MT_shading_pie` frente a
 * `_ex_pie`, `VIEW3D_MT_object_mode_pie` solo con «Tab abre el radial de modos»...
 * Mirar solo la configuracion de fabrica dejaria sin comprobar justo esos, que son los
 * que fallan mas callados porque solo los ve quien cambio la preferencia.
 *
 * Asi que se construye el keymap **22 veces**: una de fabrica y una por cada preferencia
 * movida por separado (las banderas mas las tres enumeraciones). No es una permutacion
 * exhaustiva —serian 2^17— sino una variacion simple desde el defecto, que basta para
 * que todo nombre alcanzable aparezca al menos una vez. Si algun dia hiciera falta un
 * nombre que solo sale con DOS preferencias a la vez, se anade esa pareja a la tabla
 * `variantes` de `fl_keymap_menu_check.cc`.
 *
 * Con eso salen **136 nombres distintos** (127 de fabrica + 9 que solo aparecen al mover
 * una preferencia), que son exactamente los 136 que la politica tenia contados por
 * cruce de conjuntos sobre el codigo fuente. Dos metodos independientes, el mismo
 * conjunto.
 *
 * Ver `politicas/MENUS-DEL-KEYMAP-A-CPP.md`.
 */

#ifndef __FL_KEYMAP_MENU_CHECK_HPP__
#define __FL_KEYMAP_MENU_CHECK_HPP__

struct wmWindowManager;

/**
 * Comprueba que TODO nombre de menu o panel que invoca el keymap nativo esta dado de
 * alta. Escribe el informe por salida estandar y devuelve true solo si no falta ninguno.
 *
 * \param list_filepath: si no es nulo, escribe ahi la lista ordenada de nombres con sus
 * cuentas (`nombre call_menu=N call_menu_pie=N call_panel=N keymap='...'`).
 */
bool FL_keymap_check_menus(wmWindowManager *wm, const char *list_filepath);

#endif /* __FL_KEYMAP_MENU_CHECK_HPP__ */
