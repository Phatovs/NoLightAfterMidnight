# No Light After Midnight - Sigorta kutusu modeli (Blender ile üretilir, FBX olarak dışa aktarılır)
# Çalıştırma: blender.exe -b --factory-startup --python Scripts/blender_fusebox.py -- <çıktı klasörü>
# Çıktı: SM_NL_FuseCabinet.fbx, SM_NL_FuseCartridge.fbx, SM_NL_FuseLever.fbx, SM_NL_GaugeNeedle.fbx, SM_NL_FuseLED.fbx
#
# Ölçüler Unreal'daki ANLFuseBox yerel koordinatlarıyla aynı (cm): arka yüz duvarda (Y=0), ön +Y, X sağa, Z yukarı.
# Blender'da -Y önde modellenir (FBX + Unreal içe aktarma Y'yi çevirir): U(x, y, z) -> Blender (x, -y, z).
#
# Yerleşim (C++ NLFuseLayout ile aynı olmalı):
#   Kutu X -33..33, Z -12..80, derinlik 18. Kapak sol menteşeden dışarı açık.
#   Üst sıra dikey sigortalar (yuva 0-2, CAM 1-3): X -18 / 0 / 18, Z 62, sigorta merkezi Y 6.5
#   Yatay sigorta (yuva 3, FLASH): X 16, Z 23
#   Küçük göstergeler: CAM X -18/0/18 Z 42, FLASH X 20 Z 9. Büyük gösterge X -14 Z 4. MAIN şalter mafsalı X 4 Y 7 Z 6.
import math
import os
import random
import sys

import bpy
import bmesh
from mathutils import Vector, Matrix

OUT = sys.argv[sys.argv.index("--") + 1] if "--" in sys.argv else os.path.join(os.getcwd(), "fusebox_out")
os.makedirs(OUT, exist_ok=True)
random.seed(7)
C = 0.01  # cm -> m

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = 'METRIC'
scene.unit_settings.scale_length = 1.0

MATS = {}


def mat(name):
    if name not in MATS:
        m = bpy.data.materials.new(name)
        MATS[name] = m
    return MATS[name]


def to_bl(x, y, z):
    return Vector((x * C, -y * C, z * C))


def finish(obj, m, bevel=0.0, smooth=False):
    obj.data.materials.clear()
    obj.data.materials.append(mat(m))
    if bevel > 0:
        mod = obj.modifiers.new("bevel", 'BEVEL')
        mod.width = bevel * C
        mod.segments = 2
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
    scene.collection.objects.link(ob)
    return ob


