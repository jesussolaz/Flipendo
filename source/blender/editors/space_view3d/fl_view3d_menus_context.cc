/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 *
 * Ver FL_view3d_menus.hh.
 *
 * Familia 5: los menus contextuales cortos de la vista 3D (boton derecho en
 * reticula, metabola, texto y curvas nuevas) y los tres del lapiz de cera que
 * cuelgan de teclas propias — animacion (I con Alt), grupos de vertices y
 * material activo (U).
 *
 * Todos llaman a submenus que **siguen en Python** (`VIEW3D_MT_mirror`,
 * `VIEW3D_MT_edit_font`): se buscan por cadena en `WM_menutype_find()`, asi que
 * conviven sin problema hasta que les toque el turno.
 */

#include <optional>

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_icons.h"
#include "BKE_material.hh"
#include "BKE_preview_image.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "DNA_material_types.h"
#include "DNA_object_types.h"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "FL_ui_registry.hh"

#include "FL_view3d_menus.hh"

namespace blender::ed::view3d {

/** `layout.operator_menu_enum(op, prop)` sin `text=`; ver `fl_view3d_menus_object.cc`. */
static void menu_enum_o(uiLayout *layout,
                        const bContext *C,
                        const char *opname,
                        const char *propname)
{
  wmOperatorType *ot = WM_operatortype_find(opname, false);
  if (ot == nullptr) {
    return;
  }
  PointerRNA opptr;
  uiItemMenuEnumFullO_ptr(layout, C, ot, propname, std::nullopt, ICON_NONE, &opptr);
}

/* -------------------------------------------------------------------- */
/** \name Reticula, metabola y texto
 * \{ */

static void edit_lattice_context_menu_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  layout->menu("VIEW3D_MT_mirror", std::nullopt, ICON_NONE);
  menu_enum_o(layout, C, "LATTICE_OT_flip", "axis");
  layout->menu("VIEW3D_MT_snap", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("LATTICE_OT_make_regular", std::nullopt, ICON_NONE);
}

static void edit_metaball_context_menu_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);

  /* Anadir. */
  layout->op("MBALL_OT_duplicate_move", std::nullopt, ICON_NONE);

  layout->separator();

  /* Modificar. */
  layout->menu("VIEW3D_MT_mirror", std::nullopt, ICON_NONE);
  layout->menu("VIEW3D_MT_snap", std::nullopt, ICON_NONE);

  layout->separator();

  /* Quitar. */
  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_REGION_WIN);
  layout->op("MBALL_OT_delete_metaelems", IFACE_("Delete"), ICON_NONE);
}

static void edit_font_context_menu_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);

  layout->op("FONT_OT_text_cut", IFACE_("Cut"), ICON_NONE);
  layout->op("FONT_OT_text_copy", IFACE_("Copy"), ICON_COPYDOWN);
  layout->op("FONT_OT_text_paste", IFACE_("Paste"), ICON_PASTEDOWN);

  layout->separator();

  layout->op("FONT_OT_select_all", std::nullopt, ICON_NONE);

  layout->separator();

  layout->menu("VIEW3D_MT_edit_font", std::nullopt, ICON_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Curvas nuevas
 * \{ */

static void edit_curves_add_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;

  layout->op("CURVES_OT_add_bezier", IFACE_("Bezier"), ICON_CURVE_BEZCURVE);
  layout->op("CURVES_OT_add_circle", IFACE_("Circle"), ICON_CURVE_BEZCIRCLE);
}

static void edit_curves_context_menu_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);

  /* Anadir. */
  layout->op("CURVES_OT_subdivide", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("CURVES_OT_extrude_move", std::nullopt, ICON_NONE);

  layout->separator();

  /* Deformar. */
  layout->menu("VIEW3D_MT_mirror", std::nullopt, ICON_NONE);
  layout->menu("VIEW3D_MT_snap", std::nullopt, ICON_NONE);

  layout->separator();

  /* Banderas. */
  menu_enum_o(layout, C, "CURVES_OT_curve_type_set", "type");
  menu_enum_o(layout, C, "CURVES_OT_handle_type_set", "type");
  layout->op("CURVES_OT_cyclic_toggle", std::nullopt, ICON_NONE);
  layout->op("CURVES_OT_switch_direction", std::nullopt, ICON_NONE);

  layout->separator();

  /* Quitar. */
  layout->op("CURVES_OT_separate", std::nullopt, ICON_NONE);
  layout->op("CURVES_OT_delete", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("CURVES_OT_split", std::nullopt, ICON_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lapiz de cera
 * \{ */

static void edit_greasepencil_animation_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;

  layout->op("GREASE_PENCIL_OT_insert_blank_frame",
             IFACE_("Insert Blank Keyframe (Active Layer)"),
             ICON_NONE);
  PointerRNA props = layout->op("GREASE_PENCIL_OT_insert_blank_frame",
                                IFACE_("Insert Blank Keyframe (All Layers)"),
                                ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "all_layers", true);
  }

  layout->separator();
  props = layout->op("GREASE_PENCIL_OT_frame_duplicate",
                     IFACE_("Duplicate Active Keyframe (Active Layer)"),
                     ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "all", false);
  }
  props = layout->op("GREASE_PENCIL_OT_frame_duplicate",
                     IFACE_("Duplicate Active Keyframe (All Layers)"),
                     ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "all", true);
  }

  layout->separator();
  props = layout->op("GREASE_PENCIL_OT_active_frame_delete",
                     IFACE_("Delete Active Keyframe (Active Layer)"),
                     ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "all", false);
  }
  props = layout->op("GREASE_PENCIL_OT_active_frame_delete",
                     IFACE_("Delete Active Keyframe (All Layers)"),
                     ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "all", true);
  }
}

