/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Activacion de herramientas.
 *
 * Transliteracion de `_activate_by_item`, `activate_by_id` y `activate_by_id_or_cycle`
 * (`scripts/startup/bl_ui/space_toolsystem_common.py:993-1133`) y de la busqueda de
 * `WM_OT_tool_set_by_brush_type` (`bl_operators/wm.py:2120`).
 *
 * Hasta ahora, para cambiar de herramienta el motor buscaba un operador de Python y lo
 * invocaba; el operador calculaba once argumentos y se los devolvia al motor por
 * `tool.setup(...)`, que ya era C++ (`WM_toolsystem_ref_set_from_runtime`). Aqui se
 * calculan esos once argumentos directamente.
 *
 * El calculo esta separado de la aplicacion a proposito. La parte pura
 * (`activation_compute`) es la que se verifica: el volcado la recorre para cada
 * herramienta de cada modo y se compara contra
 * `tests/flipendo/toolsystem/activation-python.txt`, que se saco interceptando
 * `setup()` en el Python real.
 */

#include <cstdio>
#include <string>

#include "BKE_context.hh"
#include "BKE_paint.hh"

#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"
#include "DNA_workspace_types.h"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "WM_api.hh"
#include "WM_toolsystem.hh"

#include "FL_toolsystem.hpp"

