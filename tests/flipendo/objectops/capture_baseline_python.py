import bpy
import sys

argv = sys.argv
outpath = argv[argv.index("--") + 1]

f = open(outpath, "w")
f.write("# FL-OBJECT-OPS-SELFTEST v1\n")


def make_cube(loc, rot, size):
    bpy.ops.mesh.primitive_cube_add(size=size, location=loc, rotation=rot)
    return bpy.context.active_object


def set_selection(active, selected):
    bpy.ops.object.select_all(action="DESELECT")
    for ob in selected:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = active


def dump_object_transform(label, ob):
    loc = ob.location
    rot = ob.rotation_euler
    scale = ob.scale
    dloc = ob.delta_location
    drot = ob.delta_rotation_euler
    dscale = ob.delta_scale
    f.write(
        "  {:<10} loc=({:.9g},{:.9g},{:.9g}) rot=({:.9g},{:.9g},{:.9g}) "
        "scale=({:.9g},{:.9g},{:.9g}) dloc=({:.9g},{:.9g},{:.9g}) "
        "drot=({:.9g},{:.9g},{:.9g}) dscale=({:.9g},{:.9g},{:.9g})\n".format(
            label,
            loc[0], loc[1], loc[2],
            rot[0], rot[1], rot[2],
            scale[0], scale[1], scale[2],
            dloc[0], dloc[1], dloc[2],
            drot[0], drot[1], drot[2],
            dscale[0], dscale[1], dscale[2],
        )
    )


def dump_mesh_stats(ob):
    me = ob.data
    s = 0.0
    ssq = 0.0
    for v in me.vertices:
        co = v.co
        s += co.x + co.y + co.z
        ssq += co.x * co.x + co.y * co.y + co.z * co.z
    uvs = 1 if len(me.uv_layers) > 0 else 0
    f.write(
        "  mesh verts={} edges={} faces={} loops={} sum={:.9g} sumsq={:.9g} uvs={}\n".format(
            len(me.vertices), len(me.edges), len(me.polygons), len(me.loops), s, ssq, uvs
        )
    )


def delete_obj(ob):
    data = ob.data
    bpy.data.objects.remove(ob, do_unlink=True)
    if data is not None:
        bpy.data.meshes.remove(data)


# -------------------------------------------------------------
# object.align
# -------------------------------------------------------------
loc_a = (1.0, 2.0, 3.0)
rot_a = (0.0, 0.0, 0.0)
loc_b = (5.0, -1.0, 0.5)
rot_b = (0.3, 0.1, 0.0)
loc_c = (-3.0, 4.0, 1.0)
rot_c = (0.0, 0.7, 0.2)

ob_a = make_cube(loc_a, rot_a, 2.0)
ob_b = make_cube(loc_b, rot_b, 1.0)
ob_c = make_cube(loc_c, rot_c, 3.0)

align_combos = [
    ("OPT_2", "OPT_4", 1, 1, 1, True),
    ("OPT_1", "OPT_2", 1, 1, 0, True),
    ("OPT_3", "OPT_3", 0, 1, 1, False),
]

for combo_index, (align_mode, relative_to, ax, ay, az, bbq) in enumerate(align_combos):
    ob_a.location = loc_a
    ob_a.rotation_euler = rot_a
    ob_b.location = loc_b
    ob_b.rotation_euler = rot_b
    ob_c.location = loc_c
    ob_c.rotation_euler = rot_c
    bpy.context.view_layer.update()

    set_selection(ob_a, [ob_a, ob_b, ob_c])

    axis = set()
    if ax:
        axis.add("X")
    if ay:
        axis.add("Y")
    if az:
        axis.add("Z")

    bpy.ops.object.align(
        bb_quality=bbq,
        align_mode=align_mode,
        relative_to=relative_to,
        align_axis=axis,
    )

    bpy.context.view_layer.update()

    f.write(
        "object.align combo={} mode={} rel={} axis={}{}{} bbq={}\n".format(
            combo_index, align_mode, relative_to, ax, ay, az, int(bbq)
        )
    )
    dump_object_transform("A", ob_a)
    dump_object_transform("B", ob_b)
    dump_object_transform("C", ob_c)

delete_obj(ob_a)
delete_obj(ob_b)
delete_obj(ob_c)

# -------------------------------------------------------------
# object.randomize_transform
# -------------------------------------------------------------
loc0 = (0.0, 0.0, 0.0)
rot0 = (0.0, 0.0, 0.0)

rand_combos = [
    (0, False, True, True, True, False, (1.0, 1.0, 1.0), (0.5, 0.5, 0.5), (1.0, 1.0, 1.0)),
    (42, False, True, False, True, True, (2.0, 0.0, 3.0), (0.0, 0.0, 0.0), (0.5, 0.5, 0.5)),
    (123, True, True, True, True, False, (0.5, 0.5, 0.5), (1.0, 0.2, 0.0), (0.2, 0.2, 0.2)),
]

for combo_index, (seed, delta, use_loc, use_rot, use_scale, scale_even, loc_range, rot_range, scale_range) in enumerate(rand_combos):
    ob1 = make_cube(loc0, rot0, 2.0)
    ob2 = make_cube(loc0, rot0, 2.0)
    ob3 = make_cube(loc0, rot0, 2.0)
    set_selection(ob1, [ob1, ob2, ob3])

    bpy.ops.object.randomize_transform(
        random_seed=seed,
        use_delta=delta,
        use_loc=use_loc,
        loc=loc_range,
        use_rot=use_rot,
        rot=rot_range,
        use_scale=use_scale,
        scale_even=scale_even,
        scale=scale_range,
    )

    f.write(
        "object.randomize_transform combo={} seed={} delta={} loc={} rot={} scale={} "
        "even={}\n".format(
            combo_index, seed, int(delta), int(use_loc), int(use_rot), int(use_scale), int(scale_even)
        )
    )
    dump_object_transform("1", ob1)
    dump_object_transform("2", ob2)
    dump_object_transform("3", ob3)

    delete_obj(ob1)
    delete_obj(ob2)
    delete_obj(ob3)

# -------------------------------------------------------------
# mesh.primitive_torus_add
# -------------------------------------------------------------
torus_combos = [
    (48, 12, "MAJOR_MINOR", 1.0, 0.25, 1.25, 0.75, True),
    (8, 5, "MAJOR_MINOR", 2.5, 0.75, 3.25, 1.75, False),
    (16, 6, "EXT_INT", 1.0, 0.25, 4.0, 1.0, True),
]

for combo_index, (major_segments, minor_segments, mode, major_radius, minor_radius, abso_major_rad, abso_minor_rad, generate_uvs) in enumerate(torus_combos):
    bpy.ops.mesh.primitive_torus_add(
        major_segments=major_segments,
        minor_segments=minor_segments,
        mode=mode,
        major_radius=major_radius,
        minor_radius=minor_radius,
        abso_major_rad=abso_major_rad,
        abso_minor_rad=abso_minor_rad,
        generate_uvs=generate_uvs,
        location=(0.0, 0.0, 0.0),
        rotation=(0.0, 0.0, 0.0),
    )
    ob = bpy.context.active_object
    f.write(
        "mesh.primitive_torus_add combo={} major_seg={} minor_seg={} mode={} uvs={}\n".format(
            combo_index, major_segments, minor_segments, mode, int(generate_uvs)
        )
    )
    dump_mesh_stats(ob)
    delete_obj(ob)

f.close()
print("selftest_driver: escrito en", outpath)
