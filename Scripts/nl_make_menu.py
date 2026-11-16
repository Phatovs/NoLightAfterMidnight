# No Light After Midnight - ana menü level'i: /Game/Levels/Lvl_MainMenu
# Gece, denizin ortasında kayalık bir adacık: üstünde deniz feneri, yanında penceresi yanan bekçi kulübesi, birkaç çam.
# Kamera denizde (hafifçe sallanır). Deniz fenerinin huzmesi fare imlecini izler: menü yazısına tutunca orası aydınlanır.
# Yağmur, sis, şimşek ve gök gürültüsü (NLWeatherDirector), dalga ve rüzgâr sesi (NLMenuDirector).
# Tekrar çalıştırılabilir (level baştan kurulur).
# Çalıştırma (editör kapalıyken): UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<tam yol>\Scripts\nl_make_menu.py"
import math
import os
import random

import unreal

MAP = "/Game/Levels/Lvl_MainMenu"
ART = "/Game/Levels/Lvl_Ada_Art"
KIT = "/Game/Environment/Kit/"
MATS = "/Game/Environment/Materials/"
V = unreal.Vector
LOG = []
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(m):
    LOG.append(m)
    unreal.log("[NLMenu] " + m)


def rot(pitch=0.0, yaw=0.0, roll=0.0):
    r = unreal.Rotator()
    r.pitch, r.yaw, r.roll = pitch, yaw, roll
    return r


def mesh_actor(label, mesh, loc, yaw=0.0, scale=V(1, 1, 1), mats=None):
    a = eas.spawn_actor_from_class(unreal.StaticMeshActor, loc, rot(yaw=yaw))
    a.set_actor_label(label)
    c = a.static_mesh_component
    c.set_editor_property("mobility", unreal.ComponentMobility.STATIC)
    c.set_static_mesh(unreal.load_asset(mesh if mesh.startswith("/") else KIT + mesh))
    a.set_actor_scale3d(scale)
    for i, m in enumerate(mats or []):
        c.set_material(i, unreal.load_asset(m if m.startswith("/") else MATS + m))
    return a


def opening(wall, offset, width, height, sill=0.0, window=False, lit=True, shutters=True, door_panel=True, lantern=0, frame=True):
    o = unreal.NLCabinOpening()
    for k, v in (("wall", wall), ("offset", offset), ("width", width), ("height", height), ("sill", sill), ("window", window),
                 ("lit", lit), ("shutters", shutters), ("door_panel", door_panel), ("lantern_side", lantern), ("frame", frame)):
        o.set_editor_property(k, v)
    return o


# Oyunun post process ayarları (sabit pozlama, gren, vinyet)
pp_text = None
unreal.EditorLoadingAndSavingUtils.load_map(ART)
for a in eas.get_all_level_actors():
    if a.get_class() == unreal.PostProcessVolume.static_class():
        pp_text = a.get_editor_property("settings").export_text()
        break

world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
world.get_world_settings().set_editor_property("default_game_mode", unreal.NLMenuGameMode.static_class())

CAM = V(-1000, 0, 180)
LH = V(2400, 700, 0)
r = random.Random(9)

# Deniz
mesh_actor("MENU_Sea", "/Engine/BasicShapes/Plane", V(0, 0, 0), scale=V(600, 600, 1), mats=[MATS + "M_NL_Sea"])

# Adacık: deniz fenerinin altında büyük düz kaya + çevresinde kayalar
rocks = ["SM_NL_RockA", "SM_NL_RockB", "SM_NL_RockC"]
mesh_actor("MENU_RockBase", "SM_NL_RockC", V(LH.x, LH.y, -15), yaw=20.0, scale=V(5.5, 5.0, 3.2))
mesh_actor("MENU_RockCabin", "SM_NL_RockA", V(LH.x - 560, LH.y + 120, -20), yaw=-60.0, scale=V(4.2, 3.6, 2.6))
for i in range(16):
    ang = r.uniform(0, 360)
    d = r.uniform(420, 1100)
    x, y = LH.x + math.cos(math.radians(ang)) * d, LH.y + math.sin(math.radians(ang)) * d
    s = r.uniform(1.2, 3.2)
    mesh_actor("MENU_Rock%d" % i, rocks[r.randrange(3)], V(x, y, -60 * s), yaw=r.uniform(0, 360), scale=V(s, s * r.uniform(0.8, 1.2), s * r.uniform(0.7, 1.1)))

# Deniz feneri (kapısı kameraya bakar)
door_yaw = math.degrees(math.atan2(CAM.y - LH.y, CAM.x - LH.x))
lh = eas.spawn_actor_from_class(unreal.NLLighthouse, V(LH.x, LH.y, 225), rot(yaw=door_yaw))
lh.set_actor_label("MENU_Lighthouse")
lh.set_editor_property("tower_height", 1050.0)
lh.set_editor_property("interior_props", False)
lh.set_editor_property("beam_pitch", 0.0)
lh.set_editor_property("beam_glow", 0.18)
lh.rebuild_now()

