# No Light After Midnight - Lvl_Ada'nın "gerçek ada" kopyası: /Game/Levels/Lvl_Ada_Art
# Lvl_Ada (blockout + oynanış) dokunulmadan kalır. Kopyada:
#  - Rıhtım kulübesi: modüler kütük kulübe (ANLLogCabin), ışıklı pencere, kapıda fener, soba borusu
#  - Rıhtım: tahta iskele (ANLPlankDock), kazıklar, babalar, halat, uçta fenerler
#  - Yıkık ev: yıkık kütük kulübe (yarı çatı, yıkık duvar, yerde kütükler)
#  - Orman blokları -> çam ağaçları (her koni bir çam), kıyıya kayalar, ara ara kesik kütük
#  - Arazi: orman zemini / patika çamuru / eğimde kaya / kıyıda ıslak kum; deniz: koyu parlak dalgalı
#  - Deniz feneri, jeneratör, yakıt alanı, direkler: dokulu materyaller
#  - Daha karanlık ve sisli gece (fenerle görülür); bulut katmanı kaldırıldı (performans)
# Oynanış aktörlerine (kapı, sigorta kutusu, jeneratör, kameralar, Runner, Drowned...) dokunulmaz.
# Tekrar çalıştırılabilir: önceki NL_ENV_* aktörlerini silip yeniden kurar. Baştan kopyalamak için NL_ART_RECOPY=1
# Çalıştırma (editör kapalıyken): UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<tam yol>\Scripts\nl_dress_ada.py"
import math
import os
import random

import unreal

SRC = "/Game/Levels/Lvl_Ada"
MAP = "/Game/Levels/Lvl_Ada_Art"
MATS = "/Game/Environment/Materials/"
LOG = []
V = unreal.Vector
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
eal = unreal.EditorAssetLibrary

# Gece / sis ayarları (Lvl_Ada'dan daha karanlık ve sisli)
ATM = {
    "moon_lux": 0.17, "moon_color": (140, 158, 205),
    "sky_intensity": 0.075,
    "fog_density": 0.13, "fog_falloff": 0.07, "fog_color": (0.010, 0.012, 0.016),
    "fog2_density": 0.12, "fog2_falloff": 0.5, "fog2_offset": 0.0,      # yere yakın ince sis katmanı
    "vol_fog_extinction": 1.9, "vol_fog_distance": 5000.0,
    "exposure_bias": -0.1,
}


def log(m):
    LOG.append(m)
    unreal.log("[NLArt] " + m)


def rot(pitch=0.0, yaw=0.0, roll=0.0):
    r = unreal.Rotator()
    r.pitch, r.yaw, r.roll = pitch, yaw, roll
    return r


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def by_label(label):
    for a in eas.get_all_level_actors():
        if a.get_actor_label() == label:
            return a
    return None


def labels_starting(prefix):
    return [a for a in eas.get_all_level_actors() if a.get_actor_label().startswith(prefix)]


def mat(name):
    m = unreal.load_asset(MATS + name)
    if not m:
        log("UYARI: materyal yok " + name)
    return m


def trace(start, end, ignore=None):
    hit = unreal.SystemLibrary.line_trace_single(world(), start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True,
                                                 ignore or [], unreal.DrawDebugTrace.NONE, True)
    if not hit:
        return None
    t = hit.to_tuple()
    return (t[5], t[9]) if t[0] else None


def ground(x, y, ignore=None):
    h = trace(V(x, y, 4000), V(x, y, -2000), ignore)
    return (h[0].z, h[1]) if h else (None, None)


def hide_visual(a):
    # Görünmez yap ama çarpışma kalsın (yürünen zemin)
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        c.set_editor_property("visible", False)
        c.set_editor_property("hidden_in_game", True)


def destroy(a):
    if a:
        eas.destroy_actor(a)


def remap_materials(a, mapping):
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        mats = c.get_materials()
        for i, m in enumerate(mats):
            name = m.get_name() if m else ""
            if name in mapping and mapping[name]:
                c.set_material(i, mat(mapping[name]))


def local_to_world(origin, yaw_deg, x, y, z=0.0):
    c, s = math.cos(math.radians(yaw_deg)), math.sin(math.radians(yaw_deg))
    return V(origin.x + x * c - y * s, origin.y + x * s + y * c, origin.z + z)


