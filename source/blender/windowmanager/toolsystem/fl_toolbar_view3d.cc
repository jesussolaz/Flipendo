/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Barra de herramientas de la vista 3D.
 *
 * Transliteracion de `VIEW3D_PT_tools_active`
 * (`scripts/startup/bl_ui/space_toolsystem_toolbar.py:3237`).
 *
 * Es la barra grande del catalogo: veintidos modos, del objeto a la escultura de
 * curvas, y las nueve entradas comunes que trece de ellos repiten. Las herramientas no
 * se declaran aqui (viven en los `fl_tool_defs_*` correspondientes), aqui solo se
 * COLOCAN: que entra en cada modo, en que orden, que va agrupado bajo un mismo boton y
 * que bloques dependen del contexto.
 *
 * Los filtros son parte de esa colocacion, y por eso las siete funciones `poll_*` que
 * los `fl_tool_defs_*` dejaron declaradas se implementan en este fichero: en el Python
 * tampoco son datos de la herramienta, sino el `if` de una lambda dentro de la lista de
 * un modo.
 */

#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "BKE_context.hh"
#include "BKE_modifier.hh"
#include "BKE_paint.hh"

#include "BLT_translation.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "fl_tool_defs_annotate.hh"
#include "fl_tool_defs_edit_mesh.hh"
#include "fl_tool_defs_edit_misc.hh"
#include "fl_tool_defs_grease_pencil_misc.hh"
#include "fl_tool_defs_grease_pencil_paint.hh"
#include "fl_tool_defs_paint.hh"
#include "fl_tool_defs_sculpt.hh"
#include "fl_tool_defs_transform.hh"
#include "fl_tool_defs_uv_curves_sculpt.hh"
#include "fl_tool_defs_view3d.hh"
#include "fl_tool_defs_view3d_add.hh"
#include "fl_toolbar_view3d.hh"

