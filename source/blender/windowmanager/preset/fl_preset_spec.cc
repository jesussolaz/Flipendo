/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Qué guarda cada familia de presets, y el escritor.
 *
 * Esto era metaprogramacion: `AddPresetBase.execute` recorria `preset_values`,
 * hacia `eval()` de cada ruta, expandia el resultado con `repr()` y escribia un
 * `.py` que luego habria que volver a ejecutar. La lista de rutas nunca fue
 * codigo: es la definicion de la familia. Aqui esta como tabla, y el escritor
 * captura el valor con RNA y lo serializa.
 *
 * Las rutas se guardan ya aplanadas, con raiz `context.`: los alias de
 * `preset_defines` (`scene = bpy.context.scene`) no existen en el formato.
 */

#include "FL_preset.hpp"

#include <cstring>
#include <string>
#include <vector>

#include "DNA_windowmanager_types.h"

#include "BKE_context.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"
#include "RNA_types.hh"

namespace flipendo::preset {

namespace {

struct Family {
  const char *subdir;
  const char *const *paths;
  int paths_num;
  /* Rutas que solo entran si se pide la distancia focal (`use_focal_length`). */
  const char *const *optional;
  int optional_num;
};

#define FL_ARRAY_NUM(a) (int(sizeof(a) / sizeof((a)[0])))

const char *const paths_render[] = {
    "context.scene.render.fps",
    "context.scene.render.fps_base",
    "context.scene.render.pixel_aspect_x",
    "context.scene.render.pixel_aspect_y",
    "context.scene.render.resolution_percentage",
    "context.scene.render.resolution_x",
    "context.scene.render.resolution_y",
};

const char *const paths_camera[] = {
    "context.camera.sensor_width",
    "context.camera.sensor_height",
    "context.camera.sensor_fit",
};
const char *const paths_camera_focal[] = {
    "context.camera.lens",
    "context.camera.lens_unit",
};

const char *const paths_safe_areas[] = {
    "context.scene.safe_areas.title",
    "context.scene.safe_areas.action",
    "context.scene.safe_areas.title_center",
    "context.scene.safe_areas.action_center",
};

const char *const paths_cloth[] = {
    "context.cloth.settings.quality",
    "context.cloth.settings.mass",
    "context.cloth.settings.air_damping",
    "context.cloth.settings.bending_model",
    "context.cloth.settings.tension_stiffness",
    "context.cloth.settings.compression_stiffness",
    "context.cloth.settings.shear_stiffness",
    "context.cloth.settings.bending_stiffness",
    "context.cloth.settings.tension_damping",
    "context.cloth.settings.compression_damping",
    "context.cloth.settings.shear_damping",
    "context.cloth.settings.bending_damping",
};

const char *const paths_fluid[] = {
    "context.fluid.domain_settings.viscosity_base",
    "context.fluid.domain_settings.viscosity_exponent",
};

const char *const paths_hair_dynamics[] = {
    "context.particle_system.cloth.settings.quality",
    "context.particle_system.cloth.settings.mass",
    "context.particle_system.cloth.settings.bending_stiffness",
    "context.particle_system.settings.bending_random",
    "context.particle_system.cloth.settings.bending_damping",
    "context.particle_system.cloth.settings.air_damping",
    "context.particle_system.cloth.settings.internal_friction",
    "context.particle_system.cloth.settings.density_target",
    "context.particle_system.cloth.settings.density_strength",
    "context.particle_system.cloth.settings.voxel_cell_size",
    "context.particle_system.cloth.settings.pin_stiffness",
};

const char *const paths_text_editor[] = {
    "context.preferences.filepaths.text_editor",
    "context.preferences.filepaths.text_editor_args",
};

const char *const paths_tracking_camera[] = {
    "context.edit_movieclip.tracking.camera.sensor_width",
    "context.edit_movieclip.tracking.camera.pixel_aspect",
    "context.edit_movieclip.tracking.camera.k1",
    "context.edit_movieclip.tracking.camera.k2",
    "context.edit_movieclip.tracking.camera.k3",
};
const char *const paths_tracking_camera_focal[] = {
    "context.edit_movieclip.tracking.camera.units",
    "context.edit_movieclip.tracking.camera.focal_length",
};

const char *const paths_tracking_track_color[] = {
    "context.edit_movieclip.tracking.tracks.active.color",
    "context.edit_movieclip.tracking.tracks.active.use_custom_color",
};

const char *const paths_tracking_settings[] = {
    "context.edit_movieclip.tracking.settings.default_correlation_min",
    "context.edit_movieclip.tracking.settings.default_pattern_size",
    "context.edit_movieclip.tracking.settings.default_search_size",
    "context.edit_movieclip.tracking.settings.default_frames_limit",
    "context.edit_movieclip.tracking.settings.default_pattern_match",
    "context.edit_movieclip.tracking.settings.default_margin",
    "context.edit_movieclip.tracking.settings.default_motion_model",
    "context.edit_movieclip.tracking.settings.use_default_brute",
    "context.edit_movieclip.tracking.settings.use_default_normalization",
    "context.edit_movieclip.tracking.settings.use_default_mask",
    "context.edit_movieclip.tracking.settings.use_default_red_channel",
    "context.edit_movieclip.tracking.settings.use_default_green_channel",
    "context.edit_movieclip.tracking.settings.use_default_blue_channel",
    "context.edit_movieclip.tracking.settings.default_weight",
};

const char *const paths_eevee_raytracing[] = {
    "context.scene.eevee.ray_tracing_method",
    "context.scene.eevee.ray_tracing_options.resolution_scale",
    "context.scene.eevee.ray_tracing_options.trace_max_roughness",
    "context.scene.eevee.ray_tracing_options.screen_trace_quality",
    "context.scene.eevee.ray_tracing_options.screen_trace_thickness",
    "context.scene.eevee.ray_tracing_options.use_denoise",
    "context.scene.eevee.ray_tracing_options.denoise_spatial",
    "context.scene.eevee.ray_tracing_options.denoise_temporal",
    "context.scene.eevee.ray_tracing_options.denoise_bilateral",
    "context.scene.eevee.fast_gi_method",
    "context.scene.eevee.fast_gi_resolution",
    "context.scene.eevee.fast_gi_ray_count",
    "context.scene.eevee.fast_gi_step_count",
    "context.scene.eevee.fast_gi_quality",
    "context.scene.eevee.fast_gi_distance",
    "context.scene.eevee.fast_gi_thickness_near",
    "context.scene.eevee.fast_gi_thickness_far",
    "context.scene.eevee.fast_gi_bias",
};

const char *const paths_white_balance[] = {
    "context.scene.view_settings.white_balance_temperature",
    "context.scene.view_settings.white_balance_tint",
};

const char *const paths_node_color[] = {
    "context.active_node.color",
    "context.active_node.use_custom_color",
};

const char *const paths_gpencil_brush[] = {
    "context.tool_settings.gpencil_paint.brush.gpencil_settings.input_samples",
    "context.tool_settings.gpencil_paint.brush.gpencil_settings.active_smooth_factor",
    "context.tool_settings.gpencil_paint.brush.gpencil_settings.angle",
    "context.tool_settings.gpencil_paint.brush.gpencil_settings.angle_factor",
    "context.tool_settings.gpencil_paint.brush.gpencil_settings.use_settings_stabilizer",
    "context.tool_settings.gpencil_paint.brush.smooth_stroke_radius",
    "context.tool_settings.gpencil_paint.brush.smooth_stroke_factor",
    "context.tool_settings.gpencil_paint.brush.gpencil_settings.pen_smooth_factor",
    "context.tool_settings.gpencil_paint.brush.gpencil_settings.pen_smooth_steps",
    "context.tool_settings.gpencil_paint.brush.gpencil_settings.pen_subdivision_steps",
    "context.tool_settings.gpencil_paint.brush.gpencil_settings.use_settings_random",
    "context.tool_settings.gpencil_paint.brush.gpencil_settings.random_pressure",
    "context.tool_settings.gpencil_paint.brush.gpencil_settings.random_strength",
    "context.tool_settings.gpencil_paint.brush.gpencil_settings.uv_random",
    "context.tool_settings.gpencil_paint.brush.gpencil_settings.pen_jitter",
    "context.tool_settings.gpencil_paint.brush.gpencil_settings.use_jitter_pressure",
    "context.tool_settings.gpencil_paint.brush.gpencil_settings.use_trim",
};

/* Ojo: `gpcolor.mix_factor` estaba DOS veces en la lista de Python. Se conserva
 * el duplicado a proposito: quitarlo cambiaria el fichero que se escribe, y este
 * modulo replica la lista, no la corrige. */
const char *const paths_gpencil_material[] = {
    "context.object.active_material.grease_pencil.mode",
    "context.object.active_material.grease_pencil.stroke_style",
    "context.object.active_material.grease_pencil.color",
    "context.object.active_material.grease_pencil.stroke_image",
    "context.object.active_material.grease_pencil.pixel_size",
    "context.object.active_material.grease_pencil.mix_stroke_factor",
    "context.object.active_material.grease_pencil.alignment_mode",
    "context.object.active_material.grease_pencil.alignment_rotation",
    "context.object.active_material.grease_pencil.fill_style",
    "context.object.active_material.grease_pencil.fill_color",
    "context.object.active_material.grease_pencil.fill_image",
    "context.object.active_material.grease_pencil.gradient_type",
    "context.object.active_material.grease_pencil.mix_color",
    "context.object.active_material.grease_pencil.mix_factor",
    "context.object.active_material.grease_pencil.flip",
    "context.object.active_material.grease_pencil.texture_offset",
    "context.object.active_material.grease_pencil.texture_scale",
    "context.object.active_material.grease_pencil.texture_angle",
    "context.object.active_material.grease_pencil.texture_clamp",
    "context.object.active_material.grease_pencil.mix_factor",
    "context.object.active_material.grease_pencil.show_stroke",
    "context.object.active_material.grease_pencil.show_fill",
};

#define FL_FAMILY(sub, arr) \
  { \
    sub, arr, FL_ARRAY_NUM(arr), nullptr, 0 \
  }
#define FL_FAMILY_OPT(sub, arr, opt) \
  { \
    sub, arr, FL_ARRAY_NUM(arr), opt, FL_ARRAY_NUM(opt) \
  }

const Family families[] = {
    FL_FAMILY("render", paths_render),
    FL_FAMILY_OPT("camera", paths_camera, paths_camera_focal),
    FL_FAMILY("safe_areas", paths_safe_areas),
    FL_FAMILY("cloth", paths_cloth),
    FL_FAMILY("fluid", paths_fluid),
    FL_FAMILY("hair_dynamics", paths_hair_dynamics),
    FL_FAMILY("text_editor", paths_text_editor),
    FL_FAMILY_OPT("tracking_camera", paths_tracking_camera, paths_tracking_camera_focal),
    FL_FAMILY("tracking_track_color", paths_tracking_track_color),
    FL_FAMILY("tracking_settings", paths_tracking_settings),
    FL_FAMILY("eevee/raytracing", paths_eevee_raytracing),
    FL_FAMILY("color_management/white_balance", paths_white_balance),
    FL_FAMILY("node_color", paths_node_color),
    FL_FAMILY("gpencil_brush", paths_gpencil_brush),
    FL_FAMILY("gpencil_material", paths_gpencil_material),
};

#undef FL_FAMILY
#undef FL_FAMILY_OPT

/**
 * Presets de operador: la lista sale de las propiedades del operador activo, no
 * de una tabla. Se salta lo que lleve la bandera `PROP_SKIP_PRESET` y todo lo
 * que pertenezca al tipo base `Operator` -- es la misma criba que hacia el
 * Python con `Operator.bl_rna.properties.keys()` como lista negra.
 */
bool operator_paths(bContext *C, std::vector<std::string> &r_paths, std::string &r_error)
{
  PointerRNA op_ptr = CTX_data_pointer_get(C, "active_operator");
  if (op_ptr.data == nullptr) {
    r_error = "no hay operador activo del que guardar un preset";
    return false;
  }
  wmOperator *op = static_cast<wmOperator *>(op_ptr.data);
  if (op->type == nullptr || op->ptr == nullptr) {
    r_error = "el operador activo no tiene propiedades";
    return false;
  }

  PointerRNA base = RNA_pointer_create_discrete(nullptr, &RNA_Operator, nullptr);
  RNA_STRUCT_BEGIN (op->ptr, prop) {
    const char *id = RNA_property_identifier(prop);
    if (RNA_property_flag(prop) & PROP_SKIP_PRESET) {
      continue;
    }
    if (RNA_struct_find_property(&base, id) != nullptr) {
      continue;
    }
    r_paths.push_back(std::string("context.active_operator.") + id);
  }
  RNA_STRUCT_END;
  return true;
}

}  // namespace

bool spec_paths(bContext *C,
                const std::string &subdir,
                bool use_focal_length,
                std::vector<std::string> &r_paths,
                std::string &r_error)
{
  if (subdir.compare(0, 9, "operator/") == 0) {
    return operator_paths(C, r_paths, r_error);
  }
  for (const Family &family : families) {
    if (subdir != family.subdir) {
      continue;
    }
    for (int i = 0; i < family.paths_num; i++) {
      r_paths.emplace_back(family.paths[i]);
    }
    if (use_focal_length) {
      for (int i = 0; i < family.optional_num; i++) {
        r_paths.emplace_back(family.optional[i]);
      }
    }
    return true;
  }
  r_error = "familia de presets desconocida: '" + subdir + "'";
  return false;
}

bool write_preset(bContext *C,
                  const std::string &subdir,
                  const std::string &filepath,
                  bool use_focal_length,
                  std::string &r_error)
{
  std::vector<std::string> paths;
  if (!spec_paths(C, subdir, use_focal_length, paths, r_error)) {
    return false;
  }
  Preset preset;
  preset.subdir = subdir;
  if (!capture(C, paths, preset, r_error)) {
    return false;
  }
  return write_file(filepath, preset, r_error);
}

}  // namespace flipendo::preset
