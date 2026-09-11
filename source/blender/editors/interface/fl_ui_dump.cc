/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Volcado y verificacion de la interfaz del editor. Ver FL_ui_dump.hpp.
 *
 * El fichero de salida es una lista de BLOQUES. Cada bloque empieza por una linea
 * `=== <TIPO> <idname>` y sigue con su contenido, una cosa por linea. El bloque es
 * la unidad que compara `--fl-check-ui`, y por eso el parte puede decir "1.632
 * bloques, 1.630 identicos, 2 distintos, y estos son": es la cifra que hace falta
 * para migrar `bl_ui` panel a panel sin perder nada por el camino.
 *
 * Las claves de dentro van en orden alfabetico y los bloques por
 * (espacio, region, categoria, idname), como manda el encargo. Todo lo que se
 * escribe es determinista: ni punteros, ni tiempos, ni contadores que cambien.
 */

#include <algorithm>
#include <csetjmp>
#include <csignal>
#include <cstdlib>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <string>

#include "MEM_guardedalloc.h"

#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_fileops.h"
#include "BLI_index_range.hh"
#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_string.h"
#include "BLI_string_ref.hh"
#include "BLI_utildefines.h"
#include "BLI_vector.hh"

#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "RNA_access.hh"
#include "RNA_enum_types.hh"

#include "ED_buttons.hh"
#include "ED_screen.hh"

#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "interface_intern.hh"

#include "FL_ui_dump.hpp"
#include "preset/FL_preset_ui.hpp"

#include "../asset/ED_asset_shelf.hh"

#include <memory>

namespace flipendo::ui_dump {

/** Marca de la primera linea: dice a `--fl-check-ui` que linea base tiene delante. */
static const char *MARCA_REGISTRO = "# FL-UI-DUMP registro v1";
static const char *MARCA_DISENO = "# FL-UI-DUMP diseno v1";

/* -------------------------------------------------------------------- */
/** \name Formato
 * \{ */

/** Una cadena entre comillas simples, con escapes; `None` si es un puntero nulo. */
static std::string quoted(const char *s)
{
  if (s == nullptr) {
    return "None";
  }
  std::string out = "'";
  for (const char *c = s; *c != '\0'; c++) {
    switch (*c) {
      case '\\':
        out += "\\\\";
        break;
      case '\'':
        out += "\\'";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(*c) < 0x20 || static_cast<unsigned char>(*c) == 0x7f) {
          char buf[8];
          SNPRINTF(buf, "\\x%02x", static_cast<unsigned char>(*c));
          out += buf;
        }
        else {
          out += *c;
        }
        break;
    }
  }
  out += "'";
  return out;
}

static std::string quoted(const blender::StringRef s)
{
  return quoted(std::string(s).c_str());
}

static const char *si_no(const bool v)
{
  return v ? "si" : "no";
}

static std::string entero(const int v)
{
  char buf[32];
  SNPRINTF(buf, "%d", v);
  return buf;
}

/** `%g`, para que 1.0 salga "1" y no "1.000000". */
static std::string real(const float v)
{
  char buf[32];
  SNPRINTF(buf, "%g", double(v));
  return buf;
}

/** Lista de nombres de banderas, en orden alfabetico; `[]` si no hay ninguna. */
static std::string banderas(const int value,
                            const int *bits,
                            const char *const *names,
                            const int count)
{
  blender::Vector<const char *> puestas;
  for (const int i : blender::IndexRange(count)) {
    if (value & bits[i]) {
      puestas.append(names[i]);
    }
  }
  std::sort(puestas.begin(), puestas.end(), [](const char *a, const char *b) {
    return strcmp(a, b) < 0;
  });
  std::string out = "[";
  for (const int i : puestas.index_range()) {
    if (i != 0) {
      out += ",";
    }
    out += puestas[i];
  }
  out += "]";
  return out;
}

/** El identificador RNA del espacio: `VIEW_3D`, `PROPERTIES`... */
static const char *space_type_identifier(const int space_type)
{
  const char *identifier = nullptr;
  if (!RNA_enum_identifier(rna_enum_space_type_items, space_type, &identifier)) {
    return "DESCONOCIDO";
  }
  return identifier;
}

/** El identificador RNA de la region: `WINDOW`, `UI`, `HEADER`... */
static const char *region_type_identifier(const int region_type)
{
  const char *identifier = nullptr;
  if (!RNA_enum_identifier(rna_enum_region_type_items, region_type, &identifier)) {
    return "DESCONOCIDO";
  }
  return identifier;
}


/** El nombre del relieve, en vez de su numero: `EMBOSS`, `NONE`, `PULLDOWN`... */
static const char *emboss_nombre(const blender::ui::EmbossType emboss)
{
  switch (emboss) {
    case blender::ui::EmbossType::Emboss:
      return "EMBOSS";
    case blender::ui::EmbossType::None:
      return "NONE";
    case blender::ui::EmbossType::Pulldown:
      return "PULLDOWN";
    case blender::ui::EmbossType::PieMenu:
      return "PIE_MENU";
    case blender::ui::EmbossType::NoneOrStatus:
      return "NONE_OR_STATUS";
    case blender::ui::EmbossType::Undefined:
      return "UNDEFINED";
  }
  return "DESCONOCIDO";
}

/** La alineacion de un layout: `EXPAND`, `LEFT`, `CENTER`, `RIGHT`. */
static const char *alignment_nombre(const int alignment)
{
  switch (alignment) {
    case UI_LAYOUT_ALIGN_EXPAND:
      return "EXPAND";
    case UI_LAYOUT_ALIGN_LEFT:
      return "LEFT";
    case UI_LAYOUT_ALIGN_CENTER:
      return "CENTER";
    case UI_LAYOUT_ALIGN_RIGHT:
      return "RIGHT";
  }
  return "DESCONOCIDO";
}

/** El nombre del tipo de boton. Un numero no dice nada en un diff. */
static const char *but_type_nombre(const eButType type)
{
  switch (type) {
    case UI_BTYPE_BUT:
      return "BUT";
    case UI_BTYPE_ROW:
      return "ROW";
    case UI_BTYPE_TEXT:
      return "TEXT";
    case UI_BTYPE_MENU:
      return "MENU";
    case UI_BTYPE_BUT_MENU:
      return "BUT_MENU";
    case UI_BTYPE_NUM:
      return "NUM";
    case UI_BTYPE_NUM_SLIDER:
      return "NUM_SLIDER";
    case UI_BTYPE_TOGGLE:
      return "TOGGLE";
    case UI_BTYPE_TOGGLE_N:
      return "TOGGLE_N";
    case UI_BTYPE_ICON_TOGGLE:
      return "ICON_TOGGLE";
    case UI_BTYPE_ICON_TOGGLE_N:
      return "ICON_TOGGLE_N";
    case UI_BTYPE_BUT_TOGGLE:
      return "BUT_TOGGLE";
    case UI_BTYPE_CHECKBOX:
      return "CHECKBOX";
    case UI_BTYPE_CHECKBOX_N:
      return "CHECKBOX_N";
    case UI_BTYPE_COLOR:
      return "COLOR";
    case UI_BTYPE_TAB:
      return "TAB";
    case UI_BTYPE_POPOVER:
      return "POPOVER";
    case UI_BTYPE_SCROLL:
      return "SCROLL";
    case UI_BTYPE_BLOCK:
      return "BLOCK";
    case UI_BTYPE_LABEL:
      return "LABEL";
    case UI_BTYPE_LINK:
      return "LINK";
    case UI_BTYPE_INLINK:
      return "INLINK";
    case UI_BTYPE_KEY_EVENT:
      return "KEY_EVENT";
    case UI_BTYPE_HSVCUBE:
      return "HSVCUBE";
    case UI_BTYPE_PULLDOWN:
      return "PULLDOWN";
    case UI_BTYPE_ROUNDBOX:
      return "ROUNDBOX";
    case UI_BTYPE_COLORBAND:
      return "COLORBAND";
    case UI_BTYPE_UNITVEC:
      return "UNITVEC";
    case UI_BTYPE_CURVE:
      return "CURVE";
    case UI_BTYPE_CURVEPROFILE:
      return "CURVEPROFILE";
    case UI_BTYPE_LISTBOX:
      return "LISTBOX";
    case UI_BTYPE_LISTROW:
      return "LISTROW";
    case UI_BTYPE_HSVCIRCLE:
      return "HSVCIRCLE";
    case UI_BTYPE_TRACK_PREVIEW:
      return "TRACK_PREVIEW";
    case UI_BTYPE_SEARCH_MENU:
      return "SEARCH_MENU";
    case UI_BTYPE_EXTRA:
      return "EXTRA";
    case UI_BTYPE_PREVIEW_TILE:
      return "PREVIEW_TILE";
    case UI_BTYPE_HOTKEY_EVENT:
      return "HOTKEY_EVENT";
    case UI_BTYPE_IMAGE:
      return "IMAGE";
    case UI_BTYPE_HISTOGRAM:
      return "HISTOGRAM";
    case UI_BTYPE_WAVEFORM:
      return "WAVEFORM";
    case UI_BTYPE_VECTORSCOPE:
      return "VECTORSCOPE";
    case UI_BTYPE_PROGRESS:
      return "PROGRESS";
    case UI_BTYPE_NODE_SOCKET:
      return "NODE_SOCKET";
    case UI_BTYPE_SEPR:
      return "SEPR";
    case UI_BTYPE_SEPR_LINE:
      return "SEPR_LINE";
    case UI_BTYPE_SEPR_SPACER:
      return "SEPR_SPACER";
    case UI_BTYPE_GRIP:
      return "GRIP";
    case UI_BTYPE_DECORATOR:
      return "DECORATOR";
    case UI_BTYPE_VIEW_ITEM:
      return "VIEW_ITEM";
  }
  return "DESCONOCIDO";
}