namespace flipendo::toolsystem {

/* -------------------------------------------------------------------- */
/** \name Auxiliares de contexto
 * \{ */

/**
 * `context.sculpt_object` y `context.pose_object`.
 *
 * Ninguno de los dos tiene accesor propio en C: son miembros del contexto de pantalla y
 * se piden por nombre, igual que desde Python. Resolverlos a mano (objeto activo mas
 * comprobacion de modo, o armadura en pose del objeto activo) duplicaria la logica de
 * `screen_context.cc`, y una copia asi se desincroniza en silencio en cuanto alli
 * cambie que cuenta como objeto en pose.
 */
static const Object *context_object(const bContext *C, const char *member)
{
  const PointerRNA ptr = CTX_data_pointer_get_type(C, member, &RNA_Object);
  return static_cast<const Object *>(ptr.data);
}

/**
 * La malla del objeto activo, o `nullptr` si el activo no es una malla.
 *
 * Es el `ob and ob.type == 'MESH'` con el que empiezan los tres filtros de los modos de
 * pintado. En el Python el `ob.data` de una malla nunca es nulo; aqui se comprueba
 * igualmente porque en C si puede serlo mientras el fichero se esta cargando.
 */
static const Mesh *active_mesh(const bContext *C)
{
  const Object *ob = CTX_data_active_object(C);
  if (ob == nullptr || ob->type != OB_MESH) {
    return nullptr;
  }
  return static_cast<const Mesh *>(ob->data);
}

/** Las mascaras de seleccion de los modos de lapiz de cera: objeto de lapiz de cera
 * activo y alguno de los tres bits (punto, trazo o segmento) puesto. */
static bool grease_pencil_select_mask(const bContext *C, const bool vertex_masks)
{
  const Object *ob = CTX_data_active_object(C);
  const ToolSettings *tool_settings = CTX_data_tool_settings(C);
  if (ob == nullptr || ob->type != OB_GREASE_PENCIL || tool_settings == nullptr) {
    return false;
  }
  if (vertex_masks) {
    return (tool_settings->gpencil_selectmode_vertex &
            (GP_VERTEX_MASK_SELECTMODE_POINT | GP_VERTEX_MASK_SELECTMODE_STROKE |
             GP_VERTEX_MASK_SELECTMODE_SEGMENT)) != 0;
  }
  return (tool_settings->gpencil_selectmode_sculpt &
          (GP_SCULPT_MASK_SELECTMODE_POINT | GP_SCULPT_MASK_SELECTMODE_STROKE |
           GP_SCULPT_MASK_SELECTMODE_SEGMENT)) != 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Filtros de contexto
 *
 * Las siete `poll_*` que los `fl_tool_defs_*` declararon sin cuerpo, mas la lambda del
 * pintado de pesos, que en el Python no tiene nombre.
 *
 * Las ocho empiezan reproduciendo el `if context is None` del Python, que NO es una
 * guarda defensiva: es lo que decide que ve el volcado del catalogo cuando no hay
 * ventana, o sea lo que hay escrito en la linea base. Siete dicen que SI y el de
 * vertices de lapiz de cera dice que NO; cambiar cualquiera de los dos valores cambia
 * la linea base.
 * \{ */

namespace defs_sculpt {

bool poll_dyntopo(const bContext *C)
{
  if (C == nullptr) {
    return true;
  }
  const Object *ob = context_object(C, "sculpt_object");
  return ob != nullptr && BKE_object_sculpt_use_dyntopo(ob);
}

/* Ojo con el caso sin objeto esculpido: aqui devuelve SI, al reves que `poll_dyntopo`,
 * que sin objeto se queda en el `and` y devuelve NO. No es un descuido del Python: es
 * exactamente lo que hace que la linea base liste los dos pinceles de multiresolucion y
 * no el de densidad. */
bool poll_multires(const bContext *C)
{
  if (C == nullptr) {
    return true;
  }
  const Object *ob = context_object(C, "sculpt_object");
  if (ob == nullptr) {
    return true;
  }
  return BKE_modifiers_findby_type(ob, eModifierType_Multires) != nullptr;
}

}  // namespace defs_sculpt

namespace defs_vertex_paint {

bool poll_select_mask(const bContext *C)
{
  if (C == nullptr) {
    return true;
  }
  const Mesh *mesh = active_mesh(C);
  return mesh != nullptr &&
         (mesh->editflag & (ME_EDIT_PAINT_FACE_SEL | ME_EDIT_PAINT_VERT_SEL)) != 0;
}

}  // namespace defs_vertex_paint

namespace defs_texture_paint {

/* Solo la mascara de CARAS: pintar textura se hace por cara, no por vertice. */
bool poll_select_mask(const bContext *C)
{
  if (C == nullptr) {
    return true;
  }
  const Mesh *mesh = active_mesh(C);
  return mesh != nullptr && (mesh->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
}

}  // namespace defs_texture_paint

namespace defs_weight_paint {

/* En el Python devuelve la lista de herramientas de seleccion o una tupla vacia; aqui
 * es un filtro sobre esa misma lista, que es lo mismo dicho de la otra forma. La
 * segunda condicion no es redundante con la del bloque de transformar: al pintar pesos
 * se puede estar sobre la malla con una armadura en pose seleccionada, y entonces las
 * de seleccion sirven para elegir hueso. */
bool poll_select_tools(const bContext *C)
{
  if (C == nullptr) {
    return true;
  }
  const Mesh *mesh = active_mesh(C);
  if (mesh != nullptr &&
      (mesh->editflag & (ME_EDIT_PAINT_FACE_SEL | ME_EDIT_PAINT_VERT_SEL)) != 0)
  {
    return true;
  }
  return context_object(C, "pose_object") != nullptr;
}

}  // namespace defs_weight_paint

namespace defs_grease_pencil_sculpt {

bool poll_select_mask(const bContext *C)
{
  if (C == nullptr) {
    return true;
  }
  return grease_pencil_select_mask(C, /*vertex_masks*/ false);
}

}  // namespace defs_grease_pencil_sculpt

namespace defs_grease_pencil_vertex {

/* El unico de los siete que sin contexto devuelve NO. La diferencia con el de escultura
 * no es cosmetica: es la que hace que la linea base no liste aqui las de seleccion. */
bool poll_select_mask(const bContext *C)
{
  if (C == nullptr) {
    return false;
  }
  return grease_pencil_select_mask(C, /*vertex_masks*/ true);
}

}  // namespace defs_grease_pencil_vertex

/**
 * `context is None or context.pose_object`, el filtro del bloque de cursor y
 * transformar del pintado de pesos.
 *
 * Es la unica lambda de la barra que no tiene detras un `poll_*` con nombre en el
 * Python, asi que se queda local a este fichero: no la comparte nadie.
 */
static bool poll_pose_object(const bContext *C)
{
  if (C == nullptr) {
    return true;
  }
  return context_object(C, "pose_object") != nullptr;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name El pincel generico de la barra
 * \{ */

/**
 * `VIEW3D_PT_tools_active._brush_tool`.
 *
 * No pertenece a ningun `_defs_*`: es una `ToolDef` de la propia clase de la barra, y
 * el Python la repite tal cual en el editor de imagen. Por eso se declara aqui, y no en
 * el catalogo compartido, donde chocaria con `defs_texture_paint::brush`, que tambien
 * dice `builtin.brush` pero con otra etiqueta y otro icono.
 */
static const ToolDecl brush_tool = {
    /*idname*/ "builtin.brush",
    /*label*/ N_("Brush"),
    /*description*/ nullptr,
    /*description_fn*/ nullptr,
    /*icon*/ "brush.generic",
    /*cursor*/ nullptr,
    /*gizmo_group*/ nullptr,
    /*gizmo_properties*/ {},
    /*keymap_name*/ nullptr,
    /*brush_type*/ nullptr,
    /*data_block*/ nullptr,
    /*op*/ nullptr,
    /*options*/ TOOL_OPTION_USE_BRUSHES,
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Tablas de herramientas
 *
 * `ToolEntry::tools` es un `Span`, y un `Span` necesita un array al que apuntar; por
 * eso hasta la herramienta suelta necesita su tabla de un elemento. Las que llevan
 * varias son los grupos con ciclo: en el Python, una tupla dentro de la lista del modo.
 *
 * El orden DENTRO de un grupo es el que se recorre al ciclar y el que sale en la linea
 * base, asi que se copia del Python tal cual. Ojo con la seleccion, que aqui va
 * tweak/caja/circulo/lazo y en el editor de nodos tweak/caja/lazo/circulo.
 * \{ */

/* `_tools_select`: encabeza casi todos los modos y ademas es el grupo de reserva de la
 * vista 3D (`tool_fallback_id` apunta a `builtin.select`). */
static const ToolDecl *tools_select[] = {
    &defs_view3d_select::select,
    &defs_view3d_select::box,
    &defs_view3d_select::circle,
    &defs_view3d_select::lasso,
};

/* `_tools_annotate`. */
static const ToolDecl *tools_annotate[] = {
    &defs_annotate::scribble,
    &defs_annotate::line,
    &defs_annotate::poly,
    &defs_annotate::eraser,
};

/* `_tools_transform`. Son CUATRO entradas, no una: mover, rotar, el grupo de escalar y
 * transformar. La jaula de escalado solo comparte boton con la escala. */
static const ToolDecl *tool_translate[] = {&defs_transform::translate};
static const ToolDecl *tool_rotate[] = {&defs_transform::rotate};
static const ToolDecl *tools_scale[] = {&defs_transform::scale, &defs_transform::scale_cage};
static const ToolDecl *tool_transform[] = {&defs_transform::transform};

/* La escala SOLA, sin la jaula. Solo la usa el modo escultura, que no monta
 * `_tools_transform` sino sus cuatro herramientas sueltas. */
static const ToolDecl *tool_scale[] = {&defs_transform::scale};

static const ToolDecl *tool_shear[] = {&defs_transform::shear};
static const ToolDecl *tool_bend[] = {&defs_transform::bend};

static const ToolDecl *tool_cursor[] = {&defs_view3d_generic::cursor};
static const ToolDecl *tool_ruler[] = {&defs_view3d_generic::ruler};

static const ToolDecl *tool_brush[] = {&brush_tool};

/* `_tools_view3d_add`: en el Python entra en la lista SIN desempaquetar, asi que las
 * cinco primitivas comparten un solo boton. */
static const ToolDecl *tools_view3d_add[] = {
    &defs_view3d_add::cube_add,
    &defs_view3d_add::cone_add,
    &defs_view3d_add::cylinder_add,
    &defs_view3d_add::uv_sphere_add,
    &defs_view3d_add::ico_sphere_add,
};

/* `_tools_grease_pencil_primitives`. */
static const ToolDecl *tools_grease_pencil_primitives[] = {
    &defs_grease_pencil_paint::box,
    &defs_grease_pencil_paint::circle,
    &defs_grease_pencil_paint::line,
    &defs_grease_pencil_paint::polyline,
    &defs_grease_pencil_paint::arc,
    &defs_grease_pencil_paint::curve,
};

/* Pose. */
static const ToolDecl *tools_pose[] = {
    &defs_pose::breakdown,
    &defs_pose::push,
    &defs_pose::relax,
};

/* Edicion de armadura. Ojo al orden del grupo de huesos: tamano primero y envoltura
 * despues, al reves que en la cabecera de `_defs_edit_armature`. */
static const ToolDecl *tool_armature_roll[] = {&defs_edit_armature::roll};
static const ToolDecl *tools_armature_bone[] = {
    &defs_edit_armature::bone_size,
    &defs_edit_armature::bone_envelope,
};
static const ToolDecl *tools_armature_extrude[] = {
    &defs_edit_armature::extrude,
    &defs_edit_armature::extrude_cursor,
};

/* Edicion de malla. */
static const ToolDecl *tools_mesh_extrude[] = {
    &defs_edit_mesh::extrude,
    &defs_edit_mesh::extrude_manifold,
    &defs_edit_mesh::extrude_normals,
    &defs_edit_mesh::extrude_individual,
    &defs_edit_mesh::extrude_cursor,
};
static const ToolDecl *tool_mesh_inset[] = {&defs_edit_mesh::inset};
static const ToolDecl *tool_mesh_bevel[] = {&defs_edit_mesh::bevel};
static const ToolDecl *tools_mesh_loopcut[] = {
    &defs_edit_mesh::loopcut_slide,
    &defs_edit_mesh::offset_edge_loops_slide,
};
static const ToolDecl *tools_mesh_knife[] = {&defs_edit_mesh::knife, &defs_edit_mesh::bisect};
static const ToolDecl *tool_mesh_poly_build[] = {&defs_edit_mesh::poly_build};
static const ToolDecl *tool_mesh_spin[] = {&defs_edit_mesh::spin};
static const ToolDecl *tools_mesh_smooth[] = {
    &defs_edit_mesh::vertex_smooth,
    &defs_edit_mesh::vertex_randomize,
};
static const ToolDecl *tools_mesh_slide[] = {
    &defs_edit_mesh::edge_slide,
    &defs_edit_mesh::vert_slide,
};
static const ToolDecl *tools_mesh_shrink_fatten[] = {
    &defs_edit_mesh::shrink_fatten,
    &defs_edit_mesh::push_pull,
};
/* El sesgado de aqui es el generico de `_defs_transform`, compartido con los demas
 * modos; el del lapiz de cera es otra herramienta con el mismo idname. */
static const ToolDecl *tools_mesh_shear[] = {&defs_transform::shear, &defs_edit_mesh::tosphere};
static const ToolDecl *tools_mesh_rip[] = {&defs_edit_mesh::rip_region, &defs_edit_mesh::rip_edge};

/* Edicion de curva. `curve_radius` y `tilt` las comparte con EDIT_CURVES, y el radio
 * ademas con EDIT_GREASE_PENCIL: es la misma declaracion, con el mismo keymap "Edit
 * Curve", en los tres. */
static const ToolDecl *tool_curve_draw[] = {&defs_edit_curve::draw};
static const ToolDecl *tool_curve_pen[] = {&defs_edit_curve::pen};
static const ToolDecl *tools_curve_extrude[] = {
    &defs_edit_curve::extrude,
    &defs_edit_curve::extrude_cursor,
};
static const ToolDecl *tool_curve_radius[] = {&defs_edit_curve::curve_radius};
static const ToolDecl *tool_curve_tilt[] = {&defs_edit_curve::tilt};
static const ToolDecl *tool_curve_randomize[] = {&defs_edit_curve::curve_vertex_randomize};

/* Edicion de curvas (el objeto nuevo). Su dibujo comparte idname e icono con el de
 * curva, pero es otra declaracion y otro keymap. */
static const ToolDecl *tool_curves_draw[] = {&defs_edit_curves::draw};

static const ToolDecl *tool_select_text[] = {&defs_edit_text::select_text};

/* Edicion de lapiz de cera. */
static const ToolDecl *tools_grease_pencil_shear[] = {
    &defs_grease_pencil_edit::shear,
    &defs_edit_mesh::tosphere,
};
static const ToolDecl *tool_grease_pencil_interpolate[] = {&defs_grease_pencil_edit::interpolate};
static const ToolDecl *tool_grease_pencil_texture_gradient[] = {
    &defs_grease_pencil_edit::texture_gradient,
};

/* Escultura. */
static const ToolDecl *tool_sculpt_paint[] = {&defs_sculpt::paint};
static const ToolDecl *tool_sculpt_mask[] = {&defs_sculpt::mask};
static const ToolDecl *tool_sculpt_draw_face_sets[] = {&defs_sculpt::draw_face_sets};
static const ToolDecl *tool_sculpt_dyntopo_density[] = {&defs_sculpt::dyntopo_density};
static const ToolDecl *tool_sculpt_multires_eraser[] = {&defs_sculpt::multires_eraser};
static const ToolDecl *tool_sculpt_multires_smear[] = {&defs_sculpt::multires_smear};
static const ToolDecl *tools_sculpt_mask[] = {
    &defs_sculpt::mask_border,
    &defs_sculpt::mask_lasso,
    &defs_sculpt::mask_line,
    &defs_sculpt::mask_polyline,
};
static const ToolDecl *tools_sculpt_hide[] = {
    &defs_sculpt::hide_border,
    &defs_sculpt::hide_lasso,
    &defs_sculpt::hide_line,
    &defs_sculpt::hide_polyline,
};
static const ToolDecl *tools_sculpt_face_set[] = {
    &defs_sculpt::face_set_box,
    &defs_sculpt::face_set_lasso,
    &defs_sculpt::face_set_line,
    &defs_sculpt::face_set_polyline,
};
static const ToolDecl *tools_sculpt_trim[] = {
    &defs_sculpt::trim_box,
    &defs_sculpt::trim_lasso,
    &defs_sculpt::trim_line,
    &defs_sculpt::trim_polyline,
};
static const ToolDecl *tool_sculpt_project_line[] = {&defs_sculpt::project_line};
static const ToolDecl *tool_sculpt_mesh_filter[] = {&defs_sculpt::mesh_filter};
static const ToolDecl *tool_sculpt_cloth_filter[] = {&defs_sculpt::cloth_filter};
static const ToolDecl *tool_sculpt_color_filter[] = {&defs_sculpt::color_filter};
static const ToolDecl *tool_sculpt_face_set_edit[] = {&defs_sculpt::face_set_edit};
static const ToolDecl *tool_sculpt_mask_by_color[] = {&defs_sculpt::mask_by_color};

/* Escultura de lapiz de cera. */
static const ToolDecl *tool_grease_pencil_sculpt_clone[] = {&defs_grease_pencil_sculpt::clone};

/* Pintado de textura. */
static const ToolDecl *tool_texture_paint_blur[] = {&defs_texture_paint::blur};
static const ToolDecl *tool_texture_paint_smear[] = {&defs_texture_paint::smear};
static const ToolDecl *tool_texture_paint_clone[] = {&defs_texture_paint::clone};
static const ToolDecl *tool_texture_paint_fill[] = {&defs_texture_paint::fill};
static const ToolDecl *tool_texture_paint_mask[] = {&defs_texture_paint::mask};

/* Pintado de vertices. */
static const ToolDecl *tool_vertex_paint_blur[] = {&defs_vertex_paint::blur};
static const ToolDecl *tool_vertex_paint_average[] = {&defs_vertex_paint::average};
static const ToolDecl *tool_vertex_paint_smear[] = {&defs_vertex_paint::smear};

/* Pintado de pesos. */
static const ToolDecl *tool_weight_paint_blur[] = {&defs_weight_paint::blur};
static const ToolDecl *tool_weight_paint_average[] = {&defs_weight_paint::average};
static const ToolDecl *tool_weight_paint_smear[] = {&defs_weight_paint::smear};
static const ToolDecl *tool_weight_paint_gradient[] = {&defs_weight_paint::gradient};
static const ToolDecl *tools_weight_paint_sample[] = {
    &defs_weight_paint::sample_weight,
    &defs_weight_paint::sample_weight_group,
};

/* Pintura de lapiz de cera. */
static const ToolDecl *tool_grease_pencil_erase[] = {&defs_grease_pencil_paint::erase};
static const ToolDecl *tool_grease_pencil_fill[] = {&defs_grease_pencil_paint::fill};
static const ToolDecl *tool_grease_pencil_trim[] = {&defs_grease_pencil_paint::trim};
static const ToolDecl *tool_grease_pencil_eyedropper[] = {&defs_grease_pencil_paint::eyedropper};
/* La interpolacion del modo de PINTURA. Comparte idname con la de edicion y son dos
 * declaraciones distintas, con keymaps distintos; confundirlas deja un modo sin atajo. */
static const ToolDecl *tool_grease_pencil_paint_interpolate[] = {
    &defs_grease_pencil_paint::interpolate,
};

/* Peso y vertices de lapiz de cera. */
static const ToolDecl *tool_grease_pencil_weight_blur[] = {&defs_grease_pencil_weight::blur};
static const ToolDecl *tool_grease_pencil_weight_average[] = {&defs_grease_pencil_weight::average};
static const ToolDecl *tool_grease_pencil_weight_smear[] = {&defs_grease_pencil_weight::smear};
static const ToolDecl *tool_grease_pencil_vertex_blur[] = {&defs_grease_pencil_vertex::blur};
static const ToolDecl *tool_grease_pencil_vertex_average[] = {&defs_grease_pencil_vertex::average};
static const ToolDecl *tool_grease_pencil_vertex_smear[] = {&defs_grease_pencil_vertex::smear};
static const ToolDecl *tool_grease_pencil_vertex_replace[] = {&defs_grease_pencil_vertex::replace};

/* Escultura de curvas. */
static const ToolDecl *tool_curves_sculpt_select[] = {&defs_curves_sculpt::select};
static const ToolDecl *tool_curves_sculpt_density[] = {&defs_curves_sculpt::density};
static const ToolDecl *tool_curves_sculpt_add[] = {&defs_curves_sculpt::add};
static const ToolDecl *tool_curves_sculpt_delete[] = {&defs_curves_sculpt::delete_};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Las entradas de cada modo
 *
 * Una entrada por elemento de la lista del Python: una herramienta suelta, un grupo con
 * ciclo, o `{}` para el `None` que separa bloques. Las que llevan `poll` son las
 * lambdas y los llamables que el Python mete dentro de la lista.
 * \{ */

/**
 * `_tools_default`: las nueve entradas con que empiezan trece de los veintidos modos.
 *
 * Tiene que ser un macro y no una tabla porque `ModeTools::entries` es un unico `Span`
 * y dos arrays de C no se concatenan. La alternativa (copiar las nueve entradas en cada
 * uno de los trece modos) es justo el dato duplicado que acaba desincronizandose en
 * cuanto alguien cambie una sola de ellas.
 */
#define FL_VIEW3D_TOOLS_DEFAULT                                                                  \
  {span(tools_select)}, {span(tool_cursor)}, {}, {span(tool_translate)}, {span(tool_rotate)},     \
      {span(tools_scale)}, {span(tool_transform)}, {}, {span(tools_annotate)}, {span(tool_ruler)}

static const ToolEntry object_entries[] = {
    FL_VIEW3D_TOOLS_DEFAULT,
    {},
    {span(tools_view3d_add)},
};

static const ToolEntry pose_entries[] = {
    FL_VIEW3D_TOOLS_DEFAULT,
    {},
    {span(tools_pose)},
};

static const ToolEntry edit_armature_entries[] = {
    FL_VIEW3D_TOOLS_DEFAULT,
    {},
    {span(tool_armature_roll)},
    {span(tools_armature_bone)},
    {},
    {span(tools_armature_extrude)},
    {span(tool_shear)},
};

static const ToolEntry edit_mesh_entries[] = {
    FL_VIEW3D_TOOLS_DEFAULT,
    {},
    {span(tools_view3d_add)},
    {},
    {span(tools_mesh_extrude)},
    {span(tool_mesh_inset)},
    {span(tool_mesh_bevel)},
    {span(tools_mesh_loopcut)},
    {span(tools_mesh_knife)},
    {span(tool_mesh_poly_build)},
    {span(tool_mesh_spin)},
    {span(tools_mesh_smooth)},
    {span(tools_mesh_slide)},
    {span(tools_mesh_shrink_fatten)},
    {span(tools_mesh_shear)},
    {span(tools_mesh_rip)},
};

static const ToolEntry edit_curve_entries[] = {
    FL_VIEW3D_TOOLS_DEFAULT,
    {},
    {span(tool_curve_draw)},
    {span(tool_curve_pen)},
    {span(tools_curve_extrude)},
    {},
    {span(tool_curve_radius)},
    {span(tool_curve_tilt)},
    {},
    {span(tool_shear)},
    {span(tool_curve_randomize)},
};

static const ToolEntry edit_curves_entries[] = {
    FL_VIEW3D_TOOLS_DEFAULT,
    {},
    {span(tool_curves_draw)},
    {},
    {span(tool_curve_radius)},
    {span(tool_curve_tilt)},
};

/* EDIT_SURFACE, EDIT_METABALL y EDIT_LATTICE. En el Python son tres listas escritas por
 * separado con exactamente el mismo contenido; aqui es una sola tabla que los tres
 * modos referencian, para que no puedan divergir por descuido. */
static const ToolEntry default_and_shear_entries[] = {
    FL_VIEW3D_TOOLS_DEFAULT,
    {},
    {span(tool_shear)},
};

/* El unico modo que no empieza por las de seleccion: al editar texto se selecciona con
 * el cursor de escritura, no con un lazo. */
static const ToolEntry edit_text_entries[] = {
    {span(tool_select_text)},
    {span(tool_cursor)},
    {},
    {span(tools_annotate)},
    {span(tool_ruler)},
};

/* Repite `_tools_default` a mano hasta las de transformar, pero NO lleva regla: por eso
 * no puede usar el macro. */
static const ToolEntry edit_grease_pencil_entries[] = {
    {span(tools_select)},
    {span(tool_cursor)},
    {},
    {span(tool_translate)},
    {span(tool_rotate)},
    {span(tools_scale)},
    {span(tool_transform)},
    {},
    {span(tool_curve_radius)},
    {span(tool_bend)},
    {span(tools_grease_pencil_shear)},
    {},
    {span(tool_grease_pencil_interpolate)},
    {},
    {span(tool_grease_pencil_texture_gradient)},
    {},
    {span(tools_annotate)},
};

/* El Python vuelve a escribir la lista entera en vez de desempaquetar `_tools_default`,
 * pero es exactamente la misma. */
static const ToolEntry edit_pointcloud_entries[] = {
    FL_VIEW3D_TOOLS_DEFAULT,
};

/* Los siete pinceles salen de la enumeracion `ParticleEdit.tool`, no de una tabla. */
static const ToolEntry particle_entries[] = {
    {span(tools_select)},
    {span(tool_cursor)},
    {},
    {/*tools*/ {}, /*poll*/ nullptr, /*generated*/ &defs_particle::generate_from_brushes},
};

static const ToolEntry sculpt_entries[] = {
    {span(tool_brush)},
    {span(tool_sculpt_paint)},
    {span(tool_sculpt_mask)},
    {span(tool_sculpt_draw_face_sets)},
    {span(tool_sculpt_dyntopo_density), /*poll*/ defs_sculpt::poll_dyntopo},
    /* La lambda de multiresolucion devuelve una TUPLA de dos, y el `yield from` del
     * Python la desgrana: son dos entradas sueltas con el mismo filtro, no un grupo con
     * ciclo. Agruparlas esconderia un pincel detras del otro. */
    {span(tool_sculpt_multires_eraser), /*poll*/ defs_sculpt::poll_multires},
    {span(tool_sculpt_multires_smear), /*poll*/ defs_sculpt::poll_multires},
    {},
    {span(tools_sculpt_mask)},
    {span(tools_sculpt_hide)},
    {span(tools_sculpt_face_set)},
    {span(tools_sculpt_trim)},
    {span(tool_sculpt_project_line)},
    {},
    {span(tool_sculpt_mesh_filter)},
    {span(tool_sculpt_cloth_filter)},
    {span(tool_sculpt_color_filter)},
    {},
    {span(tool_sculpt_face_set_edit)},
    {span(tool_sculpt_mask_by_color)},
    {},
    /* Escultura no monta `_tools_transform`, sino sus cuatro herramientas sueltas: aqui
     * la escala va SIN la jaula de escalado. */
    {span(tool_translate)},
    {span(tool_rotate)},
    {span(tool_scale)},
    {span(tool_transform)},
    {},
    {span(tools_annotate)},
};

static const ToolEntry sculpt_grease_pencil_entries[] = {
    {span(tool_brush)},
    {span(tool_grease_pencil_sculpt_clone)},
    {},
    {span(tools_annotate)},
    {span(tools_select), /*poll*/ defs_grease_pencil_sculpt::poll_select_mask},
};

static const ToolEntry paint_texture_entries[] = {
    {span(tool_brush)},
    {span(tool_texture_paint_blur)},
    {span(tool_texture_paint_smear)},
    {span(tool_texture_paint_clone)},
    {span(tool_texture_paint_fill)},
    {span(tool_texture_paint_mask)},
    {},
    {span(tools_select), /*poll*/ defs_texture_paint::poll_select_mask},
    {span(tools_annotate)},
};

static const ToolEntry paint_vertex_entries[] = {
    {span(tool_brush)},
    {span(tool_vertex_paint_blur)},
    {span(tool_vertex_paint_average)},
    {span(tool_vertex_paint_smear)},
    {},
    {span(tools_select), /*poll*/ defs_vertex_paint::poll_select_mask},
    {span(tools_annotate)},
};

static const ToolEntry paint_weight_entries[] = {
    {span(tool_brush)},
    {span(tool_weight_paint_blur)},
    {span(tool_weight_paint_average)},
    {span(tool_weight_paint_smear)},
    {span(tool_weight_paint_gradient)},
    {},
    {span(tools_weight_paint_sample)},
    {},
    /* El bloque de la lambda son SEIS entradas (cursor, separador y las cuatro de
     * transformar) y el filtro se aplica por entrada, asi que hay que repetirlo en las
     * seis. Ponerlo solo en la primera dejaria el separador y las de transformar
     * visibles sin armazon en pose. */
    {span(tool_cursor), /*poll*/ poll_pose_object},
    {/*tools*/ {}, /*poll*/ poll_pose_object},
    {span(tool_translate), /*poll*/ poll_pose_object},
    {span(tool_rotate), /*poll*/ poll_pose_object},
    {span(tools_scale), /*poll*/ poll_pose_object},
    {span(tool_transform), /*poll*/ poll_pose_object},
    {},
    /* `poll_select_tools` entra en la lista como llamable suelto, no como lambda, pero
     * hace lo mismo: devolver las de seleccion o nada. */
    {span(tools_select), /*poll*/ defs_weight_paint::poll_select_tools},
    {span(tools_annotate)},
};

/* El unico modo que empieza por el cursor: el pincel va detras. */
static const ToolEntry paint_grease_pencil_entries[] = {
    {span(tool_cursor)},
    {},
    {span(tool_brush)},
    {span(tool_grease_pencil_erase)},
    {span(tool_grease_pencil_fill)},
    {span(tools_grease_pencil_primitives)},
    {},
    {span(tool_grease_pencil_trim)},
    {},
    {span(tool_grease_pencil_eyedropper)},
    {},
    {span(tool_grease_pencil_paint_interpolate)},
};

static const ToolEntry weight_grease_pencil_entries[] = {
    {span(tool_brush)},
    {span(tool_grease_pencil_weight_blur)},
    {span(tool_grease_pencil_weight_average)},
    {span(tool_grease_pencil_weight_smear)},
    {},
    {span(tools_annotate)},
};

static const ToolEntry vertex_grease_pencil_entries[] = {
    {span(tool_brush)},
    {span(tool_grease_pencil_vertex_blur)},
    {span(tool_grease_pencil_vertex_average)},
    {span(tool_grease_pencil_vertex_smear)},
    {span(tool_grease_pencil_vertex_replace)},
    {},
    {span(tools_annotate)},
    {},
    {span(tools_select), /*poll*/ defs_grease_pencil_vertex::poll_select_mask},
};

static const ToolEntry sculpt_curves_entries[] = {
    {span(tool_brush)},
    {span(tool_curves_sculpt_select)},
    {span(tool_curves_sculpt_density)},
    {span(tool_curves_sculpt_add)},
    {span(tool_curves_sculpt_delete)},
    {},
    {span(tools_annotate)},
};

#undef FL_VIEW3D_TOOLS_DEFAULT

/** \} */

/* -------------------------------------------------------------------- */
/** \name La barra
 * \{ */

static const ModeTools view3d_modes[] = {
    /* `_tools[None]` esta VACIA a proposito, y el Python lo deja escrito: el cursor
     * estuvo aqui y hubo que sacarlo por los modos de pintado, que no lo quieren en esa
     * posicion. Se declara vacia en vez de omitirla para que quede claro que no es un
     * olvido del traslado. */
    {nullptr, {}},
    {"OBJECT", span(object_entries)},
    {"POSE", span(pose_entries)},
    {"EDIT_ARMATURE", span(edit_armature_entries)},
    {"EDIT_MESH", span(edit_mesh_entries)},
    {"EDIT_CURVE", span(edit_curve_entries)},
    {"EDIT_CURVES", span(edit_curves_entries)},
    {"EDIT_SURFACE", span(default_and_shear_entries)},
    {"EDIT_METABALL", span(default_and_shear_entries)},
    {"EDIT_LATTICE", span(default_and_shear_entries)},
    {"EDIT_TEXT", span(edit_text_entries)},
    {"EDIT_GREASE_PENCIL", span(edit_grease_pencil_entries)},
    {"EDIT_POINTCLOUD", span(edit_pointcloud_entries)},
    {"PARTICLE", span(particle_entries)},
    {"SCULPT", span(sculpt_entries)},
    {"SCULPT_GREASE_PENCIL", span(sculpt_grease_pencil_entries)},
    {"PAINT_TEXTURE", span(paint_texture_entries)},
    {"PAINT_VERTEX", span(paint_vertex_entries)},
    {"PAINT_WEIGHT", span(paint_weight_entries)},
    {"PAINT_GREASE_PENCIL", span(paint_grease_pencil_entries)},
    {"WEIGHT_GREASE_PENCIL", span(weight_grease_pencil_entries)},
    {"VERTEX_GREASE_PENCIL", span(vertex_grease_pencil_entries)},
    {"SCULPT_CURVES", span(sculpt_curves_entries)},
};

/**
 * `context.mode`.
 *
 * A diferencia de los otros tres espacios, el modo de la vista 3D no sale del espacio
 * sino del OBJETO activo, asi que aqui no hay `space_data` que mirar. No existe un
 * accesor que devuelva la cadena: `CTX_data_mode_enum` da el valor del enum y el
 * identificador (que es la clave de las listas del Python y el nombre de modo de la
 * linea base) sale de `rna_enum_context_mode_items`. Es la misma pareja que usa RNA
 * para la propiedad `Context.mode`, o sea que no puede desviarse de lo que ve el
 * usuario; una tabla propia de nombres si se desviaria en cuanto se anadiera un modo.
 */
static const char *view3d_mode_from_context(const bContext *C)
{
  if (C == nullptr) {
    return nullptr;
  }
  const char *identifier = nullptr;
  RNA_enum_identifier(rna_enum_context_mode_items, CTX_data_mode_enum(C), &identifier);
  return identifier;
}

const ToolbarDecl toolbar_view3d = {
    /*space_type*/ SPACE_VIEW3D,
    /*keymap_prefix*/ "3D View Tool:",
    /*tool_fallback_id*/ "builtin.select",
    /*mode_from_context*/ view3d_mode_from_context,
    /*modes*/ span(view3d_modes),
};

/** \} */

}  // namespace flipendo::toolsystem
