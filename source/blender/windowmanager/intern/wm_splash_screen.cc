/* SPDX-FileCopyrightText: 2007 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * This file contains the splash screen logic (the `WM_OT_splash` operator).
 *
 * - Loads the splash image.
 * - Displaying version information.
 * - Lists New Files (application templates).
 * - Lists Recent files.
 * - Links to web sites.
 */

#include <cstring>

#include "DNA_screen_types.h"
#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BKE_appdir.hh"
#include "BKE_blender_version.h"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_preferences.h"
#include "BKE_screen.hh"

#include "BLT_translation.hh"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"

#include "ED_datafiles.h"
#include "ED_screen.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface_c.hh"
#include "UI_interface.hh"
#include "UI_interface_icons.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "FL_ui_registry.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "wm.hh"

extern "C" char build_branch[];
extern "C" char build_commit_date[];
extern "C" char build_commit_time[];
extern "C" char build_hash[];

/* -------------------------------------------------------------------- */
/** \name Menus globales historicamente alojados en bl_operators/wm.py
 * \{ */

static void wm_menu_url_preset(
    const bContext *C, uiLayout &layout, const char *text, int icon, const char *type)
{
  PointerRNA props = layout.op("WM_OT_url_open_preset", IFACE_(text), icon);
  RNA_enum_set_identifier(const_cast<bContext *>(C), &props, "type", type);
}

static void wm_menu_url(uiLayout &layout, const char *text, const char *url)
{
  PointerRNA props = layout.op("WM_OT_url_open", IFACE_(text), ICON_URL);
  RNA_string_set(&props, "url", url);
}

static void wm_splash_file_templates_draw(uiLayout &layout)
{
  uiLayoutSetOperatorContext(&layout, WM_OP_INVOKE_DEFAULT);

  PointerRNA props = layout.op("WM_OT_read_homefile", IFACE_("General"), ICON_FILE_NEW);
  RNA_string_set(&props, "app_template", "");

  ListBase templates{};
  BKE_appdir_app_templates(&templates);
  const int template_count = BLI_listbase_count(&templates);
  const int template_limit = template_count > 4 ? 3 : template_count;
  int index = 0;
  LISTBASE_FOREACH (LinkData *, link, &templates) {
    const char *template_id = static_cast<const char *>(link->data);
    /* Igual que TOPBAR_MT_file_new.draw_ex(use_splash=True): tres plantillas y `...`. */
    if (index++ >= template_limit) {
      break;
    }
    char display_name[FILE_MAXFILE];
    BLI_path_to_display_name(display_name, sizeof(display_name), template_id);
    props = layout.op("WM_OT_read_homefile", IFACE_(display_name), ICON_FILE_NEW);
    RNA_string_set(&props, "app_template", template_id);
  }
  if (template_count > 4) {
    layout.menu("TOPBAR_MT_templates_more", IFACE_("..."), ICON_NONE);
  }
  LISTBASE_FOREACH (LinkData *, link, &templates) {
    MEM_freeN(link->data);
  }
  BLI_freelistN(&templates);
  uiLayoutSetOperatorContext(&layout, WM_OP_EXEC_DEFAULT);
}

