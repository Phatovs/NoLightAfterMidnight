# No Light After Midnight - Ada level'ine oynanış kurulumu
# Esat'ın blockout'u (/Game/AdaBlockout_YeniV2) dokunulmadan kalır; oynanış kopyada kurulur: /Game/Levels/Lvl_Ada
# Gece ışığı ve post process test level'den (Lvl_NLTest) kopyalanır.
# Tekrar çalıştırmak güvenlidir: var olanları çoğaltmaz, konum/ayarları düzeltir.
#
# Çalıştırma (editör kapalıyken):
#   UnrealEditor-Cmd.exe NoLightAfterMidnight.uproject -run=pythonscript -script="<tam yol>\Scripts\nl_setup_ada.py" -unattended
# Blockout'u baştan kopyalamak için: ortam değişkeni NL_ADA_RECOPY=1
# veya editör açıkken: Tools > Execute Python Script

import math
import os
import unreal

SRC_MAP = "/Game/AdaBlockout_YeniV2"
MAP = "/Game/Levels/Lvl_Ada"
NIGHT_MAP = "/Game/Test/Lvl_NLTest"
LOG = []


def log(msg):
    LOG.append(msg)
    unreal.log("[NLAda] " + msg)


eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
eal = unreal.EditorAssetLibrary
V = unreal.Vector


def rot(pitch=0.0, yaw=0.0, roll=0.0):
    r = unreal.Rotator()
    r.pitch, r.yaw, r.roll = pitch, yaw, roll
    return r


def load(path):
    a = unreal.load_asset(path)
    if not a:
        log("UYARI: asset bulunamadi: " + path)
    return a


def comp(actor, name):
    for c in actor.get_components_by_class(unreal.ActorComponent):
        if c.get_name() == name:
            return c
    return None


def set_mesh(c, mesh, loc=None, scale=None, material=None):
    if not c:
        return
    c.modify()
    if mesh:
        c.set_editor_property("static_mesh", mesh)
    if loc is not None:
        c.set_editor_property("relative_location", loc)
    if scale is not None:
        c.set_editor_property("relative_scale3d", scale)
    if material:
        c.set_material(0, material)


def all_actors():
    return eas.get_all_level_actors()


def actors_of(cls):
    return [a for a in all_actors() if a.get_class() == cls.static_class()]


def by_label(label):
    for a in all_actors():
        if a.get_actor_label() == label:
            return a
    return None


def ensure_one(cls, label, loc, rotation):
    for a in actors_of(cls):
        if a.get_actor_label() == label:
            a.modify()
            a.set_actor_location_and_rotation(loc, rotation, False, False)
            return a
    a = eas.spawn_actor_from_class(cls, loc, rotation)
    a.set_actor_label(label)
    log("Eklendi: %s @ (%.0f, %.0f, %.0f)" % (label, loc.x, loc.y, loc.z))
    return a


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def trace(start, end, ignore=None):
    hit = unreal.SystemLibrary.line_trace_single(world(), start, end, unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True,
                                                 ignore or [], unreal.DrawDebugTrace.NONE, True)
    if not hit:
        return None
    t = hit.to_tuple()
    return t[5] if t[0] else None


def ground(x, y, top=5000.0, ignore=None):
    p = trace(V(x, y, top), V(x, y, -3000), ignore)
    if p is None:
        log("UYARI: zemin bulunamadi (%.0f, %.0f)" % (x, y))
        return 0.0
    return p.z


def wall_dist(origin, direction, ignore=None):
    p = trace(origin, origin + direction * 2000.0, ignore)
    return (p - origin).length() if p is not None else 2000.0


# ------------------------------------------------------------------ assetler

CUBE = load("/Engine/BasicShapes/Cube")
CYL = load("/Engine/BasicShapes/Cylinder")
RT = load("/Game/Mechanics/RT_CameraFeed")
MON = load("/Game/Mechanics/M_NLCameraMonitor")
QUINN = load("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple")
MANNY = load("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple")
ABP = unreal.EditorAssetLibrary.load_blueprint_class("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed")

