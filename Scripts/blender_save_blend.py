# No Light After Midnight - Bir model script'ini çalıştırıp sonucu .blend olarak kaydeder (paylaşmak / Blender'da düzenlemek için)
# Çalıştırma:
#   blender.exe -b --factory-startup --python Scripts/blender_save_blend.py -- <model script> <çıktı .blend> [keep=Ad1,Ad2] [place=Ad:x,y,z;Ad2:x,y,z]
# keep : sadece bu nesneler kalır (boşsa hepsi)
# place: nesneyi Unreal koordinatına (cm) taşır, isteğe bağlı yaw: Ad:x,y,z[,yaw] (Unreal X, Y, Z -> Blender x, -y, z)
# Malzemeler: script'lerdeki önizleme renkleri Principled BSDF taban rengine aktarılır (Malzeme görünümünde renkli görünsün).
import os
import sys
import tempfile

import bpy

argv = sys.argv[sys.argv.index("--") + 1:]
script, out_blend = argv[0], argv[1]
opts = dict(a.split("=", 1) for a in argv[2:] if "=" in a)

# Model script'i kendi çıktı klasörüne (geçici) FBX yazar; önizleme yerleşimi için NL_PREVIEW açık
tmp = tempfile.mkdtemp(prefix="nlblend_")
os.environ["NL_PREVIEW"] = "1"
os.environ["NL_FUSE_PREVIEW"] = "1"
sys.argv = [sys.argv[0], "--", tmp]
g = {"__file__": os.path.abspath(script), "__name__": "__main__"}
exec(compile(open(script, encoding="utf-8").read(), script, "exec"), g)

# Kamera, ışık vb. önizleme nesnelerini ve istenmeyenleri sil
keep = [k for k in opts.get("keep", "").split(",") if k]
for ob in list(bpy.data.objects):
    if ob.type != 'MESH' or (keep and ob.name not in keep):
        bpy.data.objects.remove(ob, do_unlink=True)
# Kopyalar (önizlemede çoğaltılan sigortalar vb.) aynı mesh'i paylaşır; hepsi kalsın
# dropfar=1: önizlemede uzağa (50 m+) kaldırılmış şablon nesneleri sil
if opts.get("dropfar") == "1":
    for ob in list(bpy.data.objects):
        if ob.location.length > 50.0:
            bpy.data.objects.remove(ob, do_unlink=True)
for spec in [s for s in opts.get("place", "").split(";") if s]:
    name, xyz = spec.split(":")
    v = [float(t) for t in xyz.split(",")]
    if name in bpy.data.objects:
        ob = bpy.data.objects[name]
        ob.location = (v[0] * 0.01, -v[1] * 0.01, v[2] * 0.01)
        if len(v) > 3:
            import math
            ob.rotation_euler = (0.0, 0.0, math.radians(-v[3]))   # Unreal yaw

for m in bpy.data.materials:
    c = tuple(m.diffuse_color)
    m.use_nodes = True
    bsdf = m.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = c
        if "Glow" in m.name or "Flame" in m.name or "Led" in m.name or "Lamp" in m.name or "LensGlow" in m.name:
            if "Emission Color" in bsdf.inputs:
                bsdf.inputs["Emission Color"].default_value = c
                bsdf.inputs["Emission Strength"].default_value = 2.0
bpy.context.scene.unit_settings.system = 'METRIC'
os.makedirs(os.path.dirname(os.path.abspath(out_blend)), exist_ok=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.abspath(out_blend), compress=True)
print("NLBlend: saved", out_blend, "objects:", [o.name for o in bpy.data.objects])
