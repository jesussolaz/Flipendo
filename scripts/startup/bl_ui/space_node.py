# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

# La interfaz del editor de nodos esta en C++:
# `source/blender/editors/space_node/fl_node_ui.cc` (cabecera, 8 menus y 17 paneles).
#
# Aqui solo quedan los tipos que NO son de este editor: `NODE_PT_annotation`, que
# hereda el mixin de anotaciones compartido con la vista 3D, el editor de clips, el
# de imagen y el de secuencias, y los seis paneles que `node_panel()` clona de las
# pestanas de Propiedades. La decision del proyecto es funcion compartida, nunca
# copia: se escribiran en C++ cuando sus originales lo esten y expongan su `draw()`
# en una cabecera publica, como ya hizo `WORLD_PT_viewport_display` (ese clon ya es
# C++ y por eso no aparece mas abajo).
#
# Ver `politicas/UI-A-CPP.md`.

from bpy.types import Panel

from bl_ui.properties_grease_pencil_common import (
    AnnotationDataPanel,
)
from bl_ui.properties_material import (
    EEVEE_NEXT_MATERIAL_PT_settings,
    EEVEE_NEXT_MATERIAL_PT_settings_surface,
    EEVEE_NEXT_MATERIAL_PT_settings_volume,
    MATERIAL_PT_viewport,
)
from bl_ui.properties_data_light import (
    DATA_PT_light,
    DATA_PT_EEVEE_light,
)


# Grease Pencil properties
class NODE_PT_annotation(AnnotationDataPanel, Panel):
    bl_space_type = 'NODE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "View"
    bl_options = {'DEFAULT_CLOSED'}

    # NOTE: this is just a wrapper around the generic GP Panel

    @classmethod
    def poll(cls, context):
        snode = context.space_data
        return snode is not None and snode.node_tree is not None


# Adapt properties editor panel to display in node editor. We have to
# copy the class rather than inherit due to the way bpy registration works.
def node_panel(cls):
    node_cls_dict = cls.__dict__.copy()

    # Needed for re-registration.
    node_cls_dict.pop("bl_rna", None)

    node_cls = type('NODE_' + cls.__name__, cls.__bases__, node_cls_dict)

    node_cls.bl_space_type = 'NODE_EDITOR'
    node_cls.bl_region_type = 'UI'
    node_cls.bl_category = "Options"
    if hasattr(node_cls, "bl_parent_id"):
        node_cls.bl_parent_id = "NODE_" + node_cls.bl_parent_id

    return node_cls


classes = (
    NODE_PT_annotation,

    node_panel(EEVEE_NEXT_MATERIAL_PT_settings),
    node_panel(EEVEE_NEXT_MATERIAL_PT_settings_surface),
    node_panel(EEVEE_NEXT_MATERIAL_PT_settings_volume),
    node_panel(MATERIAL_PT_viewport),
    node_panel(DATA_PT_light),
    node_panel(DATA_PT_EEVEE_light),
)


if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
