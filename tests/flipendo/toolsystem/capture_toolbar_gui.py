# Captura de la BARRA DE HERRAMIENTAS, recortada a su region, en varios editores y modos.
#
# Sirve para verificar el dibujo nativo de la barra contra el de Python: si el dibujo es
# el mismo, las capturas tienen que ser identicas pixel a pixel. Se hace antes de cambiar
# nada (linea base) y despues, con el mismo script.
#
# Uso:  Blender --factory-startup --python capture_toolbar_gui.py -- <directorio>
import hashlib
import os
import sys

import bpy
import numpy as np

outdir = sys.argv[sys.argv.index("--") + 1]
os.makedirs(outdir, exist_ok=True)
win = bpy.context.window
area = next(a for a in win.screen.areas if a.type == 'VIEW_3D')
lineas = []


def ctx():
    region = next(r for r in area.regions if r.type == 'WINDOW')
    return bpy.context.temp_override(window=win, area=area, region=region)


def redraw():
    with ctx():
        bpy.ops.wm.redraw_timer(type='DRAW_WIN_SWAP', iterations=2)


def captura(nombre, region_type='TOOLS'):
    # Al cambiar el tipo de area por script, algunos editores (imagen, nodos) arrancan con
    # la barra oculta; sin forzarla no hay nada que comparar.
    space = area.spaces.active
    if region_type == 'TOOLS' and hasattr(space, "show_region_toolbar"):
        space.show_region_toolbar = True
    # Las regiones se asientan en varios redibujados tras un cambio (el ancho llego a
    # variar en 1 pixel entre ejecuciones). Se espera a que el tamano no cambie en dos
    # redibujados seguidos: sin esto la captura no es determinista y no prueba nada.
    previo = None
    for _ in range(30):
        redraw()
        region = next((r for r in area.regions if r.type == region_type), None)
        actual = None if region is None else (region.x, region.y, region.width, region.height)
        if actual == previo:
            break
        previo = actual
    region = next((r for r in area.regions if r.type == region_type), None)
    if region is None or region.width <= 1 or region.height <= 1:
        lineas.append("%-28s %-12s (sin region visible)" % (nombre, region_type))
        return
    tmp = os.path.join(outdir, "_area.png")
    with ctx():
        bpy.ops.screen.screenshot_area(filepath=tmp)
    img = bpy.data.images.load(tmp, check_existing=False)
    w, h = img.size
    px = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)
    bpy.data.images.remove(img)
    x0, y0 = region.x - area.x, region.y - area.y
    crop = px[y0:y0 + region.height, x0:x0 + region.width, :]
    u8 = (np.clip(crop, 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8)
    np.save(os.path.join(outdir, nombre + "-" + region_type + ".npy"), u8)
    lineas.append("%-28s %-12s %4dx%-4d %s" % (
        nombre, region_type, region.width, region.height, hashlib.sha1(u8.tobytes()).hexdigest()))


# Vista 3D en varios modos. Solo los que se pueden montar con el cubo de fabrica.
captura("view3d_object")
captura("view3d_object", 'TOOL_HEADER')
for modo, nombre in (('EDIT', "view3d_edit_mesh"), ('SCULPT', "view3d_sculpt"),
                     ('WEIGHT_PAINT', "view3d_paint_weight"), ('VERTEX_PAINT', "view3d_paint_vertex"),
                     ('TEXTURE_PAINT', "view3d_paint_texture")):
    with ctx():
        bpy.ops.object.mode_set(mode=modo)
    captura(nombre)
    with ctx():
        bpy.ops.object.mode_set(mode='OBJECT')

# Editor de imagen en modo UV, con la malla en edicion.
with ctx():
    bpy.ops.object.mode_set(mode='EDIT')
area.type = 'IMAGE_EDITOR'
area.spaces.active.mode = 'UV'
captura("image_uv")
area.spaces.active.mode = 'VIEW'
captura("image_view")
area.spaces.active.mode = 'PAINT'
captura("image_paint")
area.type = 'VIEW_3D'
with ctx():
    bpy.ops.object.mode_set(mode='OBJECT')

# Editor de nodos.
area.type = 'NODE_EDITOR'
area.ui_type = 'ShaderNodeTree'
captura("node_shader")

# Secuenciador.
area.type = 'SEQUENCE_EDITOR'
for vista in ('SEQUENCER', 'PREVIEW', 'SEQUENCER_PREVIEW'):
    area.spaces.active.view_type = vista
    captura("sequencer_" + vista.lower())

with open(os.path.join(outdir, "capturas.txt"), "w") as f:
    f.write("\n".join(lineas) + "\n")
print("CAPTURE_DONE", len(lineas))
bpy.ops.wm.quit_blender()
