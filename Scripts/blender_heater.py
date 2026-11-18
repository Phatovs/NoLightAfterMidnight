# No Light After Midnight - Eski tip elektrikli ısıtıcı (krem emaye kasa, krom reflektör, üç akkor ısıtıcı çubuk, tel ızgara)
# Çalıştırma: blender.exe -b --factory-startup --python Scripts/blender_heater.py -- <çıktı klasörü>   (önizleme: NL_PREVIEW=1)
# Çıktı: SM_NL_Heater.fbx
# Unreal yerel (cm): pivot taban ortası, ön yüz +X. X -14..14 (arka -14), Y ±33, Z 0..~80 (kulp dahil).
#   Isıtıcı çubuklar ve gösterge lambası yuvası MI_NL_HeaterGlow: C++ (ANLHeater) elektrik varken kızdırır.
#   Fiş kablosu arkadan (-22, ...) duvar prizine gider: arka yüz duvardan 8 cm önde durur.
import math
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy
from blender_nl_common import *  # noqa

reset()
OUT = out_dir("heater_out")
PAINT, METAL, GLOW, BLACK, CER, INK = "MI_NL_HeaterPaint", "MI_NL_Metal", "MI_NL_HeaterGlow", "MI_NL_Rubber", "MI_NL_FuseCeramic", "MI_NL_Ink"
P = []

# ------------------------------------------------------------------ kasa: yanlar (önü yuvarlatılmış profil), arka, alt, üst
R, CX, TOPZ = 12.0, 2.0, 72.0
prof = [(-14.0, 6.0), (14.0, 6.0), (14.0, TOPZ - R)]
for k in range(1, 10):
    a = math.radians(90.0 * k / 9)
    prof.append((CX + R * math.cos(a), TOPZ - R + R * math.sin(a)))
prof.append((-14.0, TOPZ))
for (y0, y1) in ((-32.0, -29.0), (29.0, 32.0)):
    P.append(poly_prism("side", prof, y0, y1, PAINT))
P.append(box("back", (-14, -29, 6), (-12, 29, TOPZ), PAINT))
P.append(box("bottom", (-14, -29, 6), (14, 29, 8), PAINT))
# Üst: düz kısım + öne doğru kıvrılan bant
P.append(box("topFlat", (-14, -29, TOPZ - 1.5), (CX, 29, TOPZ), PAINT))
band = []
for k in range(0, 10):
    a = math.radians(90.0 * k / 9)
    band.append((CX + R * math.cos(a), TOPZ - R + R * math.sin(a)))
inner = [(CX + (R - 1.5) * math.cos(math.radians(90.0 * k / 9)), TOPZ - R + (R - 1.5) * math.sin(math.radians(90.0 * k / 9))) for k in range(9, -1, -1)]
P.append(poly_prism("topBand", band + inner, -29, 29, PAINT))
# Alt ön panel (ızgaranın altı) ve ince süs çıtası
P.append(box("lowerFront", (11, -29, 8), (14, 29, 17), PAINT))
P.append(box("trim", (13.6, -29, 16.5), (14.4, 29, 17.5), METAL))
P.append(text("label", "2 kW", (14.25, -14, 12.2), 2.6, INK, face='+X'))
# Ön çerçevenin üst ucu (kıvrımın altında)
P.append(box("upperFront", (12.4, -29, 58), (14, 29, 60), PAINT))

# ------------------------------------------------------------------ reflektör (parabolik krom) ve ısıtıcı çubuklar
ZC = 37.5
outer = []
innr = []
for k in range(0, 15):
    z = 18.0 + (58.0 - 18.0) * k / 14
    x = -10.0 + 0.0115 * (z - ZC) ** 2
    outer.append((x - 1.0, z))
    innr.append((x, z))
P.append(poly_prism("reflector", outer + innr[::-1], -28.5, 28.5, METAL))
for z in (ZC - 11.0, ZC, ZC + 11.0):
    P.append(cyl("element", (1.0, 0, z), 1.6, 50.0, 'Y', GLOW, seg=14))
    for y in (-26.5, 26.5):
        P.append(box("holder", (-2.5, y - 1.5, z - 2.5), (2.5, y + 1.5, z + 2.5), CER, 0.5))

# ------------------------------------------------------------------ ön tel ızgara
P.append(box("frameL", (12.0, -29, 17), (14, -27.5, 60), METAL))
P.append(box("frameR", (12.0, 27.5, 17), (14, 29, 60), METAL))
for i in range(15):
    y = -24.5 + i * 3.5
    P.append(cyl("grilleWire", (13.0, y, 38.5), 0.35, 41.0, 'Z', METAL, seg=6, smooth=False))
for z in (18.5, 38.5, 58.0):
    P.append(cyl("grilleBar", (13.0, 0, z), 0.45, 56.0, 'Y', METAL, seg=6, smooth=False))

# ------------------------------------------------------------------ kulp, ayaklar, düğme, lamba
P.append(wire("handle", [(-6, -17, TOPZ), (-6, -16, TOPZ + 6), (-6, -10, TOPZ + 8), (-6, 10, TOPZ + 8), (-6, 16, TOPZ + 6), (-6, 17, TOPZ)], 1.1, BLACK))
for (x, y) in ((-10, -27), (10, -27), (-10, 27), (10, 27)):
    P.append(cyl("foot", (x, y, 3.0), 2.2, 6.0, 'Z', BLACK, seg=12, r2=1.6))
P.append(cyl("knob", (4, -33.3, 62), 3.4, 2.6, 'Y', BLACK, seg=18))
P.append(box("knobMark", (3.6, -34.8, 63.5), (4.4, -34.5, 65.6), METAL))
P.append(cyl("knob2", (4, -33.3, 50), 2.6, 2.2, 'Y', BLACK, seg=16))
P.append(cyl("lampRim", (14.2, 18.0, 12.5), 1.9, 0.8, 'X', METAL, seg=14))
P.append(sphere("lamp", (14.5, 18.0, 12.5), 1.3, GLOW, scale=(0.6, 1, 1), seg=12, rings=6))

# ------------------------------------------------------------------ fiş kablosu ve duvar prizi (arka duvar heater yerelinde X=-22)
P.append(wire("cord", [(-14, 22, 10), (-17, 23, 6), (-19, 26, 1.2), (-20, 32, 1.0), (-20.6, 38, 4), (-20.6, 41, 18), (-20.6, 41, 26)], 0.55, BLACK))
P.append(box("outlet", (-22, 35, 24), (-19.6, 47, 36), CER, 0.4))
P.append(box("plug", (-19.8, 38.5, 26.5), (-17.2, 43.5, 31.5), BLACK, 0.4))

heater = join("SM_NL_Heater", P, uv_size=0.5)
export(heater, OUT)

if os.environ.get("NL_PREVIEW") == "1":
    preview(OUT, "heater_preview.png", (120, -80, 70), (0, 0, 38),
            {PAINT: (0.82, 0.74, 0.58, 1), METAL: (0.7, 0.7, 0.72, 1), GLOW: (1.0, 0.35, 0.05, 1), BLACK: (0.03, 0.03, 0.03, 1),
             CER: (0.9, 0.88, 0.82, 1), INK: (0, 0, 0, 1)}, lens=35, res=(1100, 1000))