static void wm_splash_menu_draw(const bContext *C, Menu *menu)
{
  uiLayout &layout = *menu->layout;
  uiLayoutSetOperatorContext(&layout, WM_OP_EXEC_DEFAULT);
  uiLayoutSetEmboss(&layout, blender::ui::EmbossType::Pulldown);

  uiLayout &split = layout.split(0.0f, false);
  uiLayout &col1 = split.column(false);
  col1.label(IFACE_("New File"), ICON_NONE);
  wm_splash_file_templates_draw(col1);

  uiLayout &col2 = split.column(false);
  uiLayout &col2_title = col2.row(false);
  const bool found_recent = uiTemplateRecentFiles(&col2, 5) != 0;
  col2_title.label(IFACE_(found_recent ? "Recent Files" : "Getting Started"), ICON_NONE);
  if (!found_recent) {
    wm_menu_url_preset(C, col2, "Manual", ICON_URL, "MANUAL");
    wm_menu_url(col2, "Tutorials", "https://www.blender.org/tutorials/");
    wm_menu_url(col2, "Support", "https://www.blender.org/support/");
    wm_menu_url(col2, "User Communities", "https://www.blender.org/community/");
    wm_menu_url_preset(C, col2, "Blender Website", ICON_URL, "BLENDER");
  }

  layout.separator();
  uiLayout &bottom = layout.split(0.0f, false);
  uiLayout &bottom_left = bottom.column(false);
  uiLayout &open_row = bottom_left.row(false);
  uiLayoutSetOperatorContext(&open_row, WM_OP_INVOKE_DEFAULT);
  open_row.op("WM_OT_open_mainfile", IFACE_("Open..."), ICON_FILE_FOLDER);
  bottom_left.op("WM_OT_recover_last_session", std::nullopt, ICON_RECOVER_LAST);

  uiLayout &bottom_right = bottom.column(false);
  wm_menu_url_preset(C, bottom_right, "Donate", ICON_FUND, "FUND");
  wm_menu_url_preset(C, bottom_right, "What's New", ICON_URL, "RELEASE_NOTES");
  layout.separator();
  if ((G.f & G_FLAG_INTERNET_ALLOW) == 0 &&
      (G.f & G_FLAG_INTERNET_OVERRIDE_PREF_ANY) != 0)
  {
    layout.label(IFACE_("Running in Offline Mode"), ICON_INTERNET_OFFLINE);
  }
  layout.separator();
}

static void wm_splash_about_menu_draw(const bContext *C, Menu *menu)
{
  uiLayout &layout = *menu->layout;
  uiLayoutSetOperatorContext(&layout, WM_OP_EXEC_DEFAULT);
  uiLayout &split = layout.split(0.65f, false);
  uiLayout &left = split.column(true);
  uiLayoutSetScaleY(&left, 0.8f);
  left.separator(2.5f);

  char text[256];
  SNPRINTF(text, "Date: %s %s", build_commit_date, build_commit_time);
  left.label(text, ICON_NONE);
  SNPRINTF(text, "Hash: %s", build_hash);
  left.label(text, ICON_NONE);
  SNPRINTF(text, "Branch: %s", build_branch);
  left.label(text, ICON_NONE);
  left.separator(2.0f);
  left.label(IFACE_("UPBGE is free software"), ICON_NONE);
  left.label(IFACE_("Licensed under the GNU General Public License"), ICON_NONE);

  uiLayout &right = split.column(true);
  uiLayoutSetEmboss(&right, blender::ui::EmbossType::Pulldown);
  wm_menu_url_preset(C, right, "Donate", ICON_FUND, "FUND");
  wm_menu_url(right, "Release Notes", "https://github.com/UPBGE/upbge/wiki/Release-notes");
  right.separator(2.0f);
  wm_menu_url_preset(C, right, "Credits", ICON_URL, "CREDITS");
  wm_menu_url(right, "License", "https://www.blender.org/about/license/");
  wm_menu_url(right, "Blender Store", "https://store.blender.org");
  wm_menu_url(right, "UPBGE Website", "https://upbge.org");
}

static wmKeyConfig *wm_splash_active_keyconfig(wmWindowManager *wm)
{
  wmKeyConfig *keyconf = static_cast<wmKeyConfig *>(
      BLI_findstring(&wm->keyconfigs, U.keyconfigstr, offsetof(wmKeyConfig, idname)));
  return keyconf ? keyconf : wm->defaultconf;
}

