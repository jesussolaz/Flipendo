# Vuelca el arbol del editor de keymaps (`bl_keymap_utils.keymap_hierarchy.generate`),
# que es el ultimo consumidor del armazon de herramientas fuera de `bl_ui`.
#
# Sirve para lo mismo que las demas lineas base del subsistema: se congela con el Python
# vivo y el sustituto nativo tiene que reproducirla byte a byte. Aqui la pieza que cambia
# es `_km_expand_from_toolsystem`, que antes recorria el catalogo Python
# (`ToolSelectPanelHelper.keymap_ui_hierarchy`) y ahora pregunta al catalogo nativo por
# `WindowManager.tool_keymap_names`.
#
# Corre en `--background`: el arbol no necesita keymaps cargados, solo el catalogo.
#
# Uso:  Blender --background --factory-startup --python dump_keymap_hierarchy.py -- <salida>
import sys

import bpy
from bl_keymap_utils import keymap_hierarchy

salida = sys.argv[sys.argv.index("--") + 1]
lineas = []


def recorrer(items, nivel):
    for nombre, space_type, region_type, hijos in items:
        lineas.append("%s%s | %s | %s" % ("  " * nivel, nombre, space_type, region_type))
        recorrer(hijos, nivel + 1)


recorrer(keymap_hierarchy.generate(), 0)
with open(salida, "w") as f:
    f.write("\n".join(lineas) + "\n")
print("KEYMAP_HIERARCHY_DONE", len(lineas))
