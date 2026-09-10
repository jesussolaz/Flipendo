# La memoria de grupos que ve la interfaz Python tiene que ser la del motor.
#
# Tras fijar Select Lasso como reserva, el popover y la tarta de reserva deben resaltar
# Select Lasso, y la cabecera decir "Drag: Select Lasso". Los tres lo leen por
# `_tool_group_active_get_from_item`. Si esa memoria es un diccionario Python que el
# motor no actualiza, dicen "Tweak".
#
# Uso:  Blender --factory-startup --python check_fallback_memory_gui.py -- <salida.txt>
import sys
import traceback

import bpy

out_path = sys.argv[sys.argv.index("--") + 1]
res = []
try:
    from bl_ui.space_toolsystem_toolbar import VIEW3D_PT_tools_active as TB
    from bl_ui.space_toolsystem_common import ToolSelectPanelHelper
    win = bpy.context.window
    area = next(a for a in win.screen.areas if a.type == 'VIEW_3D')
    region = next(r for r in area.regions if r.type == 'WINDOW')
    with bpy.context.temp_override(window=win, area=area, region=region):
        bpy.ops.wm.tool_set_by_id(name="builtin.move")
        bpy.ops.wm.tool_set_by_id(name="builtin.select_lasso", as_fallback=True)
    # Aparte: `redraw_timer` deja el contexto sin area al terminar.
    with bpy.context.temp_override(window=win, area=area, region=region):
        bpy.ops.wm.redraw_timer(type='DRAW_WIN_SWAP', iterations=2)
    with bpy.context.temp_override(window=win, area=area, region=region):
        tool = ToolSelectPanelHelper.tool_active_from_context(bpy.context)
        _i, _s, grupo = TB._tool_get_by_id_active_with_group(bpy.context, TB.tool_fallback_id)
        resaltada = grupo[TB._tool_group_active_get_from_item(grupo)].idname
        item, _ = TB._tool_get_by_id_active(bpy.context, TB.tool_fallback_id)
        res.append("reserva en DNA:                 %s" % tool.idname_fallback)
        res.append("resalta el popover y la tarta:  %s" % resaltada)
        res.append("etiqueta 'Drag:' de la cabecera: %s" % item.idname)
        ok = (tool.idname_fallback == resaltada == item.idname == "builtin.select_lasso")
        res.append(("OK     " if ok else "FALLO  ") + "la interfaz Python ve la reserva del motor")
except Exception:
    res.append("EXCEPCION\n" + traceback.format_exc())
finally:
    with open(out_path, "w") as f:
        f.write("\n".join(res) + "\n")
    bpy.ops.wm.quit_blender()
