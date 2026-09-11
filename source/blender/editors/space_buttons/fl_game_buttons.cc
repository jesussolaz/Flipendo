/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spbuttons
 *
 * Los paneles de juego del editor de Propiedades, en C++ nativo.
 *
 * Sustituye a `scripts/startup/bl_ui/properties_game.py` (899 lineas): 14 paneles
 * repartidos por cuatro pestanas — Juego, Fisica, Escena y Objeto — y dos menus.
 * Mismos `idname`, mismas etiquetas, mismo orden, mismos operadores y las mismas
 * propiedades RNA: para el usuario no cambia nada.
 *
 * Es dominio propio de Flipendo, no herencia de Blender: `ob.game`,
 * `scene.game_settings`, los proxies de Python y los niveles de detalle son del
 * motor.
 *
 * Se verifica con el volcador de interfaz — `--fl-dump-ui` y `--fl-dump-ui-layout`,
 * ver `politicas/UI-A-CPP.md` —: el Python se retira solo cuando los bloques de
 * estos 16 tipos salen identicos a la linea base congelada con el Python vivo.
 *
 * Doctrina: politicas/LENGUAJE-CPP.md.
 */

#include <cstring>
#include <string>

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"

#include "BLI_string.h"
#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "FL_ui_registry.hh"

#include "buttons_intern.hh"

/* -------------------------------------------------------------------- */
/** \name Ayudas comunes
 * \{ */

/**
 * El `COMPAT_ENGINES` del Python: los paneles de juego solo salen con los motores
 * de Blender, no con uno externo. Los tres nombres son los de la tupla del script y
 * se miran donde los miraba el, en `scene.render.engine`.
 */
static bool engine_compatible(const bContext *C)
{
  const Scene *scene = CTX_data_scene(C);
  if (scene == nullptr) {
    return false;
  }
  const char *engine = scene->r.engine;
  return STREQ(engine, "BLENDER_RENDER") || STREQ(engine, "BLENDER_EEVEE_NEXT") ||
         STREQ(engine, "BLENDER_WORKBENCH");
}

/** `OBJECT_PT_activity_culling` no acepta 'BLENDER_RENDER'; el Python tampoco. */
static bool engine_compatible_sin_render(const bContext *C)
{
  const Scene *scene = CTX_data_scene(C);
  if (scene == nullptr) {
    return false;
  }
  const char *engine = scene->r.engine;
  return STREQ(engine, "BLENDER_EEVEE_NEXT") || STREQ(engine, "BLENDER_WORKBENCH");
}

static PointerRNA game_ptr(Object *ob)
{
  return RNA_pointer_create_discrete(&ob->id, &RNA_GameObjectSettings, ob);
}

static PointerRNA game_settings_ptr(Scene *scene)
{
  return RNA_pointer_create_discrete(&scene->id, &RNA_SceneGameData, &scene->gm);
}

/** El identificador de la enumeracion, para poder comparar como hacia el Python. */
static bool enum_es(PointerRNA *ptr, const char *propname, const char *identifier)
{
  const int value = RNA_enum_get(ptr, propname);
  PropertyRNA *prop = RNA_struct_find_property(ptr, propname);
  if (prop == nullptr) {
    return false;
  }
  const char *actual = nullptr;
  bool free = false;
  const EnumPropertyItem *items = nullptr;
  int items_num = 0;
  RNA_property_enum_items(nullptr, ptr, prop, &items, &items_num, &free);
  for (int i = 0; i < items_num; i++) {
    if (items[i].value == value) {
      actual = items[i].identifier;
      break;
    }
  }
  const bool result = (actual != nullptr) && STREQ(actual, identifier);
  if (free) {
    MEM_freeN(items);
  }
  return result;
}

static bool enum_en(PointerRNA *ptr,
                    const char *propname,
                    const char *const *identifiers,
                    const int count)
{
  for (int i = 0; i < count; i++) {
    if (enum_es(ptr, propname, identifiers[i])) {
      return true;
    }
  }
  return false;
}

/**
 * `split_pascal_case()` del Python, que es
 * `re.sub(r"((?<=[a-z])[A-Z]|(?<!\A)[A-Z](?=[a-z]))", r" \1", text)`: mete un espacio
 * antes de cada mayuscula que va detras de una minuscula, o que no abre la cadena y
 * va delante de una minuscula.
 */
static std::string split_pascal_case(const char *text)
{
  std::string out;
  const int len = (text != nullptr) ? int(strlen(text)) : 0;
  for (int i = 0; i < len; i++) {
    const char c = text[i];
    if (c >= 'A' && c <= 'Z' && i != 0) {
      const char prev = text[i - 1];
      const bool prev_minuscula = (prev >= 'a' && prev <= 'z');
      const bool sig_minuscula = (i + 1 < len) && (text[i + 1] >= 'a' && text[i + 1] <= 'z');
      if (prev_minuscula || sig_minuscula) {
        out += ' ';
      }
    }
    out += c;
  }
  return out;
}

/** El nombre de un elemento RNA, como cadena propia. */
static std::string nombre_de(PointerRNA *ptr)
{
  char *name = RNA_string_get_alloc(ptr, "name", nullptr, 0, nullptr);
  std::string out = (name != nullptr) ? name : "";
  if (name != nullptr) {
    MEM_freeN(name);
  }
  return out;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pestana de Juego: objeto, componentes y propiedades
 * \{ */

static bool game_object_poll(const bContext *C, PanelType * /*pt*/)
{
  return CTX_data_active_object(C) != nullptr;
}

static void game_object_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return;
  }
  PointerRNA game = game_ptr(ob);

  uiLayout *row = &layout->row(false);

  PointerRNA custom = RNA_pointer_get(&game, "custom_object");
  const bool hay_custom = (custom.data != nullptr) && (RNA_string_length(&custom, "name") > 0);

  if (hay_custom) {
    uiLayout *box = &layout->box();
    row = &box->row(false);

    row->prop(&custom, "show_expanded", UI_ITEM_R_NO_BG, "", ICON_NONE);
    row->label(split_pascal_case(nombre_de(&custom).c_str()), ICON_NONE);

    row->op("LOGIC_OT_custom_object_reload", "", ICON_RECOVER_LAST);
    row->op("LOGIC_OT_custom_object_remove", "", ICON_X);

    if (RNA_boolean_get(&custom, "show_expanded") &&
        RNA_collection_length(&custom, "properties") > 0)
    {
      uiLayout *caja = &box->box();
      RNA_BEGIN (&custom, prop_ptr, "properties") {
        uiLayout *prop_row = &caja->row(false);
        prop_row->label(split_pascal_case(nombre_de(&prop_ptr).c_str()), ICON_NONE);
        uiLayout *col = &prop_row->column(false);
        col->prop(&prop_ptr, "value", UI_ITEM_NONE, "", ICON_NONE);
      }
      RNA_END;
    }
  }
  else {
    row->op("LOGIC_OT_custom_object_register", IFACE_("Select"), ICON_PLUS);
    row->op("LOGIC_OT_custom_object_create", IFACE_("Create"), ICON_PLUS);
  }
}

