/* SPDX-FileCopyrightText: 2009 Campbell Barton
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 *
 * Puerto nativo en C++ de `scripts/startup/bl_operators/vertexpaint_dirt.py`
 * (`paint.vertex_color_dirt`, Flipendo carril C). Simula la acumulacion de suciedad en
 * los pliegues de una superficie comparando la normal de cada vertice con la direccion
 * media hacia los vertices conectados a el; con `dirt_only` desactivado tambien simula
 * el desgaste de las zonas convexas. Algoritmo original de Keith "Wahooney" Boshoff.
 *
 * FIDELIDAD NUMERICA
 * ------------------
 * El objetivo no es "un puerto razonable" sino el MISMO resultado observable que el
 * Python, byte a byte en el atributo de color. Eso obliga a reproducir tres cosas que
 * no son evidentes leyendo el .py:
 *
 * 1. `.color` de un atributo BYTE_COLOR **no** son los bytes crudos: el getter de RNA
 *    (`rna_ByteColorAttributeValue_color_get`) devuelve el color decodificado de sRGB a
 *    lineal, y el setter lo vuelve a codificar. `col[k] = tone * col[k]` multiplica por
 *    tanto en espacio LINEAL, no sobre el byte. Multiplicar el byte directamente da
 *    colores visiblemente distintos (la curva sRGB no es lineal).
 * 2. `col[k] = ...` sobre un `bpy_prop_array` pasa por
 *    `RNA_property_float_set_index()`, que hace get-array -> modifica un indice ->
 *    set-array. Es decir: cada uno de los tres canales hace un viaje completo
 *    bytes -> lineal -> bytes. Se replica canal a canal, no de una vez.
 * 3. mathutils y `array.array("f")` mezclan float y double de una forma concreta:
 *    `Vector.normalized()` calcula el modulo al cuadrado en double sumando de la ultima
 *    componente hacia la primera y luego multiplica por el reciproco en float
 *    (`normalize_vn`); `Vector.dot()` (`dot_vn_vn`) acumula en double productos hechos
 *    en float, tambien de z a x; `vec /= n` multiplica por `1.0f/n` en float
 *    (`mul_vn_fl`); y cada `vert_tone[j] += ...` de la fase de desenfoque trunca a
 *    float32 al guardar, aunque el producto se haya hecho en double.
 *
 * Ver `politicas/VERTEX-DIRT-A-CPP.md` y `tests/flipendo/meshops/`.
 */

#include <algorithm>
#include <cmath>
#include <string>

#include "MEM_guardedalloc.h"

#include "BLI_color.hh"
#include "BLI_math_color.h"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_mesh.hh"

#include "DEG_depsgraph.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "mesh_intern.hh" /* own include */

using namespace blender;

/* -------------------------------------------------------------------------- */
/** \name Aritmetica identica a la de mathutils
 * \{ */

static double sqr_db(const double d)
{
  return d * d;
}

/* `Vector.normalized()` -> `normalize_vn()`: modulo al cuadrado en double acumulado de
 * la ultima componente a la primera, raiz en double truncada a float, y multiplicacion
 * por el reciproco en float. El umbral 1.0e-35 y el vector cero son los de Blender. */
static void mathutils_normalized_v3(float r[3], const float a[3])
{
  const double d = sqr_db(double(a[2])) + sqr_db(double(a[1])) + sqr_db(double(a[0]));
  if (d > 1.0e-35) {
    const float d_sqrt = float(std::sqrt(d));
    const float inv = 1.0f / d_sqrt;
    r[2] = a[2] * inv;
    r[1] = a[1] * inv;
    r[0] = a[0] * inv;
  }
  else {
    r[0] = r[1] = r[2] = 0.0f;
  }
}

/* `Vector.dot()` -> `dot_vn_vn()`: productos en float promovidos a double y acumulados
 * de z a x. El orden importa para la ultima cifra. */
