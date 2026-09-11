# Comparacion visual DIFERENCIAL de la CABECERA DE HERRAMIENTA y del panel lateral
# "Tool", herramienta a herramienta, en todos los editores y modos que se pueden montar
# por script.
#
# Por que diferencial y no contra un binario de referencia
# --------------------------------------------------------
# La primera version capturaba con un binario de referencia (el de la fase 4a, con el
# dibujo aun en Python) y comparaba contra el nativo. Dio 819 de 819 distintas, y ninguna
# era del codigo: la linea base de referencia no se reproducia ni a si misma (dos
# ejecuciones de la MISMA app daban huellas distintas) y el resaltado dependia de donde
# cayera el raton real al abrirse la ventana.
#
# La causa de fondo es que dos ejecuciones distintas no comparten estado: escala de la
# ventana, tema, orden de asentamiento de las regiones, cursor. Asi que se cambia el
# metodo, igual que se hizo en la fase 2 con `smoke_tools_gui.py`: se compara **el mismo
# binario contra si mismo**, en la misma ejecucion, sobre el mismo estado y en el mismo
# instante. Para cada herramienta se dibuja la region dos veces:
#
#   - NATIVO: `layout.template_tool_header(...)`, el dibujo en C++ (`FL_toolbar_ui.hh`).
#   - PYTHON: el cuerpo de `ToolSelectPanelHelper.draw_active_tool_header` tal cual estaba
#     en la fase 4a (commit 14294531f85), copiado aqui abajo sin tocar una coma. Llama a
#     los `draw_settings` del catalogo Python, que siguen en
#     `bl_ui/space_toolsystem_toolbar.py`.
#
# Misma region, misma posicion, mismo tamano: si la huella cambia, cambia el dibujo.
#
# Este arnes solo puede ejecutarse mientras el catalogo Python siga en el arbol; muere con
# el. La evidencia queda en el commit y en `politicas/TOOLSYSTEM-A-CPP.md`.
#
# Uso:  Blender --factory-startup --python capture_tool_headers_gui.py -- \
#           <directorio> <activation-python.txt> [claves a guardar en imagen] [solo estos modos]
#
# Deja en <directorio>: `cabeceras-nativo.txt` y `cabeceras-python.txt`, para
# `compare_captures.py`.
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
lineas = {"NATIVO": [], "PYTHON": []}
herramientas = {}
for l in open(baseline_path):
    if l.startswith("ACT "):
        p = l.split()
        herramientas.setdefault((p[1], p[2]), []).append(p[3])


# ---------------------------------------------------------------------------------------
# El dibujo de la fase 4a, en Python, como oraculo
#
# Copia literal de `ToolSelectPanelHelper.draw_active_tool_header` y
# `draw_active_tool_fallback` en el commit 14294531f85, cuando aun dibujaban desde Python.
# No se toca nada: cualquier "mejora" aqui invalidaria la comparacion.
try:
    from bl_ui.space_toolsystem_common import ToolSelectPanelHelper
except ImportError:
    # El armazon Python ya no esta: el oraculo murio con el, como estaba previsto. Se
    # captura solo el dibujo nativo, para poder compararlo con la tanda anterior.
    ToolSelectPanelHelper = None
from bpy.app.translations import pgettext_iface as iface_, contexts as i18n_contexts

modo_dibujo = "NATIVO"
# Se pone a True cada vez que el oraculo dibuja de verdad. Sin esto, si la interfaz
# dejara de pasar por el (por ejemplo porque ya llame a la plantilla nativa directamente),
# la pasada PYTHON capturaria el dibujo NATIVO y las dos tandas saldrian identicas: un
# aprobado falso, que es peor que un suspenso.
oraculo_usado = False


