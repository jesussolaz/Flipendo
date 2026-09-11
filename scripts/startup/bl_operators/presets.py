# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import (
    Menu,
    Operator,
    Panel,
)
from bpy.props import (
    BoolProperty,
    StringProperty,
)
from bpy.app.translations import (
    pgettext_rpt as rpt_,
    pgettext_data as data_,
)
from bl_ui.utils import PresetPanel


# `WindowManager.preset_name` (el campo de texto del popover de presets) ya NO se
# declara aqui: es una propiedad RNA nativa, registrada desde
# `source/blender/windowmanager/preset/fl_preset_add_ops.cc`, porque la dibuja y la lee
# el panel nativo de presets. Ver politicas/PRESETS-A-DATOS.md.


# -----------------------------------------------------------------------------
# Private Implementation

def _call_preset_cb(fn, context, filepath, *, deprecated="4.2"):
    # Allow "None" so the caller doesn't have to assign a variable and check it.
    if fn is None:
        return

    if hasattr(fn, "__self__"):
        args_offset = 1
    else:
        args_offset = 0

    # Support a `filepath` argument, optional for backwards compatibility.
    fn_arg_count = getattr(getattr(fn, "__code__", None), "co_argcount", None)
    if fn_arg_count == 2 + args_offset:
        args = (context, filepath)
    else:
        print("Deprecated since Blender {:s}, a filepath argument should be included in: {!r}".format(deprecated, fn))
        args = (context, )

    try:
        fn(*args)
    except Exception as ex:
        print("Internal error running", fn, str(ex))


def _is_path_readonly(path):
    from bpy.utils import (
        is_path_builtin,
        is_path_extension,
    )
    # Consider extension repository paths read-only because they should not be manipulated
    # since the only way to restore the preset is to re-install the extension.
    return is_path_builtin(path) or is_path_extension(path)


# -----------------------------------------------------------------------------
# Main Preset Implementation

class AddPresetBase:
    """Base preset class, only for subclassing
    subclasses must define
     - preset_values
     - preset_subdir """
    # bl_idname = "script.preset_base_add"
    # bl_label = "Add a Python Preset"

    # only because invoke_props_popup requires. Also do not add to search menu.
    bl_options = {'REGISTER', 'INTERNAL'}

    name: StringProperty(
        name="Name",
        description="Name of the preset, used to make the path name",
        maxlen=64,
        options={'SKIP_SAVE'},
    )
    remove_name: BoolProperty(
        default=False,
        options={'HIDDEN', 'SKIP_SAVE'},
    )
    remove_active: BoolProperty(
        default=False,
        options={'HIDDEN', 'SKIP_SAVE'},
    )

    @staticmethod
    def as_filename(name):  # could reuse for other presets

        # lazy init maketrans
        def maketrans_init():
            cls = AddPresetBase
            attr = "_as_filename_trans"

            trans = getattr(cls, attr, None)
            if trans is None:
                trans = str.maketrans({char: "_" for char in " !@#$%^&*(){}:\";'[]<>,.\\/?"})
                setattr(cls, attr, trans)
            return trans

        name = name.strip()
        name = bpy.path.display_name_to_filepath(name)
        trans = maketrans_init()
        # Strip surrounding "_" as they are displayed as spaces.
        return name.translate(trans).strip("_")

    def execute(self, context):
        import os

        if hasattr(self, "pre_cb"):
            self.pre_cb(context)

        preset_menu_class = getattr(bpy.types, self.preset_menu)

        is_xml = getattr(preset_menu_class, "preset_type", None) == 'XML'
        is_preset_add = not (self.remove_name or self.remove_active)

        if is_xml:
            ext = ".xml"
        else:
            # Los presets se guardan como DATOS (`.fpreset`), no como script.
            # Ver politicas/PRESETS-A-DATOS.md. `preset_ext` solo lo fija la
            # familia keyconfig, que no es una lista de propiedades sino un
            # generador de keymaps y sigue exportandose como `.py`.
            ext = getattr(self, "preset_ext", ".fpreset")

        name = self.name.strip() if is_preset_add else self.name

        if is_preset_add:
            if not name:
                return {'FINISHED'}

            # Reset preset name
            wm = bpy.data.window_managers[0]
            if name == wm.preset_name:
                wm.preset_name = data_("New Preset")

            filename = self.as_filename(name)

            target_path = os.path.join("presets", self.preset_subdir)
            target_path = bpy.utils.user_resource('SCRIPTS', path=target_path, create=True)

            if not target_path:
                self.report({'WARNING'}, "Failed to create presets path")
                return {'CANCELLED'}

            filepath = os.path.join(target_path, filename) + ext

            if hasattr(self, "add"):
                self.add(context, filepath)
            else:
                print("Writing Preset: {!r}".format(filepath))

                if is_xml:
                    import rna_xml
                    rna_xml.xml_file_write(context, filepath, preset_menu_class.preset_xml_map)
                else:
                    # Escribir el preset ya no es metaprogramacion: la lista de
                    # rutas de cada familia es una tabla en C++
                    # (`fl_preset_spec.cc`) y `WM_OT_preset_write` captura los
                    # valores con RNA y los serializa. Ni `exec`, ni `eval`, ni
                    # `repr`. Ver politicas/PRESETS-A-DATOS.md.
                    try:
                        bpy.ops.wm.preset_write(
                            filepath=filepath,
                            subdir=self.preset_subdir,
                            use_focal_length=getattr(self, "use_focal_length", False),
                        )
                    except Exception as ex:
                        self.report({'ERROR'}, rpt_("Unable to write preset: {!r}").format(ex))
                        return {'CANCELLED'}

            preset_menu_class.bl_label = bpy.path.display_name(filename)

        else:
            if self.remove_active:
                name = preset_menu_class.bl_label

            # fairly sloppy but convenient.
            # Se busca primero con la extension de hoy y luego con `.py`: un
            # preset que el usuario guardo antes de la migracion se tiene que
            # poder borrar igual.
            filepath = ""
            for ext_try in (ext, ".py") if ext == ".fpreset" else (ext,):
                filepath = bpy.utils.preset_find(name, self.preset_subdir, ext=ext_try)
                if not filepath:
                    filepath = bpy.utils.preset_find(
                        name, self.preset_subdir, display_name=True, ext=ext_try)
                if filepath:
                    break

            if not filepath:
                return {'CANCELLED'}

            # Do not remove bundled presets
            if _is_path_readonly(filepath):
                self.report({'WARNING'}, "Unable to remove default presets")
                return {'CANCELLED'}

            try:
                if hasattr(self, "remove"):
                    self.remove(context, filepath)
                else:
                    os.remove(filepath)
            except Exception as ex:
                self.report({'ERROR'}, rpt_("Unable to remove preset: {!r}").format(ex))
                import traceback
                traceback.print_exc()
                return {'CANCELLED'}

            # XXX, stupid!
            preset_menu_class.bl_label = "Presets"

        _call_preset_cb(getattr(self, "post_cb", None), context, filepath, deprecated="4.3")

        return {'FINISHED'}

    def check(self, _context):
        self.name = self.as_filename(self.name.strip())

    def invoke(self, context, _event):
        if not (self.remove_active or self.remove_name):
            wm = context.window_manager
            return wm.invoke_props_dialog(self)
        else:
            return self.execute(context)


