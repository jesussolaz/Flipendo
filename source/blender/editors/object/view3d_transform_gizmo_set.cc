/* SPDX-FileCopyrightText: 2018 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 *
 * Puerto nativo en C++ de `view3d.transform_gizmo_set`, que vivia en
 * `scripts/startup/bl_operators/view3d.py` (Flipendo carril C). Enciende el gizmo de la
 * vista 3D y deja activos los de mover/rotar/escalar que pida la propiedad `type`, que
 * es un enum de banderas (se pueden pedir varios a la vez).
 *
 * Sobre donde vive: lo natural seria `editors/space_view3d`, pero esa noche el
 * `CMakeLists.txt` de ese directorio estaba con trabajo sin commitear de otro carril (el
 * de menus) y meter ahi un fichero nuevo habria arrastrado su trabajo a medias a este
 * commit. Se deja en `editors/object`, que es donde se registra (`ED_operatortypes_object`)
 * y donde estan las banderas que toca (`show_gizmo_object_*`). Mover el fichero a
 * `space_view3d` cuando el arbol este tranquilo es cosmetica y no cambia nada observable.
 */

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_screen.hh"

#include "object_intern.hh"

namespace blender::ed::object {

enum {
  V3D_GIZMO_SET_TRANSLATE = (1 << 0),
  V3D_GIZMO_SET_ROTATE = (1 << 1),
  V3D_GIZMO_SET_SCALE = (1 << 2),
};

static const EnumPropertyItem transform_gizmo_type_items[] = {
    {V3D_GIZMO_SET_TRANSLATE, "TRANSLATE", 0, "Move", ""},
    {V3D_GIZMO_SET_ROTATE, "ROTATE", 0, "Rotate", ""},
    {V3D_GIZMO_SET_SCALE, "SCALE", 0, "Scale", ""},
    {0, nullptr, 0, nullptr, nullptr},
};

/* Las tres propiedades de RNA, en el mismo orden que la tupla `attrs` del Python, para
 * que el emparejamiento con las banderas de `type` sea el mismo. */
static const char *transform_gizmo_attrs[3] = {
    "show_gizmo_object_translate",
    "show_gizmo_object_rotate",
    "show_gizmo_object_scale",
};

/* `area and (area.type == 'VIEW_3D')`, literal. */
static bool transform_gizmo_set_poll(bContext *C)
{
  const ScrArea *area = CTX_wm_area(C);
  return area != nullptr && area->spacetype == SPACE_VIEW3D;
}

/* Se asigna por RNA y con su `RNA_property_update()`, como hacia `setattr()` del Python:
 * las tres banderas y `show_gizmo` mandan un notificador NC_SPACE|ND_SPACE_VIEW3D y sin
 * el la vista no se redibuja. */
static void space_bool_set(bContext *C, PointerRNA *ptr, const char *identifier, const bool value)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, identifier);
  if (prop == nullptr) {
    return;
  }
  RNA_property_boolean_set(ptr, prop, value);
  RNA_property_update(C, ptr, prop);
}

static wmOperatorStatus transform_gizmo_set_exec(bContext *C, wmOperator *op)
{
  bScreen *screen = CTX_wm_screen(C);
  SpaceLink *space_data = CTX_wm_space_data(C);
  if (screen == nullptr || space_data == nullptr) {
    return OPERATOR_CANCELLED;
  }

  PointerRNA ptr = RNA_pointer_create_discrete(&screen->id, &RNA_SpaceView3D, space_data);

  space_bool_set(C, &ptr, "show_gizmo", true);

  const int type = RNA_enum_get(op->ptr, "type");
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  const int flags[3] = {V3D_GIZMO_SET_TRANSLATE, V3D_GIZMO_SET_ROTATE, V3D_GIZMO_SET_SCALE};

  for (int i = 0; i < 3; i++) {
    const bool active = (type & flags[i]) != 0;
    if (extend) {
      /* Con `extend` solo se encienden los pedidos; los demas se quedan como estaban. */
      if (active) {
        space_bool_set(C, &ptr, transform_gizmo_attrs[i], true);
      }
    }
    else {
      space_bool_set(C, &ptr, transform_gizmo_attrs[i], active);
    }
  }

  return OPERATOR_FINISHED;
}

static wmOperatorStatus transform_gizmo_set_invoke(bContext *C,
                                                   wmOperator *op,
                                                   const wmEvent *event)
{
  /* Si quien llama no ha fijado `extend` a mano, la tecla Mayus del evento decide.
   * `self.properties.is_property_set("extend")` del Python. */
  PropertyRNA *prop = RNA_struct_find_property(op->ptr, "extend");
  if (!RNA_property_is_set(op->ptr, prop)) {
    RNA_property_boolean_set(op->ptr, prop, (event->modifier & KM_SHIFT) != 0);
  }
  return transform_gizmo_set_exec(C, op);
}

void VIEW3D_OT_transform_gizmo_set(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Transform Gizmo Set";
  ot->idname = "VIEW3D_OT_transform_gizmo_set";
  ot->description = "Set the current transform gizmo";

  /* API callbacks. */
  ot->exec = transform_gizmo_set_exec;
  ot->invoke = transform_gizmo_set_invoke;
  ot->poll = transform_gizmo_set_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna, "extend", false, "Extend", "");
  RNA_def_enum_flag(ot->srna, "type", transform_gizmo_type_items, 0, "Type", "");
}

}  // namespace blender::ed::object
