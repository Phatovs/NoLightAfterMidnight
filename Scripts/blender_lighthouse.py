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
ROOF, WOOD, WINGLOW, CONC = "MI_NL_World_Metal", "MI_NL_DeckWood", "MI_NL_WindowGlow", "MI_NL_World_Concrete"
# Kule dışı kırmızı-beyaz bantlı boya (yerel Z ile bant), iç ve süsler düz beyaz sıva
PAINT, PLASTER = "MI_NL_LighthousePaint", "MI_NL_World_Plaster"


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
tower = cyl("tower", (0, 0, 500), 260, 1000, 'Z', PAINT, seg=56, r2=170)
shaft = cyl("shaft", (0, 0, 520.5), r_out(40) - WALL, 961, 'Z', PLASTER, seg=56, r2=r_out(1001) - WALL)
boolean(tower, shaft)
boolean(tower, at_door(box("door", (180, -60, 40), (300, 60, 255), PLASTER)))
smooth_by_angle(tower)
P = [tower]
# Taş temel: zemin katının altından toprağa iner (eğimli arazide boşluk kalmasın)
fd = cyl("found", (0, 0, -120), 274, 320, 'Z', STONE, seg=56, r2=272)
boolean(fd, cyl("foundcut", (0, 0, 60), 200, 50, 'Z', STONE, seg=56))
P.append(fd)
pl = cyl("plinth", (0, 0, 14), 276, 28, 'Z', PLASTER, seg=56, r2=272)
boolean(pl, cyl("plinthcut", (0, 0, 14), 250, 40, 'Z', PLASTER, seg=56))
boolean(pl, at_door(box("plinthdoor", (180, -60, -5), (300, 60, 40), PLASTER)))
P.append(pl)
P.append(cyl("collar1", (0, 0, 958), 182, 16, 'Z', PLASTER, seg=56, r2=184))
P.append(cyl("collar2", (0, 0, 982), 186, 36, 'Z', PLASTER, seg=56, r2=204))
P.append(cyl("collarBand", (0, 0, 940), 177, 10, 'Z', BLACK, seg=56, r2=176))
for c in P[-3:]:
    boolean(c, cyl("collarcut", (0, 0, 960), r_out(1000) - WALL, 120, 'Z', PLASTER, seg=56))
for y in (-66.0, 60.0):
    P.append(at_door(box("doorjamb", (226, y, 40), (256, y + 6, 262), PLASTER, 1.0)))
P.append(at_door(box("doorhead", (224, -66, 255), (258, 66, 270), PLASTER, 1.0)))
# Kapı önü taş basamaklar (dışarı doğru iner; zemin yüksekse toprağa gömülür)
for k in range(3):
    P.append(at_door(box("tstep%d" % k, (262 + k * 30, -62, 40 - 15 * (k + 1) - 20), (300 + k * 30, 62, 40 - 15 * k), STONE, 1.5)))
P.append(cyl("floor", (0, 0, 38), r_out(40) - WALL + 2, 4, 'Z', STONE, seg=56))
# Merkez sütun ve döner merdiven
P.append(cyl("column", (0, 0, (STAIR0 + STAIR_TOP) / 2), 18, STAIR_TOP - STAIR0, 'Z', PLASTER, seg=20))
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

# ------------------------------------------------------------------ bekçi evi (referans: beyaz sıvalı, gri metal çatılı,
# önde üçgen alınlıklı giriş çıkıntısı, baca kule tarafında, gri taş temel)
# Kapı kanatları ayrı aktör (ANLSwingDoor, açılıp kapanır); burada sadece boşluk ve söve var.
HX0, HX1, HY, WT, WH = 250.0, 810.0, 200.0, 20.0, 270.0
RISE_R = 130.0                       # ana çatı yüksekliği (sırt Z 400)
BX0, BX1, BY = 560.0, 780.0, 240.0   # giriş çıkıntısı (alınlık): X aralığı, ön yüzü Y -240
BRISE = 105.0
DOOR_X0, DOOR_X1, DOOR_H = 615.0, 725.0, 215.0
P = []


def wall(name, lo, hi, holes, m=PLASTER):
    w = box(name, lo, hi, m, 0.6)
    for (a, b) in holes:
        boolean(w, box("hole", a, b, m))
    P.append(w)
    return w