static double mathutils_dot_v3(const float a[3], const float b[3])
{
  double d = 0.0;
  d += double(a[2] * b[2]);
  d += double(a[1] * b[1]);
  d += double(a[0] * b[0]);
  return d;
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name Atributo de color activo
 * \{ */

/* Equivalente de `ensure_active_color_attribute()` del Python:
 *
 *   if me.attributes.active_color: return me.attributes.active_color
 *   return me.color_attributes.new("Color", 'BYTE_COLOR', 'CORNER')
 *
 * `me.color_attributes.new()` es `rna_AttributeGroupID_new()`, que llama a
 * `BKE_attribute_new()` (nombre unico incluido) y, SOLO SI estaban vacios, apunta
 * `active_color_attribute` y `default_color_attribute` a la capa nueva. Se reproduce
 * tal cual: poner el activo incondicionalmente seria un cambio de comportamiento. */
static const CustomDataLayer *ensure_active_color_attribute(Mesh &mesh, ReportList *reports)
{
  if (const CustomDataLayer *active = BKE_id_attributes_color_find(&mesh.id,
                                                                   mesh.active_color_attribute))
  {
    return active;
  }

  AttributeOwner owner = AttributeOwner::from_id(&mesh.id);
  const CustomDataLayer *layer = BKE_attribute_new(
      owner, "Color", CD_PROP_BYTE_COLOR, bke::AttrDomain::Corner, reports);
  if (layer == nullptr) {
    return nullptr;
  }

  if (mesh.active_color_attribute == nullptr) {
    mesh.active_color_attribute = BLI_strdup(layer->name);
  }
  if (mesh.default_color_attribute == nullptr) {
    mesh.default_color_attribute = BLI_strdup(layer->name);
  }

  return layer;
}

/* `col[k] = tone * col[k]` sobre un atributo BYTE_COLOR, canal a canal, con el viaje
 * completo bytes -> lineal -> bytes en cada canal (ver punto 2 de la cabecera). El
 * recorte a [0,1] es el de `RNA_property_float_clamp()` (la propiedad declara ese
 * rango); `linearrgb_to_srgb_uchar4()` recorta ademas a [0,255]. */
static void scale_byte_color_like_rna(uchar col[4], const double tone)
{
  for (int k = 0; k < 3; k++) {
    float values[4];
    srgb_to_linearrgb_uchar4(values, col);
    float value = float(tone * double(values[k]));
    value = std::clamp(value, 0.0f, 1.0f);
    values[k] = value;
    linearrgb_to_srgb_uchar4(col, values);
  }
}

/* Lo mismo para FLOAT_COLOR (`MPropCol`). Ahi `.color` es la memoria DNA directamente:
 * ni conversion de espacio de color ni rango declarado, asi que no hay recorte. */
static void scale_float_color_like_rna(float col[4], const double tone)
{
  for (int k = 0; k < 3; k++) {
    col[k] = float(tone * double(col[k]));
  }
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name Operador
 * \{ */

static wmOperatorStatus vertex_color_dirt_exec(bContext *C, wmOperator *op)
{
  Object *ob = CTX_data_active_object(C);
  Mesh *mesh = static_cast<Mesh *>(ob->data);

  /* Las propiedades se leen como double desde el primer momento: en Python
   * `self.blur_strength` y compania son floats de Python (double) obtenidos de un
   * float32 de RNA, y toda la aritmetica posterior es en double. */
  const double blur_strength = double(RNA_float_get(op->ptr, "blur_strength"));
  const int blur_iterations = RNA_int_get(op->ptr, "blur_iterations");
  const double clamp_clean = double(RNA_float_get(op->ptr, "clean_angle"));
  const double clamp_dirt = double(RNA_float_get(op->ptr, "dirt_angle"));
  const bool dirt_only = RNA_boolean_get(op->ptr, "dirt_only");
  const bool normalize = RNA_boolean_get(op->ptr, "normalize");

  const int verts_num = mesh->verts_num;

  /* `min(vert_tone)` sobre una lista vacia es un ValueError en Python: el operador
   * moria con traza. Aqui se cancela limpiamente (divergencia deliberada, ver informe). */
  if (normalize && verts_num == 0) {
    return OPERATOR_CANCELLED;
  }

  const Span<float3> positions = mesh->vert_positions();
  const Span<float3> vert_normals = mesh->vert_normals();
  const Span<int2> edges = mesh->edges();
  const OffsetIndices faces = mesh->faces();
  const Span<int> corner_verts = mesh->corner_verts();

  /* Tabla de vertices conectados por arista, en el mismo orden en que Python recorre
   * `me.edges`: el orden de esta lista decide el orden de las sumas en float de mas
   * abajo, y con el la ultima cifra del resultado. */
  Vector<Vector<int>> con(verts_num);
  for (const int2 &edge : edges) {
    con[edge[0]].append(edge[1]);
    con[edge[1]].append(edge[0]);
  }

  Vector<float> vert_tone(verts_num, 0.0f);

  for (int i = 0; i < verts_num; i++) {
    float vec[3] = {0.0f, 0.0f, 0.0f};
    const float *no = vert_normals[i];
    const float *co = positions[i];

    for (const int c : con[i]) {
      const float *other = positions[c];
      const float sub[3] = {other[0] - co[0], other[1] - co[1], other[2] - co[2]};
      float unit[3];
      mathutils_normalized_v3(unit, sub);
      vec[0] += unit[0];
      vec[1] += unit[1];
      vec[2] += unit[2];
    }

    const int tot_con = con[i].size();
    double ang;
    if (tot_con == 0) {
      ang = M_PI / 2.0; /* Se asume 90 grados, o sea plano. */
    }
    else {
      const float inv = 1.0f / float(tot_con); /* `vec /= tot_con` es `mul_vn_fl()`. */
      vec[0] *= inv;
      vec[1] *= inv;
      vec[2] *= inv;

      /* `math.acos()` de Python lanza ValueError si el producto escalar se sale de
       * [-1,1] por error de redondeo, y el operador entero moria. Se recorta, que es
       * lo que hace el resto del C++ de Blender; solo cambia el caso degenerado. */
      const double dot = std::clamp(mathutils_dot_v3(no, vec), -1.0, 1.0);
      ang = std::acos(dot);
    }

    ang = std::max(clamp_dirt, ang);
    if (!dirt_only) {
      ang = std::min(clamp_clean, ang);
    }

    vert_tone[i] = float(ang); /* `array.array("f")`: se trunca al guardar. */
  }

  /* Desenfoque. Cada `+=` de Python guarda en el array de float32, asi que el
   * acumulador se trunca despues de CADA vecino aunque el producto sea en double. */
  for (int iteration = 0; iteration < blur_iterations; iteration++) {
    const Vector<float> orig_vert_tone = vert_tone;
    for (int j = 0; j < verts_num; j++) {
      float tone = vert_tone[j];
      for (const int v : con[j]) {
        tone = float(double(tone) + blur_strength * double(orig_vert_tone[v]));
      }
      tone = float(double(tone) / (double(con[j].size()) * blur_strength + 1.0));
      vert_tone[j] = tone;
    }
  }

  double min_tone, max_tone;
  if (normalize) {
    min_tone = double(*std::min_element(vert_tone.begin(), vert_tone.end()));
    max_tone = double(*std::max_element(vert_tone.begin(), vert_tone.end()));
  }
  else {
    min_tone = clamp_dirt;
    max_tone = clamp_clean;
  }

  double tone_range = max_tone - min_tone;
  if (tone_range < 0.0001) {
    tone_range = 0.0; /* Flojo, pero no se cancela: ver #43345. */
  }
  else {
    tone_range = 1.0 / tone_range;
  }

  const CustomDataLayer *layer = ensure_active_color_attribute(*mesh, op->reports);
  if (layer == nullptr) {
    return OPERATOR_CANCELLED;
  }
  const std::string color_name = layer->name;
  const bool byte_color = (layer->type == CD_PROP_BYTE_COLOR);

  bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
  const std::optional<bke::AttributeMetaData> meta = attributes.lookup_meta_data(color_name);
  if (!meta) {
    return OPERATOR_CANCELLED;
  }
  const bool point_domain = (meta->domain == bke::AttrDomain::Point);

  bke::GSpanAttributeWriter color_attr = attributes.lookup_for_write_span(color_name);
  if (!color_attr) {
    return OPERATOR_CANCELLED;
  }

  const bool use_paint_mask = (mesh->editflag & ME_EDIT_PAINT_FACE_SEL) != 0;
  const VArray<bool> select_poly = *attributes.lookup_or_default<bool>(
      ".select_poly", bke::AttrDomain::Face, false);

  for (const int face_index : faces.index_range()) {
    if (use_paint_mask && !select_poly[face_index]) {
      continue;
    }
    for (const int loop_index : faces[face_index]) {
      const int v = corner_verts[loop_index];
      const int element = point_domain ? v : loop_index;

      double tone = double(vert_tone[v]);
      tone = (tone - min_tone) * tone_range;
      if (dirt_only) {
        tone = std::min(tone, 0.5) * 2.0;
      }

      if (byte_color) {
        uchar *col = reinterpret_cast<uchar *>(
            &color_attr.span.typed<ColorGeometry4b>()[element]);
        scale_byte_color_like_rna(col, tone);
      }
      else {
        float *col = reinterpret_cast<float *>(
            &color_attr.span.typed<ColorGeometry4f>()[element]);
        scale_float_color_like_rna(col, tone);
      }
    }
  }

  color_attr.finish();

  /* `me.update()` del Python: `DEG_id_tag_update()` + notificador NC_GEOM|ND_DATA. */
  DEG_id_tag_update(&mesh->id, ID_RECALC_GEOMETRY);
  WM_event_add_notifier(C, NC_GEOM | ND_DATA, mesh);

  return OPERATOR_FINISHED;
}

static bool vertex_color_dirt_poll(bContext *C)
{
  /* `obj and obj.type == 'MESH'`, igual que el classmethod poll del Python. */
  const Object *ob = CTX_data_active_object(C);
  return ob != nullptr && ob->type == OB_MESH;
}

void PAINT_OT_vertex_color_dirt(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Dirty Vertex Colors";
  ot->idname = "PAINT_OT_vertex_color_dirt";
  ot->description = "Generate a dirt map gradient based on cavity";

  /* API callbacks. */
  ot->exec = vertex_color_dirt_exec;
  ot->poll = vertex_color_dirt_poll;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties: mismos nombres, tipos, rangos, defectos y textos que el Python. */
  RNA_def_float(ot->srna,
                "blur_strength",
                1.0f,
                0.01f,
                1.0f,
                "Blur Strength",
                "Blur strength per iteration",
                0.01f,
                1.0f);
  RNA_def_int(ot->srna,
              "blur_iterations",
              1,
              0,
              40,
              "Blur Iterations",
              "Number of times to blur the colors (higher blurs more)",
              0,
              40);

  PropertyRNA *prop;
  prop = RNA_def_float(ot->srna,
                       "clean_angle",
                       float(M_PI),
                       0.0f,
                       float(M_PI),
                       "Highlight Angle",
                       "Less than 90 limits the angle used in the tonal range",
                       0.0f,
                       float(M_PI));
  RNA_def_property_subtype(prop, PROP_ANGLE);

  prop = RNA_def_float(ot->srna,
                       "dirt_angle",
                       0.0f,
                       0.0f,
                       float(M_PI),
                       "Dirt Angle",
                       "Less than 90 limits the angle used in the tonal range",
                       0.0f,
                       float(M_PI));
  RNA_def_property_subtype(prop, PROP_ANGLE);

  RNA_def_boolean(
      ot->srna, "dirt_only", false, "Dirt Only", "Don't calculate cleans for convex areas");
  RNA_def_boolean(
      ot->srna, "normalize", true, "Normalize", "Normalize the colors, increasing the contrast");
}

/** \} */
