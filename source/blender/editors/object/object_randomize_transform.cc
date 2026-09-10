/* SPDX-FileCopyrightText: 2010-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edobj
 *
 * Migracion de `scripts/startup/bl_operators/object_randomize_transform.py` (Carril C,
 * Flipendo). Mismo idname `object.randomize_transform`, mismas propiedades.
 *
 * Trampa deliberada y documentada: el Python usaba el modulo `random` de CPython
 * (Mersenne Twister MT19937) sembrado con `random.seed(random_seed)`. Para que la
 * linea base numerica congelada con el binario Python siga siendo valida byte a byte,
 * este fichero reimplementa en C++ el MISMO generador y el MISMO algoritmo de siembra
 * que usa CPython para enteros no negativos (que es el unico caso posible: la
 * propiedad `random_seed` es un IntProperty con min=0), en vez de usar `BLI_rng` (que
 * es un generador distinto y habria cambiado el resultado observable). Ver
 * `PythonCompatRandom` mas abajo; algoritmo tomado de `Modules/_randommodule.c` de
 * CPython (dominio publico / PSF License, mismo que el propio interprete que ya
 * empaqueta Blender).
 */

#include "DNA_object_types.h"

#include "BLI_math_rotation.h"
#include "BLI_math_vector.h"

#include "BKE_armature.hh"
#include "BKE_context.hh"
#include "BKE_layer.hh"

#include "DEG_depsgraph.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "ED_object.hh"

#include "object_intern.hh"

namespace blender::ed::object {

/* -------------------------------------------------------------------- */
/** \name Generador compatible con `random.Random` de Python (MT19937)
 * \{ */

namespace {

class PythonCompatRandom {
 public:
  explicit PythonCompatRandom(uint32_t seed)
  {
    /* Python `random.seed(n)` con n entero >= 0 (nuestro caso: IntProperty con
     * min=0) usa `init_by_array` con una unica palabra de 32 bits: n en si mismo
     * (o `key = [0]` si n == 0, ya que `keyused` nunca baja de 1). */
    uint32_t key[1] = {seed};
    init_by_array(key, 1);
  }

  /* Igual que `random.uniform(a, b)`: `a + (b - a) * random()`. Llama siempre al
   * generador (dos palabras de 32 bits), incluso si el resultado no se usa: el
   * Python original tambien lo hace en las ramas "no seleccionadas" para no
   * desincronizar el flujo de numeros del resto de objetos. */
  double uniform(double a, double b)
  {
    return a + (b - a) * random_res53();
  }

 private:
  static constexpr int N = 624;
  static constexpr int M = 397;
  static constexpr uint32_t MATRIX_A = 0x9908b0dfU;
  static constexpr uint32_t UPPER_MASK = 0x80000000U;
  static constexpr uint32_t LOWER_MASK = 0x7fffffffU;

  uint32_t state_[N];
  int index_ = N + 1;

  void init_genrand(uint32_t s)
  {
    state_[0] = s;
    for (int i = 1; i < N; i++) {
      state_[i] = (1812433253U * (state_[i - 1] ^ (state_[i - 1] >> 30)) + uint32_t(i));
    }
    index_ = N;
  }

  void init_by_array(const uint32_t *init_key, size_t key_length)
  {
    init_genrand(19650218U);
    size_t i = 1, j = 0;
    size_t k = (size_t(N) > key_length) ? size_t(N) : key_length;
    for (; k; k--) {
      state_[i] = (state_[i] ^ ((state_[i - 1] ^ (state_[i - 1] >> 30)) * 1664525U)) +
                 init_key[j] + uint32_t(j);
      i++;
      j++;
      if (i >= size_t(N)) {
        state_[0] = state_[N - 1];
        i = 1;
      }
      if (j >= key_length) {
        j = 0;
      }
    }
    for (k = N - 1; k; k--) {
      state_[i] = (state_[i] ^ ((state_[i - 1] ^ (state_[i - 1] >> 30)) * 1566083941U)) -
                 uint32_t(i);
      i++;
      if (i >= size_t(N)) {
        state_[0] = state_[N - 1];
        i = 1;
      }
    }
    state_[0] = 0x80000000U;
  }

