/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Equivalencias C++ de las llamadas de interfaz que usan los `draw_settings` del Python.
 *
 * Existe para que transliterar un `draw_settings` sea mecanico y FIEL. Cada funcion de
 * aqui hace lo mismo que su gemela de `bpy.types.UILayout`, traduccion incluida, porque
 * reproduce lo que hace `rna_ui_api.cc` al recibir la llamada desde Python. Sin esta
 * capa, cada transliteracion reimplementaria a su manera `layout.prop(text=...)` y
 * saldrian variantes sutilmente distintas; la diferencia solo se veria en otro idioma.
 *
 *     Python:  props = tool.operator_properties("mesh.bevel")
 *              layout.prop(props, "segments", text="Segs")
 *     C++:     PointerRNA props = op_props(tref, "mesh.bevel");
 *              prop(layout, &props, "segments", Prop().text("Segs"));
 *
 * Los textos se pasan tal cual estan en el Python, SIN `IFACE_()`: la traduccion la hace
 * cada funcion, igual que la hacia RNA.
 */

#pragma once

#include "RNA_types.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

struct bContext;
struct bToolRef;

namespace flipendo::ui::settings {

/* -------------------------------------------------------------------- */
/** \name Fuentes de datos
 * \{ */

/** `tool.operator_properties(op_idname)`. Acepta `"mesh.bevel"` o `"MESH_OT_bevel"`. */
PointerRNA op_props(bToolRef *tref, const char *op_idname);
/** `tool.gizmo_group_properties(group)`. */
PointerRNA gizmo_props(bToolRef *tref, const char *group);
/** `context.tool_settings`. */
PointerRNA tool_settings(const bContext *C);
/** `context.scene`. */
PointerRNA scene(const bContext *C);
/** `context.preferences.edit`. */
PointerRNA preferences_edit();
/** Cualquier miembro puntero del contexto: `context.annotation_data`, `context.brush`... */
PointerRNA context_pointer(const bContext *C, const char *member);
/** `getattr(ptr, name)` para un puntero: `tool_settings.uv_sculpt`. Nulo si no hay. */
PointerRNA pointer_get(PointerRNA *ptr, const char *name);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Contexto
 * \{ */

/** `context.region.type == 'TOOL_HEADER'`: la bifurcacion mas comun de los ajustes. */
bool region_is_tool_header(const bContext *C);
/** `context.space_data.type`, como `SPACE_*`, o -1. */
int space_type(const bContext *C);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Elementos
 * \{ */

/** Los argumentos opcionales de `layout.prop(...)`, en cadena. */
struct Prop {
  /** `text=`. `nullptr` es `None` (la etiqueta de la propiedad); `""` es sin etiqueta. */
  Prop &text(const char *value)
  {
    text_ = value;
    return *this;
  }
  Prop &text_ctxt(const char *value)
  {
    text_ctxt_ = value;
    return *this;
  }
  Prop &expand(bool value = true)
  {
    expand_ = value;
    return *this;
  }
  Prop &slider(bool value = true)
  {
    slider_ = value;
    return *this;
  }
  /** `toggle=`: 1 si, 0 no, -1 (por defecto) sin decidir. */
  Prop &toggle(int value)
  {
    toggle_ = value;
    return *this;
  }
  Prop &icon_only(bool value = true)
  {
    icon_only_ = value;
    return *this;
  }
  /** `icon=` o `icon_value=`. */
  Prop &icon(int value)
  {
    icon_ = value;
    return *this;
  }
  Prop &emboss(bool value)
  {
    emboss_ = value;
    return *this;
  }
  Prop &index(int value)
  {
    index_ = value;
    return *this;
  }
  Prop &invert_checkbox(bool value = true)
  {
    invert_checkbox_ = value;
    return *this;
  }

  const char *text_ = nullptr;
  const char *text_ctxt_ = nullptr;
  bool expand_ = false;
  bool slider_ = false;
  int toggle_ = -1;
  bool icon_only_ = false;
  int icon_ = ICON_NONE;
  bool emboss_ = true;
  int index_ = -1;
  bool invert_checkbox_ = false;
};

/** `layout.prop(ptr, name, ...)`. */
void prop(uiLayout *layout, PointerRNA *ptr, const char *name, const Prop &args = Prop());

/** `layout.prop_enum(ptr, name, value, text=..., icon=...)`. */
void prop_enum(uiLayout *layout,
               PointerRNA *ptr,
               const char *name,
               const char *value,
               const char *text = nullptr,
               int icon = ICON_NONE);

/** `layout.label(text=..., icon=...)`. */
void label(uiLayout *layout, const char *text, int icon = ICON_NONE, const char *text_ctxt = nullptr);

/** `layout.operator(opname, text=..., icon=..., depress=...)`. Devuelve sus propiedades. */
PointerRNA op(uiLayout *layout,
              const char *opname,
              const char *text = nullptr,
              int icon = ICON_NONE,
              bool depress = false);

/** `layout.popover(panel=..., text=..., icon=...)`. */
void popover(uiLayout *layout,
             const bContext *C,
             const char *panel,
             const char *text = nullptr,
             int icon = ICON_NONE);

/** `layout.row(align=..., heading=...)`, con el encabezado traducido como en RNA. */
uiLayout &row(uiLayout *layout,
              bool align = false,
              const char *heading = nullptr,
              const char *heading_ctxt = nullptr);

/** `layout.column(align=..., heading=...)`. */
uiLayout &column(uiLayout *layout,
                 bool align = false,
                 const char *heading = nullptr,
                 const char *heading_ctxt = nullptr);

/** \} */

}  // namespace flipendo::ui::settings
