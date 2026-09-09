/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Piezas comunes de las descripciones calculadas. Ver fl_tool_description.hh.
 */

#include "BKE_context.hh"

#include "DNA_windowmanager_types.h"

#include "RNA_types.hh"
#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_keymap.hh"

#include "fl_tool_description.hh"

namespace flipendo::toolsystem {

std::string kmi_to_string_or_none(const wmKeyMapItem *kmi)
{
  if (kmi == nullptr) {
    return "<none>";
  }
  return WM_keymap_item_to_string(kmi, false).value_or("");
}

const wmKeyMapItem *modal_kmi_from_identifier(const wmKeyMap *km, const char *identifier)
{
  if (km == nullptr || km->modal_items == nullptr) {
    return nullptr;
  }
  const EnumPropertyItem *items = static_cast<const EnumPropertyItem *>(km->modal_items);
  int propvalue = 0;
  if (!RNA_enum_value_from_id(items, identifier, &propvalue)) {
    return nullptr;
  }
  return WM_modalkeymap_find_propvalue(km, propvalue);
}

const wmKeyMap *user_modal_keymap(const bContext *C, const char *idname)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  if (wm == nullptr) {
    return nullptr;
  }
  /* La configuracion del USUARIO, que es la que consulta el Python
   * (`context.window_manager.keyconfigs.user`). Es donde acaban los cambios de teclas,
   * asi que preguntar a la de por defecto ensenaria un atajo que la persona ya no
   * tiene. Los modales no llevan espacio ni region. */
  return WM_keymap_find_all(wm, idname, 0, 0);
}

}  // namespace flipendo::toolsystem