  uint32_t genrand_uint32()
  {
    static const uint32_t mag01[2] = {0x0U, MATRIX_A};
    uint32_t y;

    if (index_ >= N) {
      int kk;
      for (kk = 0; kk < N - M; kk++) {
        y = (state_[kk] & UPPER_MASK) | (state_[kk + 1] & LOWER_MASK);
        state_[kk] = state_[kk + M] ^ (y >> 1) ^ mag01[y & 0x1U];
      }
      for (; kk < N - 1; kk++) {
        y = (state_[kk] & UPPER_MASK) | (state_[kk + 1] & LOWER_MASK);
        state_[kk] = state_[kk + (M - N)] ^ (y >> 1) ^ mag01[y & 0x1U];
      }
      y = (state_[N - 1] & UPPER_MASK) | (state_[0] & LOWER_MASK);
      state_[N - 1] = state_[M - 1] ^ (y >> 1) ^ mag01[y & 0x1U];
      index_ = 0;
    }

    y = state_[index_++];
    y ^= (y >> 11);
    y ^= (y << 7) & 0x9d2c5680U;
    y ^= (y << 15) & 0xefc60000U;
    y ^= (y >> 18);
    return y;
  }

  /* Igual que `genrand_res53`/`random.random()`: numero en [0, 1) con 53 bits. */
  double random_res53()
  {
    const uint32_t a = genrand_uint32() >> 5, b = genrand_uint32() >> 6;
    return (a * 67108864.0 + b) * (1.0 / 9007199254740992.0);
  }
};

}  // namespace

/** \} */

static void rand_vec3(PythonCompatRandom &rng, const float range[3], float r_vec[3])
{
  for (int i = 0; i < 3; i++) {
    r_vec[i] = float(rng.uniform(-double(range[i]), double(range[i])));
  }
}

static void randomize_selected(bContext *C,
                               const int seed,
                               const bool delta,
                               const bool use_loc,
                               const float loc[3],
                               const bool use_rot,
                               const float rot[3],
                               const bool use_scale,
                               const bool scale_even,
                               const float scale[3])
{
  PythonCompatRandom rng{uint32_t(seed)};

  CTX_DATA_BEGIN (C, Object *, ob, selected_objects) {
    if (use_loc) {
      float vec[3];
      rand_vec3(rng, loc, vec);
      if (delta) {
        add_v3_v3(ob->dloc, vec);
      }
      else {
        add_v3_v3(ob->loc, vec);
      }
    }
    else {
      /* Consumir el mismo numero de valores aleatorios que el Python original
       * para no desincronizar el resto de objetos de la seleccion. */
      rng.uniform(0.0, 0.0);
      rng.uniform(0.0, 0.0);
      rng.uniform(0.0, 0.0);
    }

    if (use_rot) {
      float vec[3];
      rand_vec3(rng, rot, vec);

      const short rotation_mode = ob->rotmode;
      if (ELEM(rotation_mode, ROT_MODE_QUAT, ROT_MODE_AXISANGLE)) {
        BKE_rotMode_change_values(
            ob->quat, ob->rot, ob->rotAxis, &ob->rotAngle, ob->rotmode, ROT_MODE_EUL);
        ob->rotmode = ROT_MODE_EUL;
      }

      if (delta) {
        ob->drot[0] += vec[0];
        ob->drot[1] += vec[1];
        ob->drot[2] += vec[2];
      }
      else {
        ob->rot[0] += vec[0];
        ob->rot[1] += vec[1];
        ob->rot[2] += vec[2];
      }

      BKE_rotMode_change_values(
          ob->quat, ob->rot, ob->rotAxis, &ob->rotAngle, ob->rotmode, rotation_mode);
      ob->rotmode = rotation_mode;
    }
    else {
      rng.uniform(0.0, 0.0);
      rng.uniform(0.0, 0.0);
      rng.uniform(0.0, 0.0);
    }

    if (use_scale) {
      float org_scale[3];
      copy_v3_v3(org_scale, delta ? ob->dscale : ob->scale);

      const float sca_x = float(rng.uniform(-scale[0] + 2.0, scale[0]));
      const float sca_y = float(rng.uniform(-scale[1] + 2.0, scale[1]));
      const float sca_z = float(rng.uniform(-scale[2] + 2.0, scale[2]));

      float result[3];
      if (scale_even) {
        result[0] = sca_x * org_scale[0];
        result[1] = sca_x * org_scale[1];
        result[2] = sca_x * org_scale[2];
      }
      else {
        result[0] = sca_x * org_scale[0];
        result[1] = sca_y * org_scale[1];
        result[2] = sca_z * org_scale[2];
      }

      copy_v3_v3(delta ? ob->dscale : ob->scale, result);
    }
    else {
      rng.uniform(0.0, 0.0);
      rng.uniform(0.0, 0.0);
      rng.uniform(0.0, 0.0);
    }

    DEG_id_tag_update(&ob->id, ID_RECALC_TRANSFORM);
  }
  CTX_DATA_END;

  WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, nullptr);
}

