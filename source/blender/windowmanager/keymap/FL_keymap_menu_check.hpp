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
 * Que NO cubre, dicho antes de que alguien se fie de mas
 * ----------------------------------------------------
 * Comprueba la configuracion por defecto **tal y como la construye
 * `flipendo::keymap::register_default()`**, o sea con los `Params` por defecto. Las
 * combinaciones que esos `Params` no activan —`VIEW3D_MT_snap` frente a
 * `VIEW3D_MT_snap_pie`, `VIEW3D_MT_shading_ex_pie`...— nombran menus que aqui no
 * aparecen. Esos salen del cruce de conjuntos sobre el arbol que describe la politica,
 * no de esta comprobacion. Por eso el numero de nombres que da esto (127) es menor que
 * los 136 de la politica, y **no** tiene nada que ver con que 127 de los 133 esten ya
 * migrados: que coincidan es casualidad.
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
