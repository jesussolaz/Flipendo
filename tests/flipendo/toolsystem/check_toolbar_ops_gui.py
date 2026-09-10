# Los tres operadores de la barra, ya nativos: `wm.toolbar`, `wm.toolbar_fallback_pie` y
# `wm.toolbar_prompt`. Se invocan por el camino real, con el area de la vista 3D, y se
# comprueba lo que se puede observar desde fuera: que responden como el Python y que el
# keymap temporal del popup existe y tiene atajos.
#
# Uso:  Blender --factory-startup --python check_toolbar_ops_gui.py -- <salida.txt>
import sys
import traceback

import bpy

out_path = sys.argv[sys.argv.index("--") + 1]
res = []


def check(name, cond, detail=""):
    res.append(("OK     " if cond else "FALLO  ") + name + ("  [%s]" % (detail,) if detail != "" else ""))


try:
    win = bpy.context.window
    area = next(a for a in win.screen.areas if a.type == 'VIEW_3D')
    region = next(r for r in area.regions if r.type == 'WINDOW')
    with bpy.context.temp_override(window=win, area=area, region=region):
        r = bpy.ops.wm.toolbar()
        check("wm.toolbar", r == {'FINISHED'}, r)
        km = bpy.context.window_manager.keyconfigs.active.keymaps.get("Toolbar Popup <temp>")
        check("keymap temporal del popup con atajos", km is not None and len(km.keymap_items) > 0,
              len(km.keymap_items) if km else None)
        r = bpy.ops.wm.toolbar_fallback_pie('INVOKE_DEFAULT')
        check("wm.toolbar_fallback_pie", r == {'FINISHED'}, r)
        r = bpy.ops.wm.toolbar_prompt('INVOKE_DEFAULT')
        check("wm.toolbar_prompt entra en modal", r == {'RUNNING_MODAL'}, r)
    # Sin espacio en el contexto, los tres se niegan (poll o CANCELLED), como el Python.
    with bpy.context.temp_override(window=win, area=None, region=None):
        check("wm.toolbar sin espacio no pasa el poll", not bpy.ops.wm.toolbar.poll())
except Exception:
    res.append("EXCEPCION\n" + traceback.format_exc())
finally:
    with open(out_path, "w") as f:
        f.write("\n".join(res) + "\n")
    bpy.ops.wm.quit_blender()
