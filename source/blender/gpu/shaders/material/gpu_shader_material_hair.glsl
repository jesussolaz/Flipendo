/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Flipendo, phase 2 of the hair work.
 *
 * What was here before, in the words of the Blender developers themselves:
 *
 *   NOTE(fclem): This is the way it should be. But we don't have proper implementation of the
 *   hair closure yet. For now fall back to a simpler diffuse surface so that we have at least a
 *   color feedback.
 *
 * So a head of hair was drawn as FLAT DIFFUSE: no highlight running along the strand, no light
 * coming through it from behind, no tinted second reflection. Both hair nodes fell into the same
 * `#if 0`. This file now builds the real thing out of three closures:
 *
 *   bin 0  translucent   TT  : light that goes THROUGH the fiber. Tinted by one traversal of the
 *                              pigment. In the reflection pass it doubles as the stand-in for the
 *                              multiple scattering between strands, which is what keeps dark hair
 *                              from turning into a black silhouette.
 *   bin 1  hair lobe 0   R   : primary specular off the cuticle. Achromatic, narrow, tilted by
 *                              `-offset`. This is the white band along the strand.
 *   bin 2  hair lobe 1   TRT : secondary reflection. Tinted by three traversals of the pigment,
 *                              twice as rough, tilted by `+1.5 * offset` so it sits away from R.
 *                              This is the coloured sheen that tells hair from plastic thread.
 *
 * The lobes themselves live in `eevee_bxdf_hair_lib.glsl`, which also lists the approximations.
 * The pigment maths (melanin, `sigma_a_from_reflectance`) is copied from Cycles
 * (`intern/cycles/kernel/closure/bsdf_hair_principled.h`) so the same material means the same
 * hair in both renderers.
 */

/* Cycles `sigma_a_from_reflectance()`: turns a target reflectance into an absorption
 * coefficient, compensating for the azimuthal roughness. Same polynomial, same units. */
float3 hair_sigma_a_from_reflectance(float3 color, float azimuthal_roughness)
{
  float x = azimuthal_roughness;
  float fac = (((((0.245f * x) + 5.574f) * x - 10.73f) * x + 2.532f) * x - 0.215f) * x + 5.969f;
  float3 sigma = log(max(color, float3(1e-5f))) / fac;
  return sigma * sigma;
}

/* Cycles `sigma_a_from_concentration()`. Eumelanin is the brown/black pigment, pheomelanin the
 * yellow/red one. */
float3 hair_sigma_a_from_concentration(float eumelanin, float pheomelanin)
{
  float3 eumelanin_color = float3(0.506f, 0.841f, 1.653f);
  float3 pheomelanin_color = float3(0.343f, 0.733f, 1.924f);
  return eumelanin * eumelanin_color + pheomelanin * pheomelanin_color;
}

/**
 * The direction the lobes are built around.
 * On curves this is the per-pixel curve tangent that `init_globals_curves()` already computes
 * (it follows the Cycles convention, so the highlight lands where Cycles puts it). On anything
 * else — a mesh with a hair material on it — there is no strand, so fall back to a tangent
 * derived from the shading normal: the result is not hair, but it is finite and not black.
 */
float3 hair_tangent_get(float3 tangent_in)
{
  float len_sqr = dot(tangent_in, tangent_in);
  if (len_sqr > 1e-8f) {
    return tangent_in * inversesqrt(len_sqr);
  }
  if (g_data.is_strand) {
    return g_data.curve_T;
  }
  /* Any direction orthogonal to the normal. */
  float3 up = (abs(g_data.N.z) < 0.99f) ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
  return normalize(cross(g_data.N, up));
}

/**
 * Emit the three closures. Shared by both hair nodes so that they cannot drift apart.
 *
 * \param sigma_a: absorption coefficient of the pigment (per fiber radius).
 * \param roughness: [0..1] user roughness, mapped to the longitudinal width of the lobes.
 * \param offset: cuticle tilt in radians.
 * \param lobe_R, lobe_TT, lobe_TRT: per-lobe user multipliers. 1 is the physical value.
 */