static wmOperatorStatus randomize_transform_exec(bContext *C, wmOperator *op)
{
  const int seed = RNA_int_get(op->ptr, "random_seed");
  const bool delta = RNA_boolean_get(op->ptr, "use_delta");

  const bool use_loc = RNA_boolean_get(op->ptr, "use_loc");
  float loc[3];
  RNA_float_get_array(op->ptr, "loc", loc);

  const bool use_rot = RNA_boolean_get(op->ptr, "use_rot");
  float rot[3];
  RNA_float_get_array(op->ptr, "rot", rot);

  const bool use_scale = RNA_boolean_get(op->ptr, "use_scale");
  const bool scale_even = RNA_boolean_get(op->ptr, "scale_even");
  float scale[3];
  RNA_float_get_array(op->ptr, "scale", scale);

  randomize_selected(
      C, seed, delta, use_loc, loc, use_rot, rot, use_scale, scale_even, scale);

  return OPERATOR_FINISHED;
}

static bool randomize_transform_poll(bContext *C)
{
  return CTX_data_mode_enum(C) == CTX_MODE_OBJECT;
}

void OBJECT_OT_randomize_transform(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Randomize Transform";
  ot->description = "Randomize objects location, rotation, and scale";
  ot->idname = "OBJECT_OT_randomize_transform";

  /* API callbacks. */
  ot->exec = randomize_transform_exec;
  ot->poll = randomize_transform_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  PropertyRNA *prop;
  RNA_def_int(
      ot->srna, "random_seed", 0, 0, 10000, "Random Seed", "Seed value for the random generator", 0, 10000);
  RNA_def_boolean(ot->srna,
                 "use_delta",
                 false,
                 "Transform Delta",
                 "Randomize delta transform values instead of regular transform");
  RNA_def_boolean(
      ot->srna, "use_loc", true, "Randomize Location", "Randomize the location values");
  prop = RNA_def_float_vector(ot->srna,
                              "loc",
                              3,
                              nullptr,
                              -100.0f,
                              100.0f,
                              "Location",
                              "Maximum distance the objects can spread over each axis",
                              -100.0f,
                              100.0f);
  RNA_def_property_subtype(prop, PROP_TRANSLATION);
  RNA_def_boolean(
      ot->srna, "use_rot", true, "Randomize Rotation", "Randomize the rotation values");
  prop = RNA_def_float_vector(ot->srna,
                              "rot",
                              3,
                              nullptr,
                              -3.141592f,
                              3.141592f,
                              "Rotation",
                              "Maximum rotation over each axis",
                              -3.141592f,
                              3.141592f);
  RNA_def_property_subtype(prop, PROP_EULER);
  RNA_def_boolean(ot->srna, "use_scale", true, "Randomize Scale", "Randomize the scale values");
  RNA_def_boolean(
      ot->srna, "scale_even", false, "Scale Even", "Use the same scale value for all axis");
  static const float scale_default[3] = {1.0f, 1.0f, 1.0f};
  RNA_def_float_vector(ot->srna,
                       "scale",
                       3,
                       scale_default,
                       -100.0f,
                       100.0f,
                       "Scale",
                       "Maximum scale randomization over each axis",
                       -100.0f,
                       100.0f);
}

}  // namespace blender::ed::object