def _python_4a_draw_active_tool_fallback(context, layout, tool, *, is_horizontal_layout=False):
    idname_fallback = tool.idname_fallback
    space_type = tool.space_type
    cls = ToolSelectPanelHelper._tool_class_from_space_type(space_type)
    item_fallback, _index = cls._tool_get_by_id(context, idname_fallback)
    if item_fallback is not None:
        draw_settings = item_fallback.draw_settings
        if draw_settings is not None:
            if not is_horizontal_layout:
                layout.separator()
            draw_settings(context, layout, tool)


def _python_4a_draw_active_tool_header(context, layout, *, show_tool_icon_always=False, tool_key=None):
    if tool_key is None:
        space_type, mode = ToolSelectPanelHelper._tool_key_from_context(context)
    else:
        space_type, mode = tool_key

    if space_type is None:
        return None

    cls = ToolSelectPanelHelper._tool_class_from_space_type(space_type)
    item, tool, icon_value = cls._tool_get_active(context, space_type, mode, with_icon=True)
    if item is None:
        return None
    if show_tool_icon_always:
        layout.label(
            text="    " + iface_(item.label, i18n_contexts.operator_default),
            icon_value=icon_value,
            translate=False,
        )
        layout.separator()
    else:
        if not context.space_data.show_region_toolbar:
            layout.template_icon(icon_value=icon_value, scale=0.5)
            layout.separator()

    draw_settings = item.draw_settings
    if draw_settings is not None:
        draw_settings(context, layout, tool)

    idname_fallback = tool.idname_fallback
    if idname_fallback and idname_fallback != item.idname:
        tool_settings = context.tool_settings

        if tool_settings.workspace_tool_type == 'FALLBACK':
            tool_fallback_id = cls.tool_fallback_id
            item, _select_index = cls._tool_get_by_id_active(context, tool_fallback_id)
            label = item.label
        else:
            label = "Active Tool"

        row = layout.row(heading="Drag", heading_ctxt=i18n_contexts.editor_view3d)
        row.context_pointer_set("tool", tool)
        row.popover(
            panel="TOPBAR_PT_tool_fallback",
            text=iface_(label, i18n_contexts.operator_default),
            translate=False,
        )

    return tool


def _header_conmutado(context, layout, **kw):
    global oraculo_usado
    # Un unico punto de entrada: lo que dibuje depende del modo en curso. Todos los
    # llamantes de `bl_ui` (cabecera de la vista 3D, del editor de imagen, del
    # secuenciador y el panel lateral "Active Tool") pasan por aqui.
    if not oraculo_usado:
        print("ORACULO: llamada a la cabecera, modo=%s" % modo_dibujo, flush=True)
    if modo_dibujo == "PYTHON":
        oraculo_usado = True
        return _python_4a_draw_active_tool_header(context, layout, **kw)
    return _nativo_header(context, layout, **kw)


def _fallback_conmutado(context, layout, tool, **kw):
    if modo_dibujo == "PYTHON":
        return _python_4a_draw_active_tool_fallback(context, layout, tool, **kw)
    return _nativo_fallback(context, layout, tool, **kw)


# Donde se engancha el conmutador
# ------------------------------
# En la fase 4c bastaba con sustituir `ToolSelectPanelHelper.draw_active_tool_header`,
# porque toda la interfaz pasaba por ahi. Ya no: los consumidores llaman a la plantilla
# nativa `layout.template_tool_header(...)` directamente, y las funciones de RNA no se
# pueden sustituir desde Python (`bpy.types.UILayout` no las expone como atributos de
# clase). Asi que el conmutador se engancha en los consumidores, que son ocho y estan
# contados:
#
#   - los cinco paneles laterales `*_PT_active_tool`, cuyo `draw` ES la cabecera; y
#   - los tres `*_HT_tool_header.draw_tool_settings`, de los que se deja SOLO la
#     cabecera, sin los ajustes de modo que vienen detras. Eso es a proposito: quitarlos
#     de las DOS pasadas deja la region con la cabecera y nada mas, que es justo lo que
#     se quiere comparar. Dejarlos solo en una lado haria que todo saliera distinto.

