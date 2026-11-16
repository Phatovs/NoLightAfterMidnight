# No Light After Midnight - çevre dokuları (kodla üretilir, döşenebilir) -> /Game/Environment/Textures
# Kütük kabuğu, kütük ucu (yıllık halkalar), tahta damarı, döşeme tahtası, orman zemini, çamur, kaya, ıslak kum,
# çam kabuğu, çam iğnesi, sıva, paslı metal, halat, deniz normali. Her biri için renk (C) ve normal (N).
# Çalıştırma: UnrealEditor-Cmd.exe NoLightAfterMidnight.uproject -run=pythonscript -script="<tam yol>\Scripts\nl_env_textures.py"
# Sadece bazılarını üretmek için: NL_TEX=LogBark,Rock
import math
import os
import random
import struct
import zlib

import unreal

S = 512
OUT = os.path.join(unreal.Paths.project_saved_dir(), "NLTextures")
DEST = "/Game/Environment/Textures"
os.makedirs(OUT, exist_ok=True)


# ------------------------------------------------------------------ gürültü

def vnoise(cx, cy, seed, size=S):
    """Döşenebilir değer gürültüsü (0..1); cx, cy hücre sayısı (yönlü: biri küçükse o yönde uzar)."""
    r = random.Random(seed)
    g = [r.random() for _ in range(cx * cy)]
    xs = []
    for x in range(size):
        fx = x * cx / size
        i = int(fx)
        t = fx - i
        xs.append((i % cx, (i + 1) % cx, t * t * (3 - 2 * t)))
    out = [0.0] * (size * size)
    for y in range(size):
        fy = y * cy / size
        j = int(fy)
        u = fy - j
        u = u * u * (3 - 2 * u)
        r0 = (j % cy) * cx
        r1 = ((j + 1) % cy) * cx
        base = y * size
        for x, (i0, i1, t) in enumerate(xs):
            a = g[r0 + i0]
            b = g[r0 + i1]
            top = a + (b - a) * t
            c = g[r1 + i0]
            bot = c + (g[r1 + i1] - c) * t
            out[base + x] = top + (bot - top) * u
    return out


def fbm(cx, cy, octaves, seed, size=S, gain=0.5):
    total = [0.0] * (size * size)
    amp, norm = 1.0, 0.0
    for o in range(octaves):
        n = vnoise(cx << o, cy << o, seed + o * 101, size)
        total = [t + v * amp for t, v in zip(total, n)]
        norm += amp
        amp *= gain
    return [t / norm for t in total]


def smooth(e0, e1, x):
    t = min(1.0, max(0.0, (x - e0) / (e1 - e0)))
    return t * t * (3 - 2 * t)


def lerp3(a, b, t):
    return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t)


# ------------------------------------------------------------------ yazma

def _png(path, size, rows_bytes, channels):
    raw = b"".join(b"\x00" + r for r in rows_bytes)
    def chunk(tag, data):
        c = struct.pack(">I", len(data)) + tag + data
        return c + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    color_type = 2 if channels == 3 else 6
    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, color_type, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b"")
    with open(path, "wb") as f:
        f.write(png)


def write_color(name, cols, size=S):
    # cols: lineer renk listesi -> sRGB
    lut = [min(255, int(255 * (i / 4095.0) ** (1 / 2.2) + 0.5)) for i in range(4096)]
    rows = []
    for y in range(size):
        row = bytearray()
        for (r, g, b) in cols[y * size:(y + 1) * size]:
            row += bytes((lut[min(4095, max(0, int(r * 4095)))], lut[min(4095, max(0, int(g * 4095)))], lut[min(4095, max(0, int(b * 4095)))]))
        rows.append(bytes(row))
    path = os.path.join(OUT, name + ".png")
    _png(path, size, rows, 3)
    return path


def write_normal(name, h, strength, size=S):
    rows = []
    for y in range(size):
        row = bytearray()
        ym = ((y - 1) % size) * size
        yp = ((y + 1) % size) * size
        yc = y * size
        for x in range(size):
            dx = (h[yc + (x - 1) % size] - h[yc + (x + 1) % size]) * strength
            dy = (h[ym + x] - h[yp + x]) * strength
            l = math.sqrt(dx * dx + dy * dy + 1.0)
            row += bytes((int((dx / l * 0.5 + 0.5) * 255), int((dy / l * 0.5 + 0.5) * 255), int((1.0 / l * 0.5 + 0.5) * 255)))
        rows.append(bytes(row))
    path = os.path.join(OUT, name + ".png")
    _png(path, size, rows, 3)
    return path


# ------------------------------------------------------------------ dokular (damar U yönünde = yatay)

