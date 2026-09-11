/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BLI_vector.hh"

#include "DNA_space_types.h"

struct ScrArea;
struct SpaceProperties;
struct bContext;
struct PointerRNA;
struct uiLayout;

/**
 * Fills an array with the tab context values for the properties editor. -1 signals a separator.
 *
 * \return The total number of items in the array returned.
 */
blender::Vector<eSpaceButtons_Context> ED_buttons_tabs_list(const SpaceProperties *sbuts,
                                                            bool apply_filter = true);
void ED_buttons_visible_tabs_menu(bContext *C, uiLayout *layout, void * /*arg*/);
void ED_buttons_navbar_menu(bContext *C, uiLayout *layout, void * /*arg*/);
bool ED_buttons_tab_has_search_result(SpaceProperties *sbuts, int index);

void ED_buttons_search_string_set(SpaceProperties *sbuts, const char *value);
int ED_buttons_search_string_length(SpaceProperties *sbuts);
const char *ED_buttons_search_string_get(SpaceProperties *sbuts);

bool ED_buttons_should_sync_with_outliner(const bContext *C,
                                          const SpaceProperties *sbuts,
                                          ScrArea *area);
void ED_buttons_set_context(const bContext *C,
                            SpaceProperties *sbuts,
                            PointerRNA *ptr,
                            int context);

/**
 * Fija la pestana del editor de Propiedades y recalcula su ruta de contexto.
 *
 * Existe para el volcador de interfaz (`--fl-dump-ui-layout`): un panel de la
 * pestana de Material no pasa ni su `poll` mientras el editor este en Objeto,
 * porque `context.material` sale de la ruta que calcula esta llamada. Sin esto,
 * la mitad de `properties_*.py` quedaria sin cubrir por un detalle de contexto.
 *
 * \return `true` si la pestana quedo puesta; `false` si en esta escena no existe.
 */
bool ED_buttons_context_tab_set(const bContext *C, SpaceProperties *sbuts, int context);
