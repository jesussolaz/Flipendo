# Prueba EN EJECUCION de la activacion nativa de herramientas.
#
# El volcado (`--fl-dump-tool-activation`) solo prueba el calculo puro. Esto prueba el
# resto, en modo grafico y por los caminos reales: los operadores C++, la aplicacion
# sobre el `bToolRef`, el ciclo con la memoria de grupos, la reserva y el tipo de
# pincel. Lo que se comprueba se lee por RNA, igual que lo leeria cualquier otro codigo.
#
# Uso:
#   Blender --factory-startup --python smoke_tools_gui.py -- \
#       <salida.txt> <activation-python.txt> <fichero-temporal.blend>
import re
import sys

import bpy

out_path, baseline_path, blend_tmp = sys.argv[sys.argv.index("--") + 1:][:3]
res = []


def check(name, cond, detail=""):
    res.append(("OK     " if cond else "FALLO  ") + name + ("  [%s]" % (detail,) if detail != "" else ""))
    return cond


win = bpy.context.window


def ctx3d():
    area = next(a for a in win.screen.areas if a.type == 'VIEW_3D')
    region = next(r for r in area.regions if r.type == 'WINDOW')
    return bpy.context.temp_override(window=win, area=area, region=region)


def tool():
    return bpy.context.workspace.tools.from_space_view3d_mode(bpy.context.mode, create=False)


def op(fn, **kw):
    with ctx3d():
        return fn(**kw)


def redraw():
    # El dibujo de la barra tambien toca la memoria de grupos: se fuerza para que la
    # prueba vea el mismo estado que un usuario con la barra a la vista.
    with ctx3d():
        bpy.ops.wm.redraw_timer(type='DRAW_WIN_SWAP', iterations=1)


def field(line, name):
    m = re.search(r"(?:^| )" + name + r"=(\S+)", line)
    return None if m is None else ("" if m.group(1) == "-" else m.group(1))


base = [l.rstrip("\n") for l in open(baseline_path)]

# 1. Estado inicial. Las herramientas no las crea la carga del fichero sino el primer
# refresco de cada area, asi que se dibuja antes de mirar. El indice prueba que hay
# RUNTIME: sin runtime, RNA devuelve 0 aunque el nombre este en DNA.
redraw()
t = tool()
check("hay herramienta activa al arrancar", t is not None and t.idname != "", t.idname if t else None)
if t is not None:
    fila = next((l for l in base if l.startswith("ACT VIEW_3D OBJECT %s " % t.idname)), None)
    esperado_idx = int(field(fila, "index")) if fila else None
    check("con runtime instalado (indice %s)" % esperado_idx, t.index == esperado_idx, t.index)

# 2. Prueba DIFERENCIAL: cada herramienta por el camino nativo y por el camino Python
# ANTIGUO (`activate_by_id` de space_toolsystem_common.py, que sigue cargado), sobre el
# mismo motor y el mismo estado. Se compara el resultado real, despues de que
# `WM_toolsystem_ref_set_from_runtime` aplique sus propias reglas (por ejemplo, borrar la
# reserva de las herramientas que no usan el keymap de reserva).
from bl_ui.space_toolsystem_common import activate_by_id as py_activate_by_id

GIZMO_INSET = "VIEW3D_GGT_tool_generic_handle_free"


def snapshot():
    t = tool()
    s = [t.idname, t.widget, t.idname_fallback, t.index]
    if t.idname == "builtin.inset_faces":
        g = t.gizmo_group_properties(GIZMO_INSET)
        s += [round(g.radius, 4), round(g.backdrop_fill_alpha, 4)]
    return tuple(s)


def diferencial(modo_linea):
    ids = [l.split()[3] for l in base if l.startswith("ACT VIEW_3D %s " % modo_linea)]
    fallos = 0
    for idname in ids:
        if idname == "builtin.inset_faces":
            # Centinela: si el nativo no escribe los valores iniciales, se veria.
            g = tool().gizmo_group_properties(GIZMO_INSET)
            g.radius, g.backdrop_fill_alpha = 1.0, 1.0
        r = op(bpy.ops.wm.tool_set_by_id, name=idname)
        nat = snapshot()
        with ctx3d():
            ok_py = py_activate_by_id(bpy.context, 'VIEW_3D', idname)
        py = snapshot()
        if r != {'FINISHED'} or not ok_py or nat != py:
            fallos += 1
            res.append("         difiere %s: nativo %r, python %r (%r, %r)" % (idname, nat, py, r, ok_py))
    check("nativo == Python antiguo en las %d herramientas de %s" % (len(ids), modo_linea),
          fallos == 0, "%d fallos" % fallos)


