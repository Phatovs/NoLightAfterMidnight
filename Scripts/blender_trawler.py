# No Light After Midnight - Eski balıkçı teknesi (açılış sinematiği)
# Çalıştırma: blender.exe -b --factory-startup --python Scripts/blender_trawler.py -- <çıktı klasörü>   (önizleme: NL_PREVIEW=1)
# Çıktı:
#   SM_NL_Trawler      : gövde, güverte, kaptan köşkü, direk, bumba, korkuluklar, güverte eşyaları. Orijin su çizgisinde, +X pruva.
#   SM_NL_TrawlerWheel : dümen dümeni (orijin göbekte, X ekseninde döner)
# Ölçüler (Unreal cm, C++ NLIntroDirector ile aynı olmalı):
#   Boy X -450..450, en ±175, güverte üstü Z 80. Kaptan köşkü X -40..140, Y ±105, tavan 285.
#   Dümen merkezi (112, 0, 175); kaptan (85, 0, 80) ayakta. Oyuncu ve yolcu arka güvertede (X ~ -150 / -230).
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
OUT = out_dir("trawler_out")
random.seed(3)
WHITE, BLUE, BOTTOM, BLACK, DECK = "MI_NL_TrawlerWhite", "MI_NL_TrawlerBlue", "MI_NL_TrawlerBottom", "MI_NL_Rubber", "MI_NL_DeckWood"
STEEL, DARK, WOOD, ROPE, RED = "MI_NL_Metal", "MI_NL_FuseCabinet", "MI_NL_Wood", "MI_NL_Rope", "MI_NL_JerryRed"
XS, XB = -450.0, 450.0
DECKZ = 80.0


def u_of(x):
    return (x - XS) / (XB - XS)


def beam_at(x):
    u = u_of(x)
    if u < 0.45:
        return 150.0 + 25.0 * math.sin(math.pi / 2 * u / 0.45)
    t = (u - 0.45) / 0.55
    return max(1.0, 175.0 * math.sqrt(max(0.0, 1.0 - t ** 2.2)))


def sheer_at(x):
    u = u_of(x)
    s = 98.0 + (2.0 * (1.0 - u / 0.45) if u < 0.45 else 0.0)
    if u > 0.45:
        s += 77.0 * ((u - 0.45) / 0.55) ** 2
    return s


def keel_at(x):
    u = u_of(x)
    z = -110.0
    if u < 0.2:
        z += 30.0 * ((0.2 - u) / 0.2) ** 2
    if u > 0.7:
        z += 200.0 * ((u - 0.7) / 0.3) ** 2.2
    return min(z, sheer_at(x) - 20.0)


def shape_p(x):
    u = u_of(x)
    return 2.6 - 1.3 * max(0.0, (u - 0.6) / 0.4)


def hull_point(x, v):
    """v: 0 omurga .. 1 küpeşte (borda) -> (y, z) +Y tarafı"""
    zk, zs = keel_at(x), sheer_at(x)
    z = zk + (zs - zk) * v
    y = beam_at(x) * (1.0 - (1.0 - v) ** shape_p(x))
    # Su üstünde hafif dışa açılma (flare), pruvada daha çok
    y *= 1.0 + 0.06 * max(0.0, v - 0.6) * (1.0 + 2.0 * max(0.0, u_of(x) - 0.6))
    return y, z


def y_at_height(x, zq):
    zk, zs = keel_at(x), sheer_at(x)
    if zq <= zk:
        return 0.0
    v = min(1.0, (zq - zk) / max(1.0, zs - zk))
    return hull_point(x, v)[0]


# ------------------------------------------------------------------ gövde (kesitlerden)
NS, NV = 56, 18
bm = bmesh.new()
grid = []
for i in range(NS + 1):
    x = XS + (XB - XS) * i / NS
    ring = []
    for s in (1, -1):
        col = []
        for j in range(NV + 1):
            v = j / NV
            y, z = hull_point(x, v)
            col.append(bm.verts.new(to_bl(x, s * y, z)))
        ring.append(col)
    grid.append(ring)
mats = [WHITE, BLUE, BOTTOM]


def matidx(zc):
    return 2 if zc < -12 else (1 if zc < 46 else 0)


# Boya çizgileri gövde hatlarını izler (satır = kemer boyunca aynı oran): alt %47 pas, %47-%75 mavi, üstü beyaz
def matrow(j):
    v = (j + 0.5) / NV
    return 2 if v < 0.47 else (1 if v < 0.75 else 0)