def opening(wall, offset, width, height, sill=0.0, window=False, lit=True, shutters=True, door_panel=True, lantern=1, frame=True):
    o = unreal.NLCabinOpening()
    o.set_editor_property("wall", wall)
    o.set_editor_property("offset", offset)
    o.set_editor_property("width", width)
    o.set_editor_property("height", height)
    o.set_editor_property("sill", sill)
    o.set_editor_property("window", window)
    o.set_editor_property("lit", lit)
    o.set_editor_property("shutters", shutters)
    o.set_editor_property("door_panel", door_panel)
    o.set_editor_property("lantern_side", lantern)
    o.set_editor_property("frame", frame)
    return o


W = unreal.NLCabinWall

# ------------------------------------------------------------------ kopya

if not eal.does_asset_exist(MAP) or os.environ.get("NL_ART_RECOPY") == "1":
    # Kaynağı yükle, hedef isimle kaydet (varsa üzerine yazar)
    src_world = unreal.EditorLoadingAndSavingUtils.load_map(SRC)
    ok = unreal.EditorLoadingAndSavingUtils.save_map(src_world, MAP)
    log("Kopyalandi: %s -> %s (%s)" % (SRC, MAP, ok))
unreal.EditorLoadingAndSavingUtils.load_map(MAP)
log("Yuklendi: %s, %d aktor" % (MAP, len(eas.get_all_level_actors())))

for a in labels_starting("NL_ENV_"):
    destroy(a)

# ------------------------------------------------------------------ rıhtım kulübesi

floor = by_label("RIHTIM_KULUBESI_TABAN")
if floor:
    ft = floor.get_actor_transform()
    fs = ft.scale3d
    yaw = ft.rotation.rotator().yaw
    top = ft.translation.z + 50.0 * fs.z
    cab = eas.spawn_actor_from_class(unreal.NLLogCabin, V(ft.translation.x, ft.translation.y, top), rot(yaw=yaw))
    cab.set_actor_label("NL_ENV_DockCabin")
    cab.set_editor_property("width", 100.0 * fs.x)
    cab.set_editor_property("depth", 100.0 * fs.y)
    cab.set_editor_property("wall_height", 278.0)
    cab.set_editor_property("roof_pitch", 40.0)
    cab.set_editor_property("chimney_pos", unreal.Vector2D(-0.45, 0.45))
    # Kapı: oyundaki NLHideDoor buraya oturur (kanat ve kasa onun)
    cab.set_editor_property("openings", [
        opening(W.FRONT, 0.0, 122.0, 222.0, door_panel=False, lantern=1, frame=False),
        opening(W.FRONT, -138.0, 66.0, 76.0, 104.0, window=True, lit=True),
        opening(W.LEFT, -95.0, 66.0, 76.0, 104.0, window=True, lit=True),
        opening(W.RIGHT, -95.0, 60.0, 70.0, 110.0, window=True, lit=False),
    ])
    cab.rebuild_now()
    hide_visual(floor)
    for suffix in ("ARKA_DUVAR", "CATI", "KAPI_LENTOSU", "ON_DUVAR", "ON_DUVAR2", "YAN_DUVAR", "YAN_DUVAR2"):
        destroy(by_label("RIHTIM_KULUBESI_" + suffix))
    for suffix in ("RAF", "RAF2", "MALZEME", "MALZEME2", "MALZEME3"):
        a = by_label("RIHTIM_KULUBESI_" + suffix)
        if a:
            remap_materials(a, {"MAT_Koyu_Gri": "MI_NL_World_Planks", "MAT_Acik_Gri": "MI_NL_World_Wood", "MAT_Yapi_Gri": "MI_NL_World_Wood"})
    log("Rihtim kulubesi: %.0f x %.0f, yaw %.0f" % (100.0 * fs.x, 100.0 * fs.y, yaw))

# ------------------------------------------------------------------ iskele