diferencial("OBJECT")
op(bpy.ops.object.mode_set, mode='EDIT')
diferencial("EDIT_MESH")
t = tool()
op(bpy.ops.wm.tool_set_by_id, name="builtin.inset_faces")
g = tool().gizmo_group_properties(GIZMO_INSET)
check("inset_faces arranca el gizmo con radio 75 y sin relleno",
      (round(g.radius, 3), round(g.backdrop_fill_alpha, 3)) == (75.0, 0.0),
      (g.radius, g.backdrop_fill_alpha))
op(bpy.ops.object.mode_set, mode='OBJECT')

# 3. Ciclo, que es lo que hace la W. Depende de la memoria de grupos.
op(bpy.ops.wm.tool_set_by_id, name="builtin.select_box")
redraw()
esperado = ["builtin.select_circle", "builtin.select_lasso", "builtin.select", "builtin.select_box"]
obtenido = []
for _ in esperado:
    op(bpy.ops.wm.tool_set_by_id, name="builtin.select_box", cycle=True)
    redraw()
    obtenido.append(tool().idname)
check("ciclo del grupo de seleccion desde Select Box", obtenido == esperado, obtenido)

# 4. Por posicion.
op(bpy.ops.wm.tool_set_by_index, index=0)
check("tool_set_by_index(0) expandido", tool().idname == "builtin.select", tool().idname)
op(bpy.ops.wm.tool_set_by_index, index=1, expand=False)
check("tool_set_by_index(1) sin expandir es el boton 2", tool().idname == "builtin.cursor", tool().idname)

# 5. Como reserva: no cambia la herramienta, cambia su reserva.
op(bpy.ops.wm.tool_set_by_id, name="builtin.move")
op(bpy.ops.wm.tool_set_by_id, name="builtin.select_lasso", as_fallback=True)
t = tool()
check("as_fallback conserva la herramienta", t.idname == "builtin.move", t.idname)
check("as_fallback cambia la reserva", t.idname_fallback == "builtin.select_lasso", t.idname_fallback)
ts = bpy.context.scene.tool_settings
check("as_fallback pone workspace_tool_type='FALLBACK'", ts.workspace_tool_type == 'FALLBACK',
      ts.workspace_tool_type)
ts.workspace_tool_type = 'DEFAULT'
op(bpy.ops.wm.tool_set_by_id, name="builtin.select", as_fallback=True)
ts.workspace_tool_type = 'DEFAULT'

# 6. Tipo de pincel, en escultura.
op(bpy.ops.object.mode_set, mode='SCULPT')
candidata = None
for line in base:
    if line.startswith("ACT VIEW_3D SCULPT ") and "USE_BRUSHES" in (field(line, "options") or ""):
        bt = field(line, "brush_type")
        if bt not in (None, "", "ANY") and line.split()[3] != "builtin.brush":
            candidata = (line.split()[3], bt)
            break
if check("hay herramienta de escultura con tipo de pincel en la linea base", candidata is not None):
    r = op(bpy.ops.wm.tool_set_by_brush_type, brush_type=candidata[1])
    check("tool_set_by_brush_type(%s)" % candidata[1], tool().idname == candidata[0],
          "%s %r" % (tool().idname, r))
    r = op(bpy.ops.wm.tool_set_by_brush_type, brush_type="NO_EXISTE")
    check("tipo de pincel desconocido cae en builtin.brush", tool().idname == "builtin.brush",
          "%s %r" % (tool().idname, r))
op(bpy.ops.object.mode_set, mode='OBJECT')

# 7. Guardar con una herramienta no por defecto, para reabrir en otro proceso.
op(bpy.ops.wm.tool_set_by_id, name="builtin.move")
bpy.ops.wm.save_as_mainfile(filepath=blend_tmp, check_existing=False)
check("guardado con builtin.move activa", tool().idname == "builtin.move", tool().idname)

with open(out_path, "w") as f:
    f.write("\n".join(res) + "\n")
print("SMOKE_TOOLS_DONE", sum(1 for r in res if r.startswith("FALLO")), "fallos")
bpy.ops.wm.quit_blender()