for i in range(NS):
    for si in range(2):
        for j in range(NV):
            a, b = grid[i][si][j], grid[i + 1][si][j]
            c, d = grid[i + 1][si][j + 1], grid[i][si][j + 1]
            f = bm.faces.new([a, b, c, d] if si == 0 else [a, d, c, b])
            f.material_index = matrow(j)
# Ayna (transom): arka düz yüz
tr = [grid[0][0][j] for j in range(NV, -1, -1)] + [grid[0][1][j] for j in range(1, NV + 1)]
f = bm.faces.new(tr)
f.material_index = 0
bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=0.002)
bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
hull = new_obj("hull", bm)
for m in mats:
    hull.data.materials.append(mat(m))
for p in hull.data.polygons:
    p.use_smooth = True
sol = hull.modifiers.new("solid", 'SOLIDIFY')
sol.thickness = 4 * C
sol.offset = -1.0
P = [hull]

# Ayna yüzündeki renkleri düzelt (alt kısmı kahverengi, orta mavi): ayrı ince plakalar
for (z0, z1, m) in ((-80, -12, BOTTOM), (-12, 46, BLUE)):
    w0, w1 = y_at_height(XS, z0 + 1), y_at_height(XS, z1)
    P.append(poly_prism("transomPaint", [(XS - 0.6, z0), (XS - 0.6, z1), (XS - 0.6, z1), (XS - 0.6, z0)], 0, 0.1, m))
# (ince plaka yerine aynanın kendisi beyaz kalır; boyalı şerit kenarlardan görünür)
P.pop()
P.pop()

# Güverte (Z 80)
bm = bmesh.new()
pts_r, pts_l = [], []
x = XS + 3
xs = []
while x < XB:
    if y_at_height(x, DECKZ) - 4 < 4:
        break
    xs.append(x)
    x += 15.0
for x in xs:
    w = y_at_height(x, DECKZ) - 4.0
    pts_r.append(bm.verts.new(to_bl(x, w, DECKZ)))
    pts_l.append(bm.verts.new(to_bl(x, -w, DECKZ)))
for k in range(len(xs) - 1):
    bm.faces.new([pts_l[k], pts_l[k + 1], pts_r[k + 1], pts_r[k]])
deck = new_obj("deck", bm)
finish(deck, DECK)
sd = deck.modifiers.new("solid", 'SOLIDIFY')
sd.thickness = 3 * C
P.append(deck)

# Sürtünme şeritleri (siyah) ve küpeşte üstü
for s in (1, -1):
    for zq, r in ((46.0, 2.6), (8.0, 1.8)):
        pts = [(x, s * (y_at_height(x, zq) + 1.5), zq) for x in range(int(XS) + 5, 440, 30) if y_at_height(x, zq) > 5]
        if len(pts) > 2:
            P.append(wire("strake", pts, r, BLACK))
    pts = [(x, s * (hull_point(x, 1.0)[0] - 1.0), sheer_at(x) + 1.5) for x in range(int(XS) + 2, 446, 25)]
    P.append(wire("caprail", pts, 3.2, BOTTOM))
# Omurga ve dümen bıçağı
for x0 in range(-410, 380, 40):
    x1 = x0 + 40
    P.append(beam("keel", (x0, 0, keel_at(x0) - 8), (x1, 0, keel_at(x1) - 8), 1, 9, BOTTOM))
P.append(box("skeg", (-440, -5, keel_at(-430) - 14), (-380, 5, keel_at(-430) + 10), BOTTOM, 1.0))
P.append(box("rudder", (-470, -3, -110), (-447, 3, 0), BOTTOM, 1.2))
P.append(cyl("rudderpost", (-450, 0, -20), 2.5, 140, 'Z', BOTTOM, seg=10))
P.append(cyl("prop", (-438, 0, -80), 7, 10, 'X', STEEL, seg=12))
# Lumbozlar
for s in (1, -1):
    for x in (210.0, 90.0, -110.0):
        y = y_at_height(x, 68) + 1.0
        P.append(torus("port", (x, s * y, 68), 5.0, 1.2, STEEL, axis='Y'))
        P.append(cyl("portglass", (x, s * (y - 0.6), 68), 4.2, 0.6, 'Y', DARK, seg=16))

