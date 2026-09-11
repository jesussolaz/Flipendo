/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup sptopbar
 *
 * Ver FL_topbar_menus.hh. Los menus de la barra superior que ya son nativos:
 * `TOPBAR_MT_file_context_menu` (el contextual de la cabecera), `TOPBAR_MT_file_new`
 * (el «Nuevo»), `TOPBAR_MT_templates_more` y `TOPBAR_MT_file_export` (Archivo >
 * Exportar).
 *
 * `TOPBAR_MT_file_export`: por que esta aqui, si el keymap no lo abre
 * ------------------------------------------------------------------
 * No entro por la lista de los 133 del keymap, sino por una **capacidad perdida**.
 * El addon del motor anadia a ese menu la fila «Save as game runtime» con
 * `bpy.types.TOPBAR_MT_file_export.append(menu_func_export)` — exportar el juego como
 * ejecutable autonomo, que es lo que vende el motor. Al migrar el addon a C++
 * (`politicas/ADDONS-MOTOR-A-CPP.md` §4.3) el operador `wm.save_as_runtime` quedo
 * nativo y registrado, pero la fila se quedo por el camino: desde C++ no hay forma de
 * insertar en un menu de Python, asi que el apunte quedo como deuda «para cuando
 * `space_topbar.py` pase a nativo». Es ese dia.
 *
 * Esa fila va la ULTIMA del menu, porque `append()` dibuja despues del `draw()` de la
 * clase. No es una licencia: es donde estaba.
 *
 * Las banderas de compilacion
 * --------------------------
 * El `draw()` del Python consulta `bpy.app.build_options.*`, que no es mas que los
 * `#ifdef WITH_*` de `bpy_app_build_options.cc`. Aqui se consultan los mismos macros
 * directamente, y por eso el `CMakeLists.txt` de este editor define los nueve que hacen
 * falta. Si alguno faltara, su fila desapareceria del menu y el volcado de dibujo lo
 * cantaria al instante: la comprobacion se comprueba sola.
 */

#include <optional>

#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_utildefines.h"

#include "BKE_appdir.hh"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "RNA_access.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_types.hh"

#include "FL_ui_registry.hh"

#include "FL_topbar_menus.hh"

namespace blender::ed::topbar {

/* -------------------------------------------------------------------- */
/** \name TOPBAR_MT_file_new
 *
 * `draw_ex(use_splash=False, use_more=False)` del Python: la plantilla
 * «General» y despues una fila por plantilla de aplicacion encontrada en disco,
 * todas sin icono. La lista sale de `BKE_appdir_app_templates()`, que es lo
 * mismo que recorre `bpy.utils.app_template_paths()`.
 * \{ */

/**
 * `TOPBAR_MT_file_new.draw_ex()` del Python, con sus dos lectores.
 *
 * `use_more` es el que usa `TOPBAR_MT_templates_more`: se salta las primeras
 * `splash_limit - 2` plantillas, que son las que ya ensena la pantalla de
 * bienvenida, y no dibuja la fila «General».
 */
static void file_new_draw_ex(uiLayout *layout, const bool use_more)
{
  /* `splash_limit = 5` en el Python. Y el icono NO es el mismo en las dos ramas:
   * con `use_more` las plantillas llevan `FILE_NEW` y sin el van sin icono. Lo
   * canto el volcado: dos filas con `icon=BLANK1` donde tenia que poner
   * `icon=FILE_NEW`. */
  const int splash_limit = 5;
  const int template_icon = use_more ? ICON_FILE_NEW : ICON_NONE;

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_DEFAULT);

  if (!use_more) {
    PointerRNA props = layout->op("WM_OT_read_homefile", IFACE_("General"), ICON_NONE);
    if (props.data) {
      RNA_string_set(&props, "app_template", "");
    }
  }

  ListBase templates{};
  BKE_appdir_app_templates(&templates);
  int index = 0;
  LISTBASE_FOREACH (LinkData *, link, &templates) {
    const int i = index++;
    if (use_more && i < splash_limit - 2) {
      continue;
    }
    const char *template_id = static_cast<const char *>(link->data);
    char display_name[FILE_MAXFILE];
    BLI_path_to_display_name(display_name, sizeof(display_name), template_id);
    PointerRNA props = layout->op("WM_OT_read_homefile", IFACE_(display_name), template_icon);
    if (props.data) {
      RNA_string_set(&props, "app_template", template_id);
    }
  }
  LISTBASE_FOREACH (LinkData *, link, &templates) {
    MEM_freeN(link->data);
  }
  BLI_freelistN(&templates);

  uiLayoutSetOperatorContext(layout, WM_OP_EXEC_DEFAULT);
}

static void file_new_draw(const bContext * /*C*/, Menu *menu)
{
  file_new_draw_ex(menu->layout, false);
}