static void wm_splash_quick_setup_menu_draw(const bContext *C, Menu *menu)
{
  uiLayout &layout = *menu->layout;
  uiLayoutSetOperatorContext(&layout, WM_OP_EXEC_DEFAULT);

  /* La busqueda de preferencias anteriores sigue perteneciendo al operador que las copia. La UI
   * conserva el contrato seguro: solo ofrece importarlas si el propio operador da poll positivo. */
  wmOperatorType *copy_prev = WM_operatortype_find("PREFERENCES_OT_copy_prev", true);
  const bool can_import = copy_prev != nullptr && WM_operator_poll(const_cast<bContext *>(C), copy_prev);
  if (can_import) {
    layout.label(IFACE_("Import Preferences From Previous Version"), ICON_NONE);
    uiLayout &margin = layout.split(0.20f, false);
    margin.label("", ICON_NONE);
    uiLayout &content = margin.split(0.73f, false).column(false);
    content.op(copy_prev, IFACE_("Import Previous Preferences"), ICON_NONE);
    layout.separator();
    layout.separator(1.0f, LayoutSeparatorType::Line);
  }
  layout.label(IFACE_(can_import ? "Create New Preferences" : "Quick Setup"), ICON_NONE);

  uiLayout &margin = layout.split(0.20f, false);
  margin.label("", ICON_NONE);
  uiLayout &col = margin.split(0.73f, false).column(false);
  uiLayoutSetPropSep(&col, true);
  uiLayoutSetPropDecorate(&col, false);

  PointerRNA view_ptr = RNA_pointer_create_discrete(nullptr, &RNA_PreferencesView, &U);
  col.prop(&view_ptr, "language", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  uiLayout &theme = col.column(false, IFACE_("Theme"));
  MenuType *theme_menu = WM_menutype_find("USERPREF_MT_interface_theme_presets", true);
  const char *theme_label = (theme_menu != nullptr && !STREQ(theme_menu->label, "Presets")) ?
                                theme_menu->label :
                                "Blender Dark";
  theme.menu("USERPREF_MT_interface_theme_presets", IFACE_(theme_label), ICON_NONE);
  col.separator();

  wmWindowManager *wm = CTX_wm_manager(C);
  wmKeyConfig *keyconf = wm ? wm_splash_active_keyconfig(wm) : nullptr;
  uiLayout &keymap = col.column(false, IFACE_("Keymap"));
  keymap.menu("USERPREF_MT_keyconfigs",
              keyconf && keyconf->idname[0] ? keyconf->idname : IFACE_("Blender"),
              ICON_NONE);
  if (keyconf != nullptr) {
    PointerRNA keyconf_ptr = RNA_pointer_create_discrete(nullptr, &RNA_KeyConfig, keyconf);
    PointerRNA prefs_ptr = RNA_pointer_get(&keyconf_ptr, "preferences");
    if (prefs_ptr.data != nullptr) {
      if (RNA_struct_find_property(&prefs_ptr, "select_mouse")) {
        col.row(false).prop(&prefs_ptr,
                            "select_mouse",
                            UI_ITEM_R_EXPAND,
                            IFACE_("Mouse Select"),
                            ICON_NONE);
      }
      if (RNA_struct_find_property(&prefs_ptr, "spacebar_action")) {
        col.row(false).prop(
            &prefs_ptr, "spacebar_action", UI_ITEM_NONE, IFACE_("Spacebar Action"), ICON_NONE);
      }
    }
  }
  uiLayout &save = col.column(false);
  save.separator(2.0f);
  save.op("WM_OT_save_userpref",
          IFACE_(can_import ? "Save New Preferences" : "Continue"),
          ICON_NONE);
  layout.separator(2.0f);
}

struct RegionToggleInfo {
  int region_type;
  const char *property;
  const char *label;
};

static constexpr RegionToggleInfo region_toggle_info[] = {
    {RGN_TYPE_TOOLS, "show_region_toolbar", N_("Tools")},
    {RGN_TYPE_UI, "show_region_ui", N_("Sidebar")},
    {RGN_TYPE_HEADER, "show_region_header", N_("Header")},
    {RGN_TYPE_FOOTER, "show_region_footer", N_("Footer")},
    {RGN_TYPE_ASSET_SHELF, "show_region_asset_shelf", N_("Asset Shelf")},
    {RGN_TYPE_CHANNELS, "show_region_channels", N_("Channels")},
};

static bool wm_region_toggle_pie_poll(const bContext *C, MenuType * /*mt*/)
{
  return CTX_wm_space_data(C) != nullptr;
}

static void wm_region_toggle_pie_menu_draw(const bContext *C, Menu *menu)
{
  uiLayout &pie = menu->layout->menu_pie();
  ScrArea *area = CTX_wm_area(C);
  SpaceLink *space = CTX_wm_space_data(C);
  bScreen *screen = CTX_wm_screen(C);
  if (area == nullptr || space == nullptr) {
    return;
  }
  PointerRNA space_ptr = RNA_pointer_create_discrete(screen ? &screen->id : nullptr, &RNA_Space, space);

  blender::Vector<const RegionToggleInfo *> slots[8];
  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    const RegionToggleInfo *info = nullptr;
    for (const RegionToggleInfo &candidate : region_toggle_info) {
      if (candidate.region_type == region->regiontype) {
        info = &candidate;
        break;
      }
    }
    if (info == nullptr || RNA_struct_find_property(&space_ptr, info->property) == nullptr) {
      continue;
    }
    int slot = -1;
    if (region->alignment == RGN_ALIGN_LEFT) {
      slot = 0;
    }
    else if (region->alignment == RGN_ALIGN_RIGHT) {
      slot = 1;
    }
    else if (region->alignment == RGN_ALIGN_BOTTOM) {
      slot = 2;
    }
    else if (region->alignment == RGN_ALIGN_TOP) {
      slot = 3;
    }
    if (slot >= 0) {
      slots[slot].append(info);
    }
  }

  static constexpr int alternatives[4][3] = {{4, 6, 1}, {5, 7, 0}, {6, 7, 3}, {4, 5, 2}};
  for (int slot = 0; slot < 4; slot++) {
    while (slots[slot].size() > 1) {
      bool moved = false;
      for (const int other : alternatives[slot]) {
        if (slots[other].is_empty()) {
          slots[other].append(slots[slot].pop_last());
          moved = true;
          break;
        }
      }
      if (!moved) {
        break;
      }
    }
  }
  for (int slot = 0; slot < 4; slot++) {
    while (slots[slot].size() > 1) {
      bool moved = false;
      for (int other = 4; other < 8; other++) {
        if (slots[other].is_empty()) {
          slots[other].append(slots[slot].pop_last());
          moved = true;
          break;
        }
      }
      if (!moved) {
        break;
      }
    }
  }

  for (int slot = 0; slot < 8; slot++) {
    if (slots[slot].is_empty()) {
      pie.separator();
      continue;
    }
    const RegionToggleInfo &info = *slots[slot].first();
    const bool visible = RNA_boolean_get(&space_ptr, info.property);
    PointerRNA props = pie.op("WM_OT_context_toggle",
                              IFACE_(info.label),
                              visible ? ICON_CHECKBOX_HLT : ICON_CHECKBOX_DEHLT);
    std::string data_path = std::string("space_data.") + info.property;
    RNA_string_set(&props, "data_path", data_path.c_str());
  }
}