dock = by_label("RIHTIM")
dock_line = None
if dock:
    dt = dock.get_actor_transform()
    ds = dt.scale3d
    yaw = dt.rotation.rotator().yaw
    top = dt.translation.z + 50.0 * ds.z
    pd = eas.spawn_actor_from_class(unreal.NLPlankDock, V(dt.translation.x, dt.translation.y, top), rot(yaw=yaw))
    pd.set_actor_label("NL_ENV_Dock")
    pd.set_editor_property("length", 100.0 * ds.x)
    pd.set_editor_property("width", 100.0 * ds.y)
    pd.rebuild_now()
    hide_visual(dock)
    for a in labels_starting("RIHTIM_AYAGI"):
        destroy(a)
    dock_line = (dt.translation, yaw, 50.0 * ds.x)
    log("Iskele: %.0f x %.0f" % (100.0 * ds.x, 100.0 * ds.y))

# ------------------------------------------------------------------ yıkık ev

rfloor = by_label("YIKIK_EV_TABAN")
if rfloor:
    t = rfloor.get_actor_transform()
    s = t.scale3d
    top = t.translation.z + 50.0 * s.z
    ruin = eas.spawn_actor_from_class(unreal.NLLogCabin, V(t.translation.x, t.translation.y, top), rot(yaw=0.0))
    ruin.set_actor_label("NL_ENV_RuinedCabin")
    ruin.set_editor_property("width", 100.0 * s.x)
    ruin.set_editor_property("depth", 520.0)
    ruin.set_editor_property("wall_height", 235.0)
    ruin.set_editor_property("ruined", True)
    ruin.set_editor_property("ruin_wall_heights", [0.0, 105.0, 0.0, 0.0])
    ruin.set_editor_property("roof_coverage", 0.55)
    ruin.set_editor_property("chimney", False)
    ruin.set_editor_property("porch_props", False)
    ruin.set_editor_property("interior_lamp", False)
    ruin.set_editor_property("seed", 21)
    ruin.set_editor_property("openings", [
        opening(W.BACK, 0.0, 150.0, 215.0, door_panel=False, lantern=0),
        opening(W.BACK, -215.0, 80.0, 80.0, 95.0, window=True, lit=False, shutters=True),
        opening(W.FRONT, 160.0, 80.0, 80.0, 95.0, window=True, lit=False, shutters=False),
        opening(W.LEFT, 40.0, 80.0, 80.0, 95.0, window=True, lit=False, shutters=True),
    ])
    ruin.rebuild_now()
    hide_visual(rfloor)
    for a in labels_starting("YIKIK_EV_"):
        if a.get_actor_label() != "YIKIK_EV_TABAN":
            destroy(a)
    log("Yikik ev: %.0f x 520" % (100.0 * s.x))

# ------------------------------------------------------------------ materyaller: fener, jeneratör, yakıt, direkler

LIGHTHOUSE = {"MAT_Yapi_Gri": "MI_NL_World_Plaster", "MAT_Acik_Gri": "MI_NL_World_Concrete", "MAT_Koyu_Gri": "MI_NL_World_Metal"}
for a in eas.get_all_level_actors():
    l = a.get_actor_label()
    if l == "DENIZ_FENERI" or l.startswith("FENER_"):
        remap_materials(a, LIGHTHOUSE)
    elif l == "JENERATOR_ANA":
        remap_materials(a, {"MAT_Yapi_Gri": "MI_NL_World_Wood", "MAT_Acik_Gri": "MI_NL_World_Concrete", "MAT_Koyu_Gri": "MI_NL_World_Metal"})
    elif l.startswith("YAKIT_TANKI") or l.startswith("YAKIT_BIDONU"):
        remap_materials(a, {"MAT_Yapi_Gri": "MI_NL_World_Metal", "MAT_Koyu_Gri": "MI_NL_World_Metal"})
    elif l == "YAKIT_ALANI":
        remap_materials(a, {"MAT_Koyu_Gri": "MI_NL_World_Concrete"})
    elif l.startswith("DIREK_"):
        remap_materials(a, {"MAT_Koyu_Gri": "MI_NL_World_Wood"})

land = by_label("ADA_ARAZISI")
if land:
    land.set_editor_property("landscape_material", mat("M_NL_Landscape"))
    # Her arazi parçasında eski materyal "override" olarak duruyor: onları da değiştir
    for lc in land.get_components_by_class(unreal.LandscapeComponent):
        lc.set_editor_property("override_material", mat("M_NL_Landscape"))
    log("Arazi materyali: %d parca" % len(land.get_components_by_class(unreal.LandscapeComponent)))
