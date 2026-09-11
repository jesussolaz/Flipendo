/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Dialogo nativo para los ficheros .blend soltados sobre una ventana.
 */

#include "BLI_path_utils.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "UI_interface.hh"
#include "UI_interface_icons.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

namespace {

wmOperatorStatus drop_blend_file_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  char filepath[FILE_MAX];
  RNA_string_get(op->ptr, "filepath", filepath);

  uiPopupMenu *popup = UI_popup_menu_begin(C, BLI_path_basename(filepath), ICON_QUESTION);
  uiLayout *layout = UI_popup_menu_layout(popup);

  uiLayout &open_column = layout->column(false);
  PointerRNA props = open_column.op(
      "WM_OT_open_mainfile", "Open", ICON_FILE_FOLDER, WM_OP_INVOKE_DEFAULT, UI_ITEM_NONE);
  RNA_string_set(&props, "filepath", filepath);
  RNA_boolean_set(&props, "display_file_selector", false);

  layout->separator();
  uiLayout &library_column = layout->column(false);
  props = library_column.op(
      "WM_OT_link", "Link...", ICON_LINK_BLEND, WM_OP_INVOKE_DEFAULT, UI_ITEM_NONE);
  RNA_string_set(&props, "filepath", filepath);
  props = library_column.op(
      "WM_OT_append", "Append...", ICON_APPEND_BLEND, WM_OP_INVOKE_DEFAULT, UI_ITEM_NONE);
  RNA_string_set(&props, "filepath", filepath);

  UI_popup_menu_end(C, popup);
  /* Mantiene el contrato del operador Python: el dialogo queda abierto, pero la
   * invocacion ya ha terminado. */
  return OPERATOR_FINISHED;
}

}  // namespace

void WM_OT_drop_blend_file(wmOperatorType *ot)
{
  ot->name = "Handle dropped .blend file";
  ot->idname = "WM_OT_drop_blend_file";
  ot->invoke = drop_blend_file_invoke;
  ot->flag = OPTYPE_INTERNAL;

  PropertyRNA *prop = RNA_def_string_file_path(
      ot->srna, "filepath", nullptr, 0, "filepath", "");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}
