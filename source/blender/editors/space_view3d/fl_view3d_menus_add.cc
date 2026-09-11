/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 *
 * Ver FL_view3d_menus.hh.
 *
 * Familia 7: los dos menus de «anadir» que el keymap nativo abre por nombre —
 * el de mallas (Shift-A en modo objeto, por dentro del menu grande) y el de
 * curvas en edicion, que es el que la barra superior y el keymap invocan como
 * `TOPBAR_MT_edit_curve_add`.
 *
 * Los dos llevan `bl_options = {'SEARCH_ON_KEY_PRESS'}`, que en el `MenuDecl` es
 * `MenuTypeFlag::SearchOnKeyPress`. Si se olvida, el volcado de **registro** lo
 * canta (`flag=[]` frente a `flag=[SEARCH_ON_KEY_PRESS]`).
 */

#include <optional>

#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "DNA_object_types.h"

#include "RNA_access.hh"

#include "ED_geometry.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_types.hh"

#include "FL_ui_registry.hh"

#include "FL_view3d_menus.hh"

namespace blender::ed::view3d {

/* -------------------------------------------------------------------- */
/** \name VIEW3D_MT_mesh_add
 * \{ */

static void mesh_add_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);

  layout->op("MESH_OT_primitive_plane_add", IFACE_("Plane"), ICON_MESH_PLANE);
  layout->op("MESH_OT_primitive_cube_add", IFACE_("Cube"), ICON_MESH_CUBE);
  layout->op("MESH_OT_primitive_circle_add", IFACE_("Circle"), ICON_MESH_CIRCLE);
  layout->op("MESH_OT_primitive_uv_sphere_add", IFACE_("UV Sphere"), ICON_MESH_UVSPHERE);
  layout->op("MESH_OT_primitive_ico_sphere_add", IFACE_("Ico Sphere"), ICON_MESH_ICOSPHERE);
  layout->op("MESH_OT_primitive_cylinder_add", IFACE_("Cylinder"), ICON_MESH_CYLINDER);
  layout->op("MESH_OT_primitive_cone_add", IFACE_("Cone"), ICON_MESH_CONE);
  layout->op("MESH_OT_primitive_torus_add", IFACE_("Torus"), ICON_MESH_TORUS);

  layout->separator();

  layout->op("MESH_OT_primitive_grid_add", IFACE_("Grid"), ICON_MESH_GRID);
  layout->op("MESH_OT_primitive_monkey_add", IFACE_("Monkey"), ICON_MESH_MONKEY);

  geometry::ui_template_node_operator_asset_menu_items(
      *layout, *const_cast<bContext *>(C), "Add");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name TOPBAR_MT_edit_curve_add
 * \{ */

static void edit_curve_add_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  /* `context.active_object.type == 'SURFACE'`. */
  const Object *ob = CTX_data_active_object(C);
  const bool is_surf = ob != nullptr && ob->type == OB_SURF;

  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_REGION_WIN);

  /* El Python llama al `draw()` de la otra clase con el mismo `layout`, que es
   * exactamente lo que hace `uiItemMContents()` — el mismo bloque, sin boton
   * propio. Ninguno de los dos menus destino tiene `poll`, asi que la
   * comprobacion que hace `uiItemMContents` de mas no cambia nada. */
  uiItemMContents(layout, is_surf ? "VIEW3D_MT_surface_add" : "VIEW3D_MT_curve_add");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registro
 * \{ */

static const flipendo::MenuDecl view3d_add_menus[] = {
    {
        /*idname*/ "VIEW3D_MT_mesh_add",
        /*label*/ N_("Mesh"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ mesh_add_draw,
        /*poll*/ nullptr,
        /*flag*/ int(MenuTypeFlag::SearchOnKeyPress),
    },
    {
        /*idname*/ "TOPBAR_MT_edit_curve_add",
        /*label*/ N_("Add"),
        /*description*/ nullptr,
        /*translation_context*/ BLT_I18NCONTEXT_OPERATOR_DEFAULT,
        /*draw*/ edit_curve_add_draw,
        /*poll*/ nullptr,
        /*flag*/ int(MenuTypeFlag::SearchOnKeyPress),
    },
};

void view3d_add_menus_register()
{
  flipendo::menus_register({view3d_add_menus, ARRAY_SIZE(view3d_add_menus)});
}

/** \} */

}  // namespace blender::ed::view3d
