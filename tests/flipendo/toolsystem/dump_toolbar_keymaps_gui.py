# Linea base del KEYMAP DEL POPUP DE LA BARRA (`wm.toolbar`).
#
# Llama al `bl_keymap_utils.keymap_from_toolbar.generate` REAL para cada espacio y modo, y
# vuelca cada atajo del keymap que fabrica. Tiene que ser en modo grafico: parte del
# keymap del usuario y busca en el las teclas que ya activan cada herramienta.
#
# Uso:  Blender --factory-startup --python dump_toolbar_keymaps_gui.py -- <salida.txt>
#
# YA NO SE PUEDE EJECUTAR, y es lo que tenia que pasar: `keymap_from_toolbar.py` se
# retiro una vez verificado el nativo (912/912 atajos). El fichero se conserva porque es
# la receta con la que se fabrico `toolbar-keymaps-python.txt`, que sigue siendo la linea
# base contra la que compara `--fl-dump-toolbar-keymaps`. Un oraculo se guarda aunque su
# sujeto muera: sin el, la linea base seria un numero sin procedencia.
import sys
import traceback

import bpy
from bl_ui.space_toolsystem_common import ToolSelectPanelHelper
from bl_keymap_utils import keymap_from_toolbar

out_path = sys.argv[sys.argv.index("--") + 1]
SIN_MODOS = "\x00sin-modo"
lineas = []


def prop_str(props, name):
    if props is None or name not in props.bl_rna.properties:
        return "-"
    return str(getattr(props, name))


try:
    for cls in sorted(ToolSelectPanelHelper.__subclasses__(), key=lambda c: c.bl_space_type):
        space = cls.bl_space_type
        modos = sorted(k for k in cls._tools.keys() if k is not None) or [SIN_MODOS]
        tfc = cls.tools_from_context.__func__
        original = cls.__dict__.get("tools_from_context")
        for mode in modos:
            etiqueta = "None" if mode is SIN_MODOS else mode
            cls.tools_from_context = classmethod(
                lambda c, context, mode_=None, _m=mode, _f=tfc: _f(c, context, _m))
            km = keymap_from_toolbar.generate(bpy.context, space)
            if km is None:
                lineas.append("K %s %s (sin keymap)" % (space, etiqueta))
                continue
            for i, kmi in enumerate(km.keymap_items):
                props = kmi.properties
                lineas.append(
                    "K %s %s %02d %s type=%s value=%s any=%d shift=%d ctrl=%d alt=%d oskey=%d "
                    "hyper=%d key_modifier=%s active=%d repeat=%d name=%s skip_depressed=%s" % (
                        space, etiqueta, i, kmi.idname, kmi.type, kmi.value, kmi.any, kmi.shift,
                        kmi.ctrl, kmi.alt, kmi.oskey, kmi.hyper, kmi.key_modifier, kmi.active,
                        kmi.repeat, prop_str(props, "name"), prop_str(props, "skip_depressed")))
        if original is not None:
            cls.tools_from_context = original
except Exception:
    lineas.append("EXCEPCION\n" + traceback.format_exc())
finally:
    with open(out_path, "w") as f:
        f.write("\n".join(lineas) + "\n")
    print("TOOLBAR_KEYMAPS_DUMP_OK", len(lineas))
    bpy.ops.wm.quit_blender()
