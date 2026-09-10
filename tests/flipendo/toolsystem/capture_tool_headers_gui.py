# Captura de la CABECERA DE HERRAMIENTA y del panel lateral "Tool", herramienta a
# herramienta, en todos los editores y modos que se pueden montar por script.
#
# Es la linea base visual del dibujo de ajustes (`draw_settings`). Los ajustes del Python
# se ramifican segun el tipo de region, asi que se recortan dos de cada captura: la
# cabecera (TOOL_HEADER) y el panel lateral (UI, pestana Tool). Si el dibujo nativo es el
# mismo, las huellas tienen que coincidir.
#
# Uso:  Blender --factory-startup --python capture_tool_headers_gui.py -- \
#           <directorio> <activation-python.txt> [claves a guardar en imagen, separadas por coma]
import hashlib
import os
import sys
import traceback

import bpy
import numpy as np

argv = sys.argv[sys.argv.index("--") + 1:]
outdir, baseline_path = argv[0], argv[1]
guardar = [k for k in (argv[2].split(",") if len(argv) > 2 else []) if k]
# Opcional: solo estas combinaciones "ESPACIO MODO", separadas por coma. Para diagnosticar
# sin recorrer los 30 modos, que lleva casi una hora.
solo = [k for k in (argv[3].split(",") if len(argv) > 3 else []) if k]
os.makedirs(outdir, exist_ok=True)

win = bpy.context.window
area = next(a for a in win.screen.areas if a.type == 'VIEW_3D')
lineas = []
herramientas = {}
for l in open(baseline_path):
    if l.startswith("ACT "):
        p = l.split()
        herramientas.setdefault((p[1], p[2]), []).append(p[3])


def ctx():
    region = next(r for r in area.regions if r.type == 'WINDOW')
    return bpy.context.temp_override(window=win, area=area, region=region)


def op(fn, **kw):
    with ctx():
        return fn(**kw)