def tex_logbark():
    # Hava almış, ıslak kütük: ince uzunlamasına damar, birkaç uzun ince çatlak, grileşmiş lekeler
    grain = fbm(2, 56, 4, 11)
    fine = vnoise(3, 220, 12)
    crack1 = vnoise(2, 16, 13)
    crack2 = vnoise(3, 28, 15)
    blot = fbm(3, 3, 4, 14)
    dark, mid, gray = (0.035, 0.024, 0.017), (0.10, 0.068, 0.045), (0.085, 0.08, 0.072)
    cols, h = [], []
    for g, f, c1, c2, b in zip(grain, fine, crack1, crack2, blot):
        cr = max(1.0 - smooth(0.0, 0.009, abs(c1 - 0.5)), 0.6 * (1.0 - smooth(0.0, 0.006, abs(c2 - 0.5))))
        v = g * 0.6 + f * 0.4
        col = lerp3(dark, mid, v)
        col = lerp3(col, gray, smooth(0.45, 0.75, b) * 0.6)
        col = lerp3(col, (0.012, 0.009, 0.007), cr * 0.8)
        cols.append(col)
        h.append(v * 0.6 - cr * 0.7 + b * 0.2)
    return cols, h, 6.0


def tex_logend():
    # Kütük ucu: yıllık halkalar, ortadan dışa çatlaklar, dışta koyu kabuk halkası
    wob = fbm(4, 4, 3, 21)
    cols, h = [], []
    for y in range(S):
        for x in range(S):
            dx, dy = (x - S / 2) / (S / 2), (y - S / 2) / (S / 2)
            r = math.sqrt(dx * dx + dy * dy)
            w = wob[y * S + x]
            ring = 0.5 + 0.5 * math.sin((r + w * 0.08) * 70.0)
            ang = math.atan2(dy, dx)
            crack = 1.0 - smooth(0.0, 0.03, abs(math.sin(ang * 3 + w * 2.0))) if r < 0.85 else 0.0
            col = lerp3((0.16, 0.11, 0.07), (0.24, 0.17, 0.10), ring)
            col = lerp3(col, (0.06, 0.045, 0.035), smooth(0.2, 0.9, r) * 0.6)
            col = lerp3(col, (0.02, 0.015, 0.012), smooth(0.86, 0.93, r))
            col = lerp3(col, (0.02, 0.015, 0.01), crack * 0.9 * smooth(0.1, 0.4, r))
            cols.append(col)
            h.append(ring * 0.3 - crack)
    return cols, h, 4.0


def tex_grain():
    # Düz tahta damarı (kapı, kepenk, çerçeve, sandık, varil)
    grain = fbm(3, 48, 4, 31)
    fine = vnoise(6, 160, 32)
    blot = fbm(2, 2, 3, 33)
    cols, h = [], []
    for g, f, b in zip(grain, fine, blot):
        v = g * 0.75 + f * 0.25
        col = lerp3((0.05, 0.034, 0.022), (0.14, 0.095, 0.06), v)
        col = lerp3(col, (0.09, 0.085, 0.078), smooth(0.5, 0.8, b) * 0.5)
        cols.append(col)
        h.append(v)
    return cols, h, 3.0


