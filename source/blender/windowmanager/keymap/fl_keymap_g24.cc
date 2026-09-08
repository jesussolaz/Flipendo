/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Mapa de teclado, grupo 24: herramientas de la vista 3D -- edicion de armature
 * (roll, tamano y envolvente de hueso, extrusiones), anadir primitiva en modo
 * objeto y edicion de malla (extrusiones, inset, bisel, cortes, cuchillo, poly
 * build, spin y suavizado).
 * Transliterado de blender_default.py.
 */

#include "FL_keymap_build.hpp"
#include "FL_keymap_params.hpp"

namespace flipendo::keymap {

/**
 * `{..., **params.tool_modifier}` del Python.
 *
 * Sale en casi todos los keymaps de este grupo, por eso esta en un sitio unico.
 *
 * TODO(keymap): `params.tool_modifier` vale `{"alt": -1}` cuando se selecciona con el
 * boton izquierdo Y esta activa la preferencia "Alt para herramienta o cursor"
 * (`params.tool_modifier_alt_any`); es decir, Alt en CUALQUIER estado. `Event` solo
 * sabe "modificador pulsado" o `.any()` (TODOS los modificadores en cualquier
 * estado), no un modificador suelto en KM_ANY. El API de abajo si lo soporta
 * (`KMI_PARAMS_MOD_TO_ANY` en WM_keymap.hh); lo que falta es exponerlo en el
 * andamiaje. Mientras tanto el evento se emite tal cual, sin Alt: con los parametros
 * por defecto `tool_modifier` esta vacio y el resultado es identico al baseline; con
 * esa preferencia activa, el atajo no respondera con Alt pulsado.
 */
static Event with_tool_modifier(const Params & /*params*/, const Event &event)
{
  return event;
}

/* -------------------------------------------------------------------- */
/** \name Herramientas: edicion de armature
 * \{ */

static void km_3d_view_tool_edit_armature_roll(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Armature, Roll", "VIEW_3D", "WINDOW");

  item(km, "transform.transform", with_tool_modifier(params, params.tool_maybe_tweak_event))
      .boolean("release_confirm", true)
      .enum_("mode", "BONE_ROLL");
}

static void km_3d_view_tool_edit_armature_bone_size(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Armature, Bone Size", "VIEW_3D", "WINDOW");

  item(km, "transform.transform", with_tool_modifier(params, params.tool_maybe_tweak_event))
      .boolean("release_confirm", true)
      .enum_("mode", "BONE_ENVELOPE");
}

static void km_3d_view_tool_edit_armature_bone_envelope(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Armature, Bone Envelope", "VIEW_3D", "WINDOW");

  item(km, "transform.bbone_resize", with_tool_modifier(params, params.tool_maybe_tweak_event))
      .boolean("release_confirm", true);
}

static void km_3d_view_tool_edit_armature_extrude(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Armature, Extrude", "VIEW_3D", "WINDOW");

  item(km, "armature.extrude_move", with_tool_modifier(params, params.tool_maybe_tweak_event))
      .sub("TRANSFORM_OT_translate")
      .boolean("release_confirm", true);
}

static void km_3d_view_tool_edit_armature_extrude_to_cursor(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(
      kc, "3D View Tool: Edit Armature, Extrude to Cursor", "VIEW_3D", "WINDOW");

  item(km, "armature.click_extrude", with_tool_modifier(params, ev(params.tool_mouse, "PRESS")));
  /* Solo con seleccion por el boton derecho: ahi el izquierdo esta libre para el
   * arrastre. Con seleccion por el izquierdo el Python no anade este atajo. */
  if (params.select_mouse_right) {
    item(km, "transform.translate", ev(params.tool_mouse, "CLICK_DRAG"))
        .boolean("release_confirm", true);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Herramientas: modo objeto
 * \{ */

static void km_3d_view_tool_interactive_add(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Object, Add Primitive", "VIEW_3D", "WINDOW");

  /* El Python pone `{"any": True}` si `tool_modifier` ya lleva Alt, y si no
   * `any_except("alt")`: todos los modificadores en cualquier estado MENOS Alt, que
   * tiene que estar sin pulsar (Ctrl y Shift le hacen falta a la herramienta para
   * ajustar y centrar, por eso van en KM_ANY).
   *
   * TODO(keymap): la segunda rama no se puede escribir con `Event`, que no sabe poner
   * un modificador suelto en KM_ANY (ver `with_tool_modifier`). Se emite `.any()`,
   * que conserva Ctrl/Shift/Cmd/Hyper pero acepta ademas Alt pulsado; el baseline
   * espera alt=0 aqui. La primera rama si es exacta. */
  item(km, "view3d.interactive_add", Event(params.tool_maybe_tweak_event).any())
      .boolean("wait_for_input", false);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Herramientas: edicion de malla
 * \{ */

static void km_3d_view_tool_edit_mesh_extrude_region(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Mesh, Extrude Region", "VIEW_3D", "WINDOW");

  item(km, "mesh.extrude_context_move", with_tool_modifier(params, params.tool_maybe_tweak_event))
      .sub("TRANSFORM_OT_translate")
      .boolean("release_confirm", true);
}

static void km_3d_view_tool_edit_mesh_extrude_manifold(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Mesh, Extrude Manifold", "VIEW_3D", "WINDOW");

  Item it = item(
      km, "mesh.extrude_manifold", with_tool_modifier(params, params.tool_maybe_tweak_event));
  it.sub("MESH_OT_extrude_region").boolean("use_dissolve_ortho_edges", true);
  /* TODO(keymap): falta `constraint_axis = (False, False, True)` dentro de
   * TRANSFORM_OT_translate. `Props` (las propiedades de un paso de macro) solo tiene
   * boolean/integer/number/string/enum_; el array de booleanos existe en `Item`
   * (`boolean_array`) pero no en `Props`. Sin el, la extrusion no queda restringida
   * al eje Z de la normal. */
  it.sub("TRANSFORM_OT_translate")
      .boolean("release_confirm", true)
      .boolean("use_automerge_and_split", true)
      .enum_("orient_type", "NORMAL");
}

static void km_3d_view_tool_edit_mesh_extrude_along_normals(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(
      kc, "3D View Tool: Edit Mesh, Extrude Along Normals", "VIEW_3D", "WINDOW");

  item(km,
       "mesh.extrude_region_shrink_fatten",
       with_tool_modifier(params, params.tool_maybe_tweak_event))
      .sub("TRANSFORM_OT_shrink_fatten")
      .boolean("release_confirm", true);
}

static void km_3d_view_tool_edit_mesh_extrude_individual(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Mesh, Extrude Individual", "VIEW_3D", "WINDOW");

  item(km, "mesh.extrude_faces_move", with_tool_modifier(params, params.tool_maybe_tweak_event))
      .sub("TRANSFORM_OT_shrink_fatten")
      .boolean("release_confirm", true);
}

static void km_3d_view_tool_edit_mesh_extrude_to_cursor(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Mesh, Extrude to Cursor", "VIEW_3D", "WINDOW");

  /* Sin `tool_modifier`: este atajo se queda con toda la entrada. */
  item(km, "mesh.dupli_extrude_cursor", ev(params.tool_mouse, "PRESS"));
  /* Igual que en el armature: el arrastre con el izquierdo solo se anade cuando la
   * seleccion va por el boton derecho. */
  if (params.select_mouse_right) {
    item(km, "transform.translate", ev(params.tool_mouse, "CLICK_DRAG"))
        .boolean("release_confirm", true);
  }
}

static void km_3d_view_tool_edit_mesh_inset_faces(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Mesh, Inset Faces", "VIEW_3D", "WINDOW");

  item(km, "mesh.inset", with_tool_modifier(params, params.tool_maybe_tweak_event))
      .boolean("release_confirm", true);
}

static void km_3d_view_tool_edit_mesh_bevel(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Mesh, Bevel", "VIEW_3D", "WINDOW");

  item(km, "mesh.bevel", with_tool_modifier(params, params.tool_maybe_tweak_event))
      .boolean("release_confirm", true);
}

static void km_3d_view_tool_edit_mesh_loop_cut(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Mesh, Loop Cut", "VIEW_3D", "WINDOW");

  /* Sin `tool_modifier`: este atajo se queda con toda la entrada. */
  item(km, "mesh.loopcut_slide", ev(params.tool_mouse, "PRESS"))
      .sub("TRANSFORM_OT_edge_slide")
      .boolean("release_confirm", true);
}

static void km_3d_view_tool_edit_mesh_offset_edge_loop_cut(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap(
      kc, "3D View Tool: Edit Mesh, Offset Edge Loop Cut", "VIEW_3D", "WINDOW");

  /* Sin `tool_modifier`: este atajo se queda con toda la entrada. */
  item(km, "mesh.offset_edge_loops_slide", ev(params.tool_mouse, "PRESS"));
}

static void km_3d_view_tool_edit_mesh_knife(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Mesh, Knife", "VIEW_3D", "WINDOW");

  /* Sin `tool_modifier`: este atajo se queda con toda la entrada. */
  item(km, "mesh.knife_tool", ev(params.tool_mouse, "PRESS")).boolean("wait_for_input", false);
}

static void km_3d_view_tool_edit_mesh_bisect(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Mesh, Bisect", "VIEW_3D", "WINDOW");

  /* Sin `tool_modifier`: este atajo se queda con toda la entrada. */
  item(km, "mesh.bisect", params.tool_maybe_tweak_event);
}

static void km_3d_view_tool_edit_mesh_poly_build(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Mesh, Poly Build", "VIEW_3D", "WINDOW");

  /* Sin `tool_modifier`: estos atajos se quedan con toda la entrada. */
  item(km, "mesh.polybuild_extrude_at_cursor_move", ev(params.tool_mouse, "PRESS"))
      .sub("TRANSFORM_OT_translate")
      .boolean("release_confirm", true);
  item(km, "mesh.polybuild_face_at_cursor_move", ev(params.tool_mouse, "PRESS").ctrl())
      .sub("TRANSFORM_OT_translate")
      .boolean("release_confirm", true);
  item(km, "mesh.polybuild_delete_at_cursor", ev(params.tool_mouse, "CLICK").shift());
}

static void km_3d_view_tool_edit_mesh_spin(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Mesh, Spin", "VIEW_3D", "WINDOW");

  item(km, "mesh.spin", with_tool_modifier(params, params.tool_maybe_tweak_event));
}

static void km_3d_view_tool_edit_mesh_spin_duplicate(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Mesh, Spin Duplicates", "VIEW_3D", "WINDOW");

  item(km, "mesh.spin", with_tool_modifier(params, params.tool_maybe_tweak_event))
      .boolean("dupli", true);
}

static void km_3d_view_tool_edit_mesh_smooth(wmKeyConfig *kc, const Params &params)
{
  wmKeyMap *km = keymap_tool(kc, "3D View Tool: Edit Mesh, Smooth", "VIEW_3D", "WINDOW");

  item(km, "mesh.vertices_smooth", with_tool_modifier(params, params.tool_maybe_tweak_event))
      .boolean("wait_for_input", false);
}

/** \} */

void register_group_24(wmKeyConfig *kc, const Params &params)
{
  km_3d_view_tool_edit_armature_roll(kc, params);
  km_3d_view_tool_edit_armature_bone_size(kc, params);
  km_3d_view_tool_edit_armature_bone_envelope(kc, params);
  km_3d_view_tool_edit_armature_extrude(kc, params);
  km_3d_view_tool_edit_armature_extrude_to_cursor(kc, params);
  km_3d_view_tool_interactive_add(kc, params);
  km_3d_view_tool_edit_mesh_extrude_region(kc, params);
  km_3d_view_tool_edit_mesh_extrude_manifold(kc, params);
  km_3d_view_tool_edit_mesh_extrude_along_normals(kc, params);
  km_3d_view_tool_edit_mesh_extrude_individual(kc, params);
  km_3d_view_tool_edit_mesh_extrude_to_cursor(kc, params);
  km_3d_view_tool_edit_mesh_inset_faces(kc, params);
  km_3d_view_tool_edit_mesh_bevel(kc, params);
  km_3d_view_tool_edit_mesh_loop_cut(kc, params);
  km_3d_view_tool_edit_mesh_offset_edge_loop_cut(kc, params);
  km_3d_view_tool_edit_mesh_knife(kc, params);
  km_3d_view_tool_edit_mesh_bisect(kc, params);
  km_3d_view_tool_edit_mesh_poly_build(kc, params);
  km_3d_view_tool_edit_mesh_spin(kc, params);
  km_3d_view_tool_edit_mesh_spin_duplicate(kc, params);
  km_3d_view_tool_edit_mesh_smooth(kc, params);
}

}  // namespace flipendo::keymap
