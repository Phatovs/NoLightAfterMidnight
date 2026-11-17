# No Light After Midnight - Deniz feneri ve bitişik bekçi evi (referans: yıpranmış beyaz kule + sıvalı ev, metal çatı)
# Çalıştırma: blender.exe -b --factory-startup --python Scripts/blender_lighthouse.py -- <çıktı klasörü>   (önizleme: NL_PREVIEW=1)
# Çıktı (ANLLighthouse::Layout bunlara göre yerleştirir; ölçüler NLLighthouse.cpp NLLH ile aynı olmalı):
#   SM_NL_LH_Tower       : taban r260 -> tepe r170, boy 1000 (Layout Z'de TowerHeight/1000 ölçekler). İçi tepeye kadar boş (duvar 22),
#                          zemin Z 40, dışarı açılan kapı DOOR_A (300) derecede (120 x 215), evin girişiyle aynı yüzde ama ayrı. Ortada sütun ve etrafında döner merdiven: Z 40 -> 1014
#                          (balkon/fener odası zemini). Son yarım tur, üstteki açık merdiven boşluğunun (48..275 derece) altında.
#   SM_NL_LH_Gallery     : balkon tabanı (r270, Z 0..14) = fener odasının zemini; merdiven boşluğu (48..275 derece, r 50..150),
#                          boşluğun kenarında korkuluk, dışta konsollar ve siyah korkuluk.
#   SM_NL_LH_LanternRoom : içi boş alt duvar (Z 0..70, r 150..157) ve camlar (70..250), +X'te balkona çıkış kapısı, ortada lens kaidesi (r30, 0..86),
#                          kubbe, havalandırma topu, paratoner.
#   SM_NL_LH_House       : kuleye bitişik bekçi evi (orijin = kule ekseni, Z 0 = ev zemini = kule zemin katı). X 250..810, Y ±210.
#                          Kuleye bakan duvar kapalı (giriş ayrı), -Y'de dış kapı (X 660), pencereler (cam: MI_NL_WindowGlow).
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy
import bmesh
from mathutils import Vector
from blender_nl_common import *  # noqa

reset()
OUT = out_dir("lighthouse_out")
random.seed(17)
WORN, BLACK, GLASS, STONE = "MI_NL_LH_Worn", "MI_NL_LH_Black", "MI_NL_LH_Glass", "MI_NL_Rock"
ROOF, WOOD, WINGLOW, CONC = "MI_NL_LH_RoofMetal", "MI_NL_DeckWood", "MI_NL_WindowGlow", "MI_NL_World_Concrete"


def boolean(target, cutter, keep=False):
    mod = target.modifiers.new("bool", 'BOOLEAN')
    mod.operation = 'DIFFERENCE'
    mod.solver = 'EXACT'
    mod.object = cutter
    bpy.context.view_layer.objects.active = target
    bpy.ops.object.modifier_apply(modifier=mod.name)
    if not keep:
        bpy.data.objects.remove(cutter)


def smooth_by_angle(ob, deg=40):
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    try:
        bpy.ops.object.shade_smooth_by_angle(angle=math.radians(deg))
    except Exception:
        bpy.ops.object.shade_smooth()
    ob.select_set(False)


def sector_prism(name, a0, a1, r0, r1, z0, z1, m, seg=24):
    """a0..a1 derece arası halka dilimi (Unreal ekseninde)"""
    bm = bmesh.new()
    pts = []
    for i in range(seg + 1):
        a = math.radians(a0 + (a1 - a0) * i / seg)
        pts.append((r1 * math.cos(a), r1 * math.sin(a)))
    for i in range(seg, -1, -1):
        a = math.radians(a0 + (a1 - a0) * i / seg)
        pts.append((r0 * math.cos(a), r0 * math.sin(a)))
    lo = [bm.verts.new(to_bl(x, y, z0)) for (x, y) in pts]
    hi = [bm.verts.new(to_bl(x, y, z1)) for (x, y) in pts]
    bm.faces.new(lo)
    bm.faces.new(list(reversed(hi)))
    n = len(pts)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new([lo[i], lo[j], hi[j], hi[i]])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return finish(new_obj(name, bm), m)


def r_out(z):
    return 260.0 - 0.09 * z


