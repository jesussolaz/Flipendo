/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 *
 * Ver FL_view3d_menus.hh.
 *
 * Familia 16: los tres menus de opciones de hueso —
 * `VIEW3D_MT_bone_options_toggle`, `_enable` y `_disable` — que el keymap nativo
 * abre con Shift-W, Ctrl-Shift-W y Alt-W en edicion de esqueleto y en pose.
 *
 * Estaban bloqueados hasta esta noche: los tres llaman a
 * `wm.context_collection_boolean_set`, que era Python. Ya es C++
 * (`windowmanager/intern/wm_context_ops.cc`), asi que se pueden cerrar.
 *
 * LO QUE TIENEN DE PARTICULAR
 * ---------------------------
 * El rotulo de cada fila **no es una cadena del programa**: sale del nombre de
 * interfaz de la propiedad RNA del hueso (`bpy.types.EditBone.bl_rna.properties[opt].name`
 * en el Python). En C++ es `RNA_property_ui_name()` sobre
 * `RNA_struct_type_find_property(&RNA_EditBone | &RNA_Bone, opt)`. Por eso no
 * hay ningun `N_()` en este fichero: no hay nada que extraer para traducir aqui,
 * el texto ya viene traducido del propio RNA.
 *
 * Y las dos ramas no se distinguen solo en el prefijo: en edicion de esqueleto
 * hay una quinta opcion (`lock`) que en pose no existe, y el camino de datos
 * cambia de `selected_bones` a `selected_pose_bones` con prefijo `bone.`.
 */

#include <optional>

#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "FL_ui_registry.hh"

#include "FL_view3d_menus.hh"

namespace blender::ed::view3d {

/** El `draw` comun de las tres, con el `type` del operador como unica variable. */
static void bone_options_draw(const bContext *C, Menu *menu, const char *type)
{
  uiLayout *layout = menu->layout;

  /* `context.mode == 'EDIT_ARMATURE'`. */
  const bool is_edit_armature = CTX_data_mode_enum(C) == CTX_MODE_EDIT_ARMATURE;

  StructRNA *bone_srna = is_edit_armature ? &RNA_EditBone : &RNA_Bone;
  const char *data_path_iter = is_edit_armature ? "selected_bones" : "selected_pose_bones";
  const char *opt_suffix = is_edit_armature ? "" : "bone.";

  const char *options[] = {
      "show_wire",
      "use_deform",
      "use_envelope_multiply",
      "use_inherit_rotation",
      /* Solo en edicion de esqueleto; en pose no existe. */
      "lock",
  };
  const int options_num = is_edit_armature ? ARRAY_SIZE(options) : ARRAY_SIZE(options) - 1;

  for (int i = 0; i < options_num; i++) {
    const char *opt = options[i];
    PropertyRNA *bone_prop = RNA_struct_type_find_property(bone_srna, opt);
    if (bone_prop == nullptr) {
      continue;
    }
    /* El rotulo sale del RNA del hueso, no de una cadena de aqui. */
    PointerRNA props = layout->op(
        "WM_OT_context_collection_boolean_set", RNA_property_ui_name(bone_prop), ICON_NONE);
    if (props.data == nullptr) {
      continue;
    }
    RNA_string_set(&props, "data_path_iter", data_path_iter);

    char data_path_item[256];
    SNPRINTF(data_path_item, "%s%s", opt_suffix, opt);
    RNA_string_set(&props, "data_path_item", data_path_item);

    RNA_enum_set_identifier(nullptr, &props, "type", type);
  }
}

static void bone_options_toggle_draw(const bContext *C, Menu *menu)
{
  bone_options_draw(C, menu, "TOGGLE");
}

static void bone_options_enable_draw(const bContext *C, Menu *menu)
{
  bone_options_draw(C, menu, "ENABLE");
}

static void bone_options_disable_draw(const bContext *C, Menu *menu)
{
  bone_options_draw(C, menu, "DISABLE");
}

static const flipendo::MenuDecl view3d_bone_menus[] = {
    {
        /*idname*/ "VIEW3D_MT_bone_options_toggle",
        /*label*/ N_("Toggle Bone Options"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ bone_options_toggle_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_bone_options_enable",
        /*label*/ N_("Enable Bone Options"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ bone_options_enable_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_bone_options_disable",
        /*label*/ N_("Disable Bone Options"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ bone_options_disable_draw,
    },
};

void view3d_bone_menus_register()
{
  flipendo::menus_register({view3d_bone_menus, ARRAY_SIZE(view3d_bone_menus)});
}

}  // namespace blender::ed::view3d
