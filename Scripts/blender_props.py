# No Light After Midnight - Küçük eşyalar: benzin bidonu, yol feneri direği, fener, alev
# Çalıştırma: blender.exe -b --factory-startup --python Scripts/blender_props.py -- <çıktı klasörü>   (önizleme: NL_PREVIEW=1)
# Çıktı:
#   SM_NL_JerryCan     : 20 L metal bidon. Taban z=0, ince eksen X (±8), Y ±17, gövde 44, saplarla ~52. Ağız -Y üst köşede.
#   SM_NL_GuidePost    : taş kaideli ahşap direk + kol. Orijin zemin ortası, kol +X'e uzanır; fener kancası (55, 0, 220).
#   SM_NL_GuideLantern : asılı fener (orijin = üstteki halka). Camlar MI_NL_LanternGlass (C++ titretir), içinde mum.
#   SM_NL_LanternFlame : mum alevi (orijin = alevin dibi). Fenerin içinde (0, 0, -29); C++ salınım verir.
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy
from blender_nl_common import *  # noqa

reset()
OUT = out_dir("props_out")
random.seed(5)

# ------------------------------------------------------------------ benzin bidonu
G, M = "MI_NL_JerryGreen", "MI_NL_Metal"
P = []
P.append(box("body", (-8, -17, 0), (8, 17, 44), G, 2.6, segs=3))
# Çevre kaynak dikişi (ortadan geçen bant)
P.append(box("seam", (-0.7, -17.6, -0.3), (0.7, 17.6, 44.6), G, 0.6, segs=2))
for s in (-1, 1):
    x = s * 8.0
    xo = s * 8.55
    lo, hi = min(x, xo), max(x, xo)
    # Kabartma çerçeve ve X
    P.append(box("rimT", (lo, -14, 39.5), (hi, 14, 41), G, 0.4))
    P.append(box("rimB", (lo, -14, 3), (hi, 14, 4.5), G, 0.4))
    P.append(box("rimL", (lo, -14, 3), (hi, -12.5, 41), G, 0.4))
    P.append(box("rimR", (lo, 12.5, 3), (hi, 14, 41), G, 0.4))
    for (y0, z0, y1, z1) in ((-12, 5.5, 12, 38.5), (12, 5.5, -12, 38.5)):
        P.append(beam("x", (s * 8.25, y0, z0), (s * 8.25, y1, z1), 1.6, 0.6 + 0.0, G, 0.25))
    # Ortadaki etiket yeri (oval çerçeve)
    P.append(box("label", (lo, -5.5, 19.5), (hi + s * 0.15, 5.5, 25.5), G, 0.8, segs=2))
P.append(text("t20", "20L", (8.7, 0, 34.5), 3.2, G, face='+X', depth=0.35))
P.append(text("t20b", "20L", (-8.7, 0, 34.5), 3.2, G, face='-X', depth=0.35))
# Üstte üçlü sap
for y in (-8.0, 0.0, 8.0):
    P.append(box("hpost", (-1.2, y - 1.4, 43.5), (1.2, y + 1.4, 50.5), G, 0.5))
P.append(box("hgrip", (-1.8, -11.5, 49.5), (1.8, 11.5, 52.5), G, 1.2, segs=2))
# Ağız (−Y üst köşe), kapak ve kam kolu
P.append(cyl("neck", (0, -13.0, 45.0), 2.8, 4.0, 'Z', M, seg=16))
P.append(cyl("cap", (0, -13.0, 48.0), 3.3, 2.2, 'Z', G, seg=16))
P.append(box("camlever", (-1.0, -16.5, 47.5), (1.0, -5.5, 49.2), M, 0.4, rot=(-12, 0, 0)))
P.append(cyl("hinge", (0, -16.2, 46.5), 0.9, 4.0, 'X', M, seg=10))
can = join("SM_NL_JerryCan", P, uv_size=0.25)
export(can, OUT)

# ------------------------------------------------------------------ yol feneri direği
WOOD, STONE, IRON = "MI_NL_Wood", "MI_NL_Rock", "MI_NL_FuseCabinet"
P = []
base = cyl("stone", (0, 0, 25), 23, 70, 'Z', STONE, seg=4, smooth=False, r2=17)
base.rotation_euler = (0, 0, math.radians(45))
P.append(finish(base, STONE, bevel=1.5))
P.append(box("stonecap", (-15, -15, 58), (15, 15, 63), STONE, 1.0))
P.append(box("post", (-6.5, -6.5, 55), (6.5, 6.5, 252), WOOD, 0.8))
P.append(box("postcap", (-7.5, -7.5, 250), (7.5, 7.5, 254), WOOD, 0.8))
for z in (66.0, 74.0):
    P.append(box("band", (-7.2, -7.2, z), (7.2, 7.2, z + 2.2), IRON, 0.3))
P.append(box("arm", (-6.5, -5, 236), (62, 5, 246), WOOD, 0.8))
P.append(beam("brace", (5.5, 0, 192), (40, 0, 238), 7, 6.5, WOOD, 0.6))
P.append(box("plate", (6.5, -3.5, 200), (8, 3.5, 230), IRON, 0.2))
for z in (205.0, 225.0):
    P.append(cyl("bolt", (8.3, 0, z), 0.9, 0.8, 'X', IRON, seg=6, smooth=False))
P.append(torus("hookring", (55, 0, 233.5), 1.8, 0.45, IRON, axis='Y'))
P.append(cyl("hookbar", (55, 0, 237.5), 0.5, 5, 'Z', IRON, seg=8))
for i in range(4):
    z = 230.0 - i * 3.0
    P.append(torus("chain%d" % i, (55, 0, z), 1.5, 0.35, IRON, axis='Y' if i % 2 else 'X', majseg=12, minseg=6))