void wm_menutypes_register()
{
  static const flipendo::MenuDecl menus[] = {
      {"WM_MT_splash_quick_setup", N_("Quick Setup"), nullptr, nullptr, wm_splash_quick_setup_menu_draw},
      {"WM_MT_splash", N_("Splash"), nullptr, nullptr, wm_splash_menu_draw},
      {"WM_MT_splash_about", N_("About"), nullptr, nullptr, wm_splash_about_menu_draw},
      {"WM_MT_region_toggle_pie",
       N_("Region Toggle"),
       nullptr,
       nullptr,
       wm_region_toggle_pie_menu_draw,
       wm_region_toggle_pie_poll},
  };
  flipendo::menus_register({menus, ARRAY_SIZE(menus)});
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Splash Screen
 * \{ */

static void wm_block_splash_close(bContext *C, void *arg_block, void * /*arg*/)
{
  wmWindow *win = CTX_wm_window(C);
  UI_popup_block_close(C, win, static_cast<uiBlock *>(arg_block));
}

static void wm_block_splash_add_label(uiBlock *block, const char *label, int x, int y)
{
  if (!(label && label[0])) {
    return;
  }

  UI_block_emboss_set(block, blender::ui::EmbossType::None);

  uiBut *but = uiDefBut(
      block, UI_BTYPE_LABEL, 0, label, 0, y, x, UI_UNIT_Y, nullptr, 0, 0, std::nullopt);
  UI_but_drawflag_disable(but, UI_BUT_TEXT_LEFT);
  UI_but_drawflag_enable(but, UI_BUT_TEXT_RIGHT);

  /* Regardless of theme, this text should always be bright white. */
  uchar color[4] = {255, 255, 255, 255};
  UI_but_color_set(but, color);

  UI_block_emboss_set(block, blender::ui::EmbossType::Emboss);
}

#ifndef WITH_HEADLESS
static void wm_block_splash_image_roundcorners_add(ImBuf *ibuf)
{
  uchar *rct = ibuf->byte_buffer.data;
  if (!rct) {
    return;
  }

  bTheme *btheme = UI_GetTheme();
  const float roundness = btheme->tui.wcol_menu_back.roundness * UI_SCALE_FAC;
  const int size = roundness * 20;

  if (size < ibuf->x && size < ibuf->y) {
    /* Y-axis initial offset. */
    rct += 4 * (ibuf->y - size) * ibuf->x;

    for (int y = 0; y < size; y++) {
      for (int x = 0; x < size; x++, rct += 4) {
        const float pixel = 1.0 / size;
        const float u = pixel * x;
        const float v = pixel * y;
        const float distance = sqrt(u * u + v * v);

        /* Pointer offset to the alpha value of pixel. */
        /* NOTE: the left corner is flipped in the X-axis. */
        const int offset_l = 4 * (size - x - x - 1) + 3;
        const int offset_r = 4 * (ibuf->x - size) + 3;

        if (distance > 1.0) {
          rct[offset_l] = 0;
          rct[offset_r] = 0;
        }
        else {
          /* Create a single pixel wide transition for anti-aliasing.
           * Invert the distance and map its range [0, 1] to [0, pixel]. */
          const float fac = (1.0 - distance) * size;

          if (fac > 1.0) {
            continue;
          }

          const uchar alpha = unit_float_to_uchar_clamp(fac);
          rct[offset_l] = alpha;
          rct[offset_r] = alpha;
        }
      }

      /* X-axis offset to the next row. */
      rct += 4 * (ibuf->x - size);
    }
  }
}
#endif /* !WITH_HEADLESS */

static ImBuf *wm_block_splash_image(int width, int *r_height)
{
  ImBuf *ibuf = nullptr;
  int height = 0;
#ifndef WITH_HEADLESS
  if (U.app_template[0] != '\0') {
    char splash_filepath[FILE_MAX];
    char template_directory[FILE_MAX];
    if (BKE_appdir_app_template_id_search(
            U.app_template, template_directory, sizeof(template_directory)))
    {
      BLI_path_join(splash_filepath, sizeof(splash_filepath), template_directory, "splash.png");
      ibuf = IMB_load_image_from_filepath(splash_filepath, IB_byte_data);
    }
  }

  if (ibuf == nullptr) {
    const char *custom_splash_path = BLI_getenv("BLENDER_CUSTOM_SPLASH");
    if (custom_splash_path) {
      ibuf = IMB_load_image_from_filepath(custom_splash_path, IB_byte_data);
    }
  }

  if (ibuf == nullptr) {
    const uchar *splash_data = (const uchar *)datatoc_splash_png;
    size_t splash_data_size = datatoc_splash_png_size;
    ibuf = IMB_load_image_from_memory(
        splash_data, splash_data_size, IB_byte_data, "<splash screen>");
  }

  if (ibuf) {
    ibuf->planes = 32; /* The image might not have an alpha channel. */
    height = (width * ibuf->y) / ibuf->x;
    if (width != ibuf->x || height != ibuf->y) {
      IMB_scale(ibuf, width, height, IMBScaleFilter::Box, false);
    }

    wm_block_splash_image_roundcorners_add(ibuf);
    IMB_premultiply_alpha(ibuf);
  }

#else
  UNUSED_VARS(width);
#endif
  *r_height = height;
  return ibuf;
}

static ImBuf *wm_block_splash_banner_image(int *r_width,
                                           int *r_height,
                                           int max_width,
                                           int max_height)
{
  ImBuf *ibuf = nullptr;
  int height = 0;
  int width = max_width;
#ifndef WITH_HEADLESS

  const char *custom_splash_path = BLI_getenv("BLENDER_CUSTOM_SPLASH_BANNER");
  if (custom_splash_path) {
    ibuf = IMB_load_image_from_filepath(custom_splash_path, IB_byte_data);
  }

  if (!ibuf) {
    return nullptr;
  }

  ibuf->planes = 32; /* The image might not have an alpha channel. */

  width = ibuf->x;
  height = ibuf->y;
  if (width > 0 && height > 0 && (width > max_width || height > max_height)) {
    const float splash_ratio = max_width / float(max_height);
    const float banner_ratio = ibuf->x / float(ibuf->y);

    if (banner_ratio > splash_ratio) {
      /* The banner is wider than the splash image. */
      width = max_width;
      height = max_width / banner_ratio;
    }
    else if (banner_ratio < splash_ratio) {
      /* The banner is taller than the splash image. */
      height = max_height;
      width = max_height * banner_ratio;
    }
    else {
      width = max_width;
      height = max_height;
    }
    if (width != ibuf->x || height != ibuf->y) {
      IMB_scale(ibuf, width, height, IMBScaleFilter::Box, false);
    }
  }

  IMB_premultiply_alpha(ibuf);

#else
  UNUSED_VARS(max_height);
#endif
  *r_height = height;
  *r_width = width;
  return ibuf;
}

/**
 * Close the splash when opening a file-selector.
 */
static void wm_block_splash_close_on_fileselect(bContext *C, void *arg1, void * /*arg2*/)
{
  wmWindow *win = CTX_wm_window(C);
  if (!win) {
    return;
  }

  /* Check for the event as this will run before the new window/area has been created. */
  bool has_fileselect = false;
  LISTBASE_FOREACH (const wmEvent *, event, &win->runtime->event_queue) {
    if (event->type == EVT_FILESELECT) {
      has_fileselect = true;
      break;
    }
  }

  if (has_fileselect) {
    wm_block_splash_close(C, arg1, nullptr);
  }
}

#if defined(__APPLE__)
/* Check if Blender is running under Rosetta for the purpose of displaying a splash screen warning.
 * From Apple's WWDC 2020 Session - Explore the new system architecture of Apple Silicon Macs.
 * Time code: 14:31 - https://developer.apple.com/videos/play/wwdc2020/10686/ */

#  include <sys/sysctl.h>

static int is_using_macos_rosetta()
{
  int ret = 0;
  size_t size = sizeof(ret);

  if (sysctlbyname("sysctl.proc_translated", &ret, &size, nullptr, 0) != -1) {
    return ret;
  }
  /* If "sysctl.proc_translated" is not present then must be native. */
  if (errno == ENOENT) {
    return 0;
  }
  return -1;
}
#endif /* __APPLE__ */

static uiBlock *wm_block_splash_create(bContext *C, ARegion *region, void * /*arg*/)
{
  const uiStyle *style = UI_style_get_dpi();

  uiBlock *block = UI_block_begin(C, region, "splash", blender::ui::EmbossType::Emboss);

  /* Note on #UI_BLOCK_NO_WIN_CLIP, the window size is not always synchronized
   * with the OS when the splash shows, window clipping in this case gives
   * ugly results and clipping the splash isn't useful anyway, just disable it #32938. */
  UI_block_flag_enable(block, UI_BLOCK_LOOP | UI_BLOCK_KEEP_OPEN | UI_BLOCK_NO_WIN_CLIP);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);

  int splash_width = style->widget.points * 45 * UI_SCALE_FAC;
  CLAMP_MAX(splash_width, WM_window_native_pixel_x(CTX_wm_window(C)) * 0.7f);
  int splash_height;

  /* Would be nice to support caching this, so it only has to be re-read (and likely resized) on
   * first draw or if the image changed. */
  ImBuf *ibuf = wm_block_splash_image(splash_width, &splash_height);
  /* This should never happen, if it does - don't crash. */
  if (LIKELY(ibuf)) {
    uiBut *but = uiDefButImage(
        block, ibuf, 0, 0.5f * U.widget_unit, splash_width, splash_height, nullptr);

    UI_but_func_set(but, wm_block_splash_close, block, nullptr);

    wm_block_splash_add_label(block,
                              BKE_upbge_version_string(),
                              splash_width - 8.0 * UI_SCALE_FAC,
                              splash_height - 13.0 * UI_SCALE_FAC);
  }

  /* Banner image passed through the environment, to overlay on the splash and
   * indicate a custom Blender version. Transparency can be used. To replace the
   * full splash screen, see BLENDER_CUSTOM_SPLASH. */
  int banner_width = 0;
  int banner_height = 0;
  ImBuf *bannerbuf = wm_block_splash_banner_image(
      &banner_width, &banner_height, splash_width, splash_height);
  if (bannerbuf) {
    uiBut *banner_but = uiDefButImage(
        block, bannerbuf, 0, 0.5f * U.widget_unit, banner_width, banner_height, nullptr);

    UI_but_func_set(banner_but, wm_block_splash_close, block, nullptr);
  }

  const int layout_margin_x = UI_SCALE_FAC * 26;
  uiLayout *layout = UI_block_layout(block,
                                     UI_LAYOUT_VERTICAL,
                                     UI_LAYOUT_PANEL,
                                     layout_margin_x,
                                     0,
                                     splash_width - (layout_margin_x * 2),
                                     UI_SCALE_FAC * 110,
                                     0,
                                     style);

  MenuType *mt;

  /* Draw setup screen if no preferences have been saved yet. */
  if (!blender::bke::preferences::exists()) {
    mt = WM_menutype_find("WM_MT_splash_quick_setup", true);

    /* The #UI_BLOCK_QUICK_SETUP flag prevents the button text from being left-aligned,
     * as it is for all menus due to the #UI_BLOCK_LOOP flag, see in #ui_def_but. */
    UI_block_flag_enable(block, UI_BLOCK_QUICK_SETUP);
  }
  else {
    mt = WM_menutype_find("WM_MT_splash", true);
  }

  UI_block_func_set(block, wm_block_splash_close_on_fileselect, block, nullptr);

  if (mt) {
    UI_menutype_draw(C, mt, layout);
  }

/* Displays a warning if blender is being emulated via Rosetta (macOS) or XTA (Windows) */
#if defined(__APPLE__) || defined(_M_X64)
#  if defined(__APPLE__)
  if (is_using_macos_rosetta() > 0)
#  elif defined(_M_X64)
  const char *proc_id = BLI_getenv("PROCESSOR_IDENTIFIER");
  if (proc_id && strncmp(proc_id, "ARM", 3) == 0)
#  endif
  {
    layout->separator(2.0f, LayoutSeparatorType::Line);

    uiLayout *split = &layout->split(0.725, true);
    uiLayout *row1 = &split->row(true);
    uiLayout *row2 = &split->row(true);

    row1->label(RPT_("Intel binary detected. Expect reduced performance."), ICON_ERROR);

    PointerRNA op_ptr = row2->op("WM_OT_url_open",
                                 CTX_IFACE_(BLT_I18NCONTEXT_OPERATOR_DEFAULT, "Learn More"),
                                 ICON_URL,
                                 WM_OP_INVOKE_DEFAULT,
                                 UI_ITEM_NONE);
#  if defined(__APPLE__)
    RNA_string_set(
        &op_ptr,
        "url",
        "https://docs.blender.org/manual/en/latest/getting_started/installing/macos.html");
#  elif defined(_M_X64)
    RNA_string_set(
        &op_ptr,
        "url",
        "https://docs.blender.org/manual/en/latest/getting_started/installing/windows.html");
#  endif

    layout->separator();
  }
#endif

  UI_block_bounds_set_centered(block, 0);

  return block;
}

static wmOperatorStatus wm_splash_invoke(bContext *C,
                                         wmOperator * /*op*/,
                                         const wmEvent * /*event*/)
{
  UI_popup_block_invoke(C, wm_block_splash_create, nullptr, nullptr);

  return OPERATOR_FINISHED;
}

void WM_OT_splash(wmOperatorType *ot)
{
  ot->name = "Splash Screen";
  ot->idname = "WM_OT_splash";
  ot->description = "Open the splash screen with release info";

  ot->invoke = wm_splash_invoke;
  ot->poll = WM_operator_winactive;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Splash Screen: About
 * \{ */

static uiBlock *wm_block_about_create(bContext *C, ARegion *region, void * /*arg*/)
{
  const uiStyle *style = UI_style_get_dpi();
  const int dialog_width = style->widget.points * 42 * UI_SCALE_FAC;

  uiBlock *block = UI_block_begin(C, region, "about", blender::ui::EmbossType::Emboss);

  UI_block_flag_enable(block, UI_BLOCK_KEEP_OPEN | UI_BLOCK_LOOP | UI_BLOCK_NO_WIN_CLIP);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);

  uiLayout *layout = UI_block_layout(
      block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, 0, 0, dialog_width, 0, 0, style);

/* Blender logo. */
#ifndef WITH_HEADLESS
  constexpr bool show_color = false;
  const float size = 0.2f * dialog_width;

  ImBuf *ibuf = UI_svg_icon_bitmap(ICON_BLENDER_LOGO_LARGE, size, show_color);

  if (ibuf) {
    bTheme *btheme = UI_GetTheme();
    const uchar *color = btheme->tui.wcol_menu_back.text_sel;

    /* The top margin. */
    uiLayout *row = &layout->row(false);
    row->separator(0.2f);

    /* The logo image. */
    row = &layout->row(false);
    uiLayoutSetAlignment(row, UI_LAYOUT_ALIGN_LEFT);
    uiDefButImage(block, ibuf, 0, U.widget_unit, ibuf->x, ibuf->y, show_color ? nullptr : color);

    /* Padding below the logo. */
    row = &layout->row(false);
    row->separator(2.7f);
  }
#endif /* !WITH_HEADLESS */

  uiLayout *col = &layout->column(true);

  uiItemL_ex(col, BLI_strdupcat("UPBGE ", BKE_upbge_version_string()), ICON_NONE, true, false);
  uiItemL_ex(col, BLI_strdupcat("Based on Blender ", BKE_blender_version_string()), ICON_NONE, true, false);

  MenuType *mt = WM_menutype_find("WM_MT_splash_about", true);
  if (mt) {
    UI_menutype_draw(C, mt, col);
  }

  UI_block_bounds_set_centered(block, 22 * UI_SCALE_FAC);

  return block;
}

static wmOperatorStatus wm_splash_about_invoke(bContext *C,
                                               wmOperator * /*op*/,
                                               const wmEvent * /*event*/)
{
  UI_popup_block_invoke(C, wm_block_about_create, nullptr, nullptr);

  return OPERATOR_FINISHED;
}

void WM_OT_splash_about(wmOperatorType *ot)
{
  ot->name = "About UPBGE";
  ot->idname = "WM_OT_splash_about";
  ot->description = "Open a window with information about UPBGE";

  ot->invoke = wm_splash_about_invoke;
  ot->poll = WM_operator_winactive;
}

/** \} */