_PANELES = (
    ("VIEW3D_PT_active_tool", 'VIEW_3D'),
    ("VIEW3D_PT_active_tool_duplicate", 'VIEW_3D'),
    ("IMAGE_PT_active_tool", 'IMAGE_EDITOR'),
    ("NODE_PT_active_tool", 'NODE_EDITOR'),
    ("SEQUENCER_PT_active_tool", 'SEQUENCE_EDITOR'),
)
_CABECERAS = (
    ("VIEW3D_HT_tool_header", 'VIEW_3D'),
    ("IMAGE_HT_tool_header", 'IMAGE_EDITOR'),
    ("SEQUENCER_HT_tool_header", 'SEQUENCE_EDITOR'),
)


def _clave(context, space_type):
    return ToolSelectPanelHelper._tool_key_from_context(context, space_type=space_type)


def _panel_conmutado(self, context, _space_type):
    layout = self.layout
    layout.use_property_split = True
    layout.use_property_decorate = False
    _header_conmutado(context, layout.column(),
                      show_tool_icon_always=True, tool_key=_clave(context, _space_type))


def _cabecera_conmutada(self, context, _space_type):
    _header_conmutado(context, self.layout, tool_key=_clave(context, _space_type))


def _enganchar():
    if ToolSelectPanelHelper is None:
        return False
    enganchados = 0
    for nombre, space_type in _PANELES:
        cls = getattr(bpy.types, nombre, None)
        if cls is None:
            continue
        cls.draw = (lambda st: lambda self, context: _panel_conmutado(self, context, st))(space_type)
        enganchados += 1
    for nombre, space_type in _CABECERAS:
        cls = getattr(bpy.types, nombre, None)
        if cls is None:
            continue
        cls.draw_tool_settings = (
            lambda st: lambda self, context: _cabecera_conmutada(self, context, st))(space_type)
        enganchados += 1
    print("ORACULO: %d consumidores enganchados de %d" % (enganchados, len(_PANELES) + len(_CABECERAS)),
          flush=True)
    return enganchados == len(_PANELES) + len(_CABECERAS)


if ToolSelectPanelHelper is not None:
    _nativo_header = ToolSelectPanelHelper.draw_active_tool_header
    _nativo_fallback = ToolSelectPanelHelper.draw_active_tool_fallback
    if not _enganchar():
        raise SystemExit("no se pudieron enganchar todos los consumidores")


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


def volcar():
    """Escribe lo capturado hasta ahora.

    Se llama al terminar cada modo, no solo al final. Una tanda completa son 30 modos y
    mas de una hora; si se corta a mitad —y se corto—, sin esto se pierde todo lo medido
    y hay que repetirlo entero.
    """
    for modo, nombre in (("NATIVO", "cabeceras-nativo.txt"), ("PYTHON", "cabeceras-python.txt")):
        if modo == "PYTHON" and not oraculo_usado:
            continue
        with open(os.path.join(outdir, nombre), "w") as f:
            f.write("\n".join(lineas[modo]) + "\n")


def avisar(texto):
    lineas["NATIVO"].append(texto)
    lineas["PYTHON"].append(texto)


def modo_reset():
    global modo_dibujo
    modo_dibujo = "NATIVO"


def _recortes():
    """Un redibujado, una captura, y los recortes de las dos regiones que interesan."""
    for region in area.regions:
        region.tag_redraw()
    with ctx():
        bpy.ops.wm.redraw_timer(type='DRAW_WIN_SWAP', iterations=1)
    tmp = os.path.join(outdir, "_area.png")
    with ctx():
        bpy.ops.screen.screenshot_area(filepath=tmp)
    img = bpy.data.images.load(tmp, check_existing=False)
    w, h = img.size
    px = np.array(img.pixels[:], dtype=np.float32).reshape(h, w, 4)
    bpy.data.images.remove(img)
    out = {}
    for region_type in ('TOOL_HEADER', 'UI'):
        region = next((r for r in area.regions if r.type == region_type), None)
        if region is None or region.width <= 1 or region.height <= 1:
            out[region_type] = None
            continue
        x0, y0 = region.x - area.x, region.y - area.y
        u8 = (np.clip(px[y0:y0 + region.height, x0:x0 + region.width, :], 0.0, 1.0) * 255.0 + 0.5).astype(np.uint8)
        out[region_type] = (region.width, region.height, hashlib.sha1(u8.tobytes()).hexdigest(), u8)
    return out