def box(name, lo, hi, m, bevel=0.0, rot_y=0.0, rot_z=0.0):
    """lo/hi: Unreal cm köşeleri"""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    sx, sy, sz = (hi[0] - lo[0]) * C, (hi[1] - lo[1]) * C, (hi[2] - lo[2]) * C
    bmesh.ops.scale(bm, vec=(sx, sy, sz), verts=bm.verts)
    ob = new_obj(name, bm)
    ob.location = to_bl((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, (lo[2] + hi[2]) / 2)
    ob.rotation_euler = (0.0, math.radians(rot_y), math.radians(rot_z))
    return finish(ob, m, bevel)


def cyl(name, center, r, length, axis, m, seg=24, smooth=True, r2=None):
    """axis: 'X' 'Y' 'Z' (Unreal ekseni). r2 verilirse koni."""
    bm = bmesh.new()
    bmesh.ops.create_cone(bm, cap_ends=True, cap_tris=False, segments=seg, radius1=r * C,
                          radius2=(r2 if r2 is not None else r) * C, depth=length * C)
    ob = new_obj(name, bm)
    ob.location = to_bl(*center)
    if axis == 'X':
        ob.rotation_euler = (0.0, math.radians(90), 0.0)
    elif axis == 'Y':
        ob.rotation_euler = (math.radians(90), 0.0, 0.0)
    finish(ob, m, smooth=smooth)
    return ob


def torus(name, center, R, r, m, axis='Y'):
    bpy.ops.mesh.primitive_torus_add(major_radius=R * C, minor_radius=r * C, major_segments=32, minor_segments=8,
                                     location=to_bl(*center))
    ob = bpy.context.active_object
    ob.name = name
    if axis == 'Y':
        ob.rotation_euler = (math.radians(90), 0.0, 0.0)
    return finish(ob, m, smooth=True)


def text(name, s, center, size, m, rot_y_face=True, depth=0.06, spacing=1.0, align='CENTER'):
    """Ön yüze (+Y Unreal) bakan düz yazı; center: yazının ortası"""
    cu = bpy.data.curves.new(name, 'FONT')
    cu.body = s
    cu.size = size * C
    cu.extrude = depth * C
    cu.align_x = align
    cu.align_y = 'CENTER'
    cu.space_character = spacing
    ob = bpy.data.objects.new(name, cu)
    scene.collection.objects.link(ob)
    ob.location = to_bl(*center)
    # Blender yazısı XY düzleminde, +Z'ye bakar -> -Y'ye (Unreal +Y) bakacak şekilde yatır
    ob.rotation_euler = (math.radians(90), 0.0, 0.0)
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    bpy.ops.object.convert(target='MESH')
    ob = bpy.context.active_object
    ob.select_set(False)
    return finish(ob, m)


def wire(name, pts, radius, m):
    cu = bpy.data.curves.new(name, 'CURVE')
    cu.dimensions = '3D'
    cu.bevel_depth = radius * C
    cu.bevel_resolution = 3
    sp = cu.splines.new('BEZIER')
    sp.bezier_points.add(len(pts) - 1)
    for i, p in enumerate(pts):
        bp = sp.bezier_points[i]
        bp.co = to_bl(*p)
        bp.handle_left_type = 'AUTO'
        bp.handle_right_type = 'AUTO'
    ob = bpy.data.objects.new(name, cu)
    scene.collection.objects.link(ob)
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    bpy.ops.object.convert(target='MESH')
    ob = bpy.context.active_object
    ob.select_set(False)
    return finish(ob, m, smooth=True)


def dprism(name, cx, cz, r, flat, y0, depth, m, seg=28):
    """D biçimli prizma: üstte yay, altta düz kenar (z = cz - flat). Ön yüz +Y (Unreal)."""
    bm = bmesh.new()
    a0 = math.asin(max(-1.0, min(1.0, -flat / r)))
    pts = []
    for i in range(seg + 1):
        a = a0 + (math.pi - 2 * a0) * i / seg
        pts.append((cx + math.cos(a) * r, cz + math.sin(a) * r))
    back = [bm.verts.new(to_bl(x, y0, z)) for (x, z) in pts]
    front = [bm.verts.new(to_bl(x, y0 + depth, z)) for (x, z) in pts]
    bm.faces.new(back)
    bm.faces.new(list(reversed(front)))
    n = len(pts)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new([back[i], back[j], front[j], front[i]])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return finish(new_obj(name, bm), m)


def gauge(prefix, cx, cz, r, y0, parts, ticks=9, label_v=True):
    """D biçimli voltmetre: krom çerçeve, koyu gövde, krem kadran, çentikler, 'V'"""
    flat = r * 0.45
    parts.append(dprism(prefix + "_rim", cx, cz, r + 0.9, flat + 0.9, y0, 3.0, "MI_NL_Metal"))
    parts.append(dprism(prefix + "_body", cx, cz, r + 0.35, flat + 0.35, y0 + 0.2, 2.8, "MI_NL_FuseCabinet"))
    parts.append(dprism(prefix + "_face", cx, cz, r, flat, y0 + 3.1, 0.2, "MI_NL_GaugeFace"))
    pivot_z = cz - r * 0.3
    for i in range(ticks):
        a = math.radians(-60 + 120 * i / (ticks - 1))
        big = i % 2 == 0
        r0, r1 = r * (0.62 if big else 0.7), r * 0.86
        mid = (r0 + r1) / 2
        x = cx + math.sin(a) * mid
        z = pivot_z + math.cos(a) * mid
        t = box(prefix + "_tick%d" % i, (x - 0.12, y0 + 3.4, z - (r1 - r0) / 2), (x + 0.12, y0 + 3.48, z + (r1 - r0) / 2), "MI_NL_Ink", rot_y=math.degrees(a))
        parts.append(t)
    # İnce yay
    for i in range(24):
        a0 = math.radians(-60 + 120 * i / 24)
        x = cx + math.sin(a0) * r * 0.88
        z = pivot_z + math.cos(a0) * r * 0.88
        parts.append(box(prefix + "_arc%d" % i, (x - r * 0.14, y0 + 3.4, z - 0.06), (x + r * 0.14, y0 + 3.46, z + 0.06), "MI_NL_Ink", rot_y=math.degrees(a0) + 90))
    if label_v:
        parts.append(text(prefix + "_V", "V", (cx, y0 + 3.45, pivot_z + r * 0.28), r * 0.28, "MI_NL_Ink"))
    return pivot_z


def tape(prefix, s, cx, cz, w, h, parts, vertical=False, size=2.4):
    rz = random.uniform(-2.5, 2.5)
    parts.append(box(prefix + "_tape", (cx - w / 2, 2.6, cz - h / 2), (cx + w / 2, 2.85, cz + h / 2), "MI_NL_Tape", rot_y=rz))
    if vertical:
        for i, ch in enumerate(s):
            parts.append(text(prefix + "_t%d" % i, ch, (cx, 2.9, cz + h / 2 - (i + 0.6) * (h / len(s))), size, "MI_NL_Ink"))
    else:
        parts.append(text(prefix + "_t", s, (cx, 2.9, cz), size, "MI_NL_Ink", spacing=1.15))


def join(name, objs):
    for o in bpy.context.selected_objects:
        o.select_set(False)
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    # Değiştiricileri uygula (bevel)
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
    # UV: küp izdüşümü (malzemeler UVScale ile dokuyu tekrarlar)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.cube_project(cube_size=0.35)
    bpy.ops.object.mode_set(mode='OBJECT')
    ob.select_set(False)
    return ob


def export(ob):
    for o in bpy.context.selected_objects:
        o.select_set(False)
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    path = os.path.join(OUT, ob.name + ".fbx")
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, apply_unit_scale=True, apply_scale_options='FBX_SCALE_UNITS',
                             object_types={'MESH'}, use_mesh_modifiers=True, mesh_smooth_type='FACE', add_leaf_bones=False,
                             bake_anim=False, axis_forward='-Z', axis_up='Y')
    bb = [ob.matrix_world @ Vector(v) for v in ob.bound_box]
    lo = [min(v[i] for v in bb) / C for i in range(3)]
    hi = [max(v[i] for v in bb) / C for i in range(3)]
    print("NLFuseModel: %s  %d tris  X %.1f..%.1f  Y(bl) %.1f..%.1f  Z %.1f..%.1f  mats=%s" % (
        ob.name, sum(len(p.vertices) - 2 for p in ob.data.polygons), lo[0], hi[0], lo[1], hi[1], lo[2], hi[2],
        [m.name for m in ob.data.materials]))
    ob.select_set(False)


