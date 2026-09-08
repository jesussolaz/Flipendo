/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Mapa de teclado, grupo 01: atajos globales de ventana y pantalla (Window,
 * Screen, Screen Editing) y navegacion generica de vistas 2D (View2D).
 * Transliterado de blender_default.py.
 */

#include "FL_keymap_build.hpp"
#include "FL_keymap_params.hpp"

namespace flipendo::keymap {

/* -------------------------------------------------------------------- */
/** \name Window
 *
 * `km_window()` del Python.
 * \{ */

static void km_window(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Window", "EMPTY", "WINDOW");

  if (params.legacy) {
    /* Atajos antiguos. */
    item(km, "wm.save_homefile", ev("U", "PRESS").ctrl());
    item(km, "wm.open_mainfile", ev("F1", "PRESS"));
    item(km, "wm.link", ev("O", "PRESS").ctrl().alt());
    item(km, "wm.append", ev("F1", "PRESS").shift());
    item(km, "wm.save_mainfile", ev("W", "PRESS").ctrl());
    item(km, "wm.save_as_mainfile", ev("F2", "PRESS"));
    item(km, "wm.save_as_mainfile", ev("S", "PRESS").ctrl().alt()).boolean("copy", true);
    item(km, "wm.window_new", ev("W", "PRESS").ctrl().alt());
    item(km, "wm.window_fullscreen_toggle", ev("F11", "PRESS").alt());
    item(km, "wm.doc_view_manual_ui_context", ev("F1", "PRESS").alt());
    item(km, "wm.search_menu", ev("SPACE", "PRESS"));
    item(km, "wm.redraw_timer", ev("T", "PRESS").ctrl().alt());
    item(km, "wm.debug_menu", ev("D", "PRESS").ctrl().alt());
  }

  /* Operaciones de fichero. */
  item_menu(km, "TOPBAR_MT_file_new", ev("N", "PRESS").ctrl());
  item_menu(km, "TOPBAR_MT_file_open_recent", ev("O", "PRESS").shift().ctrl());
  item(km, "wm.open_mainfile", ev("O", "PRESS").ctrl());
  item(km, "wm.save_mainfile", ev("S", "PRESS").ctrl());
  item(km, "wm.save_as_mainfile", ev("S", "PRESS").shift().ctrl());
  item(km, "wm.save_mainfile", ev("S", "PRESS").ctrl().alt()).boolean("incremental", true);
  item(km, "wm.quit_blender", ev("Q", "PRESS").ctrl());

  /* Menu rapido y barra de herramientas. */
  item_menu(km, "SCREEN_MT_user_menu", ev("Q", "PRESS"));

  /* Cambio rapido de editor. La tabla reproduce el generador del Python; se recorre
   * en el mismo orden porque el orden del keymap es semantico. */
  {
    struct EditorKey {
      const char *key;
      const char *space_type;
    };
    static const EditorKey editor_keys[] = {
        {"F1", "FILE_BROWSER"},
        {"F2", "CLIP_EDITOR"},
        {"F3", "NODE_EDITOR"},
        {"F4", "CONSOLE"},
        {"F5", "VIEW_3D"},
        {"F6", "GRAPH_EDITOR"},
        {"F7", "PROPERTIES"},
        {"F8", "SEQUENCE_EDITOR"},
        {"F9", "OUTLINER"},
        {"F10", "IMAGE_EDITOR"},
        {"F11", "TEXT_EDITOR"},
        {"F12", "DOPESHEET_EDITOR"},
    };
    for (const EditorKey &e : editor_keys) {
      item(km, "screen.space_type_set_or_cycle", ev(e.key, "PRESS").shift())
          .enum_("space_type", e.space_type);
    }
  }

  /* Ajustes NDOF. Las divisiones se dejan escritas como en el Python (y en doble
   * precision) para que el valor redondeado a float sea exactamente el mismo. */
  item_panel(km, "USERPREF_PT_ndof_settings", ev("NDOF_BUTTON_MENU", "PRESS"));
  item(km, "wm.context_scale_float", ev("NDOF_BUTTON_PLUS", "PRESS"))
      .string("data_path", "preferences.inputs.ndof_sensitivity")
      .number("value", 1.1f);
  item(km, "wm.context_scale_float", ev("NDOF_BUTTON_MINUS", "PRESS"))
      .string("data_path", "preferences.inputs.ndof_sensitivity")
      .number("value", float(1.0 / 1.1));
  item(km, "wm.context_scale_float", ev("NDOF_BUTTON_PLUS", "PRESS").shift())
      .string("data_path", "preferences.inputs.ndof_sensitivity")
      .number("value", 1.5f);
  item(km, "wm.context_scale_float", ev("NDOF_BUTTON_MINUS", "PRESS").shift())
      .string("data_path", "preferences.inputs.ndof_sensitivity")
      .number("value", float(2.0 / 3.0));
  item(km, "info.reports_display_update", ev("TIMER_REPORT", "ANY").any());

  if (!params.legacy) {
    /* Atajos nuevos. */
    item(km, "wm.doc_view_manual_ui_context", ev("F1", "PRESS"));
    item_panel(km, "TOPBAR_PT_name", ev("F2", "PRESS")).boolean("keep_open", false);
    item(km, "wm.batch_rename", ev("F2", "PRESS").ctrl());
    item(km, "wm.search_menu", ev("F3", "PRESS"));
    item_menu(km, "TOPBAR_MT_file_context_menu", ev("F4", "PRESS"));
    /* Deja pasar el evento cuando no hay sistema de herramientas o no hay reserva. */
    item(km, "wm.toolbar_fallback_pie", ev("W", "PRESS").alt());

    if (params.use_alt_click_leader) {
      /* Alt como "tecla lider". */
      item(km, "wm.toolbar_prompt", ev("LEFT_ALT", "CLICK"));
      item(km, "wm.toolbar_prompt", ev("RIGHT_ALT", "CLICK"));
    }

    if (params.spacebar_action == SpacebarAction::Tool) {
      item(km, "wm.toolbar", ev("SPACE", "PRESS"));
    }
    else if (params.spacebar_action == SpacebarAction::Play) {
      item(km, "wm.toolbar", ev("SPACE", "PRESS").shift());
    }
    else if (params.spacebar_action == SpacebarAction::Search) {
      item(km, "wm.search_menu", ev("SPACE", "PRESS"));
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Screen
 *
 * `km_screen()` del Python.
 * \{ */

static void km_screen(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Screen", "EMPTY", "WINDOW");

  /* Animacion. */
  item(km, "screen.animation_step", ev("TIMER0", "ANY").any());
  item(km, "screen.region_blend", ev("TIMERREGION", "ANY").any());
  /* Pantalla completa y ciclado. */
  item(km, "screen.space_context_cycle", ev("TAB", "PRESS").ctrl()).enum_("direction", "NEXT");
  item(km, "screen.space_context_cycle", ev("TAB", "PRESS").shift().ctrl())
      .enum_("direction", "PREV");
  item(km, "screen.workspace_cycle", ev("PAGE_DOWN", "PRESS").ctrl()).enum_("direction", "NEXT");
  item(km, "screen.workspace_cycle", ev("PAGE_UP", "PRESS").ctrl()).enum_("direction", "PREV");
  /* Vista cuadruple. */
  item(km, "screen.region_quadview", ev("Q", "PRESS").ctrl().alt());
  /* Repetir la ultima orden. */
  item(km, "screen.repeat_last", ev("R", "PRESS").shift().repeat());
  /* Ficheros. */
  item(km, "file.execute", ev("RET", "PRESS"));
  item(km, "file.execute", ev("NUMPAD_ENTER", "PRESS"));
  item(km, "file.cancel", ev("ESC", "PRESS"));
  /* Deshacer del catalogo de recursos: solo existe en el navegador de recursos y
   * tiene que ganar a `ed.undo`, por eso va antes. */
  item(km, "asset.catalog_undo", ev("Z", "PRESS").ctrl().repeat());
  item(km, "asset.catalog_redo", ev("Z", "PRESS").ctrl().shift().repeat());
  /* Deshacer. */
  item(km, "ed.undo", ev("Z", "PRESS").ctrl().repeat());
  item(km, "ed.redo", ev("Z", "PRESS").shift().ctrl().repeat());
  /* Render. */
  item(km, "render.render", ev("F12", "PRESS")).boolean("use_viewport", true);
  item(km, "render.render", ev("F12", "PRESS").ctrl())
      .boolean("animation", true)
      .boolean("use_viewport", true);
  item(km, "render.view_cancel", ev("ESC", "PRESS"));
  item(km, "render.view_show", ev("F11", "PRESS"));
  item(km, "render.play_rendered_anim", ev("F11", "PRESS").ctrl());

  if (!params.legacy) {
    item(km, "screen.screen_full_area", ev("SPACE", "PRESS").ctrl());
    item(km, "screen.screen_full_area", ev("SPACE", "PRESS").ctrl().alt())
        .boolean("use_hide_panels", true);
    item(km, "screen.redo_last", ev("F9", "PRESS"));
  }
  else {
    /* Mapa antiguo. */
    item(km, "ed.undo_history", ev("Z", "PRESS").ctrl().alt());
    item(km, "screen.screen_full_area", ev("UP_ARROW", "PRESS").ctrl());
    item(km, "screen.screen_full_area", ev("DOWN_ARROW", "PRESS").ctrl());
    item(km, "screen.screen_full_area", ev("SPACE", "PRESS").shift());
    item(km, "screen.screen_full_area", ev("F10", "PRESS").alt())
        .boolean("use_hide_panels", true);
    item(km, "screen.screen_set", ev("RIGHT_ARROW", "PRESS").ctrl()).integer("delta", 1);
    item(km, "screen.screen_set", ev("LEFT_ARROW", "PRESS").ctrl()).integer("delta", -1);
    item(km, "screen.screenshot", ev("F3", "PRESS").ctrl());
    item(km, "screen.repeat_history", ev("R", "PRESS").ctrl().alt());
    item(km, "screen.region_flip", ev("F5", "PRESS"));
    item(km, "screen.redo_last", ev("F6", "PRESS"));
    item(km, "script.reload", ev("F8", "PRESS"));
  }

  /* Preferencias. */
  if (!params.legacy) {
    item(km, "screen.userpref_show", ev("COMMA", "PRESS").ctrl());
  }
  else {
    item(km, "screen.userpref_show", ev("U", "PRESS").ctrl().alt());
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Screen Editing
 *
 * `km_screen_editing()` del Python.
 * \{ */

static void km_screen_editing(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(kc, "Screen Editing", "EMPTY", "WINDOW");

  /* Zonas de accion. */
  item(km, "screen.actionzone", ev("LEFTMOUSE", "PRESS")).integer("modifier", 0);
  item(km, "screen.actionzone", ev("LEFTMOUSE", "PRESS").shift()).integer("modifier", 1);
  item(km, "screen.actionzone", ev("LEFTMOUSE", "PRESS").ctrl()).integer("modifier", 2);
  /* Herramientas de pantalla. */
  item(km, "screen.area_split", ev("ACTIONZONE_AREA", "ANY"));
  item(km, "screen.area_join", ev("ACTIONZONE_AREA", "ANY"));
  item(km, "screen.area_dupli", ev("ACTIONZONE_AREA", "ANY").shift());
  item(km, "screen.area_swap", ev("ACTIONZONE_AREA", "ANY").ctrl());
  item(km, "screen.region_scale", ev("ACTIONZONE_REGION", "ANY"));
  item(km, "screen.screen_full_area", ev("ACTIONZONE_FULLSCREEN", "ANY"))
      .boolean("use_hide_panels", true);
  /* Mover el area va despues de las zonas de accion. */
  item(km, "screen.area_move", ev("LEFTMOUSE", "PRESS"));
  item(km, "screen.area_options", ev("RIGHTMOUSE", "PRESS"));

  if (params.legacy) {
    item(km, "wm.context_toggle", ev("F9", "PRESS").alt())
        .string("data_path", "space_data.show_region_header");
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name View2D
 *
 * `km_view2d()` del Python.
 * \{ */

static void km_view2d(wmKeyConfig *kc, const Params & /*params*/)
{
  wmKeyMap *km = keymap(kc, "View2D", "EMPTY", "WINDOW");

  /* Barras de desplazamiento. */
  item(km, "view2d.scroller_activate", ev("LEFTMOUSE", "PRESS"));
  item(km, "view2d.scroller_activate", ev("MIDDLEMOUSE", "PRESS"));
  /* Desplazamiento. */
  item(km, "view2d.pan", ev("MIDDLEMOUSE", "PRESS"));
  item(km, "view2d.pan", ev("MIDDLEMOUSE", "PRESS").shift());
  item(km, "view2d.pan", ev("TRACKPADPAN", "ANY"));
  item(km, "view2d.scroll_right", ev("WHEELDOWNMOUSE", "PRESS").ctrl());
  item(km, "view2d.scroll_right", ev("WHEELRIGHTMOUSE", "PRESS"));
  item(km, "view2d.scroll_left", ev("WHEELUPMOUSE", "PRESS").ctrl());
  item(km, "view2d.scroll_left", ev("WHEELLEFTMOUSE", "PRESS"));
  item(km, "view2d.scroll_down", ev("WHEELDOWNMOUSE", "PRESS").shift());
  item(km, "view2d.scroll_up", ev("WHEELUPMOUSE", "PRESS").shift());
  item(km, "view2d.ndof", ev("NDOF_MOTION", "ANY"));
  /* Zoom a pasos. */
  item(km, "view2d.zoom_out", ev("WHEELOUTMOUSE", "PRESS"));
  item(km, "view2d.zoom_in", ev("WHEELINMOUSE", "PRESS"));
  item(km, "view2d.zoom_out", ev("NUMPAD_MINUS", "PRESS").repeat());
  item(km, "view2d.zoom_in", ev("NUMPAD_PLUS", "PRESS").repeat());
  item(km, "view2d.zoom", ev("TRACKPADPAN", "ANY").ctrl());
  item(km, "view2d.smoothview", ev("TIMER1", "ANY").any());
  /* Desplazar arriba/abajo, solo cuando no hay zoom disponible. */
  item(km, "view2d.scroll_down", ev("WHEELDOWNMOUSE", "PRESS"));
  item(km, "view2d.scroll_up", ev("WHEELUPMOUSE", "PRESS"));
  item(km, "view2d.scroll_right", ev("WHEELDOWNMOUSE", "PRESS"));
  item(km, "view2d.scroll_left", ev("WHEELUPMOUSE", "PRESS"));
  /* Zoom por arrastre y por marco. */
  item(km, "view2d.zoom", ev("MIDDLEMOUSE", "PRESS").ctrl());
  item(km, "view2d.zoom", ev("TRACKPADZOOM", "ANY"));
  item(km, "view2d.zoom_border", ev("B", "PRESS").shift());
}

/** \} */

void register_group_01(wmKeyConfig *kc, const Params &params)
{
  km_window(kc, params);
  km_screen(kc, params);
  km_screen_editing(kc, params);
  km_view2d(kc, params);
}

}  // namespace flipendo::keymap
