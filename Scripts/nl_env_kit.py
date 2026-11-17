# No Light After Midnight - modüler çevre kiti (GeometryScripting ile) -> /Game/Environment/Kit
# Kütük, tahta, kiremit sırası, döşeme paneli, cam, fener, soba borusu, kazık, varil, sandık, halat kangalı,
# fener direği, çam ağaçları (3 çeşit + kuru), kayalar (3), kütük (kesik ağaç).
# Ölçü kuralları Source/.../NLEnvKit.h dosyasında. Yeniden çalıştırılabilir (var olan assetler güncellenir).
# Çalıştırma: UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<tam yol>\Scripts\nl_env_kit.py"
import math
import os
import random

import unreal

KIT = "/Game/Environment/Kit"
MAT = "/Game/Environment/Materials/"
P = unreal.GeometryScript_Primitives
NA = unreal.GeometryScript_NewAssetUtils
DF = unreal.GeometryScript_MeshDeformers
TR = unreal.GeometryScript_MeshTransforms
ED = unreal.GeometryScript_MeshEdits
UV = unreal.GeometryScript_UVs
NR = unreal.GeometryScript_Normals
COL = unreal.GeometryScript_Collision
Q = unreal.GeometryScript_MeshQueries
AU = unreal.GeometryScript_AssetUtils
eal = unreal.EditorAssetLibrary
CENTER = unreal.GeometryScriptPrimitiveOriginMode.CENTER
BASE = unreal.GeometryScriptPrimitiveOriginMode.BASE
LOG = []


def log(m):
    LOG.append(m)
    unreal.log("NLKit: " + m)


def mesh():
    return unreal.new_object(unreal.DynamicMesh)


def opts(mat=0):
    o = unreal.GeometryScriptPrimitiveOptions()
    o.material_id = mat
    return o


def T(loc=(0, 0, 0), rot=(0, 0, 0), scale=(1, 1, 1)):
    # rot = (pitch, yaw, roll)
    return unreal.Transform(unreal.Vector(*loc), unreal.Rotator(roll=rot[2], pitch=rot[0], yaw=rot[1]), unreal.Vector(*scale))


def noise(m, mag, freq, seed):
    o = unreal.GeometryScriptPerlinNoiseOptions()
    l = unreal.GeometryScriptPerlinNoiseLayerOptions()
    l.magnitude = mag
    l.frequency = freq
    l.random_seed = seed
    o.base_layer = l
    o.apply_along_normal = True
    o.empty_behavior = unreal.GeometryScriptEmptySelectionBehavior.FULL_MESH_SELECTION
    DF.apply_perlin_noise_to_mesh2(m, unreal.GeometryScriptMeshSelection(), o)


def normals(m):
    NR.recompute_normals(m, unreal.GeometryScriptCalculateNormalsOptions())


def box_uv(m, size=100.0, rotate=0.0):
    UV.set_mesh_u_vs_from_box_projection(m, 0, T(scale=(size, size, size)), unreal.GeometryScriptMeshSelection())
    if rotate:
        UV.rotate_mesh_u_vs(m, 0, rotate, unreal.Vector2D(0, 0), unreal.GeometryScriptMeshSelection())


def append(dst, src, t=None):
    ED.append_mesh(dst, src, t or T())


def mat(name):
    return unreal.load_asset(MAT + name)


def save(m, name, mats, coll=None, method=None, hulls=1):
    path = KIT + "/" + name
    if eal.does_asset_exist(path):
        sm = unreal.load_asset(path)
        try:
            AU.copy_mesh_to_static_mesh(m, sm, unreal.GeometryScriptCopyMeshToAssetOptions(), unreal.GeometryScriptMeshWriteLOD())
        except Exception as e:
            log("UYARI: %s guncellenemedi (%s)" % (name, e))
    else:
        o = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
        o.enable_recompute_normals = False
        o.enable_recompute_tangents = True
        o.enable_collision = False
        o.enable_nanite = False
        res = NA.create_new_static_mesh_asset_from_mesh(m, path, o)
        sm = res[0] if isinstance(res, tuple) else res
    if not sm:
        log("HATA: %s olusturulamadi" % name)
        return None
    slots = []
    for i, mn in enumerate(mats):
        s = unreal.StaticMaterial()
        s.set_editor_property("material_interface", mat(mn))
        s.set_editor_property("material_slot_name", "Slot%d" % i)
        slots.append(s)
    sm.set_editor_property("static_materials", slots)
    if coll is not None:
        co = unreal.GeometryScriptCollisionFromMeshOptions()
        co.method = method or unreal.GeometryScriptCollisionGenerationMethod.CONVEX_HULLS
        co.max_convex_hulls_per_mesh = hulls
        COL.set_static_mesh_collision_from_mesh(coll, sm, co)
    eal.save_loaded_asset(sm)
    b = Q.get_mesh_bounding_box(m)
    log("%s: min(%.0f,%.0f,%.0f) max(%.0f,%.0f,%.0f), %d tris" % (name, b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z, Q.get_num_triangle_i_ds(m)))
    return sm


# ------------------------------------------------------------------ yapı parçaları

def make_log():
    # X boyunca 100 cm, çap 30; gövde kabuk, iki uç yıllık halka
    body = mesh()
    P.append_cylinder(body, opts(0), T(rot=(90, 0, 0)), 15.0, 100.0, 14, 6, False, CENTER)
    noise(body, 1.1, 0.03, 5)
    box_uv(body, 100.0)
    normals(body)
    for x in (-49.6, 49.6):
        cap = mesh()
        P.append_cylinder(cap, opts(1), T(loc=(x, 0, 0), rot=(90, 0, 0)), 14.3, 0.8, 14, 0, True, CENTER)
        append(body, cap)
    coll = mesh()
    P.append_cylinder(coll, opts(0), T(rot=(90, 0, 0)), 15.0, 100.0, 10, 0, True, CENTER)
    save(body, "SM_NL_Log", ["MI_NL_LogBark", "MI_NL_LogEnd"], coll)