# ------------------------------------------------------------------ kabin
P = []
CAB = "MI_NL_FuseCabinet"
X0, X1, Z0, Z1, D, T = -33.0, 33.0, -12.0, 80.0, 18.0, 1.5
P.append(box("back", (X0, 0, Z0), (X1, T, Z1), CAB, 0.3))
P.append(box("left", (X0, 0, Z0), (X0 + T, D, Z1), CAB, 0.3))
P.append(box("right", (X1 - T, 0, Z0), (X1, D, Z1), CAB, 0.3))
P.append(box("top", (X0, 0, Z1 - T), (X1, D, Z1), CAB, 0.3))
P.append(box("bottom", (X0, 0, Z0), (X1, D, Z0 + T), CAB, 0.3))
# Ön çerçeve dudağı (kapağın kapandığı kenar)
for nm, lo, hi in (("lipL", (X0, D - 1, Z0), (X0 + 3, D, Z1)), ("lipR", (X1 - 3, D - 1, Z0), (X1, D, Z1)),
                   ("lipT", (X0, D - 1, Z1 - 3), (X1, D, Z1)), ("lipB", (X0, D - 1, Z0), (X1, D, Z0 + 3))):
    P.append(box(nm, lo, hi, CAB, 0.2))
# İç montaj plakası + köşe cıvataları
P.append(box("plate", (X0 + 3, T, Z0 + 3), (X1 - 3, T + 1.0, Z1 - 3), CAB, 0.2))
for bx in (X0 + 5, X1 - 5):
    for bz in (Z0 + 5, Z1 - 5):
        P.append(cyl("bolt", (bx, T + 1.4, bz), 0.8, 0.8, 'Y', "MI_NL_Metal", seg=6, smooth=False))