sea = by_label("DENIZ")
if sea:
    for c in sea.get_components_by_class(unreal.StaticMeshComponent):
        c.set_material(0, mat("M_NL_Sea"))

# ------------------------------------------------------------------ yeni deniz feneri (blockout kule yerine) + toz

def add_dust(label, center, extent, yaw=0.0, density=45.0, seed=1):
    d = eas.spawn_actor_from_class(unreal.NLDustVolume, center, rot(yaw=yaw))
    d.set_actor_label(label)
    d.set_editor_property("extent", extent)
    d.set_editor_property("density", density)
    d.set_editor_property("seed", seed)
    d.rebuild_now()
    return d


lh = by_label("NL_LH_Lighthouse")
if lh:
    lamp_z = lh.get_actor_location().z
    lx, ly = lh.get_actor_location().x, lh.get_actor_location().y
    for l in ("FENER_KONTROL_BINASI", "DENIZ_FENERI", "FENER_GOZLEM_KATI", "FENER_TEKNIK_KAT", "FENER_MERCEK_KATI", "FENER_MERCEGI", "FENER_DONER_MERDIVEN",
              "FENER_YASAM_ALANI", "Lamba_FenerZemin", "NL_DUST_LivingArea"):
        destroy(by_label(l))
    # Fener bekçisi kulübesi (eski kontrol binasının yerine): masa, monitör, şarj istasyonu içinde kalır.
    # Doğu duvarında kapı boşluğu; deniz feneri bu duvara yaslanır, kapısı kulübeye açılır (iki döşeme aynı hizada).
    desk = next((a for a in eas.get_all_level_actors() if a.get_class().get_name() == "NLSecurityDesk"), None)
    floor_z = desk.get_actor_location().z if desk else 502.0
    kc = V(282.0, -2012.0, floor_z + 4.0)   # döşeme arazinin en yüksek noktasının üstünde kalsın (titreme)
    kw, kd = 540.0, 420.0
    keeper = eas.spawn_actor_from_class(unreal.NLLogCabin, kc, rot())
    keeper.set_actor_label("NL_ENV_KeeperCabin")
    keeper.set_editor_property("width", kw)
    keeper.set_editor_property("depth", kd)
    keeper.set_editor_property("wall_height", 262.0)
    keeper.set_editor_property("foundation_depth", 45.0)
    keeper.set_editor_property("seed", 31)
    keeper.set_editor_property("chimney_pos", unreal.Vector2D(-0.5, 0.5))
    keeper.set_editor_property("openings", [
        opening(W.BACK, -120.0, 112.0, 215.0, door_panel=False, lantern=-1),
        opening(W.RIGHT, 0.0, 120.0, 215.0, door_panel=False, lantern=0),
        opening(W.LEFT, 60.0, 70.0, 80.0, 105.0, window=True, lit=True),
        opening(W.FRONT, -150.0, 70.0, 80.0, 105.0, window=True, lit=True),
        opening(W.BACK, 140.0, 70.0, 80.0, 105.0, window=True, lit=False),
    ])
    keeper.rebuild_now()
    height = max(700.0, lamp_z - (floor_z - 40.0) - 174.0)
    tx = kc.x + kw * 0.5 + 15.0 + 255.0
    lh.set_actor_location_and_rotation(V(tx, kc.y, floor_z - 40.0 * height / 1000.0), rot(yaw=180.0), False, False)
    lh.set_editor_property("build_tower", True)
    lh.set_editor_property("door_panel", False)
    lh.set_editor_property("interior_props", False)
    lh.set_editor_property("tower_height", height)
    lh.rebuild_now()
    add_dust("NL_ENV_DustLighthouse", V(tx, kc.y, floor_z + 125.0), V(150.0, 150.0, 105.0), seed=21)
    gz = floor_z
    yaw = 180.0
    # Oyuncu başlangıcı eski kulenin içindeydi: kontrol odasına al
    ps = by_label("OYUNCU_BASLANGICI")
    desk = next((a for a in eas.get_all_level_actors() if a.get_class().get_name() == "NLSecurityDesk"), None)
    if ps and desk:
        d = desk.get_actor_location()
        ps.set_actor_location(V(d.x - 150.0, d.y + 150.0, d.z + 30.0), False, False)
    log("Deniz feneri: zemin %.0f, kule %.0f cm, kapi yonu %.0f" % (gz, height, yaw))