def tri_prism_x(name, x0, x1, y0, y1, z0, rise, m):
    """X yüzünde duran üçgen alınlık (taban y0..y1, z0'dan rise kadar yükselir)"""
    bm = bmesh.new()
    pts = [(y0, z0), (y1, z0), ((y0 + y1) / 2, z0 + rise)]
    a = [bm.verts.new(to_bl(x0, y, z)) for (y, z) in pts]
    b = [bm.verts.new(to_bl(x1, y, z)) for (y, z) in pts]
    bm.faces.new(a)
    bm.faces.new(list(reversed(b)))
    for i in range(3):
        j = (i + 1) % 3
        bm.faces.new([a[i], a[j], b[j], b[i]])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return finish(new_obj(name, bm), m)


def tri_prism_y(name, y0, y1, x0, x1, z0, rise, m):
    """Y yüzünde duran üçgen alınlık"""
    bm = bmesh.new()
    pts = [(x0, z0), (x1, z0), ((x0 + x1) / 2, z0 + rise)]
    a = [bm.verts.new(to_bl(x, y0, z)) for (x, z) in pts]
    b = [bm.verts.new(to_bl(x, y1, z)) for (x, z) in pts]
    bm.faces.new(a)
    bm.faces.new(list(reversed(b)))
    for i in range(3):
        j = (i + 1) % 3
        bm.faces.new([a[i], a[j], b[j], b[i]])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return finish(new_obj(name, bm), m)


def roof_plane(name, p0, p1, p2, p3, thick, m):
    """4 köşeli eğimli çatı levhası (alt yüz noktaları), kalınlık normal yönünde"""
    bm = bmesh.new()
    pts = [Vector(p) for p in (p0, p1, p2, p3)]
    n = (pts[1] - pts[0]).cross(pts[3] - pts[0]).normalized()
    if n.z < 0:
        n = -n
    lo = [bm.verts.new(to_bl(*p)) for p in pts]
    hi = [bm.verts.new(to_bl(*(p + n * thick))) for p in pts]
    bm.faces.new(lo)
    bm.faces.new(list(reversed(hi)))
    for i in range(4):
        j = (i + 1) % 4
        bm.faces.new([lo[i], lo[j], hi[j], hi[i]])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    return finish(new_obj(name, bm), m)


def window_y(cx, yface, s, z0, z1, w):
    """Y yüzündeki pencere (s: dışa doğru +1/-1): sıva söve, gri denizlik, koyu kayıtlar, ışıklı cam"""
    hw = w / 2.0
    yo = sorted((yface, yface + s * 5))
    P.append(box("wsill", (cx - hw - 8, min(yface, yface + s * 9), z0 - 7), (cx + hw + 8, max(yface, yface + s * 9), z0), CONC, 1.0))
    P.append(box("wtrimT", (cx - hw - 7, yo[0], z1), (cx + hw + 7, yo[1], z1 + 9), PLASTER, 0.8))
    for xx in (cx - hw - 7, cx + hw):
        P.append(box("wtrimS", (xx, yo[0], z0), (xx + 7, yo[1], z1), PLASTER, 0.6))
    gy = yface - s * 11
    P.append(box("wglass", (cx - hw, gy - 0.5, z0), (cx + hw, gy + 0.5, z1), WINGLOW))
    P.append(box("wframe", (cx - hw, gy - 2.5, z0), (cx + hw, gy + 2.5, z0 + 4), BLACK))
    P.append(box("wframe", (cx - hw, gy - 2.5, z1 - 4), (cx + hw, gy + 2.5, z1), BLACK))
    P.append(box("wbarV", (cx - 1.8, gy - 2.5, z0), (cx + 1.8, gy + 2.5, z1), BLACK))
    for f in (1 / 3.0, 2 / 3.0):
        zz = z0 + (z1 - z0) * f
        P.append(box("wbarH", (cx - hw, gy - 2.5, zz - 1.5), (cx + hw, gy + 2.5, zz + 1.5), BLACK))


