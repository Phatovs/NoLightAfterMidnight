# No Light After Midnight - Eski tip dizel jeneratör (açık motor, palet üstünde, paslı mavi boya)
# Çalıştırma: blender.exe -b --factory-startup --python Scripts/blender_old_generator.py -- <çıktı klasörü>   (önizleme: NL_PREVIEW=1)
# Çıktı: SM_NL_OldGenerator.fbx
# Unreal yerel (cm): pivot taban ortası, uzun eksen X (-145..145), Y ±62, Z 0..~190.
#   -X ucunda radyatör, ortada 5 silindirli motor, +X'te alternatör. Kontrol kutusu ve yakıt deposu -Y (oyuncu) tarafında.
#   Durum lambası (yuva MI_NL_GenLamp, C++ rengini değiştirir): kontrol kutusunun ön yüzü (-50, -63, 80)
#   Benzin ağzı (C++ FuelInletOffset): yakıt deposunun kapağı (18, -58, 78) civarı
import math
import os
import random
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import bpy
from blender_nl_common import *  # noqa

reset()
OUT = out_dir("oldgen_out")
random.seed(9)
PAINT, WOOD, DARK, RED, METAL = "MI_NL_OldGenPaint", "MI_NL_Wood", "MI_NL_FuseCabinet", "MI_NL_JerryRed", "MI_NL_Metal"
P = []

# ------------------------------------------------------------------ palet
for y in (-48.0, 0.0, 48.0):
    P.append(box("runner", (-145, y - 6, 0), (145, y + 6, 9), WOOD, 0.6))
    for x in (-135.0, -45.0, 45.0, 135.0):
        P.append(box("block", (x - 8, y - 7, 0), (x + 8, y + 7, 10), WOOD, 0.8))
for i in range(12):
    x = -138 + i * 25.0
    P.append(box("plank", (x - 6.5, -60, 10), (x + 6.5, 60, 13), WOOD, 0.5, rot=(0, 0, random.uniform(-0.6, 0.6))))

# ------------------------------------------------------------------ çelik şase
for y in (-42.0, 42.0):
    P.append(box("rail", (-140, y - 5, 13), (140, y + 5, 26), PAINT, 0.8))
    for x in range(-125, 140, 30):
        P.append(cyl("railbolt", (x, y + (-5.6 if y < 0 else 5.6), 19.5), 1.1, 1.2, 'Y', PAINT, seg=6, smooth=False))
for x in (-110.0, -20.0, 70.0, 130.0):
    P.append(box("cross", (x - 4, -42, 15), (x + 4, 42, 24), PAINT, 0.5))

# ------------------------------------------------------------------ radyatör (-X ucu)
RX0, RX1 = -142.0, -124.0
P.append(box("radL", (RX0, -52, 26), (RX1, -44, 155), PAINT, 1.0))
P.append(box("radR", (RX0, 44, 26), (RX1, 52, 155), PAINT, 1.0))
P.append(box("radT", (RX0, -52, 147), (RX1, 52, 157), PAINT, 1.0))
P.append(box("radB", (RX0, -52, 26), (RX1, 52, 34), PAINT, 1.0))
P.append(box("radCore", (RX0 + 4, -44, 34), (RX1 - 2, 44, 147), DARK, 0.3))
for z in range(38, 146, 5):
    P.append(box("fin", (RX1 - 2.5, -44, z), (RX1 - 1.5, 44, z + 1.2), PAINT))
P.append(cyl("shroud", (RX1 + 3, 0, 92), 40, 6, 'X', PAINT, seg=32))
P.append(cyl("shroudIn", (RX1 + 5.5, 0, 92), 36, 2, 'X', DARK, seg=32))
P.append(torus("shroudRim", (RX1 + 6.2, 0, 92), 38, 1.6, PAINT, axis='X'))
P.append(cyl("fanHub", (RX1 + 10, 0, 92), 6, 6, 'X', METAL, seg=16))
for k in range(5):
    a = k * 72.0
    bl = box("blade%d" % k, (RX1 + 8, -3.5, 92), (RX1 + 9.5, 3.5, 92 + 30), DARK, 0.4)
    # Kanat göbek etrafında X ekseninde döner
    bl.rotation_euler = (math.radians(a), 0, 0)
    bl.location = to_bl(RX1 + 8.7, -math.sin(math.radians(a)) * 17, 92 + math.cos(math.radians(a)) * 17)
    P.append(bl)
