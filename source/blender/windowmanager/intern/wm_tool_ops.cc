/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Operadores para fijar la herramienta activa: `wm.tool_set_by_id`,
 * `wm.tool_set_by_index` y `wm.tool_set_by_brush_type`.
 *
 * Transliteracion de `scripts/startup/bl_operators/wm.py:2005-2173`, que se borra. Son
 * los que nombran los keymaps (148 atajos solo en el mapa por defecto) y los que pinta
 * cada boton de la barra de herramientas, asi que conservan identificador, propiedades,
 * valores por defecto y banderas: un `.blend` o un keymap guardado con ellos tiene que
 * seguir funcionando igual.
 *
 * La logica no vive aqui sino en el catalogo (`toolsystem/fl_toolsystem_activate.cc`);
 * estos operadores solo leen sus propiedades, deciden el espacio e informan.
 */

#include <string>

#include "MEM_guardedalloc.h"

#include "BKE_context.hh"
#include "BKE_report.hh"

#include "BLT_translation.hh"

#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "toolsystem/FL_toolsystem.hpp"

namespace ts = flipendo::toolsystem;

/* -------------------------------------------------------------------- */
/** \name Piezas comunes
 * \{ */

/** `rna_space_type_prop` del Python: los tipos de espacio de RNA, por defecto 'EMPTY'. */
static void def_space_type(wmOperatorType *ot)
{
  RNA_def_enum(ot->srna, "space_type", rna_enum_space_type_items, SPACE_EMPTY, "Type", "");
}

static void def_cycle(wmOperatorType *ot)
{
  PropertyRNA *prop = RNA_def_boolean(
      ot->srna, "cycle", false, "Cycle", "Cycle through tools in this group");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}

static void def_as_fallback(wmOperatorType *ot, const char *description)
{
  PropertyRNA *prop = RNA_def_boolean(ot->srna, "as_fallback", false, "Set Fallback", description);
  RNA_def_property_flag(prop, PropertyFlag(PROP_SKIP_SAVE | PROP_HIDDEN));
}

/**
 * `space_type_from_operator`: el espacio de la propiedad si se ha fijado, y si no el del
 * contexto. Los keymaps no lo fijan (vale el espacio donde se pulsa); el motor si, al
 * reactivar herramientas de un espacio que no es el actual.
 */
static bool space_type_from_operator(bContext *C, wmOperator *op, int *r_space_type)
{
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "space_type");
  if (RNA_property_is_set(op->ptr, prop)) {
    *r_space_type = RNA_property_enum_get(op->ptr, prop);
    return true;
  }
  const SpaceLink *sl = CTX_wm_space_data(C);
  if (sl == nullptr) {
    BKE_report(op->reports, RPT_WARNING, RPT_("Tool cannot be set with an empty space"));
    return false;
  }
  *r_space_type = sl->spacetype;
  return true;
}

static const char *space_type_identifier(const int space_type)
{
  const char *identifier = "";
  RNA_enum_identifier(rna_enum_space_type_items, space_type, &identifier);
  return identifier;
}

/**
 * Tras fijar una herramienta de reserva, el Python pone
 * `tool_settings.workspace_tool_type = 'FALLBACK'`. Se hace por RNA, como alli, para que
 * corra la misma actualizacion.
 */
static void workspace_tool_type_set_fallback(bContext *C)
{
  Scene *scene = CTX_data_scene(C);
  if (scene == nullptr || scene->toolsettings == nullptr) {
    return;
  }
  PointerRNA ptr = RNA_pointer_create_discrete(&scene->id, &RNA_ToolSettings, scene->toolsettings);
  PropertyRNA *prop = RNA_struct_find_property(&ptr, "workspace_tool_type");
  int value = 0;
  if (prop != nullptr && RNA_property_enum_value(C, &ptr, prop, "FALLBACK", &value)) {
    RNA_property_enum_set(&ptr, prop, value);
    RNA_property_update(C, &ptr, prop);
  }
}

/**
 * Una propiedad de cadena, entera. Las de estos operadores no tienen limite de longitud,
 * asi que un bufer fijo desbordaria con un nombre largo llegado de un keymap o de un
 * script: se usa el fijo si cabe y memoria reservada si no.
 */
static std::string string_get(PointerRNA *ptr, const char *name)
{
  char fixed[128];
  int len = 0;
  char *value = RNA_string_get_alloc(ptr, name, fixed, sizeof(fixed), &len);
  std::string out(value, len);
  if (value != fixed) {
    MEM_freeN(value);
  }
  return out;
}