# Yakıt alanındaki blockout bidonlar -> benzin bidonu modeli
for a in labels_starting("YAKIT_BIDONU"):
    o, e = a.get_actor_bounds(False)
    a.set_actor_scale3d(V(1, 1, 1))
    a.set_actor_location(V(o.x, o.y, o.z - e.z), False, False)
    c = a.get_component_by_class(unreal.StaticMeshComponent)
    c.set_editor_property("static_mesh", unreal.load_asset("/Game/Environment/Kit/SM_NL_JerryCan"))
    c.set_material(0, mat("MI_NL_JerryRed"))
    c.set_material(1, mat("MI_NL_Metal"))
cab_actor = by_label("NL_ENV_DockCabin")
if cab_actor:
    p = cab_actor.get_actor_location()
    add_dust("NL_ENV_DustDockCabin", V(p.x, p.y, p.z + 135.0), V(170.0, 180.0, 115.0), cab_actor.get_actor_rotation().yaw, seed=22)
ruin_actor = by_label("NL_ENV_RuinedCabin")
if ruin_actor:
    p = ruin_actor.get_actor_location()
    add_dust("NL_ENV_DustRuin", V(p.x, p.y, p.z + 125.0), V(300.0, 225.0, 105.0), seed=23, density=25.0)

# ------------------------------------------------------------------ ağaçlar: her orman konisi bir çam

forest = by_label("ORMAN_BLOKLARI")
trees = []
try:
  if forest:
    fc = forest.get_component_by_class(unreal.StaticMeshComponent)
    sm = fc.get_editor_property("static_mesh")
    dm = unreal.new_object(unreal.DynamicMesh)
    unreal.GeometryScript_AssetUtils.copy_mesh_from_static_mesh(sm, dm, unreal.GeometryScriptCopyMeshFromAssetOptions(), unreal.GeometryScriptMeshReadLOD())
    res = unreal.GeometryScript_MeshDecomposition.split_mesh_by_components(dm, unreal.new_object(unreal.DynamicMeshPool))
    parts = res
    if isinstance(res, tuple):
        parts = next((x for x in res if isinstance(x, (list, unreal.Array))), [])
    xf = forest.get_actor_transform()
    clusters = []   # [x, y, zmin, zmax, n]
    for p in parts:
        b = unreal.GeometryScript_MeshQueries.get_mesh_bounding_box(p)
        lo = xf.transform_location(V((b.min.x + b.max.x) * 0.5, (b.min.y + b.max.y) * 0.5, b.min.z))
        hi = xf.transform_location(V((b.min.x + b.max.x) * 0.5, (b.min.y + b.max.y) * 0.5, b.max.z))
        for c in clusters:
            if (c[0] - lo.x) ** 2 + (c[1] - lo.y) ** 2 < 170.0 ** 2:
                c[2], c[3] = min(c[2], lo.z), max(c[3], hi.z)
                break
        else:
            clusters.append([lo.x, lo.y, lo.z, hi.z, 1])
    for c in clusters:
        trees.append((V(c[0], c[1], c[2]), c[3] - c[2], 100.0))
    log("Orman bloklari: %d parca" % len(trees))
except Exception as e:
    log("HATA orman ayrisma: %s" % e)

scat = eas.spawn_actor_from_class(unreal.NLEnvScatter, V(0, 0, 0), rot())
scat.set_actor_label("NL_ENV_Scatter")
r = random.Random(17)
pines = [scat.get_editor_property(n) for n in ("pine_a", "pine_b", "pine_c")]
placed = []


REJ = {"zemin": 0, "alcak": 0, "yakin": 0}


def add_tree(x, y, h_scale):
    z, hit_actor = ground(x, y, [forest] if forest else [])
    if z is None:
        REJ["zemin"] += 1
        return False
    if z < 40.0:
        REJ["alcak"] += 1
        return False
    for (px, py) in placed:
        if (px - x) ** 2 + (py - y) ** 2 < 230.0 ** 2:
            REJ["yakin"] += 1
            return False
    placed.append((x, y))
    yaw = r.uniform(0, 360)
    s = h_scale * r.uniform(0.85, 1.15)
    xf = unreal.Transform(V(x, y, z - 15.0), rot(yaw=yaw, pitch=r.uniform(-2, 2), roll=r.uniform(-2, 2)), V(s, s, s * r.uniform(0.92, 1.08)))
    roll = r.random()
    if roll < 0.07:
        scat.get_editor_property("dead_pine").add_instance(xf, True)
    else:
        pines[r.randrange(3)].add_instance(xf, True)
    if r.random() < 0.06:
        scat.get_editor_property("stumps").add_instance(unreal.Transform(V(x + r.uniform(-200, 200), y + r.uniform(-200, 200), z - 5.0), rot(yaw=r.uniform(0, 360)), V(1, 1, 1)), True)
    return True


