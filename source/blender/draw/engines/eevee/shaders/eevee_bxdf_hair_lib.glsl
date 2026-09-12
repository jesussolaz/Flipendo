/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * Flipendo, phase 2 of the hair work: a real hair BSDF for the real-time path.
 *
 * Until now EEVEE had NO hair shading at all: `gpu_shader_material_hair.glsl` fell back to a
 * plain diffuse closure with a `NOTE(fclem)` saying so. A head of hair was therefore drawn as
 * flat matte geometry: no highlight running along the strand, no light coming through it from
 * behind, no tinted second reflection. That is why real-time hair looks like plastic.
 *
 * WHAT THIS IMPLEMENTS
 * --------------------
 * A far-field Marschner-style model, split in the three lobes the literature names R, TT, TRT:
 *
 *  - R   : the primary specular reflection off the cuticle. Achromatic (it never enters the
 *          fiber), shifted towards the root by the cuticle tilt `-alpha`. This is the white
 *          band that runs ALONG the strand, and it is lobe 0 here.
 *  - TRT : the secondary reflection: light enters the fiber, bounces on the far wall and comes
 *          back out. It crosses the pigment twice more, so it is TINTED with the hair colour,
 *          shifted by `+3*alpha/2` and twice as rough as R. This is the coloured sheen that
 *          separates hair from a plastic thread. Lobe 1 here.
 *  - TT  : straight transmission through the fiber. NOT implemented in this file: it is mapped
 *          onto EEVEE's existing translucent closure by the material node, because that closure
 *          already carries the whole "light coming from behind the surface" machinery (its own
 *          gbuffer bin, its own shadow path, its own `light_eval_transmission` pass). The price
 *          is that TT comes out as a broad cosine lobe instead of the narrow forward cone of the
 *          real model. Documented in `politicas/PELO-A-CPP.md`.
 *
 * Longitudinal term `M_p`: unit-area gaussian on the longitudinal angle `theta_h`, the standard
 * real-time stand-in for Marschner's `M_p`. Because it is normalized (integral 1 over theta),
 * raising the roughness LOWERS the peak instead of adding energy, which is the property the
 * verification checks.
 *
 * Azimuthal term `N_p`: the cheap closed forms used by the real-time literature since
 * Marschner 2003 / Scheuermann 2004, instead of solving the Bravais cylinder:
 *   - `N_R   = 0.25 * cos(phi/2)`, which integrates to 1 over phi in [-pi, pi].
 *   - `N_TRT = exp(17 * cos(phi) - 16.78)`, a narrow lobe peaking at phi = 0 (back-scatter).
 *
 * Fresnel at the cuticle uses the existing `F_eta()`, with the Marschner `1 / cos^2(theta_d)`
 * denominator that turns the cylinder integral into a BCSDF.
 *
 * WHAT IT DOES NOT COVER (say it here, not in a release note):
 *  - No near-field / azimuthal caustics: the TRT glints of a single fiber are not resolved.
 *  - No multiple scattering between strands. The translucent lobe doubles as its stand-in.
 *  - No elliptical cross-section, so `Aspect Ratio` and the Huang model are ignored.
 *  - No dedicated LTC fit: area lights are approximated by their solid angle (see
 *    `light_eval_single_closure`), so a big area light will not stretch the highlight.
 *  - Indirect light falls back to a wide probe lobe around the view-facing cylinder normal.
 */

#include "eevee_bxdf_lib.glsl"
#include "gpu_shader_codegen_lib.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"

/* Index of refraction of keratin. Same default as the `Principled Hair BSDF` node. */
#define BXDF_HAIR_IOR 1.55f

/* Lowest longitudinal width we allow. Below this the normalized gaussian peak explodes and a
 * single strand turns into a firefly on a 8bit output. 0.03 rad is ~1.7 degrees. */
#define BXDF_HAIR_MIN_BETA 0.03f

/* Upper bound of a single lobe evaluation. Pure paranoia against NaN/Inf reaching the film. */
#define BXDF_HAIR_MAX_EVAL 32.0f

/**
 * The cylinder normal that faces the viewer, rebuilt from the tangent.
 * The hair closure stores the TANGENT in the `N` slot of `ClosureUndetermined` (a tangent
 * survives the octahedral gbuffer encoding just as well as a normal does), so whenever a
 * genuine normal is needed — probe lookups, light facing — it is derived here.
 */
float3 bxdf_hair_normal(float3 T, float3 V)
{
  float3 N = V - T * dot(V, T);
  float len_sqr = dot(N, N);
  /* Looking straight down the strand: any perpendicular will do. */
  return (len_sqr > 1e-8f) ? (N * inversesqrt(len_sqr)) : V;
}

/**
 * One Marschner lobe, evaluated for a single light direction.
 * Returns a dimensionless BCSDF value (already divided by `cos^2(theta_d)`), to be multiplied
 * by the solid angle of the light and by the closure colour.
 */