WALL = 22.0
STAIR0, STAIR_TOP, NSTEP, STEP_A, START_A = 40.0, 1014.0, 46, 20.0, 75.0
RISE = (STAIR_TOP - STAIR0) / NSTEP
WELL_A0, WELL_A1, WELL_R0, WELL_R1 = 48.0, 275.0, 36.0, 146.0

# Kulenin dış kapısı: kule içindeki merdivenin ilk turu bu açıda kapının üstünden geçer (kafa çarpmaz)
DOOR_A = 300.0


def at_door(ob):
    """+X'e göre kurulmuş parçayı kule ekseni etrafında DOOR_A açısına döndür"""
    from mathutils import Matrix
    bpy.context.view_layer.update()   # konum/dönüş matrix_world'e işlensin
    R = Matrix.Rotation(math.radians(-DOOR_A), 4, 'Z')   # Unreal açısı Blender'da ters yönde
    ob.matrix_world = R @ ob.matrix_world
    return ob


# ------------------------------------------------------------------ kule
tower = cyl("tower", (0, 0, 500), 260, 1000, 'Z', WORN, seg=56, r2=170)
shaft = cyl("shaft", (0, 0, 520.5), r_out(40) - WALL, 961, 'Z', WORN, seg=56, r2=r_out(1001) - WALL)
boolean(tower, shaft)
boolean(tower, at_door(box("door", (180, -60, 40), (300, 60, 255), WORN)))
smooth_by_angle(tower)
P = [tower]
pl = cyl("plinth", (0, 0, 14), 272, 28, 'Z', WORN, seg=56, r2=268)
boolean(pl, cyl("plinthcut", (0, 0, 14), 250, 40, 'Z', WORN, seg=56))
boolean(pl, at_door(box("plinthdoor", (180, -60, -5), (300, 60, 40), WORN)))
P.append(pl)
P.append(cyl("collar1", (0, 0, 958), 182, 16, 'Z', WORN, seg=56, r2=184))
P.append(cyl("collar2", (0, 0, 982), 186, 36, 'Z', WORN, seg=56, r2=204))
P.append(cyl("collarBand", (0, 0, 940), 177, 10, 'Z', BLACK, seg=56, r2=176))
for c in P[-3:]:
    boolean(c, cyl("collarcut", (0, 0, 960), r_out(1000) - WALL, 120, 'Z', WORN, seg=56))
for y in (-66.0, 60.0):
    P.append(at_door(box("doorjamb", (226, y, 40), (252, y + 6, 262), WORN, 1.0)))
P.append(at_door(box("doorhead", (224, -66, 255), (254, 66, 268), WORN, 1.0)))
# Kapı önü taş basamaklar (dışarı doğru iner; zemin yüksekse toprağa gömülür)
for k in range(3):
    P.append(at_door(box("tstep%d" % k, (262 + k * 30, -62, 40 - 15 * (k + 1) - 20), (300 + k * 30, 62, 40 - 15 * k), STONE, 1.5)))
P.append(cyl("floor", (0, 0, 38), r_out(40) - WALL + 2, 4, 'Z', STONE, seg=56))
# Merkez sütun ve döner merdiven
P.append(cyl("column", (0, 0, (STAIR0 + STAIR_TOP) / 2), 18, STAIR_TOP - STAIR0, 'Z', WORN, seg=20))
for i in range(NSTEP):
    zt = STAIR0 + (i + 1) * RISE
    a0 = START_A + i * STEP_A
    # Tepeye yakın basamaklar fener odasının içine sığsın (oda iç yarıçapı 148)
    ro = min(r_out(zt) - WALL - 3.0, 140.0 if zt > 850 else 999.0)
    st = sector_prism("step%d" % i, a0, a0 + STEP_A + 1.5, 14.0, ro, zt - 7.0, zt, WOOD, seg=4)
    P.append(st)
    # Basamak altı destek (duvara gömülü konsol)
    a = math.radians(a0 + STEP_A * 0.5)
    rb = ro - 10
    P.append(beam("stepbrace%d" % i, (math.cos(a) * 20, math.sin(a) * 20, zt - 7), (math.cos(a) * rb, math.sin(a) * rb, zt - 7), 3, 5, BLACK))