n_trees = 0
for (base, h, rad) in trees:
    if rad > 450.0:
        # Büyük blok: alanı ağaçla doldur
        cnt = int((rad / 300.0) ** 2 * 2.2)
        for _ in range(cnt):
            a, d = r.uniform(0, 6.283), rad * math.sqrt(r.random()) * 0.9
            n_trees += add_tree(base.x + math.cos(a) * d, base.y + math.sin(a) * d, r.uniform(0.75, 1.1))
    else:
        n_trees += add_tree(base.x + r.uniform(-40, 40), base.y + r.uniform(-40, 40), max(0.8, min(1.3, h / 450.0)))
if forest and n_trees > 0:
    destroy(forest)


# ---- patika maskesini oku (PNG olarak dışa aktar, saf Python ile çöz)
import struct, zlib


def read_png(path):
    data = open(path, "rb").read()
    pos, idat, w, h, ct, bd = 8, b"", 0, 0, 0, 8
    while pos < len(data):
        ln = struct.unpack(">I", data[pos:pos + 4])[0]
        tag = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + ln]
        if tag == b"IHDR":
            w, h, bd, ct = struct.unpack(">IIBB", body[:10])
        elif tag == b"IDAT":
            idat += body
        pos += 12 + ln
    ch = {0: 1, 2: 3, 4: 2, 6: 4}[ct] * (2 if bd == 16 else 1)
    raw = zlib.decompress(idat)
    stride = w * ch
    out = bytearray(h * stride)
    prev = bytearray(stride)
    i = 0
    for y in range(h):
        f = raw[i]
        i += 1
        line = bytearray(raw[i:i + stride])
        i += stride
        for x in range(stride):
            a = line[x - ch] if x >= ch else 0
            b = prev[x]
            c = prev[x - ch] if x >= ch else 0
            if f == 1:
                line[x] = (line[x] + a) & 255
            elif f == 2:
                line[x] = (line[x] + b) & 255
            elif f == 3:
                line[x] = (line[x] + ((a + b) >> 1)) & 255
            elif f == 4:
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                line[x] = (line[x] + (a if pa <= pb and pa <= pc else (b if pb <= pc else c))) & 255
        out[y * stride:(y + 1) * stride] = line
        prev = line
    return w, h, ch, out


mask = None
try:
    tex = unreal.load_asset("/Game/AdaV2_Varliklari/DOKU_Patika")
    png = os.path.join(unreal.Paths.project_saved_dir(), "NLTextures", "patika_mask.png")
    task = unreal.AssetExportTask()
    task.object = tex
    task.filename = png
    task.automated = True
    task.replace_identical = True
    task.prompt = False
    unreal.Exporter.run_asset_export_task(task)
    mask = read_png(png)
    log("Patika maskesi: %dx%d, %d kanal" % (mask[0], mask[1], mask[2]))
except Exception as e:
    log("UYARI: patika maskesi okunamadi: %s" % e)


def path_at(x, y):
    if not mask:
        return 0.0
    w, h, ch, px = mask
    u = min(w - 1, max(0, int((x + 4671.25) / 9342.5 * w)))
    v = min(h - 1, max(0, int((y + 4671.25) / 9342.5 * h)))
    return px[(v * w + u) * ch] / 255.0