/** El nombre del icono: `ICON_NONE`, `MESH_CUBE`... */
static std::string icon_identifier(const int icon)
{
  const char *identifier = nullptr;
  if (!RNA_enum_identifier(rna_enum_icon_items, icon, &identifier)) {
    /* Los iconos de previsualizacion y los de tipo de dato no estan en la
     * enumeracion; su numero es estable, asi que vale para comparar. */
    return "#" + entero(icon);
  }
  return identifier;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Bloques
 * \{ */

struct Bloque {
  /** `PANEL VIEW3D_PT_transform`: lo que identifica el bloque en la comparacion. */
  std::string clave;
  /** Orden estable: tipo, espacio, region, categoria, idname. */
  std::string orden;
  blender::Vector<std::string> lineas;
};

static void bloques_ordenar(blender::Vector<Bloque> &bloques)
{
  std::sort(bloques.begin(), bloques.end(), [](const Bloque &a, const Bloque &b) {
    return a.orden < b.orden;
  });
}

/** La clave de orden: el tipo primero, y dentro (espacio, region, categoria, idname). */
static std::string clave_orden(const int tipo,
                               const char *space,
                               const char *region,
                               const char *category,
                               const char *idname)
{
  std::string out = entero(tipo);
  out += '\x01';
  out += space ? space : "";
  out += '\x01';
  out += region ? region : "";
  out += '\x01';
  out += category ? category : "";
  out += '\x01';
  out += idname;
  return out;
}

static bool bloques_escribir(const blender::Vector<Bloque> &bloques,
                             const char *marca,
                             const char *filepath)
{
  FILE *fp = BLI_fopen(filepath, "w");
  if (fp == nullptr) {
    fprintf(stderr, "No se pudo abrir '%s' para escribir.\n", filepath);
    return false;
  }
  fprintf(fp, "%s\n", marca);
  for (const Bloque &bloque : bloques) {
    fprintf(fp, "=== %s\n", bloque.clave.c_str());
    for (const std::string &linea : bloque.lineas) {
      fprintf(fp, "%s\n", linea.c_str());
    }
  }
  fclose(fp);
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name El registro: paneles, menus y cabeceras dados de alta
 *
 * Se recorre por espacios y regiones — `BKE_spacetypes_list()` — y no por el
 * registro global, porque es el recorrido que ve el editor al dibujar: un panel
 * que esta en el mapa global pero en ninguna region no lo dibuja nadie.
 * \{ */

static const int panel_flag_bits[] = {
    PANEL_TYPE_DEFAULT_CLOSED,
    PANEL_TYPE_NO_HEADER,
    PANEL_TYPE_HEADER_EXPAND,
    PANEL_TYPE_LAYOUT_VERT_BAR,
    PANEL_TYPE_INSTANCED,
    PANEL_TYPE_NO_SEARCH,
};
static const char *const panel_flag_names[] = {
    "DEFAULT_CLOSED",
    "NO_HEADER",
    "HEADER_EXPAND",
    "LAYOUT_VERT_BAR",
    "INSTANCED",
    "NO_SEARCH",
};

static const int menu_flag_bits[] = {
    int(MenuTypeFlag::ContextDependent),
    int(MenuTypeFlag::SearchOnKeyPress),
};
static const char *const menu_flag_names[] = {
    "CONTEXT_DEPENDENT",
    "SEARCH_ON_KEY_PRESS",
};

static Bloque panel_bloque(const PanelType *pt)
{
  const char *space = space_type_identifier(pt->space_type);
  const char *region = region_type_identifier(pt->region_type);

  Bloque bloque;
  /* La clave lleva espacio y region porque el idname NO es unico: `OPERATOR_PT_redo`
   * esta registrado en la region HUD de seis editores, con seis PanelType distintos.
   * Con la clave corta, cinco de los seis se perdian en la comparacion. */
  bloque.clave = std::string("PANEL ") + space + " " + region + " " + pt->idname;
  bloque.orden = clave_orden(1, space, region, pt->category, pt->idname);

  /* Claves en orden alfabetico. */
  bloque.lineas.append("active_property=" + quoted(pt->active_property));
  bloque.lineas.append("category=" + quoted(pt->category));
  bloque.lineas.append("context=" + quoted(pt->context));
  bloque.lineas.append("description=" + quoted(pt->description));
  bloque.lineas.append(std::string("draw=") + si_no(pt->draw != nullptr));
  bloque.lineas.append(std::string("draw_header=") + si_no(pt->draw_header != nullptr));
  bloque.lineas.append(std::string("draw_header_preset=") +
                       si_no(pt->draw_header_preset != nullptr));
  bloque.lineas.append(
      "flag=" + banderas(pt->flag, panel_flag_bits, panel_flag_names, ARRAY_SIZE(panel_flag_bits)));
  bloque.lineas.append("label=" + quoted(pt->label));
  bloque.lineas.append("order=" + entero(pt->order));
  bloque.lineas.append("owner_id=" + quoted(pt->owner_id));
  bloque.lineas.append("parent_id=" + quoted(pt->parent_id));
  bloque.lineas.append("pin_to_last_property=" + quoted(pt->pin_to_last_property));
  bloque.lineas.append(std::string("poll=") + si_no(pt->poll != nullptr));
  bloque.lineas.append(std::string("region=") + region);
  bloque.lineas.append(std::string("space=") + space);
  bloque.lineas.append("translation_context=" + quoted(pt->translation_context));
  bloque.lineas.append("ui_units_x=" + entero(pt->ui_units_x));
  return bloque;
}

static Bloque header_bloque(const HeaderType *ht)
{
  const char *space = space_type_identifier(ht->space_type);
  const char *region = region_type_identifier(ht->region_type);

  Bloque bloque;
  bloque.clave = std::string("HEADER ") + space + " " + region + " " + ht->idname;
  bloque.orden = clave_orden(2, space, region, "", ht->idname);

  bloque.lineas.append(std::string("draw=") + si_no(ht->draw != nullptr));
  bloque.lineas.append(std::string("poll=") + si_no(ht->poll != nullptr));
  bloque.lineas.append(std::string("region=") + region);
  bloque.lineas.append(std::string("space=") + space);
  return bloque;
}

static Bloque menu_bloque(const MenuType *mt)
{
  Bloque bloque;
  bloque.clave = std::string("MENU ") + mt->idname;
  bloque.orden = clave_orden(3, "", "", "", mt->idname);

  bloque.lineas.append("description=" + quoted(mt->description));
  bloque.lineas.append(std::string("draw=") + si_no(mt->draw != nullptr));
  bloque.lineas.append("flag=" + banderas(int(mt->flag),
                                          menu_flag_bits,
                                          menu_flag_names,
                                          ARRAY_SIZE(menu_flag_bits)));
  bloque.lineas.append("label=" + quoted(mt->label));
  bloque.lineas.append("owner_id=" + quoted(mt->owner_id));
  bloque.lineas.append(std::string("poll=") + si_no(mt->poll != nullptr));
  bloque.lineas.append("translation_context=" + quoted(mt->translation_context));
  return bloque;
}

/**
 * El ORDEN de los paneles dentro de una region, que es tan observable como sus
 * campos y que ningun campo captura: `order` es una prioridad, no una posicion, y
 * entre iguales manda el orden de registro. Al pasar un panel de Python a C++ el
 * registro deja de ocurrir al cargar los scripts y pasa a ocurrir en
 * `ED_spacetypes_init`, mucho antes — y un panel puede cambiar de sitio sin que
 * ninguno de sus campos cambie. Este bloque lo caza.
 */
static Bloque region_bloque(const SpaceType &st, const ARegionType &art)
{
  const char *space = space_type_identifier(st.spaceid);
  const char *region = region_type_identifier(art.regionid);

  Bloque bloque;
  bloque.clave = std::string("REGION ") + space + " " + region;
  bloque.orden = clave_orden(0, space, region, "", "");

  /* En orden de lista, no alfabetico: la lista ES el resultado. */
  LISTBASE_FOREACH (const PanelType *, pt, &art.paneltypes) {
    bloque.lineas.append(std::string("panel=") + pt->idname);
  }
  LISTBASE_FOREACH (const HeaderType *, ht, &art.headertypes) {
    bloque.lineas.append(std::string("header=") + ht->idname);
  }
  return bloque;
}

/**
 * Bloque de una lista.
 *
 * Las listas no cuelgan de ninguna region: viven en su propio registro global y
 * `uiTemplateList` las busca por `idname`. Hasta que este volcado existio, una lista
 * migrada a C++ no la veia nadie — el volcador recorria `paneltypes` y `headertypes`
 * y nada mas. Ese es exactamente el falso positivo que este proyecto se comprometio a
 * no tener, y por eso el registro y el volcado crecieron a la vez.
 */
static Bloque uilist_bloque(const uiListType *ult)
{
  Bloque bloque;
  bloque.clave = std::string("UILIST ") + ult->idname;
  bloque.orden = clave_orden(4, "", "", "", ult->idname);

  bloque.lineas.append(std::string("draw_filter=") + si_no(ult->draw_filter != nullptr));
  bloque.lineas.append(std::string("draw_item=") + si_no(ult->draw_item != nullptr));
  bloque.lineas.append(std::string("filter_items=") + si_no(ult->filter_items != nullptr));
  bloque.lineas.append(std::string("listener=") + si_no(ult->listener != nullptr));
  return bloque;
}

/** Bloque de una estanteria de recursos. */
static Bloque asset_shelf_bloque(const AssetShelfType &type)
{
  const char *space = space_type_identifier(type.space_type);

  Bloque bloque;
  bloque.clave = std::string("ASSETSHELF ") + type.idname;
  bloque.orden = clave_orden(5, space, "", "", type.idname);

  bloque.lineas.append("activate_operator=" + quoted(type.activate_operator.c_str()));
  bloque.lineas.append(std::string("asset_poll=") + si_no(type.asset_poll != nullptr));
  bloque.lineas.append("default_preview_size=" + entero(type.default_preview_size));
  bloque.lineas.append(std::string("draw_context_menu=") +
                       si_no(type.draw_context_menu != nullptr));
  bloque.lineas.append("flag=" + entero(int(type.flag)));
  bloque.lineas.append(std::string("get_active_asset=") + si_no(type.get_active_asset != nullptr));
  bloque.lineas.append(std::string("poll=") + si_no(type.poll != nullptr));
  bloque.lineas.append(std::string("space=") + space);
  return bloque;
}


static blender::Vector<Bloque> registro_bloques()
{
  blender::Vector<Bloque> bloques;

  for (const std::unique_ptr<SpaceType> &st : BKE_spacetypes_list()) {
    LISTBASE_FOREACH (ARegionType *, art, &st->regiontypes) {
      if (!BLI_listbase_is_empty(&art->paneltypes) ||
          !BLI_listbase_is_empty(&art->headertypes))
      {
        bloques.append(region_bloque(*st, *art));
      }
      LISTBASE_FOREACH (PanelType *, pt, &art->paneltypes) {
        bloques.append(panel_bloque(pt));
      }
      LISTBASE_FOREACH (HeaderType *, ht, &art->headertypes) {
        bloques.append(header_bloque(ht));
      }
    }
  }

  /* Los menus no cuelgan de ninguna region: viven en el registro global. */
  for (const MenuType *mt : WM_menutypes_registered_get()) {
    bloques.append(menu_bloque(mt));
  }

  /* Las listas, tampoco. */
  for (const uiListType *ult : WM_uilisttypes_registered_get()) {
    bloques.append(uilist_bloque(ult));
  }

  /* Y las estanterias de recursos tienen su propio registro. */
  for (const std::unique_ptr<AssetShelfType> &type : blender::ed::asset::shelf::types_all()) {
    bloques.append(asset_shelf_bloque(*type));
  }

  bloques_ordenar(bloques);
  return bloques;
}

bool dump_registry(const bContext * /*C*/, const char *filepath)
{
  const blender::Vector<Bloque> bloques = registro_bloques();
  if (!bloques_escribir(bloques, MARCA_REGISTRO, filepath)) {
    return false;
  }

  int paneles = 0, menus = 0, cabeceras = 0, regiones = 0, listas = 0, estanterias = 0;
  for (const Bloque &bloque : bloques) {
    if (bloque.clave.rfind("PANEL ", 0) == 0) {
      paneles++;
    }
    else if (bloque.clave.rfind("MENU ", 0) == 0) {
      menus++;
    }
    else if (bloque.clave.rfind("HEADER ", 0) == 0) {
      cabeceras++;
    }
    else if (bloque.clave.rfind("UILIST ", 0) == 0) {
      listas++;
    }
    else if (bloque.clave.rfind("ASSETSHELF ", 0) == 0) {
      estanterias++;
    }
    else {
      regiones++;
    }
  }
  printf(
      "UI_DUMP_OK bloques=%d paneles=%d menus=%d cabeceras=%d regiones=%d listas=%d "
      "estanterias=%d\n",
      int(bloques.size()),
      paneles,
      menus,
      cabeceras,
      regiones,
      listas,
      estanterias);
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name El dibujo: serializar el arbol de uiLayout
 * \{ */

static void serializar_boton(const uiBut *but,
                             bContext *C,
                             const int profundidad,
                             blender::Vector<std::string> &out)
{
  std::string linea(profundidad * 2, ' ');
  linea += "BUTTON";

  /* Claves en orden alfabetico. */
  linea += std::string(" activo=") + si_no((but->flag & UI_BUT_INACTIVE) == 0);
  linea += std::string(" emboss=") + emboss_nombre(but->emboss);
  linea += std::string(" enabled=") + si_no((but->flag & UI_BUT_DISABLED) == 0);
  linea += " icon=" + icon_identifier(but->icon);

  if (but->optype) {
    /* Con `all_args=false` solo salen los argumentos que el panel cambio: es lo
     * que distingue `game_property_move(direction='UP')` de su gemelo 'DOWN'. */
    const std::string op = WM_operator_pystring_ex(
        C, nullptr, false, true, but->optype, but->opptr);
    linea += " op=" + quoted(op.c_str());
  }
  else {
    linea += " op=None";
  }

  if (but->rnaprop) {
    std::string rna = RNA_struct_identifier(but->rnapoin.type);
    rna += ".";
    rna += RNA_property_identifier(but->rnaprop);
    rna += "[" + entero(but->rnaindex) + "]";
    linea += " rna=" + quoted(rna.c_str());
  }
  else {
    linea += " rna=None";
  }

  linea += " text=" + quoted(but->drawstr.c_str());
  linea += " tip=" + quoted(but->tip);
  linea += std::string(" type=") + but_type_nombre(but->type);

  out.append(linea);
}

static void serializar_item(const uiItem *item,
                            bContext *C,
                            const int profundidad,
                            blender::Vector<std::string> &out)
{
  if (const uiBut *but = item_button(item)) {
    serializar_boton(but, C, profundidad, out);
    return;
  }

  /* Todo lo que no es boton es un sub-layout, y sus campos son publicos. */
  const uiLayout *layout = static_cast<const uiLayout *>(item);
  uiLayout *layout_mut = const_cast<uiLayout *>(layout);

  std::string linea(profundidad * 2, ' ');
  linea += item_type_name(item);

  /* Claves en orden alfabetico. */
  linea += std::string(" activo=") + si_no(layout->active_);
  linea += " align=" + std::string(si_no(layout->align_));
  linea += std::string(" alignment=") + alignment_nombre(int(layout->alignment_));
  linea += std::string(" emboss=") + emboss_nombre(layout->emboss_);
  linea += std::string(" enabled=") + si_no(layout->enabled_);

  float percentage;
  int number;
  bool row_major, even_columns, even_rows;
  int columns_len;
  if (item_grid_flow(item, &row_major, &columns_len, &even_columns, &even_rows)) {
    linea += " grid_columns=" + entero(columns_len);
    linea += std::string(" grid_even_columns=") + si_no(even_columns);
    linea += std::string(" grid_even_rows=") + si_no(even_rows);
    linea += std::string(" grid_row_major=") + si_no(row_major);
  }
  linea += " heading=" + quoted(layout->heading_);
  if (item_flow_number(item, &number)) {
    linea += " number=" + entero(number);
  }
  if (const char *open_prop = item_panel_open_prop(item)) {
    linea += " open_prop=" + quoted(open_prop);
  }
  if (item_split_percentage(item, &percentage)) {
    linea += " percentage=" + real(percentage);
  }
  linea += std::string(" prop_decorate=") + si_no(uiLayoutGetPropDecorate(layout_mut));
  linea += std::string(" prop_sep=") + si_no(uiLayoutGetPropSep(layout_mut));
  linea += std::string(" redalert=") + si_no(layout->redalert_);
  linea += " scale_x=" + real(layout->scale_[0]);
  linea += " scale_y=" + real(layout->scale_[1]);
  linea += " units_x=" + real(layout->units_[0]);
  linea += " units_y=" + real(layout->units_[1]);

  out.append(linea);

  for (const uiItem *hijo : layout->items_) {
    serializar_item(hijo, C, profundidad + 1, out);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Donde dibujar: buscar una region viva para cada espacio
 *
 * Un `draw()` de verdad necesita ventana, area y region: sin ellas, ni el `poll`
 * ni `CTX_data_*` responden. Se usan las areas que el fichero de fabrica abre y,
 * para los espacios que no estan abiertos, se cambia de tipo un area de reserva
 * con `ED_area_newspace` — exactamente lo que hace el desplegable de editor.
 * \{ */

/**
 * Un area viva del espacio pedido, si el fichero de fabrica la tiene abierta.
 *
 * Mira tambien las areas globales de la ventana: la barra superior y la de estado
 * no estan en `screen->areabase`, y sin esto sus cabeceras y sus paneles — el
 * popover de anotaciones, entre otros — quedarian sin cubrir.
 */
static bool area_viva(bContext *C, const int space_type, wmWindow **r_win, ScrArea **r_area)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    ListBase *listas[2] = {nullptr, &win->global_areas.areabase};
    bScreen *screen = WM_window_get_active_screen(win);
    if (screen != nullptr) {
      listas[0] = &screen->areabase;
    }
    for (ListBase *lista : listas) {
      if (lista == nullptr) {
        continue;
      }
      LISTBASE_FOREACH (ScrArea *, area, lista) {
        if (area->spacetype == space_type && area->type != nullptr) {
          *r_win = win;
          *r_area = area;
          return true;
        }
      }
    }
  }
  return false;
}

/** El area mas grande de la primera ventana: la que se recicla para los espacios cerrados. */
static bool area_de_reserva(bContext *C, wmWindow **r_win, ScrArea **r_area)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = static_cast<wmWindow *>(wm->windows.first);
  if (win == nullptr) {
    return false;
  }
  bScreen *screen = WM_window_get_active_screen(win);
  if (screen == nullptr) {
    return false;
  }
  ScrArea *mejor = nullptr;
  int mejor_area = -1;
  LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
    const int superficie = area->winx * area->winy;
    if (superficie > mejor_area) {
      mejor_area = superficie;
      mejor = area;
    }
  }
  if (mejor == nullptr) {
    return false;
  }
  *r_win = win;
  *r_area = mejor;
  return true;
}

static ARegion *region_de_tipo(const ScrArea *area, const int region_type)
{
  LISTBASE_FOREACH (ARegion *, region, &area->regionbase) {
    if (region->regiontype == region_type && region->runtime != nullptr &&
        region->runtime->type != nullptr)
    {
      return region;
    }
  }
  return nullptr;
}

/** \} */


/* -------------------------------------------------------------------- */
/** \name La pestana del editor de Propiedades
 *
 * `properties_*.py` es el mayor bloque de `bl_ui`, y sus paneles no pasan el
 * `poll` si el editor no esta en su pestana: `context.material` sale de la ruta
 * de contexto que calcula el editor para la pestana activa. Poner la pestana que
 * le toca a cada panel antes de dibujarlo es la diferencia entre cubrir la
 * pestana de Objeto y cubrirlas todas.
 * \{ */

struct ContextoPestana {
  const char *context;
  int tab;
};

/* El reverso de `buttons_main_region_context_string` (space_buttons.cc:261). */
static const ContextoPestana pestanas[] = {
    {"bone", BCONTEXT_BONE},
    {"bone_constraint", BCONTEXT_BONE_CONSTRAINT},
    {"collection", BCONTEXT_COLLECTION},
    {"constraint", BCONTEXT_CONSTRAINT},
    {"data", BCONTEXT_DATA},
    {"game", BCONTEXT_GAME},
    {"material", BCONTEXT_MATERIAL},
    {"modifier", BCONTEXT_MODIFIER},
    {"object", BCONTEXT_OBJECT},
    {"output", BCONTEXT_OUTPUT},
    {"particle", BCONTEXT_PARTICLE},
    {"physics", BCONTEXT_PHYSICS},
    {"render", BCONTEXT_RENDER},
    {"scene", BCONTEXT_SCENE},
    {"shaderfx", BCONTEXT_SHADERFX},
    {"texture", BCONTEXT_TEXTURE},
    {"tool", BCONTEXT_TOOL},
    {"view_layer", BCONTEXT_VIEW_LAYER},
    {"world", BCONTEXT_WORLD},
};

/**
 * Pone la pestana que pide `pt->context`. Un panel sin contexto se dibuja con la
 * pestana de Objeto, siempre la misma, para que el volcado sea determinista y no
 * dependa del panel anterior.
 */
static void pestana_poner(bContext *C, ScrArea *area, const char *context)
{
  SpaceProperties *sbuts = static_cast<SpaceProperties *>(area->spacedata.first);
  if (sbuts == nullptr) {
    return;
  }
  int tab = BCONTEXT_OBJECT;
  if (context != nullptr && context[0] != '\0') {
    for (const ContextoPestana &pestana : pestanas) {
      if (STREQ(pestana.context, context)) {
        tab = pestana.tab;
        break;
      }
    }
  }
  ED_buttons_context_tab_set(C, sbuts, tab);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lo que no se puede dibujar en seco
 *
 * Un `poll` que devuelve `false` ya deja el tipo fuera y anotado, que es el caso
 * normal. Esta lista es para el caso feo: el tipo cuyo `draw()` da por hecho un
 * contexto que la escena de fabrica no tiene y se lleva el proceso por delante en
 * vez de fallar limpiamente.
 *
 * Cada entrada lleva su motivo escrito. Una lista de excepciones sin motivos es una
 * lista de errores escondidos, y esa es exactamente la forma en que una migracion
 * pierde capacidades sin que nadie se entere.
 * \{ */

struct NoDibujable {
  const char *idname;
  const char *motivo;
};

static const NoDibujable no_dibujables[] = {
    /* Este no se lleva el proceso por delante: escribe la FECHA y el HASH de la
     * compilacion. Con el dentro, la linea base caduca en cuanto alguien recompila y
     * el verificador empieza a dar un falso rojo cada dia. Lo encontro el propio
     * verificador la primera vez que se comparo contra una linea base de otra build. */
    {"WM_MT_splash_about", "datos-de-la-build"},
    /* Marca de fin: un `motivo` nulo nunca veta nada, y un array de cero elementos no
     * es C++ estandar. */
    {"", nullptr},
};

static const char *no_dibujable_motivo(const char *idname)
{
  for (const NoDibujable &entrada : no_dibujables) {
    if (entrada.motivo != nullptr && STREQ(entrada.idname, idname)) {
      return entrada.motivo;
    }
  }
  return nullptr;
}

/** \} */


/* -------------------------------------------------------------------- */
/** \name La red debajo del dibujo
 *
 * Ejecutar el `draw()` de mil paneles fuera de su sitio natural encuentra, tarde o
 * temprano, uno que da por hecho algo que la escena de fabrica no tiene y se lleva
 * el proceso por delante. Sin red, un solo tipo asi deja el volcado entero en nada
 * — que es exactamente lo que paso la primera vez, con `NODE_MT_category_GEO_OUTPUT`.
 *
 * Esta barrera atrapa la senal, anota el tipo como NO CUBIERTO y sigue. Dos cosas
 * importan y por eso estan escritas aqui:
 *
 * 1. Lo que cae en la barrera NUNCA cuenta como idéntico: se marca `fallo` y se
 *    lista en el parte. La barrera sirve para no perder los otros mil, no para
 *    tapar este.
 * 2. Volver de un SIGSEGV con `siglongjmp` deja memoria a medias. Se asume a
 *    proposito: es un proceso de diagnostico de un solo uso que sale al terminar,
 *    y el bloque que quedo a medias se abandona en vez de tocarlo.
 * \{ */

static sigjmp_buf barrera_salto;
static volatile sig_atomic_t barrera_activa = 0;
/** Cuantas veces tuvo que actuar la red. Se dice siempre en el parte. */
static int barrera_fallos = 0;

static void barrera_senal(int sig)
{
  if (barrera_activa) {
    barrera_activa = 0;
    siglongjmp(barrera_salto, sig);
  }
  /* Fuera de un draw no es cosa nuestra: que actue el manejador de siempre. */
  signal(sig, SIG_DFL);
  raise(sig);
}

/** Ejecuta `fn` con la red puesta. Devuelve 0 si fue bien, o el numero de senal. */
template<typename Fn> static int con_barrera(Fn &&fn)
{
  void (*previo_segv)(int) = signal(SIGSEGV, barrera_senal);
  void (*previo_bus)(int) = signal(SIGBUS, barrera_senal);
  void (*previo_fpe)(int) = signal(SIGFPE, barrera_senal);
  void (*previo_ill)(int) = signal(SIGILL, barrera_senal);

  const int sig = sigsetjmp(barrera_salto, 1);
  if (sig == 0) {
    barrera_activa = 1;
    fn();
    barrera_activa = 0;
  }

  signal(SIGSEGV, previo_segv);
  signal(SIGBUS, previo_bus);
  signal(SIGFPE, previo_fpe);
  signal(SIGILL, previo_ill);
  if (sig != 0) {
    barrera_fallos++;
  }
  return sig;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ejecutar el dibujo de un tipo
 * \{ */

/** Serializa la raiz de layout recien creada y le pone nombre (`DRAW`, `DRAW_HEADER`...). */
static void serializar_raiz(uiLayout *layout,
                            bContext *C,
                            const char *nombre,
                            blender::Vector<std::string> &out)
{
  out.append(std::string(nombre));
  serializar_item(layout, C, 1, out);
}

static void bloque_ui_liberar(bContext *C, ARegion *region, uiBlock *block)
{
  block->panel = nullptr;
  UI_block_layout_free(block);
  BLI_remlink(&region->runtime->uiblocks, block);
  UI_block_free(C, block);
}

/** Dibuja un panel y devuelve sus lineas; `false` si el `poll` dice que no. */
static bool dibujar_panel(bContext *C,
                          PanelType *pt,
                          ARegion *region,
                          blender::Vector<std::string> &out)
{
  if (pt->poll && !pt->poll(C, pt)) {
    return false;
  }

  const uiStyle *style = UI_style_get_dpi();
  const int ancho = UI_UNIT_X * 20;

  uiBlock *block = UI_block_begin(C, region, pt->idname, blender::ui::EmbossType::Emboss);
  Panel *panel = BKE_panel_new(pt);
  block->panel = panel;

  if (pt->draw_header_preset && !(pt->flag & PANEL_TYPE_NO_HEADER)) {
    uiLayout *layout = UI_block_layout(
        block, UI_LAYOUT_HORIZONTAL, UI_LAYOUT_HEADER, 0, 0, UI_UNIT_Y, 1, 0, style);
    panel->layout = layout;
    pt->draw_header_preset(C, panel);
    panel->layout = nullptr;
    serializar_raiz(layout, C, "DRAW_HEADER_PRESET", out);
  }

  if (pt->draw_header && !(pt->flag & PANEL_TYPE_NO_HEADER)) {
    uiLayout *layout = UI_block_layout(
        block, UI_LAYOUT_HORIZONTAL, UI_LAYOUT_HEADER, 0, 0, UI_UNIT_Y, 1, 0, style);
    panel->layout = layout;
    pt->draw_header(C, panel);
    panel->layout = nullptr;
    serializar_raiz(layout, C, "DRAW_HEADER", out);
  }

  if (pt->draw) {
    uiLayout *layout = UI_block_layout(
        block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, 0, 0, ancho, 0, 0, style);
    panel->layout = layout;
    pt->draw(C, panel);
    panel->layout = nullptr;
    serializar_raiz(layout, C, "DRAW", out);
  }

  BKE_panel_free(panel);
  bloque_ui_liberar(C, region, block);
  return true;
}

static bool dibujar_cabecera(bContext *C,
                             HeaderType *ht,
                             ARegion *region,
                             blender::Vector<std::string> &out)
{
  if (ht->poll && !ht->poll(C, ht)) {
    return false;
  }
  if (ht->draw == nullptr) {
    return false;
  }

  const uiStyle *style = UI_style_get_dpi();
  uiBlock *block = UI_block_begin(C, region, ht->idname, blender::ui::EmbossType::Emboss);
  uiLayout *layout = UI_block_layout(
      block, UI_LAYOUT_HORIZONTAL, UI_LAYOUT_HEADER, 0, 0, UI_UNIT_Y, 1, 0, style);

  Header header = {nullptr};
  header.type = ht;
  header.layout = layout;
  ht->draw(C, &header);

  serializar_raiz(layout, C, "DRAW", out);
  bloque_ui_liberar(C, region, block);
  return true;
}

static bool dibujar_menu(bContext *C,
                         MenuType *mt,
                         ARegion *region,
                         blender::Vector<std::string> &out)
{
  if (!WM_menutype_poll(C, mt)) {
    return false;
  }
  if (mt->draw == nullptr) {
    return false;
  }

  const uiStyle *style = UI_style_get_dpi();
  uiBlock *block = UI_block_begin(C, region, mt->idname, blender::ui::EmbossType::Pulldown);
  uiLayout *layout = UI_block_layout(
      block, UI_LAYOUT_VERTICAL, UI_LAYOUT_MENU, 0, 0, UI_UNIT_X * 10, 0, 0, style);

  Menu menu = {nullptr};
  menu.type = mt;
  menu.layout = layout;
  mt->draw(C, &menu);

  serializar_raiz(layout, C, "DRAW", out);
  bloque_ui_liberar(C, region, block);
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name El recorrido del volcado de dibujo
 * \{ */

/** Deja el contexto apuntando a una region concreta. */
static void contexto_poner(bContext *C, wmWindow *win, ScrArea *area, ARegion *region)
{
  CTX_wm_window_set(C, win);
  CTX_wm_area_set(C, area);
  CTX_wm_region_set(C, region);
}

/**
 * El espacio al que pertenece un menu, deducido de su `idname`.
 *
 * Los menus viven en un registro global, sin espacio, pero la mitad de ellos da por
 * hecho el suyo: `NODE_MT_category_GEO_OUTPUT` dibujado con un area 3D delante no
 * falla el `poll` — se lleva el proceso por delante. La convencion de nombres de
 * Blender, `ESPACIO_MT_nombre`, dice a cual pertenece cada uno, y dibujarlo alli es
 * ademas mas fiel: es donde lo abre el usuario.
 *
 * Un prefijo que no sea de espacio (`OBJECT_MT_`, `WM_MT_`, `TOPBAR_MT_`...) devuelve
 * `SPACE_EMPTY`, y esos se dibujan con el contexto inicial del editor.
 */
static int menu_space_type(const char *idname)
{
  struct Prefijo {
    const char *prefijo;
    int space_type;
  };
  static const Prefijo prefijos[] = {
      {"CLIP_MT_", SPACE_CLIP},
      {"CONSOLE_MT_", SPACE_CONSOLE},
      {"DOPESHEET_MT_", SPACE_ACTION},
      {"FILEBROWSER_MT_", SPACE_FILE},
      {"GRAPH_MT_", SPACE_GRAPH},
      {"IMAGE_MT_", SPACE_IMAGE},
      {"INFO_MT_", SPACE_INFO},
      {"LOGIC_MT_", SPACE_LOGIC},
      {"NLA_MT_", SPACE_NLA},
      {"NODE_MT_", SPACE_NODE},
      {"OUTLINER_MT_", SPACE_OUTLINER},
      {"PROPERTIES_MT_", SPACE_PROPERTIES},
      {"SEQUENCER_MT_", SPACE_SEQ},
      {"SPREADSHEET_MT_", SPACE_SPREADSHEET},
      {"TEXT_MT_", SPACE_TEXT},
      {"USERPREF_MT_", SPACE_USERPREF},
      {"VIEW3D_MT_", SPACE_VIEW3D},
  };
  for (const Prefijo &p : prefijos) {
    if (strncmp(idname, p.prefijo, strlen(p.prefijo)) == 0) {
      return p.space_type;
    }
  }
  return SPACE_EMPTY;
}


/**
 * Si el panel, o alguno de sus padres, es instanciado.
 *
 * Un panel instanciado no tiene un panel por tipo sino uno por elemento de una lista
 * — un modificador, una restriccion, un sistema de particulas —, y su `draw()` lee el
 * dato por `panel->runtime->custom_data`, que pone la region al dibujar de verdad.
 * Dibujarlo en seco seria inventarse el dato.
 *
 * Y hay que mirar la ASCENDENCIA, no solo el propio panel: los sub-paneles de un
 * modificador no llevan la bandera, pero heredan el `custom_data` del padre y sin el
 * se llevan el proceso por delante. Los 71 `MOD_PT_*` que tumbaron la primera pasada
 * eran justo eso.
 */
static bool panel_instanciado_en_raiz(const PanelType *pt)
{
  for (const PanelType *p = pt; p != nullptr; p = p->parent) {
    if (p->flag & PANEL_TYPE_INSTANCED) {
      return true;
    }
  }
  return false;
}

/** Anota un tipo como no cubierto y lleva la cuenta. */
static void anotar_no_cubierto(Bloque &bloque, const char *motivo, int &no_cubiertos)
{
  bloque.lineas.clear();
  bloque.lineas.append(std::string("NO-CUBIERTO motivo=") + motivo);
  no_cubiertos++;
}

/** Dibuja un menu dentro de la red, o lo anota como no cubierto. */
static void menu_a_bloque(bContext *C,
                          MenuType *mt,
                          ARegion *region,
                          Bloque &bloque,
                          int &cubiertos,
                          int &no_cubiertos)
{
  const char *veto = no_dibujable_motivo(mt->idname);
  if (region == nullptr) {
    anotar_no_cubierto(bloque, "sin-region", no_cubiertos);
    return;
  }
  if (veto != nullptr) {
    anotar_no_cubierto(bloque, veto, no_cubiertos);
    return;
  }
  if (mt->draw == nullptr) {
    anotar_no_cubierto(bloque, "sin-draw", no_cubiertos);
    return;
  }

  printf("UI_LAYOUT MENU %s\n", mt->idname);
  fflush(stdout);

  bool dibujado = false;
  const int sig = con_barrera([&]() { dibujado = dibujar_menu(C, mt, region, bloque.lineas); });
  if (sig != 0) {
    fprintf(stderr, "fallo (senal %d) dibujando el menu %s\n", sig, mt->idname);
    anotar_no_cubierto(bloque, "fallo", no_cubiertos);
    return;
  }
  if (!dibujado) {
    anotar_no_cubierto(bloque, "poll", no_cubiertos);
    return;
  }
  cubiertos++;
}

static blender::Vector<Bloque> diseno_bloques(bContext *C, int *r_cubiertos, int *r_no_cubiertos)
{
  blender::Vector<Bloque> bloques;
  int cubiertos = 0;
  int no_cubiertos = 0;

  /* Los menus, agrupados por el espacio que dice su nombre. Los que no son de
   * ningun espacio se dibujan aparte, con el contexto inicial del editor. */
  blender::Vector<MenuType *> menus;
  for (MenuType *mt : WM_menutypes_registered_get()) {
    menus.append(mt);
  }
  std::sort(menus.begin(), menus.end(), [](const MenuType *a, const MenuType *b) {
    return strcmp(a->idname, b->idname) < 0;
  });

  auto bloque_de_menu = [](const MenuType *mt) {
    Bloque bloque;
    bloque.clave = std::string("MENU ") + mt->idname;
    bloque.orden = clave_orden(3, "", "", "", mt->idname);
    return bloque;
  };

  /* 1. Los menus sin espacio, con el editor tal cual arranca: antes de tocar
   *    ningun area, para que el resultado no dependa del recorrido. */
  wmWindow *win_menu = nullptr;
  ScrArea *area_menu = nullptr;
  ARegion *region_menu = nullptr;
  if (area_de_reserva(C, &win_menu, &area_menu)) {
    region_menu = region_de_tipo(area_menu, RGN_TYPE_WINDOW);
  }
  if (region_menu != nullptr) {
    contexto_poner(C, win_menu, area_menu, region_menu);
  }

  for (MenuType *mt : menus) {
    if (menu_space_type(mt->idname) != SPACE_EMPTY) {
      continue;
    }
    Bloque bloque = bloque_de_menu(mt);
    menu_a_bloque(C, mt, region_menu, bloque, cubiertos, no_cubiertos);
    bloques.append(std::move(bloque));
  }

  /* 2. Paneles, cabeceras y menus, espacio por espacio. Cambiar de espacio destruye
   *    las regiones del area de reserva, asi que hay que terminar con un espacio
   *    antes de pasar al siguiente. */
  blender::Vector<SpaceType *> espacios;
  for (const std::unique_ptr<SpaceType> &st : BKE_spacetypes_list()) {
    espacios.append(st.get());
  }
  std::sort(espacios.begin(), espacios.end(), [](const SpaceType *a, const SpaceType *b) {
    return strcmp(space_type_identifier(a->spaceid), space_type_identifier(b->spaceid)) < 0;
  });

  wmWindow *win_reserva = nullptr;
  ScrArea *area_reserva = nullptr;
  const bool hay_reserva = area_de_reserva(C, &win_reserva, &area_reserva);

  /* Primero los espacios que el fichero de fabrica YA tiene abiertos, y solo despues
   * los que hay que abrir reciclando el area de reserva. El motivo es concreto: el
   * area de reserva es la mas grande, o sea la vista 3D, y si se recicla antes de
   * haberla recorrido, sus 176 paneles y 191 menus se quedan sin cubrir — que es
   * exactamente lo que paso en la primera pasada. Dos vueltas, y el problema
   * desaparece sin depender del orden alfabetico. */
  {
    blender::Vector<SpaceType *> vivos;
    blender::Vector<SpaceType *> por_abrir;
    for (SpaceType *st : espacios) {
      wmWindow *w = nullptr;
      ScrArea *a = nullptr;
      (area_viva(C, st->spaceid, &w, &a) ? vivos : por_abrir).append(st);
    }
    espacios = vivos;
    espacios.extend(por_abrir);
  }

  for (SpaceType *st : espacios) {
    blender::Vector<MenuType *> menus_del_espacio;
    for (MenuType *mt : menus) {
      if (menu_space_type(mt->idname) == st->spaceid) {
        menus_del_espacio.append(mt);
      }
    }

    bool tiene_algo = !menus_del_espacio.is_empty();
    LISTBASE_FOREACH (ARegionType *, art, &st->regiontypes) {
      if (!BLI_listbase_is_empty(&art->paneltypes) ||
          !BLI_listbase_is_empty(&art->headertypes)) {
        tiene_algo = true;
        break;
      }
    }
    if (!tiene_algo) {
      continue;
    }

    wmWindow *win = nullptr;
    ScrArea *area = nullptr;
    if (!area_viva(C, st->spaceid, &win, &area)) {
      if (!hay_reserva || ELEM(st->spaceid, SPACE_EMPTY, SPACE_TOPBAR, SPACE_STATUSBAR)) {
        /* La barra superior y la de estado no son editores: no se pueden abrir en
         * un area normal. O estan vivas en la ventana o no se cubren. */
        win = nullptr;
        area = nullptr;
      }
      else {
        win = win_reserva;
        area = area_reserva;
        contexto_poner(C, win, area, nullptr);
        const int sig = con_barrera([&]() {
          ED_area_newspace(C, area, st->spaceid, false);
          ED_area_init(C, win, area);
        });
        if (sig != 0 || area->spacetype != st->spaceid) {
          fprintf(stderr, "no se pudo abrir el espacio %s\n", space_type_identifier(st->spaceid));
          area = nullptr;
        }
      }
    }

    LISTBASE_FOREACH (ARegionType *, art, &st->regiontypes) {
      ARegion *region = (area != nullptr) ? region_de_tipo(area, art->regionid) : nullptr;
      if (region != nullptr) {
        contexto_poner(C, win, area, region);
      }

      LISTBASE_FOREACH (PanelType *, pt, &art->paneltypes) {
        Bloque bloque = panel_bloque(pt);
        bloque.lineas.clear();
        const char *veto = no_dibujable_motivo(pt->idname);
        if (panel_instanciado_en_raiz(pt)) {
          anotar_no_cubierto(bloque, "instanciado", no_cubiertos);
        }
        else if (region == nullptr) {
          anotar_no_cubierto(bloque, "sin-region", no_cubiertos);
        }
        else if (veto != nullptr) {
          anotar_no_cubierto(bloque, veto, no_cubiertos);
        }
        else if (pt->draw == nullptr) {
          anotar_no_cubierto(bloque, "sin-draw", no_cubiertos);
        }
        else {
          if (st->spaceid == SPACE_PROPERTIES) {
            pestana_poner(C, area, pt->context);
          }
          printf("UI_LAYOUT PANEL %s\n", pt->idname);
          fflush(stdout);
          bool dibujado = false;
          const int sig = con_barrera(
              [&]() { dibujado = dibujar_panel(C, pt, region, bloque.lineas); });
          if (sig != 0) {
            fprintf(stderr, "fallo (senal %d) dibujando el panel %s\n", sig, pt->idname);
            anotar_no_cubierto(bloque, "fallo", no_cubiertos);
          }
          else if (!dibujado) {
            anotar_no_cubierto(bloque, "poll", no_cubiertos);
          }
          else {
            cubiertos++;
          }
        }
        bloques.append(std::move(bloque));
      }

      LISTBASE_FOREACH (HeaderType *, ht, &art->headertypes) {
        Bloque bloque = header_bloque(ht);
        bloque.lineas.clear();
        const char *veto = no_dibujable_motivo(ht->idname);
        if (region == nullptr) {
          anotar_no_cubierto(bloque, "sin-region", no_cubiertos);
        }
        else if (veto != nullptr) {
          anotar_no_cubierto(bloque, veto, no_cubiertos);
        }
        else if (ht->draw == nullptr) {
          anotar_no_cubierto(bloque, "sin-draw", no_cubiertos);
        }
        else {
          printf("UI_LAYOUT HEADER %s\n", ht->idname);
          fflush(stdout);
          bool dibujado = false;
          const int sig = con_barrera(
              [&]() { dibujado = dibujar_cabecera(C, ht, region, bloque.lineas); });
          if (sig != 0) {
            fprintf(stderr, "fallo (senal %d) dibujando la cabecera %s\n", sig, ht->idname);
            anotar_no_cubierto(bloque, "fallo", no_cubiertos);
          }
          else if (!dibujado) {
            anotar_no_cubierto(bloque, "poll", no_cubiertos);
          }
          else {
            cubiertos++;
          }
        }
        bloques.append(std::move(bloque));
      }
    }

    /* Los menus del espacio, con su region principal delante. */
    ARegion *region_espacio = (area != nullptr) ? region_de_tipo(area, RGN_TYPE_WINDOW) : nullptr;
    if (region_espacio != nullptr) {
      contexto_poner(C, win, area, region_espacio);
    }
    for (MenuType *mt : menus_del_espacio) {
      Bloque bloque = bloque_de_menu(mt);
      menu_a_bloque(C, mt, region_espacio, bloque, cubiertos, no_cubiertos);
      bloques.append(std::move(bloque));
    }
  }

  bloques_ordenar(bloques);
  *r_cubiertos = cubiertos;
  *r_no_cubiertos = no_cubiertos;
  return bloques;
}

/**
 * Sale del proceso sin pasar por `WM_exit` si la barrera tuvo que actuar.
 *
 * Volver de un SIGSEGV deja el monton en un estado que nadie puede dar por bueno, y
 * el cierre ordenado de Blender lo recorre entero liberando: se caeria al final, con
 * el fichero ya escrito y un codigo de salida que mentiria sobre el resultado. El
 * trabajo esta hecho y anotado; lo unico honesto es salir aqui.
 */
static void salir_si_hubo_fallos()
{
  if (barrera_fallos == 0) {
    return;
  }
  printf(
      "UI_LAYOUT_BARRERA %d tipos se llevaron el proceso por delante y estan anotados como "
      "NO-CUBIERTO motivo=fallo. Se sale sin el cierre ordenado: tras un SIGSEGV la memoria "
      "no es de fiar.\n",
      barrera_fallos);
  fflush(nullptr);
  _exit(EXIT_SUCCESS);
}


bool dump_layout(bContext *C, const char *filepath)
{
  if (CTX_wm_manager(C) == nullptr ||
      BLI_listbase_is_empty(&CTX_wm_manager(C)->windows))
  {
    fprintf(stderr,
            "--fl-dump-ui-layout necesita modo grafico: sin ventana no hay region ni "
            "contexto donde ejecutar los draw().\n");
    return false;
  }

  int cubiertos = 0, no_cubiertos = 0;
  const blender::Vector<Bloque> bloques = diseno_bloques(C, &cubiertos, &no_cubiertos);
  if (!bloques_escribir(bloques, MARCA_DISENO, filepath)) {
    return false;
  }
  printf("UI_LAYOUT_DUMP_OK bloques=%d dibujados=%d no_cubiertos=%d\n",
         int(bloques.size()),
         cubiertos,
         no_cubiertos);
  return true;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name La comprobacion
 * \{ */

struct BaseLinea {
  blender::Map<std::string, blender::Vector<std::string>> bloques;
  blender::Vector<std::string> orden;
  std::string marca;
};

static bool baseline_leer(const char *filepath, BaseLinea *out)
{
  size_t size = 0;
  char *text = static_cast<char *>(BLI_file_read_text_as_mem(filepath, 0, &size));
  if (text == nullptr) {
    fprintf(stderr, "No se pudo leer la linea base '%s'.\n", filepath);
    return false;
  }

  const std::string todo(text, size);
  MEM_freeN(text);

  std::string actual;
  size_t start = 0;
  bool primera = true;
  while (start <= todo.size()) {
    const size_t end = todo.find('\n', start);
    const std::string linea = todo.substr(start,
                                          (end == std::string::npos ? todo.size() : end) - start);
    if (primera) {
      out->marca = linea;
      primera = false;
    }
    else if (linea.rfind("=== ", 0) == 0) {
      actual = linea.substr(4);
      out->orden.append(actual);
      out->bloques.add_overwrite(actual, {});
    }
    else if (!actual.empty() && !linea.empty()) {
      out->bloques.lookup(actual).append(linea);
    }
    if (end == std::string::npos) {
      break;
    }
    start = end + 1;
  }
  return true;
}


/* -------------------------------------------------------------------- */
/** \name Verificacion del panel de presets nativo
 *
 * `--fl-dump-preset-panel <fichero>` dibuja con `flipendo::preset::ui::draw_panel()`
 * la familia `node_color` —la misma que `NODE_PT_node_color_presets` del Python— y
 * serializa el resultado con el MISMO serializador que usa `--fl-dump-ui-layout`.
 * Asi los dos arboles se pueden comparar texto contra texto.
 *
 * Existe porque la trampa de los presets es fina: en una instalacion de fabrica la
 * carpeta no existe y el Python solo pinta `* Missing Paths *`. Reproducir eso no
 * demuestra nada. Con esta opcion se llena la carpeta de verdad, se vuelca lo que
 * dibuja el Python y lo que dibuja el C++, y se comparan con ficheros dentro.
 * \{ */

bool dump_preset_panel(bContext *C, const char *filepath)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  if (wm == nullptr || BLI_listbase_is_empty(&wm->windows)) {
    fprintf(stderr, "--fl-dump-preset-panel necesita modo grafico.\n");
    return false;
  }

  wmWindow *win = nullptr;
  ScrArea *area = nullptr;
  if (!area_de_reserva(C, &win, &area)) {
    fprintf(stderr, "No hay area donde dibujar.\n");
    return false;
  }
  ARegion *region = region_de_tipo(area, RGN_TYPE_WINDOW);
  if (region == nullptr) {
    fprintf(stderr, "No hay region donde dibujar.\n");
    return false;
  }
  contexto_poner(C, win, area, region);

  /* La misma familia y los mismos operadores que declara `NODE_PT_node_color_presets`
   * en `space_node.py`. */
  flipendo::preset::ui::MenuSpec spec;
  spec.subdir = "node_color";
  spec.op = "SCRIPT_OT_execute_preset";
  spec.menu_idname = "NODE_PT_node_color_presets";
  spec.add_op = "NODE_OT_node_color_preset_add";

  const uiStyle *style = UI_style_get_dpi();
  uiBlock *block = UI_block_begin(
      C, region, "FL_preset_panel", blender::ui::EmbossType::Emboss);
  uiLayout *layout = UI_block_layout(
      block, UI_LAYOUT_VERTICAL, UI_LAYOUT_PANEL, 0, 0, UI_UNIT_X * 20, 0, 0, style);

  flipendo::preset::ui::draw_panel(C, layout, spec);

  blender::Vector<std::string> lineas;
  serializar_raiz(layout, C, "DRAW", lineas);
  bloque_ui_liberar(C, region, block);

  FILE *fp = BLI_fopen(filepath, "w");
  if (fp == nullptr) {
    fprintf(stderr, "No se pudo abrir '%s' para escribir.\n", filepath);
    return false;
  }
  for (const std::string &linea : lineas) {
    fprintf(fp, "%s\n", linea.c_str());
  }
  fclose(fp);
  printf("PRESET_PANEL_DUMP_OK lineas=%d\n", int(lineas.size()));
  return true;
}

/** \} */

bool check(bContext *C, const char *baseline_filepath)
{
  BaseLinea base;
  if (!baseline_leer(baseline_filepath, &base)) {
    return false;
  }

  blender::Vector<Bloque> ahora;
  const char *que = nullptr;
  if (base.marca == MARCA_REGISTRO) {
    que = "registro";
    ahora = registro_bloques();
  }
  else if (base.marca == MARCA_DISENO) {
    que = "diseno";
    int cubiertos = 0, no_cubiertos = 0;
    if (CTX_wm_manager(C) == nullptr ||
        BLI_listbase_is_empty(&CTX_wm_manager(C)->windows))
    {
      fprintf(stderr,
              "Esta linea base es de diseno: hace falta modo grafico para comprobarla.\n");
      return false;
    }
    ahora = diseno_bloques(C, &cubiertos, &no_cubiertos);
    if (barrera_fallos != 0) {
      printf("Aviso: la barrera actuo %d veces; esos tipos van como NO-CUBIERTO.\n",
             barrera_fallos);
    }
  }
  else {
    fprintf(stderr,
            "'%s' no parece una linea base de interfaz: la primera linea deberia ser "
            "'%s' o '%s'.\n",
            baseline_filepath,
            MARCA_REGISTRO,
            MARCA_DISENO);
    return false;
  }

  int identicos = 0;
  int distintos = 0;
  int sobran = 0;
  blender::Vector<std::string> vistos;

  for (const Bloque &bloque : ahora) {
    const blender::Vector<std::string> *esperado = base.bloques.lookup_ptr(bloque.clave);
    if (esperado == nullptr) {
      printf("SOBRA   %s (no esta en la linea base)\n", bloque.clave.c_str());
      sobran++;
      continue;
    }
    vistos.append(bloque.clave);

    bool igual = esperado->size() == bloque.lineas.size();
    if (igual) {
      for (const int64_t i : bloque.lineas.index_range()) {
        if ((*esperado)[i] != bloque.lineas[i]) {
          igual = false;
          break;
        }
      }
    }
    if (igual) {
      identicos++;
      continue;
    }

    distintos++;
    printf("DIFIERE %s\n", bloque.clave.c_str());
    const int64_t n = std::max(esperado->size(), bloque.lineas.size());
    for (const int64_t i : blender::IndexRange(n)) {
      const std::string *a = i < esperado->size() ? &(*esperado)[i] : nullptr;
      const std::string *b = i < bloque.lineas.size() ? &bloque.lineas[i] : nullptr;
      if (a != nullptr && b != nullptr && *a == *b) {
        continue;
      }
      printf("  linea %d\n    base:  %s\n    ahora: %s\n",
             int(i + 1),
             a != nullptr ? a->c_str() : "(no hay)",
             b != nullptr ? b->c_str() : "(no hay)");
    }
  }

  int faltan = 0;
  for (const std::string &clave : base.orden) {
    if (!vistos.contains(clave)) {
      printf("FALTA   %s (esta en la linea base y ya no se registra)\n", clave.c_str());
      faltan++;
    }
  }

  printf("\nInterfaz (%s): %d bloques, %d identicos, %d distintos, %d faltan, %d sobran.\n",
         que,
         int(ahora.size()),
         identicos,
         distintos,
         faltan,
         sobran);

  const bool ok = (distintos == 0) && (faltan == 0) && (sobran == 0);
  if (barrera_fallos != 0) {
    /* Igual que en el volcado: tras un SIGSEGV el cierre ordenado no es de fiar, y
     * el parte ya esta impreso. */
    fflush(nullptr);
    _exit(ok ? EXIT_SUCCESS : EXIT_FAILURE);
  }
  return ok;
}

/** \} */

}  // namespace flipendo::ui_dump