# Yan kulak plakaları (resimdeki gibi öne taşan)
for y in (-56.0, 56.0):
    P.append(box("radEar", (RX0 - 2, y - 2, 28), (RX1 + 6, y + 2, 150), PAINT, 0.8))
    for z in (40.0, 70.0, 100.0, 130.0):
        P.append(cyl("earBolt", (RX0 + 2, y + (-2.4 if y < 0 else 2.4), z), 0.9, 1.0, 'Y', METAL, seg=6, smooth=False))
P.append(cyl("radCap", (RX0 + 9, 30, 160), 4.5, 6, 'Z', PAINT, seg=12))

# ------------------------------------------------------------------ motor bloğu
EX0, EX1 = -112.0, 38.0
P.append(box("oilpan", (EX0 + 10, -24, 26), (EX1 - 8, 24, 44), PAINT, 2.0))
P.append(box("block", (EX0, -30, 42), (EX1, 30, 106), PAINT, 2.5, segs=2))
P.append(box("blockTop", (EX0 + 2, -26, 104), (EX1 - 2, 26, 110), PAINT, 1.0))
# Kapak plakaları (-Y yüzünde, cıvatalı)
for cx in (-75.0, -30.0, 10.0):
    P.append(box("cover", (cx - 12, -32, 52), (cx + 12, -30, 76), PAINT, 1.5, segs=2))
    for dx in (-9, 9):
        for dz in (55, 73):
            P.append(cyl("cbolt", (cx + dx, -32.6, dz), 0.9, 1.0, 'Y', METAL, seg=6, smooth=False))
# 5 silindir kapağı
for i in range(5):
    cx = EX0 + 16 + i * 29.0
    P.append(box("head%d" % i, (cx - 12.5, -23, 109), (cx + 12.5, 23, 128), PAINT, 6.0, segs=3))
    P.append(box("headBase%d" % i, (cx - 13.5, -25, 108), (cx + 13.5, 25, 112), PAINT, 0.8))
    # Buji ve kablosu -> ateşleme kutusu
    P.append(cyl("plug%d" % i, (cx, -26, 116), 1.4, 5, 'Y', DARK, seg=8))
    P.append(wire("plugwire%d" % i, [(cx, -29, 116), (cx - 4, -36, 112), (-26 + i * 4.5, -44, 100)], 0.45, "MI_NL_Rubber"))
# Ateşleme / dağıtım kutusu
P.append(box("ignBox", (-40, -50, 84), (-6, -38, 100), METAL, 1.8, segs=2))
P.append(box("ignLid", (-38, -51.5, 86), (-8, -49.5, 98), METAL, 1.2, segs=2))
for i in range(5):
    P.append(cyl("ignTerm%d" % i, (-34 + i * 6, -44, 101.5), 1.3, 3, 'Z', DARK, seg=8))
# Egzoz manifoldu (+Y) ve yükselen boru
P.append(tube("exManifold", (EX0 + 10, 34, 112), (EX1 - 10, 34, 112), 4.5, PAINT))
for i in range(5):
    cx = EX0 + 16 + i * 29.0
    P.append(tube("exStub%d" % i, (cx, 24, 115), (cx, 34, 112), 3.2, PAINT))
P.append(wire("exRiser", [(EX0 + 10, 34, 112), (EX0 - 2, 30, 125), (EX0 - 4, 24, 150), (EX0 + 6, 18, 166)], 5.5, PAINT, res=4))
P.append(cyl("exFlange", (EX0 + 8, 17, 168), 9.5, 2.5, 'Z', PAINT, seg=20, rot=(math.radians(-10), math.radians(25), 0)))
# Hava filtresi / susturucu kulesi (üstte)
P.append(cyl("filterNeck", (-8, 4, 124), 6, 14, 'Z', PAINT, seg=16))
P.append(cyl("filterBody", (-8, 4, 152), 11, 36, 'Z', PAINT, seg=24))
for z in (138.0, 146.0, 154.0, 162.0):
    P.append(torus("filterRib", (-8, 4, z), 11.2, 1.2, PAINT, axis='Z'))