# Yasak bölgeler: binalar, oynanış noktaları
KEEP_OUT = []
for a in eas.get_all_level_actors():
    l = a.get_actor_label()
    cls = a.get_class().get_name()
    p = a.get_actor_location()
    if l.startswith("NL_ENV_Dock"):
        KEEP_OUT.append((p.x, p.y, 1100.0))
    elif l in ("NL_ENV_DockCabin", "NL_ENV_RuinedCabin", "NL_LH_Lighthouse", "NL_ENV_KeeperCabin", "YAKIT_ALANI", "JENERATOR_ANA", "DENIZ_FENERI", "FENER_KONTROL_BINASI", "FENER_DONER_MERDIVEN", "FENER_YASAM_ALANI"):
        KEEP_OUT.append((p.x, p.y, 750.0))
    elif cls in ("NLSecurityCamera",):
        KEEP_OUT.append((p.x, p.y, 450.0))
    elif cls in ("PlayerStart", "NLGenerator", "NLFuseBox", "NLRunner", "NLHideDoor"):
        KEEP_OUT.append((p.x, p.y, 500.0))
cams = [(a.get_actor_location(), a.get_actor_forward_vector()) for a in eas.get_all_level_actors() if a.get_class().get_name() == "NLSecurityCamera"]


BREJ = {"bina": 0, "kamera": 0, "patika": 0}


def blocked(x, y):
    for (kx, ky, kr) in KEEP_OUT:
        if (kx - x) ** 2 + (ky - y) ** 2 < kr * kr:
            BREJ["bina"] += 1
            return True
    # Güvenlik kameralarının görüşünü kapatma
    for (cp, cf) in cams:
        dx, dy = x - cp.x, y - cp.y
        d = math.sqrt(dx * dx + dy * dy)
        if d < 2200.0 and d > 1.0:
            fl = math.sqrt(cf.x * cf.x + cf.y * cf.y) or 1.0
            if (dx * cf.x + dy * cf.y) / (d * fl) > math.cos(math.radians(20.0)):
                BREJ["kamera"] += 1
                return True
    # Patikadan uzak dur
    for (ox, oy) in ((0, 0), (260, 0), (-260, 0), (0, 260), (0, -260)):
        if path_at(x + ox, y + oy) > 0.3:
            BREJ["patika"] += 1
            return True
    return False


fill = 0
rej_fill = 0
for gx in range(-4600, 4600, 255):
    for gy in range(-4600, 4600, 255):
        x, y = gx + r.uniform(-150, 150), gy + r.uniform(-150, 150)
        # Orman öbekleri ve açıklıklar
        dens = 0.72 + 0.3 * math.sin(x / 1300.0 + 1.3) * math.cos(y / 1100.0 - 0.4) + 0.15 * math.sin((x + y) / 700.0)
        if r.random() > dens:
            continue
        if blocked(x, y):
            rej_fill += 1
            continue
        z, _ = ground(x, y, [scat])
        if z is None or z < 70.0:
            continue
        if add_tree(x, y, r.uniform(0.7, 1.15)):
            fill += 1
log("Doldurma agaclari: %d (patika/bina/kamera nedeniyle elenen %d)" % (fill, rej_fill))
log("  elenme: %s" % BREJ)
if mask:
    _w, _h, _ch, _px = mask
    vals = [_px[i * _ch] for i in range(_w * _h)]
    log("  maske: ort %.2f, >0.3 orani %.2f, min %d max %d" % (sum(vals) / len(vals) / 255.0, sum(1 for v in vals if v > 76) / len(vals), min(vals), max(vals)))