# Merdiven dış kenarında tırabzan (duvar boyunca)
rail = []
for k in range(0, NSTEP * 4 + 1):
    t = k / 4.0
    a = math.radians(START_A + t * STEP_A)
    z = STAIR0 + t * RISE + 92
    rr = min(r_out(z - 92) - WALL - 8.0, 134.0 if z - 92 > 850 else 999.0)
    rail.append((math.cos(a) * rr, math.sin(a) * rr, z))
P.append(wire("handrail", rail, 2.2, BLACK, res=2))
for o in P:
    if o is not tower:
        smooth_by_angle(o)
twr = join("SM_NL_LH_Tower", P, uv_size=1.4)
export(twr, OUT)

# ------------------------------------------------------------------ balkon (fener odasının zemini)
P = []
deck = cyl("deck", (0, 0, 7), 270, 14, 'Z', BLACK, seg=56)
boolean(deck, sector_prism("well", WELL_A0, WELL_A1, WELL_R0, WELL_R1, -2, 16, BLACK))
P.append(deck)
P.append(torus("deckedge", (0, 0, 13), 269, 2.5, BLACK, axis='Z', majseg=64, minseg=6))
for i in range(16):
    a = math.radians(i * 360.0 / 16)
    ca, sa = math.cos(a), math.sin(a)
    P.append(beam("bracket%d" % i, (ca * 192, sa * 192, -48), (ca * 266, sa * 266, -2), 6, 9, BLACK, 0.5))
    P.append(beam("bracketTop%d" % i, (ca * 186, sa * 186, -4), (ca * 268, sa * 268, -4), 6, 5, BLACK, 0.4))
for i in range(36):
    a = math.radians(i * 360.0 / 36)
    P.append(cyl("post%d" % i, (math.cos(a) * 262, math.sin(a) * 262, 62), 1.7, 96, 'Z', BLACK, seg=8))
for z, r in ((108.0, 2.8), (76.0, 1.5), (44.0, 1.5)):
    P.append(torus("rail", (0, 0, z), 262, r, BLACK, axis='Z', majseg=72, minseg=6))
# Merdiven boşluğunun kapalı kenarında (48 derece) korkuluk
a = math.radians(WELL_A0)
for rr in (56.0, 92.0, 126.0):
    P.append(cyl("wellpost", (math.cos(a) * rr, math.sin(a) * rr, 60), 1.6, 92, 'Z', BLACK, seg=8))
for z in (104.0, 60.0):
    P.append(tube("wellrail", (math.cos(a) * 54, math.sin(a) * 54, z), (math.cos(a) * 128, math.sin(a) * 128, z), 1.8, BLACK, seg=8))
gal = join("SM_NL_LH_Gallery", P, uv_size=0.6)
export(gal, OUT)

# ------------------------------------------------------------------ fener odası (içine girilir)
P = []
wall = cyl("wall", (0, 0, 35), 157, 70, 'Z', BLACK, seg=10, smooth=False)
boolean(wall, cyl("wallin", (0, 0, 35), 150, 80, 'Z', BLACK, seg=10, smooth=False))
P.append(wall)
P.append(cyl("wallcap", (0, 0, 71), 161, 4, 'Z', BLACK, seg=10, smooth=False))
boolean(P[-1], cyl("capin", (0, 0, 71), 148, 10, 'Z', BLACK, seg=10, smooth=False))
gl = cyl("glass", (0, 0, 162), 149, 176, 'Z', GLASS, seg=10, smooth=False)
boolean(gl, cyl("glassin", (0, 0, 162), 147, 190, 'Z', GLASS, seg=10, smooth=False))
gl.rotation_euler = (0, 0, math.radians(18))
P.append(gl)
for i in range(10):
    a = math.radians(i * 36.0)
    P.append(box("mull%d" % i, (math.cos(a) * 150 - 3, math.sin(a) * 150 - 3, 70), (math.cos(a) * 150 + 3, math.sin(a) * 150 + 3, 250), BLACK, 0.5))
for z in (128.0, 190.0):
    P.append(torus("astragal", (0, 0, z), 150, 1.8, BLACK, axis='Z', majseg=40, minseg=6))
