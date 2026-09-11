# SPDX-FileCopyrightText: 2011-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import FileHandler

bl_file_extensions_image_and_movie = ";".join((
    *bpy.path.extensions_image,
    *bpy.path.extensions_movie,
))


class VIEW3D_FH_empty_image(FileHandler):
    bl_idname = "VIEW3D_FH_empty_image"
    bl_label = "Add empty image"
    bl_import_operator = "OBJECT_OT_empty_image_add"
    bl_file_extensions = bl_file_extensions_image_and_movie

    @classmethod
    def poll_drop(cls, context):
        if not context.space_data or context.space_data.type != 'VIEW_3D':
            return False
        rv3d = context.space_data.region_3d
        return rv3d.view_perspective == 'PERSP' or rv3d.view_perspective == 'ORTHO'


class VIEW3D_FH_camera_background_image(FileHandler):
    bl_idname = "VIEW3D_FH_camera_background_image"
    bl_label = "Add camera background image"
    bl_import_operator = "VIEW3D_OT_camera_background_image_add"
    bl_file_extensions = bl_file_extensions_image_and_movie

    @classmethod
    def poll_drop(cls, context):
        if not context.space_data or context.space_data.type != 'VIEW_3D':
            return False
        rv3d = context.space_data.region_3d
        return rv3d.view_perspective == 'CAMERA'


class VIEW3D_FH_vdb_volume(FileHandler):
    bl_idname = "VIEW3D_FH_vdb_volume"
    bl_label = "OpenVDB volume"
    bl_import_operator = "OBJECT_OT_volume_import"
    bl_file_extensions = ".vdb"

    @classmethod
    def poll_drop(cls, context):
        return context.space_data and context.space_data.type == 'VIEW_3D'


# Los cinco operadores que habia aqui (view3d.edit_mesh_extrude_individual_move,
# view3d.edit_mesh_extrude_move_normal, view3d.edit_mesh_extrude_move_shrink_fatten,
# view3d.edit_mesh_extrude_manifold_normal y view3d.transform_gizmo_set) son C++ nativo
# desde 2026-09-11 (Flipendo carril C): editors/mesh/view3d_edit_mesh_extrude.cc y
# editors/object/view3d_transform_gizmo_set.cc. Quedan aqui solo los tres FileHandler,
# que no son operadores y necesitan su propia migracion a bke::file_handler_add().
classes = (
    VIEW3D_FH_camera_background_image,
    VIEW3D_FH_empty_image,
    VIEW3D_FH_vdb_volume,
)
