/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edmesh
 *
 * Implementacion de `--fl-selftest-mesh-ops`. Ver `FL_mesh_ops_selftest.hh`.
 */

#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <optional>

#include "BLI_color.hh"
#include "BLI_listbase.h"
#include "BLI_path_utils.hh"
#include "BLI_string.h"
#include "BLI_math_color.h"
#include "BLI_vector.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_attribute.h"
#include "BKE_attribute.hh"
#include "BKE_context.hh"
#include "BKE_customdata.hh"
#include "BKE_layer.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_appdir.hh"
#include "BKE_mesh.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "RNA_access.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "FL_selftest_compare.hh"

#include "FL_mesh_ops_selftest.hh"

namespace flipendo::mesh_ops_selftest {

using namespace blender;

namespace {

/* -------------------------------------------------------------------------- */
/** \name Utilidades de escena
 * \{ */

/* Propiedades del operador bajo prueba. Se declaran TODAS en cada llamada, siempre:
 * `WM_operator_name_call()` pasa por `WM_operator_last_properties_init()`, que rellena
 * las propiedades no puestas con las de la ULTIMA ejecucion del operador (es un
 * operador OPTYPE_REGISTER). El `bpy.ops...()` de Python no arrastra ese estado, asi
 * que un arnes que dejara propiedades sin poner compararia peras con manzanas: el caso
 * N heredaria los angulos del caso N-1. Trampa real, costo cinco casos de este arnes. */
struct DirtProps {
  int blur_iterations = 1;
  float blur_strength = 1.0f;
  float clean_angle = float(M_PI);
  float dirt_angle = 0.0f;
  bool dirt_only = false;
  bool normalize = true;
};

static void call_vertex_color_dirt(bContext *C, const DirtProps &props)
{
  PointerRNA ptr;
  WM_operator_properties_create(&ptr, "paint.vertex_color_dirt");
  RNA_int_set(&ptr, "blur_iterations", props.blur_iterations);
  RNA_float_set(&ptr, "blur_strength", props.blur_strength);
  RNA_float_set(&ptr, "clean_angle", props.clean_angle);
  RNA_float_set(&ptr, "dirt_angle", props.dirt_angle);
  RNA_boolean_set(&ptr, "dirt_only", props.dirt_only);
  RNA_boolean_set(&ptr, "normalize", props.normalize);
  WM_operator_name_call(C, "paint.vertex_color_dirt", WM_OP_EXEC_DEFAULT, &ptr, nullptr);
  WM_operator_properties_free(&ptr);
}

/* Equivalente de `purge()` del guion de captura: deja la escena sin objetos ni mallas,
 * para que cada caso parta de cero igual en los dos binarios. */
static void purge(bContext *C)
{
  Main *bmain = CTX_data_main(C);
  LISTBASE_FOREACH_MUTABLE (Object *, ob, &bmain->objects) {
    BKE_id_delete(bmain, ob);
  }
  LISTBASE_FOREACH_MUTABLE (Mesh *, mesh, &bmain->meshes) {
    if (mesh->id.us == 0) {
      BKE_id_delete(bmain, mesh);
    }
  }
  DEG_relations_tag_update(bmain);
}

static void add_primitive(bContext *C,
                          const char *idname,
                          const std::function<void(PointerRNA *)> &fill)
{
  PointerRNA ptr;
  WM_operator_properties_create(&ptr, idname);
  const float loc[3] = {0.0f, 0.0f, 0.0f};
  const float rot[3] = {0.0f, 0.0f, 0.0f};
  RNA_float_set_array(&ptr, "location", loc);
  RNA_float_set_array(&ptr, "rotation", rot);
  if (fill) {
    fill(&ptr);
  }
  WM_operator_name_call(C, idname, WM_OP_EXEC_DEFAULT, &ptr, nullptr);
  WM_operator_properties_free(&ptr);
}

static void set_selection(bContext *C, Object *active, const Vector<Object *> &selected)
{
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  BKE_view_layer_synced_ensure(scene, view_layer);
  BKE_view_layer_base_deselect_all(scene, view_layer);
  for (Object *ob : selected) {
    if (Base *base = BKE_view_layer_base_find(view_layer, ob)) {
      base->flag |= BASE_SELECTED;
    }
  }
  if (Base *active_base = BKE_view_layer_base_find(view_layer, active)) {
    BKE_view_layer_base_select_and_set_active(view_layer, active_base);
  }
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name Volcado
 * \{ */

static const char *domain_name(const bke::AttrDomain domain)
{
  switch (domain) {
    case bke::AttrDomain::Point:
      return "POINT";
    case bke::AttrDomain::Corner:
      return "CORNER";
    case bke::AttrDomain::Edge:
      return "EDGE";
    case bke::AttrDomain::Face:
      return "FACE";
    default:
      return "OTHER";
  }
}

/* Un caso del volcado. El formato tiene que coincidir carácter a carácter con lo que
 * escribe la captura contra el binario de referencia (ver informe C2). Para
 * BYTE_COLOR se vuelcan los bytes crudos: en Python se recuperan exactos desde
 * `color_srgb` (que es `col->r / 255.0f`) con `round(v * 255)`. */
static void dump_case(FILE *f, int index, const char *label, const Object *ob)
{
  const Mesh *mesh = static_cast<const Mesh *>(ob->data);
  fprintf(f, "case=%d op=paint.vertex_color_dirt %s\n", index, label);
  fprintf(f,
          "  mesh verts=%d edges=%d faces=%d loops=%d\n",
          mesh->verts_num,
          mesh->edges_num,
          mesh->faces_num,
          mesh->corners_num);

  const CustomDataLayer *layer = BKE_id_attributes_color_find(&mesh->id,
                                                              mesh->active_color_attribute);
  if (layer == nullptr) {
    fprintf(f, "  attr none\n");
    return;
  }

  const bke::AttributeAccessor attributes = mesh->attributes();
  const std::optional<bke::AttributeMetaData> meta = attributes.lookup_meta_data(layer->name);
  if (!meta) {
    fprintf(f, "  attr none\n");
    return;
  }
  const int elems = attributes.domain_size(meta->domain);
  const bool byte_color = (layer->type == CD_PROP_BYTE_COLOR);

  fprintf(f,
          "  attr name=%s domain=%s type=%s elems=%d\n",
          layer->name,
          domain_name(meta->domain),
          byte_color ? "BYTE_COLOR" : "FLOAT_COLOR",
          elems);

  const bke::GAttributeReader reader = attributes.lookup(layer->name);
  if (byte_color) {
    const VArraySpan<ColorGeometry4b> colors = reader.varray.typed<ColorGeometry4b>();
    for (const int i : colors.index_range()) {
      const ColorGeometry4b &c = colors[i];
      fprintf(f, "  %d %d %d %d %d\n", i, int(c.r), int(c.g), int(c.b), int(c.a));
    }
  }
  else {
    const VArraySpan<ColorGeometry4f> colors = reader.varray.typed<ColorGeometry4f>();
    for (const int i : colors.index_range()) {
      const ColorGeometry4f &c = colors[i];
      fprintf(f,
              "  %d %.9g %.9g %.9g %.9g\n",
              i,
              double(c.r),
              double(c.g),
              double(c.b),
              double(c.a));
    }
  }
}

/** \} */

}  // namespace

/* -------------------------------------------------------------------------- */
/** \name Bateria de casos
 * \{ */

bool dump(bContext *C, const char *filepath)
{
  FILE *f = fopen(filepath, "w");
  if (f == nullptr) {
    fprintf(stderr, "fl-selftest-mesh-ops: no se pudo escribir '%s'\n", filepath);
    return false;
  }
  fprintf(f, "# FL-MESH-OPS-SELFTEST v1\n");

  int index = 0;

  /* --- Suzanne: topologia irregular, valencias 3/4/5, bordes abiertos. --- */
  struct MonkeyCase {
    const char *label;
    DirtProps props;
  };
  MonkeyCase monkey_cases[5];
  monkey_cases[0].label = "monkey defaults";
  monkey_cases[1].label = "monkey blur0";
  monkey_cases[1].props.blur_iterations = 0;
  monkey_cases[2].label = "monkey blur5 strength0.35";
  monkey_cases[2].props.blur_iterations = 5;
  monkey_cases[2].props.blur_strength = 0.35f;
  monkey_cases[3].label = "monkey dirtonly nonorm";
  monkey_cases[3].props.dirt_only = true;
  monkey_cases[3].props.normalize = false;
  monkey_cases[3].props.clean_angle = 1.5f;
  monkey_cases[3].props.dirt_angle = 0.3f;
  monkey_cases[4].label = "monkey nonorm";
  monkey_cases[4].props.normalize = false;

  for (const MonkeyCase &mc : monkey_cases) {
    purge(C);
    add_primitive(C, "mesh.primitive_monkey_add", [](PointerRNA *ptr) {
      RNA_float_set(ptr, "size", 2.0f);
    });
    Object *ob = CTX_data_active_object(C);
    call_vertex_color_dirt(C, mc.props);
    dump_case(f, index++, mc.label, ob);
  }

  /* --- Icoesfera: todos los vertices de valencia 5 o 6, superficie convexa. --- */
  {
    purge(C);
    add_primitive(C, "mesh.primitive_ico_sphere_add", [](PointerRNA *ptr) {
      RNA_int_set(ptr, "subdivisions", 3);
      RNA_float_set(ptr, "radius", 1.5f);
    });
    Object *ob = CTX_data_active_object(C);
    DirtProps props;
    props.blur_iterations = 3;
    props.blur_strength = 0.75f;
    call_vertex_color_dirt(C, props);
    dump_case(f, index++, "icosphere blur3 strength0.75", ob);
  }

  /* --- Atributo FLOAT_COLOR en dominio POINT: rama de color en coma flotante y, de
   * paso, la multiplicacion repetida del mismo vertice una vez por cada bucle que lo
   * toca (el Python hace eso, y es lo que hay que reproducir). --- */
  {
    purge(C);
    add_primitive(C, "mesh.primitive_uv_sphere_add", [](PointerRNA *ptr) {
      RNA_int_set(ptr, "segments", 16);
      RNA_int_set(ptr, "ring_count", 8);
      RNA_float_set(ptr, "radius", 1.0f);
    });
    Object *ob = CTX_data_active_object(C);
    PointerRNA ptr;
    WM_operator_properties_create(&ptr, "geometry.color_attribute_add");
    RNA_string_set(&ptr, "name", "Col");
    RNA_enum_set_identifier(C, &ptr, "domain", "POINT");
    RNA_enum_set_identifier(C, &ptr, "data_type", "FLOAT_COLOR");
    const float color[4] = {0.8f, 0.5f, 0.2f, 1.0f};
    RNA_float_set_array(&ptr, "color", color);
    WM_operator_name_call(C, "geometry.color_attribute_add", WM_OP_EXEC_DEFAULT, &ptr, nullptr);
    WM_operator_properties_free(&ptr);
    call_vertex_color_dirt(C, DirtProps());
    dump_case(f, index++, "uvsphere floatcolor point", ob);
  }

  /* --- Lo mismo con BYTE_COLOR en POINT: acumula el redondeo a byte una vez por bucle. */
  {
    purge(C);
    add_primitive(C, "mesh.primitive_uv_sphere_add", [](PointerRNA *ptr) {
      RNA_int_set(ptr, "segments", 12);
      RNA_int_set(ptr, "ring_count", 6);
      RNA_float_set(ptr, "radius", 1.0f);
    });
    Object *ob = CTX_data_active_object(C);
    PointerRNA ptr;
    WM_operator_properties_create(&ptr, "geometry.color_attribute_add");
    RNA_string_set(&ptr, "name", "Col8");
    RNA_enum_set_identifier(C, &ptr, "domain", "POINT");
    RNA_enum_set_identifier(C, &ptr, "data_type", "BYTE_COLOR");
    const float color[4] = {0.9f, 0.4f, 0.1f, 1.0f};
    RNA_float_set_array(&ptr, "color", color);
    WM_operator_name_call(C, "geometry.color_attribute_add", WM_OP_EXEC_DEFAULT, &ptr, nullptr);
    WM_operator_properties_free(&ptr);
    DirtProps props;
    props.blur_iterations = 2;
    call_vertex_color_dirt(C, props);
    dump_case(f, index++, "uvsphere bytecolor point blur2", ob);
  }

  /* --- Cubo + cuatro vertices sueltos: la unica forma de llegar a la rama
   * `tot_con == 0` (angulo = pi/2). Con `normalize` esos vertices mueven el minimo y
   * el maximo, asi que cambian el color de TODA la malla: es observable. --- */
  {
    purge(C);
    add_primitive(C, "mesh.primitive_cube_add", [](PointerRNA *ptr) {
      RNA_float_set(ptr, "size", 2.0f);
    });
    Object *cube = CTX_data_active_object(C);
    {
      PointerRNA ptr;
      WM_operator_properties_create(&ptr, "mesh.primitive_plane_add");
      RNA_float_set(&ptr, "size", 1.0f);
      const float loc[3] = {3.0f, 0.0f, 0.0f};
      const float rot[3] = {0.0f, 0.0f, 0.0f};
      RNA_float_set_array(&ptr, "location", loc);
      RNA_float_set_array(&ptr, "rotation", rot);
      WM_operator_name_call(C, "mesh.primitive_plane_add", WM_OP_EXEC_DEFAULT, &ptr, nullptr);
      WM_operator_properties_free(&ptr);
    }
    Object *plane = CTX_data_active_object(C);

    {
      PointerRNA ptr;
      WM_operator_properties_create(&ptr, "object.mode_set");
      RNA_enum_set_identifier(C, &ptr, "mode", "EDIT");
      WM_operator_name_call(C, "object.mode_set", WM_OP_EXEC_DEFAULT, &ptr, nullptr);
      WM_operator_properties_free(&ptr);
    }
    {
      PointerRNA ptr;
      WM_operator_properties_create(&ptr, "mesh.select_all");
      RNA_enum_set_identifier(C, &ptr, "action", "SELECT");
      WM_operator_name_call(C, "mesh.select_all", WM_OP_EXEC_DEFAULT, &ptr, nullptr);
      WM_operator_properties_free(&ptr);
    }
    {
      PointerRNA ptr;
      WM_operator_properties_create(&ptr, "mesh.delete");
      RNA_enum_set_identifier(C, &ptr, "type", "EDGE_FACE");
      WM_operator_name_call(C, "mesh.delete", WM_OP_EXEC_DEFAULT, &ptr, nullptr);
      WM_operator_properties_free(&ptr);
    }
    {
      PointerRNA ptr;
      WM_operator_properties_create(&ptr, "object.mode_set");
      RNA_enum_set_identifier(C, &ptr, "mode", "OBJECT");
      WM_operator_name_call(C, "object.mode_set", WM_OP_EXEC_DEFAULT, &ptr, nullptr);
      WM_operator_properties_free(&ptr);
    }

    set_selection(C, cube, {plane, cube});
    WM_operator_name_call(C, "object.join", WM_OP_EXEC_DEFAULT, nullptr, nullptr);

    Object *ob = CTX_data_active_object(C);
    call_vertex_color_dirt(C, DirtProps());
    dump_case(f, index++, "cube+loose verts", ob);
  }

  /* --- Rejilla con mascara de pintura: solo se tocan las caras seleccionadas. --- */
  {
    purge(C);
    add_primitive(C, "mesh.primitive_grid_add", [](PointerRNA *ptr) {
      RNA_int_set(ptr, "x_subdivisions", 6);
      RNA_int_set(ptr, "y_subdivisions", 6);
      RNA_float_set(ptr, "size", 2.0f);
    });
    Object *ob = CTX_data_active_object(C);
    Mesh *mesh = static_cast<Mesh *>(ob->data);
    mesh->editflag |= ME_EDIT_PAINT_FACE_SEL;
    {
      bke::MutableAttributeAccessor attributes = mesh->attributes_for_write();
      bke::SpanAttributeWriter<bool> select_poly =
          attributes.lookup_or_add_for_write_span<bool>(".select_poly", bke::AttrDomain::Face);
      for (const int i : select_poly.span.index_range()) {
        select_poly.span[i] = (i % 3 == 0);
      }
      select_poly.finish();
    }
    call_vertex_color_dirt(C, DirtProps());
    dump_case(f, index++, "grid paintmask", ob);
  }

  /* --- Dos pasadas seguidas: la segunda parte de colores ya no uniformes, que es
   * donde se nota si la multiplicacion se hace en sRGB en vez de en lineal. --- */
  {
    purge(C);
    add_primitive(C, "mesh.primitive_monkey_add", [](PointerRNA *ptr) {
      RNA_float_set(ptr, "size", 2.0f);
    });
    Object *ob = CTX_data_active_object(C);
    call_vertex_color_dirt(C, DirtProps());
    DirtProps props;
    props.blur_iterations = 2;
    props.dirt_only = true;
    call_vertex_color_dirt(C, props);
    dump_case(f, index++, "monkey twice", ob);
  }

  fclose(f);
  fprintf(stderr, "fl-selftest-mesh-ops: volcado en '%s' (%d casos)\n", filepath, index);
  return true;
}

/** \} */

/* -------------------------------------------------------------------------- */
/** \name Comparacion contra la linea base
 * \{ */

bool check(bContext *C, const char *baseline_path)
{
  char actual_path[FILE_MAX];
  BLI_path_join(actual_path,
                sizeof(actual_path),
                BKE_tempdir_session(),
                "fl-selftest-mesh-ops-actual.txt");

  if (!dump(C, actual_path)) {
    return false;
  }
  return flipendo::selftest::compare_to_baseline(
      "fl-check-mesh-ops", actual_path, baseline_path);
}

/** \} */

}  // namespace flipendo::mesh_ops_selftest
