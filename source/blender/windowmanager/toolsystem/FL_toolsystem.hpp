/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Contrato del sistema de herramientas nativo.
 *
 * Sustituye a `scripts/startup/bl_ui/space_toolsystem_common.py` (el armazon) y a
 * `space_toolsystem_toolbar.py` (el catalogo de 160 herramientas). Ver el plan y las
 * trampas verificadas en politicas/TOOLSYSTEM-A-CPP.md.
 *
 * El ESTADO de la herramienta activa ya era nativo (`bToolRef` y `bToolRef_Runtime`
 * en DNA). Lo que faltaba era la TABLA y las consultas sobre ella; por eso hoy el
 * motor invierte el control y llama a un operador de Python para activar una
 * herramienta.
 */

#ifndef __FL_TOOLSYSTEM_HPP__
#define __FL_TOOLSYSTEM_HPP__

#include <string>

#include "BLI_span.hh"
#include "BLI_vector.hh"
#include "BLI_string_ref.hh"

struct bContext;
struct bToolRef;
struct StructRNA;
struct uiLayout;
struct wmKeyMap;

namespace flipendo::toolsystem {

/**
 * `Span` de una tabla escrita como array de C.
 *
 * `blender::Span` no se construye sola desde un array literal, y el catalogo son
 * cientos de tablas cortas. Sin esto cada una tendria que repetir el tamano a mano,
 * que es justo la clase de dato duplicado que acaba desincronizandose.
 */
template<typename T, size_t N> constexpr blender::Span<T> span(const T (&array)[N])
{
  return blender::Span<T>(array, int64_t(N));
}

/* -------------------------------------------------------------------- */
/** \name Ajustes de la herramienta, en forma de datos
 *
 * De los 82 `draw_settings` del Python solo 6 tienen flujo de control; el resto son
 * `layout.prop` sobre una o dos fuentes. Esos se declaran como filas y los pinta una
 * unica funcion generica, en vez de 76 funciones escritas a mano.
 * \{ */

/** De donde salen las propiedades de una fila. */
enum class PropSource {
  /** Propiedades del operador de la herramienta: `tool.operator_properties(op)`. */
  Operator,
  /** Propiedades del grupo de gizmos: `tool.gizmo_group_properties(group)`. */
  GizmoGroup,
  /** `context.tool_settings`. */
  ToolSettings,
  /** `context.scene.tool_settings.<sub>`. */
  ToolSettingsSub,
  /** `context.preferences.edit`. Lo usa el borrador de anotaciones, que todavia
   * guarda su radio en las preferencias y no en `tool_settings`. */
  PreferencesEdit,
};

enum PropRowFlag {
  PROP_ROW_NONE = 0,
  PROP_ROW_EXPAND = 1 << 0,
  PROP_ROW_TOGGLE = 1 << 1,
  /** Sin etiqueta: equivale a `text=""`. */
  PROP_ROW_NO_TEXT = 1 << 2,
  /** Solo se dibuja en el popover "extra" del topbar, no en la cabecera. */
  PROP_ROW_EXTRA_ONLY = 1 << 3,
  /** `icon_only=True`: se pintan solo los iconos de la enumeracion. */
  PROP_ROW_ICON_ONLY = 1 << 4,
  /** `slider=True`. */
  PROP_ROW_SLIDER = 1 << 5,
};

struct PropRow {
  PropSource source = PropSource::Operator;
  /** Operador, grupo de gizmos o sub-ruta de tool_settings, segun `source`. */
  const char *source_id = nullptr;
  const char *prop = nullptr;
  /** Etiqueta alternativa; `nullptr` usa la de la propiedad. Pasar por `N_()`. */
  const char *text = nullptr;
  int flags = PROP_ROW_NONE;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Declaracion de una herramienta
 * \{ */

/** `ToolDef.options` del Python. */
enum ToolOption {
  TOOL_OPTION_NONE = 0,
  /** `'USE_BRUSHES'`: la herramienta trabaja con el pincel activo. */
  TOOL_OPTION_USE_BRUSHES = 1 << 0,
  /** `'KEYMAP_FALLBACK'`: puede usarse como herramienta de reserva. */
  TOOL_OPTION_KEYMAP_FALLBACK = 1 << 1,
  /** No puede ser reserva de un pincel. Sustituye a la lista negra de seis cadenas
   * escrita a mano en `wm.py:2151-2158`. */
  TOOL_OPTION_NO_BRUSH_FALLBACK = 1 << 2,
};

/** Descripcion calculada. Solo 7 herramientas la necesitan, y leen el keymap DEL
 * USUARIO para meter el atajo dentro del texto. */
using DescriptionFn = std::string (*)(const bContext *C, const wmKeyMap *km);

/** Ajustes que no caben en `PropRow`. Solo 6 herramientas.
 *
 * `extra` distingue los dos sitios donde se pinta lo mismo: la cabecera de la
 * herramienta activa y el popover `TOPBAR_PT_tool_settings_extra`. */
using DrawSettingsFn = void (*)(const bContext *C, uiLayout *layout, bToolRef *tref, bool extra);

/** Dibujo sobre la vista mientras la herramienta esta activa (6 usos). Se registra y
 * se desregistra al cambiar de herramienta. */
using DrawCursorFn = void (*)(const bContext *C, void *customdata);

struct ToolDecl {
  /** `builtin.select_box`, `builtin_brush.smear`... Obligatorio. */
  const char *idname = nullptr;
  /** Nombre visible. Pasar por `N_()`: sin eso el catalogo i18n pierde 425 cadenas. */
  const char *label = nullptr;
  /** Tooltip literal, o `nullptr` si lo calcula `description_fn` o se hereda del
   * operador. Pasar por `N_()`. */
  const char *description = nullptr;
  DescriptionFn description_fn = nullptr;

  /** Nombre del icono en `datafiles/icons`, sin extension. */
  const char *icon = nullptr;
  /** Cursor del raton mientras la herramienta esta activa. */
  const char *cursor = nullptr;

  /** Grupo de gizmos que se activa con la herramienta. */
  const char *gizmo_group = nullptr;
  /** Propiedades iniciales del grupo de gizmos, si las lleva. */
  blender::Span<PropRow> gizmo_properties;

  /**
   * Nombre del keymap de la herramienta. SIEMPRE literal, NUNCA calculado.
   *
   * Da la tentacion de sintetizarlo con "{prefijo} {modo}, {etiqueta}", que es lo que
   * parece hacer el Python. Es falso: el Python lo guarda mutando una lista
   * compartida, asi que gana el PRIMER modo que registra la herramienta y los demas
   * heredan ese nombre. `builtin.radius` sale en Edit Curve, Edit Curves y Edit Grease
   * Pencil, y los tres usan "3D View Tool: Edit Curve, Radius". Calcularlo por modo
   * daria dos keymaps inexistentes y esas herramientas se quedarian sin atajos sin
   * que nada avisara. Ver politicas/TOOLSYSTEM-A-CPP.md.
   */
  const char *keymap_name = nullptr;

  /* NO hay campo `keymap_fallback`, y es a proposito.
   *
   * Tienta ponerlo, porque existe un keymap "<nombre> (fallback)" por cada herramienta.
   * Pero no es un dato suyo, por dos motivos distintos:
   *
   *   1. El armazon registra ese keymap para TODO keymap de herramienta que sea una
   *      cadena (`space_toolsystem_common.py:576`), sin que la herramienta diga nada.
   *   2. Al activar, el nombre se arma en ejecucion a partir de la herramienta de
   *      RESERVA del espacio (`:1039`), no de la que se esta activando. O sea que el
   *      valor correcto para una herramienta ni siquiera depende de ella.
   *
   * Cuando existia el campo, dos de los doce que trasladaron el catalogo lo rellenaron
   * con el nombre calculado de su propia herramienta. Los dos habrian estado mal, y
   * nada lo habria detectado: el campo no sale en la linea base. Por eso se quita en
   * vez de documentarse.
   */

  /** Identificador del tipo de pincel al que se limita, si `TOOL_OPTION_USE_BRUSHES`. */
  const char *brush_type = nullptr;

  /**
   * Identificador de dato asociado. Lo usan las 7 herramientas de particulas, y el
   * motor lo lee y lo escribe (`wm_toolsystem.cc:340`, `:671`) para sincronizar la
   * herramienta con el pincel activo. NO es un campo muerto.
   *
   * Ojo: aqui va el `identifier` del enum, no el `name`; el `name` es lo que forma el
   * `idname`. Confundirlos rompe el viaje de ida y vuelta.
   */
  const char *data_block = nullptr;

  /** Operador primario, solo para introspeccion (tooltip y teclas de acceso). */
  const char *op = nullptr;

  int options = TOOL_OPTION_NONE;

  /** La via declarativa: 76 de las 82 herramientas con ajustes. */
  blender::Span<PropRow> settings;
  /** La via de codigo: las 6 que no caben en filas. */
  DrawSettingsFn draw_settings = nullptr;
  DrawCursorFn draw_cursor = nullptr;

  /**
   * Marca TEMPORAL: la herramienta tiene ajustes en el Python que todavia no se han
   * trasladado, porque dependen de algo que aun no es nativo.
   *
   * Existe para que la deuda no se pierda: un `draw_settings` a `nullptr` es
   * indistinguible de "no tiene ajustes", y esa es exactamente la forma en que se
   * pierden capacidades sin que nadie se entere. El volcado la lista en cada
   * verificacion y la fase de dibujo comprueba que no quede ninguna puesta.
   */
  bool settings_pending = false;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name Colocacion: que herramientas se ven, en que orden y agrupadas como
 * \{ */

/** Filtro de contexto: las 9 entradas que en el Python son lambdas o llamables
 * dentro de la lista de un modo, y deciden si el bloque se muestra. */
using ToolPollFn = bool (*)(const bContext *C);

/**
 * Herramientas GENERADAS de una enumeracion RNA.
 *
 * `generate_from_enum_ex` del Python, que se usa en un unico sitio: los siete pinceles
 * del modo de particulas, sacados de `ParticleEdit.tool`. Se genera en vez de
 * escribirse a mano para que siga la enumeracion si esta cambia; una tabla copiada se
 * quedaria desincronizada en silencio.
 *
 * Ojo con la enumeracion, que usa sus dos campos para cosas distintas:
 *
 *     idname     = idname_prefix + enum.name         -> "builtin_brush.Comb"
 *     label      = enum.name                         -> "Comb"
 *     icon       = icon_prefix + lower(identifier)   -> "brush.particle.comb"
 *     data_block = enum.identifier                   -> "COMB"
 *
 * Confundir `name` con `identifier` da idnames que no existen, y el viaje de ida y
 * vuelta con el pincel activo deja de funcionar.
 */
struct EnumToolsDecl {
  const char *idname_prefix = nullptr;
  const char *icon_prefix = nullptr;
  /** El struct RNA que tiene la propiedad. Se pasa como funcion para no obligar a
   * incluir RNA_prototypes.hh en cada tabla del catalogo. */
  StructRNA *(*type_fn)() = nullptr;
  const char *attr = nullptr;
  int options = 0;
  const char *cursor = "DEFAULT";
  /** Los elementos vacios de la enumeracion son separadores de la barra. */
  bool use_separators = true;
};

/** Una entrada de la barra: una herramienta suelta, un grupo con ciclo, o un
 * separador. */
struct ToolEntry {
  /** Herramientas de la entrada. Una sola = herramienta suelta; varias = grupo con
   * ciclo bajo un mismo boton. Vacia = separador. */
  blender::Span<const ToolDecl *> tools;
  /** Si esta puesto y devuelve false, la entrada no se muestra. */
  ToolPollFn poll = nullptr;
  /** Si esta puesto, las herramientas salen de una enumeracion RNA en vez de `tools`. */
  const EnumToolsDecl *generated = nullptr;
};

/** Las entradas de un modo concreto de un espacio. */
struct ModeTools {
  /** `nullptr` = vale para cualquier modo (el `None` del Python). */
  const char *mode = nullptr;
  blender::Span<ToolEntry> entries;
};

/**
 * De donde sale el modo activo de un espacio.
 *
 * No hay un sitio comun: la vista 3D usa `context.mode`, el editor de imagen
 * `space_data.mode`, el de secuencias `space_data.view_type` y el de nodos
 * `space_data.tree_type`. Devuelve `nullptr` si no hay espacio o si el espacio no
 * tiene modos.
 */
using ModeFromContextFn = const char *(*)(const bContext *C);

/** Una barra de herramientas: un espacio con sus modos. */
struct ToolbarDecl {
  /** `SPACE_VIEW3D`, `SPACE_IMAGE`... */
  int space_type = 0;
  /** Prefijo de los nombres de keymap: "3D View Tool", "Image Editor Tool"... */
  const char *keymap_prefix = nullptr;
  /** Herramienta que se usa como reserva por defecto. */
  const char *tool_fallback_id = nullptr;
  ModeFromContextFn mode_from_context = nullptr;
  /**
   * Entradas por modo.
   *
   * La entrada con `mode == nullptr` son las herramientas COMUNES del espacio, y van
   * siempre delante de las del modo activo. Un espacio sin modos (hoy solo el de
   * nodos) tiene unicamente esa.
   *
   * Ojo con reproducir el Python literalmente aqui: alli las dos listas salen de
   * `(cls._tools[None], cls._tools.get(mode, ()))`, asi que en un espacio sin modos y
   * con `mode=None` se recorre LA MISMA lista dos veces y el catalogo sale duplicado.
   * Buscando la entrada del modo por nombre eso no puede pasar, porque `nullptr` no
   * casa con ningun modo. Ver tests/flipendo/toolsystem/README.md.
   */
  blender::Span<ModeTools> modes;
};

/** \} */

/* -------------------------------------------------------------------- */
/** \name El catalogo
 * \{ */

/** Todas las barras declaradas, una por espacio. */
blender::Span<const ToolbarDecl *> toolbars_all();

/** La barra de un espacio, o `nullptr` si ese espacio no tiene herramientas. */
const ToolbarDecl *toolbar_for_space(int space_type);

/**
 * Aplana las herramientas de un espacio y un modo DADOS, sin mirar en que espacio
 * esta el contexto. Es lo que necesita el volcado para recorrer el catalogo entero, y
 * la base sobre la que `tools_for_context` resuelve el modo.
 *
 * `mode` a `nullptr` devuelve solo las comunes.
 */
blender::Vector<const ToolDecl *> tools_for_space_mode(const bContext *C,
                                                       const ToolbarDecl &toolbar,
                                                       const char *mode);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Consultas
 *
 * Estas cuatro son las que hoy C++ le pregunta a Python construyendo cadenas de
 * codigo y evaluandolas (`interface_context_menu.cc:405`,
 * `interface_region_tooltip.cc:526`, `:578`, `:703`, `:775`). Son la diferencia entre
 * "los operadores ya son C++" y "el subsistema ya no llama al interprete".
 * \{ */

/** Herramientas visibles en el contexto actual, con los grupos ya aplanados y los
 * filtros aplicados. */
blender::Vector<const ToolDecl *> tools_for_context(const bContext *C, int space_type);

const ToolDecl *tool_find_by_id(const bContext *C, int space_type, blender::StringRefNull idname);
const ToolDecl *tool_find_by_index(const bContext *C, int space_type, int index);

blender::StringRefNull tool_label_for_id(const bContext *C,
                                         int space_type,
                                         blender::StringRefNull idname);
std::string tool_description_for_id(const bContext *C,
                                    int space_type,
                                    blender::StringRefNull idname,
                                    bool use_operator);
blender::Vector<blender::StringRefNull> tool_group_idnames_for_id(const bContext *C,
                                                                  int space_type,
                                                                  blender::StringRefNull idname);
wmKeyMap *tool_keymap_for_id(const bContext *C, int space_type, blender::StringRefNull idname);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Activacion
 * \{ */

/** Equivale a `activate_by_id`. Rellena el `bToolRef_Runtime` y lo instala. */
bool activate_by_id(bContext *C,
                    int space_type,
                    blender::StringRefNull idname,
                    bool as_fallback = false);

/** Equivale a `activate_by_id_or_cycle`: si la herramienta ya esta activa, avanza a la
 * siguiente de su grupo. */
bool activate_by_id_or_cycle(bContext *C,
                             int space_type,
                             blender::StringRefNull idname,
                             int offset = 1,
                             bool as_fallback = false);

/**
 * Memoria de que variante de cada grupo se uso la ultima vez.
 *
 * No va a DNA a proposito: en el Python es un diccionario de clase que se pierde al
 * reiniciar (`space_toolsystem_common.py:527`), y el dibujo de la barra lo escribe
 * mientras dibuja. Se reproduce igual, por espacio y sin serializar.
 */
int group_active_get(int space_type, blender::StringRefNull group_leader_idname);
void group_active_set(int space_type, blender::StringRefNull group_leader_idname, int index);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Deuda pendiente
 * \{ */

/** Los idnames de las herramientas con `settings_pending`, en orden de catalogo. */
blender::Vector<blender::StringRefNull> settings_pending_list();

/** \} */

}  // namespace flipendo::toolsystem

#endif /* __FL_TOOLSYSTEM_HPP__ */