# Kaynak dikişleri (iç köşelerde ince yuvarlak)
for (a, b) in (((X0 + T, T + 0.3, Z0 + T), (X0 + T, T + 0.3, Z1 - T)), ((X1 - T, T + 0.3, Z0 + T), (X1 - T, T + 0.3, Z1 - T))):
    P.append(wire("weld", [a, ((a[0] + b[0]) / 2, a[1], (a[2] + b[2]) / 2), b], 0.35, CAB))

# Kapak: sol menteşeden ~110 derece dışarı açık
hinge = (X0 - 0.5, D + 0.8)
ang = math.radians(110)
door_w, door_t = X1 - X0, 1.4
dx, dy = math.cos(ang), math.sin(ang)
door = box("door", (0, -door_t / 2, Z0), (door_w, door_t / 2, Z1), CAB, 0.3)
door.location = to_bl(hinge[0] + dx * door_w / 2, hinge[1] + dy * door_w / 2, (Z0 + Z1) / 2)
door.rotation_euler = (0.0, 0.0, -ang)  # Blender'da Y ters
P.append(door)
for hz in (Z0 + 12, Z1 - 12):
    P.append(cyl("hinge", (hinge[0], hinge[1], hz), 1.1, 9.0, 'Z', "MI_NL_Metal", seg=12))
# Kapağın iç yüzünde takviye çıtaları
for f in (0.25, 0.75):
    cx, cy = hinge[0] + dx * door_w * 0.5, hinge[1] + dy * door_w * 0.5
    st = box("doorrib", (-door_w * 0.42, -0.6, 0), (door_w * 0.42, 0.6, 2.2), CAB, 0.2)
    st.location = to_bl(cx - dy * 1.2, cy + dx * 1.2, Z0 + (Z1 - Z0) * f)
    st.rotation_euler = (0.0, 0.0, -ang)
    P.append(st)

# Üst sıra: 3 dikey sigorta yuvası (CAM 1-3)
SLOT_X = (-18.0, 0.0, 18.0)
FZ, FY = 62.0, 6.5
for i, sx in enumerate(SLOT_X):
    P.append(box("ins%d" % i, (sx - 3.2, T + 1.0, FZ - 13), (sx + 3.2, T + 2.2, FZ + 13), "MI_NL_FuseCeramic", 0.3))
    for s in (-1, 1):
        cz = FZ + s * 10.4
        P.append(box("clip%d" % i, (sx - 2.0, T + 2.2, cz - 1.2), (sx + 2.0, FY + 0.6, cz + 1.2), "MI_NL_Metal", 0.2))
        P.append(cyl("clipscrew", (sx, FY + 0.9, cz), 0.6, 0.6, 'Y', "MI_NL_Metal", seg=8, smooth=False))
    # LED yuvası (LED ayrı mesh)
    P.append(cyl("ledhouse%d" % i, (sx, T + 2.0, 75.5), 1.5, 2.0, 'Y', "MI_NL_FuseCabinet", seg=16))
    # Sigortanın altından göstergeye inen kablo
    P.append(wire("w_fg%d" % i, [(sx, T + 2.5, FZ - 12.5), (sx + 1.5, T + 3.5, FZ - 16), (sx - 1.0, T + 2.5, 49.5)], 0.35, "MI_NL_Rubber"))
    # LED'den yukarı kıvrılıp yana giden kablo
    P.append(wire("w_led%d" % i, [(sx, T + 1.5, 77.0), (sx + 4, T + 2.5, 78.0), (sx + 9, T + 1.6, 76.5)], 0.3, "MI_NL_Rubber"))

