# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Operator
from bpy.props import (
    BoolProperty,
    EnumProperty,
    IntProperty,
    StringProperty,
)
from bpy.app.translations import (
    pgettext_rpt as rpt_,
    contexts as i18n_contexts,
)


# Los tres operadores de niveles de detalle (object.lod_by_name, object.lod_clear_all
# y object.lod_generate) son C++ nativo desde 2026-09-11 (Flipendo carril OPS-3):
# source/blender/editors/object/object_lod.cc, junto a lod_add y lod_remove, que ya lo
# eran. Ver politicas/LOD-A-CPP.md.
#
# Los tres operadores de seleccion que habia aqui (object.select_pattern,
# object.select_camera y object.select_hierarchy) son C++ nativo desde 2026-09-11
# (Flipendo carril C): source/blender/editors/object/object_select_extra.cc.
class SubdivisionSet(Operator):
    """Sets a Subdivision Surface level (1 to 5)"""

    bl_idname = "object.subdivision_set"
    bl_label = "Subdivision Set"
    bl_options = {'REGISTER', 'UNDO'}

    level: IntProperty(
        name="Level",
        min=-100, max=100,
        soft_min=-6, soft_max=6,
        default=1,
    )
    relative: BoolProperty(
        name="Relative",
        description="Apply the subdivision surface level as an offset relative to the current level",
        default=False,
    )

    @classmethod
    def poll(cls, context):
        obs = context.selected_editable_objects
        return (obs is not None)

    def execute(self, context):
        level = self.level
        relative = self.relative

        if relative and level == 0:
            return {'CANCELLED'}  # nothing to do

        if not relative and level < 0:
            self.level = level = 0

        def set_object_subd(obj):
            for mod in obj.modifiers:
                if mod.type == 'MULTIRES':
                    if not relative:
                        if level > mod.total_levels:
                            sub = level - mod.total_levels
                            for _ in range(sub):
                                bpy.ops.object.multires_subdivide(modifier="Multires")

                        if obj.mode == 'SCULPT':
                            if mod.sculpt_levels != level:
                                mod.sculpt_levels = level
                        elif obj.mode == 'OBJECT':
                            if mod.levels != level:
                                mod.levels = level
                        return
                    else:
                        if obj.mode == 'SCULPT':
                            if mod.sculpt_levels + level <= mod.total_levels:
                                mod.sculpt_levels += level
                        elif obj.mode == 'OBJECT':
                            if mod.levels + level <= mod.total_levels:
                                mod.levels += level
                        return

                elif mod.type == 'SUBSURF':
                    if relative:
                        mod.levels += level
                    else:
                        if mod.levels != level:
                            mod.levels = level

                    return

            # add a new modifier
            try:
                if obj.mode == 'SCULPT':
                    mod = obj.modifiers.new("Multires", 'MULTIRES')
                    if level > 0:
                        for _ in range(level):
                            bpy.ops.object.multires_subdivide(modifier="Multires")
                else:
                    mod = obj.modifiers.new("Subdivision", 'SUBSURF')
                    mod.levels = level
            except Exception:
                self.report({'WARNING'}, "Modifiers cannot be added to object: " + obj.name)

        for obj in context.selected_editable_objects:
            set_object_subd(obj)

        return {'FINISHED'}


