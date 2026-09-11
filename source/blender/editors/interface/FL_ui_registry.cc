/* SPDX-FileCopyrightText: 2026 Flipendo
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Implementacion del registro declarativo de UI nativa. Ver FL_ui_registry.hh.
 *
 * Todo lo que hace aqui existia ya, pero repartido: la parte mecanica en cada
 * fichero de editor que registra paneles a mano, y la parte fina (orden,
 * categoria de reserva, jerarquia padre/hijo) solo en el registro de Python
 * (`makesrna/intern/rna_ui.cc`). Aqui esta en un unico sitio y sin Python.
 */

#include "FL_ui_registry.hh"

#include "BLI_listbase.h"
#include "BLI_string.h"

#include "BLT_translation.hh"

#include "BKE_screen.hh"

#include "MEM_guardedalloc.h"

#include "UI_resources.hh"

#include "WM_api.hh"

#include "../asset/ED_asset_shelf.hh"

#include <memory>

namespace flipendo {

/* La categoria vacia haria que el panel saliera en TODAS las pestanas, asi que
 * las regiones con pestanas exigen una. Mismo criterio que rna_ui.cc:280-286. */
static void panel_category_apply(PanelType *pt, const PanelDecl &decl)
{
  if (decl.category && decl.category[0] != '\0') {
    STRNCPY(pt->category, decl.category);
    return;
  }
  if ((1 << pt->region_type) & RGN_TYPE_HAS_CATEGORY_MASK) {
    STRNCPY(pt->category, PNL_CATEGORY_FALLBACK);
  }
}

/* Los paneles sin cabecera van delante; entre iguales, manda `order`.
 * Portado de rna_ui.cc:396-408. */
static void panel_insert_ordered(ARegionType *art, PanelType *pt)
{
  PanelType *pt_iter = static_cast<PanelType *>(art->paneltypes.last);
  for (; pt_iter; pt_iter = pt_iter->prev) {
    if ((pt->flag & PANEL_TYPE_NO_HEADER) && !(pt_iter->flag & PANEL_TYPE_NO_HEADER)) {
      continue;
    }
    if (pt_iter->order <= pt->order) {
      break;
    }
  }
  BLI_insertlinkafter(&art->paneltypes, pt_iter, pt);
}

/* Enlaza el sub-panel con su padre en la posicion que le toca por `order`.
 * Portado de rna_ui.cc:412-423. */
static void panel_link_to_parent(PanelType *parent, PanelType *pt)
{
  pt->parent = parent;
  LinkData *child_iter = static_cast<LinkData *>(parent->children.last);
  for (; child_iter; child_iter = child_iter->prev) {
    const PanelType *child = static_cast<PanelType *>(child_iter->data);
    if (child->order <= pt->order) {
      break;
    }
  }
  BLI_insertlinkafter(&parent->children, child_iter, BLI_genericNodeN(pt));
}

void panels_register(ARegionType *art, const int space_type, blender::Span<const PanelDecl> decls)
{
  BLI_assert(art != nullptr);

  for (const PanelDecl &decl : decls) {
    BLI_assert(decl.idname != nullptr);

    PanelType *pt = MEM_callocN<PanelType>(__func__);

    STRNCPY(pt->idname, decl.idname);
    if (decl.label) {
      STRNCPY(pt->label, decl.label);
    }
    STRNCPY(pt->translation_context,
            decl.translation_context ? decl.translation_context : BLT_I18NCONTEXT_DEFAULT_BPYRNA);
    if (decl.context) {
      STRNCPY(pt->context, decl.context);
    }
    /* `description` es `const char *` en PanelType: se guarda el puntero, que en
     * una tabla estatica vive tanto como el programa. */
    pt->description = decl.description;

    pt->space_type = space_type;
    pt->region_type = art->regionid;
    pt->flag = decl.flag;
    pt->order = decl.order;
    pt->ui_units_x = decl.ui_units_x;

    pt->draw = decl.draw;
    pt->draw_header = decl.draw_header;
    pt->draw_header_preset = decl.draw_header_preset;
    pt->poll = decl.poll;

    panel_category_apply(pt, decl);

    PanelType *parent = nullptr;
    if (decl.parent_id && decl.parent_id[0] != '\0') {
      STRNCPY(pt->parent_id, decl.parent_id);
      LISTBASE_FOREACH (PanelType *, pt_iter, &art->paneltypes) {
        if (STREQ(pt_iter->idname, decl.parent_id)) {
          parent = pt_iter;
          break;
        }
      }
      /* Un padre que no existe deja el sub-panel huerfano y sin dibujar; es un
       * error del programador, no del usuario. */
      BLI_assert_msg(parent != nullptr, "FL_ui_registry: parent_id no encontrado en la region");
    }

    panel_insert_ordered(art, pt);

    if (parent) {
      panel_link_to_parent(parent, pt);
    }

    /* Igual que el registro de Python (rna_ui.cc:432): TODO panel entra tambien en
     * el registro global. No es solo cosa de popovers — de ahi lo saca la busqueda
     * de paneles y `uiItemPopoverPanel`. El codigo nativo escrito a mano suele
     * olvidarlo, y por eso sus paneles no aparecen en la busqueda. */
    WM_paneltype_add(pt);
  }
}

void menus_register(blender::Span<const MenuDecl> decls)
{
  for (const MenuDecl &decl : decls) {
    BLI_assert(decl.idname != nullptr);

    MenuType *mt = MEM_callocN<MenuType>(__func__);

    STRNCPY(mt->idname, decl.idname);
    if (decl.label) {
      STRNCPY(mt->label, decl.label);
    }
    STRNCPY(mt->translation_context,
            decl.translation_context ? decl.translation_context : BLT_I18NCONTEXT_DEFAULT_BPYRNA);
    mt->description = decl.description;
    mt->draw = decl.draw;
    mt->poll = decl.poll;
    mt->flag = MenuTypeFlag(decl.flag);

    WM_menutype_add(mt);
  }
}

void headers_register(ARegionType *art,
                      const int space_type,
                      const int region_type,
                      blender::Span<const HeaderDecl> decls)
{
  BLI_assert(art != nullptr);

  for (const HeaderDecl &decl : decls) {
    BLI_assert(decl.idname != nullptr);

    HeaderType *ht = MEM_callocN<HeaderType>(__func__);

    STRNCPY(ht->idname, decl.idname);
    ht->space_type = space_type;
    ht->region_type = region_type;
    ht->draw = decl.draw;
    ht->poll = decl.poll;

    BLI_addtail(&art->headertypes, ht);
  }
}

void uilists_register(blender::Span<const UIListDecl> decls)
{
  for (const UIListDecl &decl : decls) {
    BLI_assert(decl.idname != nullptr);

    uiListType *ult = MEM_callocN<uiListType>(__func__);
    STRNCPY(ult->idname, decl.idname);
    ult->draw_item = decl.draw_item;
    ult->draw_filter = decl.draw_filter;
    ult->filter_items = decl.filter_items;
    ult->listener = decl.listener;

    WM_uilisttype_add(ult);
  }
}

void asset_shelves_register(blender::Span<const AssetShelfDecl> decls)
{
  for (const AssetShelfDecl &decl : decls) {
    BLI_assert(decl.idname != nullptr);

    /* El registro de estanterias posee el tipo, asi que se le entrega uno propio. */
    std::unique_ptr<AssetShelfType> type = std::make_unique<AssetShelfType>();
    STRNCPY(type->idname, decl.idname);
    type->space_type = decl.space_type;
    if (decl.activate_operator != nullptr) {
      type->activate_operator = decl.activate_operator;
    }
    type->flag = AssetShelfTypeFlag(decl.flag);
    type->default_preview_size = decl.default_preview_size;
    type->poll = decl.poll;
    type->asset_poll = decl.asset_poll;
    type->draw_context_menu = decl.draw_context_menu;
    type->get_active_asset = decl.get_active_asset;

    blender::ed::asset::shelf::type_register(std::move(type));
  }
}

}  // namespace flipendo