# ------------------------------------------------------------------ kaptan köşkü
WX0, WX1, WY, WT, Z0, ZW0, ZW1, ZR = -40.0, 140.0, 105.0, 5.0, DECKZ, 190.0, 250.0, 285.0


def wall_x(x0, x1, y_face, s, posts, door=None):
    """Y'de duran yan duvar parçaları: alt panel, üst bant, pencere aralarındaki dikmeler"""
    y0, y1 = sorted((y_face, y_face - s * WT))
    if door:
        P.append(box("wlow", (x0, y0, Z0), (door[0], y1, ZW0), WHITE, 0.4))
        P.append(box("wlow", (door[1], y0, Z0), (x1, y1, ZW0), WHITE, 0.4))
    else:
        P.append(box("wlow", (x0, y0, Z0), (x1, y1, ZW0), WHITE, 0.4))
    P.append(box("wtop", (x0, y0, ZW1), (x1, y1, ZR), WHITE, 0.4))
    for (a, b) in posts:
        P.append(box("wpost", (a, y0, ZW0), (b, y1, ZW1), WHITE, 0.3))


def wall_y(y0w, y1w, x_face, s, posts):
    x0, x1 = sorted((x_face, x_face - s * WT))
    P.append(box("wlowY", (x0, y0w, Z0), (x1, y1w, ZW0), WHITE, 0.4))
    P.append(box("wtopY", (x0, y0w, ZW1), (x1, y1w, ZR), WHITE, 0.4))
    for (a, b) in posts:
        P.append(box("wpostY", (x0, a, ZW0), (x1, b, ZW1), WHITE, 0.3))


# Ön duvar: 4 pencere
wall_y(-WY, WY, WX1, 1, [(-WY, -89), (-51, -42), (-4, 4), (42, 51), (89, WY)])
# Arka duvar: ortada pencere, can simidi
wall_y(-WY, WY, WX0 + WT, 1, [(-WY, -30), (30, WY)])
# Yan duvarlar: kapı (arkada), iki pencere
for s in (1, -1):
    wall_x(WX0, WX1, s * WY, s, [(WX0, -30), (10, 30), (70, 80), (120, WX1)], door=(-30, 10))
    # Kapı kanadı (yarı açık, dışa)
    dr = box("door", (0, -1.5, Z0 + 2), (38, 1.5, ZW1 - 4), WHITE, 0.5)
    dr.location = to_bl(-30 + 19 * math.cos(math.radians(70)), s * (WY + 19 * math.sin(math.radians(70))), (Z0 + ZW1) / 2)
    dr.rotation_euler = (0, 0, math.radians(-s * 70))
    P.append(dr)
    P.append(box("doorwin", (0, 0, 0), (1, 1, 1), DARK))
    P.pop()
# Pencere çerçeveleri (koyu, ince)
for s in (1, -1):
    for (a, b) in ((30.0, 70.0), (80.0, 120.0)):
        P.append(box("wframe", (a, s * WY - 1, ZW0 - 1), (b, s * WY + 1, ZW0 + 1), DARK))
        P.append(box("wframe", (a, s * WY - 1, ZW1 - 1), (b, s * WY + 1, ZW1 + 1), DARK))
# Tavan (mavi üst, beyaz saçak)
P.append(box("roof", (WX0 - 8, -WY - 8, ZR), (WX1 + 8, WY + 8, ZR + 6), WHITE, 1.5))
P.append(box("roofTop", (WX0 - 4, -WY - 4, ZR + 6), (WX1 + 4, WY + 4, ZR + 10), BLUE, 1.5))
P.append(box("floor", (WX0 + 2, -WY + 2, Z0), (WX1 - 2, WY - 2, Z0 + 3), DARK))
# İç: konsol, gösterge, pusula, dümen mili
P.append(box("console", (118, -96, Z0), (WX1 - WT, 96, 165), DARK, 1.5))
P.append(box("consoleTop", (112, -96, 158), (WX1 - WT, 96, 168), WOOD, 1.0, rot=(0, -12, 0)))
P.append(cyl("wheelShaft", (122, 0, 175), 3, 18, 'X', STEEL, seg=10))
P.append(sphere("compass", (125, -40, 172), 7, STEEL, scale=(1, 1, 0.6)))
for k, y in enumerate((-80.0, -60.0, 45.0, 70.0)):
    P.append(box("instr", (125, y - 7, 165), (133, y + 7, 182), DARK, 0.8))
    P.append(cyl("dial", (124.6, y, 173.5), 5, 0.6, 'X', "MI_NL_GaugeFace", seg=16))