post = join("SM_NL_GuidePost", P, uv_size=0.5)
export(post, OUT)

# ------------------------------------------------------------------ asılı fener (orijin üst halkada)
GLASS, WAX = "MI_NL_LanternGlass", "MI_NL_GaugeFace"
P = []
P.append(torus("ring", (0, 0, -2.2), 1.7, 0.4, IRON, axis='Y', majseg=16, minseg=6))
P.append(cyl("top", (0, 0, -5.0), 1.6, 2.4, 'Z', IRON, seg=12))
roof = cyl("roof", (0, 0, -11.0), 13.0, 9.5, 'Z', IRON, seg=4, smooth=False, r2=3.2)
roof.rotation_euler = (0, 0, math.radians(45))
P.append(roof)
P.append(box("eave", (-9.5, -9.5, -17.0), (9.5, 9.5, -15.6), IRON, 0.3))
H0, H1, Wd = -35.5, -16.0, 8.0
for sx in (-1, 1):
    for sy in (-1, 1):
        P.append(box("cpost", (sx * Wd - 0.8, sy * Wd - 0.8, H0), (sx * Wd + 0.8, sy * Wd + 0.8, H1), IRON))
for s in (-1, 1):
    # Camlar (4 yan)
    P.append(box("glassX", (s * Wd - 0.25, -Wd + 0.8, H0 + 0.5), (s * Wd + 0.25, Wd - 0.8, H1 - 0.5), GLASS))
    P.append(box("glassY", (-Wd + 0.8, s * Wd - 0.25, H0 + 0.5), (Wd - 0.8, s * Wd + 0.25, H1 - 0.5), GLASS))
    # Kayıtlar (artı biçimi)
    P.append(box("mulXh", (s * Wd - 0.5, -Wd, -26.2), (s * Wd + 0.5, Wd, -25.2), IRON))
    P.append(box("mulXv", (s * Wd - 0.5, -0.5, H0), (s * Wd + 0.5, 0.5, H1), IRON))
    P.append(box("mulYh", (-Wd, s * Wd - 0.5, -26.2), (Wd, s * Wd + 0.5, -25.2), IRON))
    P.append(box("mulYv", (-0.5, s * Wd - 0.5, H0), (0.5, s * Wd + 0.5, H1), IRON))
P.append(box("frameT", (-Wd - 0.9, -Wd - 0.9, H1 - 0.6), (Wd + 0.9, Wd + 0.9, H1 + 0.6), IRON, 0.2))
P.append(box("frameB", (-Wd - 1.2, -Wd - 1.2, H0 - 1.4), (Wd + 1.2, Wd + 1.2, H0 + 0.4), IRON, 0.3))
bot = cyl("bottom", (0, 0, -39.0), 9.5, 3.0, 'Z', IRON, seg=4, smooth=False, r2=12.5)
bot.rotation_euler = (0, 0, math.radians(45))
P.append(bot)
P.append(sphere("finial", (0, 0, -41.5), 1.6, IRON))
# Mum
P.append(cyl("candle", (0, 0, -32.2), 1.7, 5.0, 'Z', WAX, seg=12))
P.append(cyl("wick", (0, 0, -29.3), 0.18, 0.8, 'Z', "MI_NL_Ink", seg=6))
lan = join("SM_NL_GuideLantern", P, uv_size=0.2)
export(lan, OUT)

# ------------------------------------------------------------------ alev (orijin dipte)
fl = sphere("flame", (0, 0, 2.3), 1.0, "MI_NL_Flame", scale=(0.9, 0.9, 2.5), seg=12, rings=8)
# Üstü sivri: üst köşeleri içeri çek
for v in fl.data.vertices:
    z = v.co.z / (2.5)
    if z > 0:
        k = max(0.15, 1.0 - z / (1.0 * C) * 0.75)
        v.co.x *= k
        v.co.y *= k
flame = join("SM_NL_LanternFlame", [fl], uv_size=0.05)
export(flame, OUT)

if os.environ.get("NL_PREVIEW") == "1":
    # Fener direğe asılı, alev içinde, bidon yerde
    lan.location = to_bl(55, 0, 220)
    flame.location = to_bl(55, 0, 220 - 29)
    can.location = to_bl(-45, 40, 0)
    preview(OUT, "props_preview.png", (190, -230, 170), (15, 0, 120),
            {G: (0.18, 0.25, 0.1, 1), M: (0.5, 0.5, 0.5, 1), WOOD: (0.45, 0.28, 0.12, 1), STONE: (0.4, 0.4, 0.38, 1),
             IRON: (0.06, 0.06, 0.06, 1), GLASS: (1.0, 0.75, 0.35, 1), WAX: (0.9, 0.88, 0.8, 1), "MI_NL_Ink": (0, 0, 0, 1),
             "MI_NL_Flame": (1.0, 0.6, 0.1, 1)}, lens=40, res=(1000, 1100))
    # Bidon yakın plan
    bpy.data.objects.remove(bpy.data.objects["cam"])
    can.location = to_bl(0, 0, 0)
    post.location = to_bl(500, 500, 0)
    lan.location = to_bl(500, 500, 0)
    flame.location = to_bl(500, 500, 0)
    preview(OUT, "can_preview.png", (70, -60, 50), (0, 0, 25),
            {G: (0.2, 0.27, 0.1, 1), M: (0.5, 0.5, 0.5, 1)}, lens=50, res=(900, 900))