# CAM göstergeleri + etiketler
for i, sx in enumerate(SLOT_X):
    gauge("g%d" % i, sx, 42.0, 6.0, T + 1.0, P)
    tape("lbl%d" % i, "CAM %d" % (i + 1), sx, 33.0, 13.0, 3.6, P)

# WARNING levhası
P.append(box("warn", (-27.5, T + 1.0, 20.0), (1.5, T + 1.8, 27.0), "MI_NL_WarnPlate", 0.25))
for bx in (-26.5, 0.5):
    P.append(cyl("warnbolt", (bx, T + 2.0, 23.5), 0.5, 0.5, 'Y', "MI_NL_Metal", seg=6, smooth=False))
tri = bmesh.new()
v = [tri.verts.new(to_bl(*p)) for p in ((-24.5, T + 1.9, 21.3), (-21.3, T + 1.9, 21.3), (-22.9, T + 1.9, 25.4))]
tri.faces.new(v)
P.append(finish(new_obj("warntri", tri), "MI_NL_Ink"))
P.append(text("warntext", "WARNING", (-10.0, T + 1.9, 23.4), 3.6, "MI_NL_Ink", spacing=1.05))

# Yatay sigorta yuvası (FLASH)
HX, HZ = 16.0, 23.0
P.append(box("insH", (HX - 13, T + 1.0, HZ - 3.2), (HX + 13, T + 2.2, HZ + 3.2), "MI_NL_FuseCeramic", 0.3))
for s in (-1, 1):
    cx = HX + s * 10.4
    P.append(box("clipH", (cx - 1.2, T + 2.2, HZ - 2.0), (cx + 1.2, FY + 0.6, HZ + 2.0), "MI_NL_Metal", 0.2))
P.append(cyl("ledhouseH", (HX + 14.5, T + 2.0, HZ + 4.0), 1.5, 2.0, 'Y', "MI_NL_FuseCabinet", seg=16))

# Büyük ana gösterge
gauge("gm", -14.0, 4.0, 10.0, T + 1.0, P, ticks=11)
# FLASH göstergesi
gauge("gf", 20.0, 9.0, 5.5, T + 1.0, P)
tape("lblF", "FLASH", 20.0, 0.5, 12.0, 3.4, P, size=2.2)

# MAIN şalter gövdesi (kol ayrı mesh) + dikey etiket
P.append(box("mainbase", (1.5, T + 1.0, -4.0), (8.5, T + 4.0, 15.0), CAB, 0.4))
P.append(box("mainslot", (3.6, T + 4.0, -2.0), (6.4, T + 4.6, 13.0), "MI_NL_Rubber", 0.1))
P.append(cyl("mainaxle", (5.0, 7.0, 6.0), 1.3, 7.5, 'X', "MI_NL_Metal", seg=12))
tape("lblM", "MAIN", 11.0, 6.0, 3.6, 15.0, P, vertical=True, size=2.6)

