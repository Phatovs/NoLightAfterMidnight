# No Light After Midnight - Blender model script'leri için ortak yardımcılar
# Ölçüler Unreal cm'si; Unreal ekseni U(x, y, z) -> Blender (x, -y, z) (FBX + Unreal içe aktarma Y'yi çevirir).
# Malzeme adları Unreal'daki malzeme örneği adlarıdır (MI_NL_...); içe aktarma script'i yuvaları adla eşler.
import math
import os
import sys

import bpy
import bmesh
from mathutils import Vector

C = 0.01
MATS = {}


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    MATS.clear()


def out_dir(default_name):
    d = sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv else os.path.join(os.getcwd(), default_name)
    os.makedirs(d, exist_ok=True)
    return d


def scene():
    return bpy.context.scene


def mat(name):
    if name not in MATS:
        MATS[name] = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    return MATS[name]


def to_bl(x, y, z):
    return Vector((x * C, -y * C, z * C))


def finish(obj, m, bevel=0.0, smooth=False, segs=1):
    obj.data.materials.clear()
    obj.data.materials.append(mat(m))
    if bevel > 0:
        mod = obj.modifiers.new("bevel", 'BEVEL')
        mod.width = bevel * C
        mod.segments = segs
        mod.limit_method = 'ANGLE'
    if smooth:
        for p in obj.data.polygons:
            p.use_smooth = True
    return obj


def new_obj(name, bm):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    scene().collection.objects.link(ob)
    return ob