# ------------------------------------------------------------------ gece ayarları (test level'den)

# Not: harita değişince Python'daki struct kopyaları bozulabiliyor, bu yüzden değerler düz sayı olarak saklanır
def rgba(col):
    return (col.r, col.g, col.b, col.a)


night = {}
unreal.EditorLoadingAndSavingUtils.load_map(NIGHT_MAP)
for a in all_actors():
    cls = a.get_class()
    if cls == unreal.DirectionalLight.static_class():
        c = a.get_component_by_class(unreal.DirectionalLightComponent)
        r0 = a.get_actor_rotation()
        night["sun"] = ((r0.pitch, r0.yaw, r0.roll), c.get_editor_property("intensity"), rgba(c.get_editor_property("light_color")))
    elif cls == unreal.SkyLight.static_class():
        c = a.get_component_by_class(unreal.SkyLightComponent)
        night["sky"] = (c.get_editor_property("intensity"), rgba(c.get_editor_property("light_color")))
    elif cls == unreal.PostProcessVolume.static_class():
        night["pp"] = a.get_editor_property("settings").export_text()
    elif cls == unreal.ExponentialHeightFog.static_class():
        c = a.get_component_by_class(unreal.ExponentialHeightFogComponent)
        night["fog"] = (c.get_editor_property("fog_density"), rgba(c.get_editor_property("fog_inscattering_luminance")))
log("Gece ayarlari: %s" % {k: v for k, v in night.items() if k != "pp"})

# ------------------------------------------------------------------ kopya

if not eal.does_asset_exist(MAP) or os.environ.get("NL_ADA_RECOPY") == "1":
    if eal.does_asset_exist(MAP):
        eal.delete_asset(MAP)
    eal.duplicate_asset(SRC_MAP, MAP)
    log("Kopyalandi: %s -> %s" % (SRC_MAP, MAP))

unreal.EditorLoadingAndSavingUtils.load_map(MAP)
ws = world().get_world_settings()
ws.modify()
ws.set_editor_property("default_game_mode", unreal.NLGameMode.static_class())

# Gece
for a in all_actors():
    cls = a.get_class()
    if cls == unreal.DirectionalLight.static_class() and "sun" in night:
        a.modify()
        r, i, col = night["sun"]
        a.set_actor_rotation(rot(*r), False)
        c = a.get_component_by_class(unreal.DirectionalLightComponent)
        c.set_editor_property("intensity", i)
        c.set_editor_property("light_color", unreal.Color(r=col[0], g=col[1], b=col[2], a=col[3]))
    elif cls == unreal.SkyLight.static_class() and "sky" in night:
        c = a.get_component_by_class(unreal.SkyLightComponent)
        c.set_editor_property("intensity", night["sky"][0])
        sc = night["sky"][1]
        c.set_editor_property("light_color", unreal.Color(r=sc[0], g=sc[1], b=sc[2], a=sc[3]))
        c.recapture_sky()
    elif cls == unreal.PostProcessVolume.static_class() and "pp" in night:
        a.modify()
        pp = unreal.PostProcessSettings()
        pp.import_text(night["pp"])
        a.set_editor_property("settings", pp)
        a.set_editor_property("unbound", True)
    elif cls == unreal.ExponentialHeightFog.static_class() and "fog" in night:
        c = a.get_component_by_class(unreal.ExponentialHeightFogComponent)
        c.set_editor_property("fog_density", night["fog"][0])
        fc = night["fog"][1]
        c.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(r=fc[0], g=fc[1], b=fc[2], a=fc[3]))
    elif cls == unreal.VolumetricCloud.static_class():
        a.set_is_temporarily_hidden_in_editor(False)
        a.set_actor_hidden_in_game(True)