class ShapeTransfer(Operator):
    """Copy the active shape key of another selected object to this one"""

    bl_idname = "object.shape_key_transfer"
    bl_label = "Transfer Shape Key"
    bl_options = {'REGISTER', 'UNDO'}

    mode: EnumProperty(
        items=(
            ('OFFSET',
             "Offset",
             "Apply the relative positional offset",
             ),
            ('RELATIVE_FACE',
             "Relative Face",
             "Calculate relative position (using faces)",
             ),
            ('RELATIVE_EDGE',
             "Relative Edge",
             "Calculate relative position (using edges)",
             ),
        ),
        name="Transformation Mode",
        description="Relative shape positions to the new shape method",
        default='OFFSET',
    )
    use_clamp: BoolProperty(
        name="Clamp Offset",
        description="Clamp the transformation to the distance each vertex moves in the original shape",
        default=False,
    )

    def _main(self, ob_act, objects, mode='OFFSET', use_clamp=False):

        def me_nos(verts):
            return [v.normal.copy() for v in verts]

        def me_cos(verts):
            return [v.co.copy() for v in verts]

        def ob_add_shape(ob, name):
            me = ob.data
            key = ob.shape_key_add(from_mix=False)
            if len(me.shape_keys.key_blocks) == 1:
                key.name = "Basis"
                key = ob.shape_key_add(from_mix=False)  # we need a rest
            key.name = name
            ob.active_shape_key_index = len(me.shape_keys.key_blocks) - 1
            ob.show_only_shape_key = True

        from mathutils.geometry import barycentric_transform
        from mathutils import Vector

        if use_clamp and mode == 'OFFSET':
            use_clamp = False

        me = ob_act.data
        orig_key_name = ob_act.active_shape_key.name

        orig_shape_coords = me_cos(ob_act.active_shape_key.data)

        orig_normals = me_nos(me.vertices)
        # actual mesh vertex location isn't as reliable as the base shape :S
        # orig_coords = me_cos(me.vertices)
        orig_coords = me_cos(me.shape_keys.key_blocks[0].data)

        for ob_other in objects:
            if ob_other.type != 'MESH':
                self.report(
                    {'WARNING'},
                    rpt_("Skipping '{:s}', not a mesh").format(ob_other.name),
                )
                continue
            me_other = ob_other.data
            if len(me_other.vertices) != len(me.vertices):
                self.report(
                    {'WARNING'},
                    rpt_("Skipping '{:s}', vertex count differs").format(ob_other.name),
                )
                continue

            target_normals = me_nos(me_other.vertices)
            if me_other.shape_keys:
                target_coords = me_cos(me_other.shape_keys.key_blocks[0].data)
            else:
                target_coords = me_cos(me_other.vertices)

            ob_add_shape(ob_other, orig_key_name)

            # editing the final coords, only list that stores wrapped coords
            target_shape_coords = [v.co for v in ob_other.active_shape_key.data]

            median_coords = [[] for i in range(len(me.vertices))]

            # Method 1, edge
            if mode == 'OFFSET':
                for i, vert_cos in enumerate(median_coords):
                    vert_cos.append(target_coords[i] +
                                    (orig_shape_coords[i] - orig_coords[i]))

            elif mode == 'RELATIVE_FACE':
                for poly in me.polygons:
                    idxs = poly.vertices[:]
                    v_before = idxs[-2]
                    v = idxs[-1]
                    for v_after in idxs:
                        pt = barycentric_transform(
                            orig_shape_coords[v],
                            orig_coords[v_before],
                            orig_coords[v],
                            orig_coords[v_after],
                            target_coords[v_before],
                            target_coords[v],
                            target_coords[v_after],
                        )
                        median_coords[v].append(pt)
                        v_before = v
                        v = v_after

            elif mode == 'RELATIVE_EDGE':
                for ed in me.edges:
                    i1, i2 = ed.vertices
                    v1, v2 = orig_coords[i1], orig_coords[i2]
                    edge_length = (v1 - v2).length
                    n1loc = v1 + orig_normals[i1] * edge_length
                    n2loc = v2 + orig_normals[i2] * edge_length

                    # now get the target nloc's
                    v1_to, v2_to = target_coords[i1], target_coords[i2]
                    edlen_to = (v1_to - v2_to).length
                    n1loc_to = v1_to + target_normals[i1] * edlen_to
                    n2loc_to = v2_to + target_normals[i2] * edlen_to

                    pt = barycentric_transform(
                        orig_shape_coords[i1],
                        v2, v1, n1loc,
                        v2_to, v1_to, n1loc_to,
                    )
                    median_coords[i1].append(pt)

                    pt = barycentric_transform(
                        orig_shape_coords[i2],
                        v1, v2, n2loc,
                        v1_to, v2_to, n2loc_to,
                    )
                    median_coords[i2].append(pt)

            # apply the offsets to the new shape
            from functools import reduce
            VectorAdd = Vector.__add__

            for i, vert_cos in enumerate(median_coords):
                if vert_cos:
                    co = reduce(VectorAdd, vert_cos) / len(vert_cos)

                    if use_clamp:
                        # clamp to the same movement as the original
                        # breaks copy between different scaled meshes.
                        len_from = (orig_shape_coords[i] - orig_coords[i]).length
                        ofs = co - target_coords[i]
                        ofs.length = len_from
                        co = target_coords[i] + ofs

                    target_shape_coords[i][:] = co

        return {'FINISHED'}

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj and obj.mode != 'EDIT')

    def execute(self, context):
        ob_act = context.active_object
        objects = [
            ob for ob in context.selected_editable_objects
            if ob != ob_act
        ]

        if 1:  # swap from/to, means we can't copy to many at once.
            if len(objects) != 1:
                self.report({'ERROR'}, "Expected one other selected mesh object to copy from")
                return {'CANCELLED'}
            ob_act, objects = objects[0], [ob_act]

        if ob_act.type != 'MESH':
            self.report({'ERROR'}, "Other object is not a mesh")
            return {'CANCELLED'}

        if ob_act.active_shape_key is None:
            self.report({'ERROR'}, "Other object has no shape key")
            return {'CANCELLED'}
        return self._main(ob_act, objects, self.mode, self.use_clamp)


