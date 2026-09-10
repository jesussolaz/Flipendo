/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Dibujo sobre la vista de las herramientas (`draw_cursor` del Python): el circulo del
 * radio bajo el raton. Lo registra la activacion (`fl_toolsystem_activate.cc`).
 */

#include <algorithm>
#include <cmath>

#include "BLI_math_vector_types.hh"

#include "GPU_immediate.hh"
#include "GPU_matrix.hh"
#include "GPU_shader.hh"

#include "RNA_access.hh"

#include "FL_tool_settings_ui.hh"

namespace flipendo::ui::cursor {

/**
 * `gpu_extras.presets.draw_circle_2d(xy, (1, 1, 1, 1), radius, segments=n)`.
 *
 * Mismos vertices que el Python: el paso es 2*pi/(n-1), no 2*pi/n, asi que el ultimo
 * punto coincide con el primero y la tira de lineas cierra el circulo sin
 * `LINE_LOOP`. Se calculan en `double`, como alli, y se guardan en `float`.
 */
static void draw_circle_2d(const blender::int2 &xy, const float radius, const int segments)
{
  GPU_matrix_push();
  GPU_matrix_translate_2f(float(xy.x), float(xy.y));
  GPU_matrix_scale_1f(radius);

  GPUVertFormat *format = immVertexFormat();
  const uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  /* `gpu.shader.from_builtin('UNIFORM_COLOR')`. */
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformColor4f(1.0f, 1.0f, 1.0f, 1.0f);

  const double mul = (1.0 / double(segments - 1)) * (M_PI * 2.0);
  immBegin(GPU_PRIM_LINE_STRIP, uint(segments));
  for (int i = 0; i < segments; i++) {
    immVertex2f(pos, float(std::sin(i * mul)), float(std::cos(i * mul)));
  }
  immEnd();
  immUnbindProgram();

  GPU_matrix_pop();
}

/** `tool.operator_properties(op).radius`: entero en los operadores de circulo. */
static float operator_radius(bToolRef *tref, const char *op_idname)
{
  PointerRNA props = settings::op_props(tref, op_idname);
  if (props.data == nullptr) {
    return 0.0f;
  }
  PropertyRNA *prop = RNA_struct_find_property(&props, "radius");
  if (prop == nullptr) {
    return 0.0f;
  }
  return RNA_property_type(prop) == PROP_INT ? float(RNA_property_int_get(&props, prop)) :
                                               RNA_property_float_get(&props, prop);
}

/**
 * `draw_circle_2d` sin `segments`: el Python los calcula del radio, con un error maximo de
 * un cuarto de pixel, entre 8 y 1000. Con un radio nulo o que saque a `acos` de su
 * dominio, el Python lanza una excepcion y no dibuja nada; aqui igual.
 */
static void draw_circle_2d_auto(const blender::int2 &xy, const float radius)
{
  const double max_pixel_error = 0.25;
  if (radius == 0.0f) {
    return;
  }
  const double cos_arg = 1.0 - max_pixel_error / double(radius);
  if (cos_arg < -1.0 || cos_arg > 1.0) {
    return;
  }
  int segments = int(std::ceil(M_PI / std::acos(cos_arg)));
  segments = std::max(segments, 8);
  segments = std::min(segments, 1000);
  draw_circle_2d(xy, radius, segments);
}

/** `context.scene.tool_settings.uv_sculpt.size`. */
static float uv_sculpt_size(const bContext *C)
{
  PointerRNA ts = settings::tool_settings(C);
  PointerRNA uv_sculpt = settings::pointer_get(&ts, "uv_sculpt");
  if (uv_sculpt.data == nullptr) {
    return 0.0f;
  }
  PropertyRNA *prop = RNA_struct_find_property(&uv_sculpt, "size");
  if (prop == nullptr) {
    return 0.0f;
  }
  return RNA_property_type(prop) == PROP_INT ? float(RNA_property_int_get(&uv_sculpt, prop)) :
                                               RNA_property_float_get(&uv_sculpt, prop);
}

/* Las tres herramientas de escultura de UV dibujan lo mismo. */
void draw_uv_sculpt_brush(bContext *C, bToolRef * /*tref*/, const blender::int2 &xy)
{
  draw_circle_2d_auto(xy, uv_sculpt_size(C));
}

void draw_select_circle_view3d(bContext * /*C*/, bToolRef *tref, const blender::int2 &xy)
{
  draw_circle_2d(xy, operator_radius(tref, "view3d.select_circle"), 32);
}

void draw_select_circle_uv(bContext * /*C*/, bToolRef *tref, const blender::int2 &xy)
{
  draw_circle_2d(xy, operator_radius(tref, "uv.select_circle"), 32);
}

void draw_select_circle_node(bContext * /*C*/, bToolRef *tref, const blender::int2 &xy)
{
  draw_circle_2d(xy, operator_radius(tref, "node.select_circle"), 32);
}

}  // namespace flipendo::ui::cursor