def window_x(cy, xface, s, z0, z1, w):
    hw = w / 2.0
    xo = sorted((xface, xface + s * 5))
    P.append(box("wsill", (min(xface, xface + s * 9), cy - hw - 8, z0 - 7), (max(xface, xface + s * 9), cy + hw + 8, z0), CONC, 1.0))
    P.append(box("wtrimT", (xo[0], cy - hw - 7, z1), (xo[1], cy + hw + 7, z1 + 9), PLASTER, 0.8))
    for yy in (cy - hw - 7, cy + hw):
        P.append(box("wtrimS", (xo[0], yy, z0), (xo[1], yy + 7, z1), PLASTER, 0.6))
    gx = xface - s * 11
    P.append(box("wglass", (gx - 0.5, cy - hw, z0), (gx + 0.5, cy + hw, z1), WINGLOW))
    P.append(box("wbarV", (gx - 2.5, cy - 1.8, z0), (gx + 2.5, cy + 1.8, z1), BLACK))
    for f in (1 / 3.0, 2 / 3.0):
        zz = z0 + (z1 - z0) * f
        P.append(box("wbarH", (gx - 2.5, cy - hw, zz - 1.5), (gx + 2.5, cy + hw, zz + 1.5), BLACK))


WZ0, WZ1 = 95.0, 200.0
# --- duvarlar
# Kuleye bakan uç (X 250): kapalı (kule girişi ayrı)
wall("wallTower", (HX0, -HY, -2), (HX0 + WT, HY, WH), [])
tri_obj = tri_prism_x("gableTower", HX0, HX0 + WT, -HY, HY, WH, RISE_R, PLASTER)
P.append(tri_obj)
# Uzak uç (X 810): pencere + alınlıkta küçük havalandırma penceresi
wall("wallEnd", (HX1 - WT, -HY, -2), (HX1, HY, WH), [((HX1 - WT - 5, -40, WZ0), (HX1 + 5, 40, WZ1))])
P.append(tri_prism_x("gableEnd", HX1 - WT, HX1, -HY, HY, WH, RISE_R, PLASTER))
window_x(0, HX1, 1, WZ0, WZ1, 80)
# Arka (+Y): iki pencere
wall("wallBack", (HX0, HY - WT, -2), (HX1, HY, WH), [((380 - 40, HY - WT - 5, WZ0), (380 + 40, HY + 5, WZ1)), ((680 - 40, HY - WT - 5, WZ0), (680 + 40, HY + 5, WZ1))])
window_y(380, HY, 1, WZ0, WZ1, 80)
window_y(680, HY, 1, WZ0, WZ1, 80)
# Ön (-Y): kule tarafında pencere; giriş çıkıntısının arkası açık
wall("wallFrontA", (HX0, -HY, -2), (BX0, -HY + WT, WH), [((400 - 40, -HY - 5, WZ0), (400 + 40, -HY + WT + 5, WZ1))])
window_y(400, -HY, -1, WZ0, WZ1, 80)
wall("wallFrontB", (BX1, -HY, -2), (HX1, -HY + WT, WH), [])
P.append(box("frontLintel", (BX0, -HY, WH - 30), (BX1, -HY + WT, WH), PLASTER, 0.4))
# Giriş çıkıntısı: ön duvar (kapı), yan duvarlar, üçgen alınlık
wall("bayFront", (BX0, -BY, -2), (BX1, -BY + WT, WH), [((DOOR_X0, -BY - 5, 0), (DOOR_X1, -BY + WT + 5, DOOR_H))])
wall("baySideA", (BX0, -BY, -2), (BX0 + WT, -HY + WT, WH), [])
wall("baySideB", (BX1 - WT, -BY, -2), (BX1, -HY + WT, WH), [])
P.append(tri_prism_y("bayGable", -BY, -BY + WT, BX0, BX1, WH, BRISE, PLASTER))
# Alınlıkta yuvarlak pencere
P.append(torus("oculusRim", ((BX0 + BX1) / 2, -BY - 1, WH + 42), 17, 3.5, PLASTER, axis='Y', majseg=24, minseg=6))
P.append(cyl("oculus", ((BX0 + BX1) / 2, -BY + 2, WH + 42), 15, 1.0, 'Y', WINGLOW, seg=24))
# Kapı sövesi ve üst pervaz (kanat ayrı aktör)
for xx in (DOOR_X0 - 9, DOOR_X1):
    P.append(box("doorTrim", (xx, -BY - 5, 0), (xx + 9, -BY + 2, DOOR_H + 9), PLASTER, 0.8))
