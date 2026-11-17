# No Light After Midnight - Deniz fenerinin bekçi evine geçiş: eski kütük kulübeleri siler, feneri yeniden kurar,
# kullanılmayan eski modelleri (sandal, eski fener) siler.
# Önce: blender_lighthouse.py + nl_env_materials.py + nl_import_models.py (NL_MODELS=SM_NL_LH_Tower,SM_NL_LH_Gallery,SM_NL_LH_LanternRoom,SM_NL_LH_House)
# Çalıştırma (editör kapalıyken): UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<tam yol>\Scripts\nl_lighthouse_house.py"
import os
import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
eal = unreal.EditorAssetLibrary
LOG = []


def log(m):
    LOG.append(m)
    unreal.log("NLHouse: " + m)


def touch(a):
    a.modify()
    for c in a.get_components_by_class(unreal.ActorComponent):
        c.modify()


for level, cabin, yaw in (("/Game/Levels/Lvl_Ada_Art", "NL_ENV_KeeperCabin", None), ("/Game/Levels/Lvl_MainMenu", "MENU_KeeperCabin", 100.0)):
    if not les.load_level(level):
        log("UYARI: acilamadi " + level)
        continue
    for a in sub.get_all_level_actors():
        if a.get_actor_label() == cabin:
            sub.destroy_actor(a)
            log("%s: %s silindi" % (level, cabin))
            break
    for a in sub.get_all_level_actors():
        if a.get_class().get_name() != "NLLighthouse":
            continue
        touch(a)
        if yaw is not None:
            # Menü: ev kameradan bakınca kulenin sağında kalsın
            r = a.get_actor_rotation()
            a.set_actor_rotation(unreal.Rotator(roll=r.roll, pitch=r.pitch, yaw=yaw), False)
        a.set_editor_property("build_house", True)
        a.set_editor_property("door_panel", False)
        a.rebuild_now()
        log("%s: %s yeniden kuruldu (yaw %.0f)" % (level, a.get_actor_label(), a.get_actor_rotation().yaw))
    les.save_current_level()

# Kullanılmayan eski modeller: başka bir asset kullanmıyorsa sil
ar = unreal.AssetRegistryHelpers.get_asset_registry()
opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
for p in ("/Game/Environment/Kit/SM_NL_Boat", "/Game/Environment/Kit/SM_NL_Outboard", "/Game/Environment/Kit/SM_NL_Oar", "/Game/Environment/Kit/SM_NL_Lantern"):
    if not eal.does_asset_exist(p):
        continue
    refs = [str(r) for r in (ar.get_referencers(p, opts) or []) if str(r) != p]
    if refs:
        log("silinmedi (kullaniliyor): %s <- %s" % (p, refs))
        continue
    eal.delete_asset(p)
    log("silindi: " + p)

with open(os.path.join(unreal.Paths.project_saved_dir(), "NLHouse.log"), "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
