"""Materiales del mapa con texturas de PBRgen, proyectadas en el mundo (las islas no tienen UVs fiables).

Reconstruye EN SU LUGAR los M_Map_XX de Content/Game/Core/World/MapDressed (los referencian ~180 actores de L_Main):
    BaseColor = Color * lerp(1, Tex / TexPromedio, DetailStrength)
- `Color` conserva nombre y valor: es el ancla de la paleta (docs/ART.md). DetailStrength = 0 -> el look plano de antes.
- TexPromedio = la textura leida en su mip mas chico (1x1), asi el detalle no cambia el tono medio.
- Proyeccion desde arriba (WorldPosition.xy / TileSize) o triplanar (rocas), segun "proj".
- v1 solo color base: sin normal ni altura (tangentes poco fiables y el desplazamiento pide Nanite).

Uso (editor abierto, sin PIE): en la caja Cmd -> py "<ruta>/ue_world_mats.py" [M_Map_00 M_Map_02 ...]
Antes: git lfs lock de cada M_Map_XX. Datos: WORLD_MATS abajo.
"""
import os
import sys
import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
SRC = os.path.normpath(os.path.join(HERE, "..", "PBRgen"))
TEX_DEST = "/Game/Game/Art/Tripo/Textures/World"
MAT_DIR = "/Game/Game/Core/World/MapDressed"

# material -> textura (carpeta en SourceArt/Tripo/PBRgen + archivo BC), tamano del tile en cm, fuerza del detalle, proyeccion
WORLD_MATS = {
    "M_Map_00": {"src": "Grass_Meadow/T_Grass_Meadow_Ground_BC.png", "tile": 250.0, "detail": 0.85, "proj": "top", "antitile": True},  # cima de islas
    "M_Map_02": {"src": "Grass_Meadow/T_Grass_Meadow_Ground_BC.png", "tile": 300.0, "detail": 0.85, "proj": "top", "antitile": True},  # pradera y cerros
    "M_Map_01": {"src": "Cliff_Rock/T_Stylized_Cliff_Rock_BC.png", "tile": 400.0, "detail": 0.8, "proj": "tri"},  # roca (caras empinadas)
    "M_Map_04": {"src": "Wood_Grain/T_Honey_Wood_Grain_BC.png", "tile": 200.0, "detail": 0.7, "proj": "top"},  # tablas de puente
    "M_Map_05": {"src": "Dirt_Path/T_Packed_Dirt_Path_BC.png", "tile": 300.0, "detail": 0.75, "proj": "top", "antitile": True, "hue_variation": 0.3},  # caminos (y cuerdas)
}

# `Color` original de cada M_Map_XX (lineal, leido del asset generado por gen_map_dressed.py): ancla de la paleta.
ORIGINAL = {
    "M_Map_00": ((0.400864, 0.612066, 0.065656), 0.9),  # pasto de islas, verde lima
    "M_Map_01": ((0.189939, 0.118994, 0.065656), 0.9),  # roca de la base de las islas
    "M_Map_02": ((0.189939, 0.325037, 0.065656), 0.9),  # pradera / cerros
    "M_Map_04": ((0.595360, 0.361867, 0.148302), 0.9),  # tablas de puente
    "M_Map_05": ((0.500392, 0.301677, 0.125992), 0.9),  # cuerdas y caminos
    "M_Map_06": ((0.699394, 0.500392, 0.257790), 0.9),  # base del molino
}

MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
TOOLS = unreal.AssetToolsHelpers.get_asset_tools()


def import_texture(rel):
    path = os.path.join(SRC, rel)
    name = os.path.splitext(os.path.basename(path))[0]
    dest = TEX_DEST + "/" + name
    task = unreal.AssetImportTask()
    task.set_editor_property("filename", path)
    task.set_editor_property("destination_path", TEX_DEST)
    task.set_editor_property("automated", True)
    task.set_editor_property("replace_existing", True)
    task.set_editor_property("save", False)
    TOOLS.import_asset_tasks([task])
    tex = EAL.load_asset(dest)
    tex.set_editor_property("srgb", True)
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_WORLD)
    EAL.save_loaded_asset(tex)
    return tex


