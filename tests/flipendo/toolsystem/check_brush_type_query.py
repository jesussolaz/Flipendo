# Comprobacion DIFERENCIAL de `has_tool_with_brush_type`.
#
# Es la unica consulta del subsistema que no tenia linea base propia: la usa el estante de
# pinceles (`BrushAssetShelf.brush_type_poll`) para esconder los pinceles cuyo tipo no
# tiene herramienta en el modo actual. Si el nativo dijera "si" de mas, aparecerian
# pinceles que antes no salian; si dijera "no" de mas, desaparecerian. Ninguna de las dos
# cosas la ve el volcado del catalogo.
#
# Para cada modo de pintura y para CADA valor de la enumeracion de tipos de pincel de ese
# modo se comparan:
#   - NATIVO: `context.workspace.tools.has_tool_with_brush_type(valor)`.
#   - PYTHON: el recorrido que hacia `properties_paint_common.py` antes de migrarlo,
#     copiado aqui literalmente, sobre el catalogo Python que aun esta en el arbol.
#
# Muere con el catalogo Python, como los demas oraculos del subsistema.
#
# Uso:  Blender --background --factory-startup --python check_brush_type_query.py
import sys
import traceback

import bpy

IGNORADAS = {
    "builtin.arc", "builtin.curve", "builtin.line",
    "builtin.box", "builtin.circle", "builtin.polyline",
}

# Los nueve estantes de pincel que existen, con su modo y la propiedad de `Brush` que
# declara cada uno (`space_view3d.py:9059` y siguientes, `space_image.py:1793`).
# (objeto que hace falta, modo de objeto, modo de contexto, propiedad de tipos de pincel)
MODOS = [
    ("MALLA", 'SCULPT', 'SCULPT', "sculpt_tool"),
    ("MALLA", 'VERTEX_PAINT', 'PAINT_VERTEX', "vertex_tool"),
    ("MALLA", 'WEIGHT_PAINT', 'PAINT_WEIGHT', "weight_tool"),
    ("MALLA", 'TEXTURE_PAINT', 'PAINT_TEXTURE', "image_tool"),
    ("CURVAS", 'SCULPT_CURVES', 'SCULPT_CURVES', "curves_sculpt_tool"),
    ("LAPIZ", 'PAINT_GREASE_PENCIL', 'PAINT_GREASE_PENCIL', "gpencil_tool"),
    ("LAPIZ", 'SCULPT_GREASE_PENCIL', 'SCULPT_GREASE_PENCIL', "gpencil_sculpt_tool"),
    ("LAPIZ", 'VERTEX_GREASE_PENCIL', 'VERTEX_GREASE_PENCIL', "gpencil_vertex_tool"),
    ("LAPIZ", 'WEIGHT_GREASE_PENCIL', 'WEIGHT_GREASE_PENCIL', "gpencil_weight_tool"),
]
# El noveno estante vive en el editor de imagen, no en la vista 3D (`space_image.py:1793`).
MODOS_IMAGEN = [('PAINT', "image_tool")]


def python_has_tool_with_brush_type(context, tool_prop, brush_type):
    from bl_ui.space_toolsystem_common import ToolSelectPanelHelper
    space_type = context.space_data.type
    brush_type_items = bpy.types.Brush.bl_rna.properties[tool_prop].enum_items
    tool_helper_cls = ToolSelectPanelHelper._tool_class_from_space_type(space_type)
    for item in ToolSelectPanelHelper._tools_flatten(
            tool_helper_cls.tools_from_context(context, mode=context.mode),
    ):
        if item is None:
            continue
        if item.idname in IGNORADAS:
            continue
        if item.options is None or ('USE_BRUSHES' not in item.options):
            continue
        if item.brush_type is not None:
            if brush_type_items[item.brush_type].value == brush_type:
                return True
    return False


