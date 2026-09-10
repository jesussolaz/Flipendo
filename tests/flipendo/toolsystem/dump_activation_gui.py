# Linea base de la ACTIVACION de herramientas.
#
# Herramienta de medida de un solo uso. No reimplementa nada: llama al
# `_activate_by_item` REAL del Python e intercepta `tool.setup(...)`, que es donde el
# Python entrega al motor los once argumentos de la herramienta. Asi la linea base es
# lo que hace el codigo, no lo que yo creo que hace.
#
# Dos cosas se fijan para que el resultado sea determinista:
#   - el modo, forzando `tools_from_context` (en uso real sale del contexto);
#   - la memoria de grupos, vaciada antes de cada activacion (en uso real depende de
#     lo que el usuario haya pulsado antes).
#
# Para `as_fallback=True` hace falta ademas una herramienta "activa": se toma la
# primera del modo. El volcado nativo usa la misma regla.
import sys
import bpy
from bl_ui.space_toolsystem_common import ToolSelectPanelHelper
import bl_ui.space_toolsystem_common as common

out_path = sys.argv[sys.argv.index("--") + 1]
SIN_MODOS = "\x00sin-modo"


class Registro:
    """Hace de `bToolRef`: guarda lo que el Python le pasa a `setup`."""

    def __init__(self, idname=""):
        self.idname = idname
        self.idname_fallback = ""
        self.kw = None

    def setup(self, **kw):
        self.kw = kw

    def gizmo_group_properties(self, _group):
        # Las propiedades de gizmo se vuelcan aparte, desde la declaracion.
        return None

    def refresh_from_context(self):
        pass


def fmt_kw(kw):
    opts = kw.get("options") or set()
    return (
        "index=%d keymap=%s cursor=%s options=%s gizmo=%s brush_type=%s data_block=%s "
        "op=%s idname_fallback=%s keymap_fallback=%s"
        % (
            kw["index"], kw["keymap"], kw["cursor"],
            ",".join(sorted(opts)) or "-", kw["gizmo_group"] or "-", kw["brush_type"],
            kw["data_block"] or "-", kw["operator"] or "-",
            kw["idname_fallback"] or "-", kw["keymap_fallback"] or "-",
        )
    )


def fmt_gizmo_props(item):
    props = item.widget_properties
    if not props:
        return "-"
    return ",".join("%s=%g" % (k, v) for k, v in props)


lineas = []
tfc_original = {}
tafc_original = ToolSelectPanelHelper._tool_active_from_context

for cls in sorted(ToolSelectPanelHelper.__subclasses__(), key=lambda c: c.bl_space_type):
    space = cls.bl_space_type
    modos = sorted(k for k in cls._tools.keys() if k is not None) or [SIN_MODOS]
    tfc = cls.tools_from_context.__func__
    tfc_original[cls] = cls.__dict__.get("tools_from_context")

    for mode in modos:
        etiqueta = "None" if mode is SIN_MODOS else mode
        cls.tools_from_context = classmethod(
            lambda c, context, mode_=None, _m=mode, _f=tfc: _f(c, context, _m))

        plano = [
            (item, index)
            for item, index in ToolSelectPanelHelper._tools_flatten_with_tool_index(
                cls.tools_from_context(bpy.context))
            if item is not None
        ]
        if not plano:
            continue
        actual = plano[0][0].idname

        for item, index in plano:
            # Activacion normal.
            cls._tool_group_active = {}  # por clase, como en el Python (:527)
            reg = Registro()
            ToolSelectPanelHelper._tool_active_from_context = staticmethod(
                lambda *a, _r=reg, **k: _r)
            common._activate_by_item(bpy.context, space, item, index)
            lineas.append("ACT %s %s %s %s gizmo_props=%s draw_cursor=%s" % (
                space, etiqueta, item.idname, fmt_kw(reg.kw), fmt_gizmo_props(item),
                "si" if item.draw_cursor is not None else "no"))

        # Activacion como herramienta de RESERVA: solo las del grupo de reserva.
        _i, _idx, grupo = cls._tool_get_by_id_active_with_group(bpy.context, cls.tool_fallback_id)
        if grupo is not None:
            for sub in grupo:
                if sub is None:
                    continue
                cls._tool_group_active = {}  # por clase, como en el Python (:527)
                reg = Registro(actual)
                ToolSelectPanelHelper._tool_active_from_context = staticmethod(
                    lambda *a, _r=reg, **k: _r)
                common._activate_by_item(bpy.context, space, sub, 0, as_fallback=True)
                lineas.append("FALLBACK %s %s %s activa=%s %s" % (
                    space, etiqueta, sub.idname, actual, fmt_kw(reg.kw)))

    if tfc_original[cls] is not None:
        cls.tools_from_context = tfc_original[cls]
    ToolSelectPanelHelper._tool_active_from_context = tafc_original

with open(out_path, "w") as f:
    f.write("\n".join(lineas) + "\n")
print("ACTIVATION_DUMP_OK", len(lineas))
bpy.ops.wm.quit_blender()
