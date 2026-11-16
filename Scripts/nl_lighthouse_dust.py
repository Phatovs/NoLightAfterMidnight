# No Light After Midnight - Lvl_Ada'ya dönen fener ışığı (eski kulenin merceğinde) ve iç mekânlara uçuşan toz.
# Tekrar çalıştırılabilir. Lvl_Ada_Art için nl_dress_ada.py bunları kopyalayıp yeni deniz fenerine çevirir.
# Çalıştırma: UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<tam yol>\Scripts\nl_lighthouse_dust.py"
import os
import unreal

MAP = os.environ.get("NL_LEVEL", "/Game/Levels/Lvl_Ada")
LOG = []
V = unreal.Vector
eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(m):
    LOG.append(m)
    unreal.log("[NLLighthouse] " + m)


def by_label(label):
    for a in eas.get_all_level_actors():
        if a.get_actor_label() == label:
            return a
    return None


def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def trace_z(x, y, z0, z1):
    hit = unreal.SystemLibrary.line_trace_single(world(), V(x, y, z0), V(x, y, z1), unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
    if not hit:
        return None
    t = hit.to_tuple()
    return t[5].z if t[0] else None


def add_dust(label, center, extent, yaw=0.0, density=6.0, seed=1):
    a = eas.spawn_actor_from_class(unreal.NLDustVolume, center, unreal.Rotator(roll=0, pitch=0, yaw=yaw))
    a.set_actor_label(label)
    a.set_editor_property("extent", extent)
    a.set_editor_property("density", density)
    a.set_editor_property("seed", seed)
    a.rebuild_now()
    log("Toz: %s (%.0f x %.0f x %.0f)" % (label, extent.x * 2, extent.y * 2, extent.z * 2))
    return a


def room_dust(label, building, seed, shrink=40.0):
    """Binanın içine: tabanı ve tavanı merkezden ışınla bul, sınırların biraz içine toz koy."""
    a = by_label(building)
    if not a:
        log("UYARI: bina yok " + building)
        return None
    o, e = a.get_actor_bounds(False)
    floor = trace_z(o.x, o.y, o.z + 50.0, o.z - e.z - 50.0)
    ceil = trace_z(o.x, o.y, (floor or o.z) + 30.0, o.z + e.z + 50.0)
    if floor is None:
        floor = o.z - e.z
    if ceil is None or ceil - floor < 120.0:
        ceil = floor + 260.0
    h = (ceil - floor) * 0.5
    return add_dust(label, V(o.x, o.y, floor + h), V(max(60.0, e.x - shrink), max(60.0, e.y - shrink), max(50.0, h - 15.0)), 0.0, 45.0, seed)


unreal.EditorLoadingAndSavingUtils.load_map(MAP)
for a in eas.get_all_level_actors():
    l = a.get_actor_label()
    if l.startswith("NL_LH_") or l.startswith("NL_DUST_"):
        eas.destroy_actor(a)

# Dönen ışık: eski kulenin merceğinde (blockout kulesi yerinde kalır)
lens = by_label("FENER_MERCEGI")
if lens:
    o, e = lens.get_actor_bounds(False)
    lh = eas.spawn_actor_from_class(unreal.NLLighthouse, V(o.x, o.y, o.z + 20.0), unreal.Rotator(roll=0, pitch=0, yaw=0))
    lh.set_actor_label("NL_LH_Lighthouse")
    lh.set_editor_property("build_tower", False)
    lh.rebuild_now()
    log("Fener isigi: (%.0f, %.0f, %.0f)" % (o.x, o.y, o.z + 20.0))

room_dust("NL_DUST_ControlRoom", "FENER_KONTROL_BINASI", 11)
room_dust("NL_DUST_LivingArea", "FENER_YASAM_ALANI", 12)
room_dust("NL_DUST_Generator", "JENERATOR_ANA", 13)

# Güvenlik masası: ahşap masa + tüplü monitör (ekran = kamera görüntüsü, yuva 1). Sandalye C++ varsayılanında.
KIT = "/Game/Environment/Kit/"
MATS = "/Game/Environment/Materials/"
for a in eas.get_all_level_actors():
    if a.get_class().get_name() == "NLSecurityDesk":
        for c in a.get_components_by_class(unreal.StaticMeshComponent):
            n = c.get_name()
            if n == "DeskMesh":
                c.set_editor_property("static_mesh", unreal.load_asset(KIT + "SM_NL_Desk"))
                c.set_editor_property("relative_location", V(10, 0, 37.5))
                c.set_editor_property("relative_scale3d", V(1, 1, 1))
                c.set_material(0, unreal.load_asset(MATS + "MI_NL_Wood"))
            elif n == "MonitorMesh":
                c.set_editor_property("static_mesh", unreal.load_asset(KIT + "SM_NL_CRT"))
                c.set_editor_property("relative_location", V(-5, 0, 109))
                c.set_editor_property("relative_scale3d", V(1, 1, 1))
                c.set_material(0, unreal.load_asset(MATS + "MI_NL_Plastic"))
                c.set_material(1, unreal.load_asset("/Game/Mechanics/M_NLCameraMonitor"))
        a.set_editor_property("monitor_material_index", 1)
        log("Masa: ahsap masa + CRT monitor")
    elif a.get_class().get_name() == "NLFuelCan":
        o, e = a.get_actor_bounds(False)
        a.set_actor_scale3d(V(1, 1, 1))
        a.set_actor_location(V(o.x, o.y, o.z - e.z), False, False)
        c = a.get_component_by_class(unreal.StaticMeshComponent)
        c.set_editor_property("static_mesh", unreal.load_asset(KIT + "SM_NL_JerryCan"))
        c.set_material(0, unreal.load_asset(MATS + "MI_NL_JerryRed"))
        c.set_material(1, unreal.load_asset(MATS + "MI_NL_Metal"))
        log("Benzin bidonu modeli: %s" % a.get_actor_label())

ok = unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
log("Kaydedildi: %s" % ok)
with open(os.path.join(unreal.Paths.project_saved_dir(), "NLLighthouseDust.log"), "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
