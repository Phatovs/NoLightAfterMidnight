# No Light After Midnight - açılış sinematiği (botla rıhtıma yanaşma) için Lvl_Ada'ya NLIntroDirector koyar.
# Rıhtımın ("RIHTIM" etiketli kutu) konumu, yönü ve ölçüleri otomatik okunur.
# Çalıştırma: UnrealEditor-Cmd.exe NoLightAfterMidnight.uproject -run=pythonscript -script="<tam yol>\Scripts\nl_setup_intro.py"
import os
import unreal

MAP = "/Game/Levels/Lvl_Ada"
LOG = []
def log(m):
    LOG.append(m)
    unreal.log("NLSetupIntro: " + m)

unreal.EditorLoadingAndSavingUtils.load_map(MAP)
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = eas.get_all_level_actors()
dock = next((a for a in actors if a.get_actor_label() == "RIHTIM"), None)
sea = next((a for a in actors if a.get_actor_label() == "DENIZ"), None)
if not dock:
    log("UYARI: RIHTIM bulunamadi")
else:
    t = dock.get_actor_transform()
    sc = t.scale3d
    loc = t.translation
    yaw = t.rotation.rotator().yaw
    # Kutu 100 cm: uzun eksen hangisiyse sinematik onu kullanır; denize doğru olan yön adanın merkezinden uzaklaşan yöndür
    long_x = sc.x >= sc.y
    half_len = 50.0 * (sc.x if long_x else sc.y)
    half_w = 50.0 * (sc.y if long_x else sc.x)
    axis_yaw = yaw if long_x else yaw + 90.0
    import math
    d = unreal.Vector(math.cos(math.radians(axis_yaw)), math.sin(math.radians(axis_yaw)), 0)
    if d.x * loc.x + d.y * loc.y < 0:   # merkezden dışarı bakmıyorsa çevir
        axis_yaw += 180.0
    top = loc.z + 50.0 * sc.z
    water = 12.0
    if sea:
        so, se = sea.get_actor_bounds(False)
        water = so.z + se.z
    intro = next((a for a in actors if a.get_class() == unreal.NLIntroDirector.static_class()), None)
    if not intro:
        intro = eas.spawn_actor_from_class(unreal.NLIntroDirector, unreal.Vector(loc.x, loc.y, top), unreal.Rotator(0, 0, 0))
        log("Eklendi: NLIntroDirector")
    intro.set_actor_label("NLIntroDirector")
    intro.modify()
    intro.set_actor_location(unreal.Vector(loc.x, loc.y, top), False, False)
    intro.set_editor_property("dock_center", unreal.Vector(loc.x, loc.y, top))
    intro.set_editor_property("dock_yaw", ((axis_yaw + 180.0) % 360.0) - 180.0)
    intro.set_editor_property("dock_half_length", half_len)
    intro.set_editor_property("dock_half_width", half_w)
    intro.set_editor_property("water_z", water)
    log("Rihtim: merkez (%.0f, %.0f, %.0f), yon %.0f, yari boy %.0f, yari en %.0f, su %.1f" % (loc.x, loc.y, top, axis_yaw, half_len, half_w, water))

ok = unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
log("Kaydedildi: %s" % ok)
with open(os.path.join(unreal.Paths.project_saved_dir(), "NLSetupIntro.log"), "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