P.append(cyl("filterCap", (-8, 4, 172), 13, 4, 'Z', PAINT, seg=24, r2=9))
P.append(cyl("filterTop", (-8, 4, 176), 5, 4, 'Z', PAINT, seg=12))
# Radyatörden motora borular (-Y tarafı)
P.append(wire("hoseTop", [(RX1 + 2, -30, 140), (-110, -34, 136), (-98, -30, 118), (-90, -22, 112)], 4.0, PAINT, res=4))
P.append(wire("hoseLow", [(RX1 + 2, -34, 50), (-118, -38, 50), (EX0 - 2, -30, 60)], 4.0, PAINT, res=4))
for (x, y, z) in ((-116, -33, 137), (-117, -38, 50)):
    P.append(torus("clamp", (x, y, z), 4.6, 0.8, METAL, axis='X'))

# ------------------------------------------------------------------ volan, kavrama
P.append(cyl("bell", (48, 0, 82), 33, 18, 'X', PAINT, seg=32))
P.append(cyl("coupling", (64, 0, 82), 22, 14, 'X', PAINT, seg=28))
for x in (58.0, 63.0, 68.0):
    P.append(torus("groove", (x, 0, 82), 22.3, 1.2, DARK, axis='X'))
P.append(cyl("shaft", (40, -30, 70), 4, 14, 'X', METAL, seg=12))

# ------------------------------------------------------------------ alternatör
AX0, AX1, AR, AZ = 72.0, 136.0, 42.0, 84.0
P.append(box("altBase", (AX0 - 4, -46, 24), (AX1 + 4, 46, 44), PAINT, 1.5))
P.append(cyl("alt", ((AX0 + AX1) / 2, 0, AZ), AR, AX1 - AX0, 'X', PAINT, seg=40))
P.append(torus("altRing", (AX0 + 6, 0, AZ), AR + 0.5, 2.0, PAINT, axis='X'))
P.append(cyl("altEnd", (AX1 + 4, 0, AZ), AR - 3, 8, 'X', PAINT, seg=40))
P.append(cyl("altHub", (AX1 + 10, 0, AZ), 12, 5, 'X', PAINT, seg=24))
P.append(torus("altHubRing", (AX1 + 12.5, 0, AZ), 9, 2, PAINT, axis='X'))
P.append(cyl("altHubIn", (AX1 + 13, 0, AZ), 5, 2, 'X', DARK, seg=16))
for k in range(4):
    a = math.radians(45 + 90 * k)
    y, z = math.cos(a) * 26, AZ + math.sin(a) * 26
    v = box("altVent%d" % k, (AX1 + 7.5, y - 6, z - 6), (AX1 + 8.6, y + 6, z + 6), DARK, 2.0, segs=2, rot=(math.degrees(a), 0, 0))
    P.append(v)
P.append(box("termBox", (92, -10, AZ + AR - 4), (112, 10, AZ + AR + 8), PAINT, 1.5, segs=2))
P.append(cyl("termCap", (100, 18, AZ + AR + 2), 3, 10, 'Z', PAINT, seg=12))
# Izgara (alternatörün kavrama tarafı, -Y)
P.append(box("grilleFrame", (AX0 - 2, -48, 40), (AX0 + 10, -42, 125), PAINT, 1.0))
for z in range(46, 122, 5):
    P.append(box("grilleSlat", (AX0, -49, z), (AX0 + 8, -46, z + 2), DARK))
# Kırmızı kapak (alternatör tabanının önünde)
P.append(box("redPanel", (88, -47.5, 28), (128, -45.5, 74), RED, 1.5, segs=2))
P.append(box("redPanelRim", (86, -46.2, 26), (130, -44.6, 76), PAINT, 0.8))