float bxdf_hair_lobe_eval(float3 T, float3 L, float3 V, float beta, float tilt, bool is_trt)
{
  float sin_theta_i = clamp(dot(T, L), -1.0f, 1.0f);
  float sin_theta_o = clamp(dot(T, V), -1.0f, 1.0f);

  float theta_i = asin(sin_theta_i);
  float theta_o = asin(sin_theta_o);
  /* Half angle: where the specular cone of the cylinder sits. */
  float theta_h = (theta_i + theta_o) * 0.5f;
  /* Difference angle: drives Fresnel and the cylinder-to-BCSDF denominator. */
  float theta_d = (theta_o - theta_i) * 0.5f;
  float cos_theta_d = cos(theta_d);

  /* Longitudinal: unit-area gaussian centred on the tilted specular cone. */
  float b = max(beta, BXDF_HAIR_MIN_BETA);
  float x = (theta_h - tilt) / b;
  /* 1 / sqrt(2 * pi), so that the integral over theta is 1 whatever the width. */
  float M = exp(-0.5f * x * x) * 0.39894228f / b;

  /* Azimuthal: angle between the projections of L and V on the plane normal to the strand. */
  float3 Lp = L - T * sin_theta_i;
  float3 Vp = V - T * sin_theta_o;
  float denom = sqrt(max(dot(Lp, Lp) * dot(Vp, Vp), 1e-12f));
  float cos_phi = clamp(dot(Lp, Vp) / denom, -1.0f, 1.0f);
  /* |cos(phi/2)| through the half-angle identity, no trig needed. */
  float cos_half_phi = sqrt(max(0.5f + 0.5f * cos_phi, 0.0f));

  /* Fresnel at the air/keratin boundary, at the true incidence of the cylinder. */
  float F = F_eta(BXDF_HAIR_IOR, cos_theta_d * cos_half_phi);

  float N, attenuation;
  if (is_trt) {
    /* Narrow back-scatter lobe. Two refractions and one internal reflection. */
    N = exp(17.0f * cos_phi - 16.78f);
    float one_minus_f = 1.0f - F;
    attenuation = one_minus_f * one_minus_f * F;
  }
  else {
    /* Broad primary reflection: this is what makes the highlight a BAND along the strand
     * instead of a dot, because it barely depends on where along the strand we are. */
    N = 0.25f * cos_half_phi;
    attenuation = F;
  }

  float value = M * N * attenuation / max(cos_theta_d * cos_theta_d, 1e-3f);
  /* A NaN compares false against anything, so this also filters NaN out instead of letting it
   * reach the film: that is one of the sanity checks of this phase. */
  return (value > 0.0f) ? min(value, BXDF_HAIR_MAX_EVAL) : 0.0f;
}

/** Evaluate the lobe carried by a `ClosureLight` tagged as hair. */
float bxdf_hair_light_eval(ClosureLight cl, float3 L, float3 V)
{
  return bxdf_hair_lobe_eval(
      cl.hair_T, L, V, cl.hair_roughness, cl.hair_tilt, cl.hair_lobe > 0.5f);
}

/**
 * Both lobes are far too wide to be worth a screen trace, and the tangent in `cl.N` would make
 * the ray generator produce nonsense. Report "as rough as diffuse" so the ray-tracing module
 * skips them and falls back to the light probes.
 */
float bxdf_hair_perceived_roughness()
{
  return 1.0f;
}

LightProbeRay bxdf_hair_lightprobe(ClosureUndetermined cl, float3 V)
{
  LightProbeRay probe;
  probe.perceptual_roughness = bxdf_hair_perceived_roughness();
  probe.dominant_direction = bxdf_hair_normal(cl.N, V);
  return probe;
}

BsdfEval bxdf_hair_eval(ClosureUndetermined cl, float3 L, float3 V)
{
  BsdfEval eval;
  eval.throughput = eval.pdf = bxdf_hair_lobe_eval(
      cl.N, L, V, cl.data.x, cl.data.y, cl.data.z > 0.5f);
  return eval;
}

#ifdef EEVEE_UTILITY_TX

ClosureLight bxdf_hair_light(ClosureUndetermined cl, float3 V)
{
  ClosureLight light;
  /* Never used for hair: `light_eval_single_closure` takes the analytic branch. Kept as the
   * identity so that anything reading it before the branch sees a plain cosine lobe. */
  light.ltc_mat = float4(1.0f, 0.0f, 0.0f, 1.0f);
  /* A genuine normal, for the facing attenuation and the shadow bias. */
  light.N = bxdf_hair_normal(cl.N, V);
  light.type = LIGHT_SPECULAR;
  light.hair_T = cl.N;
  light.hair_roughness = cl.data.x;
  light.hair_tilt = cl.data.y;
  light.hair_lobe = (cl.data.z > 0.5f) ? 1.0f : 0.0f;
  return light;
}

#endif