# Göstergeler arası ve aşağı inen kalın kablolar
P.append(wire("w_main1", [(-14, T + 2.0, 15.5), (-8, T + 4.5, 18.0), (5, T + 3.0, 16.5)], 0.45, "MI_NL_Rubber"))
P.append(wire("w_main2", [(5, T + 2.0, -4.5), (3, T + 4.0, -8.5), (-6, T + 2.5, -10.4)], 0.55, "MI_NL_Rubber"))
P.append(wire("w_flash", [(20, T + 2.5, 15.0), (24, T + 4.0, 17.0), (HX + 14.5, T + 2.5, HZ + 2.0)], 0.35, "MI_NL_Rubber"))
P.append(wire("w_cams", [(-18, T + 2.5, 35.5), (-22, T + 4.5, 29.0), (-30.0, T + 3.0, 18.0), (-28.5, T + 2.5, 2.0), (-24, T + 2.5, 4.0)], 0.45, "MI_NL_Rubber"))
P.append(wire("w_cam2", [(0, T + 2.5, 35.5), (3, T + 4.0, 29.5), (8.0, T + 3.0, 27.5)], 0.4, "MI_NL_Rubber"))
P.append(wire("w_bottom", [(-6, T + 2.5, -10.4), (-6.5, T + 3.0, -11.5), (-7.0, T + 3.5, -16.0)], 0.9, "MI_NL_Rubber"))
cab = join("SM_NL_FuseCabinet", P)
export(cab)

# ------------------------------------------------------------------ sigorta kartuşu (Z ekseninde, ortalı, 18 cm)
P = []
P.append(cyl("cer", (0, 0, 0), 1.3, 12.0, 'Z', "MI_NL_FuseCeramic", seg=20))
for s in (-1, 1):
    P.append(cyl("cap", (0, 0, s * 7.5), 1.55, 3.0, 'Z', "MI_NL_Metal", seg=20))
    P.append(cyl("capring", (0, 0, s * 6.2), 1.7, 0.5, 'Z', "MI_NL_Metal", seg=20))
    P.append(cyl("capend", (0, 0, s * 9.2), 0.9, 0.5, 'Z', "MI_NL_Metal", seg=12))
P.append(cyl("band", (0, 0, 0), 1.38, 3.2, 'Z', "MI_NL_FuseBand", seg=20))
export(join("SM_NL_FuseCartridge", P))

# ------------------------------------------------------------------ şalter kolu (Z ekseni, ortalı, 14 cm; alt uç mafsal)
P = []
P.append(cyl("rod", (0, 0, -1.0), 0.65, 12.0, 'Z', "MI_NL_Metal", seg=12))
P.append(cyl("hub", (0, 0, -6.2), 1.2, 3.4, 'X', "MI_NL_Metal", seg=16))
P.append(cyl("grip", (0, 0, 5.2), 1.35, 4.0, 'Z', "MI_NL_Rubber", seg=16))
P.append(cyl("gripend", (0, 0, 7.4), 1.0, 0.6, 'Z', "MI_NL_Rubber", seg=16, r2=0.6))
export(join("SM_NL_FuseLever", P))

# ------------------------------------------------------------------ gösterge ibresi (mafsal orijinde, +Z yönüne 5 cm)
P = []
nb = bmesh.new()
pts = [(-0.18, 0, -0.6), (0.18, 0, -0.6), (0.05, 0, 5.0), (-0.05, 0, 5.0)]
vv = [nb.verts.new(to_bl(*p)) for p in pts]
nb.faces.new(vv)
bmesh.ops.solidify(nb, geom=nb.faces[:], thickness=0.12 * C)
P.append(finish(new_obj("needle", nb), "MI_NL_Ink"))
P.append(cyl("pin", (0, 0.15, 0), 0.45, 0.35, 'Y', "MI_NL_Metal", seg=12))
export(join("SM_NL_GaugeNeedle", P))