def make_board():
    m = mesh()
    P.append_box(m, opts(0), T(), 100.0, 20.0, 4.0, 0, 0, 0, CENTER)
    box_uv(m, 100.0)
    save(m, "SM_NL_Board", ["MI_NL_Wood"], m, unreal.GeometryScriptCollisionGenerationMethod.ALIGNED_BOXES)


def make_shingle_row():
    r = random.Random(4)
    m = mesh()
    x = -50.0
    while x < 49.0:
        w = min(r.uniform(14.0, 24.0), 50.0 - x)
        if w > 4.0:
            L = 40.0 + r.uniform(-3.0, 3.0)
            s = mesh()
            P.append_box(s, opts(0), T(), w - 1.2, L, 2.0, 0, 0, 0, CENTER)
            box_uv(s, 60.0, 90.0)
            append(m, s, T(loc=(x + w * 0.5, L * 0.5, 1.2 + r.uniform(-0.3, 0.3)), rot=(0, r.uniform(-1.5, 1.5), -4.0)))
        x += w
    save(m, "SM_NL_ShingleRow", ["MI_NL_Shingle"])


def make_deck_panel():
    r = random.Random(8)
    m = mesh()
    for i in range(5):
        y = -50.0 + 10.0 + i * 20.0
        # Bazı tahtalar iki parça (ek yeri)
        pieces = [(-100.0, 100.0)] if r.random() < 0.6 else [(-100.0, j := r.uniform(-60, 60)), (j, 100.0)]
        for a, b in pieces:
            s = mesh()
            P.append_box(s, opts(0), T(), (b - a) - 0.8, 18.0, 4.0, 0, 0, 0, CENTER)
            box_uv(s, 100.0)
            append(m, s, T(loc=((a + b) * 0.5 + r.uniform(-1, 1), y, -2.0 + r.uniform(-0.35, 0.35)), rot=(r.uniform(-0.3, 0.3), r.uniform(-0.4, 0.4), r.uniform(-0.6, 0.6))))
    coll = mesh()
    P.append_box(coll, opts(0), T(loc=(0, 0, -2)), 200.0, 100.0, 4.0, 0, 0, 0, CENTER)
    save(m, "SM_NL_DeckPanel", ["MI_NL_DeckWood"], coll, unreal.GeometryScriptCollisionGenerationMethod.ALIGNED_BOXES)


def make_pane():
    m = mesh()
    P.append_box(m, opts(0), T(), 100.0, 1.0, 100.0, 0, 0, 0, CENTER)
    save(m, "SM_NL_Pane", ["MI_NL_WindowGlow"], m, unreal.GeometryScriptCollisionGenerationMethod.ALIGNED_BOXES)


def make_lantern():
    m = mesh()
    P.append_box(m, opts(0), T(loc=(0, 0, -2.5)), 1.5, 1.5, 5.0, 0, 0, 0, CENTER)
    P.append_torus(m, opts(0), T(loc=(0, 0, 0), rot=(0, 0, 90)), unreal.GeometryScriptRevolveOptions(), 3.0, 0.6, 10, 4, CENTER)
    P.append_cone(m, opts(0), T(loc=(0, 0, -14)), 11.0, 2.5, 9.0, 12, 1, True, BASE)
    P.append_box(m, opts(1), T(loc=(0, 0, -25)), 13.0, 13.0, 17.0, 0, 0, 0, CENTER)
    for sx in (-1, 1):
        for sy in (-1, 1):
            P.append_box(m, opts(0), T(loc=(sx * 7.0, sy * 7.0, -25)), 1.6, 1.6, 18.0, 0, 0, 0, CENTER)
    P.append_cylinder(m, opts(0), T(loc=(0, 0, -36)), 9.5, 3.0, 12, 0, True, BASE)
    save(m, "SM_NL_Lantern", ["MI_NL_Metal", "MI_NL_LanternGlass"])


def make_stovepipe():
    m = mesh()
    P.append_cylinder(m, opts(0), T(), 9.0, 150.0, 12, 0, False, BASE)
    P.append_cylinder(m, opts(0), T(loc=(0, 0, 40)), 10.0, 3.0, 12, 0, False, BASE)
    P.append_cone(m, opts(0), T(loc=(0, 0, 156)), 17.0, 2.0, 10.0, 12, 1, True, BASE)
    for a in range(3):
        ang = a * 2.0944
        P.append_box(m, opts(0), T(loc=(math.cos(ang) * 8, math.sin(ang) * 8, 152)), 1.2, 1.2, 9.0, 0, 0, 0, CENTER)
    save(m, "SM_NL_Stovepipe", ["MI_NL_Metal"])


def make_post():
    # Kazık: X boyunca kurulur (damar boyuna gelsin diye UV), sonra dikilir; taban z=0, 100 cm
    m = mesh()
    P.append_cylinder(m, opts(0), T(rot=(90, 0, 0)), 12.0, 100.0, 10, 4, False, CENTER)
    noise(m, 0.8, 0.03, 9)
    box_uv(m, 100.0)
    normals(m)
    TR.rotate_mesh(m, unreal.Rotator(roll=0, pitch=90, yaw=0), unreal.Vector(0, 0, 0))
    b = Q.get_mesh_bounding_box(m)
    TR.translate_mesh(m, unreal.Vector(0, 0, -b.min.z))
    P.append_cylinder(m, opts(1), T(loc=(0, 0, 100.0)), 11.5, 0.8, 10, 0, True, BASE)
    coll = mesh()
    P.append_cylinder(coll, opts(0), T(), 12.0, 100.0, 8, 0, True, BASE)
    save(m, "SM_NL_Post", ["MI_NL_LogBark", "MI_NL_LogEnd"], coll)


