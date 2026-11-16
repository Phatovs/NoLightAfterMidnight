# Bir level'deki aktörleri (sınıf, isim, konum, boyut) Saved/level_dump.txt dosyasına yazar.
# Kullanım: UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="Scripts/nl_inspect_level.py"
import os
import unreal

LEVEL = os.environ.get("NL_LEVEL", "/Game/AdaBlockout_YeniV2")
out_path = os.path.join(unreal.Paths.project_saved_dir(), "level_dump_" + LEVEL.split("/")[-1] + ".txt")

unreal.EditorLoadingAndSavingUtils.load_map(LEVEL)
actors = unreal.EditorLevelLibrary.get_all_level_actors()
lines = []
for a in actors:
    try:
        origin, ext = a.get_actor_bounds(False)
    except Exception:
        origin, ext = unreal.Vector(), unreal.Vector()
    loc = a.get_actor_location()
    rot = a.get_actor_rotation()
    extra = ""
    for comp in a.get_components_by_class(unreal.StaticMeshComponent):
        sm = comp.static_mesh
        if sm:
            extra += " mesh=" + sm.get_path_name().split(".")[-1]
    lines.append("%s | %s | loc=(%.0f,%.0f,%.0f) yaw=%.0f | ext=(%.0f,%.0f,%.0f)%s" % (
        a.get_class().get_name(), a.get_actor_label(), loc.x, loc.y, loc.z, rot.yaw, ext.x, ext.y, ext.z, extra[:200]))
lines.sort()
ws = unreal.EditorLevelLibrary.get_editor_world().get_world_settings()
lines.insert(0, "GameMode override: %s" % ws.get_editor_property("default_game_mode"))
with open(out_path, "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
unreal.log("NLInspect: %d actors -> %s" % (len(lines), out_path))
