/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Operadores de la barra de herramientas:
 * - `wm.toolbar`: la barra como popup, con una tecla para cada herramienta.
 * - `wm.toolbar_fallback_pie`: la tarta para elegir la herramienta de reserva.
 * - `wm.toolbar_prompt`: tecla de prefijo; se pulsa y luego la tecla de la herramienta.
 *
 * Transliteracion de `scripts/startup/bl_operators/wm.py`, que se borra. El keymap que
 * usan los tres lo fabrica `flipendo::toolsystem::toolbar_keymap_generate`, verificado
 * 912/912 contra el Python.
 */

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>

#include "MEM_guardedalloc.h"

#include "BKE_context.hh"
#include "BKE_report.hh"

#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "BLT_translation.hh"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "ED_screen.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "UI_interface.hh"
#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_keymap.hh"
#include "WM_types.hh"
#include "wm_event_types.hh"

#include "FL_toolbar_ui.hh"
#include "toolsystem/FL_toolsystem.hpp"

namespace ts = flipendo::toolsystem;

static bool space_data_poll(bContext *C)
{
  /* `context.space_data is not None`. */
  return CTX_wm_space_data(C) != nullptr;
}

/* -------------------------------------------------------------------- */
/** \name `wm.toolbar`
 * \{ */

static wmOperatorStatus toolbar_exec(bContext *C, wmOperator *op)
{
  const int space_type = CTX_wm_space_data(C)->spacetype;
  /* `keymap_from_toolbar`: si el espacio no tiene barra, no hay nada que abrir. */
  if (ts::toolbar_for_space(space_type) == nullptr) {
    return OPERATOR_CANCELLED;
  }
  wmKeyMap *keymap = ts::toolbar_keymap_generate(C, space_type, true, true);
  if (keymap == nullptr) {
    return OPERATOR_CANCELLED;
  }
  /* `wm.popover` exige ventana (`rna_popup_context_ok_or_report`). */
  if (CTX_wm_window(C) == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "context \"window\" is None");
    return OPERATOR_CANCELLED;
  }
  uiPopover *pup = UI_popover_begin(C, U.widget_unit * 8, false);
  uiLayout *layout = UI_popover_layout(pup);
  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);
  flipendo::ui::toolbar_draw(C, layout, false, 1.0f);
  UI_popover_end(C, pup, keymap);
  return OPERATOR_FINISHED;
}

