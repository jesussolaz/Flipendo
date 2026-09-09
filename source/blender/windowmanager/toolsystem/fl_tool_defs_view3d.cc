/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Herramientas comunes de la vista 3D y las de seleccion. Transliteracion de
 * `_defs_view3d_generic` y `_defs_view3d_select`
 * (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:105` y `:425`).
 *
 * Siete herramientas, de las cuales seis salen en la linea base; `builtin.none` no,
 * porque el Python la declara y no la coloca en ninguna barra.
 *
 * La regla es una de las siete herramientas que NO tienen descripcion literal sino
 * calculada: mete dentro del tooltip los atajos que el USUARIO tenga puestos para anadir
 * y quitar mediciones, asi que el texto no se puede congelar en la tabla.
 */

#include <fmt/format.h>

#include "BLT_translation.hh"

#include "WM_keymap.hh"

#include "wm_event_types.hh"

#include "fl_tool_defs_view3d.hh"

namespace flipendo::toolsystem {

/* -------------------------------------------------------------------- */
/** \name `_defs_view3d_generic`
 * \{ */

namespace defs_view3d_generic {

/* El cursor 3D es de las pocas herramientas cuyos ajustes son los del propio operador
 * que dispara, sin nada del contexto: profundidad y orientacion de `view3d.cursor3d`. */
static const PropRow cursor_settings[] = {
    {PropSource::Operator, "view3d.cursor3d", "use_depth"},
    {PropSource::Operator, "view3d.cursor3d", "orientation"},
};

const ToolDecl cursor = {
    /*idname*/ "builtin.cursor",
    /*label*/ N_("Cursor"),
    /*description*/ N_("Set the cursor location, drag to transform"),
    /*description_fn*/ nullptr,
    /*icon*/ "ops.generic.cursor",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Cursor",
    /*keymap_fallback*/ nullptr,
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(cursor_settings),
};

/**
 * La entrada vacia del bloque del cursor: el mismo icono, ninguna accion y ningun
 * ajuste. No la coloca ninguna barra del Python, asi que no llega a la linea base y no
 * hay con que contrastar sus campos; se traslada igual porque una herramienta perdida
 * en el camino y una que nunca existio no se distinguen despues.
 *
 * En el Python lleva `keymap=()`, que NO es lo mismo que no llevar keymap: es una tabla
 * de atajos vacia a la que el registro le pondria nombre al colocarla en una barra. Como
 * no la coloca nadie, ese nombre no llega a existir nunca, asi que aqui el keymap se
 * queda a `nullptr` en vez de inventarle uno.
 */
const ToolDecl cursor_click = {
    /*idname*/ "builtin.none",
    /*label*/ N_("None"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.generic.cursor",
};

/**
 * `kmi_to_string_or_none` (`space_toolsystem_toolbar.py:27`): el atajo escrito, o
 * `<none>` si esa accion no tiene ninguno. Ese `<none>` no se traduce; entra tal cual
 * dentro de un texto que ya viene traducido.
 *
 * En el Python es una funcion de modulo que usan las siete descripciones calculadas del
 * catalogo. Aqui se queda local porque hoy la unica trasladada es la de la regla;
 * cuando aparezcan las demas hay que subirla a un sitio comun en vez de copiarla.
 */
static std::string kmi_to_string_or_none(const wmKeyMapItem *kmi)
{
  if (kmi == nullptr) {
    return "<none>";
  }
  return WM_keymap_item_to_string(kmi, false).value_or("");
}

/**
 * `_defs_view3d_generic.ruler.description`.
 *
 * Busca los atajos DENTRO del keymap de la propia herramienta, no en el global: la regla
 * anade y quita mediciones con teclas que solo existen mientras esta activa, y el
 * usuario puede haberlas cambiado.
 *
 * Los operadores se nombran en su forma interna (`VIEW3D_OT_ruler_add`). El Python los
 * escribe como `view3d.ruler_add` y es el RNA quien los convierte al vuelo; aqui no hay
 * RNA por medio, asi que se escriben ya convertidos.
 */
static std::string description_ruler(const bContext * /*C*/, const wmKeyMap *km)
{
  wmKeyMapItem *kmi_add = nullptr;
  wmKeyMapItem *kmi_remove = nullptr;
  if (km != nullptr) {
    /* La busqueda no toca el keymap, pero la API de ventanas no es const-correcta y el
     * contrato si lo es. */
    wmKeyMap *km_mut = const_cast<wmKeyMap *>(km);
    kmi_add = WM_key_event_operator_from_keymap(
        km_mut, "VIEW3D_OT_ruler_add", nullptr, EVT_TYPE_MASK_ALL, 0);
    kmi_remove = WM_key_event_operator_from_keymap(
        km_mut, "VIEW3D_OT_ruler_remove", nullptr, EVT_TYPE_MASK_ALL, 0);
  }

  return fmt::format(fmt::runtime(TIP_("Measure distance and angles\n"
                                       " \u2022 {:s} anywhere for new measurement\n"
                                       " \u2022 Drag ruler segment to measure an angle\n"
                                       " \u2022 {:s} to remove the active ruler\n"
                                       " \u2022 Ctrl while dragging to snap\n"
                                       " \u2022 Shift while dragging to measure surface "
                                       "thickness")),
                     kmi_to_string_or_none(kmi_add),
                     kmi_to_string_or_none(kmi_remove));
}

/* La regla no tiene ajustes: todo lo que se toca de ella se toca sobre la vista, con su
 * grupo de gizmos. */
const ToolDecl ruler = {
    /*idname*/ "builtin.measure",
    /*label*/ N_("Measure"),
    /*description*/ nullptr,
    /*description_fn*/ description_ruler,
    /*icon*/ "ops.view3d.ruler",
    /*cursor*/ nullptr,
    /*gizmo_group*/ "VIEW3D_GGT_ruler",
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Measure",
};

}  // namespace defs_view3d_generic

/** \} */

/* -------------------------------------------------------------------- */
/** \name `_defs_view3d_select`
 * \{ */

namespace defs_view3d_select {

const ToolDecl select = {
    /*idname*/ "builtin.select",
    /*label*/ N_("Tweak"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.generic.select",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Tweak",
};

/* Las tres restantes pintan el mismo control, el modo de seleccion del operador, sin
 * etiqueta, expandido y solo con iconos. En el Python son tres `draw_settings` copiadas
 * palabra por palabra; aqui es la misma fila con distinto operador. */
static const PropRow box_settings[] = {
    {PropSource::Operator,
     "view3d.select_box",
     "mode",
     nullptr,
     PROP_ROW_EXPAND | PROP_ROW_NO_TEXT | PROP_ROW_ICON_ONLY},
};

const ToolDecl box = {
    /*idname*/ "builtin.select_box",
    /*label*/ N_("Select Box"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.generic.select_box",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Select Box",
    /*keymap_fallback*/ nullptr,
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(box_settings),
};

static const PropRow lasso_settings[] = {
    {PropSource::Operator,
     "view3d.select_lasso",
     "mode",
     nullptr,
     PROP_ROW_EXPAND | PROP_ROW_NO_TEXT | PROP_ROW_ICON_ONLY},
};

const ToolDecl lasso = {
    /*idname*/ "builtin.select_lasso",
    /*label*/ N_("Select Lasso"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.generic.select_lasso",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Select Lasso",
    /*keymap_fallback*/ nullptr,
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(lasso_settings),
};

static const PropRow circle_settings[] = {
    {PropSource::Operator,
     "view3d.select_circle",
     "mode",
     nullptr,
     PROP_ROW_EXPAND | PROP_ROW_NO_TEXT | PROP_ROW_ICON_ONLY},
    {PropSource::Operator, "view3d.select_circle", "radius"},
};

/* El circulo pinta ademas su radio sobre la vista mientras esta activo, igual que el del
 * editor de nodos. El dibujo va en la fase de dibujado, con el resto de `draw_cursor`. */
const ToolDecl circle = {
    /*idname*/ "builtin.select_circle",
    /*label*/ N_("Select Circle"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "ops.generic.select_circle",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ "3D View Tool: Select Circle",
    /*keymap_fallback*/ nullptr,
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_NONE,
    /*settings*/ span(circle_settings),
};

}  // namespace defs_view3d_select

/** \} */

}  // namespace flipendo::toolsystem