namespace flipendo::toolsystem {

/* -------------------------------------------------------------------- */
/** \name Consultas sobre la vista sin aplanar
 *
 * Una por cada ayuda del Python que la activacion usa. Se conservan separadas, y con
 * sus rarezas, porque de ellas sale el keymap de reserva: una sola que se "arreglara"
 * cambiaria que teclas funcionan con cada herramienta.
 * \{ */

/** `_tool_get_by_id`: la primera coincidencia, con su posicion DENTRO de su entrada
 * (0 si es suelta; el Python dice -1 en el comentario, pero el codigo da 0). */
static const ToolDecl *get_by_id(const blender::Span<ToolGroupView> entries,
                                 const char *idname,
                                 int *r_index)
{
  if (idname != nullptr) {
    for (const ToolGroupView &group : entries) {
      for (const int i : group.tools.index_range()) {
        if (STREQ(group.tools[i]->idname, idname)) {
          *r_index = i;
          return group.tools[i];
        }
      }
    }
  }
  *r_index = -1;
  return nullptr;
}

/** `_tool_group_active_get_from_item`: la variante recordada, o la primera si la
 * memoria se sale del grupo (el Python lo comprueba porque los grupos pueden cambiar). */
static int group_active_index(const int space_type, const ToolGroupView &group)
{
  const int index = group_active_get(space_type, group.tools[0]->idname);
  return (index < 0 || index >= group.tools.size()) ? 0 : index;
}

/** `_tool_get_active_by_index`: por posicion de ENTRADA, sin aplanar. Un grupo
 * devuelve su variante recordada; una suelta devuelve indice -1. */
static const ToolDecl *get_active_by_index(const int space_type,
                                           const blender::Span<ToolGroupView> entries,
                                           const int entry_index,
                                           int *r_index)
{
  if (entry_index < 0 || entry_index >= entries.size()) {
    *r_index = -1;
    return nullptr;
  }
  const ToolGroupView &group = entries[entry_index];
  if (group.is_group()) {
    const int index = group_active_index(space_type, group);
    *r_index = index;
    return group.tools[index];
  }
  *r_index = -1;
  return group.tools[0];
}

/** `_tool_get_by_id_active_with_group`: el grupo solo cuenta si la herramienta es su
 * LIDER. Si aparece suelta, no hay grupo. */
static const ToolGroupView *get_group_led_by(const blender::Span<ToolGroupView> entries,
                                             const char *idname)
{
  if (idname == nullptr) {
    return nullptr;
  }
  for (const ToolGroupView &group : entries) {
    if (STREQ(group.tools[0]->idname, idname)) {
      return group.is_group() ? &group : nullptr;
    }
  }
  return nullptr;
}

/** `_tool_group_active_set_by_id`: recuerda `idname` como variante del grupo que
 * contiene a `idname_group`, este donde este dentro de el. */
static void group_active_set_by_id(const int space_type,
                                   const blender::Span<ToolGroupView> entries,
                                   const char *idname_group,
                                   const char *idname)
{
  for (const ToolGroupView &group : entries) {
    for (const ToolDecl *tool : group.tools) {
      if (!STREQ(tool->idname, idname_group)) {
        continue;
      }
      for (const int i : group.tools.index_range()) {
        if (STREQ(group.tools[i]->idname, idname)) {
          group_active_set(space_type, group.tools[0]->idname, i);
          return;
        }
      }
      return;
    }
  }
}

blender::Vector<const ToolDecl *> fallback_group_tools(const bContext *C,
                                                       const ToolbarDecl &toolbar,
                                                       const char *mode)
{
  const blender::Vector<ToolGroupView> entries = tools_unexpanded_for_space_mode(
      C, toolbar, mode);
  const ToolGroupView *group = get_group_led_by(entries, toolbar.tool_fallback_id);
  blender::Vector<const ToolDecl *> out;
  if (group != nullptr) {
    out.extend(group->tools);
  }
  return out;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Calculo
 * \{ */

bool activation_compute(const bContext *C,
                        const ToolbarDecl &toolbar,
                        const char *mode,
                        const ToolDecl &item_in,
                        const int index_in,
                        const bool as_fallback,
                        const char *active_idname,
                        const char *stored_idname_fallback,
                        ActivationArgs &r_args)
{
  const int space_type = toolbar.space_type;
  const char *fallback_id = toolbar.tool_fallback_id;
  const blender::Vector<ToolGroupView> entries = tools_unexpanded_for_space_mode(
      C, toolbar, mode);

  const ToolDecl *item = &item_in;
  int index = index_in;

  if (as_fallback) {
    /* Se activa `item` como variante del grupo de reserva, y lo que se instala es la
     * herramienta que ya estaba activa, con la reserva nueva. */
    const ToolGroupView *group = get_group_led_by(entries, fallback_id);
    if (group == nullptr) {
      fprintf(stderr, "Herramientas: no existe la herramienta de reserva '%s'.\n", fallback_id);
      return false;
    }
    int index_new = -1;
    for (const int i : group->tools.index_range()) {
      if (STREQ(group->tools[i]->idname, item->idname)) {
        index_new = i;
        break;
      }
    }
    if (index_new == -1) {
      fprintf(stderr,
              "Herramientas: '%s' no esta en el grupo de reserva '%s'.\n",
              item->idname,
              fallback_id);
      return false;
    }
    group_active_set(space_type, fallback_id, index_new);

    int active_index = -1;
    const ToolDecl *active = get_by_id(entries, active_idname, &active_index);
    if (active == nullptr) {
      /* En el Python esto revienta un poco mas abajo, al leer `item.widget`. */
      fprintf(stderr,
              "Herramientas: la herramienta activa '%s' no esta en este modo.\n",
              active_idname != nullptr ? active_idname : "");
      return false;
    }
    item = active;
    index = active_index;
  }
  else if (stored_idname_fallback != nullptr && stored_idname_fallback[0] != '\0' &&
           fallback_id != nullptr)
  {
    /* La reserva elegida se guarda en DNA; se recupera antes de calcular el keymap de
     * reserva, aunque la herramienta de reserva no este en uso. */
    group_active_set_by_id(space_type, entries, fallback_id, stored_idname_fallback);
  }

  /* La herramienta de reserva. Rareza del Python conservada a proposito:
   * `select_index` es la posicion de la reserva DENTRO de su grupo, y se usa como
   * posicion de ENTRADA en la lista sin aplanar. Funciona porque el grupo de seleccion
   * es la primera entrada de todas las barras y la reserva es su lider (posicion 0 en
   * los dos sentidos). Donde la reserva no existe — el secuenciador en modo linea de
   * tiempo — no hay reserva. */
  const ToolDecl *item_fallback = nullptr;
  int select_index = -1;
  if (fallback_id != nullptr) {
    get_by_id(entries, fallback_id, &select_index);
  }
  if (select_index != -1) {
    int unused;
    item_fallback = get_active_by_index(space_type, entries, select_index, &unused);
  }

  r_args = ActivationArgs();
  r_args.tool = item;
  r_args.index = index;
  r_args.keymap = item->keymap_name != nullptr ? item->keymap_name : "";
  r_args.cursor = item->cursor != nullptr ? item->cursor : "DEFAULT";
  r_args.options = item->options & (TOOL_OPTION_KEYMAP_FALLBACK | TOOL_OPTION_USE_BRUSHES);
  r_args.gizmo_group = item->gizmo_group != nullptr ? item->gizmo_group : "";
  r_args.brush_type = item->brush_type != nullptr ? item->brush_type : "ANY";
  r_args.data_block = item->data_block != nullptr ? item->data_block : "";
  r_args.op = item->op != nullptr ? item->op : "";
  if (item_fallback != nullptr) {
    r_args.idname_fallback = item_fallback->idname;
    if (item_fallback->keymap_name != nullptr && item_fallback->keymap_name[0] != '\0') {
      r_args.keymap_fallback = std::string(item_fallback->keymap_name) + " (fallback)";
    }
  }
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Aplicacion
 * \{ */

/**
 * `_tool_active_from_context`: el `bToolRef` del espacio y modo actuales.
 *
 * Cada espacio saca su modo de un sitio distinto, igual que en el Python. Tras
 * encontrarlo se sincroniza con el contexto (`refresh_from_context`), que en los modos
 * de pincel puede cambiar la herramienta guardada.
 */
/** La clave (espacio, modo) del contexto. Separada de `active_tref` porque hay consultas
 * sobre el modo que no necesitan —ni deben esperar— que la herramienta activa exista ya. */
static bool tool_key_from_context(const bContext *C, const int space_type, bToolKey *r_key)
{
  *r_key = bToolKey{};
  r_key->space_type = space_type;
  switch (space_type) {
    case SPACE_VIEW3D:
      r_key->mode = CTX_data_mode_enum(C);
      return true;
    case SPACE_IMAGE: {
      const SpaceImage *sima = CTX_wm_space_image(C);
      r_key->mode = sima != nullptr ? sima->mode : SI_MODE_VIEW;
      return true;
    }
    case SPACE_NODE:
      r_key->mode = 0;
      return true;
    case SPACE_SEQ: {
      const SpaceSeq *sseq = CTX_wm_space_seq(C);
      if (sseq == nullptr) {
        return false;
      }
      r_key->mode = sseq->view;
      return true;
    }
  }
  return false;
}

static bToolRef *active_tref(const bContext *C,
                             WorkSpace *workspace,
                             const int space_type,
                             bool create)
{
  if (workspace == nullptr) {
    return nullptr;
  }
  bToolKey key{};
  if (!tool_key_from_context(C, space_type, &key)) {
    return nullptr;
  }

  bToolRef *tref = nullptr;
  if (create) {
    WM_toolsystem_ref_ensure(workspace, &key, &tref);
  }
  else {
    tref = WM_toolsystem_ref_find(workspace, &key);
  }
  if (tref != nullptr) {
    WM_toolsystem_ref_sync_from_context(CTX_data_main(C), workspace, tref);
  }
  return tref;
}

bToolRef *tool_active_ref(const bContext *C, const int space_type, const bool create)
{
  return active_tref(C, CTX_wm_workspace(C), space_type, create);
}

bToolRef *tool_ref_for_mode(const bContext *C,
                             const int space_type,
                             const char *mode,
                             const bool create)
{
  if (mode == nullptr) {
    return active_tref(C, CTX_wm_workspace(C), space_type, create);
  }
  WorkSpace *workspace = CTX_wm_workspace(C);
  if (workspace == nullptr) {
    return nullptr;
  }
  /* El Python pasa el modo como cadena y RNA lo convierte con la enumeracion de cada
   * espacio; aqui igual. */
  bToolKey key{};
  key.space_type = space_type;
  int value = 0;
  switch (space_type) {
    case SPACE_VIEW3D:
      if (!RNA_enum_value_from_id(rna_enum_context_mode_items, mode, &value)) {
        return nullptr;
      }
      break;
    case SPACE_IMAGE:
      if (!RNA_enum_value_from_id(rna_enum_space_image_mode_all_items, mode, &value)) {
        return nullptr;
      }
      break;
    case SPACE_SEQ:
      if (!RNA_enum_value_from_id(rna_enum_space_sequencer_view_type_items, mode, &value)) {
        return nullptr;
      }
      break;
    case SPACE_NODE:
      value = 0;
      break;
    default:
      return nullptr;
  }
  key.mode = value;
  bToolRef *tref = nullptr;
  if (create) {
    WM_toolsystem_ref_ensure(workspace, &key, &tref);
  }
  else {
    tref = WM_toolsystem_ref_find(workspace, &key);
  }
  if (tref != nullptr) {
    WM_toolsystem_ref_sync_from_context(CTX_data_main(C), workspace, tref);
  }
  return tref;
}

const ToolDecl *tool_find_by_id_active(const bContext *C,
                                       const ToolbarDecl &toolbar,
                                       const char *mode,
                                       const blender::StringRefNull idname)
{
  for (const ToolGroupView &group : tools_unexpanded_for_space_mode(C, toolbar, mode)) {
    if (group.is_group()) {
      if (idname == group.tools[0]->idname) {
        return group.tools[group_active_index(toolbar.space_type, group)];
      }
    }
    else if (idname == group.tools[0]->idname) {
      return group.tools[0];
    }
  }
  return nullptr;
}

/** El tipo de pincel en entero. Depende del modo de pintura de la herramienta: el
 * mismo nombre es otro numero en escultura que en pintura de vertices.
 *
 * Toma el espacio y el modo sueltos, no un `bToolRef`, porque hay una pregunta legitima
 * —"tiene este modo alguna herramienta de tal tipo de pincel"— que se hace ANTES de que
 * exista ninguna herramienta activa. `BKE_paintmode_get_from_tool` solo lee esos dos
 * campos, asi que el `bToolRef` de paso es un detalle de su firma, no un requisito. */
static int brush_type_value_in(const int space_type, const int mode, const char *identifier)
{
  if (STREQ(identifier, "ANY")) {
    return -1;
  }
  bToolRef key_only{};
  key_only.space_type = short(space_type);
  key_only.mode = mode;
  const PaintMode paint_mode = BKE_paintmode_get_from_tool(&key_only);
  if (paint_mode != PaintMode::Invalid) {
    const EnumPropertyItem *items = BKE_paint_get_tool_enum_from_paintmode(paint_mode);
    int value = -1;
    if (items != nullptr && RNA_enum_value_from_id(items, identifier, &value)) {
      return value;
    }
  }
  fprintf(stderr, "Herramientas: tipo de pincel '%s' desconocido en este modo.\n", identifier);
  return -1;
}

static int brush_type_value(const bToolRef *tref, const char *identifier)
{
  return brush_type_value_in(tref->space_type, tref->mode, identifier);
}

bool tool_with_brush_type_exists(const bContext *C, const int space_type, const int brush_type)
{
  /* El modo sale del CONTEXTO, no de la herramienta activa.
   *
   * La primera version pedia el `bToolRef` activo para sacar de el el modo de pintura, y
   * devolvia `false` si no lo habia. Parecia inofensivo —en el dibujo siempre hay
   * herramienta— y no lo era: en un modo al que se acaba de entrar y cuya barra aun no se
   * ha dibujado, `WM_toolsystem_ref_find` devuelve nulo, y entonces el estante escondia
   * TODOS los pinceles del modo. La prueba diferencial lo caza en los cuatro modos de
   * lapiz de grasa: 10 diferencias, todas "el nativo dice que no y el Python que si".
   * El Python nunca miro la herramienta activa; esto tampoco. */
  bToolKey key{};
  if (!tool_key_from_context(C, space_type, &key)) {
    return false;
  }
  const ToolbarDecl *toolbar = toolbar_for_space(space_type);
  if (toolbar == nullptr) {
    return false;
  }
  /* El modo con el que se busca es `context.mode`, TAMBIEN fuera de la vista 3D, y eso
   * se replica a proposito aunque parezca un error.
   *
   * El Python pregunta `cls.tools_from_context(context, mode=context.mode)`
   * (`properties_paint_common.py:34`), y `context.mode` es el modo del OBJETO
   * ('OBJECT', 'PAINT_TEXTURE'...). En el editor de imagen los modos del catalogo se
   * llaman de otra forma ('VIEW', 'UV', 'PAINT'), asi que alli no casa ninguno y la
   * busqueda solo ve las herramientas comunes del espacio —que no son de pincel—: el
   * estante de pinceles del editor de imagen NUNCA filtra por tipo. Buscar con el modo
   * del espacio, que es lo natural, devuelve `true` donde el Python devolvia `false` y
   * hace desaparecer del estante los cinco pinceles de pintura 2D. Es el mismo criterio
   * que ya se documento para el tipo de pincel en la fase 2. */
  const char *lookup_mode = nullptr;
  RNA_enum_identifier(rna_enum_context_mode_items, CTX_data_mode_enum(C), &lookup_mode);
  /* Usan el pincel pero no fijan su tipo: el Python las salta por idname. */
  static const char *ignored[] = {
      "builtin.arc",
      "builtin.curve",
      "builtin.line",
      "builtin.box",
      "builtin.circle",
      "builtin.polyline",
  };
  for (const ToolDecl *item : tools_for_space_mode(C, *toolbar, lookup_mode)) {
    if (item->brush_type == nullptr || (item->options & TOOL_OPTION_USE_BRUSHES) == 0) {
      continue;
    }
    bool skip = false;
    for (const char *idname : ignored) {
      if (STREQ(item->idname, idname)) {
        skip = true;
        break;
      }
    }
    if (skip) {
      continue;
    }
    if (brush_type_value_in(key.space_type, key.mode, item->brush_type) == brush_type) {
      return true;
    }
  }
  return false;
}

/* -------------------------------------------------------------------- */
/** \name Dibujo sobre la vista
 *
 * `_activate_by_item._cursor_draw_handle` del Python: un dibujo por tipo de espacio, que
 * se quita siempre al activar otra herramienta en ese espacio y se pone si la nueva lo
 * tiene. Sin condicion (`poll` nulo), como alli.
 *
 * Una diferencia a proposito: el Python captura el `tool` al activar y lo usa en cada
 * dibujo, y si ese `bToolRef` se libera el puntero queda colgando. Aqui se guarda su
 * CLAVE y en cada dibujo se busca la herramienta activa; si ya no es la misma, no se
 * dibuja. Se busca sin sincronizar con el contexto: esto corre en cada movimiento.
 * \{ */

struct CursorDraw {
  const ToolDecl *tool = nullptr;
  bToolKey key{};
};
static CursorDraw g_cursor_draw[SPACE_TYPE_NUM];
static wmPaintCursor *g_cursor_handle[SPACE_TYPE_NUM] = {nullptr};

static void tool_cursor_draw(bContext *C,
                             const blender::int2 &xy,
                             const blender::float2 & /*tilt*/,
                             void *customdata)
{
  const CursorDraw *draw = static_cast<const CursorDraw *>(customdata);
  WorkSpace *workspace = CTX_wm_workspace(C);
  if (draw->tool == nullptr || draw->tool->draw_cursor == nullptr || workspace == nullptr) {
    return;
  }
  bToolRef *tref = WM_toolsystem_ref_find(workspace, &draw->key);
  if (tref == nullptr || !STREQ(tref->idname, draw->tool->idname)) {
    return;
  }
  draw->tool->draw_cursor(C, tref, xy);
}

static void cursor_draw_update(const bToolRef *tref, const ToolDecl &tool)
{
  const int space_type = tref->space_type;
  if (space_type < 0 || space_type >= SPACE_TYPE_NUM) {
    return;
  }
  if (g_cursor_handle[space_type] != nullptr) {
    /* Busca el puntero en la lista del gestor antes de liberarlo: si ya no esta, no pasa
     * nada. */
    WM_paint_cursor_end(g_cursor_handle[space_type]);
    g_cursor_handle[space_type] = nullptr;
  }
  if (tool.draw_cursor == nullptr) {
    return;
  }
  g_cursor_draw[space_type].tool = &tool;
  g_cursor_draw[space_type].key.space_type = tref->space_type;
  g_cursor_draw[space_type].key.mode = tref->mode;
  g_cursor_handle[space_type] = WM_paint_cursor_activate(
      short(space_type), RGN_TYPE_WINDOW, nullptr, tool_cursor_draw, &g_cursor_draw[space_type]);
}

/** \} */

/** Lo que hace `rna_WorkSpaceTool_setup`, mas las propiedades iniciales del gizmo. */
static void activation_apply(bContext *C,
                             WorkSpace *workspace,
                             bToolRef *tref,
                             const ActivationArgs &args)
{
  bToolRef_Runtime tref_rt{};

  int cursor = 0;
  if (!RNA_enum_value_from_id(rna_enum_window_cursor_items, args.cursor, &cursor)) {
    fprintf(stderr, "Herramientas: cursor '%s' desconocido.\n", args.cursor);
  }
  tref_rt.cursor = cursor;
  STRNCPY(tref_rt.keymap, args.keymap);
  STRNCPY(tref_rt.gizmo_group, args.gizmo_group);
  STRNCPY(tref_rt.data_block, args.data_block);
  STRNCPY(tref_rt.op, args.op);
  tref_rt.brush_type = brush_type_value(tref, args.brush_type);
  tref_rt.index = args.index;
  tref_rt.flag = ((args.options & TOOL_OPTION_KEYMAP_FALLBACK) ? TOOLREF_FLAG_FALLBACK_KEYMAP : 0) |
                 ((args.options & TOOL_OPTION_USE_BRUSHES) ? TOOLREF_FLAG_USE_BRUSHES : 0);

  /* Igual que `setup`: la reserva se escribe en el `bToolRef` (DNA) para recordarla. */
  STRNCPY(tref->idname_fallback, args.idname_fallback);
  STRNCPY(tref_rt.keymap_fallback, args.keymap_fallback.c_str());

  WM_toolsystem_ref_set_from_runtime(C, workspace, tref, &tref_rt, args.tool->idname);

  /* El Python pide las propiedades del grupo de gizmos para TODA herramienta con gizmo,
   * lleve o no valores iniciales, y pedirlas las crea. Se hace igual. */
  if (args.gizmo_group[0] != '\0') {
    wmGizmoGroupType *gzgt = WM_gizmogrouptype_find(args.gizmo_group, false);
    if (gzgt == nullptr) {
      fprintf(stderr, "Herramientas: no existe el grupo de gizmos '%s'.\n", args.gizmo_group);
      return;
    }
    PointerRNA ptr;
    WM_toolsystem_ref_properties_ensure_from_gizmo_group(tref, gzgt, &ptr);
    for (const GizmoProp &gizmo_prop : args.tool->gizmo_properties) {
      PropertyRNA *prop = RNA_struct_find_property(&ptr, gizmo_prop.prop);
      if (prop == nullptr) {
        fprintf(stderr,
                "Herramientas: '%s' no tiene la propiedad '%s'.\n",
                args.gizmo_group,
                gizmo_prop.prop);
        continue;
      }
      switch (gizmo_prop.type) {
        case GizmoProp::Type::Number:
          RNA_property_float_set(&ptr, prop, gizmo_prop.number);
          break;
        case GizmoProp::Type::Boolean:
          RNA_property_boolean_set(&ptr, prop, gizmo_prop.boolean);
          break;
        case GizmoProp::Type::Integer:
          RNA_property_int_set(&ptr, prop, gizmo_prop.integer);
          break;
        case GizmoProp::Type::Enum: {
          int value;
          if (RNA_property_enum_value(C, &ptr, prop, gizmo_prop.enum_identifier, &value)) {
            RNA_property_enum_set(&ptr, prop, value);
          }
          break;
        }
      }
    }
  }

  cursor_draw_update(tref, *args.tool);
}

static bool activate_by_item(bContext *C,
                             const ToolbarDecl &toolbar,
                             const char *mode,
                             const ToolDecl &item,
                             const int index,
                             const bool as_fallback)
{
  WorkSpace *workspace = CTX_wm_workspace(C);
  bToolRef *tref = active_tref(C, workspace, toolbar.space_type, true);
  if (tref == nullptr) {
    return false;
  }
  ActivationArgs args;
  if (!activation_compute(
          C, toolbar, mode, item, index, as_fallback, tref->idname, tref->idname_fallback, args))
  {
    return false;
  }
  activation_apply(C, workspace, tref, args);

  /* Lo que en el Python hace el DIBUJO de la barra (`space_toolsystem_common.py:718`):
   * si la herramienta activa esta en un grupo, el grupo la recuerda como su variante.
   *
   * Hace falta aqui porque ese dibujo sigue siendo Python y escribe en SU diccionario, no
   * en esta memoria. Sin esto el ciclo lee una memoria que nadie actualiza: con Select
   * Box activa, la W saltaba a Tweak en vez de pasar a Select Circle, y la prueba en
   * ejecucion lo cazo. Una diferencia, a favor: el Python solo la actualiza si la barra
   * esta a la vista; aqui se actualiza siempre, que es lo que el usuario espera. */
  for (const ToolGroupView &group : tools_unexpanded_for_space_mode(C, toolbar, mode)) {
    if (!group.is_group()) {
      continue;
    }
    for (const int i : group.tools.index_range()) {
      if (STREQ(group.tools[i]->idname, args.tool->idname)) {
        group_active_set(toolbar.space_type, group.tools[0]->idname, i);
        break;
      }
    }
  }
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Entradas publicas
 * \{ */

static const char *mode_for(const bContext *C, const ToolbarDecl &toolbar)
{
  return toolbar.mode_from_context != nullptr ? toolbar.mode_from_context(C) : nullptr;
}

bool activate_by_id(bContext *C,
                    const int space_type,
                    const blender::StringRefNull idname,
                    const bool as_fallback)
{
  const ToolbarDecl *toolbar = toolbar_for_space(space_type);
  if (toolbar == nullptr) {
    return false;
  }
  const char *mode = mode_for(C, *toolbar);
  const blender::Vector<ToolGroupView> entries = tools_unexpanded_for_space_mode(
      C, *toolbar, mode);
  int index = -1;
  const ToolDecl *item = get_by_id(entries, idname.c_str(), &index);
  if (item == nullptr) {
    return false;
  }
  return activate_by_item(C, *toolbar, mode, *item, index, as_fallback);
}

/**
 * `activate_by_id_or_cycle`: solo cicla cuando se vuelve a activar la herramienta que
 * ya esta activa; si no, activa la variante recordada del grupo.
 *
 * `as_fallback` se acepta y se ignora, como en el Python, que lo recibe y no lo pasa a
 * ninguna de sus llamadas. Hacerle caso cambiaria el comportamiento de los atajos.
 */
bool activate_by_id_or_cycle(bContext *C,
                             const int space_type,
                             const blender::StringRefNull idname,
                             const int offset,
                             const bool /*as_fallback*/)
{
  const ToolbarDecl *toolbar = toolbar_for_space(space_type);
  if (toolbar == nullptr) {
    return false;
  }
  const char *mode = mode_for(C, *toolbar);
  const blender::Vector<ToolGroupView> entries = tools_unexpanded_for_space_mode(
      C, *toolbar, mode);
  int unused;
  if (get_by_id(entries, idname.c_str(), &unused) == nullptr) {
    return false;
  }

  WorkSpace *workspace = CTX_wm_workspace(C);
  const bToolRef *tref_active = active_tref(C, workspace, space_type, false);
  const char *id_active = tref_active != nullptr ? tref_active->idname : nullptr;

  const ToolGroupView *group = nullptr;
  const char *id_current = nullptr;
  for (const ToolGroupView &candidate : entries) {
    if (!candidate.is_group()) {
      continue;
    }
    for (const ToolDecl *tool : candidate.tools) {
      if (idname == tool->idname) {
        id_current = candidate.tools[group_active_index(space_type, candidate)]->idname;
        break;
      }
    }
    if (id_current != nullptr) {
      group = &candidate;
      break;
    }
  }

  if (id_current == nullptr) {
    return activate_by_id(C, space_type, idname);
  }
  if (id_active == nullptr || !STREQ(id_active, id_current)) {
    return activate_by_id(C, space_type, id_current);
  }

  /* El `%` del Python nunca es negativo, el de C++ si. */
  const int size = int(group->tools.size());
  const int current = tref_active->runtime != nullptr ? tref_active->runtime->index : 0;
  const int index_found = (((current + offset) % size) + size) % size;

  group_active_set(space_type, group->tools[0]->idname, index_found);
  return activate_by_item(C, *toolbar, mode, *group->tools[index_found], index_found, false);
}

bool activate_by_brush_type(bContext *C,
                            const int space_type,
                            const blender::StringRefNull brush_type,
                            const char **r_tool_id)
{
  const ToolbarDecl *toolbar = toolbar_for_space(space_type);
  if (toolbar == nullptr) {
    return false;
  }
  /* `context.mode` en todos los espacios. Ver la nota en FL_toolsystem.hpp. */
  const char *context_mode = nullptr;
  RNA_enum_identifier(rna_enum_context_mode_items, CTX_data_mode_enum(C), &context_mode);

  const char *tool_id = "builtin.brush";
  for (const ToolDecl *tool : tools_for_space_mode(C, *toolbar, context_mode)) {
    /* Las primitivas de lapiz usan el tipo 'DRAW', que es el del pincel general: no se
     * activan nunca por esta via. Antes era una lista de seis nombres escrita en el
     * operador; ahora es una bandera en su declaracion. */
    if (tool->options & TOOL_OPTION_NO_BRUSH_FALLBACK) {
      continue;
    }
    if ((tool->options & TOOL_OPTION_USE_BRUSHES) && tool->brush_type != nullptr &&
        brush_type == tool->brush_type)
    {
      tool_id = tool->idname;
      break;
    }
  }
  if (r_tool_id != nullptr) {
    *r_tool_id = tool_id;
  }
  return activate_by_id(C, space_type, tool_id);
}

const ToolDecl *tool_find_by_index_active(const bContext *C,
                                          const int space_type,
                                          const int index)
{
  const ToolbarDecl *toolbar = toolbar_for_space(space_type);
  if (toolbar == nullptr) {
    return nullptr;
  }
  const blender::Vector<ToolGroupView> entries = tools_unexpanded_for_space_mode(
      C, *toolbar, mode_for(C, *toolbar));
  int unused;
  return get_active_by_index(space_type, entries, index, &unused);
}

/** \} */

}  // namespace flipendo::toolsystem