static void game_components_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return;
  }
  PointerRNA game = game_ptr(ob);

  uiLayout *row = &layout->row(false);
  row->op("LOGIC_OT_python_component_register", IFACE_("Add"), ICON_PLUS);
  row->op("LOGIC_OT_python_component_create", IFACE_("Create"), ICON_PLUS);

  int i = 0;
  RNA_BEGIN (&game, componente, "components") {
    uiLayout *box = &layout->box();
    row = &box->row(false);

    row->prop(&componente, "show_expanded", UI_ITEM_R_NO_BG, "", ICON_NONE);
    row->label(split_pascal_case(nombre_de(&componente).c_str()), ICON_NONE);
    uiLayoutSetContextPointer(row, "component", &componente);
    row->menu("GAME_MT_component_context_menu", "", ICON_DOWNARROW_HLT);

    PointerRNA op_ptr = row->op("LOGIC_OT_python_component_remove", "", ICON_X);
    if (op_ptr.data) {
      RNA_int_set(&op_ptr, "index", i);
    }

    if (RNA_boolean_get(&componente, "show_expanded") &&
        RNA_collection_length(&componente, "properties") > 0)
    {
      uiLayout *caja = &box->box();
      RNA_BEGIN (&componente, prop_ptr, "properties") {
        uiLayout *prop_row = &caja->row(false);
        prop_row->label(split_pascal_case(nombre_de(&prop_ptr).c_str()), ICON_NONE);
        uiLayout *col = &prop_row->column(false);
        col->prop(&prop_ptr, "value", UI_ITEM_NONE, "", ICON_NONE);
      }
      RNA_END;
    }
    i++;
  }
  RNA_END;
}

static bool game_component_menu_poll(const bContext *C, MenuType * /*mt*/)
{
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return false;
  }
  PointerRNA game = game_ptr(ob);
  return RNA_collection_length(&game, "components") > 0;
}

static void game_component_menu_draw(const bContext *C, Menu *menu)
{
  uiLayout *layout = menu->layout;
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return;
  }

  /* El Python busca el indice por nombre — con su propio FIXME al lado — usando el
   * puntero de contexto `component` que puso la fila. Aqui igual, para no cambiar el
   * comportamiento observable mientras el nombre siga siendo la clave. */
  PointerRNA componente = CTX_data_pointer_get(C, "component");
  int index = 0;
  if (componente.data != nullptr) {
    const std::string buscado = nombre_de(&componente);
    PointerRNA game = game_ptr(ob);
    int i = 0;
    RNA_BEGIN (&game, item, "components") {
      if (nombre_de(&item) == buscado) {
        index = i;
        break;
      }
      i++;
    }
    RNA_END;
  }

  PointerRNA op_ptr = layout->op(
      "LOGIC_OT_python_component_reload", std::nullopt, ICON_RECOVER_LAST);
  if (op_ptr.data) {
    RNA_int_set(&op_ptr, "index", index);
  }

  layout->separator();

  op_ptr = layout->op("LOGIC_OT_python_component_move_up", std::nullopt, ICON_TRIA_UP);
  if (op_ptr.data) {
    RNA_int_set(&op_ptr, "index", index);
  }
  op_ptr = layout->op("LOGIC_OT_python_component_move_down", std::nullopt, ICON_TRIA_DOWN);
  if (op_ptr.data) {
    RNA_int_set(&op_ptr, "index", index);
  }
}