P.append(box("doorHead", (DOOR_X0 - 14, -BY - 8, DOOR_H + 9), (DOOR_X1 + 14, -BY + 2, DOOR_H + 20), PLASTER, 1.0))
# Köşe pilastrları ve saçak altı kuşağı
for (x, y) in ((HX0, -HY), (HX1 - 10, -HY), (HX1 - 10, HY - 10), (BX0, -BY), (BX1 - 10, -BY)):
    P.append(box("corner", (x - 2, y - 2, 0), (x + 12, y + 12, WH), PLASTER, 0.8))
for (lo, hi) in (((HX0, -HY - 4, WH - 14), (HX1, -HY + 2, WH)), ((HX0, HY - 2, WH - 14), (HX1, HY + 4, WH)), ((BX0, -BY - 4, WH - 14), (BX1, -BY + 2, WH))):
    P.append(box("cornice", lo, hi, PLASTER, 0.8))

# --- çatı: ana çatı (sırt X boyunca) ve giriş alınlığının çatısı (sırt Y boyunca)
OV, OVE, TH = 28.0, 26.0, 5.0
slope = RISE_R / HY
zl = WH - OV * slope
for s in (-1, 1):
    y_eave = s * (HY + OV)
    P.append(roof_plane("roof%d" % s, (HX0 - OVE, y_eave, zl), (HX1 + OVE, y_eave, zl), (HX1 + OVE, 0, WH + RISE_R), (HX0 - OVE, 0, WH + RISE_R), TH, ROOF))
    # Dikiş çıtaları
    for k in range(int((HX1 - HX0 + 2 * OVE) / 42) + 1):
        x = HX0 - OVE + 12 + k * 42
        P.append(beam("seam", (x, y_eave, zl + TH + 1.2), (x, 0, WH + RISE_R + TH + 1.2), 2.2, 2.4, ROOF))
    P.append(box("fascia", (HX0 - OVE, min(y_eave, y_eave - s * 4), zl - 14), (HX1 + OVE, max(y_eave, y_eave - s * 4), zl + 2), PLASTER, 0.6))
P.append(cyl("ridge", ((HX0 + HX1) / 2, 0, WH + RISE_R + TH + 3), 5.5, HX1 - HX0 + 2 * OVE, 'X', ROOF, seg=10))
# Uçlarda saçak tahtaları (alınlık kenarı)
for x in (HX0 - OVE, HX1 + OVE - 6):
    for s in (-1, 1):
        P.append(beam("barge", (x + 3, s * (HY + OV), zl - 2), (x + 3, 0, WH + RISE_R - 2), 6, 16, PLASTER, 0.5))
# Giriş alınlığının çatısı: iki eğim, sırtı ana çatıya kadar uzanır
bslope = BRISE / ((BX1 - BX0) / 2)
bov = 22.0
ytip = -55.0
for s in (-1, 1):
    x_e = (BX0 - bov) if s < 0 else (BX1 + bov)
    zle = WH - bov * bslope
    P.append(roof_plane("bayroof%d" % s, (x_e, -BY - bov, zle), ((BX0 + BX1) / 2, -BY - bov, WH + BRISE), ((BX0 + BX1) / 2, ytip, WH + BRISE), (x_e, ytip, zle), TH, ROOF))
    P.append(beam("bayBarge", (x_e, -BY - bov - 3, zle - 2), ((BX0 + BX1) / 2, -BY - bov - 3, WH + BRISE - 2), 6, 16, PLASTER, 0.5))
P.append(cyl("bayRidge", ((BX0 + BX1) / 2, (-BY - bov + ytip) / 2, WH + BRISE + TH + 3), 5, (BY + bov) + ytip, 'Y', ROOF, seg=10))
# Baca: kule tarafında, sırtın arkasında
P.append(box("chimney", (300, 20, WH + 40), (356, 76, WH + RISE_R + 75), PLASTER, 1.5))
P.append(box("chimneyCap", (294, 14, WH + RISE_R + 75), (362, 82, WH + RISE_R + 84), CONC, 1.0))
for dx in (12, 32):
    P.append(cyl("chimneyPot", (300 + dx + 6, 48, WH + RISE_R + 96), 8, 24, 'Z', CONC, seg=12))

