# Linea base de las CONSULTAS del sistema de herramientas.
#
# Son las que hoy C++ le hace a Python evaluando cadenas de codigo, para el tooltip y el
# menu contextual: la etiqueta, la descripcion y el grupo de cada herramienta. Se sacan
# llamando a las funciones REALES de `space_toolsystem_common.py`, modo a modo.
#
# La delicada es la descripcion: casi ninguna herramienta tiene texto propio, sale del
# PRIMER atajo activo de su keymap. Un matiz mal replicado degradaria cientos de
# tooltips sin que nada avisara. Tiene que generarse en modo grafico: los keymaps de
# usuario, que es donde se buscan esos atajos, solo existen alli.
#
# Uso:  Blender --factory-startup --python dump_queries_gui.py -- <salida.txt>
import sys

import bpy
from bl_ui.space_toolsystem_common import (
    ToolSelectPanelHelper,
    description_from_id,
    item_group_from_id,
)

out_path = sys.argv[sys.argv.index("--") + 1]
SIN_MODOS = "\x00sin-modo"

lineas = []
for cls in sorted(ToolSelectPanelHelper.__subclasses__(), key=lambda c: c.bl_space_type):
    space = cls.bl_space_type
    modos = sorted(k for k in cls._tools.keys() if k is not None) or [SIN_MODOS]
    tfc = cls.tools_from_context.__func__
    original = cls.__dict__.get("tools_from_context")
    for mode in modos:
        etiqueta = "None" if mode is SIN_MODOS else mode
        cls.tools_from_context = classmethod(
            lambda c, context, mode_=None, _m=mode, _f=tfc: _f(c, context, _m))
        cls._tool_group_active = {}
        for item in ToolSelectPanelHelper._tools_flatten(cls.tools_from_context(bpy.context)):
            if item is None:
                continue
            grupo = item_group_from_id(bpy.context, space, item.idname, coerce=True)
            ids = ",".join(i.idname for i in grupo if i is not None) if grupo else "-"
            desc = description_from_id(bpy.context, space, item.idname)
            lineas.append("Q %s %s %s label=%s group=%s desc=%r" % (
                space, etiqueta, item.idname, item.label, ids, desc))
    if original is not None:
        cls.tools_from_context = original

with open(out_path, "w") as f:
    f.write("\n".join(lineas) + "\n")
print("QUERIES_DUMP_OK", len(lineas))
bpy.ops.wm.quit_blender()