log("Agaclar: %d (elenen %s)" % (n_trees, REJ))
hs = sorted(t[1] for t in trees)
rs = sorted(t[2] for t in trees)
if trees:
    log("parca yukseklik min/med/max %.0f/%.0f/%.0f, yaricap %.0f/%.0f/%.0f" % (hs[0], hs[len(hs)//2], hs[-1], rs[0], rs[len(rs)//2], rs[-1]))

# ------------------------------------------------------------------ kıyı kayaları

rocks = [scat.get_editor_property(n) for n in ("rock_a", "rock_b", "rock_c")]
n_rocks = 0
for i in range(150):
    ang = i * (360.0 / 150) + r.uniform(-1.0, 1.0)
    dx, dy = math.cos(math.radians(ang)), math.sin(math.radians(ang))
    last_land = None
    for d in range(500, 7400, 120):
        z, _ = ground(dx * d, dy * d, [scat])
        if z is None:
            continue
        if z > 30.0:
            last_land = (dx * d, dy * d, z)
        elif last_land is not None:
            break
    if not last_land:
        continue
    x, y, _ = last_land
    # rıhtımın ve botun yaklaşma yolunun yakınına kaya koyma
    if dock_line:
        o, dyaw, hl = dock_line
        lx = (x - o.x) * math.cos(math.radians(dyaw)) + (y - o.y) * math.sin(math.radians(dyaw))
        ly = -(x - o.x) * math.sin(math.radians(dyaw)) + (y - o.y) * math.cos(math.radians(dyaw))
        if -hl - 600 < lx < hl + 6000 and abs(ly) < 1500:
            continue
    for k in range(r.randint(1, 3)):
        px = x + dx * r.uniform(-150, 250) + r.uniform(-120, 120)
        py = y + dy * r.uniform(-150, 250) + r.uniform(-120, 120)
        z, _ = ground(px, py, [scat])
        if z is None:
            continue
        s = r.uniform(0.5, 1.6)
        rocks[r.randrange(3)].add_instance(unreal.Transform(V(px, py, max(z, 0.0) - 25.0 * s), rot(yaw=r.uniform(0, 360), pitch=r.uniform(-8, 8), roll=r.uniform(-8, 8)), V(s, s, s * r.uniform(0.8, 1.2))), True)
        n_rocks += 1
log("Kayalar: %d" % n_rocks)

# ------------------------------------------------------------------ gece, sis, pozlama

for a in eas.get_all_level_actors():
    cls = a.get_class()
    if cls == unreal.VolumetricCloud.static_class():
        destroy(a)
        log("Bulut katmani kaldirildi (performans)")
    elif cls == unreal.DirectionalLight.static_class():
        c = a.get_component_by_class(unreal.DirectionalLightComponent)
        c.set_editor_property("intensity", ATM["moon_lux"])
        mc = ATM["moon_color"]
        c.set_editor_property("light_color", unreal.Color(r=mc[0], g=mc[1], b=mc[2], a=255))
        # Ay sisi az aydınlatsın: ağaçların sisteki keskin gölge sütunları kaybolsun
        c.set_editor_property("volumetric_scattering_intensity", 0.2)
    elif cls == unreal.SkyLight.static_class():
        a.get_component_by_class(unreal.SkyLightComponent).set_editor_property("intensity", ATM["sky_intensity"])
    elif cls == unreal.ExponentialHeightFog.static_class():
        c = a.get_component_by_class(unreal.ExponentialHeightFogComponent)
        c.set_editor_property("fog_density", ATM["fog_density"])
        c.set_editor_property("fog_height_falloff", ATM["fog_falloff"])
        fcol = ATM["fog_color"]
        c.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(r=fcol[0], g=fcol[1], b=fcol[2], a=1.0))
        try:
            f2 = c.get_editor_property("second_fog_data")
            f2.set_editor_property("fog_density", ATM["fog2_density"])
            f2.set_editor_property("fog_height_falloff", ATM["fog2_falloff"])
            f2.set_editor_property("fog_height_offset", ATM["fog2_offset"])
            c.set_editor_property("second_fog_data", f2)
        except Exception as e:
            log("UYARI: ikinci sis katmani: %s" % e)
        c.set_editor_property("enable_volumetric_fog", True)
        c.set_editor_property("volumetric_fog_extinction_scale", ATM["vol_fog_extinction"])
        c.set_editor_property("volumetric_fog_distance", ATM["vol_fog_distance"])
    elif cls == unreal.PostProcessVolume.static_class():
        pp = a.get_editor_property("settings")

        def setp(name, value):
            try:
                pp.set_editor_property("override_" + name, True)
                pp.set_editor_property(name, value)
            except Exception as e:
                log("UYARI: post process %s: %s" % (name, e))
        setp("auto_exposure_bias", ATM["exposure_bias"])
        # Performans: Lumen yansımaları yerine ekran-uzayı yansıma; Lumen GI daha düşük kalitede
        setp("reflection_method", unreal.ReflectionMethod.SCREEN_SPACE)
        setp("lumen_scene_lighting_quality", 0.5)
        setp("lumen_final_gather_quality", 0.6)
        setp("lumen_scene_detail", 0.75)
        a.set_editor_property("settings", pp)

ok = unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
log("Kaydedildi: %s" % ok)
with open(os.path.join(unreal.Paths.project_saved_dir(), "NLDressAda.log"), "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
