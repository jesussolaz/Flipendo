# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

from bpy.types import (
    AssetShelf,
    Header,
    Menu,
    Panel,
    UIList,
)
from bl_ui.properties_paint_common import (
    UnifiedPaintPanel,
    brush_texture_settings,
    brush_basic_texpaint_settings,
    brush_settings,
    brush_settings_advanced,
    draw_color_settings,
    ClonePanel,
    BrushSelectPanel,
    TextureMaskPanel,
    ColorPalettePanel,
    StrokePanel,
    SmoothStrokePanel,
    FalloffPanel,
    DisplayPanel,
    BrushAssetShelf,
)
from bl_ui.properties_grease_pencil_common import (
    AnnotationDataPanel,
)
from bpy.app.translations import (
    contexts as i18n_contexts,
    pgettext_iface as iface_,
)


class ImagePaintPanel:
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'


class BrushButtonsPanel(UnifiedPaintPanel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'

    @classmethod
    def poll(cls, context):
        tool_settings = context.tool_settings.image_paint
        return tool_settings.brush


class IMAGE_PT_active_tool(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Tool"
    bl_label = "Active Tool"

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        layout.use_property_decorate = False
        # La cabecera de la herramienta activa: dibujo nativo (C++, `FL_toolbar_ui.hh`).
        layout.column().template_tool_header(
            show_tool_icon_always=True,
            space_type='IMAGE_EDITOR',
        )


class IMAGE_HT_tool_header(Header):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'TOOL_HEADER'

    def draw(self, context):
        layout = self.layout

        self.draw_tool_settings(context)

        layout.separator_spacer()

        self.draw_mode_settings(context)

    def draw_tool_settings(self, context):
        layout = self.layout

        # Active Tool
        # -----------
        # Dibujo nativo (C++, `FL_toolbar_ui.hh`).
        tool = layout.template_tool_header()
        tool_mode = context.mode if tool is None else tool.mode

        # Object Mode Options
        # -------------------

        # Example of how tool_settings can be accessed as pop-overs.

        # TODO(campbell): editing options should be after active tool options
        # (obviously separated for from the users POV)
        draw_fn = getattr(_draw_tool_settings_context_mode, tool_mode, None)
        if draw_fn is not None:
            draw_fn(context, layout, tool)

        if tool_mode == 'PAINT':
            if (tool is not None) and tool.use_brushes:
                layout.popover("IMAGE_PT_paint_settings_advanced")
                layout.popover("IMAGE_PT_paint_stroke")
                layout.popover("IMAGE_PT_paint_curve")
                layout.popover("IMAGE_PT_tools_brush_display")
                layout.popover("IMAGE_PT_tools_brush_texture")
                layout.popover("IMAGE_PT_tools_mask_texture")

    def draw_mode_settings(self, context):
        layout = self.layout

        # Active Tool
        # -----------
        tool = context.workspace.tools.from_active_space()
        tool_mode = context.mode if tool is None else tool.mode

        if tool_mode == 'PAINT':
            layout.popover_group(space_type='IMAGE_EDITOR', region_type='UI', context=".imagepaint_2d", category="")


class _draw_tool_settings_context_mode:
    @staticmethod
    def UV(context, layout, tool):
        if tool and tool.use_brushes:
            if context.mode == 'EDIT_MESH':
                tool_settings = context.tool_settings
                uv_sculpt = tool_settings.uv_sculpt
                brush = uv_sculpt.brush
                if brush:
                    UnifiedPaintPanel.prop_unified(
                        layout,
                        context,
                        brush,
                        "size",
                        pressure_name="use_pressure_size",
                        unified_name="use_unified_size",
                        slider=True,
                        header=True,
                    )
                    UnifiedPaintPanel.prop_unified(
                        layout,
                        context,
                        brush,
                        "strength",
                        pressure_name="use_pressure_strength",
                        unified_name="use_unified_strength",
                        slider=True,
                        header=True,
                    )

    @staticmethod
    def PAINT(context, layout, tool):
        if (tool is None) or (not tool.use_brushes):
            return

        paint = context.tool_settings.image_paint
        brush = paint.brush

        BrushAssetShelf.draw_popup_selector(layout, context, brush)

        if brush is None:
            return

        brush_basic_texpaint_settings(layout, context, brush, compact=True)


class IMAGE_HT_header(Header):
    bl_space_type = 'IMAGE_EDITOR'

    @staticmethod
    def draw_xform_template(layout, context):
        sima = context.space_data
        show_uvedit = sima.show_uvedit

        if show_uvedit:
            layout.prop(sima, "pivot_point", icon_only=True)

        if show_uvedit:
            tool_settings = context.tool_settings

            # Snap.
            snap_uv_elements = tool_settings.snap_uv_element
            if len(snap_uv_elements) == 1:
                text = ""
                elem = next(iter(snap_uv_elements))
                act_snap_icon = tool_settings.bl_rna.properties["snap_uv_element"].enum_items[elem].icon
            else:
                text = iface_("Mix", i18n_contexts.id_image)
                act_snap_icon = 'NONE'

            row = layout.row(align=True)
            row.prop(tool_settings, "use_snap_uv", text="")

            sub = row.row(align=True)
            sub.popover(
                panel="IMAGE_PT_snapping",
                icon=act_snap_icon,
                text=text,
            )

            # Proportional Editing
            row = layout.row(align=True)
            row.prop(
                tool_settings,
                "use_proportional_edit",
                icon_only=True,
                icon='PROP_CON' if tool_settings.use_proportional_connected else 'PROP_ON',
            )
            sub = row.row(align=True)
            sub.active = tool_settings.use_proportional_edit
            sub.prop_with_popover(
                tool_settings,
                "proportional_edit_falloff",
                text="",
                icon_only=True,
                panel="IMAGE_PT_proportional_edit",
            )

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        overlay = sima.overlay
        ima = sima.image
        iuser = sima.image_user
        tool_settings = context.tool_settings

        show_render = sima.show_render
        show_uvedit = sima.show_uvedit
        show_maskedit = sima.show_maskedit

        layout.template_header()

        if sima.mode != 'UV':
            layout.prop(sima, "ui_mode", text="")

        # UV editing.
        if show_uvedit:
            layout.prop(tool_settings, "use_uv_select_sync", text="")

            if tool_settings.use_uv_select_sync:
                layout.template_edit_mode_selection()

                # Currently this only works for edge-select & face-select modes.
                row = layout.row()
                if tool_settings.mesh_select_mode[0]:
                    row.active = False
                row.prop(tool_settings, "uv_sticky_select_mode", icon_only=True)
            else:
                row = layout.row(align=True)
                uv_select_mode = tool_settings.uv_select_mode[:]
                row.operator("uv.select_mode", text="", icon='UV_VERTEXSEL',
                             depress=(uv_select_mode == 'VERTEX')).type = 'VERTEX'
                row.operator("uv.select_mode", text="", icon='UV_EDGESEL',
                             depress=(uv_select_mode == 'EDGE')).type = 'EDGE'
                row.operator("uv.select_mode", text="", icon='UV_FACESEL',
                             depress=(uv_select_mode == 'FACE')).type = 'FACE'
                row.operator("uv.select_mode", text="", icon='UV_ISLANDSEL',
                             depress=(uv_select_mode == 'ISLAND')).type = 'ISLAND'

                layout.prop(tool_settings, "uv_sticky_select_mode", icon_only=True)

        IMAGE_MT_editor_menus.draw_collapsible(context, layout)

        layout.separator_spacer()

        IMAGE_HT_header.draw_xform_template(layout, context)

        layout.template_ID(sima, "image", new="image.new", open="image.open")

        if show_maskedit:
            layout.template_ID(sima, "mask", new="mask.new")
            layout.prop(sima, "pivot_point", icon_only=True)

            row = layout.row(align=True)
            row.prop(tool_settings, "use_proportional_edit_mask", text="", icon_only=True)
            sub = row.row(align=True)
            sub.active = tool_settings.use_proportional_edit_mask
            sub.prop_with_popover(
                tool_settings,
                "proportional_edit_falloff",
                text="",
                icon_only=True,
                panel="IMAGE_PT_proportional_edit",
            )

        if not show_render:
            layout.prop(sima, "use_image_pin", text="", emboss=False)

        layout.separator_spacer()

        # Gizmo toggle & popover.
        row = layout.row(align=True)
        row.prop(sima, "show_gizmo", icon='GIZMO', text="")
        sub = row.row(align=True)
        sub.active = sima.show_gizmo
        sub.popover(panel="IMAGE_PT_gizmo_display", text="")

        # Overlay toggle & popover
        row = layout.row(align=True)
        row.prop(overlay, "show_overlays", icon='OVERLAY', text="")
        sub = row.row(align=True)
        sub.active = overlay.show_overlays
        sub.popover(panel="IMAGE_PT_overlay", text="")

        if show_uvedit:
            mesh = context.edit_object.data
            layout.prop_search(mesh.uv_layers, "active", mesh, "uv_layers", text="")

        if ima:
            if ima.is_stereo_3d:
                row = layout.row()
                row.prop(sima, "show_stereo_3d", text="")
            if show_maskedit:
                row = layout.row()
                row.popover(panel="IMAGE_PT_mask_display")

            # layers.
            layout.template_image_layers(ima, iuser)

            # draw options.
            row = layout.row()
            row.prop(sima, "display_channels", icon_only=True)


class IMAGE_MT_editor_menus(Menu):
    bl_idname = "IMAGE_MT_editor_menus"
    bl_label = ""

    def draw(self, context):
        layout = self.layout
        sima = context.space_data
        ima = sima.image

        show_uvedit = sima.show_uvedit
        show_maskedit = sima.show_maskedit

        layout.menu("IMAGE_MT_view")

        if show_uvedit:
            layout.menu("IMAGE_MT_select")
        if show_maskedit:
            layout.menu("MASK_MT_select")

        if ima and ima.is_dirty:
            layout.menu("IMAGE_MT_image", text="Image*")
        else:
            layout.menu("IMAGE_MT_image", text="Image")

        if show_uvedit:
            layout.menu("IMAGE_MT_uvs")
        if show_maskedit:
            layout.menu("MASK_MT_add")
            layout.menu("MASK_MT_mask")


# -----------------------------------------------------------------------------
# Mask (similar code in space_clip.py, keep in sync)
# note! - panel placement does _not_ fit well with image panels... need to fix.

from bl_ui.properties_mask_common import (
    MASK_PT_mask,
    MASK_PT_layers,
    MASK_PT_spline,
    MASK_PT_point,
    MASK_PT_animation,
    MASK_PT_display,
)


class IMAGE_PT_mask(MASK_PT_mask, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Mask"


class IMAGE_PT_mask_layers(MASK_PT_layers, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Mask"


class IMAGE_PT_active_mask_spline(MASK_PT_spline, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Mask"


class IMAGE_PT_active_mask_point(MASK_PT_point, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Mask"


class IMAGE_PT_mask_animation(MASK_PT_animation, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Mask"


class IMAGE_PT_mask_display(MASK_PT_display, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'HEADER'


# --- end mask ---

class IMAGE_PT_snapping(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Snapping"

    def draw(self, context):
        tool_settings = context.tool_settings

        layout = self.layout
        col = layout.column()
        col.label(text="Snap Target")
        col.prop(tool_settings, "snap_uv_element", expand=True)

        col.label(text="Snap Base")
        row = col.row(align=True)
        row.active = bool(tool_settings.snap_uv_element.difference({'INCREMENT', 'GRID'}))
        row.prop(tool_settings, "snap_target", expand=True)

        col.separator()

        col.label(text="Affect")
        row = col.row(align=True)
        row.prop(
            tool_settings,
            "use_snap_translate",
            text="Move",
            text_ctxt=i18n_contexts.operator_default,
            toggle=True,
        )
        row.prop(tool_settings, "use_snap_rotate", text="Rotate", text_ctxt=i18n_contexts.operator_default, toggle=True)
        row.prop(tool_settings, "use_snap_scale", text="Scale", text_ctxt=i18n_contexts.operator_default, toggle=True)
        col.label(text="Rotation Increment")
        row = col.row(align=True)
        row.prop(tool_settings, "snap_angle_increment_2d", text="")
        row.prop(tool_settings, "snap_angle_increment_2d_precision", text="")


class IMAGE_PT_proportional_edit(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Proportional Editing"
    bl_ui_units_x = 8

    def draw(self, context):
        layout = self.layout
        tool_settings = context.tool_settings
        col = layout.column()

        col.prop(tool_settings, "use_proportional_connected")
        col.separator()

        col.prop(tool_settings, "proportional_edit_falloff", expand=True)
        col.prop(tool_settings, "proportional_size")


class IMAGE_PT_image_properties(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Image"
    bl_label = "Image"

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        return (sima.image)

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        iuser = sima.image_user

        layout.template_image(sima, "image", iuser, multiview=True)


class IMAGE_PT_view_display(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_label = "Display"
    bl_category = "View"

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        return (sima and (sima.image or sima.show_uvedit))

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True

        sima = context.space_data
        ima = sima.image

        show_uvedit = sima.show_uvedit
        uvedit = sima.uv_editor

        col = layout.column()

        if ima:
            col.prop(ima, "display_aspect", text="Aspect Ratio")
            row = col.row()
            row.active = ima.source != 'TILED'
            row.prop(sima, "show_repeat", text="Repeat Image")

        if show_uvedit:
            col.prop(uvedit, "show_pixel_coords", text="Pixel Coordinates")


class IMAGE_UL_render_slots(UIList):
    def draw_item(self, _context, layout, _data, item, _icon, _active_data, _active_propname, _index):
        slot = item
        layout.prop(slot, "name", text="", emboss=False)


class IMAGE_PT_render_slots(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Image"
    bl_label = "Render Slots"

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        return (sima and sima.image and sima.show_render)

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        ima = sima.image

        row = layout.row()

        col = row.column()
        col.template_list(
            "IMAGE_UL_render_slots", "render_slots", ima,
            "render_slots", ima.render_slots, "active_index", rows=3,
        )

        col = row.column(align=True)
        col.operator("image.add_render_slot", icon='ADD', text="")
        col.operator("image.remove_render_slot", icon='REMOVE', text="")

        col.separator()

        col.operator("image.clear_render_slot", icon='X', text="")


class IMAGE_UL_udim_tiles(UIList):
    def draw_item(self, _context, layout, _data, item, _icon, _active_data, _active_propname, _index):
        tile = item
        layout.prop(tile, "label", text="", emboss=False)


class IMAGE_PT_udim_tiles(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Image"
    bl_label = "UDIM Tiles"

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        return (sima and sima.image and sima.image.source == 'TILED')

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        ima = sima.image

        row = layout.row()
        col = row.column()
        col.template_list("IMAGE_UL_udim_tiles", "", ima, "tiles", ima.tiles, "active_index", rows=4)

        col = row.column()
        sub = col.column(align=True)
        sub.operator("image.tile_add", icon='ADD', text="")
        sub.operator("image.tile_remove", icon='REMOVE', text="")

        tile = ima.tiles.active
        if tile:
            col = layout.column(align=True)
            col.operator("image.tile_fill")


class IMAGE_PT_paint_select(Panel, ImagePaintPanel, BrushSelectPanel):
    bl_label = "Brush Asset"
    bl_context = ".paint_common_2d"
    bl_category = "Tool"


class IMAGE_PT_paint_settings(Panel, ImagePaintPanel):
    bl_context = ".paint_common_2d"
    bl_category = "Tool"
    bl_label = "Brush Settings"

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True
        layout.use_property_decorate = False

        settings = context.tool_settings.image_paint
        brush = settings.brush

        if brush:
            brush_settings(layout.column(), context, brush, popover=self.is_popover)


class IMAGE_PT_paint_settings_advanced(Panel, ImagePaintPanel):
    bl_context = ".paint_common_2d"
    bl_parent_id = "IMAGE_PT_paint_settings"
    bl_category = "Tool"
    bl_label = "Advanced"
    bl_ui_units_x = 12

    def draw(self, context):
        layout = self.layout

        layout.use_property_split = True
        layout.use_property_decorate = False  # No animation.

        settings = context.tool_settings.image_paint
        brush = settings.brush
        if brush:
            brush_settings_advanced(layout.column(), context, settings, brush, self.is_popover)


class IMAGE_PT_paint_color(Panel, ImagePaintPanel):
    bl_context = ".paint_common_2d"
    bl_parent_id = "IMAGE_PT_paint_settings"
    bl_category = "Tool"
    bl_label = "Color Picker"

    @classmethod
    def poll(cls, context):
        settings = context.tool_settings.image_paint
        brush = settings.brush
        if not brush:
            return False
        capabilities = brush.image_paint_capabilities
        return capabilities.has_color

    def draw(self, context):
        layout = self.layout
        settings = context.tool_settings.image_paint
        brush = settings.brush
        if brush:
            draw_color_settings(context, layout, brush, color_type=True)


class IMAGE_PT_paint_swatches(Panel, ImagePaintPanel, ColorPalettePanel):
    bl_category = "Tool"
    bl_context = ".paint_common_2d"
    bl_parent_id = "IMAGE_PT_paint_settings"
    bl_label = "Color Palette"
    bl_options = {'DEFAULT_CLOSED'}


class IMAGE_PT_paint_clone(Panel, ImagePaintPanel, ClonePanel):
    bl_category = "Tool"
    bl_context = ".paint_common_2d"
    bl_parent_id = "IMAGE_PT_paint_settings"
    bl_label = "Clone from Image/UV Map"


class IMAGE_PT_tools_brush_display(Panel, BrushButtonsPanel, DisplayPanel):
    bl_context = ".paint_common_2d"
    bl_parent_id = "IMAGE_PT_paint_settings"
    bl_category = "Tool"
    bl_label = "Cursor"
    bl_options = {'DEFAULT_CLOSED'}
    bl_ui_units_x = 15


class IMAGE_PT_tools_brush_texture(BrushButtonsPanel, Panel):
    bl_label = "Texture"
    bl_context = ".paint_common_2d"
    bl_parent_id = "IMAGE_PT_paint_settings"
    bl_category = "Tool"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        tool_settings = context.tool_settings.image_paint
        brush = tool_settings.brush
        tex_slot = brush.texture_slot

        col = layout.column()
        col.template_ID_preview(tex_slot, "texture", new="texture.new", rows=3, cols=8)

        brush_texture_settings(col, brush, 0)


class IMAGE_PT_tools_mask_texture(Panel, BrushButtonsPanel, TextureMaskPanel):
    bl_context = ".paint_common_2d"
    bl_parent_id = "IMAGE_PT_paint_settings"
    bl_category = "Tool"
    bl_label = "Texture Mask"
    bl_ui_units_x = 12


class IMAGE_PT_paint_stroke(BrushButtonsPanel, Panel, StrokePanel):
    bl_label = "Stroke"
    bl_context = ".paint_common_2d"
    bl_parent_id = "IMAGE_PT_paint_settings"
    bl_category = "Tool"
    bl_options = {'DEFAULT_CLOSED'}


class IMAGE_PT_paint_stroke_smooth_stroke(Panel, BrushButtonsPanel, SmoothStrokePanel):
    bl_context = ".paint_common_2d"
    bl_label = "Stabilize Stroke"
    bl_parent_id = "IMAGE_PT_paint_stroke"
    bl_category = "Tool"
    bl_options = {'DEFAULT_CLOSED'}


class IMAGE_PT_paint_curve(BrushButtonsPanel, Panel, FalloffPanel):
    bl_label = "Falloff"
    bl_context = ".paint_common_2d"
    bl_parent_id = "IMAGE_PT_paint_settings"
    bl_category = "Tool"
    bl_options = {'DEFAULT_CLOSED'}


class IMAGE_PT_tools_imagepaint_symmetry(BrushButtonsPanel, Panel):
    bl_context = ".imagepaint_2d"
    bl_label = "Tiling"
    bl_category = "Tool"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout

        tool_settings = context.tool_settings
        ipaint = tool_settings.image_paint

        col = layout.column(align=True)
        row = col.row(align=True)
        row.prop(ipaint, "tile_x", text="X", toggle=True)
        row.prop(ipaint, "tile_y", text="Y", toggle=True)


# Only a popover.
class IMAGE_PT_uv_sculpt_curve(Panel):
    bl_space_type = 'TOPBAR'  # dummy.
    bl_region_type = 'HEADER'

    bl_context = ".uv_sculpt"  # Dot on purpose (access from top-bar).
    bl_label = "Falloff"

    def draw(self, context):
        layout = self.layout
        props = context.scene.tool_settings.uv_sculpt

        col = layout.column()
        col.prop(props, "curve_preset", expand=True)

        if props.curve_preset == 'CUSTOM':
            col = layout.column()
            col.template_curve_mapping(props, "strength_curve")


# Only a popover.
class IMAGE_PT_uv_sculpt_options(Panel):
    bl_space_type = 'TOPBAR'  # dummy.
    bl_region_type = 'HEADER'

    bl_context = ".uv_sculpt"  # Dot on purpose (access from top-bar).
    bl_label = "Options"

    def draw(self, context):
        layout = self.layout

        tool_settings = context.tool_settings

        col = layout.column()
        col.prop(tool_settings, "uv_sculpt_lock_borders")
        col.prop(tool_settings, "uv_sculpt_all_islands")


class ImageScopesPanel:
    @classmethod
    def poll(cls, context):
        sima = context.space_data

        if not (sima and sima.image):
            return False

        # scopes are not updated in paint modes, hide.
        if sima.mode == 'PAINT':
            return False

        ob = context.active_object
        if ob and ob.mode in {'TEXTURE_PAINT', 'EDIT'}:
            return False

        return True


class IMAGE_PT_view_histogram(ImageScopesPanel, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Scopes"
    bl_label = "Histogram"

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        hist = sima.scopes.histogram

        layout.template_histogram(sima.scopes, "histogram")

        row = layout.row(align=True)
        row.prop(hist, "mode", expand=True)
        row.prop(hist, "show_line", text="")


class IMAGE_PT_view_waveform(ImageScopesPanel, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Scopes"
    bl_label = "Waveform"

    def draw(self, context):
        layout = self.layout

        sima = context.space_data

        layout.template_waveform(sima, "scopes")
        row = layout.split(factor=0.75)
        row.prop(sima.scopes, "waveform_alpha")
        row.prop(sima.scopes, "waveform_mode", text="")


class IMAGE_PT_view_vectorscope(ImageScopesPanel, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Scopes"
    bl_label = "Vectorscope"

    def draw(self, context):
        layout = self.layout

        sima = context.space_data

        layout.template_vectorscope(sima, "scopes")
        row = layout.split(factor=0.75)
        row.prop(sima.scopes, "vectorscope_alpha")
        row.prop(sima.scopes, "vectorscope_mode", text="")


class IMAGE_PT_sample_line(ImageScopesPanel, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Scopes"
    bl_label = "Sample Line"

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        hist = sima.sample_histogram

        layout.operator("image.sample_line")
        layout.template_histogram(sima, "sample_histogram")

        row = layout.row(align=True)
        row.prop(hist, "mode", expand=True)
        row.prop(hist, "show_line", text="")


class IMAGE_PT_scope_sample(ImageScopesPanel, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "Scopes"
    bl_label = "Samples"
    bl_options = {'DEFAULT_CLOSED'}

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = True
        flow = layout.grid_flow(row_major=True, columns=0, even_columns=True, even_rows=False, align=True)

        sima = context.space_data

        col = flow.column()
        col.prop(sima.scopes, "use_full_resolution")

        col = flow.column()
        col.active = not sima.scopes.use_full_resolution
        col.prop(sima.scopes, "accuracy")


class IMAGE_PT_uv_cursor(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "View"
    bl_label = "2D Cursor"

    @classmethod
    def poll(cls, context):
        sima = context.space_data

        return (sima and (sima.show_uvedit or sima.show_maskedit))

    def draw(self, context):
        layout = self.layout

        sima = context.space_data

        layout.use_property_split = True
        layout.use_property_decorate = False

        col = layout.column()
        col.prop(sima, "cursor_location", text="Location")


class IMAGE_PT_gizmo_display(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Gizmos"
    bl_ui_units_x = 8

    def draw(self, context):
        layout = self.layout

        view = context.space_data

        col = layout.column()
        col.label(text="Viewport Gizmos")
        col.separator()

        col.active = view.show_gizmo
        colsub = col.column()
        colsub.prop(view, "show_gizmo_navigate", text="Navigate")


class IMAGE_PT_overlay(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Overlays"
    bl_ui_units_x = 14

    def draw(self, context):
        pass


class IMAGE_PT_overlay_guides(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Guides"
    bl_parent_id = "IMAGE_PT_overlay"

    @classmethod
    def poll(cls, context):
        sima = context.space_data

        return sima.show_uvedit

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        overlay = sima.overlay
        uvedit = sima.uv_editor

        layout.active = overlay.show_overlays

        row = layout.row()
        row.prop(overlay, "show_grid_background", text="Grid")

        if overlay.show_grid_background:
            sub = row.row()
            sub.prop(uvedit, "show_grid_over_image", text="Over Image")
            sub.active = sima.image is not None

            layout.row().prop(uvedit, "grid_shape_source", expand=True)

            layout.use_property_split = True
            layout.use_property_decorate = False

            row = layout.row()
            row.prop(uvedit, "custom_grid_subdivisions", text="Fixed Subdivisions")
            row.active = uvedit.grid_shape_source == 'FIXED'

            layout.prop(uvedit, "tile_grid_shape", text="Tiles")


class IMAGE_PT_overlay_uv_stretch(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "UV Stretch"
    bl_parent_id = "IMAGE_PT_overlay"

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        return (sima and (sima.show_uvedit))

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        uvedit = sima.uv_editor
        overlay = sima.overlay

        layout.active = overlay.show_overlays

        row = layout.row(align=True)
        row.row().prop(uvedit, "show_stretch", text="")
        subrow = row.row()
        subrow.active = uvedit.show_stretch
        subrow.prop(uvedit, "display_stretch_type", text="")
        subrow.prop(uvedit, "stretch_opacity", text="Opacity")


class IMAGE_PT_overlay_uv_edit_geometry(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Geometry"
    bl_parent_id = "IMAGE_PT_overlay"

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        return (sima and (sima.show_uvedit))

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        uvedit = sima.uv_editor
        overlay = sima.overlay

        layout.active = overlay.show_overlays

        # Edges
        col = layout.column()
        col.prop(uvedit, "uv_opacity")
        col.prop(uvedit, "edge_display_type", text="")
        col.prop(uvedit, "show_modified_edges", text="Modified Edges")

        # Faces
        row = col.row()
        row.active = not uvedit.show_stretch
        row.prop(uvedit, "show_faces", text="Faces")


class IMAGE_PT_overlay_uv_display(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Geometry"
    bl_parent_id = "IMAGE_PT_overlay"

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        return (sima and sima.mode in {'UV', 'PAINT'} and not (sima.show_uvedit or sima.show_render))

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        uvedit = sima.uv_editor
        overlay = sima.overlay

        layout.active = overlay.show_overlays
        layout.prop(uvedit, "show_uv")
        layout.prop(uvedit, "uv_face_opacity")


class IMAGE_PT_overlay_image(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Image"
    bl_parent_id = "IMAGE_PT_overlay"

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        uvedit = sima.uv_editor
        overlay = sima.overlay

        layout.active = overlay.show_overlays
        layout.prop(uvedit, "show_metadata")


class IMAGE_PT_overlay_render_guides(Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'HEADER'
    bl_label = "Guides"
    bl_parent_id = "IMAGE_PT_overlay"

    @classmethod
    def poll(cls, context):
        sima = context.space_data
        return (
            (sima.mode in {'MASK', 'VIEW'}) and
            (image := sima.image) is not None and
            (image.source == 'VIEWER') and
            (image.type == 'COMPOSITING')
        )

    def draw(self, context):
        layout = self.layout

        sima = context.space_data
        overlay = sima.overlay

        layout.active = overlay.show_overlays

        row = layout.row(align=True)
        layout.prop(overlay, "show_text_info")

        row = layout.row(align=True)
        row.prop(overlay, "show_render_size")
        subrow = row.row()
        subrow.active = overlay.show_render_size
        subrow.prop(overlay, "passepartout_alpha", text="Passepartout")


# Grease Pencil properties
class IMAGE_PT_annotation(AnnotationDataPanel, Panel):
    bl_space_type = 'IMAGE_EDITOR'
    bl_region_type = 'UI'
    bl_category = "View"

    # NOTE: this is just a wrapper around the generic GP Panel.

# Grease Pencil drawing tools.


class ImageAssetShelf(BrushAssetShelf):
    bl_space_type = 'IMAGE_EDITOR'


class IMAGE_AST_brush_paint(ImageAssetShelf, AssetShelf):
    mode_prop = "use_paint_image"
    brush_type_prop = "image_brush_type"
    tool_prop = "image_tool"

    @classmethod
    def poll(cls, context):
        return context.space_data and context.space_data.mode == 'PAINT'


classes = (
    IMAGE_HT_tool_header,
    IMAGE_HT_header,
    IMAGE_MT_editor_menus,
    IMAGE_PT_active_tool,
    IMAGE_PT_mask,
    IMAGE_PT_mask_layers,
    IMAGE_PT_mask_display,
    IMAGE_PT_active_mask_spline,
    IMAGE_PT_active_mask_point,
    IMAGE_PT_mask_animation,
    IMAGE_PT_snapping,
    IMAGE_PT_proportional_edit,
    IMAGE_PT_image_properties,
    IMAGE_UL_render_slots,
    IMAGE_PT_render_slots,
    IMAGE_UL_udim_tiles,
    IMAGE_PT_udim_tiles,
    IMAGE_PT_view_display,
    IMAGE_PT_paint_select,
    IMAGE_PT_paint_settings,
    IMAGE_PT_paint_color,
    IMAGE_PT_paint_swatches,
    IMAGE_PT_paint_settings_advanced,
    IMAGE_PT_paint_clone,
    IMAGE_PT_tools_brush_texture,
    IMAGE_PT_tools_mask_texture,
    IMAGE_PT_paint_stroke,
    IMAGE_PT_paint_stroke_smooth_stroke,
    IMAGE_PT_paint_curve,
    IMAGE_PT_tools_brush_display,
    IMAGE_PT_tools_imagepaint_symmetry,
    IMAGE_PT_uv_sculpt_options,
    IMAGE_PT_uv_sculpt_curve,
    IMAGE_PT_view_histogram,
    IMAGE_PT_view_waveform,
    IMAGE_PT_view_vectorscope,
    IMAGE_PT_sample_line,
    IMAGE_PT_scope_sample,
    IMAGE_PT_uv_cursor,
    IMAGE_PT_annotation,
    IMAGE_PT_gizmo_display,
    IMAGE_PT_overlay,
    IMAGE_PT_overlay_guides,
    IMAGE_PT_overlay_uv_stretch,
    IMAGE_PT_overlay_uv_edit_geometry,
    IMAGE_PT_overlay_uv_display,
    IMAGE_PT_overlay_image,
    IMAGE_PT_overlay_render_guides,
    IMAGE_AST_brush_paint,
)


if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