class ExecutePreset(Operator):
    """Load a preset"""
    bl_idname = "script.execute_preset"
    bl_label = "Execute a Python Preset"

    filepath: StringProperty(
        subtype='FILE_PATH',
        options={'SKIP_SAVE'},
    )
    menu_idname: StringProperty(
        name="Menu ID Name",
        description="ID name of the menu this was called from",
        options={'SKIP_SAVE'},
    )

    def execute(self, context):
        from os.path import basename, splitext
        filepath = self.filepath

        # change the menu title to the most recently chosen option
        preset_class = getattr(bpy.types, self.menu_idname)
        preset_class.bl_label = bpy.path.display_name(basename(filepath), title_case=False)

        ext = splitext(filepath)[1].lower()

        if ext not in {".fpreset", ".py", ".xml"}:
            self.report({'ERROR'}, rpt_("Unknown file type: {!r}").format(ext))
            return {'CANCELLED'}

        _call_preset_cb(getattr(preset_class, "reset_cb", None), context, filepath)

        if ext in {".fpreset", ".py"}:
            # Un preset ya no se EJECUTA: se lee como dato y se aplica desde C++
            # (`WM_OT_preset_apply`). Un `.py` heredado -- los que el usuario
            # tenga guardados de antes -- se analiza igualmente de forma nativa,
            # sin interprete. Ver politicas/PRESETS-A-DATOS.md.
            try:
                bpy.ops.wm.preset_apply(filepath=filepath)
            except Exception as ex:
                if ext != ".py":
                    self.report({'ERROR'}, "Failed to execute the preset: " + repr(ex))
                else:
                    # Ultimo recurso, y solo para un `.py`: el lector nativo solo
                    # entiende asignaciones, y un preset con logica de verdad
                    # -- los cinco de FFmpeg con su condicional NTSC/PAL, o uno
                    # que el usuario escribiera a mano -- lo rechaza. Mientras no
                    # tenga sustituto nativo NO se puede perder: se ejecuta como
                    # antes. Es el ultimo puente C++ -> Python de los presets, y
                    # esta contado como deuda 2 en politicas/PRESETS-A-DATOS.md.
                    print("Preset no convertible a datos, se ejecuta como script:", filepath)
                    try:
                        bpy.utils.execfile(filepath)
                    except Exception as ex_exec:
                        self.report({'ERROR'}, "Failed to execute the preset: " + repr(ex_exec))

        elif ext == ".xml":
            import rna_xml
            preset_xml_map = preset_class.preset_xml_map
            preset_xml_secure_types = getattr(preset_class, "preset_xml_secure_types", None)

            rna_xml.xml_file_run(context, filepath, preset_xml_map, secure_types=preset_xml_secure_types)

        _call_preset_cb(getattr(preset_class, "post_cb", None), context, filepath)

        return {'FINISHED'}