static void game_properties_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return;
  }
  PointerRNA game = game_ptr(ob);
  const bool is_font = (ob->type == OB_FONT);

  /* En un objeto de texto la propiedad "Text" es la que el motor usa como contenido:
   * se muestra aparte y no se puede renombrar. */
  int prop_index = -1;
  if (is_font) {
    int i = 0;
    RNA_BEGIN (&game, prop_ptr, "properties") {
      if (nombre_de(&prop_ptr) == "Text") {
        prop_index = i;
        break;
      }
      i++;
    }
    RNA_END;

    if (prop_index != -1) {
      PointerRNA op_ptr = layout->op(
          "OBJECT_OT_game_property_remove", IFACE_("Remove Text Game Property"), ICON_X);
      if (op_ptr.data) {
        RNA_int_set(&op_ptr, "index", prop_index);
      }

      int i2 = 0;
      RNA_BEGIN (&game, prop_ptr, "properties") {
        if (i2 == prop_index) {
          uiLayout *row = &layout->row(false);
          uiLayout *sub = &row->row(false);
          uiLayoutSetEnabled(sub, false);
          sub->prop(&prop_ptr, "name", UI_ITEM_NONE, "", ICON_NONE);
          row->prop(&prop_ptr, "type", UI_ITEM_NONE, "", ICON_NONE);
          /* El cuerpo del texto no se muestra: puede ser enorme y releerlo por frame
           * de interfaz es caro. Mismo motivo que el comentario del Python. */
          row->label(IFACE_("See Text Object"), ICON_NONE);
          break;
        }
        i2++;
      }
      RNA_END;
    }
    else {
      PointerRNA op_ptr = layout->op(
          "OBJECT_OT_game_property_new", IFACE_("Add Text Game Property"), ICON_PLUS);
      if (op_ptr.data) {
        RNA_string_set(&op_ptr, "name", "Text");
        RNA_enum_set_identifier(nullptr, &op_ptr, "type", "STRING");
      }
    }
  }

  PointerRNA op_ptr = layout->op(
      "OBJECT_OT_game_property_new", IFACE_("Add Game Property"), ICON_PLUS);
  if (op_ptr.data) {
    RNA_string_set(&op_ptr, "name", "");
  }

  int i = 0;
  RNA_BEGIN (&game, prop_ptr, "properties") {
    if (is_font && i == prop_index) {
      i++;
      continue;
    }

    uiLayout *box = &layout->box();
    uiLayout *row = &box->row(false);
    row->prop(&prop_ptr, "name", UI_ITEM_NONE, "", ICON_NONE);
    row->prop(&prop_ptr, "type", UI_ITEM_NONE, "", ICON_NONE);
    row->prop(&prop_ptr, "value", UI_ITEM_NONE, "", ICON_NONE);
    row->prop(&prop_ptr, "show_debug", UI_ITEM_R_TOGGLE, "", ICON_INFO);

    uiLayout *sub = &row->row(true);
    PointerRNA mover = sub->op("OBJECT_OT_game_property_move", "", ICON_TRIA_UP);
    if (mover.data) {
      RNA_int_set(&mover, "index", i);
      RNA_enum_set_identifier(nullptr, &mover, "direction", "UP");
    }
    mover = sub->op("OBJECT_OT_game_property_move", "", ICON_TRIA_DOWN);
    if (mover.data) {
      RNA_int_set(&mover, "index", i);
      RNA_enum_set_identifier(nullptr, &mover, "direction", "DOWN");
    }

    PointerRNA quitar = row->op("OBJECT_OT_game_property_remove",
                                "",
                                ICON_X,
                                WM_OP_INVOKE_REGION_WIN,
                                UI_ITEM_R_NO_BG);
    if (quitar.data) {
      RNA_int_set(&quitar, "index", i);
    }
    i++;
  }
  RNA_END;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pestana de Fisica
 * \{ */

static bool physics_game_poll(const bContext *C, PanelType * /*pt*/)
{
  return (CTX_data_active_object(C) != nullptr) && engine_compatible(C);
}

static void physics_dynamic_draw(uiLayout *layout, PointerRNA *game, PointerRNA *ob_ptr)
{
  uiLayout *split = &layout->split(0.0f, false);

  uiLayout *col = &split->column(false);
  col->prop(game, "use_actor", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(game, "use_ghost", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(ob_ptr, "hide_render", UI_ITEM_NONE, IFACE_("Invisible"), ICON_NONE);

  col = &split->column(false);
  col->prop(game, "use_physics_fh", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(game, "use_rotate_from_normal", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(game, "use_sleep", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  layout->separator();

  split = &layout->split(0.0f, false);

  col = &split->column(false);
  col->label(IFACE_("Attributes:"), ICON_NONE);
  col->prop(game, "mass", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(game, "radius", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(game, "form_factor", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);
  col->prop(game, "elasticity", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);

  col->label(IFACE_("Linear Velocity:"), ICON_NONE);
  uiLayout *sub = &col->column(true);
  sub->prop(game, "velocity_min", UI_ITEM_NONE, IFACE_("Minimum"), ICON_NONE);
  sub->prop(game, "velocity_max", UI_ITEM_NONE, IFACE_("Maximum"), ICON_NONE);

  col = &split->column(false);
  col->label(IFACE_("Friction:"), ICON_NONE);
  col->prop(game, "friction", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(game, "rolling_friction", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->separator();

  sub = &col->column(false);
  sub->prop(game, "use_anisotropic_friction", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  uiLayout *subsub = &sub->column(false);
  uiLayoutSetActive(subsub, RNA_boolean_get(game, "use_anisotropic_friction"));
  subsub->prop(game, "friction_coefficients", UI_ITEM_R_SLIDER, "", ICON_NONE);

  split = &layout->split(0.0f, false);
  col = &split->column(false);
  col->label(IFACE_("Angular velocity:"), ICON_NONE);
  sub = &col->column(true);
  sub->prop(game, "angular_velocity_min", UI_ITEM_NONE, IFACE_("Minimum"), ICON_NONE);
  sub->prop(game, "angular_velocity_max", UI_ITEM_NONE, IFACE_("Maximum"), ICON_NONE);

  col = &split->column(false);
  col->label(IFACE_("Damping:"), ICON_NONE);
  sub = &col->column(true);
  sub->prop(game, "damping", UI_ITEM_R_SLIDER, IFACE_("Translation"), ICON_NONE);
  sub->prop(game, "rotation_damping", UI_ITEM_R_SLIDER, IFACE_("Rotation"), ICON_NONE);

  layout->separator();
  split = &layout->split(0.0f, false);

  col = &split->column(false);
  col->prop(game, "use_ccd_rigid_body", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  sub = &col->column(false);
  uiLayoutSetActive(sub, RNA_boolean_get(game, "use_ccd_rigid_body"));
  sub->prop(game, "ccd_motion_threshold", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  sub->prop(game, "ccd_swept_sphere_radius", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  layout->separator();
  col = &layout->column(false);

  col->label(IFACE_("Lock Translation:"), ICON_NONE);
  uiLayout *row = &col->row(false);
  row->prop(game, "lock_location_x", UI_ITEM_NONE, IFACE_("X"), ICON_NONE);
  row->prop(game, "lock_location_y", UI_ITEM_NONE, IFACE_("Y"), ICON_NONE);
  row->prop(game, "lock_location_z", UI_ITEM_NONE, IFACE_("Z"), ICON_NONE);
}

static void physics_soft_body_draw(uiLayout *layout, PointerRNA *game, PointerRNA *ob_ptr)
{
  PointerRNA soft = RNA_pointer_get(game, "soft_body");

  uiLayout *col = &layout->column(false);
  col->prop(game, "use_actor", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  /* `use_ghost` no lo soporta Bullet para cuerpos blandos; en el Python tambien
   * esta comentado. */
  col->prop(ob_ptr, "hide_render", UI_ITEM_NONE, IFACE_("Invisible"), ICON_NONE);

  layout->separator();

  uiLayout *split = &layout->split(0.0f, false);

  col = &split->column(false);
  col->label(IFACE_("General Attributes:"), ICON_NONE);
  col->prop(game, "mass", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(&soft, "linear_stiffness", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);
  col->prop(&soft, "dynamic_friction", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);
  col->prop(&soft, "kdp", UI_ITEM_R_SLIDER, IFACE_("Damping"), ICON_NONE);
  col->prop(&soft, "collision_margin", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);
  col->prop(&soft, "kvcf", UI_ITEM_R_SLIDER, IFACE_("Velocity Correction"), ICON_NONE);
  col->prop(&soft, "use_bending_constraints", UI_ITEM_NONE, IFACE_("Bending Constraints"), ICON_NONE);

  uiLayout *sub = &col->column(false);
  uiLayoutSetActive(sub, RNA_boolean_get(&soft, "use_bending_constraints"));
  sub->prop(&soft, "bending_distance", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  col->prop(&soft, "use_shape_match", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  sub = &col->column(false);
  uiLayoutSetActive(sub, RNA_boolean_get(&soft, "use_shape_match"));
  sub->prop(&soft, "shape_threshold", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);

  col->label(IFACE_("Solver Iterations:"), ICON_NONE);
  col->prop(&soft, "position_solver_iterations", UI_ITEM_NONE, IFACE_("Position Solver"), ICON_NONE);
  col->prop(&soft, "velocity_solver_iterations", UI_ITEM_NONE, IFACE_("Velocity Solver"), ICON_NONE);
  col->prop(&soft, "cluster_solver_iterations", UI_ITEM_NONE, IFACE_("Cluster Solver"), ICON_NONE);
  col->prop(&soft, "drift_solver_iterations", UI_ITEM_NONE, IFACE_("Drift Solver"), ICON_NONE);

  col = &split->column(false);
  col->label(IFACE_("Hardness:"), ICON_NONE);
  col->prop(&soft, "kchr", UI_ITEM_R_SLIDER, IFACE_("Rigid Contacts"), ICON_NONE);
  col->prop(&soft, "kkhr", UI_ITEM_R_SLIDER, IFACE_("Kinetic Contacts"), ICON_NONE);
  col->prop(&soft, "kshr", UI_ITEM_R_SLIDER, IFACE_("Soft Contacts"), ICON_NONE);
  col->prop(&soft, "kahr", UI_ITEM_R_SLIDER, IFACE_("Anchors"), ICON_NONE);

  col->label(IFACE_("Cluster Collision:"), ICON_NONE);
  col->prop(&soft, "use_cluster_rigid_to_softbody", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(&soft, "use_cluster_soft_to_softbody", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  sub = &col->column(false);
  uiLayoutSetActive(sub,
                    RNA_boolean_get(&soft, "use_cluster_rigid_to_softbody") ||
                        RNA_boolean_get(&soft, "use_cluster_soft_to_softbody"));
  sub->prop(&soft, "cluster_iterations", UI_ITEM_NONE, IFACE_("Iterations"), ICON_NONE);
  sub->prop(&soft, "ksrhr_cl", UI_ITEM_R_SLIDER, IFACE_("Rigid Hardness"), ICON_NONE);
  sub->prop(&soft, "kskhr_cl", UI_ITEM_R_SLIDER, IFACE_("Kinetic Hardness"), ICON_NONE);
  sub->prop(&soft, "ksshr_cl", UI_ITEM_R_SLIDER, IFACE_("Soft Hardness"), ICON_NONE);
  sub->prop(&soft, "ksr_split_cl", UI_ITEM_R_SLIDER, IFACE_("Rigid Impulse Split"), ICON_NONE);
  sub->prop(&soft, "ksk_split_cl", UI_ITEM_R_SLIDER, IFACE_("Kinetic Impulse Split"), ICON_NONE);
  sub->prop(&soft, "kss_split_cl", UI_ITEM_R_SLIDER, IFACE_("Soft Impulse Split"), ICON_NONE);

  split = &layout->split(0.0f, false);

  col = &split->column(false);
  col->label(IFACE_("Volume:"), ICON_NONE);
  col->prop(&soft, "kpr", UI_ITEM_NONE, IFACE_("Pressure Coefficient"), ICON_NONE);
  col->prop(&soft, "kvc", UI_ITEM_NONE, IFACE_("Volume Conservation"), ICON_NONE);

  col = &split->column(false);
  col->label(IFACE_("Aerodynamics:"), ICON_NONE);
  col->prop(&soft, "kdg", UI_ITEM_NONE, IFACE_("Drag Coefficient"), ICON_NONE);
  col->prop(&soft, "klf", UI_ITEM_NONE, IFACE_("Lift Coefficient"), ICON_NONE);
}

static void physics_game_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return;
  }
  PointerRNA game = game_ptr(ob);
  PointerRNA ob_ptr = RNA_id_pointer_create(&ob->id);

  layout->prop(&game, "physics_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  layout->separator();

  if (enum_es(&game, "physics_type", "CHARACTER")) {
    layout->prop(&game, "use_actor", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    layout->prop(&ob_ptr, "hide_render", UI_ITEM_NONE, IFACE_("Invisible"), ICON_NONE);

    layout->separator();

    uiLayout *split = &layout->split(0.0f, false);

    uiLayout *col = &split->column(false);
    col->prop(&game, "step_height", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);
    col->prop(&game, "fall_speed", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(&game, "max_slope", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col = &split->column(false);
    col->prop(&game, "jump_speed", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(&game, "jump_max", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  else {
    static const char *dinamicos[] = {"DYNAMIC", "RIGID_BODY"};
    if (enum_en(&game, "physics_type", dinamicos, ARRAY_SIZE(dinamicos))) {
      physics_dynamic_draw(layout, &game, &ob_ptr);
    }
  }

  if (enum_es(&game, "physics_type", "RIGID_BODY")) {
    uiLayout *col = &layout->column(false);
    col->label(IFACE_("Lock Rotation:"), ICON_NONE);
    uiLayout *row = &col->row(false);
    row->prop(&game, "lock_rotation_x", UI_ITEM_NONE, IFACE_("X"), ICON_NONE);
    row->prop(&game, "lock_rotation_y", UI_ITEM_NONE, IFACE_("Y"), ICON_NONE);
    row->prop(&game, "lock_rotation_z", UI_ITEM_NONE, IFACE_("Z"), ICON_NONE);
  }
  else if (enum_es(&game, "physics_type", "SOFT_BODY")) {
    physics_soft_body_draw(layout, &game, &ob_ptr);
  }
  else if (enum_es(&game, "physics_type", "STATIC")) {
    uiLayout *col = &layout->column(false);
    col->prop(&game, "use_actor", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(&game, "use_ghost", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(&ob_ptr, "hide_render", UI_ITEM_NONE, IFACE_("Invisible"), ICON_NONE);

    layout->separator();

    uiLayout *split = &layout->split(0.0f, false);

    col = &split->column(false);
    col->label(IFACE_("Attributes:"), ICON_NONE);
    col->prop(&game, "radius", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(&game, "elasticity", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);
    col->label(IFACE_("Friction:"), ICON_NONE);
    col->prop(&game, "friction", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    col->prop(&game, "rolling_friction", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    col = &split->column(false);
    uiLayout *sub = &col->column(false);
    sub->prop(&game, "use_anisotropic_friction", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    uiLayout *subsub = &sub->column(false);
    uiLayoutSetActive(subsub, RNA_boolean_get(&game, "use_anisotropic_friction"));
    subsub->prop(&game, "friction_coefficients", UI_ITEM_R_SLIDER, "", ICON_NONE);
  }
  else if (enum_es(&game, "physics_type", "SENSOR")) {
    uiLayout *col = &layout->column(false);
    col->prop(&game, "use_actor", UI_ITEM_NONE, IFACE_("Detect Actors"), ICON_NONE);
    col->prop(&ob_ptr, "hide_render", UI_ITEM_NONE, IFACE_("Invisible"), ICON_NONE);
  }
  else {
    static const char *invisibles[] = {"INVISIBLE", "NO_COLLISION", "OCCLUDER"};
    if (enum_en(&game, "physics_type", invisibles, ARRAY_SIZE(invisibles))) {
      layout->prop(&ob_ptr, "hide_render", UI_ITEM_NONE, IFACE_("Invisible"), ICON_NONE);
    }
    else if (enum_es(&game, "physics_type", "NAVMESH")) {
      layout->op("MESH_OT_navmesh_face_copy", std::nullopt, ICON_NONE);
      layout->op("MESH_OT_navmesh_face_add", std::nullopt, ICON_NONE);

      layout->separator();

      layout->op("MESH_OT_navmesh_reset", std::nullopt, ICON_NONE);
      layout->op("MESH_OT_navmesh_clear", std::nullopt, ICON_NONE);
    }
  }

  static const char *con_campo[] = {"STATIC", "DYNAMIC", "RIGID_BODY"};
  if (enum_en(&game, "physics_type", con_campo, ARRAY_SIZE(con_campo))) {
    uiLayout *row = &layout->row(false);
    row->label(IFACE_("Force Field:"), ICON_NONE);

    row = &layout->row(false);
    row->prop(&game, "fh_force", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    row->prop(&game, "fh_damping", UI_ITEM_R_SLIDER, std::nullopt, ICON_NONE);

    row = &layout->row(false);
    row->prop(&game, "fh_distance", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    row->prop(&game, "use_fh_normal", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
}

static bool collision_bounds_poll(const bContext *C, PanelType * /*pt*/)
{
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr || !engine_compatible(C)) {
    return false;
  }
  PointerRNA game = game_ptr(ob);
  static const char *tipos[] = {
      "SENSOR", "STATIC", "DYNAMIC", "RIGID_BODY", "CHARACTER", "SOFT_BODY"};
  return enum_en(&game, "physics_type", tipos, ARRAY_SIZE(tipos));
}

static void collision_bounds_draw_header(const bContext *C, Panel *panel)
{
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return;
  }
  PointerRNA game = game_ptr(ob);
  panel->layout->prop(&game, "use_collision_bounds", UI_ITEM_NONE, "", ICON_NONE);
}

static void collision_bounds_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return;
  }
  PointerRNA game = game_ptr(ob);

  uiLayout *split = &layout->split(0.0f, false);
  uiLayoutSetActive(split, RNA_boolean_get(&game, "use_collision_bounds"));

  uiLayout *col = &split->column(false);
  col->prop(&game, "collision_bounds_type", UI_ITEM_NONE, IFACE_("Bounds"), ICON_NONE);

  uiLayout *row = &col->row(false);
  row->prop(&game, "collision_margin", UI_ITEM_R_SLIDER, IFACE_("Margin"), ICON_NONE);

  uiLayout *sub = &row->row(false);
  static const char *sin_compound[] = {"SOFT_BODY", "CHARACTER"};
  uiLayoutSetActive(sub, !enum_en(&game, "physics_type", sin_compound, ARRAY_SIZE(sin_compound)));
  sub->prop(&game, "use_collision_compound", UI_ITEM_NONE, IFACE_("Compound"), ICON_NONE);

  layout->separator();
  split = &layout->split(0.0f, false);
  col = &split->column(false);
  col->prop(&game, "collision_group", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col = &split->column(false);
  col->prop(&game, "collision_mask", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static bool obstacles_poll(const bContext *C, PanelType * /*pt*/)
{
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr || !engine_compatible(C)) {
    return false;
  }
  PointerRNA game = game_ptr(ob);
  static const char *tipos[] = {
      "SENSOR", "STATIC", "DYNAMIC", "RIGID_BODY", "SOFT_BODY", "CHARACTER", "NO_COLLISION"};
  return enum_en(&game, "physics_type", tipos, ARRAY_SIZE(tipos));
}

static void obstacles_draw_header(const bContext *C, Panel *panel)
{
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return;
  }
  PointerRNA game = game_ptr(ob);
  panel->layout->prop(&game, "use_obstacle_create", UI_ITEM_NONE, "", ICON_NONE);
}

static void obstacles_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return;
  }
  PointerRNA game = game_ptr(ob);

  uiLayoutSetActive(layout, RNA_boolean_get(&game, "use_obstacle_create"));

  uiLayout *row = &layout->row(false);
  row->prop(&game, "obstacle_radius", UI_ITEM_NONE, IFACE_("Radius"), ICON_NONE);
  row->label("", ICON_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pestana de Escena
 * \{ */

static bool scene_game_poll(const bContext *C, PanelType * /*pt*/)
{
  return engine_compatible(C);
}

static void scene_physics_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  Scene *scene = CTX_data_scene(C);
  if (scene == nullptr) {
    return;
  }
  PointerRNA gs = game_settings_ptr(scene);

  layout->prop(&gs, "physics_engine", UI_ITEM_NONE, IFACE_("Engine"), ICON_NONE);
  if (!enum_es(&gs, "physics_engine", "NONE")) {
    layout->prop(&gs, "physics_solver", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    layout->prop(&gs, "physics_gravity", UI_ITEM_NONE, IFACE_("Gravity"), ICON_NONE);

    uiLayout *split = &layout->split(0.0f, false);

    uiLayout *col = &split->column(false);
    col->label(IFACE_("Physics Steps:"), ICON_NONE);
    uiLayout *sub = &col->column(true);
    sub->prop(&gs, "physics_step_max", UI_ITEM_NONE, IFACE_("Max"), ICON_NONE);
    sub->prop(&gs, "physics_step_sub", UI_ITEM_NONE, IFACE_("Substeps"), ICON_NONE);

    col = &split->column(false);
    col->label(IFACE_("Logic Steps:"), ICON_NONE);
    col->prop(&gs, "logic_step_max", UI_ITEM_NONE, IFACE_("Max"), ICON_NONE);

    uiLayout *row = &layout->row(false);
    row->prop(&gs, "fps", UI_ITEM_NONE, IFACE_("FPS"), ICON_NONE);
    row->prop(&gs, "time_scale", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    col = &layout->column(false);
    col->label(IFACE_("Physics Deactivation:"), ICON_NONE);
    sub = &col->row(true);
    sub->prop(&gs, "deactivation_linear_threshold", UI_ITEM_NONE, IFACE_("Linear Threshold"), ICON_NONE);
    sub->prop(&gs, "deactivation_angular_threshold", UI_ITEM_NONE, IFACE_("Angular Threshold"), ICON_NONE);
    sub = &col->row(false);
    sub->prop(&gs, "deactivation_time", UI_ITEM_NONE, IFACE_("Time"), ICON_NONE);

    col = &layout->column(false);
    col->label(IFACE_("Physics Joint Error Reduction:"), ICON_NONE);
    sub = &col->column(true);
    sub->prop(&gs, "erp_parameter", UI_ITEM_NONE, IFACE_("ERP for Non Contact Constraints"), ICON_NONE);
    sub->prop(&gs, "erp2_parameter", UI_ITEM_NONE, IFACE_("ERP for Contact Constraints"), ICON_NONE);
    sub->prop(&gs, "cfm_parameter", UI_ITEM_NONE, IFACE_("CFM for Soft Constraints"), ICON_NONE);

    row = &layout->row(false);
    row->label(IFACE_("Object Activity:"), ICON_NONE);
    row->prop(&gs, "use_activity_culling", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
  else {
    uiLayout *split = &layout->split(0.0f, false);

    uiLayout *col = &split->column(false);
    col->label(IFACE_("Physics Steps:"), ICON_NONE);
    col->prop(&gs, "fps", UI_ITEM_NONE, IFACE_("FPS"), ICON_NONE);

    col = &split->column(false);
    col->label(IFACE_("Logic Steps:"), ICON_NONE);
    col->prop(&gs, "logic_step_max", UI_ITEM_NONE, IFACE_("Max"), ICON_NONE);
  }
}

static void scene_blender_physics_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  Scene *scene = CTX_data_scene(C);
  if (scene == nullptr) {
    return;
  }
  PointerRNA gs = game_settings_ptr(scene);

  layout->prop(&gs, "use_interactive_dynapaint", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  uiLayout *row = &layout->row(false);
  row->prop(&gs, "use_interactive_rigidbody", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void scene_obstacles_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  Scene *scene = CTX_data_scene(C);
  if (scene == nullptr) {
    return;
  }
  PointerRNA gs = game_settings_ptr(scene);

  layout->prop(&gs, "obstacle_simulation", UI_ITEM_NONE, IFACE_("Type"), ICON_NONE);
  if (!enum_es(&gs, "obstacle_simulation", "NONE")) {
    layout->prop(&gs, "level_height", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    layout->prop(&gs, "show_obstacle_simulation", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }
}

static void scene_navmesh_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  Scene *scene = CTX_data_scene(C);
  if (scene == nullptr) {
    return;
  }
  PointerRNA gs = game_settings_ptr(scene);
  PointerRNA rd = RNA_pointer_get(&gs, "recast_data");

  layout->op("MESH_OT_navmesh_make", IFACE_("Build Navigation Mesh"), ICON_NONE);

  uiLayout *col = &layout->column(false);
  col->label(IFACE_("Rasterization:"), ICON_NONE);
  uiLayout *row = &col->row(false);
  row->prop(&rd, "cell_size", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  row->prop(&rd, "cell_height", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  col = &layout->column(false);
  col->label(IFACE_("Agent:"), ICON_NONE);
  uiLayout *split = &col->split(0.0f, false);

  col = &split->column(false);
  col->prop(&rd, "agent_height", UI_ITEM_NONE, IFACE_("Height"), ICON_NONE);
  col->prop(&rd, "agent_radius", UI_ITEM_NONE, IFACE_("Radius"), ICON_NONE);

  col = &split->column(false);
  col->prop(&rd, "slope_max", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(&rd, "climb_max", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  col = &layout->column(false);
  col->label(IFACE_("Region:"), ICON_NONE);
  row = &col->row(false);
  row->prop(&rd, "region_min_size", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  if (!enum_es(&rd, "partitioning", "LAYERS")) {
    row->prop(&rd, "region_merge_size", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  }

  col = &layout->column(false);
  col->prop(&rd, "partitioning", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  col = &layout->column(false);
  col->label(IFACE_("Polygonization:"), ICON_NONE);
  split = &col->split(0.0f, false);

  col = &split->column(false);
  col->prop(&rd, "edge_max_len", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  col->prop(&rd, "edge_max_error", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  split->prop(&rd, "verts_per_poly", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  col = &layout->column(false);
  col->label(IFACE_("Detail Mesh:"), ICON_NONE);
  row = &col->row(false);
  row->prop(&rd, "sample_dist", UI_ITEM_NONE, std::nullopt, ICON_NONE);
  row->prop(&rd, "sample_max_error", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void scene_hysteresis_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  Scene *scene = CTX_data_scene(C);
  if (scene == nullptr) {
    return;
  }
  PointerRNA gs = game_settings_ptr(scene);

  uiLayout *row = &layout->row(false);
  row->prop(&gs, "use_scene_hysteresis", UI_ITEM_NONE, IFACE_("Hysteresis"), ICON_NONE);
  row = &layout->row(false);
  uiLayoutSetActive(row, RNA_boolean_get(&gs, "use_scene_hysteresis"));
  row->prop(&gs, "scene_hysteresis_percentage", UI_ITEM_NONE, "", ICON_NONE);
}

static void scene_console_draw_header(const bContext *C, Panel *panel)
{
  Scene *scene = CTX_data_scene(C);
  if (scene == nullptr) {
    return;
  }
  PointerRNA gs = game_settings_ptr(scene);
  panel->layout->prop(&gs, "use_python_console", UI_ITEM_NONE, "", ICON_NONE);
}

static void scene_console_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  Scene *scene = CTX_data_scene(C);
  if (scene == nullptr) {
    return;
  }
  PointerRNA gs = game_settings_ptr(scene);

  uiLayout *row = &layout->row(true);
  uiLayoutSetActive(row, RNA_boolean_get(&gs, "use_python_console"));
  row->label(IFACE_("Keys:"), ICON_NONE);
  row->prop(&gs, "python_console_key1", UI_ITEM_R_EVENT, "", ICON_NONE);
  row->prop(&gs, "python_console_key2", UI_ITEM_R_EVENT, "", ICON_NONE);
  row->prop(&gs, "python_console_key3", UI_ITEM_R_EVENT, "", ICON_NONE);
  row->prop(&gs, "python_console_key4", UI_ITEM_R_EVENT, "", ICON_NONE);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Pestana de Objeto
 * \{ */

static bool activity_culling_poll(const bContext *C, PanelType * /*pt*/)
{
  const Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return false;
  }
  return engine_compatible_sin_render(C) && ob->type != OB_CAMERA;
}

static void activity_culling_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return;
  }
  PointerRNA game = game_ptr(ob);
  PointerRNA activity = RNA_pointer_get(&game, "activity_culling");

  uiLayout *split = &layout->split(0.0f, false);

  uiLayout *col = &split->column(false);
  col->prop(&activity, "use_physics", UI_ITEM_NONE, IFACE_("Physics"), ICON_NONE);
  uiLayout *sub = &col->column(false);
  uiLayoutSetActive(sub, RNA_boolean_get(&activity, "use_physics"));
  sub->prop(&activity, "physics_radius", UI_ITEM_NONE, std::nullopt, ICON_NONE);

  col = &split->column(false);
  col->prop(&activity, "use_logic", UI_ITEM_NONE, IFACE_("Logic"), ICON_NONE);
  sub = &col->column(false);
  uiLayoutSetActive(sub, RNA_boolean_get(&activity, "use_logic"));
  sub->prop(&activity, "logic_radius", UI_ITEM_NONE, std::nullopt, ICON_NONE);
}

static void lod_tools_menu_draw(const bContext * /*C*/, Menu *menu)
{
  uiLayout *layout = menu->layout;
  layout->op("OBJECT_OT_lod_by_name", IFACE_("Set By Name"), ICON_NONE);
  layout->op("OBJECT_OT_lod_generate", IFACE_("Generate"), ICON_NONE);
  layout->op("OBJECT_OT_lod_clear_all", IFACE_("Clear All"), ICON_PANEL_CLOSE);
}

static bool levels_of_detail_poll(const bContext *C, PanelType * /*pt*/)
{
  const Object *ob = CTX_data_active_object(C);
  if (ob == nullptr) {
    return false;
  }
  return engine_compatible(C) && !ELEM(ob->type, OB_CAMERA, OB_EMPTY, OB_LAMP);
}

static void levels_of_detail_draw(const bContext *C, Panel *panel)
{
  uiLayout *layout = panel->layout;
  Object *ob = CTX_data_active_object(C);
  Scene *scene = CTX_data_scene(C);
  if (ob == nullptr || scene == nullptr) {
    return;
  }
  PointerRNA ob_ptr = RNA_id_pointer_create(&ob->id);
  PointerRNA gs = game_settings_ptr(scene);
  const bool hysteresis = RNA_boolean_get(&gs, "use_scene_hysteresis");

  uiLayout *col = &layout->column(false);
  col->prop(&ob_ptr, "lod_factor", UI_ITEM_NONE, IFACE_("Distance Factor"), ICON_NONE);

  col = &layout->column(false);
  col->prop(&ob_ptr, "use_lod_physics", UI_ITEM_NONE, IFACE_("Physics Update"), ICON_NONE);

  int i = 0;
  RNA_BEGIN (&ob_ptr, level, "lod_levels") {
    if (i == 0) {
      i++;
      continue;
    }
    uiLayout *box = &col->box();
    uiLayout *row = &box->row(false);
    row->prop(&level, "object", UI_ITEM_NONE, "", ICON_NONE);
    PointerRNA op_ptr = row->op("OBJECT_OT_lod_remove", "", ICON_PANEL_CLOSE);
    if (op_ptr.data) {
      RNA_int_set(&op_ptr, "index", i);
    }

    row = &box->row(false);
    row->prop(&level, "distance", UI_ITEM_NONE, std::nullopt, ICON_NONE);

    row = &box->row(false);
    uiLayoutSetActive(row, hysteresis);
    row->prop(&level, "use_object_hysteresis", UI_ITEM_NONE, IFACE_("Hysteresis Override"), ICON_NONE);
    row = &box->row(false);
    uiLayoutSetActive(row, hysteresis && RNA_boolean_get(&level, "use_object_hysteresis"));
    row->prop(&level, "object_hysteresis_percentage", UI_ITEM_NONE, "", ICON_NONE);
    i++;
  }
  RNA_END;

  uiLayout *row = &col->row(true);
  row->op("OBJECT_OT_lod_add", IFACE_("Add"), ICON_PLUS);
  row->menu("OBJECT_MT_lod_tools", "", ICON_TRIA_DOWN);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Registro
 *
 * El orden de la tabla es el orden de `classes` en `properties_game.py`, y el campo
 * `order` reproduce el `bl_order` de cada clase base: 1000 para las pestanas de
 * Juego y Fisica, 0 (por defecto) para las de Escena y Objeto.
 * \{ */

static const flipendo::PanelDecl game_panels[] = {
    {
        /*idname*/ "GAME_PT_game_object",
        /*label*/ N_("Game Object"),
        /*category*/ nullptr,
        /*context*/ "game",
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ game_object_draw,
        /*draw_header*/ nullptr,
        /*draw_header_preset*/ nullptr,
        /*poll*/ game_object_poll,
        /*flag*/ 0,
        /*order*/ 1000,
    },
    {
        /*idname*/ "GAME_PT_game_components",
        /*label*/ N_("Game Components"),
        /*category*/ nullptr,
        /*context*/ "game",
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ game_components_draw,
        /*draw_header*/ nullptr,
        /*draw_header_preset*/ nullptr,
        /*poll*/ game_object_poll,
        /*flag*/ 0,
        /*order*/ 1000,
    },
    {
        /*idname*/ "GAME_PT_game_properties",
        /*label*/ N_("Game Properties"),
        /*category*/ nullptr,
        /*context*/ "game",
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ game_properties_draw,
        /*draw_header*/ nullptr,
        /*draw_header_preset*/ nullptr,
        /*poll*/ game_object_poll,
        /*flag*/ 0,
        /*order*/ 1000,
    },
    {
        /*idname*/ "PHYSICS_PT_game_physics",
        /*label*/ N_("Game Physics"),
        /*category*/ nullptr,
        /*context*/ "physics",
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ physics_game_draw,
        /*draw_header*/ nullptr,
        /*draw_header_preset*/ nullptr,
        /*poll*/ physics_game_poll,
        /*flag*/ 0,
        /*order*/ 1000,
    },
    {
        /*idname*/ "PHYSICS_PT_game_collision_bounds",
        /*label*/ N_("Collision Bounds"),
        /*category*/ nullptr,
        /*context*/ "physics",
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ collision_bounds_draw,
        /*draw_header*/ collision_bounds_draw_header,
        /*draw_header_preset*/ nullptr,
        /*poll*/ collision_bounds_poll,
        /*flag*/ 0,
        /*order*/ 1000,
    },
    {
        /*idname*/ "PHYSICS_PT_game_obstacles",
        /*label*/ N_("Create Obstacle"),
        /*category*/ nullptr,
        /*context*/ "physics",
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ obstacles_draw,
        /*draw_header*/ obstacles_draw_header,
        /*draw_header_preset*/ nullptr,
        /*poll*/ obstacles_poll,
        /*flag*/ 0,
        /*order*/ 1000,
    },
    {
        /*idname*/ "SCENE_PT_game_physics",
        /*label*/ N_("Game Physics"),
        /*category*/ nullptr,
        /*context*/ "scene",
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ scene_physics_draw,
        /*draw_header*/ nullptr,
        /*draw_header_preset*/ nullptr,
        /*poll*/ scene_game_poll,
    },
    {
        /*idname*/ "SCENE_PT_game_blender_physics",
        /*label*/ N_("Game Blender Physics"),
        /*category*/ nullptr,
        /*context*/ "scene",
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ scene_blender_physics_draw,
        /*draw_header*/ nullptr,
        /*draw_header_preset*/ nullptr,
        /*poll*/ scene_game_poll,
    },
    {
        /*idname*/ "SCENE_PT_game_physics_obstacles",
        /*label*/ N_("Obstacle Simulation"),
        /*category*/ nullptr,
        /*context*/ "scene",
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ scene_obstacles_draw,
        /*draw_header*/ nullptr,
        /*draw_header_preset*/ nullptr,
        /*poll*/ scene_game_poll,
        /*flag*/ PANEL_TYPE_DEFAULT_CLOSED,
    },
    {
        /*idname*/ "SCENE_PT_game_navmesh",
        /*label*/ N_("Navigation Mesh"),
        /*category*/ nullptr,
        /*context*/ "scene",
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ scene_navmesh_draw,
        /*draw_header*/ nullptr,
        /*draw_header_preset*/ nullptr,
        /*poll*/ scene_game_poll,
        /*flag*/ PANEL_TYPE_DEFAULT_CLOSED,
    },
    {
        /*idname*/ "SCENE_PT_game_hysteresis",
        /*label*/ N_("Level of Detail"),
        /*category*/ nullptr,
        /*context*/ "scene",
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ scene_hysteresis_draw,
        /*draw_header*/ nullptr,
        /*draw_header_preset*/ nullptr,
        /*poll*/ scene_game_poll,
    },
    {
        /*idname*/ "SCENE_PT_game_console",
        /*label*/ N_("Game Python Console"),
        /*category*/ nullptr,
        /*context*/ "scene",
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ scene_console_draw,
        /*draw_header*/ scene_console_draw_header,
        /*draw_header_preset*/ nullptr,
        /*poll*/ scene_game_poll,
        /*flag*/ PANEL_TYPE_DEFAULT_CLOSED,
    },
    {
        /*idname*/ "OBJECT_PT_activity_culling",
        /*label*/ N_("Activity Culling"),
        /*category*/ nullptr,
        /*context*/ "object",
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ activity_culling_draw,
        /*draw_header*/ nullptr,
        /*draw_header_preset*/ nullptr,
        /*poll*/ activity_culling_poll,
    },
    {
        /*idname*/ "OBJECT_PT_levels_of_detail",
        /*label*/ N_("Levels of Detail"),
        /*category*/ nullptr,
        /*context*/ "object",
        /*parent_id*/ nullptr,
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ levels_of_detail_draw,
        /*draw_header*/ nullptr,
        /*draw_header_preset*/ nullptr,
        /*poll*/ levels_of_detail_poll,
    },
};

static const flipendo::MenuDecl game_menus[] = {
    {
        /*idname*/ "GAME_MT_component_context_menu",
        /*label*/ N_("Game Component"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ game_component_menu_draw,
        /*poll*/ game_component_menu_poll,
    },
    {
        /*idname*/ "OBJECT_MT_lod_tools",
        /*label*/ N_("Level Of Detail Tools"),
        /*description*/ nullptr,
        /*translation_context*/ nullptr,
        /*draw*/ lod_tools_menu_draw,
    },
};

void fl_game_buttons_register(ARegionType *art)
{
  flipendo::panels_register(art, SPACE_PROPERTIES, {game_panels, ARRAY_SIZE(game_panels)});
}

void fl_game_menus_register()
{
  flipendo::menus_register({game_menus, ARRAY_SIZE(game_menus)});
}

/** \} */