# --- taş temel (görünen gri kaide, zemine iner), giriş basamakları, iç döşeme ve tavan
P.append(box("found", (HX0 - 8, -HY - 8, -260), (HX1 + 8, HY + 8, 4), STONE, 2.0))
P.append(box("foundBay", (BX0 - 8, -BY - 8, -260), (BX1 + 8, -HY, 4), STONE, 2.0))
for k in range(4):
    P.append(box("step%d" % k, (DOOR_X0 - 20 - k * 8, -BY - 36 - k * 32, -14 * (k + 1) - 40), (DOOR_X1 + 20 + k * 8, -BY - 4 - k * 32, -14 * k), STONE, 2.0))
P.append(box("ifloor", (HX0 + WT, -HY + WT, 0), (HX1 - WT, HY - WT, 4), WOOD))
P.append(box("ifloorBay", (BX0 + WT, -BY + WT, 0), (BX1 - WT, -HY + WT, 4), WOOD))
P.append(box("ceiling", (HX0, -HY, WH - 10), (HX1, HY, WH - 4), PLASTER))
P.append(box("ceilingBay", (BX0, -BY, WH - 10), (BX1, -HY, WH - 4), PLASTER))
# Duvar dibi süpürgelik (içeride)
for (lo, hi) in (((HX0 + WT, -HY + WT, 4), (HX1 - WT, -HY + WT + 2, 16)), ((HX0 + WT, HY - WT - 2, 4), (HX1 - WT, HY - WT, 16))):
    P.append(box("skirting", lo, hi, WOOD))
house = join("SM_NL_LH_House", P, uv_size=1.0)
export(house, OUT)

# --- kapı kanatları (ANLSwingDoor): orijin = menteşe altı, kanat +X'e uzanır (kapalı), kalınlık Y'de ortalı
P = []
P.append(box("leaf", (0, -2.5, 0), (106, 2.5, 212), WOOD, 0.6))
for z in (40.0, 106.0, 172.0):
    P.append(box("ledge", (6, -3.5, z - 7), (100, -2.5, z + 7), WOOD, 0.4))
    P.append(box("ledgeB", (6, 2.5, z - 7), (100, 3.5, z + 7), WOOD, 0.4))
for z in (35.0, 175.0):
    P.append(box("hinge", (-2, -3.8, z - 3), (40, 3.8, z + 3), BLACK, 0.3))
P.append(cyl("knob", (94, -6, 100), 3, 6, 'Y', BLACK, seg=12))
P.append(cyl("knobB", (94, 6, 100), 3, 6, 'Y', BLACK, seg=12))
dh = join("SM_NL_LH_DoorHouse", P, uv_size=0.5)
export(dh, OUT)
P = []
P.append(box("leaf", (0, -3, 0), (116, 3, 213), WOOD, 0.6))
for x in (8.0, 38.0, 68.0, 98.0):
    P.append(box("plank", (x - 0.6, -3.6, 0), (x + 0.6, 3.6, 213), BLACK))
for z in (30.0, 180.0):
    P.append(box("strap", (-2, -4.2, z - 4), (100, 4.2, z + 4), BLACK, 0.4))
    for x in (20.0, 50.0, 80.0):
        P.append(cyl("rivet", (x, -4.6, z), 1.2, 1.2, 'Y', BLACK, seg=6, smooth=False))
P.append(torus("ring", (104, -6.5, 104), 5, 1, BLACK, axis='Y', majseg=12, minseg=6))
dt = join("SM_NL_LH_DoorTower", P, uv_size=0.5)
export(dt, OUT)

if os.environ.get("NL_PREVIEW") == "1":
    gal.location = to_bl(0, 0, 1000)
    lr.location = to_bl(0, 0, 1014)
    house.location = to_bl(0, 0, 40)
    dh.location = to_bl(500, -900, 0)
    dt.location = to_bl(500, -900, 0)
    preview(OUT, "lighthouse_preview.png", (1350, -1900, 650), (300, 0, 520),
            {WORN: (0.82, 0.8, 0.75, 1), BLACK: (0.06, 0.06, 0.07, 1), GLASS: (0.35, 0.45, 0.5, 1), STONE: (0.4, 0.4, 0.38, 1),
             ROOF: (0.45, 0.47, 0.5, 1), WOOD: (0.35, 0.25, 0.15, 1), WINGLOW: (1.0, 0.75, 0.4, 1), CONC: (0.35, 0.35, 0.34, 1)},
            lens=32, res=(1300, 1300))
    # İç: merdiven (kesit görünümü için kulenin yarısını gizlemek yerine yukarıdan bakış)
    twr.hide_render = False
