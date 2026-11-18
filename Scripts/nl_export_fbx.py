# No Light After Midnight - Unreal'dan FBX dışa aktarma (Blender'a almak için)
#  - Rıhtım kulübesi (NL_ENV_DockCabin, kod ile kurulan modüler yapı): tüm mesh / örnek (instance) parçaları kulübenin
#    yerel koordinatında tek bir mesh'e birleştirilir (malzemeler korunur), geçici bir asset olarak FBX'e yazılır, asset silinir.
#  - Deniz feneri lensi ve taş kaidesi (kit parçaları)
# Level kaydedilmez. Çıktı: Saved/NLExport/*.fbx
# Çalıştırma (editör kapalıyken): UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<tam yol>\Scripts\nl_export_fbx.py"
import os
import unreal

OUT = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "NLExport")
os.makedirs(OUT, exist_ok=True)
G = unreal.GeometryScript_AssetUtils
ED = unreal.GeometryScript_MeshEdits
MAT = unreal.GeometryScript_Materials
eal = unreal.EditorAssetLibrary
LOG = []


def log(m):
    LOG.append(m)
    unreal.log("NLExport: " + m)


def export(obj, path):
    t = unreal.AssetExportTask()
    t.object = obj
    t.filename = path
    t.automated = True
    t.prompt = False
    t.replace_identical = True
    t.exporter = unreal.StaticMeshExporterFBX()
    ok = unreal.Exporter.run_asset_export_task(t)
    log("%s -> %s (%s)" % (obj.get_name(), path, "ok" if ok and os.path.exists(path) else "HATA"))


def merge_actor(actor, name):
    """Aktörün bütün (instanced) static mesh parçalarını aktör yerelinde tek mesh'e birleştirir"""
    inv = actor.get_actor_transform().inverse()
    dst = unreal.DynamicMesh()
    mats = []
    count = 0
    opts = unreal.GeometryScriptCopyMeshFromAssetOptions()
    for comp in actor.get_components_by_class(unreal.StaticMeshComponent):
        sm = comp.static_mesh
        if not sm or not comp.is_visible():
            continue
        src = unreal.DynamicMesh()
        res = G.copy_mesh_from_static_mesh(sm, src, opts, unreal.GeometryScriptMeshReadLOD())
        if isinstance(res, tuple):
            res = res[-1] if not isinstance(res[-1], unreal.DynamicMesh) else res[0]
        # Malzeme kimliklerini ortak listeye taşı
        comp_mats = comp.get_materials()
        ids = []
        for i, m in enumerate(comp_mats):
            if m not in mats:
                mats.append(m)
            ids.append(mats.index(m))
        for i in range(len(ids) - 1, -1, -1):
            MAT.remap_material_i_ds(src, i, 1000 + ids[i])
        for i in range(len(ids)):
            MAT.remap_material_i_ds(src, 1000 + ids[i], ids[i])
        if isinstance(comp, unreal.InstancedStaticMeshComponent):
            for k in range(comp.get_instance_count()):
                t = comp.get_instance_transform(k, True)
                ED.append_mesh(dst, src, t * inv)
                count += 1
        else:
            ED.append_mesh(dst, src, comp.get_world_transform() * inv)
            count += 1
    path = "/Game/Temp_NLExport/" + name
    o = unreal.GeometryScriptCreateNewStaticMeshAssetOptions()
    o.enable_recompute_normals = False
    o.enable_nanite = False
    o.enable_collision = False
    res = unreal.GeometryScript_NewAssetUtils.create_new_static_mesh_asset_from_mesh(dst, path, o)
    sm = res[0] if isinstance(res, tuple) else res
    slots = []
    for i, m in enumerate(mats):
        s = unreal.StaticMaterial()
        s.set_editor_property("material_interface", m)
        s.set_editor_property("material_slot_name", m.get_name() if m else "Slot%d" % i)
        slots.append(s)
    sm.set_editor_property("static_materials", slots)
    log("%s: %d parca, %d malzeme" % (name, count, len(mats)))
    return sm


les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
les.load_level("/Game/Levels/Lvl_Ada_Art")
cabin = next((a for a in sub.get_all_level_actors() if a.get_actor_label() == "NL_ENV_DockCabin"), None)
if cabin:
    sm = merge_actor(cabin, "SM_DockCabin_Export")
    export(sm, os.path.join(OUT, "DockCabin.fbx"))
    eal.delete_directory("/Game/Temp_NLExport")
else:
    log("HATA: NL_ENV_DockCabin yok")
for name in ("SM_NL_LH_Lens", "SM_NL_LH_Base"):
    m = unreal.load_asset("/Game/Environment/Kit/" + name)
    if m:
        export(m, os.path.join(OUT, name + ".fbx"))

with open(os.path.join(unreal.Paths.project_saved_dir(), "NLExport.log"), "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