void WM_OT_toolbar(wmOperatorType *ot)
{
  ot->name = "Toolbar";
  ot->idname = "WM_OT_toolbar";
  /* La clase de Python no tenia docstring. */
  ot->description = "";

  ot->exec = toolbar_exec;
  ot->poll = space_data_poll;
  /* Lo que tiene por defecto un operador de Python sin `bl_options`. */
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name `wm.toolbar_fallback_pie`
 * \{ */

static wmOperatorStatus toolbar_fallback_pie_invoke(bContext *C,
                                                    wmOperator *op,
                                                    const wmEvent *event)
{
  const int space_type = CTX_wm_space_data(C)->spacetype;
  const ts::ToolbarDecl *toolbar = ts::toolbar_for_space(space_type);
  if (toolbar == nullptr) {
    return OPERATOR_PASS_THROUGH;
  }
  /* "It's possible we don't have the fallback tool available": en el editor de imagen,
   * por ejemplo, en modos de pintura sin seleccion. */
  if (ts::tool_find_by_id(C, space_type, toolbar->tool_fallback_id) == nullptr) {
    printf("Tool %s not active in space %d\n", toolbar->tool_fallback_id, space_type);
    return OPERATOR_PASS_THROUGH;
  }
  /* `wm.popup_menu_pie` exige ventana (`rna_popup_context_ok_or_report`). */
  if (CTX_wm_window(C) == nullptr) {
    BKE_report(op->reports, RPT_ERROR, "context \"window\" is None");
    return OPERATOR_CANCELLED;
  }
  uiPieMenu *pie = UI_pie_menu_begin(C, IFACE_("Fallback Tool"), ICON_NONE, event);
  if (pie != nullptr) {
    flipendo::ui::tool_fallback_items_draw(C, UI_pie_menu_layout(pie), true);
    UI_pie_menu_end(C, pie);
  }
  return OPERATOR_FINISHED;
}

void WM_OT_toolbar_fallback_pie(wmOperatorType *ot)
{
  ot->name = "Fallback Tool Pie Menu";
  ot->idname = "WM_OT_toolbar_fallback_pie";
  ot->description = "";

  ot->invoke = toolbar_fallback_pie_invoke;
  ot->poll = space_data_poll;
  ot->flag = OPTYPE_REGISTER;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name `wm.toolbar_prompt`
 * \{ */

struct ToolbarPromptData {
  /** El keymap temporal de la barra. Se rehace en cada apertura, igual que en el Python,
   * que tambien lo guardaba entre llamadas del modal. */
  wmKeyMap *keymap = nullptr;
  short init_event_type = 0;
};

/** `str.title()` del Python: mayuscula al principio de cada palabra y minusculas en el
 * resto, siendo frontera cualquier caracter que no sea letra. */
static std::string python_title(const char *text)
{
  std::string out;
  bool prev_cased = false;
  for (const char *c = text; *c != '\0'; c++) {
    const unsigned char ch = uchar(*c);
    if (std::isalpha(ch)) {
      out += char(prev_cased ? std::tolower(ch) : std::toupper(ch));
      prev_cased = true;
    }
    else {
      out += char(ch);
      prev_cased = false;
    }
  }
  return out;
}

/** El nombre de la tecla inicial: 'LEFT_ALT' -> "Alt". "Left Alt" no aporta nada. */
static std::string init_event_text(const short type)
{
  const char *identifier = nullptr;
  if (!RNA_enum_identifier(rna_enum_event_type_items, type, &identifier) || identifier == nullptr) {
    return "";
  }
  const std::string title = python_title(identifier);
  blender::Vector<std::string> parts;
  size_t start = 0;
  while (true) {
    const size_t pos = title.find('_', start);
    parts.append(title.substr(start, pos == std::string::npos ? std::string::npos : pos - start));
    if (pos == std::string::npos) {
      break;
    }
    start = pos + 1;
  }
  if (!parts.is_empty() && (parts[0] == "Left" || parts[0] == "Right")) {
    parts.remove(0);
  }
  std::string out;
  for (const int64_t i : parts.index_range()) {
    out += (i == 0 ? "" : " ") + parts[i];
  }
  return out;
}

static std::string kmi_tool_name(const wmKeyMapItem *kmi)
{
  if (kmi->ptr == nullptr) {
    return "";
  }
  char *value = RNA_string_get_alloc(kmi->ptr, "name", nullptr, 0, nullptr);
  std::string out = value;
  MEM_freeN(value);
  return out;
}

/**
 * `_status_items_generate` y `status_text_fn`: las teclas del popup en la barra de estado,
 * en el orden de la barra de herramientas.
 *
 * DIFERENCIA DELIBERADA de aspecto. El Python lo pintaba SUSTITUYENDO el metodo de dibujo
 * de la barra de estado (`STATUSBAR_HT_header.draw`) por una funcion propia, con una caja
 * para la tecla inicial y una rejilla de atajos. Esa tecnica no tiene equivalente en C++:
 * lo idiomatico es `WorkspaceStatus`, que es lo que usan todos los modales nativos. El
 * contenido es el mismo (la tecla inicial y, para cada herramienta, sus modificadores, su
 * tecla y su nombre); cambian la caja y el espaciado.
 */
static void toolbar_prompt_status(bContext *C, const int space_type, const ToolbarPromptData &data)
{
  blender::Map<std::string, std::pair<int, const char *>> tools;
  for (const ts::ToolDecl *tool : ts::tools_for_context(C, space_type)) {
    tools.add(tool->idname, {int(tools.size()), tool->label});
  }

  struct StatusItem {
    int order;
    std::string name;
    const wmKeyMapItem *kmi;
  };
  blender::Vector<StatusItem> items;
  LISTBASE_FOREACH (const wmKeyMapItem *, kmi, &data.keymap->items) {
    /* Repiten los numeros normales. */
    const std::string key_str = WM_keymap_item_to_string(kmi, false).value_or("");
    if (key_str.rfind("Numpad ", 0) == 0) {
      continue;
    }
    if (!STREQ(kmi->idname, "WM_OT_tool_set_by_id")) {
      continue;
    }
    const std::pair<int, const char *> *tool = tools.lookup_ptr(kmi_tool_name(kmi));
    if (tool == nullptr) {
      continue;
    }
    std::string name = tool->second;
    const size_t pos = name.find("Annotate ");
    if (pos != std::string::npos) {
      name.erase(pos, strlen("Annotate "));
    }
    items.append({tool->first, name, kmi});
  }
  std::stable_sort(items.begin(), items.end(), [](const StatusItem &a, const StatusItem &b) {
    return a.order < b.order;
  });

  WorkspaceStatus status(C);
  status.item(init_event_text(data.init_event_type), ICON_NONE);
  for (const StatusItem &item : items) {
    int icon_mod[KM_MOD_NUM] = {0};
    const int icon = UI_icon_from_keymap_item(item.kmi, icon_mod);
    const std::string text = CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, item.name.c_str());
    if (icon == 0) {
      /* Sin icono para la tecla: su nombre, como `text_fallback` de la plantilla. */
      status.item(std::string(WM_key_event_string(item.kmi->type, true)) + " " + text, ICON_NONE);
      continue;
    }
    blender::Vector<int> icons;
    for (int j = 0; j < KM_MOD_NUM && icon_mod[j] != 0; j++) {
      icons.append(icon_mod[j]);
    }
    icons.append(icon);
    /* `item()` admite dos iconos; con mas modificadores se encadenan sin texto. */
    while (icons.size() > 2) {
      status.item("", icons[0], icons[1]);
      icons.remove(0);
      icons.remove(0);
    }
    status.item(text, icons[0], icons.size() > 1 ? icons[1] : 0);
  }
}

static void toolbar_prompt_end(bContext *C, wmOperator *op)
{
  ED_workspace_status_text(C, nullptr);
  MEM_delete(static_cast<ToolbarPromptData *>(op->customdata));
  op->customdata = nullptr;
}

static wmOperatorStatus toolbar_prompt_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  SpaceLink *sl = CTX_wm_space_data(C);
  if (sl == nullptr) {
    return OPERATOR_CANCELLED;
  }
  const int space_type = sl->spacetype;
  if (ts::toolbar_for_space(space_type) == nullptr) {
    return OPERATOR_CANCELLED;
  }
  /* Sin numeros de reserva ni reinicio por doble pulsacion: aqui solo valen las teclas de
   * verdad. */
  wmKeyMap *keymap = ts::toolbar_keymap_generate(C, space_type, false, false);
  if (keymap == nullptr || BLI_listbase_is_empty(&keymap->items)) {
    return OPERATOR_CANCELLED;
  }
  ToolbarPromptData *data = MEM_new<ToolbarPromptData>(__func__);
  data->keymap = keymap;
  data->init_event_type = event->type;
  op->customdata = data;

  toolbar_prompt_status(C, space_type, *data);
  WM_event_add_modal_handler(C, op);
  return OPERATOR_RUNNING_MODAL;
}

static wmOperatorStatus toolbar_prompt_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
  ToolbarPromptData *data = static_cast<ToolbarPromptData *>(op->customdata);

  switch (event->type) {
    case LEFTMOUSE:
    case RIGHTMOUSE:
    case MIDDLEMOUSE:
    case WHEELDOWNMOUSE:
    case WHEELUPMOUSE:
    case WHEELINMOUSE:
    case WHEELOUTMOUSE:
    case EVT_ESCKEY:
      toolbar_prompt_end(C, op);
      return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
    default:
      break;
  }

  if (const wmKeyMapItem *kmi = WM_event_match_keymap_item(C, data->keymap, event)) {
    if (STREQ(kmi->idname, "WM_OT_tool_set_by_id")) {
      /* `bpy.ops.wm.tool_set_by_id(name=...)`. */
      wmOperatorType *ot = WM_operatortype_find("WM_OT_tool_set_by_id", false);
      if (ot != nullptr) {
        PointerRNA props;
        WM_operator_properties_create_ptr(&props, ot);
        RNA_string_set(&props, "name", kmi_tool_name(kmi).c_str());
        WM_operator_name_call_ptr(C, ot, WM_OP_EXEC_DEFAULT, &props, nullptr);
        WM_operator_properties_free(&props);
      }
    }
    toolbar_prompt_end(C, op);
    return OPERATOR_FINISHED;
  }

  /* Volver a pulsar la tecla inicial, sin modificadores, tambien sale. */
  if (event->type == data->init_event_type && event->val == KM_RELEASE &&
      (event->modifier & (KM_CTRL | KM_ALT | KM_SHIFT | KM_OSKEY | KM_HYPER)) == 0)
  {
    toolbar_prompt_end(C, op);
    return OPERATOR_CANCELLED;
  }
  return OPERATOR_RUNNING_MODAL;
}

static void toolbar_prompt_cancel(bContext *C, wmOperator *op)
{
  toolbar_prompt_end(C, op);
}

void WM_OT_toolbar_prompt(wmOperatorType *ot)
{
  ot->name = "Toolbar Prompt";
  ot->idname = "WM_OT_toolbar_prompt";
  ot->description = "Leader key like functionality for accessing tools";

  ot->invoke = toolbar_prompt_invoke;
  ot->modal = toolbar_prompt_modal;
  ot->cancel = toolbar_prompt_cancel;
  ot->flag = OPTYPE_REGISTER;
}

/** \} */