P.append(box("lampFix", (40, -8, ZR - 5), (60, 8, ZR), DARK, 0.5))
P.append(sphere("bulb", (50, 0, ZR - 8), 3.5, "MI_NL_LanternGlass"))
# Can simidi (arka duvarda, dışta)
P.append(torus("lifering", (WX0 - 2.5, 60, 200), 17, 5, RED, axis='X'))
for k in range(4):
    a = math.radians(45 + 90 * k)
    P.append(box("lrtape", (WX0 - 8, 60 + math.cos(a) * 17 - 3, 200 + math.sin(a) * 17 - 3), (WX0 + 3, 60 + math.cos(a) * 17 + 3, 200 + math.sin(a) * 17 + 3), WHITE, 1.0))

# ------------------------------------------------------------------ direk (A çerçeve + merdiven), anten, bumba
TOP = Vector((90, 0, 640))
legs = (Vector((105, -62, ZR + 10)), Vector((105, 62, ZR + 10)))
for lg in legs:
    P.append(tube("mastleg", lg, TOP, 3.2, STEEL))
    P.append(cyl("mastfoot", (lg.x, lg.y, ZR + 12), 6, 4, 'Z', STEEL, seg=10))
for k in range(1, 11):
    t = k / 11.0
    a = legs[0].lerp(TOP, t)
    b = legs[1].lerp(TOP, t)
    P.append(tube("rung", a, b, 1.4, STEEL))
P.append(tube("mastpole", (90, 0, ZR + 10), (90, 0, 760), 2.6, STEEL))
P.append(tube("radarbar", (88, -72, 560), (88, 72, 560), 2.2, STEEL))
for y in (-72.0, 72.0):
    P.append(tube("antenna", (88, y, 560), (88, y, 690), 0.7, STEEL))
for (x, y, h) in ((60, -40, 140), (40, 30, 110), (120, 50, 90), (0, -70, 120)):
    P.append(tube("whip", (x, y, ZR + 10), (x, y, ZR + 10 + h), 0.6, STEEL))
P.append(sphere("mastlight", (90, 0, 764), 4, "MI_NL_LanternGlass"))
P.append(box("radar", (78, -30, 600), (102, 30, 606), DARK, 1.0))
# Bumba: köşkün arkasından kıça doğru yükselir
BA, BB = Vector((-30, 60, ZR + 10)), Vector((-175, 35, 650))
P.append(tube("boom", BA, BB, 4.5, BOTTOM))
P.append(tube("boom2", BA + Vector((0, -16, 0)), BB + Vector((0, -10, 0)), 3.0, BOTTOM))
for k in range(1, 6):
    t = k / 6.0
    P.append(tube("boomtie", BA.lerp(BB, t), (BA + Vector((0, -16, 0))).lerp(BB + Vector((0, -10, 0)), t), 1.2, BOTTOM))
P.append(cyl("block", (BB.x, BB.y - 5, BB.z - 10), 4, 9, 'Y', DARK, seg=10))
# Halatlar / teller
P.append(wire("forestay", [tuple(TOP), (265, 0, 410), (445, 0, sheer_at(440) + 4)], 0.5, DARK))
for s in (1, -1):
    P.append(wire("backstay", [tuple(TOP), (-445, s * 140, 104)], 0.5, DARK))
P.append(wire("toplift", [tuple(BB), (-40, 25, 640), tuple(TOP)], 0.5, DARK))
P.append(wire("boomcable", [tuple(BB), (-200, 30, 400), (-250, 20, 140)], 0.6, DARK))

# ------------------------------------------------------------------ korkuluklar (pruva ve kıç)
for s in (1, -1):
    xs_r = list(range(155, 431, 46))
    tops = []
    for x in xs_r:
        y = s * (hull_point(x, 1.0)[0] - 3.0)
        z = sheer_at(x) + 3
        P.append(tube("rpost", (x, y, z), (x, y, z + 40), 1.4, STEEL, seg=8))
        tops.append((x, y, z + 40))
    P.append(wire("rtop", tops, 1.5, STEEL))
    P.append(wire("rmid", [(x, y, z - 20) for (x, y, z) in tops], 1.1, STEEL))
    for x in (-440.0, -390.0, -340.0):
        y = s * (hull_point(x, 1.0)[0] - 3.0)
        P.append(tube("spost", (x, y, sheer_at(x) + 3), (x, y, sheer_at(x) + 34), 1.4, STEEL, seg=8))
    P.append(wire("srail", [(x, s * (hull_point(x, 1.0)[0] - 3.0), sheer_at(x) + 34) for x in (-440, -390, -340)], 1.3, STEEL))

