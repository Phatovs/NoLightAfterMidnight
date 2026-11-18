# No Light After Midnight - Blender'da üretilen modelleri içe aktarır -> /Game/Environment/Kit
# Önce ilgili Blender script'lerini Saved/NLProps klasörüne çalıştır:
#   blender_props.py (bidon, yol feneri), blender_old_generator.py (jeneratör), blender_trawler.py (tekne)
# ve Scripts/nl_env_materials.py (malzemeler).
# Çalıştırma (editör kapalıyken): UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<tam yol>\Scripts\nl_import_models.py"
# Adadaki NLGenerator da SM_NL_OldGenerator modeline geçer; eski sarı jeneratör (SM_NL_Generator) silinir.
import os
import unreal

SRC = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "NLProps")
DEST = "/Game/Environment/Kit"
MAT = "/Game/Environment/Materials/"
eal = unreal.EditorAssetLibrary
LOG = []

# (mesh adı, çarpışma)
MODELS = [
    ("SM_NL_JerryCan", True),
    ("SM_NL_GuidePost", True),
    ("SM_NL_GuideLantern", False),
    ("SM_NL_LanternFlame", False),
    ("SM_NL_OldGenerator", True),
    ("SM_NL_Trawler", False),
    ("SM_NL_TrawlerWheel", False),
    ("SM_NL_LH_Tower", True),
    ("SM_NL_LH_Gallery", True),
    ("SM_NL_LH_LanternRoom", True),
    ("SM_NL_LH_House", True),
    ("SM_NL_LH_DoorHouse", True),
    ("SM_NL_LH_DoorTower", True),
    ("SM_NL_Heater", True),
]
COMPLEX = ("SM_NL_LH_Tower", "SM_NL_LH_Gallery", "SM_NL_LH_LanternRoom", "SM_NL_LH_House")
only = [s.strip() for s in os.environ.get("NL_MODELS", "").split(",") if s.strip()]


def log(m):
    LOG.append(m)
    unreal.log("NLImport: " + m)


def material_for(slot):
    s = str(slot)
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
    slots = []
    for s in mesh.get_editor_property("static_materials"):
        s.set_editor_property("material_interface", material_for(s.get_editor_property("material_slot_name")))
        slots.append(s)
    mesh.set_editor_property("static_materials", slots)
    if not collision:
        unreal.EditorStaticMeshLibrary.remove_collisions(mesh)
    if name in COMPLEX:
        # İçine girilen kule: çarpışma mesh'in kendi şekli (kapı ve iç oda açık)
        mesh.get_editor_property("body_setup").set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    eal.save_loaded_asset(mesh)
    b = mesh.get_bounding_box()
    log("%s: min(%.0f, %.0f, %.0f) max(%.0f, %.0f, %.0f) slots=%d" % (name, b.min.x, b.min.y, b.min.z, b.max.x, b.max.y, b.max.z, len(slots)))


for n, c in MODELS:
    if only and n not in only:
        continue
    import_mesh(n, c)

# Adadaki jeneratör -> eski tip model
if (not only or "SM_NL_OldGenerator" in only) and os.environ.get("NL_SKIP_LEVEL") != "1":
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    gen_mesh = unreal.load_asset(DEST + "/SM_NL_OldGenerator")
    if les.load_level("/Game/Levels/Lvl_Ada_Art"):
        for a in sub.get_all_level_actors():
            if a.get_class().get_name() != "NLGenerator":
                continue
            a.modify()
            c = a.get_component_by_class(unreal.StaticMeshComponent)
            c.modify()
            old = c.static_mesh.get_name() if c.static_mesh else "-"
            c.set_editor_property("static_mesh", gen_mesh)
            c.set_editor_property("override_materials", [])
            c.set_editor_property("relative_scale3d", unreal.Vector(1, 1, 1))
            # Benzin ağzı: yakıt deposunun kırmızı kapağı (-Y, oyuncu tarafı)
            a.set_editor_property("fuel_inlet_offset", unreal.Vector(18, -66, 82))
            log("Lvl_Ada_Art: jenerator %s -> SM_NL_OldGenerator at %s" % (old, a.get_actor_location()))
        les.save_current_level()
    if eal.does_asset_exist(DEST + "/SM_NL_Generator"):
        eal.delete_asset(DEST + "/SM_NL_Generator")
        log("silindi: SM_NL_Generator (sari jenerator)")
    for old_mi in ("MI_NL_GenPaint", "MI_NL_GenSkid"):
        if eal.does_asset_exist(MAT + old_mi):
            eal.delete_asset(MAT + old_mi)
            log("silindi: " + old_mi)

with open(os.path.join(unreal.Paths.project_saved_dir(), "NLImport.log"), "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