def make_barrel():
    m = mesh()
    P.append_cone(m, opts(0), T(), 24.0, 28.5, 45.0, 18, 2, False, BASE)
    P.append_cone(m, opts(0), T(loc=(0, 0, 45)), 28.5, 24.0, 45.0, 18, 2, False, BASE)
    UV.rotate_mesh_u_vs(m, 0, 90.0, unreal.Vector2D(0, 0), unreal.GeometryScriptMeshSelection())
    P.append_cylinder(m, opts(0), T(loc=(0, 0, 87.5)), 23.6, 1.5, 18, 0, True, BASE)
    for z, r in ((14.0, 26.6), (72.0, 26.6), (40.0, 28.6)):
        P.append_cylinder(m, opts(1), T(loc=(0, 0, z)), r, 4.0, 18, 0, False, BASE)
    coll = mesh()
    P.append_cylinder(coll, opts(0), T(), 27.0, 90.0, 10, 0, True, BASE)
    save(m, "SM_NL_Barrel", ["MI_NL_Wood", "MI_NL_Metal"], coll)


def make_crate():
    m = mesh()
    s = mesh()
    P.append_box(s, opts(0), T(loc=(0, 0, 35)), 64.0, 64.0, 64.0, 0, 0, 0, CENTER)
    for ax in range(3):
        for a in (-31.0, 31.0):
            for b in (-31.0, 31.0):
                loc = [0.0, 0.0, 35.0]
                dims = [8.0, 8.0, 8.0]
                dims[ax] = 70.0
                o = [i for i in range(3) if i != ax]
                loc[o[0]] += a
                loc[o[1]] += b
                P.append_box(s, opts(0), T(loc=tuple(loc)), dims[0], dims[1], dims[2], 0, 0, 0, CENTER)
    box_uv(s, 70.0)
    append(m, s)
    save(m, "SM_NL_Crate", ["MI_NL_Crate"], s, unreal.GeometryScriptCollisionGenerationMethod.ALIGNED_BOXES)


def make_rope_coil():
    m = mesh()
    for i, (r, z) in enumerate(((26.0, 3.5), (22.0, 10.0), (17.5, 16.0), (24.0, 9.0))):
        P.append_torus(m, opts(0), T(loc=(0, 0, z)), unreal.GeometryScriptRevolveOptions(), r, 3.6, 18, 6, CENTER)
    save(m, "SM_NL_RopeCoil", ["MI_NL_Rope"])


def make_lantern_post():
    m = mesh()
    s = mesh()
    P.append_box(s, opts(0), T(loc=(0, 0, 160)), 14.0, 14.0, 320.0, 0, 0, 0, CENTER)
    P.append_box(s, opts(0), T(loc=(42, 0, 304)), 98.0, 10.0, 10.0, 0, 0, 0, CENTER)
    P.append_box(s, opts(0), T(loc=(26, 0, 276), rot=(40, 0, 0)), 70.0, 8.0, 8.0, 0, 0, 0, CENTER)
    box_uv(s, 100.0)
    append(m, s)
    P.append_box(m, opts(1), T(loc=(80, 0, 296)), 1.2, 1.2, 8.0, 0, 0, 0, CENTER)
    coll = mesh()
    P.append_box(coll, opts(0), T(loc=(0, 0, 160)), 14.0, 14.0, 320.0, 0, 0, 0, CENTER)
    save(m, "SM_NL_LanternPost", ["MI_NL_Wood", "MI_NL_Metal"], coll, unreal.GeometryScriptCollisionGenerationMethod.ALIGNED_BOXES)


# ------------------------------------------------------------------ doğa

def make_pine(name, height, layers, base_r, seed, dead=False):
    r = random.Random(seed)
    trunk = mesh()
    P.append_cone(trunk, opts(0), T(), 17.0, 3.0, height * (0.75 if dead else 0.97), 9, 8, False, BASE)
    noise(trunk, 1.8, 0.02, seed)
    normals(trunk)
    m = mesh()
    append(m, trunk)
    if dead:
        # Kuru çam: çıplak, kırık dallar
        for i in range(9):
            z = height * r.uniform(0.25, 0.7)
            L = r.uniform(60, 160) * (1 - z / height)
            ang = r.uniform(0, 360)
            b = mesh()
            P.append_cone(b, opts(0), T(), 4.0, 0.8, L, 5, 1, False, BASE)
            append(m, b, T(loc=(0, 0, z), rot=(r.uniform(-75, -40), ang, 0)))
    else:
        fol = mesh()
        for i in range(layers):
            t = i / float(layers - 1)
            z0 = height * (0.2 + t * 0.7)
            rad = base_r * (1.0 - t * 0.82) * r.uniform(0.88, 1.1)
            h = rad * r.uniform(1.15, 1.4)
            P.append_cone(fol, opts(1), T(loc=(r.uniform(-8, 8), r.uniform(-8, 8), z0), rot=(r.uniform(-4, 4), r.uniform(0, 360), r.uniform(-4, 4))),
                          rad, rad * 0.06, h, 11, 3, True, BASE)
        noise(fol, base_r * 0.11, 0.012, seed + 1)
        normals(fol)
        append(m, fol)
    coll = mesh()
    P.append_cylinder(coll, opts(0), T(), 20.0, 320.0, 8, 0, True, BASE)
    save(m, name, ["MI_NL_PineBark", "MI_NL_Needles"], coll)