# ------------------------------------------------------------------ güverte eşyaları
P.append(box("hold", (-330, -55, DECKZ), (-235, 50, DECKZ + 62), BLUE, 3.0, segs=2))
P.append(box("holdLid", (-334, -59, DECKZ + 62), (-231, 54, DECKZ + 68), BLUE, 2.0))
P.append(box("crate", (-205, -120, DECKZ), (-160, -80, DECKZ + 35), WOOD, 1.5, rot=(0, 0, 8)))
for k in range(5):
    x, y = -230 + random.uniform(-25, 25), 95 + random.uniform(-15, 15)
    P.append(sphere("net%d" % k, (x, y, DECKZ + 10), 18, ROPE, scale=(1.3, 1.0, 0.45)))
for k in range(4):
    P.append(torus("coil%d" % k, (-410, 70, DECKZ + 3 + k * 2.6), 14 - k, 1.6, ROPE, axis='Z'))
P.append(cyl("winch", (-65, 0, DECKZ + 32), 18, 70, 'Y', DARK, seg=20))
P.append(box("winchBase", (-80, -45, DECKZ), (-50, 45, DECKZ + 20), DARK, 1.0))
for s in (1, -1):
    for x in (-420.0, -150.0, 250.0):
        y = s * (y_at_height(x, DECKZ) - 18)
        P.append(cyl("bollard", (x, y, DECKZ + 12), 5, 24, 'Z', DARK, seg=12))
        P.append(cyl("bollardTop", (x, y, DECKZ + 25), 6.5, 3, 'Z', DARK, seg=12))
# Halat makarası ve tel düğümleri pruvada
P.append(cyl("anchorwinch", (330, 0, DECKZ + 22), 14, 40, 'Y', DARK, seg=16))
P.append(box("hatch", (180, -40, DECKZ), (240, 40, DECKZ + 18), WHITE, 2.0))

boat = join("SM_NL_Trawler", P, uv_size=1.2)
export(boat, OUT)

# ------------------------------------------------------------------ dümen (orijin göbek, X ekseni)
P = []
P.append(torus("rim", (0, 0, 0), 22, 1.8, WOOD, axis='X', majseg=36, minseg=8))
P.append(cyl("hub", (0, 0, 0), 4.5, 6, 'X', STEEL, seg=12))
for k in range(8):
    a = math.radians(k * 45)
    p_in = (0, math.sin(a) * 4, math.cos(a) * 4)
    p_out = (0, math.sin(a) * 30, math.cos(a) * 30)
    P.append(tube("spoke%d" % k, p_in, p_out, 1.1, WOOD, seg=8))
    P.append(sphere("knob%d" % k, (0, math.sin(a) * 30.5, math.cos(a) * 30.5), 1.8, WOOD, scale=(1, 1, 1)))
wheel = join("SM_NL_TrawlerWheel", P, uv_size=0.2)
export(wheel, OUT)

if os.environ.get("NL_PREVIEW") == "1":
    wheel.location = to_bl(112, 0, 175)
    preview(OUT, "trawler_preview.png", (520, -1050, 420), (0, 0, 200),
            {WHITE: (0.85, 0.85, 0.82, 1), BLUE: (0.3, 0.55, 0.8, 1), BOTTOM: (0.35, 0.16, 0.08, 1), BLACK: (0.03, 0.03, 0.03, 1),
             DECK: (0.35, 0.27, 0.18, 1), STEEL: (0.6, 0.6, 0.6, 1), DARK: (0.07, 0.07, 0.07, 1), WOOD: (0.45, 0.28, 0.12, 1),
             ROPE: (0.15, 0.2, 0.12, 1), RED: (0.7, 0.07, 0.05, 1), "MI_NL_GaugeFace": (0.9, 0.9, 0.85, 1),
             "MI_NL_LanternGlass": (1, 0.8, 0.4, 1)}, lens=30, res=(1500, 1100))