static void greasepencil_vertex_group_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;

  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_AREA);

  layout->op("OBJECT_OT_vertex_group_add", IFACE_("Add New Group"), ICON_NONE);
}

/** `poll`: objeto activo con al menos una ranura de material. */
static bool greasepencil_material_active_poll(const bContext *C, MenuType * /*mt*/)
{
  const Object *ob = CTX_data_active_object(C);
  return ob != nullptr && ob->totcol != 0;
}

static void greasepencil_material_active_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return;
  }

  for (int i = 0; i < ob->totcol; i++) {
    Material *mat = BKE_object_material_get(ob, i + 1);
    if (mat == nullptr) {
      continue;
    }
    /* `mat.id_data.preview_ensure()` y `preview.icon_id`. */
    PreviewImage *preview = BKE_previewimg_id_ensure(&mat->id);
    if (preview == nullptr) {
      continue;
    }
    const int icon = BKE_icon_preview_ensure(&mat->id, preview);
    /* El texto es el nombre del dato, no una cadena del programa; pasa por el
     * mismo `IFACE_` que le aplica `rna_uiItemO` en Python. */
    PointerRNA props = layout->op(
        "GREASE_PENCIL_OT_set_material", IFACE_(mat->id.name + 2), icon);
    if (props.data) {
      /* `slot` es una enumeracion DINAMICA (`material_enum_itemf`), no una cadena:
       * hay que resolver el identificador con el contexto delante. Con
       * `RNA_string_set` el proceso se cae, y el volcador lo canta como
       * `NO-CUBIERTO motivo=fallo`. */
      RNA_enum_set_identifier(const_cast<bContext *>(C), &props, "slot", mat->id.name + 2);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registro
 * \{ */

static const flipendo::MenuDecl view3d_context_menus[] = {
    {
        /*idname*/ "VIEW3D_MT_edit_lattice_context_menu",
        /*label*/ N_("Lattice"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ edit_lattice_context_menu_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_edit_metaball_context_menu",
        /*label*/ N_("Metaball"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ edit_metaball_context_menu_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_edit_font_context_menu",
        /*label*/ N_("Text"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ edit_font_context_menu_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_edit_curves_add",
        /*label*/ N_("Add"),
        /*description*/ nullptr,
        /*translation_context*/ BLT_I18NCONTEXT_OPERATOR_DEFAULT,
        /*draw*/ edit_curves_add_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_edit_curves_context_menu",
        /*label*/ N_("Curves"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ edit_curves_context_menu_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_edit_greasepencil_animation",
        /*label*/ N_("Animation"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ edit_greasepencil_animation_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_greasepencil_vertex_group",
        /*label*/ N_("Vertex Groups"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ greasepencil_vertex_group_draw,
    },
    {
        /*idname*/ "VIEW3D_MT_greasepencil_material_active",
        /*label*/ N_("Active Material"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ greasepencil_material_active_draw,
        /*poll*/ greasepencil_material_active_poll,
    },
};

void view3d_context_menus_register()
{
  flipendo::menus_register({view3d_context_menus, ARRAY_SIZE(view3d_context_menus)});
}

/** \} */

}  // namespace blender::ed::view3d
