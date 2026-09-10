# Al abrir un .blend, la herramienta guardada se reactiva por el camino nativo.
#
# Antes esto pasaba por un operador de Python, y en el primer instante del arranque
# `WM_toolsystem_ref_set_by_id_ex` devolvia nulo porque el operador aun no existia. El
# nombre queda en DNA, pero el RUNTIME (keymap, gizmo, cursor) solo lo rellena una
# activacion. Por eso lo que se comprueba es el runtime: `widget` no vacio prueba que la
# activacion ha corrido de verdad al cargar.
#
# Uso:  Blender fichero.blend --python check_reopen_gui.py -- <salida.txt> [idname] [gizmo]
import sys

import bpy

argv = sys.argv[sys.argv.index("--") + 1:]
out_path = argv[0]
esperado_id = argv[1] if len(argv) > 1 else None
esperado_gizmo = argv[2] if len(argv) > 2 else None

# Las herramientas se reactivan en el primer refresco de cada area, no al leer el
# fichero: se dibuja una vez antes de mirar.
win = bpy.context.window
area = next(a for a in win.screen.areas if a.type == 'VIEW_3D')
region = next(r for r in area.regions if r.type == 'WINDOW')
with bpy.context.temp_override(window=win, area=area, region=region):
    bpy.ops.wm.redraw_timer(type='DRAW_WIN_SWAP', iterations=1)

res = ["fichero: " + bpy.data.filepath]
t = bpy.context.workspace.tools.from_space_view3d_mode(bpy.context.mode, create=False)
res.append("modo: %s  herramienta: %s  gizmo: %r  reserva: %s  indice: %s" % (
    bpy.context.mode, t.idname if t else None, t.widget if t else None,
    t.idname_fallback if t else None, t.index if t else None))
if esperado_id is not None:
    res.append(("OK     " if t and t.idname == esperado_id else "FALLO  ") + "herramienta restaurada = " + esperado_id)
if esperado_gizmo is not None:
    res.append(("OK     " if t and t.widget == esperado_gizmo else "FALLO  ") + "runtime reactivado (gizmo = %s)" % esperado_gizmo)
for ws in bpy.data.workspaces:
    for tr in ws.tools:
        res.append("  espacio de trabajo %-16s %-16s modo %-22s %s" % (ws.name, tr.space_type, tr.mode, tr.idname))

with open(out_path, "w") as f:
    f.write("\n".join(res) + "\n")
print("REOPEN_DONE")
bpy.ops.wm.quit_blender()