class AddPresetInterfaceTheme(AddPresetBase, Operator):
    """Add a custom theme to the preset list"""
    bl_idname = "wm.interface_theme_preset_add"
    bl_label = "Add Theme"
    preset_menu = "USERPREF_MT_interface_theme_presets"
    preset_subdir = "interface_theme"

    def post_cb(self, context, filepath):
        # Ensure the saved preset is considered "active" after saving.
        # Typically handled by the classes `bl_label` however themes use the `filepath` instead.
        context.preferences.themes[0].filepath = filepath


class RemovePresetInterfaceTheme(AddPresetBase, Operator):
    """Remove a custom theme from the preset list"""
    bl_idname = "wm.interface_theme_preset_remove"
    bl_label = "Remove Theme"
    preset_menu = "USERPREF_MT_interface_theme_presets"
    preset_subdir = "interface_theme"

    remove_active: BoolProperty(
        default=True,
        options={'HIDDEN', 'SKIP_SAVE'},
    )

    # NOTE: leave poll unset as file-system scanning should be avoided
    # while redrawing as it may involve remote file-system access.

    def invoke(self, context, event):
        filepath = context.preferences.themes[0].filepath
        if (not filepath) or _is_path_readonly(filepath):
            self.report({'ERROR'}, "Built-in themes cannot be removed")
            return {'CANCELLED'}

        return context.window_manager.invoke_confirm(self, event, title="Remove Custom Theme", confirm_text="Delete")

    def post_cb(self, context, _filepath):
        # Without this, the name & colors are kept after removing the theme.
        # Even though the theme is removed from the list, it's seems like a bug to keep it displayed after removal.
        bpy.ops.preferences.reset_default_theme()


class SavePresetInterfaceTheme(AddPresetBase, Operator):
    """Save a custom theme in the preset list"""
    bl_idname = "wm.interface_theme_preset_save"
    bl_label = "Save Theme"
    preset_menu = "USERPREF_MT_interface_theme_presets"
    preset_subdir = "interface_theme"

    remove_active: BoolProperty(
        default=True,
        options={'HIDDEN', 'SKIP_SAVE'},
    )

    # NOTE: leave poll unset as file-system scanning should be avoided
    # while redrawing as it may involve remote file-system access.

    def execute(self, context):
        import rna_xml
        filepath = context.preferences.themes[0].filepath
        if (not filepath) or _is_path_readonly(filepath):
            self.report({'ERROR'}, "Built-in themes cannot be overwritten")
            return {'CANCELLED'}

        preset_menu_class = getattr(bpy.types, self.preset_menu)
        try:
            rna_xml.xml_file_write(context, filepath, preset_menu_class.preset_xml_map)
        except Exception as ex:
            self.report({'ERROR'}, "Unable to overwrite preset: {:s}".format(str(ex)))
            import traceback
            traceback.print_exc()
            return {'CANCELLED'}

        context.preferences.themes[0].filepath = filepath

        return {'FINISHED'}

    def invoke(self, context, event):
        filepath = context.preferences.themes[0].filepath
        if (not filepath) or _is_path_readonly(filepath):
            self.report({'ERROR'}, "Built-in themes cannot be overwritten")
            return {'CANCELLED'}

        return context.window_manager.invoke_confirm(self, event, title="Overwrite Custom Theme?", confirm_text="Save")


def _operator_path(operator):
    # Era `AddPresetOperator.operator_path`. La clase se fue a C++
    # (`fl_preset_add_ops.cc::operator_subdir`); esta copia queda solo mientras los dos
    # menus de abajo sigan siendo clases de Python.
    import os
    prefix, suffix = operator.split("_OT_", 1)
    return os.path.join("operator", "{:s}.{:s}".format(prefix.lower(), suffix))


class WM_MT_operator_presets(Menu):
    bl_label = "Operator Presets"

    def draw(self, context):
        self.operator = context.active_operator.bl_idname

        # dummy 'default' menu item
        layout = self.layout
        layout.operator("wm.operator_defaults")
        layout.separator()

        Menu.draw_preset(self, context)

    @property
    def preset_subdir(self):
        return _operator_path(self.operator)

    preset_operator = "script.execute_preset"


class WM_PT_operator_presets(PresetPanel, Panel):
    bl_label = "Operator Presets"
    preset_add_operator = "wm.operator_preset_add"
    preset_operator = "script.execute_preset"

    @property
    def preset_subdir(self):
        return _operator_path(self.operator)

    @property
    def preset_add_operator_properties(self):
        return {"operator": self.operator}

    def draw(self, context):
        self.operator = context.active_operator.bl_idname
        PresetPanel.draw(self, context)


classes = (
    AddPresetInterfaceTheme,
    RemovePresetInterfaceTheme,
    SavePresetInterfaceTheme,
    ExecutePreset,
    WM_MT_operator_presets,
    WM_PT_operator_presets,
)