def n(m, cls, x, y, **props):
    e = MEL.create_material_expression(m, cls, x, y)
    for k, v in props.items():
        e.set_editor_property(k, v)
    return e


def link(a, out, b, inp):
    if not MEL.connect_material_expressions(a, out, b, inp):
        raise RuntimeError("no conecta %s -> %s.%s" % (a.get_name(), b.get_name(), inp))


def uv_nodes(m, proj, tile, x, y):
    """Devuelve una lista de (expresion_uv, peso) para la proyeccion pedida."""
    wp = n(m, unreal.MaterialExpressionWorldPosition, x, y)
    if proj == "top":
        xy = n(m, unreal.MaterialExpressionComponentMask, x + 200, y, r=True, g=True, b=False, a=False)
        link(wp, "", xy, "")
        uv = n(m, unreal.MaterialExpressionDivide, x + 380, y)
        link(xy, "", uv, "A")
        link(tile, "", uv, "B")
        return [(uv, None)]
    # triplanar: pesos por la normal en mundo
    out = []
    nrm = n(m, unreal.MaterialExpressionVertexNormalWS, x, y + 400)
    absn = n(m, unreal.MaterialExpressionAbs, x + 200, y + 400)
    link(nrm, "", absn, "")
    for i, (mask, w) in enumerate((((False, True, True), "R"), ((True, False, True), "G"), ((True, True, False), "B"))):
        cm = n(m, unreal.MaterialExpressionComponentMask, x + 200, y + i * 140, r=mask[0], g=mask[1], b=mask[2], a=False)
        link(wp, "", cm, "")
        uv = n(m, unreal.MaterialExpressionDivide, x + 380, y + i * 140)
        link(cm, "", uv, "A")
        link(tile, "", uv, "B")
        wm = n(m, unreal.MaterialExpressionComponentMask, x + 380, y + 420 + i * 100,
               r=(w == "R"), g=(w == "G"), b=(w == "B"), a=False)
        link(absn, "", wm, "")
        out.append((uv, wm))
    return out