class JoinUVs(Operator):
    """Transfer UV Maps from active to selected objects """ \
        """(needs matching geometry)"""
    bl_idname = "object.join_uvs"
    bl_label = "Transfer UV Maps"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return (obj and obj.type == 'MESH')

    def _main(self, context):
        import array
        obj = context.active_object
        mesh = obj.data

        is_editmode = (obj.mode == 'EDIT')
        if is_editmode:
            bpy.ops.object.mode_set(mode='OBJECT', toggle=False)

        if not mesh.uv_layers:
            self.report(
                {'WARNING'},
                rpt_("Object: {:s}, Mesh: '{:s}' has no UVs").format(obj.name, mesh.name),
            )
        else:
            nbr_loops = len(mesh.loops)

            # seems to be the fastest way to create an array
            uv_array = array.array("f", [0.0] * 2) * nbr_loops
            mesh.uv_layers.active.data.foreach_get("uv", uv_array)

            objects = context.selected_editable_objects[:]

            for obj_other in objects:
                if obj_other.type == 'MESH':
                    obj_other.data.tag = False

            for obj_other in objects:
                if not (obj_other != obj and obj_other.type == 'MESH'):
                    continue
                mesh_other = obj_other.data
                if mesh_other == mesh:
                    continue
                if mesh_other.tag is True:
                    continue

                mesh_other.tag = True
                if len(mesh_other.loops) != nbr_loops:
                    self.report(
                        {'WARNING'},
                        rpt_(
                            "Object: {:s}, Mesh: '{:s}' has {:d} loops (for {:d} faces), expected {:d}"
                        ).format(
                            obj_other.name,
                            mesh_other.name,
                            len(mesh_other.loops),
                            len(mesh_other.polygons),
                            nbr_loops,
                        ),
                    )
                else:
                    uv_other = mesh_other.uv_layers.active
                    if not uv_other:
                        mesh_other.uv_layers.new()
                        uv_other = mesh_other.uv_layers.active
                        if not uv_other:
                            self.report(
                                {'ERROR'},
                                rpt_(
                                    "Could not add a new UV map to object '{:s}' (Mesh '{:s}')"
                                ).format(
                                    obj_other.name,
                                    mesh_other.name,
                                ),
                            )

                    # finally do the copy
                    uv_other.data.foreach_set("uv", uv_array)
                    mesh_other.update()

        if is_editmode:
            bpy.ops.object.mode_set(mode='EDIT', toggle=False)

    def execute(self, context):
        self._main(context)
        return {'FINISHED'}