win = bpy.context.window_manager.windows[0]
area = next(a for a in win.screen.areas if a.type == 'VIEW_3D')
region = next(r for r in area.regions if r.type == 'WINDOW')
fallos = []
iguales = distintas = 0
# Cuantos "si" da cada lado. Sin esto la prueba podria pasar con los dos lados diciendo
# siempre que no, que es exactamente la forma en que una comprobacion no comprueba nada.
ciertos = [0, 0]


def crear(fn, **kw):
    with bpy.context.temp_override(window=win, area=area, region=region):
        if bpy.context.object is not None and bpy.context.object.mode != 'OBJECT':
            bpy.ops.object.mode_set(mode='OBJECT')
        fn(**kw)
    return bpy.context.active_object


objetos = {"MALLA": bpy.data.objects["Cube"]}
try:
    objetos["CURVAS"] = crear(bpy.ops.object.curves_random_add)
    objetos["LAPIZ"] = crear(bpy.ops.object.grease_pencil_add, type='STROKE')
except Exception:
    fallos.append("EXCEPCION creando objetos\n" + traceback.format_exc())


def activar(ob):
    with bpy.context.temp_override(window=win, area=area, region=region):
        if bpy.context.object is not None and bpy.context.object.mode != 'OBJECT':
            bpy.ops.object.mode_set(mode='OBJECT')
        bpy.ops.object.select_all(action='DESELECT')
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob


try:
    for tipo_objeto, modo_objeto, modo_contexto, tool_prop in MODOS:
        ob = objetos.get(tipo_objeto)
        if ob is None:
            fallos.append("%s: no hay objeto %s" % (modo_contexto, tipo_objeto))
            continue
        activar(ob)
        with bpy.context.temp_override(window=win, area=area, region=region):
            bpy.ops.object.mode_set(mode=modo_objeto)
            if bpy.context.mode != modo_contexto:
                fallos.append("%s: se entro en %s" % (modo_contexto, bpy.context.mode))
                continue
            items = bpy.types.Brush.bl_rna.properties[tool_prop].enum_items
            for item in items:
                nativo = bpy.context.workspace.tools.has_tool_with_brush_type(item.value)
                pitonico = python_has_tool_with_brush_type(bpy.context, tool_prop, item.value)
                ciertos[0] += int(nativo)
                ciertos[1] += int(pitonico)
                if nativo == pitonico:
                    iguales += 1
                else:
                    distintas += 1
                    fallos.append("%s %s=%d: nativo=%r python=%r" % (
                        modo_contexto, item.identifier, item.value, nativo, pitonico))
    with bpy.context.temp_override(window=win, area=area, region=region):
        bpy.ops.object.mode_set(mode='OBJECT')

    area.type = 'IMAGE_EDITOR'
    region_img = next(r for r in area.regions if r.type == 'WINDOW')
    for modo_imagen, tool_prop in MODOS_IMAGEN:
        area.spaces.active.mode = modo_imagen
        with bpy.context.temp_override(window=win, area=area, region=region_img):
            items = bpy.types.Brush.bl_rna.properties[tool_prop].enum_items
            for item in items:
                nativo = bpy.context.workspace.tools.has_tool_with_brush_type(item.value)
                pitonico = python_has_tool_with_brush_type(bpy.context, tool_prop, item.value)
                ciertos[0] += int(nativo)
                ciertos[1] += int(pitonico)
                if nativo == pitonico:
                    iguales += 1
                else:
                    distintas += 1
                    fallos.append("IMAGE_EDITOR/%s %s=%d: nativo=%r python=%r" % (
                        modo_imagen, item.identifier, item.value, nativo, pitonico))
    area.type = 'VIEW_3D'
except Exception:
    fallos.append("EXCEPCION\n" + traceback.format_exc())

for f in fallos:
    print("  FALLO", f)
print("BRUSH_TYPE_QUERY iguales=%d distintas=%d ciertos_nativo=%d ciertos_python=%d"
      % (iguales, distintas, ciertos[0], ciertos[1]))
if ciertos[1] == 0:
    print("  FALLO la prueba es vacia: el lado Python nunca dijo que si")
    distintas += 1
sys.exit(0 if (distintas == 0 and not fallos) else 1)
