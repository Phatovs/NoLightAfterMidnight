# No Light After Midnight - çevre materyalleri -> /Game/Environment/Materials
#  M_NL_Surface   : renk + normal dokusu, ton, pürüzlülük, UV ölçeği (kit meshleri)
#  M_NL_World     : dünyaya hizalı (3 yönden yansıtma) - UV'si olmayan/ölçeklenmiş kutular, deniz feneri, jeneratör
#  M_NL_Landscape : orman zemini + patika çamuru (DOKU_Patika maskesi) + eğimde kaya + kıyıda ıslak kum
#  M_NL_Sea       : koyu, parlak deniz; iki katman kayan dalga normali
#  M_NL_Glow      : pencere/fener camı (sıcak ışık), aynı materyal ışıksız koyu cam için de
# Çalıştırma: UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="<tam yol>\Scripts\nl_env_materials.py"
import os
import unreal

DEST = "/Game/Environment/Materials"
TEX = "/Game/Environment/Textures/"
mel = unreal.MaterialEditingLibrary
at = unreal.AssetToolsHelpers.get_asset_tools()
eal = unreal.EditorAssetLibrary
LOG = []


def log(m):
    LOG.append(m)
    unreal.log("NLMat: " + m)


def tex(name):
    return unreal.load_asset(TEX + name)


class Exists(Exception):
    def __init__(self, m):
        self.m = m


# Var olan ana materyaller yeniden kurulmaz (yüklü materyalin düğümlerini silmek editörü çökertiyor).
# Yeniden kurmak için önce assetin kendisini silin.
def material(name):
    path = DEST + "/" + name
    m = unreal.load_asset(path) if eal.does_asset_exist(path) else None
    if m:
        raise Exists(m)
    else:
        m = at.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("used_with_instanced_static_meshes", True)
    return m


