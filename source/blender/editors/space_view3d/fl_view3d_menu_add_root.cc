/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spview3d
 *
 * Ver FL_view3d_menus.hh.
 *
 * Familia 8: `VIEW3D_MT_add`, el menu de Shift-A. Es el menu mas usado de la
 * vista 3D y el que mas decisiones toma de toda la tanda, asi que va solo.
 *
 * UNA DECISION QUE HAY QUE ESCRIBIR: `is_extended()`
 * -------------------------------------------------
 * El Python pregunta dos veces `VIEW3D_MT_armature_add.is_extended()` y
 * `VIEW3D_MT_camera_add.is_extended()`, que es
 * `len(cls.draw._draw_funcs) > 1` (`bpy_types.py:1117`): «¿alguien ha metido
 * filas en este menu con `bpy.types.X.append()`?». Si las hay, ensena el
 * submenu; si no, ensena directamente la fila suelta.
 *
 * Desde C++ no hay forma de preguntarselo a una clase de Python sin usar la API
 * de Python, y en el Flipendo de destino **no hay interprete**, asi que no hay
 * addons y `is_extended()` es estructuralmente falso. Se escribe la rama «no
 * extendido», que es la que el editor va a tomar siempre. Queda dicho aqui y en
 * `politicas/MENUS-DEL-KEYMAP-A-CPP.md`: no es una copia, es una decision.
 */

#include <optional>

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_main.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "FL_ui_registry.hh"

#include "FL_view3d_menus.hh"

namespace blender::ed::view3d {

static void add_root_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  /* Si se llega aqui desde la busqueda (contexto `EXEC_REGION_WIN`), el Python
   * ofrece primero su propia busqueda dentro del menu. */
  if (uiLayoutGetOperatorContext(layout) == WM_OP_EXEC_REGION_WIN) {
    uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_REGION_WIN);
    PointerRNA props = layout->op("WM_OT_search_single_menu", IFACE_("Search..."), ICON_VIEWZOOM);
    if (props.data) {
      RNA_string_set(&props, "menu_idname", "VIEW3D_MT_add");
    }
    layout->separator();
  }

  /* Ni `EXEC_SCREEN` ni `EXEC_AREA`: el primero deja a los operadores sin `v3d`
   * y el segundo sin `rv3d`, que es lo que rompia «align_view» en la primera
   * llamada (#32719). Comentario heredado del Python, y sigue valiendo. */
  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_REGION_WIN);

  layout->menu("VIEW3D_MT_mesh_add", std::nullopt, ICON_OUTLINER_OB_MESH);
  layout->menu("VIEW3D_MT_curve_add", std::nullopt, ICON_OUTLINER_OB_CURVE);
  layout->menu("VIEW3D_MT_surface_add", std::nullopt, ICON_OUTLINER_OB_SURFACE);
  layout->menu("VIEW3D_MT_metaball_add", IFACE_("Metaball"), ICON_OUTLINER_OB_META);
  layout->op("OBJECT_OT_text_add", IFACE_("Text"), ICON_OUTLINER_OB_FONT);
  layout->op("OBJECT_OT_pointcloud_random_add", IFACE_("Point Cloud"), ICON_OUTLINER_OB_POINTCLOUD);
  layout->menu("VIEW3D_MT_volume_add",
               CTX_IFACE_(BLT_I18NCONTEXT_ID_ID, "Volume"),
               ICON_OUTLINER_OB_VOLUME);
  layout->menu("VIEW3D_MT_grease_pencil_add",
               IFACE_("Grease Pencil"),
               ICON_OUTLINER_OB_GREASEPENCIL);

  layout->separator();

  /* `VIEW3D_MT_armature_add.is_extended()` — ver la cabecera del fichero. */
  layout->op("OBJECT_OT_armature_add", IFACE_("Armature"), ICON_OUTLINER_OB_ARMATURE);

  PointerRNA props = layout->op("OBJECT_OT_add", IFACE_("Lattice"), ICON_OUTLINER_OB_LATTICE);
  if (props.data) {
    RNA_enum_set_identifier(nullptr, &props, "type", "LATTICE");
  }

  layout->separator();

  layout->menu("VIEW3D_MT_empty_add", std::nullopt, ICON_OUTLINER_OB_EMPTY);
  layout->menu("VIEW3D_MT_image_add", IFACE_("Image"), ICON_OUTLINER_OB_IMAGE);

  layout->separator();

  layout->menu("VIEW3D_MT_light_add", std::nullopt, ICON_OUTLINER_OB_LIGHT);
  layout->menu("VIEW3D_MT_lightprobe_add", std::nullopt, ICON_OUTLINER_OB_LIGHTPROBE);

  layout->separator();

  /* `VIEW3D_MT_camera_add.is_extended()` — rama «no extendido»: el Python llama
   * al `draw()` del otro menu sobre este mismo layout, que es `uiItemMContents`. */
  uiItemMContents(layout, "VIEW3D_MT_camera_add");

  layout->separator();

  layout->op("OBJECT_OT_speaker_add", IFACE_("Speaker"), ICON_OUTLINER_OB_SPEAKER);

  layout->separator();

  uiItemMenuEnumO(layout,
                  C,
                  "OBJECT_OT_effector_add",
                  "type",
                  IFACE_("Force Field"),
                  ICON_OUTLINER_OB_FORCE_FIELD);

  layout->separator();

  /* `bool(bpy.data.collections)` y `len(bpy.data.collections) > 10`. */
  const Main *bmain = CTX_data_main(C);
  const int collections_num = bmain ? BLI_listbase_count(&bmain->collections) : 0;
  const bool has_collections = collections_num != 0;

  uiLayout *col = &layout->column(false);
  uiLayoutSetEnabled(col, has_collections);

  if (!has_collections || collections_num > 10) {
    uiLayoutSetOperatorContext(col, WM_OP_INVOKE_REGION_WIN);
    col->op("OBJECT_OT_collection_instance_add",
            has_collections ? IFACE_("Collection Instance...") :
                              IFACE_("No Collections to Instance"),
            ICON_OUTLINER_OB_GROUP_INSTANCE);
  }
  else {
    uiItemMenuEnumO(col,
                    C,
                    "OBJECT_OT_collection_instance_add",
                    "collection",
                    IFACE_("Collection Instance"),
                    ICON_OUTLINER_OB_GROUP_INSTANCE);
  }
}

static const flipendo::MenuDecl view3d_add_root_menu[] = {
    {
        /*idname*/ "VIEW3D_MT_add",
        /*label*/ N_("Add"),
        /*description*/ nullptr,
        /*translation_context*/ BLT_I18NCONTEXT_OPERATOR_DEFAULT,
        /*draw*/ add_root_draw,
        /*poll*/ nullptr,
        /*flag*/ int(MenuTypeFlag::SearchOnKeyPress),
    },
};

void view3d_add_root_menu_register()
{
  flipendo::menus_register({view3d_add_root_menu, ARRAY_SIZE(view3d_add_root_menu)});
}

}  // namespace blender::ed::view3d