# ------------------------------------------------------------------ LED (yarım küre, +Y'ye bakar)
bpy.ops.mesh.primitive_uv_sphere_add(radius=0.85 * C, segments=16, ring_count=8, location=(0, 0, 0))
led = bpy.context.active_object
led.name = "SM_NL_FuseLED"
led.scale = (1.0, 0.6, 1.0)
finish(led, "MI_NL_LedGreen", smooth=True)
export(join("SM_NL_FuseLED", [led]))
print("NLFuseModel: done ->", OUT)

# ------------------------------------------------------------------ önizleme (NL_FUSE_PREVIEW=1): kartuş/kol/ibre yerlerine konup render alınır
if os.environ.get("NL_FUSE_PREVIEW") == "1":
    cols = {"MI_NL_FuseCabinet": (0.16, 0.17, 0.17, 1), "MI_NL_Metal": (0.55, 0.55, 0.55, 1), "MI_NL_FuseCeramic": (0.75, 0.74, 0.7, 1),
            "MI_NL_Rubber": (0.03, 0.03, 0.03, 1), "MI_NL_GaugeFace": (0.85, 0.82, 0.72, 1), "MI_NL_Ink": (0.02, 0.02, 0.02, 1),
            "MI_NL_Tape": (0.8, 0.75, 0.6, 1), "MI_NL_WarnPlate": (0.75, 0.6, 0.2, 1), "MI_NL_FuseBand": (0.8, 0.1, 0.1, 1),
            "MI_NL_LedGreen": (0.1, 1.0, 0.2, 1)}
    for n, c in cols.items():
        if n in MATS:
            MATS[n].diffuse_color = c
    cart = bpy.data.objects["SM_NL_FuseCartridge"]
    lever = bpy.data.objects["SM_NL_FuseLever"]
    needle = bpy.data.objects["SM_NL_GaugeNeedle"]
    ledo = bpy.data.objects["SM_NL_FuseLED"]
    for i, sx in enumerate(SLOT_X):
        d = cart.copy(); d.data = cart.data; scene.collection.objects.link(d); d.location = to_bl(sx, FY, FZ)
        l2 = ledo.copy(); scene.collection.objects.link(l2); l2.location = to_bl(sx, T + 3.2, 75.5)
        n2 = needle.copy(); scene.collection.objects.link(n2); n2.location = to_bl(sx, T + 4.6, 42.0 - 6.0 * 0.3); n2.rotation_euler = (0, math.radians(25), 0)
    d = cart.copy(); scene.collection.objects.link(d); d.location = to_bl(HX, FY, HZ); d.rotation_euler = (0, math.radians(90), 0)
    n2 = needle.copy(); scene.collection.objects.link(n2); n2.location = to_bl(-14, T + 4.6, 1.0); n2.scale = (1.6, 1.6, 1.6); n2.rotation_euler = (0, math.radians(30), 0)
    n2 = needle.copy(); scene.collection.objects.link(n2); n2.location = to_bl(20, T + 4.6, 9 - 5.5 * 0.3); n2.rotation_euler = (0, math.radians(20), 0)
    a = math.radians(55)
    lever.location = to_bl(5, 7 + math.cos(a) * 7, 6 + math.sin(a) * 7)
    lever.rotation_euler = (math.radians(90) - a, 0, 0)
    cart.location = (100, 0, 0); needle.location = (100, 0, 0); ledo.location = (100, 0, 0)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
    scene.collection.objects.link(cam)
    cam.location = to_bl(-8, 150, 40)
    cam.rotation_euler = (math.radians(90), 0, math.radians(-3))
    cam.data.lens = 35
    scene.camera = cam
    scene.render.engine = 'BLENDER_WORKBENCH'
    scene.display.shading.light = 'STUDIO'
    scene.display.shading.color_type = 'MATERIAL'
    scene.display.shading.show_cavity = True
    scene.display.shading.show_shadows = True
    scene.render.resolution_x, scene.render.resolution_y = 1100, 1100
    scene.render.filepath = os.path.join(OUT, "preview.png")
    bpy.ops.render.render(write_still=True)
    print("NLFuseModel: preview ->", scene.render.filepath)