def E(m, cls, x, y, **props):
    e = mel.create_material_expression(m, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def C(a, out, b, inp):
    if not mel.connect_material_expressions(a, out, b, inp):
        log("UYARI: baglanamadi %s.%s -> %s.%s" % (a.get_name(), out, b.get_name(), inp))


def P(a, out, prop):
    mel.connect_material_property(a, out, prop)


def scalar(m, name, val, x, y):
    return E(m, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=val)


def vector(m, name, col, x, y):
    return E(m, unreal.MaterialExpressionVectorParameter, x, y, parameter_name=name, default_value=unreal.LinearColor(*col))


def const(m, v, x, y):
    return E(m, unreal.MaterialExpressionConstant, x, y, r=v)


def const3(m, v, x, y):
    return E(m, unreal.MaterialExpressionConstant3Vector, x, y, constant=unreal.LinearColor(v[0], v[1], v[2], 1))


def mul(m, a, ao, b, bo, x, y):
    e = E(m, unreal.MaterialExpressionMultiply, x, y)
    C(a, ao, e, "A")
    C(b, bo, e, "B")
    return e


def add(m, a, ao, b, bo, x, y):
    e = E(m, unreal.MaterialExpressionAdd, x, y)
    C(a, ao, e, "A")
    C(b, bo, e, "B")
    return e


def lerp(m, a, ao, b, bo, alpha, alo, x, y):
    e = E(m, unreal.MaterialExpressionLinearInterpolate, x, y)
    C(a, ao, e, "A")
    C(b, bo, e, "B")
    C(alpha, alo, e, "Alpha")
    return e


def sample(m, texture, uv, uvo, x, y, normal=False, param=None):
    cls = unreal.MaterialExpressionTextureSampleParameter2D if param else unreal.MaterialExpressionTextureSample
    e = E(m, cls, x, y)
    if param:
        e.set_editor_property("parameter_name", param)
    if normal:
        e.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    e.set_editor_property("texture", texture)
    if uv is not None:
        C(uv, uvo, e, "UVs")
    return e


def build(fn):
    try:
        return fn()
    except Exists as e:
        log("var, atlandi: " + e.m.get_name())
        return e.m


def finish(m):
    mel.layout_material_expressions(m)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    log("materyal: " + m.get_name())


# ------------------------------------------------------------------ M_NL_Surface

def make_surface():
    m = material("M_NL_Surface")
    tc = E(m, unreal.MaterialExpressionTextureCoordinate, -1200, 0)
    uvs = scalar(m, "UVScale", 1.0, -1200, 120)
    uv = mul(m, tc, "", uvs, "", -1000, 40)
    base = sample(m, tex("T_NL_Grain_C"), uv, "", -800, -200, param="BaseTex")
    nrm = sample(m, tex("T_NL_Grain_N"), uv, "", -800, 150, normal=True, param="NormalTex")
    tint = vector(m, "Tint", (1, 1, 1), -800, -400)
    col = mul(m, base, "RGB", tint, "", -500, -250)
    flat = const3(m, (0, 0, 1), -800, 400)
    ns = scalar(m, "NormalStrength", 1.0, -800, 500)
    n = lerp(m, flat, "", nrm, "RGB", ns, "", -500, 200)
    rough = scalar(m, "Roughness", 0.6, -500, 50)
    P(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    P(n, "", unreal.MaterialProperty.MP_NORMAL)
    P(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    finish(m)
    return m


# ------------------------------------------------------------------ M_NL_World (3 yönlü)

def make_world():
    m = material("M_NL_World")
    wp = E(m, unreal.MaterialExpressionWorldPosition, -1600, 0)
    size = scalar(m, "WorldSize", 220.0, -1600, 150)
    p = E(m, unreal.MaterialExpressionDivide, -1400, 50)
    C(wp, "", p, "A")
    C(size, "", p, "B")
    texobj = E(m, unreal.MaterialExpressionTextureObjectParameter, -1400, -300, parameter_name="BaseTex", texture=tex("T_NL_Plaster_C"))
    nrm = E(m, unreal.MaterialExpressionVertexNormalWS, -1400, 400)
    ab = E(m, unreal.MaterialExpressionAbs, -1250, 400)
    C(nrm, "", ab, "")
    sq = mul(m, ab, "", ab, "", -1100, 400)
    w4 = mul(m, sq, "", sq, "", -950, 400)
    masks = {}
    for i, (ch, rg) in enumerate((("X", (False, True, True)), ("Y", (True, False, True)), ("Z", (True, True, False)))):
        mk = E(m, unreal.MaterialExpressionComponentMask, -1200, -100 + i * 120, r=rg[0], g=rg[1], b=rg[2], a=False)
        C(p, "", mk, "")
        s = E(m, unreal.MaterialExpressionTextureSample, -1000, -150 + i * 160)
        C(mk, "", s, "UVs")
        C(texobj, "", s, "Tex")
        wm = E(m, unreal.MaterialExpressionComponentMask, -950, 520 + i * 60, r=(ch == "X"), g=(ch == "Y"), b=(ch == "Z"), a=False)
        C(w4, "", wm, "")
        masks[ch] = (s, wm)
    # ağırlıklı toplam / ağırlık toplamı
    wsum = add(m, add(m, masks["X"][1], "", masks["Y"][1], "", -800, 520), "", masks["Z"][1], "", -650, 520)
    cx = mul(m, masks["X"][0], "RGB", masks["X"][1], "", -750, -150)
    cy = mul(m, masks["Y"][0], "RGB", masks["Y"][1], "", -750, 0)
    cz = mul(m, masks["Z"][0], "RGB", masks["Z"][1], "", -750, 150)
    csum = add(m, add(m, cx, "", cy, "", -600, -80), "", cz, "", -450, 0)
    col = E(m, unreal.MaterialExpressionDivide, -300, 0)
    C(csum, "", col, "A")
    C(wsum, "", col, "B")
    tint = vector(m, "Tint", (1, 1, 1), -300, -200)
    out = mul(m, col, "", tint, "", -150, -100)
    P(out, "", unreal.MaterialProperty.MP_BASE_COLOR)
    P(scalar(m, "Roughness", 0.75, -150, 150), "", unreal.MaterialProperty.MP_ROUGHNESS)
    finish(m)
    return m


# ------------------------------------------------------------------ M_NL_Landscape

def make_landscape():
    m = material("M_NL_Landscape")
    wp = E(m, unreal.MaterialExpressionWorldPosition, -2600, 0)
    xy = E(m, unreal.MaterialExpressionComponentMask, -2400, 0, r=True, g=True, b=False, a=False)
    C(wp, "", xy, "")
    z = E(m, unreal.MaterialExpressionComponentMask, -2400, 300, r=False, g=False, b=True, a=False)
    C(wp, "", z, "")

    def uvscale(div, x, y):
        d = E(m, unreal.MaterialExpressionDivide, x, y)
        C(xy, "", d, "A")
        C(const(m, div, x - 150, y + 60), "", d, "B")
        return d

    uvg = uvscale(380.0, -2200, -600)
    uvg2 = uvscale(1130.0, -2200, -450)
    uvm = uvscale(260.0, -2200, -150)
    uvr = uvscale(420.0, -2200, 100)
    uvs = uvscale(300.0, -2200, 350)
    g1 = sample(m, tex("T_NL_Ground_C"), uvg, "", -1900, -700)
    g2 = sample(m, tex("T_NL_Ground_C"), uvg2, "", -1900, -550)
    gn = sample(m, tex("T_NL_Ground_N"), uvg, "", -1900, -400, normal=True)
    ground = lerp(m, g1, "RGB", g2, "RGB", const(m, 0.45, -1750, -500), "", -1650, -620)
    mc = sample(m, tex("T_NL_Mud_C"), uvm, "", -1900, -250)
    mn = sample(m, tex("T_NL_Mud_N"), uvm, "", -1900, -100, normal=True)
    rc = sample(m, tex("T_NL_Rock_C"), uvr, "", -1900, 50)
    rn = sample(m, tex("T_NL_Rock_N"), uvr, "", -1900, 200, normal=True)
    sc = sample(m, tex("T_NL_Sand_C"), uvs, "", -1900, 350)
    sn = sample(m, tex("T_NL_Sand_N"), uvs, "", -1900, 500, normal=True)

    # Patika maskesi (eski arazi materyalindeki eşleme ile aynı)
    pm_uv = E(m, unreal.MaterialExpressionDivide, -2100, 700)
    pm_add = E(m, unreal.MaterialExpressionAdd, -2250, 700)
    C(xy, "", pm_add, "A")
    C(E(m, unreal.MaterialExpressionConstant2Vector, -2400, 760, r=4671.25, g=4671.25), "", pm_add, "B")
    C(pm_add, "", pm_uv, "A")
    C(const(m, 9342.5, -2250, 800), "", pm_uv, "B")
    pmask = sample(m, unreal.load_asset("/Game/AdaV2_Varliklari/DOKU_Patika"), pm_uv, "", -1900, 700)
    path = E(m, unreal.MaterialExpressionSaturate, -1650, 700)
    C(mul(m, pmask, "R", const(m, 1.6, -1800, 820), "", -1750, 760), "", path, "")
    # Eğim: 1 - normal.z
    vn = E(m, unreal.MaterialExpressionVertexNormalWS, -2400, 1000)
    vz = E(m, unreal.MaterialExpressionComponentMask, -2250, 1000, r=False, g=False, b=True, a=False)
    C(vn, "", vz, "")
    slope = E(m, unreal.MaterialExpressionOneMinus, -2100, 1000)
    C(vz, "", slope, "")
    rockw = E(m, unreal.MaterialExpressionSaturate, -1800, 1000)
    C(mul(m, add(m, slope, "", const(m, -0.3, -2000, 1080), "", -1950, 1000), "", const(m, 6.0, -1950, 1100), "", -1880, 1040), "", rockw, "")
    # Kıyı: deniz seviyesine yakın (z < 90)
    shore = E(m, unreal.MaterialExpressionSaturate, -1800, 1250)
    C(mul(m, add(m, const(m, 95.0, -2100, 1300), "", mul(m, z, "", const(m, -1.0, -2250, 1350), "", -2150, 1250), "", -2000, 1250), "", const(m, 1.0 / 55.0, -2000, 1350), "", -1900, 1280), "", shore, "")

    col = lerp(m, ground, "", mc, "RGB", path, "", -1300, -400)
    col = lerp(m, col, "", sc, "RGB", shore, "", -1100, -300)
    col = lerp(m, col, "", rc, "RGB", rockw, "", -900, -200)
    nrm = lerp(m, gn, "RGB", mn, "RGB", path, "", -1300, 200)
    nrm = lerp(m, nrm, "", sn, "RGB", shore, "", -1100, 250)
    nrm = lerp(m, nrm, "", rn, "RGB", rockw, "", -900, 300)
    rough = lerp(m, const(m, 0.88, -1400, 500), "", const(m, 0.42, -1400, 560), "", path, "", -1300, 520)
    rough = lerp(m, rough, "", const(m, 0.55, -1200, 600), "", shore, "", -1100, 560)
    rough = lerp(m, rough, "", const(m, 0.7, -1000, 640), "", rockw, "", -900, 600)
    P(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    P(nrm, "", unreal.MaterialProperty.MP_NORMAL)
    P(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    finish(m)
    return m


# ------------------------------------------------------------------ M_NL_Sea

def make_sea():
    m = material("M_NL_Sea")
    wp = E(m, unreal.MaterialExpressionWorldPosition, -1400, 0)
    xy = E(m, unreal.MaterialExpressionComponentMask, -1250, 0, r=True, g=True, b=False, a=False)
    C(wp, "", xy, "")
    layers = []
    for i, (div, sx, sy) in enumerate(((1100.0, 0.012, 0.007), (2600.0, -0.006, 0.01), (420.0, 0.02, -0.015))):
        d = E(m, unreal.MaterialExpressionDivide, -1100, -200 + i * 200)
        C(xy, "", d, "A")
        C(const(m, div, -1250, -150 + i * 200), "", d, "B")
        pan = E(m, unreal.MaterialExpressionPanner, -950, -200 + i * 200, speed_x=sx, speed_y=sy)
        C(d, "", pan, "Coordinate")
        layers.append(sample(m, tex("T_NL_Water_N"), pan, "", -750, -200 + i * 200, normal=True))
    s = add(m, add(m, layers[0], "RGB", layers[1], "RGB", -500, -100), "", layers[2], "RGB", -400, 0)
    flat = mul(m, s, "", const3(m, (1, 1, 0.34), -500, 150), "", -300, 50)
    n = E(m, unreal.MaterialExpressionNormalize, -150, 50)
    C(flat, "", n, "")
    P(n, "", unreal.MaterialProperty.MP_NORMAL)
    P(vector(m, "Color", (0.003, 0.008, 0.011), -300, -200), "", unreal.MaterialProperty.MP_BASE_COLOR)
    P(scalar(m, "Roughness", 0.07, -300, 250), "", unreal.MaterialProperty.MP_ROUGHNESS)
    P(const(m, 0.6, -300, 350), "", unreal.MaterialProperty.MP_SPECULAR)
    finish(m)
    return m


# ------------------------------------------------------------------ M_NL_Glow

def make_glow():
    m = material("M_NL_Glow")
    col = vector(m, "Color", (1.0, 0.55, 0.22), -500, 0)
    inten = scalar(m, "Intensity", 8.0, -500, 150)
    P(mul(m, col, "", inten, "", -250, 50), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    P(vector(m, "Base", (0.01, 0.012, 0.015), -250, -150), "", unreal.MaterialProperty.MP_BASE_COLOR)
    P(scalar(m, "Roughness", 0.08, -250, 250), "", unreal.MaterialProperty.MP_ROUGHNESS)
    finish(m)
    return m


# ------------------------------------------------------------------ örnekler

def instance(name, parent, textures=None, scalars=None, vectors=None):
    path = DEST + "/" + name
    mi = unreal.load_asset(path) if eal.does_asset_exist(path) else None
    if not mi:
        mi = at.create_asset(name, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mi.set_editor_property("parent", parent)
    for k, v in (textures or {}).items():
        mel.set_material_instance_texture_parameter_value(mi, k, tex(v))
    for k, v in (scalars or {}).items():
        mel.set_material_instance_scalar_parameter_value(mi, k, v)
    for k, v in (vectors or {}).items():
        mel.set_material_instance_vector_parameter_value(mi, k, unreal.LinearColor(v[0], v[1], v[2], 1))
    mel.update_material_instance(mi)
    eal.save_loaded_asset(mi)
    log("ornek: " + name)


surf = build(make_surface)
world = build(make_world)
build(make_landscape)
build(make_sea)
glow = build(make_glow)


def surf_mi(name, t, rough, tint=(1, 1, 1), uv=1.0, ns=1.0):
    instance(name, surf, {"BaseTex": "T_NL_%s_C" % t, "NormalTex": "T_NL_%s_N" % t},
             {"Roughness": rough, "UVScale": uv, "NormalStrength": ns}, {"Tint": tint})


surf_mi("MI_NL_LogBark", "LogBark", 0.5)
surf_mi("MI_NL_LogEnd", "LogEnd", 0.65)
surf_mi("MI_NL_Wood", "Grain", 0.55)
surf_mi("MI_NL_Shingle", "Grain", 0.42, (0.55, 0.55, 0.6))
surf_mi("MI_NL_DeckWood", "Grain", 0.4, (0.8, 0.78, 0.75))
surf_mi("MI_NL_Crate", "Grain", 0.6, (1.35, 1.25, 1.1))
surf_mi("MI_NL_PineBark", "PineBark", 0.75)
surf_mi("MI_NL_Needles", "Needles", 0.8, uv=2.0)
surf_mi("MI_NL_Rock", "Rock", 0.7)
surf_mi("MI_NL_Metal", "Metal", 0.45)
surf_mi("MI_NL_Rope", "Rope", 0.8, uv=3.0)


def world_mi(name, t, rough, tint=(1, 1, 1), size=220.0):
    instance(name, world, {"BaseTex": "T_NL_%s_C" % t}, {"Roughness": rough, "WorldSize": size}, {"Tint": tint})


world_mi("MI_NL_World_Plaster", "Plaster", 0.7)
world_mi("MI_NL_World_PlasterDark", "Plaster", 0.7, (0.45, 0.45, 0.47))
world_mi("MI_NL_World_Metal", "Metal", 0.45)
world_mi("MI_NL_World_Planks", "Planks", 0.5, size=200.0)
world_mi("MI_NL_World_Wood", "Grain", 0.55, size=150.0)
world_mi("MI_NL_World_Concrete", "Rock", 0.8, (0.9, 0.9, 0.9), 300.0)
instance("MI_NL_WindowGlow", glow, scalars={"Intensity": 0.22}, vectors={"Color": (1.0, 0.42, 0.13)})
instance("MI_NL_WindowDark", glow, scalars={"Intensity": 0.0, "Roughness": 0.05}, vectors={"Base": (0.008, 0.01, 0.012)})
instance("MI_NL_LanternGlass", glow, scalars={"Intensity": 0.5}, vectors={"Color": (1.0, 0.5, 0.18)})

# Sigorta kutusu (Scripts/blender_fusebox.py)
surf_mi("MI_NL_FuseCabinet", "Metal", 0.55, (0.3, 0.32, 0.31), uv=1.5)
surf_mi("MI_NL_GaugeFace", "Plaster", 0.35, (1.3, 1.24, 1.08))
surf_mi("MI_NL_Tape", "Plaster", 0.8, (1.15, 1.0, 0.78), uv=2.0)
surf_mi("MI_NL_Ink", "Plaster", 0.6, (0.03, 0.03, 0.03))
surf_mi("MI_NL_WarnPlate", "Metal", 0.5, (1.25, 0.95, 0.3), uv=2.0)
surf_mi("MI_NL_Rubber", "Plaster", 0.55, (0.05, 0.05, 0.05))
surf_mi("MI_NL_FuseCeramic", "Plaster", 0.3, (1.05, 1.03, 0.97), uv=3.0)
instance("MI_NL_LedGreen", glow, scalars={"Intensity": 2.0}, vectors={"Color": (0.15, 1.0, 0.25)})

# Jeneratör durum lambası (Scripts/blender_old_generator.py)
instance("MI_NL_GenLamp", glow, scalars={"Intensity": 2.0}, vectors={"Color": (0.15, 1.0, 0.25)})

# Eski jeneratör, balıkçı teknesi, bidon, fener alevi, mürettebat kıyafetleri
surf_mi("MI_NL_OldGenPaint", "Metal", 0.55, (0.75, 1.5, 1.9), uv=1.2)
surf_mi("MI_NL_TrawlerWhite", "Metal", 0.6, (2.5, 2.5, 2.4), uv=0.7)
surf_mi("MI_NL_TrawlerBlue", "Metal", 0.55, (0.8, 1.6, 2.6), uv=0.7)
surf_mi("MI_NL_TrawlerBottom", "Metal", 0.7, (0.6, 0.32, 0.18), uv=0.7)
surf_mi("MI_NL_JerryGreen", "Metal", 0.45, (0.6, 0.78, 0.32), uv=2.0)
instance("MI_NL_Flame", glow, scalars={"Intensity": 6.0}, vectors={"Color": (1.0, 0.55, 0.15)})
# Kıyafetler manken (iskeletli mesh) üstünde kullanılır: ana malzeme iskeletli mesh desteği ister
if not surf.get_editor_property("used_with_skeletal_mesh"):
    surf.set_editor_property("used_with_skeletal_mesh", True)
    mel.recompile_material(surf)
    eal.save_loaded_asset(surf)
    log("M_NL_Surface: iskeletli mesh destegi acildi")
surf_mi("MI_NL_Raincoat", "Plaster", 0.3, (1.45, 1.1, 0.22), uv=2.0)
surf_mi("MI_NL_DarkCoat", "Plaster", 0.7, (0.14, 0.15, 0.17), uv=2.0)

with open(os.path.join(unreal.Paths.project_saved_dir(), "NLMaterials.log"), "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))


# ------------------------------------------------------------------ deniz feneri boyası (beyaz/kırmızı bantlar), huzme, toz

def make_lighthouse_paint():
    # Dünyaya hizalı sıva (3 yön) x yatay bantlar (mesh yerel yüksekliğine göre: kule ölçeklense de bant sayısı sabit)
    m = material("M_NL_LighthousePaint")
    wp = E(m, unreal.MaterialExpressionWorldPosition, -1600, 0)
    p = E(m, unreal.MaterialExpressionDivide, -1400, 50)
    C(wp, "", p, "A")
    C(const(m, 240.0, -1600, 150), "", p, "B")
    texobj = E(m, unreal.MaterialExpressionTextureObjectParameter, -1400, -300, parameter_name="BaseTex", texture=tex("T_NL_Plaster_C"))
    nrm = E(m, unreal.MaterialExpressionVertexNormalWS, -1400, 400)
    ab = E(m, unreal.MaterialExpressionAbs, -1250, 400)
    C(nrm, "", ab, "")
    w4 = mul(m, mul(m, ab, "", ab, "", -1100, 400), "", mul(m, ab, "", ab, "", -1100, 480), "", -950, 400)
    acc = None
    wsum = None
    for i, (ch, rg) in enumerate((("X", (False, True, True)), ("Y", (True, False, True)), ("Z", (True, True, False)))):
        mk = E(m, unreal.MaterialExpressionComponentMask, -1200, -100 + i * 120, r=rg[0], g=rg[1], b=rg[2], a=False)
        C(p, "", mk, "")
        s = E(m, unreal.MaterialExpressionTextureSample, -1000, -150 + i * 160)
        C(mk, "", s, "UVs")
        C(texobj, "", s, "Tex")
        wm = E(m, unreal.MaterialExpressionComponentMask, -950, 520 + i * 60, r=(ch == "X"), g=(ch == "Y"), b=(ch == "Z"), a=False)
        C(w4, "", wm, "")
        cw = mul(m, s, "RGB", wm, "", -750, -150 + i * 150)
        acc = cw if acc is None else add(m, acc, "", cw, "", -600, -100 + i * 80)
        wsum = wm if wsum is None else add(m, wsum, "", wm, "", -800, 520 + i * 40)
    col = E(m, unreal.MaterialExpressionDivide, -450, 0)
    C(acc, "", col, "A")
    C(wsum, "", col, "B")
    # Bantlar
    lp = E(m, unreal.MaterialExpressionLocalPosition, -1400, 800)
    lz = E(m, unreal.MaterialExpressionComponentMask, -1250, 800, r=False, g=False, b=True, a=False)
    C(lp, "", lz, "")
    bands = scalar(m, "Bands", 4.0, -1250, 900)
    f = E(m, unreal.MaterialExpressionFrac, -950, 820)
    C(mul(m, mul(m, lz, "", const(m, 1.0 / 1000.0, -1150, 980), "", -1100, 850), "", bands, "", -1000, 870), "", f, "")
    band = E(m, unreal.MaterialExpressionRound, -850, 820)
    C(f, "", band, "")
    tint = lerp(m, vector(m, "White", (1.0, 0.97, 0.9), -700, 700), "", vector(m, "Red", (0.42, 0.05, 0.04), -700, 800), "", band, "", -500, 750)
    out = mul(m, col, "", tint, "", -300, 200)
    P(out, "", unreal.MaterialProperty.MP_BASE_COLOR)
    P(scalar(m, "Roughness", 0.65, -300, 350), "", unreal.MaterialProperty.MP_ROUGHNESS)
    finish(m)
    return m


def make_beam():
    m = material("M_NL_Beam")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("two_sided", True)
    lp = E(m, unreal.MaterialExpressionLocalPosition, -1200, 0)
    lx = E(m, unreal.MaterialExpressionComponentMask, -1050, 0, r=True, g=False, b=False, a=False)
    C(lp, "", lx, "")
    t = E(m, unreal.MaterialExpressionSaturate, -800, 0)
    C(mul(m, lx, "", const(m, 1.0 / 4000.0, -1050, 100), "", -900, 30), "", t, "")
    inv = E(m, unreal.MaterialExpressionOneMinus, -650, 0)
    C(t, "", inv, "")
    fade = E(m, unreal.MaterialExpressionPower, -500, 0)
    C(inv, "", fade, "Base")
    C(const(m, 1.6, -650, 100), "", fade, "Exponent")
    fr = E(m, unreal.MaterialExpressionFresnel, -650, 250, exponent=2.5)
    center = E(m, unreal.MaterialExpressionOneMinus, -500, 250)
    C(fr, "", center, "")
    df = E(m, unreal.MaterialExpressionDepthFade, -650, 400)
    C(const(m, 250.0, -800, 450), "", df, "FadeDistance")
    k = mul(m, mul(m, fade, "", center, "", -350, 100), "", df, "", -250, 200)
    col = mul(m, vector(m, "Color", (1.0, 0.9, 0.7), -400, -200), "", scalar(m, "Intensity", 0.5, -400, -100), "", -250, -150)
    P(mul(m, col, "", k, "", -100, 0), "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    finish(m)
    return m


def make_dust():
    # Havada süzülen toz: ışıktan etkilenir (sadece fener/lamba vurunca görünür), her zerre kendi yolunda yavaşça salınır
    m = material("M_NL_DustSoft")
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    m.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE_PER_PIXEL_LIGHTING)
    tm = E(m, unreal.MaterialExpressionTime, -1400, 0)
    rnd = E(m, unreal.MaterialExpressionPerInstanceRandom, -1400, 200)
    comps = []
    for i, (spd, ph, amp) in enumerate(((0.21, 37.1, 22.0), (0.17, 91.7, 22.0), (0.11, 53.3, 12.0))):
        a = add(m, mul(m, tm, "", const(m, spd, -1300, i * 150), "", -1200, i * 150), "", mul(m, rnd, "", const(m, ph, -1300, i * 150 + 60), "", -1200, i * 150 + 60), "", -1050, i * 150)
        s = E(m, unreal.MaterialExpressionSine, -900, i * 150)
        C(a, "", s, "")
        comps.append(mul(m, s, "", const(m, amp, -900, i * 150 + 60), "", -750, i * 150))
    xy = E(m, unreal.MaterialExpressionAppendVector, -600, 50)
    C(comps[0], "", xy, "A")
    C(comps[1], "", xy, "B")
    xyz = E(m, unreal.MaterialExpressionAppendVector, -450, 100)
    C(xy, "", xyz, "A")
    C(comps[2], "", xyz, "B")
    P(xyz, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    P(vector(m, "Color", (0.8, 0.77, 0.7), -300, -200), "", unreal.MaterialProperty.MP_BASE_COLOR)
    # Kameranın dibindeki zerreler solar (iri kare görünmesin)
    pdepth = E(m, unreal.MaterialExpressionPixelDepth, -700, 450)
    near = E(m, unreal.MaterialExpressionSaturate, -450, 450)
    C(mul(m, add(m, pdepth, "", const(m, -35.0, -700, 520), "", -600, 450), "", const(m, 1.0 / 140.0, -600, 550), "", -520, 480), "", near, "")
    P(mul(m, scalar(m, "Opacity", 0.4, -450, 300), "", near, "", -300, 330), "", unreal.MaterialProperty.MP_OPACITY)
    P(const(m, 0.35, -300, 400), "", unreal.MaterialProperty.MP_ROUGHNESS)
    finish(m)
    return m


def _step(name):
    with open(os.path.join(unreal.Paths.project_saved_dir(), "NLMatStep.log"), "a") as f:
        f.write(name + "|")


_step("paint")
lh = build(make_lighthouse_paint)
_step("beam")
beam = build(make_beam)
_step("dust")
build(make_dust)
_step("mis")
instance("MI_NL_LighthousePaint", lh)
instance("MI_NL_Beam", beam)
instance("MI_NL_LH_Metal", surf, {"BaseTex": "T_NL_Metal_C", "NormalTex": "T_NL_Metal_N"}, {"Roughness": 0.45, "UVScale": 2.0}, {"Tint": (0.75, 0.25, 0.22)})
instance("MI_NL_LH_Glass", glow, scalars={"Intensity": 0.05, "Roughness": 0.03}, vectors={"Base": (0.02, 0.022, 0.025), "Color": (1.0, 0.7, 0.4)})
# Deniz feneri (Scripts/blender_lighthouse.py): pas lekeli yıpranmış beyaz kule, siyah balkon ve fener odası
surf_mi("MI_NL_LH_Worn", "Metal", 0.6, (3.4, 3.35, 3.15), uv=0.45)
surf_mi("MI_NL_LH_Black", "Metal", 0.5, (0.17, 0.18, 0.19), uv=1.5)
surf_mi("MI_NL_LH_RoofMetal", "Metal", 0.45, (1.0, 1.05, 1.12), uv=1.0)
instance("MI_NL_LensGlow", glow, scalars={"Intensity": 2.5, "Roughness": 0.1}, vectors={"Base": (0.3, 0.28, 0.24), "Color": (1.0, 0.74, 0.4)})

with open(os.path.join(unreal.Paths.project_saved_dir(), "NLMaterials.log"), "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))


# ------------------------------------------------------------------ tekne, motor, plastik, bidon
instance("MI_NL_BoatHull", surf, {"BaseTex": "T_NL_Planks_C", "NormalTex": "T_NL_Planks_N"}, {"Roughness": 0.5, "UVScale": 1.0}, {"Tint": (0.75, 0.82, 0.85)})
instance("MI_NL_MotorPaint", surf, {"BaseTex": "T_NL_Metal_C", "NormalTex": "T_NL_Metal_N"}, {"Roughness": 0.35, "UVScale": 3.0, "NormalStrength": 0.4}, {"Tint": (1.6, 1.6, 1.7)})
instance("MI_NL_Plastic", surf, {"BaseTex": "T_NL_Plaster_C", "NormalTex": "T_NL_Plaster_N"}, {"Roughness": 0.55, "UVScale": 3.0, "NormalStrength": 0.2}, {"Tint": (0.7, 0.66, 0.56)})
instance("MI_NL_JerryRed", surf, {"BaseTex": "T_NL_Metal_C", "NormalTex": "T_NL_Metal_N"}, {"Roughness": 0.4, "UVScale": 2.0, "NormalStrength": 0.6}, {"Tint": (3.2, 0.45, 0.35)})
with open(os.path.join(unreal.Paths.project_saved_dir(), "NLMaterials.log"), "w", encoding="utf-8") as f:
    f.write("\n".join(LOG))