# Migrados a C++ el 2026-09-11 (Flipendo carril C):
#   object.make_dupli_face        -> editors/object/object_make_dupli_face.cc
#   object.isolate_type_render    -> editors/object/object_misc_ops.cc
#   object.hide_render_clear_all  -> editors/object/object_misc_ops.cc
#   object.instance_offset_from_cursor / _to_cursor / _from_object -> idem
class TransformsToDeltas(Operator):
    """Convert normal object transforms to delta transforms, """ \
        """any existing delta transforms will be included as well"""
    bl_idname = "object.transforms_to_deltas"
    bl_label = "Transforms to Deltas"
    bl_options = {'REGISTER', 'UNDO'}

    mode: EnumProperty(
        items=(
            ('ALL', "All Transforms", "Transfer location, rotation, and scale transforms"),
            ('LOC', "Location", "Transfer location transforms only"),
            ('ROT', "Rotation", "Transfer rotation transforms only"),
            ('SCALE', "Scale", "Transfer scale transforms only"),
        ),
        name="Mode",
        description="Which transforms to transfer",
        default='ALL',
    )
    reset_values: BoolProperty(
        name="Reset Values",
        description=("Clear transform values after transferring to deltas"),
        default=True,
    )

    @classmethod
    def poll(cls, context):
        obs = context.selected_editable_objects
        return (obs is not None)

    def execute(self, context):
        for obj in context.selected_editable_objects:
            if self.mode in {'ALL', 'LOC'}:
                self.transfer_location(obj)

            if self.mode in {'ALL', 'ROT'}:
                self.transfer_rotation(obj)

            if self.mode in {'ALL', 'SCALE'}:
                self.transfer_scale(obj)

        return {'FINISHED'}

    def transfer_location(self, obj):
        obj.delta_location += obj.location

        if self.reset_values:
            obj.location.zero()

    def transfer_rotation(self, obj):
        # TODO: add transforms together...
        if obj.rotation_mode == 'QUATERNION':
            delta = obj.delta_rotation_quaternion.copy()
            obj.delta_rotation_quaternion = obj.rotation_quaternion
            obj.delta_rotation_quaternion.rotate(delta)

            if self.reset_values:
                obj.rotation_quaternion.identity()
        elif obj.rotation_mode == 'AXIS_ANGLE':
            pass  # Unsupported
        else:
            delta = obj.delta_rotation_euler.copy()
            obj.delta_rotation_euler = obj.rotation_euler
            obj.delta_rotation_euler.rotate(delta)

            if self.reset_values:
                obj.rotation_euler.zero()

    def transfer_scale(self, obj):
        obj.delta_scale[0] *= obj.scale[0]
        obj.delta_scale[1] *= obj.scale[1]
        obj.delta_scale[2] *= obj.scale[2]

        if self.reset_values:
            obj.scale[:] = (1, 1, 1)