def _huellas(recortes):
    return {k: (v[:3] if v is not None else None) for k, v in recortes.items()}


def _capturar_una(clave_base, modo):
    """Captura hasta que DOS capturas seguidas den los mismos pixeles.

    Esperar solo a que la geometria de las regiones se asiente no basta, y costo 20 de
    las 38 diferencias de la primera comparacion completa: en el secuenciador en vista de
    previsualizacion la captura salia con la herramienta ANTERIOR todavia pintada, asi que
    la pasada nativa se comparaba contra el dibujo Python de la herramienta siguiente. No
    era el codigo, era el arnes; y la forma del error —cada huella nativa igual a la
    Python de la anterior— es lo que lo delato. Aqui se exige estabilidad del CONTENIDO,
    que es lo unico que no puede ir con retraso.
    """
    global modo_dibujo
    modo_dibujo = modo
    fijar_cursor()
    previas = None
    recortes = None
    for _ in range(8):
        recortes = _recortes()
        huellas = _huellas(recortes)
        if huellas == previas:
            break
        previas = huellas
    else:
        avisar("%s (%s: el dibujo no se estabilizo en 8 redibujados)" % (clave_base, modo))
    destino = lineas[modo]
    for region_type in ('TOOL_HEADER', 'UI'):
        clave = clave_base + " " + region_type
        dato = recortes[region_type]
        if dato is None:
            destino.append("%s (sin region)" % clave)
            continue
        ancho, alto, huella, u8 = dato
        destino.append("%s %dx%d %s" % (clave, ancho, alto, huella))
        if any(g in clave for g in guardar):
            np.save(os.path.join(outdir, modo + "__" + clave.replace(" ", "__") + ".npy"), u8)


def capturar(clave_base):
    # Las dos versiones seguidas, sobre el mismo estado: solo cambia quien dibuja.
    _capturar_una(clave_base, "NATIVO")
    if ToolSelectPanelHelper is not None:
        _capturar_una(clave_base, "PYTHON")
    modo_reset()


def recorrer(space, mode):
    if solo and ("%s %s" % (space, mode)) not in solo:
        return
    lista = herramientas.get((space, mode), [])
    if not lista:
        avisar("%s %s (sin herramientas en la linea base)" % (space, mode))
        return
    preparar_area()
    print("MODO %s %s (%d herramientas)" % (space, mode, len(lista)), flush=True)
    for idname in lista:
        try:
            r = op(bpy.ops.wm.tool_set_by_id, name=idname)
        except Exception as ex:
            avisar("%s %s %s (no se pudo activar: %s)" % (space, mode, idname, ex))
            continue
        if r != {'FINISHED'}:
            avisar("%s %s %s (no activa: %r)" % (space, mode, idname, r))
            continue
        capturar("%s %s %s" % (space, mode, idname))
        print("  %s" % idname, flush=True)
    volcar()


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
        avisar("VIEW_3D %s (no se pudo entrar: %s)" % (esperado, ex))
        return False
    if bpy.context.mode != esperado:
        avisar("VIEW_3D %s (se entro en %s)" % (esperado, bpy.context.mode))
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
        avisar("(no se pudo crear %s: %s)" % (fn, ex))
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
    avisar("EXCEPCION\n" + traceback.format_exc())
finally:
    volcar()
    if not oraculo_usado:
        print("HEADERS_CAPTURE_SIN_ORACULO: solo se capturo el dibujo nativo.")
    print("HEADERS_CAPTURE_DONE", len(lineas["NATIVO"]), len(lineas["PYTHON"]))
    bpy.ops.wm.quit_blender()