def box(name, lo, hi, m, bevel=0.0, rot=(0.0, 0.0, 0.0), segs=1, pivot=None):
    """lo/hi: Unreal cm köşeleri. rot: Unreal X/Y/Z eksenleri etrafında derece (kutunun ortası etrafında)."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=((hi[0] - lo[0]) * C, (hi[1] - lo[1]) * C, (hi[2] - lo[2]) * C), verts=bm.verts)
    ob = new_obj(name, bm)
    ob.location = to_bl((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2)
    ob.rotation_euler = (math.radians(-rot[0]), math.radians(rot[1]), math.radians(-rot[2]))
    return finish(ob, m, bevel, segs=segs)


def beam(name, a, b, w, h, m, bevel=0.0, up=(0, 0, 1)):
    """a'dan b'ye uzanan kare kesitli çubuk (w x h)"""
    a, b = Vector(a), Vector(b)
    d = b - a
    L = d.length
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=(L * C, w * C, h * C), verts=bm.verts)
    ob = new_obj(name, bm)
    mid = (a + b) / 2
    ob.location = to_bl(mid.x, mid.y, mid.z)
    dbl = Vector((d.x, -d.y, d.z)).normalized()
    ob.rotation_euler = (dbl.to_track_quat('X', 'Y') if abs(dbl.z) > 0.99 else dbl.to_track_quat('X', 'Z')).to_euler()
    return finish(ob, m, bevel)


def cyl(name, center, r, length, axis, m, seg=20, smooth=True, r2=None, rot=None):
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=seg, radius1=r * C,
                          radius2=(r2 if r2 is not None else r) * C, depth=length * C)
    ob = new_obj(name, bm)
    ob.location = to_bl(*center)
    if axis == 'X':
        ob.rotation_euler = (0.0, math.radians(90), 0.0)
    elif axis == 'Y':
        ob.rotation_euler = (math.radians(90), 0.0, 0.0)
    if rot is not None:
        ob.rotation_euler = rot
    return finish(ob, m, smooth=smooth)


def tube(name, a, b, r, m, seg=12, smooth=True):
    """a'dan b'ye boru"""
    a, b = Vector(a), Vector(b)
    d = b - a
    ob = cyl(name, ((a.x + b.x) / 2, (a.y + b.y) / 2, (a.z + b.z) / 2), r, d.length, 'Z', m, seg=seg, smooth=smooth)
    dbl = Vector((d.x, -d.y, d.z)).normalized()
    ob.rotation_euler = dbl.to_track_quat('Z', 'Y').to_euler()
    return ob


def torus(name, center, R, r, m, axis='Y', majseg=32, minseg=8):
    bpy.ops.mesh.primitive_torus_add(major_radius=R * C, minor_radius=r * C, major_segments=majseg, minor_segments=minseg,
                                     location=to_bl(*center))
    ob = bpy.context.active_object
    ob.name = name
    if axis == 'Y':
        ob.rotation_euler = (math.radians(90), 0.0, 0.0)
    elif axis == 'X':
        ob.rotation_euler = (0.0, math.radians(90), 0.0)
    return finish(ob, m, smooth=True)


def sphere(name, center, r, m, scale=(1, 1, 1), seg=16, rings=8):
    bpy.ops.mesh.primitive_uv_sphere_add(radius=r * C, segments=seg, ring_count=rings, location=to_bl(*center))
    ob = bpy.context.active_object
    ob.name = name
    ob.scale = (scale[0], scale[1], scale[2])
    return finish(ob, m, smooth=True)


def text(name, s, center, size, m, face='-Y', depth=0.15, spacing=1.0):
    cu = bpy.data.curves.new(name, 'FONT')
    cu.body = s
    cu.size = size * C
    cu.extrude = depth * C
    cu.align_x = 'CENTER'
    cu.align_y = 'CENTER'
    cu.space_character = spacing
    ob = bpy.data.objects.new(name, cu)
    scene().collection.objects.link(ob)
    ob.location = to_bl(*center)
    # Blender yazısı +Z'ye bakar. Unreal -Y = Blender +Y, Unreal +Y = Blender -Y, Unreal +X = Blender +X
    rots = {'-Y': (90, 0, 180), '+Y': (90, 0, 0), '+X': (90, 0, 90), '-X': (90, 0, -90)}
    r = rots[face]
    ob.rotation_euler = (math.radians(r[0]), math.radians(r[1]), math.radians(r[2]))
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    bpy.ops.object.convert(target='MESH')
    ob = bpy.context.active_object
    ob.select_set(False)
    return finish(ob, m)


def wire(name, pts, radius, m, res=3):
    cu = bpy.data.curves.new(name, 'CURVE')
    cu.dimensions = '3D'
    cu.bevel_depth = radius * C
    cu.bevel_resolution = res
    sp = cu.splines.new('BEZIER')
    sp.bezier_points.add(len(pts) - 1)
    for i, p in enumerate(pts):
        bp = sp.bezier_points[i]
        bp.co = to_bl(*p)
        bp.handle_left_type = 'AUTO'
        bp.handle_right_type = 'AUTO'
    ob = bpy.data.objects.new(name, cu)
    scene().collection.objects.link(ob)
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    bpy.ops.object.convert(target='MESH')
    ob = bpy.context.active_object
    ob.select_set(False)
    return finish(ob, m, smooth=True)


def poly_prism(name, pts_xz, y0, y1, m):
    """XZ düzleminde çokgen (Unreal), Y boyunca y0..y1 kalınlık"""
    bm = bmesh.new()
    back = [bm.verts.new(to_bl(x, y0, z)) for (x, z) in pts_xz]
    front = [bm.verts.new(to_bl(x, y1, z)) for (x, z) in pts_xz]
    bm.faces.new(back)
    bm.faces.new(list(reversed(front)))
    n = len(pts_xz)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new([back[i], back[j], front[j], front[i]])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return finish(new_obj(name, bm), m)


def join(name, objs, uv_size=0.6):
    for o in bpy.context.selected_objects:
        o.select_set(False)
    for o in objs:
        bpy.context.view_layer.objects.active = o
        for mod in list(o.modifiers):
            bpy.ops.object.modifier_apply(modifier=mod.name)
    bpy.context.view_layer.objects.active = objs[0]
    for o in objs:
        o.select_set(True)
    bpy.ops.object.join()
    ob = bpy.context.active_object
    ob.name = name
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.cube_project(cube_size=uv_size)
    bpy.ops.object.mode_set(mode='OBJECT')
    ob.select_set(False)
    return ob


def export(ob, out):
    for o in bpy.context.selected_objects:
        o.select_set(False)
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.fbx(filepath=os.path.join(out, ob.name + ".fbx"), use_selection=True, apply_unit_scale=True,
                             apply_scale_options='FBX_SCALE_UNITS', object_types={'MESH'}, use_mesh_modifiers=True,
                             mesh_smooth_type='FACE', add_leaf_bones=False, bake_anim=False, axis_forward='-Z', axis_up='Y')
    bb = [ob.matrix_world @ Vector(v) for v in ob.bound_box]
    lo = [min(v[i] for v in bb) / C for i in range(3)]
    hi = [max(v[i] for v in bb) / C for i in range(3)]
    print("NLModel: %s  %d tris  X %.0f..%.0f  Y(UE) %.0f..%.0f  Z %.0f..%.0f  mats=%s" % (
        ob.name, sum(len(p.vertices) - 2 for p in ob.data.polygons), lo[0], hi[0], -hi[1], -lo[1], lo[2], hi[2],
        [m.name for m in ob.data.materials]))
    ob.select_set(False)


def preview(out, fname, cam_ue, target_ue, colors, lens=35, res=(1200, 900), extra=None):
    for n, c in colors.items():
        if n in MATS:
            MATS[n].diffuse_color = c
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    scene().collection.objects.link(cam)
    t = to_bl(*target_ue)
    cam.location = to_bl(*cam_ue)
    cam.rotation_euler = (t - cam.location).to_track_quat('-Z', 'Y').to_euler()
    cam.data.lens = lens
    sc = scene()
    sc.camera = cam
    sc.render.engine = 'BLENDER_WORKBENCH'
    sc.display.shading.light = 'STUDIO'
    sc.display.shading.color_type = 'MATERIAL'
    sc.display.shading.show_cavity = True
    sc.display.shading.show_shadows = True
    sc.render.resolution_x, sc.render.resolution_y = res
    sc.render.filepath = os.path.join(out, fname)
    bpy.ops.render.render(write_still=True)
    print("NLModel: preview ->", sc.render.filepath)