class TransformsToDeltasAnim(Operator):
    """Convert object animation for normal transforms to delta transforms"""
    bl_idname = "object.anim_transforms_to_deltas"
    bl_label = "Animated Transforms to Deltas"
    bl_options = {'REGISTER', 'UNDO'}

    @classmethod
    def poll(cls, context):
        obs = context.selected_editable_objects
        return (obs is not None)

    def execute(self, context):
        # map from standard transform paths to "new" transform paths
        STANDARD_TO_DELTA_PATHS = {
            "location": "delta_location",
            "rotation_euler": "delta_rotation_euler",
            "rotation_quaternion": "delta_rotation_quaternion",
            # "rotation_axis_angle" : "delta_rotation_axis_angle",
            "scale": "delta_scale",
        }
        DELTA_PATHS = STANDARD_TO_DELTA_PATHS.values()

        # try to apply on each selected object
        for obj in context.selected_editable_objects:
            adt = obj.animation_data
            if (adt is None) or (adt.action is None):
                self.report(
                    {'WARNING'},
                    rpt_("No animation data to convert on object: {!r}").format(obj.name),
                )
                continue

            # first pass over F-Curves: ensure that we don't have conflicting
            # transforms already (e.g. if this was applied already) #29110.
            existingFCurves = {}
            for fcu in adt.action.fcurves:
                # get "delta" path - i.e. the final paths which may clash
                path = fcu.data_path
                if path in STANDARD_TO_DELTA_PATHS:
                    # to be converted - conflicts may exist...
                    dpath = STANDARD_TO_DELTA_PATHS[path]
                elif path in DELTA_PATHS:
                    # already delta - check for conflicts...
                    dpath = path
                else:
                    # non-transform - ignore
                    continue

                # a delta path like this for the same index shouldn't
                # exist already, otherwise we've got a conflict
                if dpath in existingFCurves:
                    # ensure that this index hasn't occurred before
                    if fcu.array_index in existingFCurves[dpath]:
                        # conflict
                        self.report(
                            {'ERROR'},
                            rpt_(
                                "Object {!r} already has {!r} F-Curve(s). "
                                "Remove these before trying again"
                            ).format(obj.name, dpath))
                        return {'CANCELLED'}
                    else:
                        # no conflict here
                        existingFCurves[dpath] += [fcu.array_index]
                else:
                    # no conflict yet
                    existingFCurves[dpath] = [fcu.array_index]

            # if F-Curve uses standard transform path
            # just append "delta_" to this path
            for fcu in adt.action.fcurves:
                if fcu.data_path == "location":
                    fcu.data_path = "delta_location"
                    obj.location.zero()
                elif fcu.data_path == "rotation_euler":
                    fcu.data_path = "delta_rotation_euler"
                    obj.rotation_euler.zero()
                elif fcu.data_path == "rotation_quaternion":
                    fcu.data_path = "delta_rotation_quaternion"
                    obj.rotation_quaternion.identity()
                # XXX: currently not implemented
                # ~ elif fcu.data_path == "rotation_axis_angle":
                # ~    fcu.data_path = "delta_rotation_axis_angle"
                elif fcu.data_path == "scale":
                    fcu.data_path = "delta_scale"
                    obj.scale = 1.0, 1.0, 1.0

        # hack: force animsys flush by changing frame, so that deltas get run
        context.scene.frame_set(context.scene.frame_current)

        return {'FINISHED'}


class OBJECT_OT_assign_property_defaults(Operator):
    """Assign the current values of custom properties as their defaults, """ \
        """for use as part of the rest pose state in NLA track mixing"""
    bl_idname = "object.assign_property_defaults"
    bl_label = "Assign Custom Property Values as Default"
    bl_options = {'UNDO', 'REGISTER'}

    process_data: BoolProperty(name="Process data properties", default=True)
    process_bones: BoolProperty(name="Process bone properties", default=True)

    @classmethod
    def poll(cls, context):
        obj = context.active_object
        return obj is not None and obj.is_editable and obj.mode in {'POSE', 'OBJECT'}

    @staticmethod
    def assign_defaults(obj):
        from rna_prop_ui import rna_idprop_ui_prop_default_set

        rna_properties = {prop.identifier for prop in obj.bl_rna.properties if prop.is_runtime}

        for prop, value in obj.items():
            if prop not in rna_properties:
                rna_idprop_ui_prop_default_set(obj, prop, value)

    def execute(self, context):
        obj = context.active_object

        self.assign_defaults(obj)

        if self.process_bones and obj.pose:
            for pbone in obj.pose.bones:
                self.assign_defaults(pbone)

        if self.process_data and obj.data and obj.data.is_editable:
            self.assign_defaults(obj.data)

            if self.process_bones and isinstance(obj.data, bpy.types.Armature):
                for bone in obj.data.bones:
                    self.assign_defaults(bone)

        return {'FINISHED'}


classes = (
    JoinUVs,
    ShapeTransfer,
    SubdivisionSet,
    TransformsToDeltas,
    TransformsToDeltasAnim,
    OBJECT_OT_assign_property_defaults,
)
