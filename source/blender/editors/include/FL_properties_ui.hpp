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
 * El `draw()` generico del propio `PropertiesAnimationMixin`, que es lo que usan
 * las pestanas que NO lo sobrescriben (metaball, altavoz, volumen, camara...):
 * una columna alineada con separacion de propiedad y sin decorador, y dentro el
 * selector de accion y ranura.
 *
 * Las pestanas que si lo sobrescriben (malla, rejilla) siguen llamando
 * directamente a `draw_action_and_slot_selector()` con su propia disposicion.
 */
void draw_animation_panel(const bContext *C, uiLayout *layout, ID *id);

/**
 * El nucleo de `rna_prop_ui.draw()`, o sea el mixin `PropertyPanel`: la lista de
 * propiedades personalizadas de un ID, ordenada por nombre, con sus botones de
 * anadir, editar y quitar.
 *
 * `data_path` es la ruta que reciben los operadores `WM_OT_properties_*`
 * (`"object"`, `"collection"`, `"world"`...), o sea el `_context_path` del mixin.
 */
/**
 * Andamio de medicion: monta en la escena el dato que una pestana de datos
 * necesita para que su `poll` diga que si, llamando a los mismos operadores que
 * llamaria el usuario. Lo usa `--fl-ui-scene <spec>` antes de
 * `--fl-dump-ui-layout`, para que el volcado de diseno cubra de verdad los
 * paneles que la escena de fabrica deja en `NO-CUBIERTO motivo=poll`.
 *
 * `spec` es `FAMILIA` o `FAMILIA:VARIANTE`:
 * `METABALL[:BALL|CAPSULE|PLANE|ELLIPSOID|CUBE]`, `SPEAKER[:MUTED]`, `LATTICE`,
 * `VOLUME`, `CURVES`. Devuelve false y escribe el motivo si algo falla; un
 * andamio que falla en silencio deja la pestana sin cubrir y el volcado vuelve a
 * decir `NO-CUBIERTO`, que es un falso verde.
 */
bool scene_setup(bContext *C, const char *spec);

void draw_custom_properties(
    const bContext *C, uiLayout *layout, PointerRNA *ptr, ID *id, const char *data_path);

}  // namespace flipendo::properties_ui
