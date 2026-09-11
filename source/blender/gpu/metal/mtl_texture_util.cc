/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "BKE_global.hh"

#include "mtl_debug.hh"

#include "DNA_userdef_types.h"

#include "GPU_batch.hh"
#include "GPU_batch_presets.hh"
#include "GPU_capabilities.hh"
#include "GPU_framebuffer.hh"
#include "GPU_platform.hh"
#include "GPU_state.hh"

#include "mtl_backend.hh"
#include "mtl_context.hh"
#include "mtl_texture.hh"

/* Utility file for secondary functionality which supports mtl_texture.mm. */

extern char datatoc_compute_texture_update_msl[];
extern char datatoc_compute_texture_read_msl[];

namespace blender::gpu {

/* -------------------------------------------------------------------- */
/** \name Texture Utility Functions
 * \{ */

MTLPixelFormat gpu_texture_format_to_metal(eGPUTextureFormat tex_format)
{
  switch (tex_format) {
    /* Texture & Render-Buffer Formats. */
    case GPU_RGBA8UI:
      return MTLPixelFormatRGBA8Uint;
    case GPU_RGBA8I:
      return MTLPixelFormatRGBA8Sint;
    case GPU_RGBA8:
      return MTLPixelFormatRGBA8Unorm;
    case GPU_RGBA32UI:
      return MTLPixelFormatRGBA32Uint;
    case GPU_RGBA32I:
      return MTLPixelFormatRGBA32Sint;
    case GPU_RGBA32F:
      return MTLPixelFormatRGBA32Float;
    case GPU_RGBA16UI:
      return MTLPixelFormatRGBA16Uint;
    case GPU_RGBA16I:
      return MTLPixelFormatRGBA16Sint;
    case GPU_RGBA16F:
      return MTLPixelFormatRGBA16Float;
    case GPU_RGBA16:
      return MTLPixelFormatRGBA16Unorm;
    case GPU_RG8UI:
      return MTLPixelFormatRG8Uint;
    case GPU_RG8I:
      return MTLPixelFormatRG8Sint;
    case GPU_RG8:
      return MTLPixelFormatRG8Unorm;
    case GPU_RG32UI:
      return MTLPixelFormatRG32Uint;
    case GPU_RG32I:
      return MTLPixelFormatRG32Sint;
    case GPU_RG32F:
      return MTLPixelFormatRG32Float;
    case GPU_RG16UI:
      return MTLPixelFormatRG16Uint;
    case GPU_RG16I:
      return MTLPixelFormatRG16Sint;
    case GPU_RG16F:
      return MTLPixelFormatRG16Float;
    case GPU_RG16:
      return MTLPixelFormatRG16Unorm;
    case GPU_R8UI:
      return MTLPixelFormatR8Uint;
    case GPU_R8I:
      return MTLPixelFormatR8Sint;
    case GPU_R8:
      return MTLPixelFormatR8Unorm;
    case GPU_R32UI:
      return MTLPixelFormatR32Uint;
    case GPU_R32I:
      return MTLPixelFormatR32Sint;
    case GPU_R32F:
      return MTLPixelFormatR32Float;
    case GPU_R16UI:
      return MTLPixelFormatR16Uint;
    case GPU_R16I:
      return MTLPixelFormatR16Sint;
    case GPU_R16F:
      return MTLPixelFormatR16Float;
    case GPU_R16:
      return MTLPixelFormatR16Unorm;
    /* Special formats texture & render-buffer. */
    case GPU_RGB10_A2:
      return MTLPixelFormatRGB10A2Unorm;
    case GPU_RGB10_A2UI:
      return MTLPixelFormatRGB10A2Uint;
    case GPU_R11F_G11F_B10F:
      return MTLPixelFormatRG11B10Float;
    case GPU_DEPTH24_STENCIL8:
      /* NOTE(fclem): DEPTH24_STENCIL8 not supported by Apple Silicon. Fallback to Depth32F8S. */
    case GPU_DEPTH32F_STENCIL8:
      return MTLPixelFormatDepth32Float_Stencil8;
    case GPU_SRGB8_A8:
      return MTLPixelFormatRGBA8Unorm_sRGB;
    /* Texture only formats. */
    case GPU_RGB16F:
      /* 48-Bit pixel format are not supported. Emulate using a padded type with alpha. */
      return MTLPixelFormatRGBA16Float;
    case GPU_RGBA16_SNORM:
      return MTLPixelFormatRGBA16Snorm;
    case GPU_RGBA8_SNORM:
      return MTLPixelFormatRGBA8Snorm;
    case GPU_RGB32F:
      /* 96-Bit pixel format are not supported. Emulate using a padded type with alpha. */
      return MTLPixelFormatRGBA32Float;
    case GPU_RGB32I:
      /* 96-Bit pixel format are not supported. Emulate using a padded type with alpha. */
      return MTLPixelFormatRGBA32Sint;
    case GPU_RGB32UI:
      /* 96-Bit pixel format are not supported. Emulate using a padded type with alpha. */
      return MTLPixelFormatRGBA32Uint;
    case GPU_RGB16_SNORM:
      /* 48-Bit pixel format are not supported. Emulate using a padded type with alpha. */
      return MTLPixelFormatRGBA16Snorm;
    case GPU_RGB16I:
      /* 48-Bit pixel format are not supported. Emulate using a padded type with alpha. */
      return MTLPixelFormatRGBA16Sint;
    case GPU_RGB16UI:
      /* 48-Bit pixel format are not supported. Emulate using a padded type with alpha. */
      return MTLPixelFormatRGBA16Uint;
    case GPU_RGB16:
      /* 48-Bit pixel format are not supported. Emulate using a padded type with alpha. */
      return MTLPixelFormatRGBA16Unorm;
    case GPU_RGB8_SNORM:
      /* 24-Bit pixel format are not supported. Emulate using a padded type with alpha. */
      return MTLPixelFormatRGBA8Snorm;
    case GPU_RGB8:
      /* 24-Bit pixel format are not supported. Emulate using a padded type with alpha. */
      return MTLPixelFormatRGBA8Unorm;
    case GPU_RGB8I:
      /* 24-Bit pixel format are not supported. Emulate using a padded type with alpha. */
      return MTLPixelFormatRGBA8Sint;
    case GPU_RGB8UI:
      /* 24-Bit pixel format are not supported. Emulate using a padded type with alpha. */
      return MTLPixelFormatRGBA8Uint;
    case GPU_RG16_SNORM:
      return MTLPixelFormatRG16Snorm;
    case GPU_RG8_SNORM:
      return MTLPixelFormatRG8Snorm;
    case GPU_R16_SNORM:
      return MTLPixelFormatR16Snorm;
    case GPU_R8_SNORM:
      return MTLPixelFormatR8Snorm;
    /* Special formats, texture only. */
    case GPU_SRGB8_A8_DXT1:
      return MTLPixelFormatBC1_RGBA_sRGB;
    case GPU_SRGB8_A8_DXT3:
      return MTLPixelFormatBC2_RGBA_sRGB;
    case GPU_SRGB8_A8_DXT5:
      return MTLPixelFormatBC3_RGBA_sRGB;
    case GPU_RGBA8_DXT1:
      return MTLPixelFormatBC1_RGBA;
    case GPU_RGBA8_DXT3:
      return MTLPixelFormatBC2_RGBA;
    case GPU_RGBA8_DXT5:
      return MTLPixelFormatBC3_RGBA;
    case GPU_SRGB8:
      /* 24-Bit pixel format are not supported. Emulate using a padded type with alpha. */
      return MTLPixelFormatRGBA8Unorm_sRGB;
    case GPU_RGB9_E5:
      return MTLPixelFormatRGB9E5Float;
    /* Depth Formats. */
    case GPU_DEPTH_COMPONENT32F:
      return MTLPixelFormatDepth32Float;
    case GPU_DEPTH_COMPONENT24:
      /* This formal is not supported on Metal.
       * Use 32Float depth instead with some conversion steps for download and upload. */
      return MTLPixelFormatDepth32Float;
    case GPU_DEPTH_COMPONENT16:
      return MTLPixelFormatDepth16Unorm;
  }
  BLI_assert_msg(false, "Unrecognised GPU pixel format!\n");
  return MTLPixelFormatRGBA8Unorm;
}

size_t get_mtl_format_bytesize(MTLPixelFormat tex_format)
{
  switch (tex_format) {
    case MTLPixelFormatRGBA8Uint:
    case MTLPixelFormatRGBA8Sint:
    case MTLPixelFormatRGBA8Unorm:
    case MTLPixelFormatRGBA8Snorm:
    case MTLPixelFormatRGB10A2Uint:
    case MTLPixelFormatRGB10A2Unorm:
      return 4;
    case MTLPixelFormatRGBA32Uint:
    case MTLPixelFormatRGBA32Sint:
    case MTLPixelFormatRGBA32Float:
      return 16;
    case MTLPixelFormatRGBA16Uint:
    case MTLPixelFormatRGBA16Sint:
    case MTLPixelFormatRGBA16Float:
    case MTLPixelFormatRGBA16Unorm:
    case MTLPixelFormatRGBA16Snorm:
      return 8;
    case MTLPixelFormatRG8Uint:
    case MTLPixelFormatRG8Sint:
    case MTLPixelFormatRG8Unorm:
    case MTLPixelFormatRG8Snorm:
    case MTLPixelFormatRG8Unorm_sRGB:
      return 2;
    case MTLPixelFormatRG32Uint:
    case MTLPixelFormatRG32Sint:
    case MTLPixelFormatRG32Float:
      return 8;
    case MTLPixelFormatRG16Uint:
    case MTLPixelFormatRG16Sint:
    case MTLPixelFormatRG16Float:
    case MTLPixelFormatRG16Unorm:
    case MTLPixelFormatRG16Snorm:
      return 4;
    case MTLPixelFormatR8Uint:
    case MTLPixelFormatR8Sint:
    case MTLPixelFormatR8Unorm:
    case MTLPixelFormatR8Snorm:
      return 1;
    case MTLPixelFormatR32Uint:
    case MTLPixelFormatR32Sint:
    case MTLPixelFormatR32Float:
      return 4;
    case MTLPixelFormatR16Uint:
    case MTLPixelFormatR16Sint:
    case MTLPixelFormatR16Float:
    case MTLPixelFormatR16Snorm:
    case MTLPixelFormatR16Unorm:
      return 2;
    case MTLPixelFormatRG11B10Float:
      return 4;
    case MTLPixelFormatDepth32Float_Stencil8:
      return 8;
    case MTLPixelFormatRGBA8Unorm_sRGB:
    case MTLPixelFormatDepth32Float:
    case MTLPixelFormatDepth24Unorm_Stencil8:
      return 4;
    case MTLPixelFormatDepth16Unorm:
      return 2;
    case MTLPixelFormatBC1_RGBA:
    case MTLPixelFormatBC1_RGBA_sRGB:
      return 1; /* NOTE: not quite correct (BC1 is 0.5 BPP). */
    case MTLPixelFormatBC2_RGBA:
    case MTLPixelFormatBC2_RGBA_sRGB:
    case MTLPixelFormatBC3_RGBA:
    case MTLPixelFormatBC3_RGBA_sRGB:
      return 1;

    default:
      BLI_assert_msg(false, "Unrecognised GPU pixel format!\n");
      return 1;
  }
}

int get_mtl_format_num_components(MTLPixelFormat tex_format)
{
  switch (tex_format) {
    case MTLPixelFormatRGBA8Uint:
    case MTLPixelFormatRGBA8Sint:
    case MTLPixelFormatRGBA8Unorm:
    case MTLPixelFormatRGBA8Snorm:
    case MTLPixelFormatRGBA32Uint:
    case MTLPixelFormatRGBA32Sint:
    case MTLPixelFormatRGBA32Float:
    case MTLPixelFormatRGBA16Uint:
    case MTLPixelFormatRGBA16Sint:
    case MTLPixelFormatRGBA16Float:
    case MTLPixelFormatRGBA16Unorm:
    case MTLPixelFormatRGBA16Snorm:
    case MTLPixelFormatRGBA8Unorm_sRGB:
    case MTLPixelFormatRGB10A2Uint:
    case MTLPixelFormatRGB10A2Unorm:
    case MTLPixelFormatBC1_RGBA_sRGB:
    case MTLPixelFormatBC2_RGBA_sRGB:
    case MTLPixelFormatBC3_RGBA_sRGB:
    case MTLPixelFormatBC1_RGBA:
    case MTLPixelFormatBC2_RGBA:
    case MTLPixelFormatBC3_RGBA:
      return 4;

    case MTLPixelFormatRG11B10Float:
      return 3;

    case MTLPixelFormatRG8Uint:
    case MTLPixelFormatRG8Sint:
    case MTLPixelFormatRG8Unorm:
    case MTLPixelFormatRG32Uint:
    case MTLPixelFormatRG32Sint:
    case MTLPixelFormatRG32Float:
    case MTLPixelFormatRG16Uint:
    case MTLPixelFormatRG16Sint:
    case MTLPixelFormatRG16Float:
    case MTLPixelFormatDepth32Float_Stencil8:
    case MTLPixelFormatRG16Snorm:
    case MTLPixelFormatRG16Unorm:
    case MTLPixelFormatRG8Snorm:
      return 2;

    case MTLPixelFormatR8Uint:
    case MTLPixelFormatR8Sint:
    case MTLPixelFormatR8Unorm:
    case MTLPixelFormatR8Snorm:
    case MTLPixelFormatR32Uint:
    case MTLPixelFormatR32Sint:
    case MTLPixelFormatR32Float:
    case MTLPixelFormatR16Uint:
    case MTLPixelFormatR16Sint:
    case MTLPixelFormatR16Float:
    case MTLPixelFormatR16Unorm:
    case MTLPixelFormatR16Snorm:
    case MTLPixelFormatDepth32Float:
    case MTLPixelFormatDepth16Unorm:
    case MTLPixelFormatDepth24Unorm_Stencil8:
      /* Treating this format as single-channel for direct data copies -- Stencil component is not
       * addressable. */
      return 1;

    default:
      BLI_assert_msg(false, "Unrecognised GPU pixel format!\n");
      return 1;
  }
}

bool mtl_format_supports_blending(MTLPixelFormat format)
{
  /* Add formats as needed -- Verify platforms. */
  const MTLCapabilities &capabilities = MTLBackend::get_capabilities();

  if (capabilities.supports_family_mac1 || capabilities.supports_family_mac_catalyst1) {

    switch (format) {
      case MTLPixelFormatA8Unorm:
      case MTLPixelFormatR8Uint:
      case MTLPixelFormatR8Sint:
      case MTLPixelFormatR16Uint:
      case MTLPixelFormatR16Sint:
      case MTLPixelFormatRG32Uint:
      case MTLPixelFormatRG32Sint:
      case MTLPixelFormatRGBA8Uint:
      case MTLPixelFormatRGBA8Sint:
      case MTLPixelFormatRGBA32Uint:
      case MTLPixelFormatRGBA32Sint:
      case MTLPixelFormatDepth16Unorm:
      case MTLPixelFormatDepth32Float:
      case MTLPixelFormatInvalid:
      case MTLPixelFormatBGR10A2Unorm:
      case MTLPixelFormatRGB10A2Uint:
        return false;
      default:
        return true;
    }
  }
  else {
    switch (format) {
      case MTLPixelFormatA8Unorm:
      case MTLPixelFormatR8Uint:
      case MTLPixelFormatR8Sint:
      case MTLPixelFormatR16Uint:
      case MTLPixelFormatR16Sint:
      case MTLPixelFormatRG32Uint:
      case MTLPixelFormatRG32Sint:
      case MTLPixelFormatRGBA8Uint:
      case MTLPixelFormatRGBA8Sint:
      case MTLPixelFormatRGBA32Uint:
      case MTLPixelFormatRGBA32Sint:
      case MTLPixelFormatRGBA32Float:
      case MTLPixelFormatDepth16Unorm:
      case MTLPixelFormatDepth32Float:
      case MTLPixelFormatInvalid:
      case MTLPixelFormatBGR10A2Unorm:
      case MTLPixelFormatRGB10A2Uint:
        return false;
      default:
        return true;
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Texture data upload routines
 * \{ */

MTLComputePipelineStatePtr gpu::MTLTexture::mtl_texture_update_impl(
    TextureUpdateRoutineSpecialisation specialization_params,
    blender::Map<TextureUpdateRoutineSpecialisation, MTLComputePipelineStatePtr>
        &specialization_cache,
    eGPUTextureType texture_type)
{
  /* Check whether the Kernel exists. */
  MTLComputePipelineStatePtr *result = specialization_cache.lookup_ptr(specialization_params);
  if (result != nullptr) {
    return *result;
  }

  MTLComputePipelineStatePtr return_pso = nullptr;
  {
    /* Equivalente de @autoreleasepool: el pool se drena al salir del ambito. */
    MTLAutoreleasePoolScope pool_scope;

    /* Fetch active context. */
    MTLContext *ctx = MTLContext::get();
    BLI_assert(ctx);

    /** SOURCE. **/
    NSString *tex_update_kernel_src = mtl_string(datatoc_compute_texture_update_msl);

    /* Prepare options and specializations. */
    MTLCompileOptions *options = MTL::CompileOptions::alloc()->init()->autorelease();
    options->setLanguageVersion(MTLLanguageVersion2_2);
    /* El literal `@{...}` de Objective-C no existe en C++: NS::Dictionary se construye
     * con dos arrays paralelos (valores y claves) y su tama&#241;o. El orden de los pares se
     * conserva igual que en el original. */
    const NS::Object *macro_keys[] = {
        mtl_string("INPUT_DATA_TYPE"),
        mtl_string("OUTPUT_DATA_TYPE"),
        mtl_string("COMPONENT_COUNT_INPUT"),
        mtl_string("COMPONENT_COUNT_OUTPUT"),
        mtl_string("TEX_TYPE"),
        mtl_string("IS_TEXTURE_CLEAR"),
    };
    const NS::Object *macro_values[] = {
        mtl_string(specialization_params.input_data_type.c_str()),
        mtl_string(specialization_params.output_data_type.c_str()),
        NS::Number::number(specialization_params.component_count_input),
        NS::Number::number(specialization_params.component_count_output),
        NS::Number::number(int(texture_type)),
        NS::Number::number(int(specialization_params.is_clear ? 1 : 0)),
    };
    options->setPreprocessorMacros(
        NS::Dictionary::dictionary(macro_values, macro_keys, ARRAY_SIZE(macro_keys)));

    /* Prepare shader library for conversion routine. */
    NSError *error = nullptr;
    MTLLibraryPtr temp_lib = ctx->device->newLibrary(tex_update_kernel_src, options, &error)->autorelease();
    if (error) {
      /* Only exit out if genuine error and not warning. */
      if (error->localizedDescription()->rangeOfString(mtl_string("Compilation succeeded"), NS::StringCompareOptions(0)).location ==
          NS::NotFound)
      {
        MTL_LOG_ERROR("Compile Error - Metal Shader Library error %s ", error->localizedDescription()->utf8String());
        BLI_assert(false);
        return nullptr;
      }
    }

    /* Fetch compute function. */
    BLI_assert(temp_lib != nullptr);
    MTLFunctionPtr temp_compute_function = temp_lib->newFunction(mtl_string("compute_texture_update"))->autorelease();
    BLI_assert(temp_compute_function);

    /* Otherwise, bake new Kernel. */
    MTLComputePipelineStatePtr compute_pso = ctx->device->newComputePipelineState(temp_compute_function, &error);
    if (error || compute_pso == nullptr) {
      MTL_LOG_ERROR("Failed to prepare texture_update MTLComputePipelineState %s", error->localizedDescription()->utf8String());
      BLI_assert(false);
    }

    /* Store PSO. */
    specialization_cache.add_new(specialization_params, compute_pso);
    return_pso = compute_pso;
  }

  BLI_assert(return_pso != nullptr);
  return return_pso;
}

MTLComputePipelineStatePtr gpu::MTLTexture::texture_update_1d_get_kernel(
    TextureUpdateRoutineSpecialisation specialization)
{
  MTLContext *mtl_context = MTLContext::get();
  BLI_assert(mtl_context != nullptr);
  return mtl_texture_update_impl(specialization,
                                 mtl_context->get_texture_utils().texture_1d_update_compute_psos,
                                 GPU_TEXTURE_1D);
}

MTLComputePipelineStatePtr gpu::MTLTexture::texture_update_1d_array_get_kernel(
    TextureUpdateRoutineSpecialisation specialization)
{
  MTLContext *mtl_context = MTLContext::get();
  BLI_assert(mtl_context != nullptr);
  return mtl_texture_update_impl(
      specialization,
      mtl_context->get_texture_utils().texture_1d_array_update_compute_psos,
      GPU_TEXTURE_1D_ARRAY);
}

MTLComputePipelineStatePtr gpu::MTLTexture::texture_update_2d_get_kernel(
    TextureUpdateRoutineSpecialisation specialization)
{
  MTLContext *mtl_context = MTLContext::get();
  BLI_assert(mtl_context != nullptr);
  return mtl_texture_update_impl(specialization,
                                 mtl_context->get_texture_utils().texture_2d_update_compute_psos,
                                 GPU_TEXTURE_2D);
}

MTLComputePipelineStatePtr gpu::MTLTexture::texture_update_2d_array_get_kernel(
    TextureUpdateRoutineSpecialisation specialization)
{
  MTLContext *mtl_context = MTLContext::get();
  BLI_assert(mtl_context != nullptr);
  return mtl_texture_update_impl(
      specialization,
      mtl_context->get_texture_utils().texture_2d_array_update_compute_psos,
      GPU_TEXTURE_2D_ARRAY);
}

MTLComputePipelineStatePtr gpu::MTLTexture::texture_update_3d_get_kernel(
    TextureUpdateRoutineSpecialisation specialization)
{
  MTLContext *mtl_context = MTLContext::get();
  BLI_assert(mtl_context != nullptr);
  return mtl_texture_update_impl(specialization,
                                 mtl_context->get_texture_utils().texture_3d_update_compute_psos,
                                 GPU_TEXTURE_3D);
}

/* TODO(Metal): Data upload routine kernel for texture cube and texture cube array.
 * Currently does not appear to be hit. */

GPUShader *gpu::MTLTexture::depth_2d_update_sh_get(
    DepthTextureUpdateRoutineSpecialisation specialization)
{

  /* Check whether the Kernel exists. */
  MTLContext *mtl_context = MTLContext::get();
  BLI_assert(mtl_context != nullptr);

  GPUShader **result = mtl_context->get_texture_utils().depth_2d_update_shaders.lookup_ptr(
      specialization);
  if (result != nullptr) {
    return *result;
  }

  const char *depth_2d_info_variant = nullptr;
  switch (specialization.data_mode) {
    case MTL_DEPTH_UPDATE_MODE_FLOAT:
      depth_2d_info_variant = "depth_2d_update_float";
      break;
    case MTL_DEPTH_UPDATE_MODE_INT24:
      depth_2d_info_variant = "depth_2d_update_int24";
      break;
    case MTL_DEPTH_UPDATE_MODE_INT32:
      depth_2d_info_variant = "depth_2d_update_int32";
      break;
    default:
      BLI_assert(false && "Invalid format mode\n");
      return nullptr;
  }

  GPUShader *shader = GPU_shader_create_from_info_name(depth_2d_info_variant);
  mtl_context->get_texture_utils().depth_2d_update_shaders.add_new(specialization, shader);
  return shader;
}

GPUShader *gpu::MTLTexture::fullscreen_blit_sh_get()
{
  MTLContext *mtl_context = MTLContext::get();
  BLI_assert(mtl_context != nullptr);
  if (mtl_context->get_texture_utils().fullscreen_blit_shader == nullptr) {
    GPUShader *shader = GPU_shader_create_from_info_name("fullscreen_blit");

    mtl_context->get_texture_utils().fullscreen_blit_shader = shader;
  }
  return mtl_context->get_texture_utils().fullscreen_blit_shader;
}

/* Special routine for updating 2D depth textures using the rendering pipeline. */
void gpu::MTLTexture::update_sub_depth_2d(
    int mip, int offset[3], int extent[3], eGPUDataFormat type, const void *data)
{
  /* Verify we are in a valid configuration. */
  BLI_assert(ELEM(format_,
                  GPU_DEPTH_COMPONENT24,
                  GPU_DEPTH_COMPONENT32F,
                  GPU_DEPTH_COMPONENT16,
                  GPU_DEPTH24_STENCIL8,
                  GPU_DEPTH32F_STENCIL8));
  BLI_assert(validate_data_format(format_, type));
  BLI_assert(ELEM(type, GPU_DATA_FLOAT, GPU_DATA_UINT_24_8, GPU_DATA_UINT));

  /* Determine whether we are in GPU_DATA_UINT_24_8 or GPU_DATA_FLOAT mode. */
  bool is_float = (type == GPU_DATA_FLOAT);
  eGPUTextureFormat format = (is_float) ? GPU_R32F : GPU_R32I;

  /* Shader key - Add parameters here for different configurations. */
  DepthTextureUpdateRoutineSpecialisation specialization;
  switch (type) {
    case GPU_DATA_FLOAT:
      specialization.data_mode = MTL_DEPTH_UPDATE_MODE_FLOAT;
      break;

    case GPU_DATA_UINT_24_8:
      specialization.data_mode = MTL_DEPTH_UPDATE_MODE_INT24;
      break;

    case GPU_DATA_UINT:
      specialization.data_mode = MTL_DEPTH_UPDATE_MODE_INT32;
      break;

    default:
      BLI_assert_msg(false, "Unsupported eGPUDataFormat being passed to depth texture update\n");
      return;
  }

  /* Push contents into an r32_tex and render contents to depth using a shader. */
  GPUTexture *r32_tex_tmp = GPU_texture_create_2d("depth_intermediate_copy_tex",
                                                  w_,
                                                  h_,
                                                  1,
                                                  format,
                                                  GPU_TEXTURE_USAGE_SHADER_READ |
                                                      GPU_TEXTURE_USAGE_ATTACHMENT,
                                                  nullptr);
  GPU_texture_filter_mode(r32_tex_tmp, false);
  GPU_texture_extend_mode(r32_tex_tmp, GPU_SAMPLER_EXTEND_MODE_EXTEND);
  gpu::MTLTexture *mtl_tex = static_cast<gpu::MTLTexture *>(unwrap(r32_tex_tmp));
  mtl_tex->update_sub(mip, offset, extent, type, data);

  GPUFrameBuffer *restore_fb = GPU_framebuffer_active_get();
  GPUFrameBuffer *depth_fb_temp = GPU_framebuffer_create("depth_intermediate_copy_fb");
  GPU_framebuffer_texture_attach(depth_fb_temp, wrap(static_cast<Texture *>(this)), 0, mip);
  GPU_framebuffer_bind(depth_fb_temp);
  if (extent[0] == w_ && extent[1] == h_) {
    /* Skip load if the whole texture is being updated. */
    GPU_framebuffer_clear_depth(depth_fb_temp, 0.0);
    GPU_framebuffer_clear_stencil(depth_fb_temp, 0);
  }

  GPUShader *depth_2d_update_sh = depth_2d_update_sh_get(specialization);
  BLI_assert(depth_2d_update_sh != nullptr);
  Batch *quad = GPU_batch_preset_quad();
  GPU_batch_set_shader(quad, depth_2d_update_sh);

  GPU_batch_texture_bind(quad, "source_data", r32_tex_tmp);
  GPU_batch_uniform_1i(quad, "mip", mip);
  GPU_batch_uniform_2f(quad, "extent", (float)extent[0], (float)extent[1]);
  GPU_batch_uniform_2f(quad, "offset", (float)offset[0], (float)offset[1]);
  GPU_batch_uniform_2f(quad, "size", (float)w_, (float)h_);

  bool depth_write_prev = GPU_depth_mask_get();
  uint stencil_mask_prev = GPU_stencil_mask_get();
  eGPUDepthTest depth_test_prev = GPU_depth_test_get();
  eGPUStencilTest stencil_test_prev = GPU_stencil_test_get();
  GPU_scissor_test(true);
  GPU_scissor(offset[0], offset[1], extent[0], extent[1]);

  GPU_stencil_write_mask_set(0xFF);
  GPU_stencil_reference_set(0);
  GPU_stencil_test(GPU_STENCIL_ALWAYS);
  GPU_depth_mask(true);
  GPU_depth_test(GPU_DEPTH_ALWAYS);

  GPU_batch_draw(quad);

  GPU_depth_mask(depth_write_prev);
  GPU_stencil_write_mask_set(stencil_mask_prev);
  GPU_stencil_test(stencil_test_prev);
  GPU_depth_test(depth_test_prev);

  if (restore_fb != nullptr) {
    GPU_framebuffer_bind(restore_fb);
  }
  else {
    GPU_framebuffer_restore();
  }
  GPU_framebuffer_free(depth_fb_temp);
  GPU_texture_free(r32_tex_tmp);
}
/** \} */

/* -------------------------------------------------------------------- */
/** \name Texture data read routines
 * \{ */

MTLComputePipelineStatePtr gpu::MTLTexture::mtl_texture_read_impl(
    TextureReadRoutineSpecialisation specialization_params,
    blender::Map<TextureReadRoutineSpecialisation, MTLComputePipelineStatePtr>
        &specialization_cache,
    eGPUTextureType texture_type)
{
  /* Check whether the Kernel exists. */
  MTLComputePipelineStatePtr *result = specialization_cache.lookup_ptr(specialization_params);
  if (result != nullptr) {
    return *result;
  }

  MTLComputePipelineStatePtr return_pso = nullptr;
  {
    /* Equivalente de @autoreleasepool: el pool se drena al salir del ambito. */
    MTLAutoreleasePoolScope pool_scope;

    /* Fetch active context. */
    MTLContext *ctx = MTLContext::get();
    BLI_assert(ctx);

    /** SOURCE. **/
    NSString *tex_update_kernel_src = mtl_string(datatoc_compute_texture_read_msl);

    /* Defensive Debug Checks. */
    int64_t depth_scale_factor = 1;
    if (specialization_params.depth_format_mode > 0) {
      BLI_assert(specialization_params.component_count_input == 1);
      BLI_assert(specialization_params.component_count_output == 1);
      switch (specialization_params.depth_format_mode) {
        case 1:
          /* FLOAT */
          depth_scale_factor = 1;
          break;
        case 2:
          /* D24 uint */
          depth_scale_factor = 0xFFFFFFu;
          break;
        case 4:
          /* D32 uint */
          depth_scale_factor = 0xFFFFFFFFu;
          break;
        default:
          BLI_assert_msg(0, "Unrecognized mode");
          break;
      }
    }

    /* Prepare options and specializations. */
    MTLCompileOptions *options = MTL::CompileOptions::alloc()->init()->autorelease();
    options->setLanguageVersion(MTLLanguageVersion2_2);
    /* Ver la nota del diccionario en la ruta de escritura: `@{...}` no existe en C++ y
     * NS::Dictionary se construye con valores y claves en arrays paralelos. */
    const NS::Object *macro_keys[] = {
        mtl_string("INPUT_DATA_TYPE"),
        mtl_string("OUTPUT_DATA_TYPE"),
        mtl_string("COMPONENT_COUNT_INPUT"),
        mtl_string("COMPONENT_COUNT_OUTPUT"),
        mtl_string("WRITE_COMPONENT_COUNT"),
        mtl_string("IS_DEPTH_FORMAT"),
        mtl_string("DEPTH_SCALE_FACTOR"),
        mtl_string("TEX_TYPE"),
        mtl_string("IS_DEPTHSTENCIL_24_8"),
    };
    const NS::Object *macro_values[] = {
        mtl_string(specialization_params.input_data_type.c_str()),
        mtl_string(specialization_params.output_data_type.c_str()),
        NS::Number::number(specialization_params.component_count_input),
        NS::Number::number(specialization_params.component_count_output),
        NS::Number::number(min_ii(specialization_params.component_count_input,
                                  specialization_params.component_count_output)),
        NS::Number::number((specialization_params.depth_format_mode > 0) ? 1 : 0),
        NS::Number::number((long long)depth_scale_factor),
        NS::Number::number(int(texture_type)),
        NS::Number::number((specialization_params.depth_format_mode == 2) ? 1 : 0),
    };
    options->setPreprocessorMacros(
        NS::Dictionary::dictionary(macro_values, macro_keys, ARRAY_SIZE(macro_keys)));

    /* Prepare shader library for conversion routine. */
    NSError *error = nullptr;
    MTLLibraryPtr temp_lib = ctx->device->newLibrary(tex_update_kernel_src, options, &error)->autorelease();
    if (error) {
      /* Only exit out if genuine error and not warning. */
      if (error->localizedDescription()->rangeOfString(mtl_string("Compilation succeeded"), NS::StringCompareOptions(0)).location ==
          NS::NotFound)
      {
        MTL_LOG_ERROR("Compile Error - Metal Shader Library error %s ", error->localizedDescription()->utf8String());
        BLI_assert(false);
        return nullptr;
      }
    }

    /* Fetch compute function. */
    BLI_assert(temp_lib != nullptr);
    MTLFunctionPtr temp_compute_function = temp_lib->newFunction(mtl_string("compute_texture_read"))->autorelease();
    BLI_assert(temp_compute_function);

    /* Otherwise, bake new Kernel. */
    MTLComputePipelineStatePtr compute_pso = ctx->device->newComputePipelineState(temp_compute_function, &error);
    if (error || compute_pso == nullptr) {
      MTL_LOG_ERROR("Failed to prepare texture_read MTLComputePipelineState %s", error->localizedDescription()->utf8String());
      BLI_assert(false);
      return nullptr;
    }

    /* Store PSO. */
    specialization_cache.add_new(specialization_params, compute_pso);
    return_pso = compute_pso;
  }

  BLI_assert(return_pso != nullptr);
  return return_pso;
}

MTLComputePipelineStatePtr gpu::MTLTexture::texture_read_2d_get_kernel(
    TextureReadRoutineSpecialisation specialization)
{
  MTLContext *mtl_context = MTLContext::get();
  BLI_assert(mtl_context != nullptr);
  return mtl_texture_read_impl(specialization,
                               mtl_context->get_texture_utils().texture_2d_read_compute_psos,
                               GPU_TEXTURE_2D);
}

MTLComputePipelineStatePtr gpu::MTLTexture::texture_read_2d_array_get_kernel(
    TextureReadRoutineSpecialisation specialization)
{
  MTLContext *mtl_context = MTLContext::get();
  BLI_assert(mtl_context != nullptr);
  return mtl_texture_read_impl(specialization,
                               mtl_context->get_texture_utils().texture_2d_array_read_compute_psos,
                               GPU_TEXTURE_2D_ARRAY);
}

MTLComputePipelineStatePtr gpu::MTLTexture::texture_read_1d_get_kernel(
    TextureReadRoutineSpecialisation specialization)
{
  MTLContext *mtl_context = MTLContext::get();
  BLI_assert(mtl_context != nullptr);
  return mtl_texture_read_impl(specialization,
                               mtl_context->get_texture_utils().texture_1d_read_compute_psos,
                               GPU_TEXTURE_1D);
}

MTLComputePipelineStatePtr gpu::MTLTexture::texture_read_1d_array_get_kernel(
    TextureReadRoutineSpecialisation specialization)
{
  MTLContext *mtl_context = MTLContext::get();
  BLI_assert(mtl_context != nullptr);
  return mtl_texture_read_impl(specialization,
                               mtl_context->get_texture_utils().texture_1d_array_read_compute_psos,
                               GPU_TEXTURE_1D_ARRAY);
}

MTLComputePipelineStatePtr gpu::MTLTexture::texture_read_3d_get_kernel(
    TextureReadRoutineSpecialisation specialization)
{
  MTLContext *mtl_context = MTLContext::get();
  BLI_assert(mtl_context != nullptr);
  return mtl_texture_read_impl(specialization,
                               mtl_context->get_texture_utils().texture_3d_read_compute_psos,
                               GPU_TEXTURE_3D);
}

/** \} */

}  // namespace blender::gpu
