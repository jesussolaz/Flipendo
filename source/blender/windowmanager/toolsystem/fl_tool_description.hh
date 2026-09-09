/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Piezas comunes de las descripciones CALCULADAS del catalogo.
 *
 * Siete herramientas no tienen un tooltip fijo: lo arman leyendo el keymap DEL USUARIO
 * y metiendo dentro los atajos que esa persona tenga puestos. Es informacion que no
 * puede vivir en una cadena, porque cambia con la configuracion de cada uno.
 *
 * Estas ayudas viven aqui, y no copiadas en cada fichero, porque en el Python tambien
 * son de modulo (`space_toolsystem_toolbar.py:27`): la primera version las tuvo
 * duplicadas y esa es justo la forma en que dos tooltips acaban divergiendo.
 */

#ifndef __FL_TOOL_DESCRIPTION_HH__
#define __FL_TOOL_DESCRIPTION_HH__

#include <string>

struct bContext;
struct wmKeyMap;
struct wmKeyMapItem;

namespace flipendo::toolsystem {

/**
 * `kmi_to_string_or_none` del Python: el atajo escrito, o `<none>` si esa accion no
 * tiene ninguno asignado.
 *
 * El `<none>` no se traduce a proposito: entra tal cual dentro de un texto que ya viene
 * traducido, igual que en el Python.
 */
std::string kmi_to_string_or_none(const wmKeyMapItem *kmi);

/**
 * El elemento de un keymap MODAL que corresponde a un identificador de su enumeracion
 * ('SNAP_ON', 'CANCEL'...).
 *
 * Se resuelve por la tabla de enumeracion que el propio keymap lleva colgada, no por un
 * numero escrito a mano: esos valores son un enum privado de quien define el modal
 * (`view3d_placement.cc`), y copiarlos aqui los dejaria desincronizados en silencio en
 * cuanto alguien inserte uno en medio.
 */
const wmKeyMapItem *modal_kmi_from_identifier(const wmKeyMap *km, const char *identifier);

/** El keymap modal del usuario con ese nombre, o `nullptr`. */
const wmKeyMap *user_modal_keymap(const bContext *C, const char *idname);

}  // namespace flipendo::toolsystem

#endif /* __FL_TOOL_DESCRIPTION_HH__ */
