# No Light After Midnight - Blender'da üretilen sigorta kutusu FBX'lerini içe aktarır -> /Game/Environment/Kit
# Önce: blender.exe -b --factory-startup --python Scripts/blender_fusebox.py -- <proje>/Saved/NLFuse
#        ve Scripts/nl_env_materials.py (MI_NL_FuseCabinet vb. malzemeler)
# Çalıştırma (editör kapalıyken): UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<tam yol>\Scripts\nl_import_fusebox.py"
import os
import unreal

SRC = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "NLFuse")
DEST = "/Game/Environment/Kit"
MAT = "/Game/Environment/Materials/"
eal = unreal.EditorAssetLibrary
LOG = []


def log(m):
    LOG.append(m)
    unreal.log("NLFuseImport: " + m)


def material_for(slot):
    s = str(slot)
    if s.startswith("MI_NL_FuseBand"):
        return unreal.load_asset("/Engine/BasicShapes/BasicShapeMaterial")   # rengi C++ MID verir
    m = unreal.load_asset(MAT + s) if eal.does_asset_exist(MAT + s) else None
    if not m:
        log("UYARI: malzeme yok: " + s)
    return m


def import_mesh(name, collision):
    fbx = os.path.join(SRC, name + ".fbx")
    if not os.path.exists(fbx):
        log("HATA: " + fbx + " yok")
        return
    ui = unreal.FbxImportUI()
    ui.import_mesh = True
    ui.import_as_skeletal = False
    ui.import_materials = False
    ui.import_textures = False
    ui.import_animations = False
    ui.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    sm = ui.static_mesh_import_data
    sm.combine_meshes = True
    sm.auto_generate_collision = collision
    sm.generate_lightmap_u_vs = False
    sm.normal_import_method = unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS
    task = unreal.AssetImportTask()
    task.filename = fbx
    task.destination_path = DEST
    task.destination_name = name
    task.automated = True
    task.replace_existing = True
    task.save = False
    task.options = ui
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.load_asset(DEST + "/" + name)
    if not mesh:
        log("HATA: " + name + " içe aktarılamadı")
        return
    slots = mesh.get_editor_property("static_materials")
    new = []
    for s in slots:
        nm = str(s.get_editor_property("material_slot_name"))
        s.set_editor_property("material_interface", material_for(nm))
        new.append(s)
    mesh.set_editor_property("static_materials", new)
    if not collision:
        unreal.EditorStaticMeshLibrary.remove_collisions(mesh)
    eal.save_loaded_asset(mesh)
    b = mesh.get_bounding_box()
    log("%s: min(%.1f, %.1f, %.1f) max(%.1f, %.1f, %.1f) slots=%s" % (name, b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z,
                                                                   [str(s.get_editor_property("material_slot_name")) for s in new]))


import_mesh("SM_NL_FuseCabinet", True)
for n in ("SM_NL_FuseCartridge", "SM_NL_FuseLever", "SM_NL_GaugeNeedle", "SM_NL_FuseLED"):
    import_mesh(n, False)

with open(os.path.join(unreal.Paths.project_saved_dir(), "NLFuseImport.log"), "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