P.append(cyl("topring", (0, 0, 252), 159, 8, 'Z', BLACK, seg=40))
boolean(P[-1], cyl("toprin", (0, 0, 252), 145, 12, 'Z', BLACK, seg=40))
dome = cyl("dome", (0, 0, 300), 172, 92, 'Z', BLACK, seg=40, r2=22)
P.append(dome)
P.append(torus("domeEdge", (0, 0, 255), 170, 4, BLACK, axis='Z', majseg=48, minseg=6))
P.append(cyl("ventNeck", (0, 0, 352), 12, 14, 'Z', BLACK, seg=16))
P.append(sphere("ventBall", (0, 0, 372), 22, BLACK, seg=20, rings=10))
P.append(cyl("ventCap", (0, 0, 392), 8, 8, 'Z', BLACK, seg=12))
P.append(cyl("rod", (0, 0, 425), 1.6, 60, 'Z', BLACK, seg=8, r2=0.5))
for i in range(10):
    a = math.radians(i * 36.0 + 18)
    P.append(beam("rib%d" % i, (math.cos(a) * 170, math.sin(a) * 170, 256), (math.cos(a) * 24, math.sin(a) * 24, 344), 3, 3, BLACK, 0.3))
# Lens kaidesi (merkez sütunun devamı)
P.append(cyl("pedestal", (0, 0, 43), 30, 86, 'Z', BLACK, seg=24, r2=26))
P.append(torus("pedRing", (0, 0, 84), 27, 3, BLACK, axis='Z', majseg=24, minseg=6))
# Balkona çıkış kapısı (+X): boşluk, kasa ve yarı açık kanat
for ob in [o for o in P if o.name.startswith(("wall", "glass", "mull0", "astragal", "wallcap"))]:
    boolean(ob, box("doorcut", (132, -52, -2), (180, 52, 205), BLACK), keep=False)
for y in (-56.0, 52.0):
    P.append(box("djamb", (145, y, 0), (161, y + 4, 208), BLACK, 0.4))
P.append(box("dhead", (145, -56, 204), (161, 56, 212), BLACK, 0.4))
# Kapı kanadı yok: açık geçit (balkonda dolaşmayı engellemesin)
lr = join("SM_NL_LH_LanternRoom", P, uv_size=0.6)
export(lr, OUT)

# ------------------------------------------------------------------ bekçi evi
HX0, HX1, HY, WT, WH = 250.0, 810.0, 210.0, 20.0, 280.0
P = []


def wall_y(y_face, s, x0, x1, holes):
    """Y'de duran uzun duvar: holes = [(x0, x1, z0, z1)]"""
    y0, y1 = sorted((y_face, y_face - s * WT))
    w = box("wallY", (x0, y0, -2), (x1, y1, WH), WORN, 0.8)
    for (a, b, z0, z1) in holes:
        boolean(w, box("hole", (a, y0 - 5, z0), (b, y1 + 5, z1), WORN))
    P.append(w)


def wall_x(x_face, s, holes, gable=True):
    x0, x1 = sorted((x_face, x_face - s * WT))
    w = box("wallX", (x0, -HY, -2), (x1, HY, WH), WORN, 0.8)
    for (a, b, z0, z1) in holes:
        boolean(w, box("hole", (x0 - 5, a, z0), (x1 + 5, b, z1), WORN))
    P.append(w)
    if gable:
        P.append(poly_prism_x("gable", x0, x1, [(-HY, WH), (HY, WH), (0, WH + RISE_R)], WORN))


def poly_prism_x(name, x0, x1, pts_yz, m):
    bm = bmesh.new()
    a = [bm.verts.new(to_bl(x0, y, z)) for (y, z) in pts_yz]
    b = [bm.verts.new(to_bl(x1, y, z)) for (y, z) in pts_yz]
    bm.faces.new(a)
    bm.faces.new(list(reversed(b)))
    n = len(pts_yz)
    for i in range(n):
        j = (i + 1) % n
        bm.faces.new([a[i], a[j], b[j], b[i]])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return finish(new_obj(name, bm), m)