/** Activa por nombre, ciclando o no. Comun a `tool_set_by_id` y `tool_set_by_index`. */
static bool activate(bContext *C,
                     const int space_type,
                     const char *idname,
                     const bool cycle,
                     const bool as_fallback)
{
  return cycle ? ts::activate_by_id_or_cycle(C, space_type, idname, 1, as_fallback) :
                 ts::activate_by_id(C, space_type, idname, as_fallback);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Por nombre
 * \{ */

static wmOperatorStatus tool_set_by_id_exec(bContext *C, wmOperator *op)
{
  int space_type;
  if (!space_type_from_operator(C, op, &space_type)) {
    return OPERATOR_CANCELLED;
  }
  const std::string name = string_get(op->ptr, "name");
  const bool as_fallback = RNA_boolean_get(op->ptr, "as_fallback");

  if (activate(C, space_type, name.c_str(), RNA_boolean_get(op->ptr, "cycle"), as_fallback)) {
    if (as_fallback) {
      workspace_tool_type_set_fallback(C);
    }
    return OPERATOR_FINISHED;
  }
  BKE_reportf(op->reports,
              RPT_WARNING,
              RPT_("Tool '%s' not found for space '%s'"),
              name.c_str(),
              space_type_identifier(space_type));
  return OPERATOR_CANCELLED;
}

void WM_OT_tool_set_by_id(wmOperatorType *ot)
{
  ot->name = "Set Tool by Name";
  ot->idname = "WM_OT_tool_set_by_id";
  ot->description = "Set the tool by name (for key-maps)";

  ot->exec = tool_set_by_id_exec;
  /* Lo que tiene por defecto un operador de Python sin `bl_options`. */
  ot->flag = OPTYPE_REGISTER;

  RNA_def_string(ot->srna, "name", nullptr, 0, "Identifier", "Identifier of the tool");
  def_cycle(ot);
  def_as_fallback(ot, "Set the fallback tool instead of the primary tool");
  def_space_type(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Por posicion
 * \{ */

static wmOperatorStatus tool_set_by_index_exec(bContext *C, wmOperator *op)
{
  int space_type;
  if (!space_type_from_operator(C, op, &space_type)) {
    return OPERATOR_CANCELLED;
  }
  const int index = RNA_int_get(op->ptr, "index");
  /* `expand`: la posicion cuenta cada herramienta de cada grupo. Sin el, cuenta botones
   * de la barra y un grupo es su variante recordada. */
  const ts::ToolDecl *item = RNA_boolean_get(op->ptr, "expand") ?
                                 ts::tool_find_by_index(C, space_type, index) :
                                 ts::tool_find_by_index_active(C, space_type, index);
  if (item == nullptr) {
    /* Sin aviso: el numero de herramientas cambia con el modo y el contexto. */
    return OPERATOR_CANCELLED;
  }

  const bool as_fallback = RNA_boolean_get(op->ptr, "as_fallback");
  if (activate(C, space_type, item->idname, RNA_boolean_get(op->ptr, "cycle"), as_fallback)) {
    if (as_fallback) {
      workspace_tool_type_set_fallback(C);
    }
    return OPERATOR_FINISHED;
  }
  /* La herramienta acaba de salir del propio catalogo, asi que no deberia pasar. El
   * Python lanza una excepcion aqui. */
  BKE_reportf(op->reports, RPT_ERROR, "Internal error setting tool '%s'", item->idname);
  return OPERATOR_CANCELLED;
}

void WM_OT_tool_set_by_index(wmOperatorType *ot)
{
  ot->name = "Set Tool by Index";
  ot->idname = "WM_OT_tool_set_by_index";
  ot->description = "Set the tool by index (for key-maps)";

  ot->exec = tool_set_by_index_exec;
  ot->flag = OPTYPE_REGISTER;

  RNA_def_int(ot->srna, "index", 0, INT_MIN, INT_MAX, "Index in Toolbar", "", INT_MIN, INT_MAX);
  def_cycle(ot);
  PropertyRNA *prop = RNA_def_boolean(ot->srna, "expand", true, "", "Include tool subgroups");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  def_as_fallback(ot, "Set the fallback tool instead of the primary");
  def_space_type(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Por tipo de pincel
 * \{ */

static wmOperatorStatus tool_set_by_brush_type_exec(bContext *C, wmOperator *op)
{
  int space_type;
  if (!space_type_from_operator(C, op, &space_type)) {
    return OPERATOR_CANCELLED;
  }
  const std::string brush_type = string_get(op->ptr, "brush_type");
  const char *tool_id = nullptr;
  if (ts::activate_by_brush_type(C, space_type, brush_type.c_str(), &tool_id)) {
    return OPERATOR_FINISHED;
  }
  BKE_reportf(op->reports,
              RPT_WARNING,
              RPT_("Tool '%s' not found for space '%s'"),
              tool_id != nullptr ? tool_id : "",
              space_type_identifier(space_type));
  return OPERATOR_CANCELLED;
}

void WM_OT_tool_set_by_brush_type(wmOperatorType *ot)
{
  ot->name = "Set Tool by Brush Type";
  ot->idname = "WM_OT_tool_set_by_brush_type";
  ot->description =
      "Look up the most appropriate tool for the given brush type and activate that";

  ot->exec = tool_set_by_brush_type_exec;
  ot->flag = OPTYPE_REGISTER;

  RNA_def_string(ot->srna,
                 "brush_type",
                 nullptr,
                 0,
                 "Brush Type",
                 "Brush type identifier for which the most appropriate tool will be looked up");
  def_space_type(ot);
}

/** \} */
