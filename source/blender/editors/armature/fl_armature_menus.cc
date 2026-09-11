/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edarmature
 *
 * `POSE_MT_selection_sets_select`, el ultimo menu de los 133 que el keymap nativo abre
 * por nombre y que no era ni un panel ni estaba contaminado.
 *
 * El esqueleto no tiene editor propio, asi que el alta cuelga de
 * `ED_operatortypes_armature()`, que es lo que corre al arrancar — el mismo sitio que
 * eligio la migracion de `MASK_MT_add` para las mascaras.
 *
 * Por que estaba dado por bloqueado, y por que ya no lo esta
 * --------------------------------------------------------
 * La politica lo daba por bloqueado «porque `pose.selection_set_select` sigue siendo un
 * operador de Python». Eso NO bloquea: el `idname` de un operador se resuelve en tiempo
 * de dibujo, asi que el menu nativo funciona hoy con el operador de Python y seguira
 * funcionando el dia que sea nativo. Es la misma «deuda con nombre y apellidos» que ya
 * llevan `VIEW3D_MT_transform_gizmo_pie` y los de extrusion. Lo que si bloqueaba era la
 * propiedad del fichero: la clase vive en `properties_data_armature.py`.
 *
 * Todo lo que este menu toca lo registra Python hoy
 * ------------------------------------------------
 * - `Object.selection_sets` es una `CollectionProperty` que declara
 *   `bl_operators/bone_selection_sets.py` con `bpy.props`. Desde C++ se lee igual, por
 *   RNA y por nombre: a `RNA_struct_find_property()` le da lo mismo quien la registrara.
 * - `POSE_OT_selection_set_select` y su `poll` son de ese mismo fichero. El `poll` del
 *   menu del Python es literalmente `bpy.types.POSE_OT_selection_set_select.poll(context)`,
 *   asi que aqui se busca el tipo de operador y se le pregunta con `WM_operator_poll()`.
 *   Si no esta registrado —editor sin interprete y con ese fichero aun sin migrar— el
 *   `poll` devuelve falso y el menu no se dibuja, que es lo que hace el Python cuando
 *   `bpy.types.POSE_OT_...` no existe: la excepcion del `poll` se traga y cuenta como
 *   falso.
 *
 * Lo que NO se verifico, dicho: el bloque de la linea base sale `NO-CUBIERTO
 * motivo=poll`, porque la escena de fabrica no tiene ningun esqueleto en modo pose. De
 * este menu queda verificado el REGISTRO —idname, etiqueta, contexto de traduccion,
 * banderas y que tiene `draw` y `poll`— y no su dibujo.
 */

#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "FL_ui_registry.hh"

#include "MEM_guardedalloc.h"

#include "FL_armature_menus.hh"

namespace blender::ed::armature {

/** `return bpy.types.POSE_OT_selection_set_select.poll(context)`. */
static bool selection_sets_select_poll(const bContext *C, MenuType * /*mt*/)
{
  wmOperatorType *ot = WM_operatortype_find("POSE_OT_selection_set_select", true);
  if (ot == nullptr) {
    return false;
  }
  return WM_operator_poll(const_cast<bContext *>(C), ot);
}

static void selection_sets_select_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  /* `layout.operator_context = 'EXEC_DEFAULT'`: va ANTES del bucle, como en el Python. */
  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_DEFAULT);

  /* `context.object.selection_sets`: con el objeto a `None`, AttributeError. */
  PointerRNA ob_ptr = CTX_data_pointer_get(C, "object");
  if (ob_ptr.data == nullptr) {
    return;
  }
  PropertyRNA *sets = RNA_struct_find_property(&ob_ptr, "selection_sets");
  if (sets == nullptr) {
    /* Sin el fichero que declara la propiedad tampoco hay operador, asi que el `poll`
     * ya habria dicho que no; se comprueba igual para no desreferenciar nada. */
    return;
  }

  int idx = 0;
  RNA_PROP_BEGIN (&ob_ptr, itemptr, sets) {
    char name[256];
    char *name_ptr = RNA_string_get_alloc(&itemptr, "name", name, sizeof(name), nullptr);
    PointerRNA props = layout->op(
        "POSE_OT_selection_set_select", IFACE_(name_ptr ? name_ptr : ""), ICON_NONE);
    if (props.data != nullptr) {
      RNA_int_set(&props, "selection_set_index", idx);
    }
    if (name_ptr != nullptr && name_ptr != name) {
      MEM_freeN(name_ptr);
    }
    idx++;
  }
  RNA_PROP_END;
}

static const flipendo::MenuDecl armature_menus[] = {
    {
        /*idname*/ "POSE_MT_selection_sets_select",
        /*label*/ N_("Select Selection Set"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ selection_sets_select_draw,
        /*poll*/ selection_sets_select_poll,
    },
};

void menus_register()
{
  flipendo::menus_register({armature_menus, ARRAY_SIZE(armature_menus)});
}

}  // namespace blender::ed::armature