def window(cx_or_cy, face, z0, z1, w, along='X'):
    """Pencere: beyaz söve, koyu kayıtlar, ışıklı cam (C++ yakar/söndürür)"""
    hw = w / 2.0
    if along == 'X':   # Y yüzünde, X boyunca
        y = face
        s = 1 if y > 0 else -1
        yo = (min(y, y + s * 4), max(y, y + s * 4))
        P.append(box("wsill", (cx_or_cy - hw - 6, min(y, y + s * 8), z0 - 6), (cx_or_cy + hw + 6, max(y, y + s * 8), z0), WORN, 1.0))
        P.append(box("wtop", (cx_or_cy - hw - 5, yo[0], z1), (cx_or_cy + hw + 5, yo[1], z1 + 7), WORN, 0.8))
        for xx in (cx_or_cy - hw - 5, cx_or_cy + hw):
            P.append(box("wside", (xx, yo[0], z0), (xx + 5, yo[1], z1), WORN, 0.6))
        P.append(box("wglass", (cx_or_cy - hw, y - s * 12 - 0.5, z0), (cx_or_cy + hw, y - s * 12 + 0.5, z1), WINGLOW))
        P.append(box("wbarV", (cx_or_cy - 1.5, y - s * 12 - 2, z0), (cx_or_cy + 1.5, y - s * 12 + 2, z1), BLACK))
        P.append(box("wbarH", (cx_or_cy - hw, y - s * 12 - 2, (z0 + z1) / 2 - 1.5), (cx_or_cy + hw, y - s * 12 + 2, (z0 + z1) / 2 + 1.5), BLACK))
    else:              # X yüzünde, Y boyunca
        x = face
        s = 1 if x > 0 else -1
        xo = (min(x, x + s * 4), max(x, x + s * 4))
        P.append(box("wsill", (min(x, x + s * 8), cx_or_cy - hw - 6, z0 - 6), (max(x, x + s * 8), cx_or_cy + hw + 6, z0), WORN, 1.0))
        P.append(box("wtop", (xo[0], cx_or_cy - hw - 5, z1), (xo[1], cx_or_cy + hw + 5, z1 + 7), WORN, 0.8))
        for yy in (cx_or_cy - hw - 5, cx_or_cy + hw):
            P.append(box("wside", (xo[0], yy, z0), (xo[1], yy + 5, z1), WORN, 0.6))
        P.append(box("wglass", (x - s * 12 - 0.5, cx_or_cy - hw, z0), (x - s * 12 + 0.5, cx_or_cy + hw, z1), WINGLOW))
        P.append(box("wbarV", (x - s * 12 - 2, cx_or_cy - 1.5, z0), (x - s * 12 + 2, cx_or_cy + 1.5, z1), BLACK))
        P.append(box("wbarH", (x - s * 12 - 2, cx_or_cy - hw, (z0 + z1) / 2 - 1.5), (x - s * 12 + 2, cx_or_cy + hw, (z0 + z1) / 2 + 1.5), BLACK))


RISE_R = 150.0
# Duvarlar: kuleye bakan (X 250), uzak uç (X 810), uzun yanlar (Y ±210)
WZ0, WZ1 = 105.0, 190.0
# Kuleye bakan duvar kapalı: evin ve fenerin girişleri ayrı
wall_x(HX0, -1, [])
wall_x(HX1, 1, [(-60 - 38, -60 + 38, WZ0, WZ1)])
wall_y(-HY, -1, HX0, HX1, [(604, 716, 0, 222), (362, 438, WZ0, WZ1)])
wall_y(HY, 1, HX0, HX1, [(652, 728, WZ0, WZ1), (360, 436, WZ0, WZ1)])
window(400, -HY, WZ0, WZ1, 76, 'X')
window(690, HY, WZ0, WZ1, 76, 'X')
window(398, HY, WZ0, WZ1, 76, 'X')
window(-60, HX1, WZ0, WZ1, 76, 'Y')
# Dış kapı (-Y, X 660): söve, yarı açık kanat (içe), taş eşik
for xx in (598, 716):
    P.append(box("dside", (xx, -HY - 5, 0), (xx + 6, -HY + 2, 228), WORN, 0.8))
P.append(box("dtop", (598, -HY - 6, 222), (722, -HY + 2, 232), WORN, 0.8))
leaf = box("dleaf", (0, -2, 0), (108, 2, 214), WOOD, 0.6)
leaf.location = to_bl(606 + 54 * math.cos(math.radians(70)), -HY + WT + 54 * math.sin(math.radians(70)), 108)
leaf.rotation_euler = (0, 0, math.radians(-70))
P.append(leaf)
for k in range(3):
    P.append(box("step%d" % k, (592 - k * 6, -HY - 34 - k * 30, -15 * (k + 1) - 20), (728 + k * 6, -HY - 2 - k * 30, -15 * k), STONE, 2.0))