def fijar_cursor():
    # El resaltado de botones y regiones depende de donde este el raton, y el raton real
    # queda donde caiga al abrirse la ventana: sin fijarlo, dos ejecuciones del MISMO
    # binario dieron pixeles distintos. Se lleva siempre al centro del esquema (outliner),
    # que no se captura. `cursor_warp` actualiza tambien `eventstate->xy`.
    destino = next((a for a in win.screen.areas if a.type == 'OUTLINER'), None)
    if destino is not None:
        win.cursor_warp(destino.x + destino.width // 2, destino.y + destino.height // 2)
    else:
        win.cursor_warp(1, 1)


def estabilizar():
    fijar_cursor()
    previo = None
    for _ in range(30):
        with ctx():
            bpy.ops.wm.redraw_timer(type='DRAW_WIN_SWAP', iterations=1)
        actual = tuple((r.type, r.x, r.y, r.width, r.height) for r in area.regions)
        if actual == previo:
            return
        previo = actual


def preparar_area():
    space = area.spaces.active
    for attr in ("show_region_tool_header", "show_region_ui", "show_region_toolbar"):
        if hasattr(space, attr):
            setattr(space, attr, True)
    estabilizar()
    ui = next((r for r in area.regions if r.type == 'UI'), None)
    if ui is not None:
        try:
            ui.active_panel_category = 'Tool'
        except Exception:
            pass


def capturar(clave_base):
    estabilizar()
    tmp = os.path.join(outdir, "_area.png")
    with ctx():
        bpy.ops.screen.screenshot_area(filepath=tmp)
    img = bpy.data.images.load(tmp, check_existing=False)
    w, h = img.size
    px = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)
    bpy.data.images.remove(img)
    for region_type in ('TOOL_HEADER', 'UI'):
        region = next((r for r in area.regions if r.type == region_type), None)
        clave = clave_base + " " + region_type
        if region is None or region.width <= 1 or region.height <= 1:
            lineas.append("%s (sin region)" % clave)
            continue
        x0, y0 = region.x - area.x, region.y - area.y
        u8 = (np.clip(px[y0:y0 + region.height, x0:x0 + region.width, :], 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8)
        lineas.append("%s %dx%d %s" % (clave, region.width, region.height, hashlib.sha1(u8.tobytes()).hexdigest()))
        if any(g in clave for g in guardar):
            np.save(os.path.join(outdir, clave.replace(" ", "__") + ".npy"), u8)


def recorrer(space, mode):
    if solo and ("%s %s" % (space, mode)) not in solo:
        return
    lista = herramientas.get((space, mode), [])
    if not lista:
        lineas.append("%s %s (sin herramientas en la linea base)" % (space, mode))
        return
    preparar_area()
    for idname in lista:
        try:
            r = op(bpy.ops.wm.tool_set_by_id, name=idname)
        except Exception as ex:
            lineas.append("%s %s %s (no se pudo activar: %s)" % (space, mode, idname, ex))
            continue
        if r != {'FINISHED'}:
            lineas.append("%s %s %s (no activa: %r)" % (space, mode, idname, r))
            continue
        capturar("%s %s %s" % (space, mode, idname))


def activar_objeto(ob):
    with ctx():
        if bpy.context.object is not None and bpy.context.object.mode != 'OBJECT':
            bpy.ops.object.mode_set(mode='OBJECT')
        bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob


def modo(ob, mode_set, esperado):
    try:
        activar_objeto(ob)
        op(bpy.ops.object.mode_set, mode=mode_set)
    except Exception as ex:
        lineas.append("VIEW_3D %s (no se pudo entrar: %s)" % (esperado, ex))
        return False
    if bpy.context.mode != esperado:
        lineas.append("VIEW_3D %s (se entro en %s)" % (esperado, bpy.context.mode))
        return False
    return True


def anadir(fn, **kw):
    try:
        with ctx():
            if bpy.context.object is not None and bpy.context.object.mode != 'OBJECT':
                bpy.ops.object.mode_set(mode='OBJECT')
        op(fn, **kw)
        return bpy.context.active_object
    except Exception as ex:
        lineas.append("(no se pudo crear %s: %s)" % (fn, ex))
        return None


try:
    cubo = bpy.data.objects["Cube"]
    configs = [
        (cubo, 'OBJECT', 'OBJECT'), (cubo, 'EDIT', 'EDIT_MESH'), (cubo, 'SCULPT', 'SCULPT'),
        (cubo, 'WEIGHT_PAINT', 'PAINT_WEIGHT'), (cubo, 'VERTEX_PAINT', 'PAINT_VERTEX'),
        (cubo, 'TEXTURE_PAINT', 'PAINT_TEXTURE'),
    ]
    for ob, ms, esperado in configs:
        if modo(ob, ms, esperado):
            recorrer('VIEW_3D', esperado)

    activar_objeto(cubo)
    if anadir(bpy.ops.object.particle_system_add) is not None:
        cubo.particle_systems[0].settings.type = 'HAIR'
        if modo(cubo, 'PARTICLE_EDIT', 'PARTICLE'):
            recorrer('VIEW_3D', 'PARTICLE')

    otros = [
        (bpy.ops.curve.primitive_bezier_curve_add, {}, [('EDIT', 'EDIT_CURVE')]),
        (bpy.ops.surface.primitive_nurbs_surface_sphere_add, {}, [('EDIT', 'EDIT_SURFACE')]),
        (bpy.ops.object.text_add, {}, [('EDIT', 'EDIT_TEXT')]),
        (bpy.ops.object.armature_add, {}, [('EDIT', 'EDIT_ARMATURE'), ('POSE', 'POSE')]),
        (bpy.ops.object.metaball_add, {}, [('EDIT', 'EDIT_METABALL')]),
        (bpy.ops.object.add, {"type": 'LATTICE'}, [('EDIT', 'EDIT_LATTICE')]),
        (bpy.ops.object.curves_random_add, {}, [('EDIT', 'EDIT_CURVES'), ('SCULPT_CURVES', 'SCULPT_CURVES')]),
        (bpy.ops.object.pointcloud_random_add, {}, [('EDIT', 'EDIT_POINTCLOUD')]),
        (bpy.ops.object.grease_pencil_add, {"type": 'STROKE'},
         [('EDIT', 'EDIT_GREASE_PENCIL'), ('PAINT_GREASE_PENCIL', 'PAINT_GREASE_PENCIL'),
          ('SCULPT_GREASE_PENCIL', 'SCULPT_GREASE_PENCIL'), ('WEIGHT_GREASE_PENCIL', 'WEIGHT_GREASE_PENCIL'),
          ('VERTEX_GREASE_PENCIL', 'VERTEX_GREASE_PENCIL')]),
    ]
    for fn, kw, modos in otros:
        ob = anadir(fn, **kw)
        if ob is None:
            continue
        for ms, esperado in modos:
            if modo(ob, ms, esperado):
                recorrer('VIEW_3D', esperado)

    # Editor de imagen, con la malla en edicion para el modo UV.
    modo(cubo, 'EDIT', 'EDIT_MESH')
    area.type = 'IMAGE_EDITOR'
    for m in ('VIEW', 'UV', 'PAINT'):
        area.spaces.active.mode = m
        recorrer('IMAGE_EDITOR', m)
    area.type = 'VIEW_3D'
    modo(cubo, 'OBJECT', 'OBJECT')

    area.type = 'NODE_EDITOR'
    area.ui_type = 'ShaderNodeTree'
    recorrer('NODE_EDITOR', 'None')

    area.type = 'SEQUENCE_EDITOR'
    for vista in ('SEQUENCER', 'PREVIEW', 'SEQUENCER_PREVIEW'):
        area.spaces.active.view_type = vista
        recorrer('SEQUENCE_EDITOR', vista)
except Exception:
    lineas.append("EXCEPCION\n" + traceback.format_exc())
finally:
    with open(os.path.join(outdir, "cabeceras.txt"), "w") as f:
        f.write("\n".join(lineas) + "\n")
    print("HEADERS_CAPTURE_DONE", len(lineas))
    bpy.ops.wm.quit_blender()