# ------------------------------------------------------------------ atmosfer (korku)
# Gerçek karanlık: sabit pozlama (göz alışmaz), soluk ay, hacimli sis (fener huzmesi görünür),
# gren + vinyet + hafif renk kayması, soğuk ve soluk renkler. Değerleri buradan ayarlayın, scripti tekrar çalıştırın.
ATM = {
    "moon_lux": 0.3, "moon_color": (150, 165, 200),
    "sky_intensity": 0.12,
    "fog_density": 0.12, "fog_falloff": 0.06, "fog_color": (0.018, 0.020, 0.024),
    "vol_fog_albedo": 0.8, "vol_fog_extinction": 2.0, "vol_fog_distance": 6000.0,
    "exposure_bias": 0.2,          # sabit pozlama (EV); büyüdükçe aydınlanır
    "lamp_intensity": 30.0, "grain": 0.35, "vignette": 0.65, "fringe": 0.7, "bloom": 0.5,
    "saturation": 0.72, "contrast": 1.1, "gain": (0.94, 0.98, 1.05),
}


def set_pp(pp, name, value):
    try:
        pp.set_editor_property("override_" + name, True)
        pp.set_editor_property(name, value)
    except Exception as e:
        log("UYARI: post process alani ayarlanamadi: %s (%s)" % (name, e))