def make_rock(name, seed, scale):
    m = mesh()
    P.append_sphere_box(m, opts(0), T(), 100.0, 10, 10, 10, CENTER)
    noise(m, 30.0, 0.007, seed)
    noise(m, 8.0, 0.03, seed + 7)
    TR.scale_mesh(m, unreal.Vector(*scale), unreal.Vector(0, 0, 0))
    TR.translate_mesh(m, unreal.Vector(0, 0, 100.0 * scale[2] * 0.55))
    box_uv(m, 220.0)
    normals(m)
    save(m, name, ["MI_NL_Rock"], m, unreal.GeometryScriptCollisionGenerationMethod.CONVEX_HULLS)


def make_stump():
    m = mesh()
    P.append_cone(m, opts(0), T(), 30.0, 24.0, 45.0, 12, 3, False, BASE)
    noise(m, 2.5, 0.03, 31)
    normals(m)
    P.append_disc(m, opts(1), T(loc=(0, 0, 44.5)), 23.5, 12, 0, 0.0, 360.0, 0.0)
    coll = mesh()
    P.append_cylinder(coll, opts(0), T(), 28.0, 45.0, 8, 0, True, BASE)
    save(m, "SM_NL_Stump", ["MI_NL_PineBark", "MI_NL_LogEnd"], coll)


_skip_old = os.environ.get("NL_KIT_ONLY_NEW") == "1"
for fn in (make_log, make_board, make_shingle_row, make_deck_panel, make_pane, make_lantern, make_stovepipe,
           make_post, make_barrel, make_crate, make_rope_coil, make_lantern_post, make_stump):
    if _skip_old:
        break
    try:
        fn()
    except Exception as e:
        log("HATA %s: %s" % (fn.__name__, e))

if not _skip_old:
    for args in (("SM_NL_PineA", 1250.0, 9, 270.0, 101), ("SM_NL_PineB", 1050.0, 8, 240.0, 202), ("SM_NL_PineC", 1450.0, 10, 230.0, 303)):
        try:
            make_pine(*args)
        except Exception as e:
            log("HATA %s: %s" % (args[0], e))
    try:
        make_pine("SM_NL_DeadPine", 1000.0, 0, 0.0, 404, dead=True)
    except Exception as e:
        log("HATA dead pine: %s" % e)
    for name, seed, sc in (("SM_NL_RockA", 11, (1.4, 1.0, 0.6)), ("SM_NL_RockB", 22, (1.0, 0.9, 0.8)), ("SM_NL_RockC", 33, (1.8, 1.1, 0.45))):
        try:
            make_rock(name, seed, sc)
        except Exception as e:
            log("HATA %s: %s" % (name, e))