# ------------------------------------------------------------------ kontrol kutusu (-Y, motorun önünde)
P.append(box("ctlLeg", (-92, -54, 13), (-86, -46, 40), PAINT, 0.5))
P.append(box("ctlLeg2", (-40, -54, 13), (-34, -46, 40), PAINT, 0.5))
P.append(box("ctlBox", (-98, -62, 38), (-28, -42, 98), PAINT, 1.8, segs=2))
P.append(box("ctlLid", (-96, -63.2, 40), (-30, -61.8, 96), PAINT, 1.2, segs=2))
for (x, z) in ((-93, 44), (-33, 44), (-93, 92), (-33, 92)):
    P.append(cyl("ctlScrew", (x, -63.6, z), 0.9, 0.8, 'Y', METAL, seg=6, smooth=False))
P.append(cyl("btnBlack", (-86, -65, 80), 2.6, 3.0, 'Y', DARK, seg=16))
P.append(cyl("btnRed1", (-72, -65, 80), 2.8, 3.0, 'Y', RED, seg=16))
P.append(cyl("lampBase", (-50, -63.8, 80), 3.6, 1.6, 'Y', DARK, seg=16))
P.append(cyl("lamp", (-50, -65.3, 80), 2.8, 2.2, 'Y', "MI_NL_GenLamp", seg=16))
P.append(box("switchPlate", (-64, -64, 76), (-56, -62.6, 86), METAL, 0.4))
P.append(box("switchLever", (-61, -69, 80), (-59, -63.5, 82.5), DARK, 0.4, rot=(0, 0, 0)))
P.append(box("gaugeSq", (-80, -64.2, 52), (-62, -62.6, 70), DARK, 1.5, segs=2))
P.append(cyl("gaugeFace", (-71, -64.6, 61), 6, 0.6, 'Y', "MI_NL_GaugeFace", seg=24))
P.append(box("namePlate", (-56, -64, 54), (-38, -62.8, 64), METAL, 0.5))
P.append(text("nameText", "TYPE  D-5", (-47, -64.2, 59.5), 1.8, "MI_NL_Ink", face='-Y'))
# Kutudan motora kablo kanalı
P.append(wire("ctlCable", [(-62, -45, 98), (-58, -40, 104), (-44, -34, 100)], 1.2, "MI_NL_Rubber"))

# ------------------------------------------------------------------ yakıt deposu (-Y, kontrol kutusu ile alternatör arasında)
P.append(cyl("tank", (5, -50, 52), 14, 50, 'X', PAINT, seg=28))
for x in (-16.0, 26.0):
    P.append(torus("tankStrap", (x, -50, 52), 14.4, 1.0, DARK, axis='X'))
P.append(cyl("tankNeck", (18, -50, 68), 3.6, 6, 'Z', METAL, seg=12))
P.append(cyl("tankCap", (18, -50, 72), 5.0, 2.6, 'Z', RED, seg=16))
P.append(wire("fuelLine", [(-12, -42, 46), (-20, -34, 44), (-40, -31, 52)], 0.8, "MI_NL_Rubber"))

gen = join("SM_NL_OldGenerator", P, uv_size=0.7)
export(gen, OUT)

if os.environ.get("NL_PREVIEW") == "1":
    preview(OUT, "oldgen_preview.png", (330, -380, 210), (0, 0, 85),
            {PAINT: (0.22, 0.45, 0.55, 1), WOOD: (0.45, 0.3, 0.15, 1), DARK: (0.05, 0.05, 0.05, 1), RED: (0.55, 0.08, 0.05, 1),
             METAL: (0.55, 0.55, 0.55, 1), "MI_NL_Rubber": (0.03, 0.03, 0.03, 1), "MI_NL_GenLamp": (0.2, 1.0, 0.3, 1),
             "MI_NL_GaugeFace": (0.9, 0.9, 0.85, 1), "MI_NL_Ink": (0, 0, 0, 1)}, lens=32, res=(1400, 850))