for a in all_actors():
    cls = a.get_class()
    if cls == unreal.DirectionalLight.static_class():
        c = a.get_component_by_class(unreal.DirectionalLightComponent)
        c.set_editor_property("intensity", ATM["moon_lux"])
        # Ay gökyüzünü güneş gibi maviye boyamasın: gökyüzü karanlık kalsın
        c.set_editor_property("atmosphere_sun_light", False)
        mc = ATM["moon_color"]
        c.set_editor_property("light_color", unreal.Color(r=mc[0], g=mc[1], b=mc[2], a=255))
    elif cls == unreal.SkyLight.static_class():
        c = a.get_component_by_class(unreal.SkyLightComponent)
        c.set_editor_property("intensity", ATM["sky_intensity"])
    elif cls == unreal.ExponentialHeightFog.static_class():
        c = a.get_component_by_class(unreal.ExponentialHeightFogComponent)
        a.modify()
        a.set_actor_location(V(0, 0, 0), False, False)   # sis yüksekliği deniz seviyesinden başlasın
        c.set_editor_property("fog_density", ATM["fog_density"])
        c.set_editor_property("fog_height_falloff", ATM["fog_falloff"])
        fc = ATM["fog_color"]
        c.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(r=fc[0], g=fc[1], b=fc[2], a=1.0))
        c.set_editor_property("enable_volumetric_fog", True)
        c.set_editor_property("volumetric_fog_albedo", unreal.Color(r=int(255 * ATM["vol_fog_albedo"]), g=int(255 * ATM["vol_fog_albedo"]), b=int(255 * ATM["vol_fog_albedo"]), a=255))
        c.set_editor_property("volumetric_fog_extinction_scale", ATM["vol_fog_extinction"])
        c.set_editor_property("volumetric_fog_distance", ATM["vol_fog_distance"])
    elif cls == unreal.PostProcessVolume.static_class():
        a.modify()
        pp = a.get_editor_property("settings")
        set_pp(pp, "auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL)
        set_pp(pp, "auto_exposure_apply_physical_camera_exposure", False)
        set_pp(pp, "auto_exposure_bias", ATM["exposure_bias"])
        set_pp(pp, "film_grain_intensity", ATM["grain"])
        set_pp(pp, "vignette_intensity", ATM["vignette"])
        set_pp(pp, "scene_fringe_intensity", ATM["fringe"])
        set_pp(pp, "bloom_intensity", ATM["bloom"])
        s = ATM["saturation"]
        set_pp(pp, "color_saturation", unreal.Vector4(s, s, s, 1.0))
        k = ATM["contrast"]
        set_pp(pp, "color_contrast", unreal.Vector4(k, k, k, 1.0))
        g = ATM["gain"]
        set_pp(pp, "color_gain", unreal.Vector4(g[0], g[1], g[2], 1.0))
        a.set_editor_property("settings", pp)
        a.set_editor_property("unbound", True)
log("Atmosfer uygulandi: %s" % ATM)

# ------------------------------------------------------------------ kontrol binası: masa, sigorta kutusu, fener şarjı

CTRL = V(272, -2012, 0)
# Ölçüler oyun içi çarpışma testinden (editör komut satırında bina meshlerinin çarpışması güvenilir değil)
# Oda: batı duvarı x=42, güney duvarı y=-2192, doğusu fenerin zemin katına, kuzeyi dışarıya açık
floor_z, west_x, east_x, south_y, north_y = 502.0, 42.0, 500.0, -2192.0, -1832.0
log("Kontrol odasi: zemin z=%.0f  x %.0f..%.0f  y %.0f..%.0f" % (floor_z, west_x, east_x, south_y, north_y))

# Masa güney duvarına bakar (oyuncu kuzeyde oturur, monitör güneyde)
desk_list = actors_of(unreal.NLSecurityDesk)
desk = ensure_one(unreal.NLSecurityDesk, "NLSecurityDesk", V(CTRL.x + 40, south_y + 70, floor_z), rot(0, -90, 0))
set_mesh(comp(desk, "DeskMesh"), CUBE, loc=V(10, 0, 37.5), scale=V(0.7, 1.4, 0.75))
set_mesh(comp(desk, "MonitorMesh"), CUBE, loc=V(30, 0, 110), scale=V(0.05, 0.9, 0.55), material=MON)
if RT:
    desk.set_editor_property("feed_render_target", RT)

# (Sigorta kutusu ve benzin bidonu rıhtım kulübesinde, aşağıda)

# Fener şarjı: sigorta kutusunun güneyinde, duvarda
charger = ensure_one(unreal.NLFlashlightCharger, "NLFlashlightCharger", V(west_x + 15, CTRL.y - 110, floor_z + 120), rot(0, 0, 0))
set_mesh(comp(charger, "Mesh"), CUBE, scale=V(0.2, 0.3, 0.25))

# Deniz feneri camlı bina: içindeyken camlara çarpan rüzgâr sesleri (NLAmbienceDirector bu etiketi arar)
lighthouse = by_label("DENIZ_FENERI")
if lighthouse:
    tags = list(lighthouse.get_editor_property("tags"))
    if "NLWindowGlass" not in [str(t) for t in tags]:
        lighthouse.modify()
        tags.append(unreal.Name("NLWindowGlass"))
        lighthouse.set_editor_property("tags", tags)
        log("Etiket: DENIZ_FENERI -> NLWindowGlass")
else:
    log("UYARI: DENIZ_FENERI bulunamadi (cam ruzgari sesi calmaz)")

# Tavan lambaları (test level'deki Light_C): kontrol odası ve fenerin zemin katı
LAMP = unreal.EditorAssetLibrary.load_blueprint_class("/Game/Variant_Horror/Blueprints/Light/Light")
if LAMP:
    for label, pos in [("Lamba_KontrolOdasi", V(CTRL.x + 30, CTRL.y, floor_z + 135)), ("Lamba_FenerZemin", V(925, -2013, 760))]:
        found = [a for a in all_actors() if a.get_actor_label() == label]
        lamp = found[0] if found else eas.spawn_actor_from_class(LAMP, pos, rot())
        lamp.set_actor_label(label)
        lamp.set_actor_location(pos, False, False)
        # Sabit pozlamada lambalar soluk kalmasın: iç mekân seçilsin, sıcak ve loş
        for lc in lamp.get_components_by_class(unreal.LocalLightComponent):
            log("Lamba %s: %s eski parlaklik %.1f (%s)" % (label, lc.get_class().get_name(), lc.get_editor_property("intensity"), lc.get_editor_property("intensity_units")))
            lc.set_editor_property("intensity", ATM["lamp_intensity"])
            lc.set_editor_property("light_color", unreal.Color(r=255, g=214, b=170, a=255))

# ------------------------------------------------------------------ jeneratör ve benzin

# Jeneratör = haritadaki JENERATOR_ANA binası (ayrı kutu yok). Binanın mesh'i, konumu ve malzemeleri NLGenerator'a aktarılır,
# blockout'taki StaticMeshActor kaldırılır. Tekrar çalıştırınca bina zaten NLGenerator olduğu için dokunulmaz.
GEN_LABEL = "JENERATOR_ANA"
gen_src = by_label(GEN_LABEL)
gen_existing = actors_of(unreal.NLGenerator)
gen_src = next((a for a in all_actors() if a.get_actor_label() == GEN_LABEL and a.get_class() == unreal.StaticMeshActor.static_class()), None)
gen = gen_existing[0] if gen_existing else eas.spawn_actor_from_class(unreal.NLGenerator, V(-208.21, -606.40, 104.69), rot())
gen.set_actor_label("NLGenerator")
gen.modify()
gmesh = comp(gen, "Mesh")
gmesh.modify()
if gen_src and gen_src.get_class() == unreal.StaticMeshActor.static_class():
    src_comp = gen_src.get_component_by_class(unreal.StaticMeshComponent)
    gen.set_actor_transform(gen_src.get_actor_transform(), False, False)
    gmesh.set_editor_property("relative_scale3d", V(1, 1, 1))
    gmesh.set_editor_property("static_mesh", src_comp.get_editor_property("static_mesh"))
    for i, mat in enumerate(src_comp.get_materials()):
        gmesh.set_material(i, mat)
    eas.destroy_actor(gen_src)
    log("Jenerator: %s binasi NLGenerator oldu (ayri kutu kaldirildi)" % GEN_LABEL)
gen.set_actor_label(GEN_LABEL)
# Benzin ağzı: güney duvarında, kapının sağında, el hizasında (binanın yerel koordinatı)
gen.set_editor_property("fuel_inlet_offset", V(-92, -92, 112))


# ------------------------------------------------------------------ 8 kamera (blockout'taki KAMERA_xx küreleri yerine)

CAMS = [
    ("01_RIHTIM", "DOCK"),
    ("02_YAKIT", "FUEL DEPOT"),
    ("03_YIKIK_EV", "RUINED HOUSE"),
    ("04_BATI_PATIKA", "WEST PATH"),
    ("05_ANA_KAVSAK", "CROSSROADS"),
    ("07_KUZEY_ORMAN", "NORTH WOODS"),
    ("08_DOGU_PATIKA", "EAST PATH"),
    ("09_FENER_GIRISI", "LIGHTHOUSE GATE"),
]
DROWNED_HALF_HEIGHT = 90.0
DROWNED_DIST = 650.0

existing = {a.get_actor_label(): a for a in actors_of(unreal.NLSecurityCamera)}
cams = []
for i, (key, label) in enumerate(CAMS):
    sphere = by_label("KAMERA_" + key)
    arrow_box = by_label("YON_" + key)
    cam_label = "NLSecurityCamera_%02d" % (i + 1)
    if sphere:
        pos = sphere.get_actor_location()
        if arrow_box:
            d = arrow_box.get_actor_location() - pos
        else:
            yaw = math.radians(sphere.get_actor_rotation().yaw)
            d = V(math.cos(yaw), math.sin(yaw), 0)
        d.z = 0
        d = d.normal()
        # Kamera direğin tepesinde, yön kutusunun gösterdiği yöne bakar
        feet_xy = (pos.x + d.x * DROWNED_DIST, pos.y + d.y * DROWNED_DIST)
        feet = V(feet_xy[0], feet_xy[1], ground(*feet_xy))
        node = existing.get(cam_label)
        if node is None:
            node = eas.spawn_actor_from_class(unreal.NLSecurityCamera, pos, rot())
            node.set_actor_label(cam_label)
            log("Eklendi: %s (%s)" % (cam_label, label))
        node.modify()
        node.set_actor_location(pos, False, False)
        look = unreal.MathLibrary.find_look_at_rotation(pos, feet + V(0, 0, 100))
        node.set_actor_rotation(look, False)
        node.set_editor_property("camera_name", "Cam%02d" % (i + 1))
        node.set_editor_property("location_label", label)
        set_mesh(comp(node, "CameraMesh"), CUBE, loc=V(-10, 0, 0), scale=V(0.3, 0.15, 0.15))
        arrow = comp(node, "DrownedSpawnPoint")
        arrow.modify()
        face = unreal.MathLibrary.find_look_at_rotation(feet, V(pos.x, pos.y, feet.z))
        arrow.set_world_location_and_rotation(feet + V(0, 0, DROWNED_HALF_HEIGHT), rot(0, face.yaw, 0), False, False)
        # Blockout'un yer tutucularını kaldır (direkler kalır)
        eas.destroy_actor(sphere)
        if arrow_box:
            eas.destroy_actor(arrow_box)
        cams.append(node)
    elif cam_label in existing:
        cams.append(existing[cam_label])
    else:
        log("UYARI: KAMERA_%s bulunamadi" % key)
desk.set_editor_property("cameras", cams)
log("Masa kameralari: %d" % len(cams))

# ------------------------------------------------------------------ yaratıklar

# Drowned: başta görünmez bir köşede durur, yönetici kameralara taşır
# (kuzey ormanının batısı, karada)
DROWNED_HOME = (-2004, -3000)   # kuzey ormanı kamerasının yakını
dz = ground(*DROWNED_HOME)
drowned = ensure_one(unreal.NLDrowned, "NLDrowned", V(DROWNED_HOME[0], DROWNED_HOME[1], dz + DROWNED_HALF_HEIGHT), rot())
log("Drowned zemin z=%.0f" % dz)
sk = drowned.get_components_by_class(unreal.SkeletalMeshComponent)[0]
sk.modify()
if MANNY and not sk.get_editor_property("skeletal_mesh_asset"):
    sk.set_editor_property("skeletal_mesh_asset", MANNY)
if ABP:
    sk.set_editor_property("anim_class", ABP)

director = ensure_one(unreal.NLDrownedDirector, "NLDrownedDirector", V(0, 0, 0), rot())
director.set_editor_property("placed_drowned", drowned)
director.set_editor_property("desk", desk)

# Runner: yıkık evin içinde bekler
# Çatının altından aşağı ışın: evin tabanı
rz = 118.0   # YIKIK_EV_TABAN üstü
runner = ensure_one(unreal.NLRunner, "NLRunner", V(2557, 871, rz + 92), rot(0, 90, 0))
rsk = runner.get_components_by_class(unreal.SkeletalMeshComponent)[0]
rsk.modify()
if QUINN and not rsk.get_editor_property("skeletal_mesh_asset"):
    rsk.set_editor_property("skeletal_mesh_asset", QUINN)
if ABP:
    rsk.set_editor_property("anim_class", ABP)

# ------------------------------------------------------------------ saklanma kapısı: rıhtım kulübesi

# Kulübenin ön duvarları arasındaki 112 cm boşluk; kapının ileri yönü kulübenin içi
old_door = by_label("RIHTIM_KULUBE_KAPISI")
if old_door:
    eas.destroy_actor(old_door)
    log("Kaldirildi: RIHTIM_KULUBE_KAPISI (yerine NLHideDoor)")
wall_a = by_label("RIHTIM_KULUBESI_ON_DUVAR")
wall_b = by_label("RIHTIM_KULUBESI_ON_DUVAR2")
shack_floor = by_label("RIHTIM_KULUBESI_TABAN")
if wall_a and wall_b and shack_floor:
    pa, pb = wall_a.get_actor_location(), wall_b.get_actor_location()
    door_mid = V((pa.x + pb.x) / 2, (pa.y + pb.y) / 2, 0)
    fo, fe = shack_floor.get_actor_bounds(False)
    door_mid.z = fo.z + fe.z
    inward = fo - door_mid
    wall_dir = (pb - pa)
    normal = V(-wall_dir.y, wall_dir.x, 0).normal()
    if normal.x * inward.x + normal.y * inward.y < 0:
        normal = normal * -1.0
    yaw = math.degrees(math.atan2(normal.y, normal.x))
    door = ensure_one(unreal.NLHideDoor, "NLHideDoor_Dock", door_mid, rot(0, yaw, 0))
    lintel = by_label("RIHTIM_KULUBESI_KAPI_LENTOSU")
    lo, le = lintel.get_actor_bounds(False) if lintel else (V(0, 0, door_mid.z + 212), V(0, 0, 0))
    open_h = (lo.z - le.z) - door_mid.z
    gap = (pb - pa).length() - 142.0
    door.set_editor_property("opening_width", gap)
    door.set_editor_property("opening_height", open_h)
    door.set_editor_property("wall_thickness", 22.0)
    door.set_editor_property("door_width", gap - 6.0)
    door.set_editor_property("door_height", open_h - 4.0)
    door.set_editor_property("handle_height", 95.0)
    log("Kapi: (%.0f, %.0f, %.0f) yaw=%.0f  bosluk=%.0f  yukseklik=%.0f" % (door_mid.x, door_mid.y, door_mid.z, yaw, gap, open_h))

    # --- Kulübenin içi: sigorta kutusu (arka duvar) ve benzin bidonu ---
    # Kulübe ekseni: n = kapıdan içeri, u = ön duvar boyunca. İç derinlik ~390 cm, genişlik ~370 cm (ölçü: duvar aktörleri)
    u = wall_dir.normal()
    u.z = 0
    back = by_label("RIHTIM_KULUBESI_ARKA_DUVAR")
    bo, be = back.get_actor_bounds(False)
    rel = bo - door_mid
    back_depth = rel.x * normal.x + rel.y * normal.y - 11.0   # duvarın iç yüzü (kalınlık 22)
    floor_top = door_mid.z
    # Sigorta kutusu: arkası duvara yaslı, ön yüzü (+Y) kapıya bakar; kutu +u yönüne, panel -u yönüne uzanır
    fuse_yaw = yaw + 90.0
    fuse_pos = door_mid + normal * back_depth + u * -5.0
    fuse_pos.z = floor_top + 100.0
    fuse = ensure_one(unreal.NLFuseBox, "NLFuseBox", fuse_pos, rot(0, fuse_yaw, 0))
    # Kabin, kol ve sigortalar C++ (ANLFuseBox::ApplyModel) tarafindan SM_NL_FuseCabinet modelinden kurulur
    # Benzin bidonu: kapının yanında yerde, ön rafın hemen yanında (rafın altında kalmasın)
    can_pos = door_mid + normal * 130.0 + u * 55.0
    can_pos.z = floor_top + 25.0
    can = ensure_one(unreal.NLFuelCan, "NLFuelCan", can_pos, rot())
    set_mesh(comp(can, "Mesh"), CYL, scale=V(0.3, 0.3, 0.5))
    # Runner pusu alanı: kulübenin çevresi (rastgele: girişte düşük ihtimalle çığlık + kovalama)
    amb = ensure_one(unreal.NLRunnerAmbushZone, "NLRunnerAmbush_Shack", V(fo.x, fo.y, fo.z), rot())
    amb.set_editor_property("radius", 1400.0)
    log("Kulube: sigorta (%.0f, %.0f, %.0f) yaw=%.0f  bidon (%.0f, %.0f)  arka duvar derinligi=%.0f" % (fuse_pos.x, fuse_pos.y, fuse_pos.z, fuse_yaw, can_pos.x, can_pos.y, back_depth))
else:
    log("UYARI: rihtim kulubesi duvarlari bulunamadi")

# ------------------------------------------------------------------ NavMesh: bütün ada

navs = actors_of(unreal.NavMeshBoundsVolume)
nav = navs[0] if navs else eas.spawn_actor_from_class(unreal.NavMeshBoundsVolume, V(-300, 0, 400), rot())
nav.modify()
nav.set_actor_label("NavMeshBoundsVolume")
# Alt sınır z=20: deniz seviyesinin (z~11) altındaki arazi NavMesh dışında kalır (Runner denizden çıkmasın)
nav.set_actor_location(V(-300, 0, 820), False, False)
nav.set_actor_scale3d(V(48, 46, 8))   # 9600 x 9200 x 1600 cm (z 20..1620)

# ------------------------------------------------------------------ kaydet
ok = unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
log("Kaydedildi: %s" % ok)

out = os.path.join(unreal.Paths.project_saved_dir(), "NLSetupAda.log")
with open(out, "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