void hair_closures_emit(float3 T,
                        float3 sigma_a,
                        float roughness,
                        float offset,
                        float weight,
                        float lobe_R,
                        float lobe_TT,
                        float lobe_TRT)
{
  /* Transmittance of ONE traversal of the fiber. Two interfaces, mean path of two radii. */
  float3 transmittance = exp(-2.0f * max(sigma_a, float3(0.0f)));

  /* Longitudinal width. Squared so the slider spends most of its range on the sharp end, where
   * hair actually lives (a real cuticle is 5 to 10 degrees wide). */
  float r = saturate(roughness);
  float beta_R = 0.03f + r * r * 0.5f;
  /* The secondary reflection is always blurrier: it crossed the fiber three times. */
  float beta_TRT = beta_R * 2.0f;

  /* Marschner's cuticle shifts. Opposite signs on purpose: that is what makes the secondary
   * highlight sit BELOW the primary one instead of on top of it. */
  float tilt_R = -offset;
  float tilt_TRT = 1.5f * offset;

  /* TT: straight through the fiber. Mapped onto the translucent closure, which already owns
   * the "light from behind" path (own gbuffer bin, own shadow term, own light loop). It is a
   * broad cosine lobe instead of the narrow forward cone of the real model: documented
   * approximation, see `eevee_bxdf_hair_lib.glsl`. */
  ClosureTranslucent tt_data;
  tt_data.weight = weight;
  tt_data.color = transmittance * max(lobe_TT, 0.0f);
  tt_data.N = g_data.N;
  closure_eval(tt_data);

  /* R: off the cuticle, never enters the fiber, so it is achromatic. */
  ClosureHair r_data;
  r_data.weight = weight;
  r_data.color = float3(max(lobe_R, 0.0f));
  r_data.T = T;
  r_data.offset = tilt_R;
  r_data.roughness = float2(beta_R, beta_R);
  r_data.lobe = 0.0f;
  closure_eval(r_data);

  /* TRT: in, off the back wall, out. Three traversals of the pigment. */
  ClosureHair trt_data;
  trt_data.weight = weight;
  trt_data.color = transmittance * transmittance * transmittance * max(lobe_TRT, 0.0f);
  trt_data.T = T;
  trt_data.offset = tilt_TRT;
  trt_data.roughness = float2(beta_TRT, beta_TRT);
  trt_data.lobe = 1.0f;
  closure_eval(trt_data);
}

void node_bsdf_hair(float4 color,
                    float offset,
                    float roughness_u,
                    float roughness_v,
                    float3 T,
                    float weight,
                    const float component,
                    out Closure result)
{
  color = max(color, float4(0.0f));

  /* This node has no pigment model: its `Color` IS the reflectance of the chosen component. */
  float3 sigma_a = hair_sigma_a_from_reflectance(color.rgb, saturate(roughness_v));

  /* `component`: 0 = Reflection, 1 = Transmission (`SHD_HAIR_REFLECTION` / `_TRANSMISSION`).
   * The old node only has one lobe, so the other two are muted. The reflection variant keeps
   * the tinted TRT as well, because the `Color` of this node is meant to be seen. */
  float is_transmission = (component > 0.5f) ? 1.0f : 0.0f;
  hair_closures_emit(hair_tangent_get(T),
                     sigma_a,
                     saturate(roughness_u),
                     offset,
                     weight,
                     1.0f - is_transmission,
                     is_transmission,
                     1.0f - is_transmission);

  result = Closure(0);
}

void node_bsdf_hair_principled(float4 color,
                               float melanin,
                               float melanin_redness,
                               float4 tint,
                               float3 absorption_coefficient,
                               float aspect_ratio,
                               float roughness,
                               float radial_roughness,
                               float coat,
                               float ior,
                               float offset,
                               float random_color,
                               float random_roughness,
                               float random,
                               float weight,
                               float lobe_R,
                               float lobe_TT,
                               float lobe_TRT,
                               const float parametrization,
                               out Closure result)
{
  /* Per strand variation. Same formulas as Cycles: without them every hair of the head has
   * exactly the same colour and roughness, which is one of the tells of fake hair. `Random` is
   * 0 when the user has not wired `Hair Info > Random`, exactly as in Cycles. */
  float rand_offset = 2.0f * (random - 0.5f);
  float melanin_varied = max(melanin * (1.0f + rand_offset * saturate(random_color)), 0.0f);
  float roughness_varied = saturate(roughness *
                                    (1.0f + rand_offset * saturate(random_roughness)));

  float3 sigma_a;
  if (parametrization > 1.5f) {
    /* SHD_PRINCIPLED_HAIR_DIRECT_ABSORPTION. */
    sigma_a = max(absorption_coefficient, float3(0.0f));
  }
  else if (parametrization > 0.5f) {
    /* SHD_PRINCIPLED_HAIR_PIGMENT_CONCENTRATION. */
    float redness = saturate(melanin_redness);
    sigma_a = hair_sigma_a_from_concentration(melanin_varied * (1.0f - redness),
                                              melanin_varied * redness);
    /* The tint is an extra dye layer on top of the pigment, as in Cycles. */
    sigma_a += hair_sigma_a_from_reflectance(max(tint.rgb, float3(0.0f)),
                                             saturate(radial_roughness));
  }
  else {
    /* SHD_PRINCIPLED_HAIR_REFLECTANCE. */
    sigma_a = hair_sigma_a_from_reflectance(max(color.rgb, float3(0.0f)),
                                            saturate(radial_roughness));
  }

  hair_closures_emit(hair_tangent_get(float3(0.0f)),
                     sigma_a,
                     roughness_varied,
                     offset,
                     weight,
                     lobe_R,
                     lobe_TT,
                     lobe_TRT);

  result = Closure(0);
}