# Temel (zeminin altına iner, eğimli arazide boşluk kalmasın)
P.append(box("found", (HX0 - 6, -HY - 6, -130), (HX1 + 6, HY + 6, 2), CONC, 1.5))
# İç: döşeme, tavan
P.append(box("ifloor", (HX0 + 2, -HY + WT, 0), (HX1 - WT, HY - WT, 3), WOOD))
P.append(box("ceiling", (HX0, -HY, WH - 8), (HX1, HY, WH), WORN))
# Çatı: metal levhalar, dikiş çıtaları, saçak
OV, OVE = 26.0, 22.0
span = HY + OV
ang = math.atan2(RISE_R, HY)
L = span / math.cos(ang) + 4
for s in (1, -1):
    cy = s * (span * 0.5 - 4)
    cz = WH + RISE_R * 0.5 - OV * math.tan(ang) * 0.5 + 6
    rf = box("roof", (HX0 - OVE, -L / 2, -2.5), (HX1 + OVE, L / 2, 2.5), ROOF, 0.5)
    rf.location = to_bl((HX0 + HX1) / 2, cy, cz)
    rf.rotation_euler = (math.radians(s * math.degrees(ang)), 0, 0)
    P.append(rf)
    for k in range(int((HX1 - HX0 + 2 * OVE) / 45) + 1):
        x = HX0 - OVE + 10 + k * 45
        sm = box("seam", (x - 1.5, -L / 2, 2.5), (x + 1.5, L / 2, 5.5), ROOF, 0.4)
        sm.location = to_bl(x, cy, cz) + Vector((0, 0, 4 * C * math.cos(ang)))
        sm.rotation_euler = (math.radians(s * math.degrees(ang)), 0, 0)
        P.append(sm)
P.append(cyl("ridge", (((HX0 + HX1) / 2), 0, WH + RISE_R + 7), 5, HX1 - HX0 + 2 * OVE, 'X', ROOF, seg=10))
for s in (1, -1):
    P.append(box("fascia", (HX0 - OVE, s * (HY + OV) - 3, WH - 22), (HX1 + OVE, s * (HY + OV) + 3, WH - 6), WORN, 0.5))
# Baca (sırtın üstünde, uzak uçta)
P.append(box("chimney", (700, -28, WH + 60), (756, 28, WH + RISE_R + 85), WORN, 1.5))
P.append(box("chimneycap", (694, -34, WH + RISE_R + 85), (762, 34, WH + RISE_R + 93), CONC, 1.0))
P.append(cyl("chimneypot", (728, 0, WH + RISE_R + 102), 9, 18, 'Z', CONC, seg=12))
# Duvarın dibinde ıslak toprak / yosun izi gibi koyu bant
for (a, b, c, d) in ((HX0, -HY - 1.5, HX1, -HY + 0.1), (HX0, HY - 0.1, HX1, HY + 1.5)):
    P.append(box("grime", (a, b, 0), (c, d, 14), CONC))
house = join("SM_NL_LH_House", P, uv_size=1.0)
export(house, OUT)

if os.environ.get("NL_PREVIEW") == "1":
    gal.location = to_bl(0, 0, 1000)
    lr.location = to_bl(0, 0, 1014)
    house.location = to_bl(0, 0, 40)
    preview(OUT, "lighthouse_preview.png", (1350, -1900, 650), (300, 0, 520),
            {WORN: (0.82, 0.8, 0.75, 1), BLACK: (0.06, 0.06, 0.07, 1), GLASS: (0.35, 0.45, 0.5, 1), STONE: (0.4, 0.4, 0.38, 1),
             ROOF: (0.45, 0.47, 0.5, 1), WOOD: (0.35, 0.25, 0.15, 1), WINGLOW: (1.0, 0.75, 0.4, 1), CONC: (0.35, 0.35, 0.34, 1)},
            lens=32, res=(1300, 1300))
    # İç: merdiven (kesit görünümü için kulenin yarısını gizlemek yerine yukarıdan bakış)
    twr.hide_render = False
