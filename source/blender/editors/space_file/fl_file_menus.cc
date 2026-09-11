/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 *
 * Ver FL_file_menus.hh. Los tres menus que el keymap nativo abre por nombre en
 * el explorador: el contextual de ficheros, el radial de vista y el contextual
 * del explorador de recursos.
 *
 * Los `poll` vienen de dos mixins de Python (`FileBrowserMenu` y
 * `AssetBrowserMenu`) que en C++ son una linea cada uno:
 * `sfile->browse_mode == FILE_BROWSE_MODE_FILES` y
 * `ED_fileselect_is_asset_browser(sfile)`.
 */

#include <optional>

#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "ED_fileselect.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_types.hh"

#include "FL_ui_registry.hh"

#include "FL_file_menus.hh"

namespace blender::ed::file {

/** `context.space_data.params`, por RNA como el Python. */
static PointerRNA file_params_ptr(const bContext *C)
{
  SpaceFile *sfile = CTX_wm_space_file(C);
  if (sfile == nullptr) {
    return PointerRNA_NULL;
  }
  PointerRNA space = RNA_pointer_create_discrete(
      reinterpret_cast<ID *>(CTX_wm_screen(C)), &RNA_SpaceFileBrowser, sfile);
  return RNA_pointer_get(&space, "params");
}

/** `layout.prop_menu_enum(ptr, prop, text=...)`. */
static void prop_menu_enum(uiLayout *layout,
                           PointerRNA *ptr,
                           const char *propname,
                           std::optional<blender::StringRefNull> text = std::nullopt)
{
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);
  if (prop == nullptr) {
    return;
  }
  uiItemMenuEnumR_prop(layout, ptr, prop, text, ICON_NONE);
}

/* -------------------------------------------------------------------- */
/** \name FILEBROWSER_MT_context_menu
 * \{ */

/** `FileBrowserMenu.poll`. */
static bool file_browser_poll(const bContext *C, MenuType * /*mt*/)
{
  const SpaceFile *sfile = CTX_wm_space_file(C);
  return sfile != nullptr && sfile->browse_mode == FILE_BROWSE_MODE_FILES;
}

static void context_menu_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  /* `st.params` se pide AQUI, no al principio: el Python dibuja todas las filas
   * de operador primero y solo revienta al llegar a `prop_menu_enum(params, ...)`
   * si `params` es `None` — que es lo que pasa en un explorador recien abierto.
   * Cogerlo al principio y salir deja el menu vacio, y el volcado lo canto. */
  layout->op("FILE_OT_previous", IFACE_("Back"), ICON_NONE);
  layout->op("FILE_OT_next", IFACE_("Forward"), ICON_NONE);
  layout->op("FILE_OT_parent", IFACE_("Go to Parent"), ICON_NONE);
  layout->op("FILE_OT_refresh", IFACE_("Refresh"), ICON_NONE);
  layout->menu("FILEBROWSER_MT_operations_menu", std::nullopt, ICON_NONE);

  layout->separator();

  PointerRNA props = layout->op("FILE_OT_filenum", IFACE_("Increase Number"), ICON_ADD);
  if (props.data) {
    RNA_int_set(&props, "increment", 1);
  }
  props = layout->op("FILE_OT_filenum", IFACE_("Decrease Number"), ICON_REMOVE);
  if (props.data) {
    RNA_int_set(&props, "increment", -1);
  }

  layout->separator();

  layout->op("FILE_OT_rename", IFACE_("Rename"), ICON_NONE);
  uiLayout *sub = &layout->row(false);
  uiLayoutSetOperatorContext(sub, WM_OP_EXEC_DEFAULT);
  sub->op("FILE_OT_delete", IFACE_("Delete"), ICON_NONE);

  layout->separator();

  sub = &layout->row(false);
  uiLayoutSetOperatorContext(sub, WM_OP_EXEC_DEFAULT);
  sub->op("FILE_OT_directory_new", IFACE_("New Folder"), ICON_NONE);
  layout->op("FILE_OT_bookmark_add", IFACE_("Add Bookmark"), ICON_NONE);

  layout->separator();

  PointerRNA params = file_params_ptr(C);
  if (params.data == nullptr) {
    /* Aqui es donde revienta el Python. */
    return;
  }

  prop_menu_enum(layout, &params, "display_type");
  if (RNA_enum_get(&params, "display_type") == FILE_IMGDISPLAY) {
    prop_menu_enum(layout, &params, "display_size_discrete");
  }
  prop_menu_enum(layout, &params, "recursion_level", IFACE_("Recursions"));
  prop_menu_enum(layout, &params, "sort_method");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name FILEBROWSER_MT_view_pie
 * \{ */

static void view_pie_draw(const bContext *C, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();

  PointerRNA params = file_params_ptr(C);
  if (params.data == nullptr) {
    return;
  }
  uiItemEnumR_string(&pie, &params, "display_type", "LIST_VERTICAL", std::nullopt, ICON_NONE);
  uiItemEnumR_string(&pie, &params, "display_type", "LIST_HORIZONTAL", std::nullopt, ICON_NONE);
  uiItemEnumR_string(&pie, &params, "display_type", "THUMBNAIL", std::nullopt, ICON_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name ASSETBROWSER_MT_context_menu
 * \{ */

/** `AssetBrowserMenu.poll` -> `SpaceAssetInfo.is_asset_browser_poll`. */
static bool asset_browser_poll(const bContext *C, MenuType * /*mt*/)
{
  const SpaceFile *sfile = CTX_wm_space_file(C);
  return sfile != nullptr && ED_fileselect_is_asset_browser(sfile);
}

static void asset_context_menu_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;

  layout->op("ASSET_OT_library_refresh", std::nullopt, ICON_NONE);

  layout->separator();

  uiLayout *sub = &layout->column(false);
  uiLayoutSetOperatorContext(sub, WM_OP_EXEC_DEFAULT);
  PointerRNA props = sub->op("ASSET_OT_clear", IFACE_("Clear Asset"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "set_fake_user", false);
  }
  props = sub->op("ASSET_OT_clear", IFACE_("Clear Asset (Set Fake User)"), ICON_NONE);
  if (props.data) {
    RNA_boolean_set(&props, "set_fake_user", true);
  }

  layout->separator();

  layout->op("ASSET_OT_open_containing_blend_file", std::nullopt, ICON_NONE);

  layout->separator();

  PointerRNA params = file_params_ptr(C);
  if (params.data == nullptr) {
    return;
  }
  if (RNA_enum_get(&params, "display_type") == FILE_IMGDISPLAY) {
    prop_menu_enum(layout, &params, "display_size_discrete");
  }
  prop_menu_enum(layout, &params, "sort_method");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registro
 * \{ */

static const flipendo::MenuDecl file_menus[] = {
    {
        /*idname*/ "FILEBROWSER_MT_context_menu",
        /*label*/ N_("Files"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ context_menu_draw,
        /*poll*/ file_browser_poll,
    },
    {
        /*idname*/ "FILEBROWSER_MT_view_pie",
        /*label*/ N_("View"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ view_pie_draw,
    },
    {
        /*idname*/ "ASSETBROWSER_MT_context_menu",
        /*label*/ N_("Assets"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ asset_context_menu_draw,
        /*poll*/ asset_browser_poll,
    },
};

void file_menus_register()
{
  flipendo::menus_register({file_menus, ARRAY_SIZE(file_menus)});
}

/** \} */

}  // namespace blender::ed::file