def tex_planks():
    # Döşeme: 4 tahta / döşeme, aralarda koyu boşluk, uç birleşimleri, çiviler
    grain = fbm(3, 48, 4, 41)
    blot = fbm(3, 3, 3, 42)
    r = random.Random(43)
    boards = []
    for i in range(4):
        joint = r.randint(40, S - 40)
        boards.append((r.uniform(0.75, 1.15), joint, r.uniform(0, 1)))
    cols, h = [], []
    bh = S // 4
    for y in range(S):
        bi = y // bh
        yy = y % bh
        tint, joint, _ = boards[bi]
        for x in range(S):
            g = grain[y * S + ((x + bi * 97) % S)]
            gap = 1.0 if (yy < 3 or abs(x - joint) < 2) else 0.0
            col = lerp3((0.04, 0.028, 0.019), (0.12, 0.08, 0.05), g)
            col = (col[0] * tint, col[1] * tint, col[2] * tint)
            col = lerp3(col, (0.08, 0.078, 0.072), smooth(0.55, 0.85, blot[y * S + x]) * 0.45)
            nail = 0.0
            for nx in (joint - 8, joint + 8):
                if (x - nx) ** 2 + (yy - bh // 2) ** 2 < 9:
                    nail = 1.0
            col = lerp3(col, (0.03, 0.03, 0.032), nail)
            col = lerp3(col, (0.006, 0.005, 0.004), gap)
            edge = smooth(0, 6, yy) * smooth(0, 6, bh - yy)
            cols.append(col)
            h.append(g * 0.4 + edge * 0.6 - gap)
    return cols, h, 5.0


def tex_ground():
    # Orman zemini: koyu toprak, ölü ot, çam iğnesi, ufak taşlar
    big = fbm(4, 4, 5, 51)
    mid = fbm(16, 16, 3, 52)
    speck = vnoise(160, 160, 53)
    straw = vnoise(64, 256, 54)
    soil, grass, needle = (0.028, 0.022, 0.016), (0.05, 0.052, 0.026), (0.07, 0.042, 0.024)
    cols, h = [], []
    for b, m, s, st in zip(big, mid, speck, straw):
        col = lerp3(soil, grass, smooth(0.35, 0.65, b))
        col = lerp3(col, needle, smooth(0.55, 0.8, m) * 0.7)
        col = lerp3(col, (0.07, 0.065, 0.045), smooth(0.7, 0.9, st) * 0.4)
        stone = smooth(0.88, 0.95, s)
        col = lerp3(col, (0.09, 0.088, 0.085), stone)
        cols.append(col)
        h.append(b * 0.3 + m * 0.4 + stone * 0.6 + st * 0.2)
    return cols, h, 4.0


def tex_mud():
    # Patika: ıslak çamur, çakıl, ara ara su birikintisi gibi koyu düz yerler
    big = fbm(4, 4, 4, 61)
    mid = fbm(20, 20, 3, 62)
    grav = vnoise(120, 120, 63)
    cols, h = [], []
    for b, m, g in zip(big, mid, grav):
        col = lerp3((0.02, 0.016, 0.012), (0.055, 0.045, 0.034), m)
        puddle = smooth(0.6, 0.7, b)
        col = lerp3(col, (0.012, 0.011, 0.01), puddle * 0.7)
        stone = smooth(0.82, 0.9, g) * (1 - puddle)
        col = lerp3(col, (0.075, 0.07, 0.065), stone)
        cols.append(col)
        h.append(m * 0.5 + stone * 0.7 - puddle * 0.4)
    return cols, h, 4.0


def tex_rock():
    # Gri kaya: büyük lekeler, çatlaklar, yosun/liken
    big = fbm(4, 4, 5, 71)
    mid = fbm(12, 12, 4, 72)
    crk = vnoise(10, 10, 73)
    lich = fbm(8, 8, 3, 74)
    cols, h = [], []
    for b, m, c, l in zip(big, mid, crk, lich):
        cr = 1.0 - smooth(0.0, 0.02, abs(c - 0.5))
        col = lerp3((0.05, 0.05, 0.05), (0.16, 0.155, 0.145), b * 0.6 + m * 0.4)
        col = lerp3(col, (0.06, 0.07, 0.035), smooth(0.65, 0.8, l) * 0.6)
        col = lerp3(col, (0.015, 0.015, 0.015), cr * 0.8)
        cols.append(col)
        h.append(b * 0.5 + m * 0.5 - cr * 0.8)
    return cols, h, 6.0


def tex_sand():
    # Kıyı: ıslak koyu kum ve çakıl
    mid = fbm(16, 16, 4, 81)
    grav = vnoise(200, 200, 82)
    cols, h = [], []
    for m, g in zip(mid, grav):
        col = lerp3((0.05, 0.045, 0.036), (0.1, 0.09, 0.072), m)
        st = smooth(0.8, 0.88, g)
        col = lerp3(col, (0.04, 0.04, 0.042), st)
        cols.append(col)
        h.append(m * 0.4 + st * 0.6)
    return cols, h, 3.0


def tex_pinebark():
    # Çam kabuğu: dikey plakalar (V = yükseklik)
    plates = vnoise(10, 3, 91)
    grain = fbm(24, 6, 3, 92)
    cols, h = [], []
    for p, g in zip(plates, grain):
        groove = 1.0 - smooth(0.0, 0.08, abs(p - 0.5))
        col = lerp3((0.05, 0.032, 0.022), (0.11, 0.07, 0.045), g)
        col = lerp3(col, (0.012, 0.008, 0.006), groove * 0.9)
        cols.append(col)
        h.append(g * 0.4 - groove)
    return cols, h, 6.0


def tex_needles():
    # Çam iğneleri: koyu yeşil ince çizgiler, kahverengi kurumuş yerler
    streak = vnoise(180, 12, 101)
    streak2 = vnoise(12, 180, 102)
    blot = fbm(4, 4, 3, 103)
    cols, h = [], []
    for a, b, c in zip(streak, streak2, blot):
        v = max(a, b)
        col = lerp3((0.006, 0.014, 0.008), (0.03, 0.055, 0.025), v)
        col = lerp3(col, (0.05, 0.035, 0.018), smooth(0.65, 0.85, c) * 0.5)
        cols.append(col)
        h.append(v)
    return cols, h, 5.0


def tex_plaster():
    # Eski badana/sıva: kirli beyaz, aşağı akan lekeler, çatlak
    big = fbm(4, 4, 4, 111)
    drip = fbm(32, 3, 3, 112)
    crk = vnoise(3, 9, 113)
    crm = fbm(3, 3, 2, 114)
    cols, h = [], []
    for b, d, c, cm in zip(big, drip, crk, crm):
        cr = (1.0 - smooth(0.0, 0.004, abs(c - 0.5))) * smooth(0.6, 0.75, cm) * 0.6
        col = lerp3((0.32, 0.31, 0.28), (0.45, 0.44, 0.40), b)
        col = lerp3(col, (0.12, 0.11, 0.09), smooth(0.55, 0.8, d) * 0.6)
        col = lerp3(col, (0.06, 0.06, 0.055), cr * 0.7)
        cols.append(col)
        h.append(b * 0.3 - cr * 0.6)
    return cols, h, 3.0


def tex_metal():
    # Koyu boyalı, paslanmış metal
    big = fbm(4, 4, 5, 121)
    rust = fbm(8, 8, 4, 122)
    streak = fbm(40, 3, 2, 123)
    cols, h = [], []
    for b, r, s in zip(big, rust, streak):
        col = lerp3((0.025, 0.027, 0.03), (0.06, 0.062, 0.066), b)
        rr = smooth(0.55, 0.75, r * 0.7 + s * 0.3)
        col = lerp3(col, (0.12, 0.05, 0.02), rr)
        cols.append(col)
        h.append(rr * 0.5 + b * 0.2)
    return cols, h, 3.0


def tex_rope():
    # Halat: çapraz bükümler
    fine = vnoise(64, 64, 131)
    cols, h = [], []
    for y in range(S):
        for x in range(S):
            t = 0.5 + 0.5 * math.sin((x + y) * 2 * math.pi * 16 / S)
            v = t * 0.8 + fine[y * S + x] * 0.2
            cols.append(lerp3((0.06, 0.045, 0.025), (0.17, 0.13, 0.08), v))
            h.append(t)
    return cols, h, 5.0


def tex_water():
    # Deniz normali: farklı yönlerde döşenebilir dalgalar + gürültü (sadece normal)
    n = fbm(8, 8, 3, 141)
    waves = [(3, 1, 1.0), (-2, 3, 0.7), (5, -2, 0.45), (1, 7, 0.3), (-7, -4, 0.2), (11, 5, 0.12)]
    h = []
    for y in range(S):
        for x in range(S):
            v = 0.0
            for (kx, ky, a) in waves:
                v += a * math.sin(2 * math.pi * (kx * x + ky * y) / S)
            h.append(v * 0.25 + n[y * S + x] * 0.35)
    return None, h, 7.0


TEXTURES = {
    "LogBark": tex_logbark, "LogEnd": tex_logend, "Grain": tex_grain, "Planks": tex_planks,
    "Ground": tex_ground, "Mud": tex_mud, "Rock": tex_rock, "Sand": tex_sand,
    "PineBark": tex_pinebark, "Needles": tex_needles, "Plaster": tex_plaster, "Metal": tex_metal,
    "Rope": tex_rope, "Water": tex_water,
}
_only = [s.strip() for s in os.environ.get("NL_TEX", "").split(",") if s.strip()]
if _only:
    TEXTURES = {k: v for k, v in TEXTURES.items() if k in _only}

files = []
for name, fn in TEXTURES.items():
    cols, h, strength = fn()
    if cols is not None:
        files.append(("T_NL_%s_C" % name, write_color("T_NL_%s_C" % name, cols), False))
    files.append(("T_NL_%s_N" % name, write_normal("T_NL_%s_N" % name, h, strength), True))
    unreal.log("NLTex: %s" % name)

tasks = []
for asset, path, _ in files:
    t = unreal.AssetImportTask()
    t.filename = path
    t.destination_path = DEST
    t.destination_name = asset
    t.automated = True
    t.replace_existing = True
    t.save = False
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

log = []
for asset, _, is_normal in files:
    tex = unreal.load_asset(DEST + "/" + asset)
    if not tex:
        log.append("%s: MISSING" % asset)
        continue
    if is_normal:
        tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD_NORMAL_MAP)
    unreal.EditorAssetLibrary.save_loaded_asset(tex)
    log.append("%s: OK" % asset)
with open(os.path.join(unreal.Paths.project_saved_dir(), "NLTextures.log"), "w", encoding="utf-8") as f:
    f.write("\n".join(log))
