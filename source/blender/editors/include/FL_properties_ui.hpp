/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 *
 * Dibujo compartido por paneles de Propiedades y sus clones en otros editores.
 */

#pragma once

struct bContext;
struct ID;
struct Panel;
struct PointerRNA;
struct uiLayout;

namespace flipendo::properties_ui {

/** Dibujo de WORLD_PT_viewport_display, reutilizado como NODE_WORLD_PT_viewport_display. */
void world_viewport_display_draw(const bContext *C, Panel *panel);

/**
 * El `PropertiesAnimationMixin.draw_action_and_slot_selector()` de
 * `bl_ui/space_properties.py`: selector de accion y, si la accion es por capas,
 * el buscador de ranura.
 *
 * Lo hereda casi todo `properties_*.py`; tenerlo aqui es lo que hace que migrar
 * una pestana nueva no obligue a reescribirlo.
 */
void draw_action_and_slot_selector(const bContext *C, uiLayout *layout, ID *id);

/**
 * El nucleo de `rna_prop_ui.draw()`, o sea el mixin `PropertyPanel`: la lista de
 * propiedades personalizadas de un ID, ordenada por nombre, con sus botones de
 * anadir, editar y quitar.
 *
 * `data_path` es la ruta que reciben los operadores `WM_OT_properties_*`
 * (`"object"`, `"collection"`, `"world"`...), o sea el `_context_path` del mixin.
 */
void draw_custom_properties(
    const bContext *C, uiLayout *layout, PointerRNA *ptr, ID *id, const char *data_path);

}  // namespace flipendo::properties_ui
