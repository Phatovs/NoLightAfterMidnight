# No Light After Midnight - Deniz fenerini ve bekçi evini araziye oturtur.
# Fener + ev tabanının altındaki araziyi ölçer, zemini (kule zemin katı = ev döşemesi) en yüksek noktanın biraz üstüne alır
# (taş temel aşağı iner, boşluk kalmaz). Evin / kulenin içindeki aktörler (masa, şarj istasyonu, başlangıç noktası, toz) aynı
# miktarda kaydırılır. Çalıştırma (editör kapalıyken): UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<tam yol>\Scripts\nl_lighthouse_ground.py"
import math
import os
import unreal

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
LOG = []
CLEAR = 6.0   # zemin, en yüksek arazinin bu kadar üstünde


def log(m):
    LOG.append(m)
    unreal.log("NLGround: " + m)


def footprint():
    """Kule yereli (X eve doğru) örnek noktalar: kule diski + ev + giriş çıkıntısı + basamaklar"""
    pts = []
    for r in (0.0, 90.0, 180.0, 270.0):
        n = max(1, int(r / 30.0))
        for k in range(n * 2):
            a = math.pi * 2 * k / (n * 2)
            pts.append((r * math.cos(a), r * math.sin(a)))
    for x in range(240, 821, 40):
        for y in range(-200, 201, 40):
            pts.append((float(x), float(y)))
    for x in range(560, 781, 40):
        pts.append((float(x), -240.0))
    return pts


def ground_z(world, ignore, p):
    hits = unreal.SystemLibrary.line_trace_multi(world, unreal.Vector(p.x, p.y, p.z + 3000), unreal.Vector(p.x, p.y, p.z - 3000),
                                                 unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignore, unreal.DrawDebugTrace.NONE, True)
    for h in hits or []:
        t = h.to_tuple()
        actor = t[9] if len(t) > 9 else None
        if actor and "Landscape" in actor.get_class().get_name():
            return t[5].z   # ImpactPoint
    return None


for level in ("/Game/Levels/Lvl_Ada_Art", "/Game/Levels/Lvl_MainMenu"):
    if not les.load_level(level):
        continue
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    actors = sub.get_all_level_actors()
    for lh in [a for a in actors if a.get_class().get_name() == "NLLighthouse"]:
        X = lh.get_actor_transform()
        sz = lh.get_editor_property("tower_height") / 1000.0
        floor = X.translation.z + 40.0 * sz
        ignore = [a for a in actors if "Landscape" not in a.get_class().get_name()]
        zs = []
        for (lx, ly) in footprint():
            p = X.transform_location(unreal.Vector(lx, ly, 0.0))
            z = ground_z(world, ignore, unreal.Vector(p.x, p.y, floor))
            if z is not None:
                zs.append(z)
        if not zs:
            log("%s: arazi bulunamadi (kaya/mesh uzerinde olabilir), dokunulmadi" % level)
            continue
        zmax, zmin = max(zs), min(zs)
        delta = (zmax + CLEAR) - floor
        log("%s: zemin %.0f, arazi %.0f..%.0f (%d nokta) -> kaydirma %.1f; temel derinligi gereken %.0f" % (level, floor, zmin, zmax, len(zs), delta, (zmax + CLEAR) - zmin))
        if abs(delta) < 1.0:
            continue
        # İçerideki aktörleri de kaydır (fener yerelinde ev/kule alanında ve zemin yakınında olanlar)
        inv = X.inverse()
        moved = []
        for a in actors:
            if a == lh or "Landscape" in a.get_class().get_name():
                continue
            loc = a.get_actor_location()
            l = inv.transform_location(loc)
            in_house = 230 <= l.x <= 830 and -250 <= l.y <= 220
            in_tower = math.hypot(l.x, l.y) < 250
            if (in_house or in_tower) and floor - 60 <= loc.z <= floor + 450:
                a.modify()
                a.set_actor_location(unreal.Vector(loc.x, loc.y, loc.z + delta), False, False)
                moved.append(a.get_actor_label())
        lh.modify()
        lh.set_actor_location(unreal.Vector(X.translation.x, X.translation.y, X.translation.z + delta), False, False)
        lh.rebuild_now()
        log("%s: fener %+.0f cm, kaydirilanlar: %s" % (level, delta, ", ".join(moved)))
    les.save_current_level()

with open(os.path.join(unreal.Paths.project_saved_dir(), "NLGround.log"), "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