/**
 * `TOPBAR_MT_templates_more`. No esta entre los 133 —el keymap no lo abre— pero
 * su `draw()` era `bpy.types.TOPBAR_MT_file_new.draw_ex(..., use_more=True)`, un
 * metodo de la clase que este mismo commit retira. Si se dejara en Python se
 * quedaria sin nada a lo que llamar, asi que se migra con el. Ademas lo nombra
 * el C++ de la pantalla de bienvenida (`wm_splash_screen.cc`).
 */
static void templates_more_draw(const bContext * /*C*/, Menu *menu)
{
  file_new_draw_ex(menu->layout, true);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name TOPBAR_MT_file_context_menu
 * \{ */

static void file_context_menu_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;

  uiLayoutSetOperatorContext(layout, WM_OP_INVOKE_AREA);
  layout->menu("TOPBAR_MT_file_new",
               CTX_IFACE_(BLT_I18NCONTEXT_ID_WINDOWMANAGER, "New"),
               ICON_FILE_NEW);
  layout->op("WM_OT_open_mainfile", IFACE_("Open..."), ICON_FILE_FOLDER);
  layout->menu("TOPBAR_MT_file_open_recent", std::nullopt, ICON_NONE);

  layout->separator();

  layout->op("WM_OT_link", IFACE_("Link..."), ICON_LINK_BLEND);
  layout->op("WM_OT_append", IFACE_("Append..."), ICON_APPEND_BLEND);

  layout->separator();

  layout->menu("TOPBAR_MT_file_import", std::nullopt, ICON_IMPORT);
  layout->menu("TOPBAR_MT_file_export", std::nullopt, ICON_EXPORT);

  layout->separator();

  layout->op("SCREEN_OT_userpref_show", IFACE_("Preferences..."), ICON_PREFERENCES);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name TOPBAR_MT_file_export
 *
 * Transliteracion de `TOPBAR_MT_file_export.draw()`
 * (`scripts/startup/bl_ui/space_topbar.py:300`), MAS la fila del runtime que el addon
 * del motor le anadia por detras (ver la cabecera del fichero).
 *
 * `bl_owner_use_filter = False` del Python no se transcribe porque no existe: para un
 * `Menu`, `rna_Menu_register` no lo lee y `MenuType` no tiene ese campo. El volcado de
 * registro lo confirma — el bloque de la linea base dice `flag=[]` y `owner_id=''`.
 * \{ */

static void file_export_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;

#ifdef WITH_ALEMBIC
  layout->op("WM_OT_alembic_export", IFACE_("Alembic (.abc)"), ICON_NONE);
#endif
#ifdef WITH_USD
  layout->op("WM_OT_usd_export", IFACE_("Universal Scene Description (.usd*)"), ICON_NONE);
#endif

#ifdef WITH_IO_GREASE_PENCIL
  /* Dependencia de la biblioteca PUGIXML. */
#  ifdef WITH_PUGIXML
  layout->op("WM_OT_grease_pencil_export_svg", IFACE_("Grease Pencil as SVG"), ICON_NONE);
#  endif
  /* Dependencia de la biblioteca HARU. */
#  ifdef WITH_HARU
  layout->op("WM_OT_grease_pencil_export_pdf", IFACE_("Grease Pencil as PDF"), ICON_NONE);
#  endif
#endif

#ifdef WITH_IO_WAVEFRONT_OBJ
  layout->op("WM_OT_obj_export", IFACE_("Wavefront (.obj)"), ICON_NONE);
#endif
#ifdef WITH_IO_PLY
  layout->op("WM_OT_ply_export", IFACE_("Stanford PLY (.ply)"), ICON_NONE);
#endif
#ifdef WITH_IO_STL
  layout->op("WM_OT_stl_export", IFACE_("STL (.stl)"), ICON_NONE);
#endif
#ifdef WITH_COLLADA
  layout->op("WM_OT_collada_export", IFACE_("Collada (.dae) (Legacy)"), ICON_NONE);
#endif

  /* La fila que el addon del motor metia con `append()`, repuesta. Sin ella, Flipendo
   * no tiene por donde exportar el juego como ejecutable autonomo desde el menu, que
   * es la capacidad que distingue a este fork. El operador es nativo desde la
   * migracion de los addons del motor. */
  layout->op("WM_OT_save_as_runtime", IFACE_("Save as game runtime"), ICON_NONE);
}

/** \} */

static const flipendo::MenuDecl topbar_menus[] = {
    {
        /*idname*/ "TOPBAR_MT_file_context_menu",
        /*label*/ N_("File"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ file_context_menu_draw,
    },
    {
        /*idname*/ "TOPBAR_MT_file_new",
        /*label*/ N_("New File"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ file_new_draw,
    },
    {
        /*idname*/ "TOPBAR_MT_templates_more",
        /*label*/ N_("Templates"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ templates_more_draw,
    },
    {
        /*idname*/ "TOPBAR_MT_file_export",
        /*label*/ N_("Export"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ file_export_draw,
    },
};

void topbar_menus_register()
{
  flipendo::menus_register({topbar_menus, ARRAY_SIZE(topbar_menus)});
}

}  // namespace blender::ed::topbar