with open(os.path.join(unreal.Paths.project_saved_dir(), "NLKit.log"), "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))


# ------------------------------------------------------------------ deniz feneri (yeni) + toz

BOOL = unreal.GeometryScript_MeshBooleans


def boolean_subtract(target, tool):
    BOOL.apply_mesh_boolean(target, T(), tool, T(), unreal.GeometryScriptBooleanOperation.SUBTRACT, unreal.GeometryScriptMeshBooleanOptions())


def complex_collision(sm):
    try:
        bs = sm.get_editor_property("body_setup")
        bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        eal.save_loaded_asset(sm)
    except Exception as e:
        log("UYARI karmasik carpisma: %s" % e)


def make_lh_tower():
    # Daralan kule (taban r260, tepe r170, 1000 cm). Zemin katında içine girilen oda (60..330) ve +X yönünde kapı.
    m = mesh()
    P.append_cone(m, opts(0), T(), 260.0, 170.0, 1000.0, 28, 10, True, BASE)
    room = mesh()
    P.append_cone(room, opts(0), T(loc=(0, 0, 40)), 228.0, 216.0, 290.0, 28, 1, True, BASE)
    boolean_subtract(m, room)
    door = mesh()
    P.append_box(door, opts(0), T(loc=(240, 0, 40)), 120.0, 120.0, 215.0, 0, 0, 0, BASE)
    boolean_subtract(m, door)
    normals(m)
    sm = save(m, "SM_NL_LH_Tower", ["MI_NL_LighthousePaint"])
    if sm:
        complex_collision(sm)


def make_lh_base():
    m = mesh()
    P.append_cylinder(m, opts(0), T(), 330.0, 40.0, 8, 0, True, BASE)
    noise(m, 2.0, 0.02, 61)
    box_uv(m, 200.0)
    normals(m)
    door = mesh()
    save(m, "SM_NL_LH_Base", ["MI_NL_Rock"], m, unreal.GeometryScriptCollisionGenerationMethod.CONVEX_HULLS)


def make_lh_gallery():
    m = mesh()
    P.append_cylinder(m, opts(0), T(), 232.0, 14.0, 28, 0, True, BASE)
    for i in range(14):
        a = i * 360.0 / 14
        ca, sa = math.cos(math.radians(a)), math.sin(math.radians(a))
        P.append_box(m, opts(0), T(loc=(ca * 190, sa * 190, -30), rot=(-35, a, 0)), 90.0, 8.0, 12.0, 0, 0, 0, CENTER)
    for i in range(28):
        a = math.radians(i * 360.0 / 28)
        P.append_box(m, opts(0), T(loc=(math.cos(a) * 224, math.sin(a) * 224, 14)), 3.0, 3.0, 95.0, 0, 0, 0, BASE)
    for z in (60.0, 108.0):
        P.append_torus(m, opts(0), T(loc=(0, 0, z)), unreal.GeometryScriptRevolveOptions(), 224.0, 2.6 if z > 100 else 1.6, 40, 6, CENTER)
    coll = mesh()
    P.append_cylinder(coll, opts(0), T(), 232.0, 14.0, 16, 0, True, BASE)
    save(m, "SM_NL_LH_Gallery", ["MI_NL_LH_Metal"], coll)


def make_lh_lantern_room():
    m = mesh()
    P.append_cylinder(m, opts(0), T(), 138.0, 70.0, 10, 0, True, BASE)
    P.append_cylinder(m, opts(1), T(loc=(0, 0, 70)), 130.0, 180.0, 10, 0, False, BASE)
    for i in range(10):
        a = math.radians(i * 36.0 + 18.0)
        P.append_box(m, opts(0), T(loc=(math.cos(a) * 128, math.sin(a) * 128, 70)), 5.0, 5.0, 180.0, 0, 0, 0, BASE)
    P.append_torus(m, opts(0), T(loc=(0, 0, 250)), unreal.GeometryScriptRevolveOptions(), 135.0, 6.0, 30, 6, CENTER)
    P.append_cone(m, opts(0), T(loc=(0, 0, 250)), 165.0, 14.0, 95.0, 20, 2, True, BASE)
    P.append_sphere_lat_long(m, opts(0), T(loc=(0, 0, 360)), 17.0, 10, 12, CENTER)
    P.append_cone(m, opts(0), T(loc=(0, 0, 374)), 3.5, 0.5, 45.0, 6, 1, True, BASE)
    save(m, "SM_NL_LH_LanternRoom", ["MI_NL_LH_Metal", "MI_NL_LH_Glass"])


def make_lh_lens():
    m = mesh()
    z = 0.0
    for i in range(9):
        r = 46.0 + (5.0 if i % 2 else 0.0) - abs(i - 4) * 2.5
        P.append_cylinder(m, opts(0), T(loc=(0, 0, z)), r, 13.0, 16, 0, True, BASE)
        z += 13.0
    P.append_cylinder(m, opts(1), T(loc=(0, 0, -30)), 30.0, 30.0, 12, 0, True, BASE)
    save(m, "SM_NL_LH_Lens", ["MI_NL_LensGlow", "MI_NL_LH_Metal"])


def make_lh_beam():
    # Işık huzmesi: +X boyunca 4000 cm, uçta r 320 (materyal LocalPosition.x ile söner)
    m = mesh()
    P.append_cone(m, opts(0), T(), 22.0, 320.0, 4000.0, 20, 6, False, BASE)
    TR.rotate_mesh(m, unreal.Rotator(roll=0, pitch=-90, yaw=0), unreal.Vector(0, 0, 0))
    b = Q.get_mesh_bounding_box(m)
    if b.max.x < 100.0:
        TR.rotate_mesh(m, unreal.Rotator(roll=0, pitch=180, yaw=0), unreal.Vector(0, 0, 0))
    save(m, "SM_NL_LH_Beam", ["MI_NL_Beam"])


def make_dust():
    m = mesh()
    P.append_sphere_box(m, opts(0), T(), 0.4, 2, 2, 2, CENTER)
    save(m, "SM_NL_Dust", ["M_NL_DustSoft"])


def make_sign():
    m = mesh()
    s = mesh()
    P.append_box(s, opts(0), T(loc=(0, 0, 85)), 10.0, 10.0, 170.0, 0, 0, 0, CENTER)
    P.append_box(s, opts(0), T(loc=(0, 0, 150)), 6.0, 110.0, 30.0, 0, 0, 0, CENTER)
    P.append_box(s, opts(0), T(loc=(0, 62, 150)), 6.0, 22.0, 22.0, 0, 0, 0, CENTER)
    box_uv(s, 100.0)
    append(m, s)
    coll = mesh()
    P.append_box(coll, opts(0), T(loc=(0, 0, 85)), 10.0, 10.0, 170.0, 0, 0, 0, CENTER)
    save(m, "SM_NL_Sign", ["MI_NL_Wood"], coll, unreal.GeometryScriptCollisionGenerationMethod.ALIGNED_BOXES)


_only = [s.strip() for s in os.environ.get("NL_KIT", "").split(",") if s.strip()]
for fn in (make_lh_tower, make_lh_base, make_lh_gallery, make_lh_lantern_room, make_lh_lens, make_lh_beam, make_dust, make_sign):
    if _only and fn.__name__ not in _only:
        continue
    try:
        fn()
    except Exception as e:
        log("HATA %s: %s" % (fn.__name__, e))

with open(os.path.join(unreal.Paths.project_saved_dir(), "NLKit.log"), "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))


# ------------------------------------------------------------------ tekne, motor, kürek, masa, CRT monitör, sandalye, benzin bidonu

MQ = unreal.GeometryScript_MeshQueries


def deform(m, fn):
    n = MQ.get_vertex_count(m)
    for vid in range(n):
        res = MQ.get_vertex_position(m, vid)
        p = res[0] if isinstance(res, tuple) else res
        x, y, z = fn(p.x, p.y, p.z)
        ED.set_vertex_position(m, vid, unreal.Vector(x, y, z))


BOAT_L, BOAT_B, BOAT_Z0, BOAT_Z1 = 250.0, 85.0, -24.0, 48.0


def boat_plan(u):
    if u < -0.5:
        return 0.82 + 0.18 * (u + 1.0) / 0.5
    if u <= 0.15:
        return 1.0
    v = (u - 0.15) / 0.85
    return max(0.07, math.sqrt(max(0.0, 1.0 - v * v)))


def boat_section(t):
    return 0.38 + 0.62 * (max(t, 0.0) ** 0.55)


def boat_lift(u, tt):
    return 22.0 * max(0.0, u - 0.2) ** 2 / 0.64 * tt + 34.0 * max(0.0, u - 0.35) ** 2 / 0.4225 * (1.0 - tt) + 6.0 * max(0.0, -u - 0.6) / 0.4 * (1.0 - tt)


def boat_fn(x, y, z):
    u = max(-1.0, min(1.0, x / BOAT_L))
    t = (z - BOAT_Z0) / (BOAT_Z1 - BOAT_Z0)
    tt = max(0.0, min(1.0, t))
    return x, y * boat_plan(u) * boat_section(t), z + boat_lift(u, tt)


def boat_edge(x, inner=0.0):
    # Küpeşte (üst kenar) noktası: (y, z)
    u = max(-1.0, min(1.0, x / BOAT_L))
    return (BOAT_B - inner) * boat_plan(u) * boat_section(1.0), BOAT_Z1 + boat_lift(u, 1.0)


def seg_box(m, mat, a, b, w, h):
    dx, dy, dz = b[0] - a[0], b[1] - a[1], b[2] - a[2]
    L = math.sqrt(dx * dx + dy * dy + dz * dz)
    if L < 0.5:
        return
    yaw = math.degrees(math.atan2(dy, dx))
    pitch = math.degrees(math.atan2(dz, math.sqrt(dx * dx + dy * dy)))
    P.append_box(m, opts(mat), T(loc=((a[0] + b[0]) / 2, (a[1] + b[1]) / 2, (a[2] + b[2]) / 2), rot=(pitch, yaw, 0)), L + 0.6, w, h, 0, 0, 0, CENTER)


def make_boat():
    hull = mesh()
    P.append_box(hull, opts(0), T(loc=(0, 0, BOAT_Z0)), 2 * BOAT_L, 2 * BOAT_B, BOAT_Z1 - BOAT_Z0, 32, 8, 6, BASE)
    deform(hull, boat_fn)
    inner = mesh()
    # Pruva ucu dolu kalır (ince duvarlar delik bırakmasın): iç oyuk pruvadan 45 cm önce biter
    P.append_box(inner, opts(0), T(loc=(-19.5, 0, BOAT_Z0 + 6)), 2 * BOAT_L - 51, 2 * BOAT_B - 14, 90.0, 32, 8, 6, BASE)
    deform(inner, boat_fn)
    boolean_subtract(hull, inner)
    normals(hull)
    box_uv(hull, 100.0)
    m = mesh()
    append(m, hull)
    trim = mesh()
    xs = [-BOAT_L + 2 + i * 20.0 for i in range(int((2 * BOAT_L - 14) / 20.0) + 1)]
    for side in (-1, 1):
        pts = []
        for x in xs:
            y, z = boat_edge(x, 3.0)
            pts.append((x, side * y, z + 1.5))
        for a, b in zip(pts[:-1], pts[1:]):
            seg_box(trim, 1, a, b, 8.0, 5.0)
    ys, zs = boat_edge(-BOAT_L + 2, 3.0)
    seg_box(trim, 1, (-BOAT_L + 3, -ys, zs + 1.5), (-BOAT_L + 3, ys, zs + 1.5), 8.0, 5.0)
    # Pruva güvertesi (üçgen tahta): uçtaki birleşimi örter
    for i, x0 in enumerate((196.0, 214.0, 230.0)):
        yw, zw = boat_edge(x0 + 8.0, 2.0)
        P.append_box(trim, opts(1), T(loc=(x0 + 8.0, 0, zw + 1.0)), 18.0, 2 * yw + 2.0, 3.0, 0, 0, 0, CENTER)
    yb, zb = boat_edge(BOAT_L - 4)
    seg_box(trim, 1, (BOAT_L - 6, 0, -2), (BOAT_L - 2, 0, zb + 6), 6.0, 6.0)
    for x in range(-200, 201, 50):
        u = x / BOAT_L
        yb0 = (BOAT_B - 7) * boat_plan(u) * boat_section(0.12)
        zb0 = BOAT_Z0 + 8 + boat_lift(u, 0.0)
        yt, zt = boat_edge(x, 7.0)
        for side in (-1, 1):
            seg_box(trim, 1, (x, side * yb0, zb0), (x, side * yt, zt - 3), 4.0, 4.0)
        seg_box(trim, 1, (x, -yb0, zb0 + 1), (x, yb0, zb0 + 1), 4.0, 4.0)
    for x in (-190.0, 0.0, 120.0):
        u = x / BOAT_L
        w = (BOAT_B - 7) * boat_plan(u) * boat_section((28.0 - BOAT_Z0) / (BOAT_Z1 - BOAT_Z0))
        P.append_box(trim, opts(1), T(loc=(x, 0, 26)), 26.0, 2 * w, 4.0, 0, 0, 0, CENTER)
    box_uv(trim, 100.0)
    append(m, trim)
    floor = mesh()
    for y in (-24.0, 0.0, 24.0):
        P.append_box(floor, opts(2), T(loc=(-30, y, -5)), 400.0, 15.0, 2.5, 0, 0, 0, CENTER)
    box_uv(floor, 100.0)
    append(m, floor)
    sm = save(m, "SM_NL_Boat", ["MI_NL_BoatHull", "MI_NL_Wood", "MI_NL_DeckWood"])
    if sm:
        complex_collision(sm)


def make_outboard():
    # Kıç aynalığına takılan motor: orijin = kelepçe (aynalığın üstü), motor -X yönünde, dümen kolu +X'e uzanır
    m = mesh()
    P.append_box(m, opts(1), T(loc=(-4, 0, -4)), 10.0, 16.0, 14.0, 0, 0, 0, CENTER)
    cow = mesh()
    P.append_sphere_box(cow, opts(0), T(), 1.0, 4, 4, 4, CENTER)
    TR.scale_mesh(cow, unreal.Vector(19, 14, 17), unreal.Vector(0, 0, 0))
    append(m, cow, T(loc=(-24, 0, 26)))
    P.append_box(m, opts(0), T(loc=(-24, 0, 6)), 18.0, 13.0, 22.0, 0, 0, 0, CENTER)
    P.append_cylinder(m, opts(1), T(loc=(-24, 0, -58)), 4.0, 64.0, 10, 0, True, BASE)
    P.append_cylinder(m, opts(1), T(loc=(-14, 0, -62), rot=(-90, 0, 0)), 5.5, 26.0, 10, 0, True, BASE)
    P.append_box(m, opts(1), T(loc=(-22, 0, -72)), 14.0, 2.0, 16.0, 0, 0, 0, CENTER)
    for i in range(3):
        P.append_box(m, opts(1), T(loc=(-42, 0, -62), rot=(0, 0, i * 120.0)), 1.2, 4.0, 18.0, 0, 0, 0, CENTER)
    yaw = 20.0
    L = 95.0
    hx, hy = math.cos(math.radians(yaw)) * L, math.sin(math.radians(yaw)) * L
    seg_box(m, 1, (-14.0, 0.0, 26.0), (-14.0 + hx, hy, 20.0), 4.5, 4.5)
    P.append_cylinder(m, opts(0), T(loc=(-14.0 + hx, hy, 20.0), rot=(-90, yaw, 0)), 3.6, 16.0, 10, 0, True, BASE)
    save(m, "SM_NL_Outboard", ["MI_NL_MotorPaint", "MI_NL_Metal"])
    log("Dumen kolu ucu (yerel): (%.0f, %.0f, 20)" % (-14.0 + hx + 8, hy + 3))


def make_oar():
    m = mesh()
    P.append_cylinder(m, opts(0), T(rot=(90, 0, 0)), 2.6, 210.0, 8, 0, True, CENTER)
    P.append_box(m, opts(0), T(loc=(118, 0, 0)), 52.0, 14.0, 1.6, 0, 0, 0, CENTER)
    P.append_cylinder(m, opts(0), T(loc=(-108, 0, 0), rot=(90, 0, 0)), 3.2, 16.0, 8, 0, True, CENTER)
    box_uv(m, 100.0)
    save(m, "SM_NL_Oar", ["MI_NL_Wood"])


def make_desk():
    # Masa: orijin ortada, 70 (X) x 140 (Y) x 75 (Z). Oturan oyuncu -X tarafında.
    m = mesh()
    w = mesh()
    P.append_box(w, opts(0), T(loc=(0, 0, 35.5)), 74.0, 146.0, 4.0, 0, 0, 0, CENTER)
    P.append_box(w, opts(0), T(loc=(0, -48, -2.5)), 66.0, 44.0, 72.0, 0, 0, 0, CENTER)
    for sx, sy in ((-31, 66), (31, 66)):
        P.append_box(w, opts(0), T(loc=(sx, sy, -2.5)), 5.0, 5.0, 72.0, 0, 0, 0, CENTER)
    P.append_box(w, opts(0), T(loc=(32, 20, 8)), 2.0, 92.0, 42.0, 0, 0, 0, CENTER)
    for i in range(3):
        P.append_box(w, opts(0), T(loc=(-34, -48, 24.0 - i * 23.0)), 2.0, 40.0, 20.0, 0, 0, 0, CENTER)
    box_uv(w, 100.0)
    append(m, w)
    for i in range(3):
        P.append_box(m, opts(1), T(loc=(-36, -48, 26.0 - i * 23.0)), 2.0, 12.0, 2.0, 0, 0, 0, CENTER)
    coll = mesh()
    P.append_box(coll, opts(0), T(), 70.0, 140.0, 75.0, 0, 0, 0, CENTER)
    save(m, "SM_NL_Desk", ["MI_NL_Wood", "MI_NL_Metal"], coll, unreal.GeometryScriptCollisionGenerationMethod.ALIGNED_BOXES)


def make_crt():
    # Tüplü monitör: orijin = ekran yüzünün ortası, ekran -X'e bakar. Yuva 1 = ekran (kamera görüntüsü)
    m = mesh()
    P.append_box(m, opts(0), T(loc=(3.5, 0, 0)), 7.0, 70.0, 57.0, 0, 0, 0, CENTER)
    P.append_box(m, opts(0), T(loc=(26, 0, 0)), 40.0, 58.0, 48.0, 0, 0, 0, CENTER)
    P.append_box(m, opts(0), T(loc=(52, 0, 1)), 18.0, 38.0, 32.0, 0, 0, 0, CENTER)
    P.append_box(m, opts(0), T(loc=(24, 0, -31)), 34.0, 40.0, 5.0, 0, 0, 0, CENTER)
    P.append_box(m, opts(0), T(loc=(24, 0, -26)), 14.0, 14.0, 6.0, 0, 0, 0, CENTER)
    for i in range(2):
        P.append_box(m, opts(0), T(loc=(-0.5, 22 + i * 6, -24)), 1.5, 3.0, 3.0, 0, 0, 0, CENTER)
    scr = mesh()
    P.append_box(scr, opts(1), T(loc=(-0.6, 0, 1)), 1.0, 60.0, 46.0, 0, 0, 0, CENTER)
    append(m, scr)
    save(m, "SM_NL_CRT", ["MI_NL_Plastic", "M_NLCameraMonitor"])


def make_chair():
    # Ahşap sandalye: orijin = oturağın ortası (oturak üstü +4), bacaklar -44'e iner, sırt -X'te
    m = mesh()
    P.append_box(m, opts(0), T(loc=(0, 0, 2)), 44.0, 44.0, 4.0, 0, 0, 0, CENTER)
    for sx in (-19.0, 19.0):
        for sy in (-19.0, 19.0):
            P.append_box(m, opts(0), T(loc=(sx, sy, -22)), 4.0, 4.0, 44.0, 0, 0, 0, CENTER)
    for sy in (-19.0, 19.0):
        P.append_box(m, opts(0), T(loc=(-20, sy, 26)), 4.0, 4.0, 48.0, 0, 0, 0, CENTER)
    for z in (18.0, 32.0, 46.0):
        P.append_box(m, opts(0), T(loc=(-20, 0, z)), 3.0, 40.0, 7.0, 0, 0, 0, CENTER)
    for sy in (-19.0, 19.0):
        P.append_box(m, opts(0), T(loc=(0, sy, -30)), 40.0, 2.5, 3.0, 0, 0, 0, CENTER)
    box_uv(m, 100.0)
    save(m, "SM_NL_Chair", ["MI_NL_Wood"])


def make_jerrycan():
    # Benzin bidonu (20 L): taban z=0, 16 (X) x 34 (Y) x 46 (Z); üstte üçlü sap, köşede ağız, yanlarda kabartma X
    m = mesh()
    body = mesh()
    P.append_box(body, opts(0), T(loc=(0, 0, 21)), 16.0, 34.0, 42.0, 0, 0, 0, CENTER)
    for sx in (-8.2, 8.2):
        for ang in (40.0, -40.0):
            P.append_box(body, opts(0), T(loc=(sx, 0, 21), rot=(0, 0, ang)), 1.0, 3.0, 46.0, 0, 0, 0, CENTER)
    P.append_box(body, opts(0), T(loc=(0, 0, 42.5)), 14.0, 32.0, 1.5, 0, 0, 0, CENTER)
    for y in (-7.0, 0.0, 7.0):
        P.append_box(body, opts(0), T(loc=(0, y, 46)), 3.0, 2.0, 7.0, 0, 0, 0, CENTER)
    P.append_box(body, opts(0), T(loc=(0, 0, 49)), 3.5, 18.0, 2.5, 0, 0, 0, CENTER)
    append(m, body)
    P.append_cylinder(m, opts(1), T(loc=(0, 13, 42), rot=(25, 90, 0)), 2.6, 9.0, 10, 0, True, BASE)
    P.append_box(m, opts(1), T(loc=(0, 13, 44)), 6.0, 6.0, 3.0, 0, 0, 0, CENTER)
    coll = mesh()
    P.append_box(coll, opts(0), T(loc=(0, 0, 25)), 16.0, 34.0, 50.0, 0, 0, 0, CENTER)
    save(m, "SM_NL_JerryCan", ["MI_NL_JerryRed", "MI_NL_Metal"], coll, unreal.GeometryScriptCollisionGenerationMethod.ALIGNED_BOXES)


def make_flashlight():
    # Eski tip metal el feneri: orijin = arka kapak, fener +X'e bakar (toplam ~22 cm). Yuva 1 = cam
    m = mesh()
    X = (-90, 0, 0)  # silindir ekseni +X
    P.append_cylinder(m, opts(0), T(loc=(0, 0, 0), rot=X), 2.15, 1.6, 16, 0, True, BASE)
    P.append_cylinder(m, opts(0), T(loc=(1.6, 0, 0), rot=X), 1.85, 13.6, 16, 0, True, BASE)
    for x in (3.0, 4.6, 6.2, 7.8, 9.4):
        P.append_cylinder(m, opts(0), T(loc=(x, 0, 0), rot=X), 2.0, 0.55, 16, 0, True, BASE)
    P.append_cylinder(m, opts(0), T(loc=(14.6, 0, 0), rot=X), 2.05, 0.6, 16, 0, True, BASE)
    P.append_box(m, opts(0), T(loc=(11.8, 0, 1.95)), 2.4, 1.2, 0.9, 0, 0, 0, CENTER)
    P.append_box(m, opts(0), T(loc=(11.8, 0, 2.45)), 1.0, 0.8, 0.6, 0, 0, 0, CENTER)
    P.append_cone(m, opts(0), T(loc=(15.2, 0, 0), rot=X), 1.85, 2.85, 4.6, 16, 1, False, BASE)
    P.append_cylinder(m, opts(0), T(loc=(19.8, 0, 0), rot=X), 3.1, 1.9, 18, 0, True, BASE)
    P.append_cylinder(m, opts(1), T(loc=(21.65, 0, 0), rot=X), 2.65, 0.3, 18, 0, True, BASE)
    P.append_torus(m, opts(0), T(loc=(-0.6, 0, -1.6), rot=(0, 0, 90)), unreal.GeometryScriptRevolveOptions(), 1.1, 0.25, 10, 6, CENTER)
    save(m, "SM_NL_Flashlight", ["MI_NL_Metal", "MI_NL_LH_Glass"])


for fn in (make_boat, make_outboard, make_oar, make_desk, make_crt, make_chair, make_flashlight):  # bidon artık Scripts/blender_props.py
    if _only and fn.__name__ not in _only:
        continue
    try:
        fn()
    except Exception as e:
        import traceback
        log("HATA %s: %s %s" % (fn.__name__, e, traceback.format_exc().splitlines()[-3:]))

with open(os.path.join(unreal.Paths.project_saved_dir(), "NLKit.log"), "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