def anti_tiling(m, cfg, tile, tex, avg_safe, alpha_base, lerp_fine):
    """Rompe la repeticion (lo que hacen los juegos grandes), todo con la misma textura:
    1) segunda lectura a otra escala (TileSize * BigScale, no multiplo) y rotada 37 grados, multiplicada encima;
    2) variacion grande: ruido suave de decenas de metros que aclara/oscurece zonas (MacroStrength);
    3) menos detalle fino a lo lejos (FarStart/FarRange en cm, FarDetail = lo que queda del detalle a lo lejos).
    Todo escala con DetailStrength: en 0 vuelve el color plano."""
    import math
    x0, y0 = -2600, 1200
    # 3) distancia a la camara -> el detalle fino baja a lo lejos
    cam = n(m, unreal.MaterialExpressionCameraPositionWS, x0, y0)
    wpd = n(m, unreal.MaterialExpressionWorldPosition, x0, y0 + 100)
    dist = n(m, unreal.MaterialExpressionDistance, x0 + 200, y0)
    link(wpd, "", dist, "A")
    link(cam, "", dist, "B")
    fstart = n(m, unreal.MaterialExpressionScalarParameter, x0 + 200, y0 + 150, parameter_name="FarStart", default_value=1500.0)
    frange = n(m, unreal.MaterialExpressionScalarParameter, x0 + 200, y0 + 250, parameter_name="FarRange", default_value=4000.0)
    fdet = n(m, unreal.MaterialExpressionScalarParameter, x0 + 200, y0 + 350, parameter_name="FarDetail", default_value=0.35)
    dsub = n(m, unreal.MaterialExpressionSubtract, x0 + 400, y0)
    link(dist, "", dsub, "A")
    link(fstart, "", dsub, "B")
    ddiv = n(m, unreal.MaterialExpressionDivide, x0 + 550, y0)
    link(dsub, "", ddiv, "A")
    link(frange, "", ddiv, "B")
    dsat = n(m, unreal.MaterialExpressionSaturate, x0 + 700, y0)
    link(ddiv, "", dsat, "")
    far_mul = n(m, unreal.MaterialExpressionLinearInterpolate, x0 + 850, y0, const_a=1.0)
    link(fdet, "", far_mul, "B")
    link(dsat, "", far_mul, "Alpha")
    a_fine = n(m, unreal.MaterialExpressionMultiply, x0 + 1000, y0)
    link(alpha_base, "", a_fine, "A")
    link(far_mul, "", a_fine, "B")
    link(a_fine, "", lerp_fine, "Alpha")

    # 1) segunda escala rotada
    ang = math.radians(37.0)
    c, s = math.cos(ang), math.sin(ang)
    wp2 = n(m, unreal.MaterialExpressionWorldPosition, x0, y0 + 600)
    xy2 = n(m, unreal.MaterialExpressionComponentMask, x0 + 150, y0 + 600, r=True, g=True, b=False, a=False)
    link(wp2, "", xy2, "")
    ka = n(m, unreal.MaterialExpressionConstant2Vector, x0 + 150, y0 + 700, r=c, g=-s)
    kb = n(m, unreal.MaterialExpressionConstant2Vector, x0 + 150, y0 + 800, r=s, g=c)
    dx = n(m, unreal.MaterialExpressionDotProduct, x0 + 300, y0 + 650)
    link(xy2, "", dx, "A")
    link(ka, "", dx, "B")
    dy = n(m, unreal.MaterialExpressionDotProduct, x0 + 300, y0 + 750)
    link(xy2, "", dy, "A")
    link(kb, "", dy, "B")
    rot = n(m, unreal.MaterialExpressionAppendVector, x0 + 450, y0 + 700)
    link(dx, "", rot, "A")
    link(dy, "", rot, "B")
    bsc = n(m, unreal.MaterialExpressionScalarParameter, x0 + 450, y0 + 850, parameter_name="BigScale", default_value=cfg.get("big_scale", 5.3))
    btile = n(m, unreal.MaterialExpressionMultiply, x0 + 600, y0 + 800)
    link(tile, "", btile, "A")
    link(bsc, "", btile, "B")
    uv2 = n(m, unreal.MaterialExpressionDivide, x0 + 750, y0 + 700)
    link(rot, "", uv2, "A")
    link(btile, "", uv2, "B")
    s2 = n(m, unreal.MaterialExpressionTextureSampleParameter2D, x0 + 900, y0 + 700, parameter_name="DetailTex",
           texture=tex, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
    link(uv2, "", s2, "UVs")
    r2 = n(m, unreal.MaterialExpressionDivide, x0 + 1100, y0 + 700)
    link(s2, "RGB", r2, "A")
    link(avg_safe, "", r2, "B")
    bst = n(m, unreal.MaterialExpressionScalarParameter, x0 + 1100, y0 + 850, parameter_name="BigStrength", default_value=cfg.get("big_strength", 0.7))
    a2 = n(m, unreal.MaterialExpressionMultiply, x0 + 1250, y0 + 800)
    link(alpha_base, "", a2, "A")
    link(bst, "", a2, "B")
    lerp2 = n(m, unreal.MaterialExpressionLinearInterpolate, x0 + 1400, y0 + 700, const_a=1.0)
    link(r2, "", lerp2, "B")
    link(a2, "", lerp2, "Alpha")

    # 2) variacion grande con ruido barato (gradiente por textura), manchas de ~20 m
    noise = n(m, unreal.MaterialExpressionNoise, x0 + 900, y0 + 1000,
              noise_function=unreal.NoiseFunction.NOISEFUNCTION_GRADIENT_TEX, scale=cfg.get("macro_scale", 0.0005),
              levels=3, level_scale=2.0, output_min=0.0, output_max=1.0, quality=1)
    nsub = n(m, unreal.MaterialExpressionSubtract, x0 + 1100, y0 + 1000, const_b=0.5)
    link(noise, "", nsub, "A")
    mst = n(m, unreal.MaterialExpressionScalarParameter, x0 + 1100, y0 + 1150, parameter_name="MacroStrength", default_value=cfg.get("macro_strength", 0.3))
    amp = n(m, unreal.MaterialExpressionMultiply, x0 + 1250, y0 + 1100)
    link(alpha_base, "", amp, "A")
    link(mst, "", amp, "B")
    amp2 = n(m, unreal.MaterialExpressionMultiply, x0 + 1400, y0 + 1100, const_b=2.0)
    link(amp, "", amp2, "A")
    mm = n(m, unreal.MaterialExpressionMultiply, x0 + 1550, y0 + 1000)
    link(nsub, "", mm, "A")
    link(amp2, "", mm, "B")
    macro = n(m, unreal.MaterialExpressionAdd, x0 + 1700, y0 + 1000, const_b=1.0)
    link(mm, "", macro, "A")

    # 2b) variacion de tono: donde el ruido sube, el pasto/tierra tira a seco (DryTint); HueVariation = cuanto
    dry = n(m, unreal.MaterialExpressionVectorParameter, x0 + 1250, y0 + 1300, parameter_name="DryTint",
            default_value=unreal.LinearColor(*cfg.get("dry_tint", (1.15, 1.0, 0.65)), 1.0))
    one3 = n(m, unreal.MaterialExpressionConstant3Vector, x0 + 1250, y0 + 1400, constant=unreal.LinearColor(1, 1, 1, 1))
    hv = n(m, unreal.MaterialExpressionScalarParameter, x0 + 1250, y0 + 1500, parameter_name="HueVariation", default_value=cfg.get("hue_variation", 0.8))
    ha = n(m, unreal.MaterialExpressionMultiply, x0 + 1400, y0 + 1450)
    link(noise, "", ha, "A")
    link(hv, "", ha, "B")
    hb = n(m, unreal.MaterialExpressionMultiply, x0 + 1550, y0 + 1450)
    link(ha, "", hb, "A")
    link(alpha_base, "", hb, "B")
    tint = n(m, unreal.MaterialExpressionLinearInterpolate, x0 + 1700, y0 + 1350)
    link(one3, "", tint, "A")
    link(dry, "", tint, "B")
    link(hb, "", tint, "Alpha")

    d12 = n(m, unreal.MaterialExpressionMultiply, -450, 100)
    link(lerp_fine, "", d12, "A")
    link(lerp2, "", d12, "B")
    d123 = n(m, unreal.MaterialExpressionMultiply, -400, 200)
    link(d12, "", d123, "A")
    link(macro, "", d123, "B")
    d1234 = n(m, unreal.MaterialExpressionMultiply, -350, 300)
    link(d123, "", d1234, "A")
    link(tint, "", d1234, "B")
    return d1234


def build(name, cfg):
    path = MAT_DIR + "/" + name
    m = EAL.load_asset(path)
    if not m:
        raise RuntimeError("no existe " + path)
    # ancla de paleta y rugosidad: valores originales fijos (no se leen del material, que este script ya reescribio)
    (cr, cg, cb), rough = ORIGINAL[name]
    color = unreal.LinearColor(cr, cg, cb, 1.0)

    tex = import_texture(cfg["src"])
    for _ in range(3):  # una sola pasada a veces deja nodos huerfanos
        MEL.delete_all_material_expressions(m)

    col = n(m, unreal.MaterialExpressionVectorParameter, -500, -300, parameter_name="Color", default_value=color)
    ro = n(m, unreal.MaterialExpressionScalarParameter, -500, 300, parameter_name="Roughness", default_value=rough)
    tile = n(m, unreal.MaterialExpressionScalarParameter, -2000, 0, parameter_name="TileSize", default_value=cfg["tile"])
    det = n(m, unreal.MaterialExpressionScalarParameter, -800, 150, parameter_name="DetailStrength", default_value=cfg["detail"])

    # promedio de la textura: su mip mas chico
    avg = n(m, unreal.MaterialExpressionTextureSampleParameter2D, -1200, 400, parameter_name="DetailTex", texture=tex,
            sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR,
            mip_value_mode=unreal.TextureMipValueMode.TMVM_MIP_LEVEL, const_mip_value=14)
    avg_safe = n(m, unreal.MaterialExpressionMax, -1000, 400, const_b=0.02)
    link(avg, "RGB", avg_safe, "A")

    samples = []
    for i, (uv, w) in enumerate(uv_nodes(m, cfg["proj"], tile, -2000, 200)):
        s = n(m, unreal.MaterialExpressionTextureSampleParameter2D, -1400, -200 + i * 220, parameter_name="DetailTex",
              texture=tex, sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
        link(uv, "", s, "UVs")
        samples.append((s, w))
    if len(samples) == 1:
        tex_rgb, tex_out = samples[0][0], "RGB"
    else:
        acc = None
        for s, w in samples:
            mul = n(m, unreal.MaterialExpressionMultiply, -1150, -200)
            link(s, "RGB", mul, "A")
            link(w, "", mul, "B")
            if acc is None:
                acc = mul
            else:
                add = n(m, unreal.MaterialExpressionAdd, -1050, -200)
                link(acc, "", add, "A")
                link(mul, "", add, "B")
                acc = add
        # normaliza los pesos (|n| no suma 1)
        nrm = n(m, unreal.MaterialExpressionVertexNormalWS, -1300, 600)
        absn = n(m, unreal.MaterialExpressionAbs, -1150, 600)
        link(nrm, "", absn, "")
        dot = n(m, unreal.MaterialExpressionDotProduct, -1000, 600)
        link(absn, "", dot, "A")
        one = n(m, unreal.MaterialExpressionConstant3Vector, -1150, 700, constant=unreal.LinearColor(1, 1, 1, 1))
        link(one, "", dot, "B")
        div = n(m, unreal.MaterialExpressionDivide, -900, -200)
        link(acc, "", div, "A")
        link(dot, "", div, "B")
        tex_rgb, tex_out = div, ""

    ratio = n(m, unreal.MaterialExpressionDivide, -800, -100)
    link(tex_rgb, tex_out, ratio, "A")
    link(avg_safe, "", ratio, "B")
    lerp = n(m, unreal.MaterialExpressionLinearInterpolate, -600, 0, const_a=1.0)
    link(ratio, "", lerp, "B")
    if cfg["proj"] == "top":
        # la proyeccion desde arriba se estira en caras verticales: ahi el detalle se apaga (queda el color plano)
        vn = n(m, unreal.MaterialExpressionVertexNormalWS, -1300, 800)
        nz = n(m, unreal.MaterialExpressionComponentMask, -1150, 800, r=False, g=False, b=True, a=False)
        link(vn, "", nz, "")
        sub = n(m, unreal.MaterialExpressionSubtract, -1000, 800, const_b=0.6)
        link(nz, "", sub, "A")
        mul = n(m, unreal.MaterialExpressionMultiply, -900, 800, const_b=5.0)
        link(sub, "", mul, "A")
        sat = n(m, unreal.MaterialExpressionSaturate, -800, 800)
        link(mul, "", sat, "")
        fade = n(m, unreal.MaterialExpressionMultiply, -700, 300)
        link(det, "", fade, "A")
        link(sat, "", fade, "B")
        alpha_base = fade
    else:
        alpha_base = det
    detail = lerp
    if cfg.get("antitile"):
        detail = anti_tiling(m, cfg, tile, tex, avg_safe, alpha_base, lerp)
    else:
        link(alpha_base, "", lerp, "Alpha")
    base = n(m, unreal.MaterialExpressionMultiply, -300, -200)
    link(col, "", base, "A")
    link(detail, "", base, "B")

    MEL.connect_material_property(base, "", unreal.MaterialProperty.MP_BASE_COLOR)
    MEL.connect_material_property(ro, "", unreal.MaterialProperty.MP_ROUGHNESS)
    MEL.layout_material_expressions(m)
    MEL.recompile_material(m)
    if not EAL.save_loaded_asset(m, False):
        raise RuntimeError("no se pudo guardar " + path + " (lock de LFS / solo lectura?)")
    return {"color": [cr, cg, cb], "rough": rough, "tex": tex.get_path_name()}


def main(names):
    import json
    out = {}
    for name in names or list(WORLD_MATS):
        try:
            out[name] = build(name, WORLD_MATS[name])
        except Exception as ex:
            out[name] = {"error": repr(ex)}
    report = os.path.join(unreal.Paths.project_saved_dir(), "ue_world_mats_report.json")
    json.dump(out, open(report, "w"), indent=1)
    unreal.log("ue_world_mats: " + json.dumps(out))


main([a for a in sys.argv[1:] if a.startswith("M_Map_")])