# Bekçi kulübesi: ön yüzü kameraya, penceresi ve kapı feneri yanar
dx, dy = CAM.x - (LH.x - 560), CAM.y - (LH.y + 120)
L = math.hypot(dx, dy)
cab_yaw = math.degrees(math.atan2(dx / L, -dy / L))
W = unreal.NLCabinWall
cab = eas.spawn_actor_from_class(unreal.NLLogCabin, V(LH.x - 560, LH.y + 120, 205), rot(yaw=cab_yaw))
cab.set_actor_label("MENU_KeeperCabin")
cab.set_editor_property("width", 380.0)
cab.set_editor_property("depth", 300.0)
cab.set_editor_property("wall_height", 240.0)
cab.set_editor_property("foundation_depth", 160.0)
cab.set_editor_property("porch_props", False)
cab.set_editor_property("openings", [
    opening(W.FRONT, -90.0, 100.0, 205.0, door_panel=True, lantern=1),
    opening(W.FRONT, 105.0, 70.0, 75.0, 100.0, window=True, lit=True),
    opening(W.LEFT, 0.0, 66.0, 72.0, 100.0, window=True, lit=True),
])
cab.rebuild_now()

# Çamlar (adacığın arkası)
scat = eas.spawn_actor_from_class(unreal.NLEnvScatter, V(0, 0, 0), rot())
scat.set_actor_label("MENU_Trees")
pines = [scat.get_editor_property(n) for n in ("pine_a", "pine_b", "pine_c", "dead_pine")]
for i in range(9):
    x = LH.x + r.uniform(250, 900)
    y = LH.y + r.uniform(-700, 700)
    s = r.uniform(0.5, 0.85)
    pines[r.randrange(4)].add_instance(unreal.Transform(V(x, y, r.uniform(80, 180)), rot(yaw=r.uniform(0, 360)), V(s, s, s)), True)

# Kamera (denizde)
cam_yaw = math.degrees(math.atan2(LH.y - CAM.y, LH.x - CAM.x)) - 17.0
director = eas.spawn_actor_from_class(unreal.NLMenuDirector, CAM, rot(pitch=6.0, yaw=cam_yaw))
director.set_actor_label("MENU_Director")
director.set_editor_property("hand_flashlight", False)
director.set_editor_property("indoor", False)
director.set_editor_property("lighthouse_aim_distance", 1600.0)

# Hava: yağmur, şimşek, gök gürültüsü
wd = eas.spawn_actor_from_class(unreal.NLWeatherDirector, V(0, 0, 0), rot())
wd.set_actor_label("MENU_Weather")
wd.set_editor_property("first_lightning_delay", 5.0)
wd.set_editor_property("lightning_gap_min", 14.0)
wd.set_editor_property("lightning_gap_max", 30.0)

# Ay ışığı (fenerin arkasından, silüet için)
moon = eas.spawn_actor_from_class(unreal.DirectionalLight, V(0, 0, 1000), rot(pitch=-28.0, yaw=200.0))
moon.set_actor_label("MENU_Moon")
mc = moon.get_component_by_class(unreal.DirectionalLightComponent)
mc.set_editor_property("intensity", 0.22)
mc.set_editor_property("light_color", unreal.Color(r=140, g=160, b=210, a=255))
mc.set_editor_property("volumetric_scattering_intensity", 0.3)

# Post process + sis (huzme sisin içinde görünsün)
ppv = eas.spawn_actor_from_class(unreal.PostProcessVolume, V(0, 0, 0), rot())
ppv.set_actor_label("MENU_PostProcess")
ppv.set_editor_property("unbound", True)
if pp_text:
    pp = unreal.PostProcessSettings()
    pp.import_text(pp_text)
    ppv.set_editor_property("settings", pp)
fog = eas.spawn_actor_from_class(unreal.ExponentialHeightFog, V(0, 0, 0), rot())
fog.set_actor_label("MENU_Fog")
fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
fc.set_editor_property("fog_density", 0.06)
fc.set_editor_property("fog_height_falloff", 0.12)
fc.set_editor_property("fog_inscattering_luminance", unreal.LinearColor(r=0.008, g=0.01, b=0.014, a=1.0))
fc.set_editor_property("enable_volumetric_fog", True)
fc.set_editor_property("volumetric_fog_extinction_scale", 1.6)
fc.set_editor_property("volumetric_fog_distance", 7000.0)

ok = unreal.EditorLoadingAndSavingUtils.save_map(world, MAP)
log("Kaydedildi: %s %s (fener kapi yonu %.0f, kamera yaw %.0f)" % (MAP, ok, door_yaw, cam_yaw))
with open(os.path.join(unreal.Paths.project_saved_dir(), "NLMenu.log"), "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
