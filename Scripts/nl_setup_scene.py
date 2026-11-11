# No Light After Midnight - Lvl_NLTest sahne kurulumu
# Eksik aktörleri koyar, kameraları 8'e tamamlar, mesh/anim/materyal atar, döngü seslerini ayarlar.
# Tekrar çalıştırmak güvenlidir: var olanları çoğaltmaz, sadece ayarlarını düzeltir.
#
# Çalıştırma (editör kapalıyken):
#   UnrealEditor-Cmd.exe NoLightAfterMidnight.uproject -ExecutePythonScript="<tam yol>\Scripts\nl_setup_scene.py" -unattended -nullrhi
# veya editör açıkken: Tools > Execute Python Script

import os
import unreal

MAP = "/Game/Test/Lvl_NLTest"
LOG = []


def log(msg):
    LOG.append(msg)
    unreal.log("[NLSetup] " + msg)


eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
V = unreal.Vector
R = unreal.Rotator


def rot(pitch=0.0, yaw=0.0, roll=0.0):
    r = R()
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


def set_mesh(c, mesh, loc=None, scale=None, rotation=None, material=None):
    if not c:
        return
    c.modify()
    if mesh:
        c.set_editor_property("static_mesh", mesh)
    if loc is not None:
        c.set_editor_property("relative_location", loc)
    if rotation is not None:
        c.set_editor_property("relative_rotation", rotation)
    if scale is not None:
        c.set_editor_property("relative_scale3d", scale)
    if material:
        c.set_material(0, material)


def actors_of(cls):
    return [a for a in eas.get_all_level_actors() if a.get_class() == cls.static_class()]


def ensure_one(cls, label, loc, rotation):
    found = actors_of(cls)
    if found:
        return found[0], False
    a = eas.spawn_actor_from_class(cls, loc, rotation)
    a.set_actor_label(label)
    log("Eklendi: %s @ %s" % (label, loc))
    return a, True


# ------------------------------------------------------------------ assetler

CUBE = load("/Engine/BasicShapes/Cube")
CYL = load("/Engine/BasicShapes/Cylinder")
RT = load("/Game/Mechanics/RT_CameraFeed")
QUINN = load("/Game/Characters/Mannequins/Meshes/SKM_Quinn_Simple")
MANNY = load("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple")
ABP = unreal.EditorAssetLibrary.load_blueprint_class("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed")

# Döngü sesleri
for name in ["generator_hum_loop", "monitor_hum_loop", "drowned_breath_loop", "runner_chase_loop", "ambient_wind_sea_loop"]:
    w = load("/Game/Audio/NL/" + name)
    if w and not w.get_editor_property("looping"):
        w.set_editor_property("looping", True)
        unreal.EditorAssetLibrary.save_loaded_asset(w)
        log("Looping acildi: " + name)

# Monitör materyali: Emissive = FeedTexture x Power
MON_PATH = "/Game/Mechanics/M_NLCameraMonitor"
MON = unreal.load_asset(MON_PATH) if unreal.EditorAssetLibrary.does_asset_exist(MON_PATH) else None
if not MON:
    mel = unreal.MaterialEditingLibrary
    MON = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_NLCameraMonitor", "/Game/Mechanics", unreal.Material, unreal.MaterialFactoryNew())
    MON.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    tex = mel.create_material_expression(MON, unreal.MaterialExpressionTextureSampleParameter2D, -700, 0)
    tex.set_editor_property("parameter_name", "FeedTexture")
    if RT:
        tex.set_editor_property("texture", RT)
    pw = mel.create_material_expression(MON, unreal.MaterialExpressionScalarParameter, -700, 300)
    pw.set_editor_property("parameter_name", "Power")
    pw.set_editor_property("default_value", 1.0)
    mul = mel.create_material_expression(MON, unreal.MaterialExpressionMultiply, -350, 100)
    mel.connect_material_expressions(tex, "RGB", mul, "A")
    mel.connect_material_expressions(pw, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(MON)
    unreal.EditorAssetLibrary.save_loaded_asset(MON)
    log("Olusturuldu: " + MON_PATH)

# ------------------------------------------------------------------ harita

unreal.EditorLoadingAndSavingUtils.load_map(MAP)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
ws = world.get_world_settings()
if ws.get_editor_property("default_game_mode") != unreal.NLGameMode.static_class():
    ws.modify()
    ws.set_editor_property("default_game_mode", unreal.NLGameMode.static_class())
    log("GameMode Override = NLGameMode")

# --- Masa: masa + monitör görünümü ---
desk = actors_of(unreal.NLSecurityDesk)[0]
desk.modify()
set_mesh(comp(desk, "DeskMesh"), CUBE, loc=V(10, 0, 37.5), scale=V(0.7, 1.4, 0.75))
set_mesh(comp(desk, "MonitorMesh"), CUBE, loc=V(30, 0, 110), scale=V(0.05, 0.9, 0.55), material=MON)
if RT:
    desk.set_editor_property("feed_render_target", RT)

# --- 8 kamera: (kamera konumu, Drowned'ın ayak noktası, yer adı) ---
CAMS = [
    (V(-1930, -620, 830),  V(-2250, -950, 500),  "ARŞİV"),
    (V(-130, -1870, 830),  V(-700, -1550, 500),  "ÜST OFİS"),
    (V(-150, -1180, 360),  V(-400, -850, 0),     "KORİDOR"),
    (V(-1100, -1320, 360), V(-1550, -1650, 0),   "DEPO"),
    (V(-2050, -1320, 360), V(-2350, -1450, 0),   "MERDİVEN"),
    (V(-2850, 50, 820),    V(-2100, 650, 0),     "LOBİ BATI"),
    (V(-150, 50, 820),     V(-750, 300, 0),      "LOBİ DOĞU"),
    (V(-1230, -620, 830),  V(-1550, -1000, 500), "TOPLANTI"),
]
DROWNED_HALF_HEIGHT = 90.0

existing = actors_of(unreal.NLSecurityCamera)


def nearest_existing(pos):
    best, bd = None, 1e18
    for a in existing:
        d = (a.get_actor_location() - pos).length()
        if d < bd:
            best, bd = a, d
    return best if bd < 50 else None


cams = []
for i, (pos, feet, label) in enumerate(CAMS):
    cam = nearest_existing(pos)
    if cam:
        existing.remove(cam)
    else:
        cam = eas.spawn_actor_from_class(unreal.NLSecurityCamera, pos, rot())
        log("Eklendi: Kamera %d (%s)" % (i + 1, label))
    cam.modify()
    cam.set_actor_label("NLSecurityCamera_%02d" % (i + 1))
    look = unreal.MathLibrary.find_look_at_rotation(pos, feet + V(0, 0, 100))
    cam.set_actor_rotation(look, False)
    cam.set_editor_property("camera_name", "Cam%02d" % (i + 1))
    cam.set_editor_property("location_label", label)
    set_mesh(comp(cam, "CameraMesh"), CUBE, loc=V(-10, 0, 0), scale=V(0.3, 0.15, 0.15))

    # Drowned kameraya bakarak, ışığın menzilinde durur
    arrow = comp(cam, "DrownedSpawnPoint")
    arrow.modify()
    face = unreal.MathLibrary.find_look_at_rotation(feet, V(pos.x, pos.y, feet.z))
    arrow.set_world_location_and_rotation(feet + V(0, 0, DROWNED_HALF_HEIGHT), rot(0, face.yaw, 0), False, False)
    cams.append(cam)

for extra in existing:
    log("Not: fazladan kamera birakildi (listeye eklenmedi): " + extra.get_actor_label())

desk.set_editor_property("cameras", cams)
log("Masa kameralari: %d" % len(cams))

# --- Drowned ---
for d in actors_of(unreal.NLDrowned):
    d.modify()
    sk = d.get_components_by_class(unreal.SkeletalMeshComponent)[0]
    sk.modify()
    if MANNY and not sk.get_editor_property("skeletal_mesh_asset"):
        sk.set_editor_property("skeletal_mesh_asset", MANNY)
    if ABP:
        sk.set_editor_property("anim_class", ABP)

# --- Şalter kolu ---
fuse = actors_of(unreal.NLFuseBox)[0]
fuse.modify()
set_mesh(comp(fuse, "LeverMesh"), CUBE, loc=V(50, 105, 50), scale=V(0.06, 0.06, 0.3))

# --- Fener şarj istasyonu: şalterin yanında ---
charger, _ = ensure_one(unreal.NLFlashlightCharger, "NLFlashlightCharger", V(-2020, -60, 140), rot(0, 90, 0))
charger.modify()
set_mesh(comp(charger, "Mesh"), CUBE, scale=V(0.3, 0.2, 0.25))

# --- Benzin bidonu: alt kat depo ---
can, _ = ensure_one(unreal.NLFuelCan, "NLFuelCan", V(-1500, -1750, 25), rot())
can.modify()
set_mesh(comp(can, "Mesh"), CYL, scale=V(0.3, 0.3, 0.5))

# --- Runner: alt katın doğu odası ---
runner, _ = ensure_one(unreal.NLRunner, "NLRunner", V(-300, -1600, 92), rot(0, 90, 0))
runner.modify()
rsk = runner.get_components_by_class(unreal.SkeletalMeshComponent)[0]
rsk.modify()
if QUINN and not rsk.get_editor_property("skeletal_mesh_asset"):
    rsk.set_editor_property("skeletal_mesh_asset", QUINN)
if ABP:
    rsk.set_editor_property("anim_class", ABP)

# --- NavMesh: binanın tamamı ---
navs = actors_of(unreal.NavMeshBoundsVolume)
if not navs:
    nav = eas.spawn_actor_from_class(unreal.NavMeshBoundsVolume, V(-1500, -500, 425), rot())
    nav.set_actor_label("NavMeshBoundsVolume")
    nav.set_actor_scale3d(V(16, 16, 5.5))
    log("Eklendi: NavMeshBoundsVolume")
    navs = [nav]
o, e = navs[0].get_actor_bounds(False)
log("NavMesh sinirlari: merkez=(%.0f,%.0f,%.0f) yaricap=(%.0f,%.0f,%.0f)" % (o.x, o.y, o.z, e.x, e.y, e.z))

# ------------------------------------------------------------------ kaydet
ok = unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
log("Kaydedildi: %s" % ok)

out = os.path.join(unreal.Paths.project_saved_dir(), "NLSetup.log")
with open(out, "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))

if "-unattended" in unreal.SystemLibrary.get_command_line().lower():
    unreal.SystemLibrary.quit_editor()
