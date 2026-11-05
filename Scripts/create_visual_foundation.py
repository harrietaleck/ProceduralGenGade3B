# create_visual_foundation.py
# Builds The Grove Meadows' material and Niagara foundation inside the editor.
#   In the editor:   Tools > Execute Python Script... > pick this file
#   Headless:        UnrealEditor-Cmd.exe <project>.uproject -run=pythonscript -script="<this file>"
# Existing assets are never overwritten, so the script is safe to run again.

import unreal

AT = unreal.AssetToolsHelpers.get_asset_tools()
MEL = unreal.MaterialEditingLibrary
EAL = unreal.EditorAssetLibrary
MP = unreal.MaterialProperty

CREATED, SKIPPED, FAILED = [], [], []

FOLDERS = [
    "/Game/Materials/Environment", "/Game/Materials/Enchanted", "/Game/Materials/Towers",
    "/Game/Materials/Corruption", "/Game/Materials/Instances",
    "/Game/VFX/Towers", "/Game/VFX/Projectiles", "/Game/VFX/Enemies",
    "/Game/VFX/Environment", "/Game/VFX/Gameplay",
]

WHITE = "/Engine/EngineResources/WhiteSquareTexture"
NOISE = "/Engine/EngineMaterials/Good64x64TilingNoiseHighFreq"


# ---------- helpers ----------
def new_material(folder, name):
    full = f"{folder}/{name}"
    if EAL.does_asset_exist(full):
        SKIPPED.append(full)
        return None
    mat = AT.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    if not mat:
        FAILED.append(full)
    return mat


def node(mat, cls, x, y, **props):
    expr = MEL.create_material_expression(mat, cls, x, y)
    for key, value in props.items():
        expr.set_editor_property(key, value)
    return expr


def vec(mat, name, rgb, x, y):
    return node(mat, unreal.MaterialExpressionVectorParameter, x, y,
                parameter_name=name, default_value=unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))


def scalar(mat, name, value, x, y):
    return node(mat, unreal.MaterialExpressionScalarParameter, x, y, parameter_name=name, default_value=value)


def sampler_for(texture):
    # The sampler must match the texture's compression, otherwise the material fails to compile.
    tcs = unreal.TextureCompressionSettings
    st = unreal.MaterialSamplerType
    compression = texture.get_editor_property("compression_settings")
    srgb = texture.get_editor_property("srgb")
    if compression == tcs.TC_NORMALMAP:
        return st.SAMPLERTYPE_NORMAL
    if compression == tcs.TC_MASKS:
        return st.SAMPLERTYPE_MASKS
    if compression in (tcs.TC_GRAYSCALE, tcs.TC_ALPHA):
        return st.SAMPLERTYPE_GRAYSCALE if srgb else st.SAMPLERTYPE_LINEAR_GRAYSCALE
    return st.SAMPLERTYPE_COLOR if srgb else st.SAMPLERTYPE_LINEAR_COLOR


def tex(mat, name, path, x, y):
    texture = unreal.load_asset(path)
    sample = node(mat, unreal.MaterialExpressionTextureSampleParameter2D, x, y, parameter_name=name)
    sample.set_editor_property("texture", texture)
    sample.set_editor_property("sampler_type", sampler_for(texture))
    return sample


def link(src, src_out, dst, dst_in):
    MEL.connect_material_expressions(src, src_out, dst, dst_in)


def op(mat, cls, a, b, x, y, a_out="", b_out=""):
    expr = node(mat, cls, x, y)
    link(a, a_out, expr, "A")
    link(b, b_out, expr, "B")
    return expr


def lerp(mat, a, b, alpha, x, y, alpha_out=""):
    expr = node(mat, unreal.MaterialExpressionLinearInterpolate, x, y)
    link(a, "", expr, "A")
    link(b, "", expr, "B")
    link(alpha, alpha_out, expr, "Alpha")
    return expr


def single(mat, cls, src, x, y, src_out="", **props):
    expr = node(mat, cls, x, y, **props)
    link(src, src_out, expr, "")
    return expr


def set_props(mat, **props):
    for key, value in props.items():
        try:
            mat.set_editor_property(key, value)
        except Exception as error:  # property names differ slightly between engine versions
            unreal.log_warning(f"{mat.get_name()}: could not set {key}: {error}")


def finish(mat):
    MEL.layout_material_expressions(mat)
    MEL.recompile_material(mat)
    EAL.save_loaded_asset(mat)
    CREATED.append(mat.get_path_name())


# ---------- master materials ----------
def make_environment():
    mat = new_material("/Game/Materials/Environment", "M_Environment_Master")
    if not mat:
        return
    world = node(mat, unreal.MaterialExpressionWorldPosition, -1400, 0)
    scale = scalar(mat, "VariationScale", 0.002, -1400, 150)
    pos = op(mat, unreal.MaterialExpressionMultiply, world, scale, -1200, 50)
    noise = node(mat, unreal.MaterialExpressionNoise, -1000, 50, levels=3, output_min=0.0, output_max=1.0)
    link(pos, "", noise, "Position")
    base = lerp(mat, vec(mat, "BaseColorDark", (0.08, 0.16, 0.06), -1000, -200),
                vec(mat, "BaseColorLight", (0.22, 0.32, 0.1), -1000, -350), noise, -750, -200)
    detail = tex(mat, "DetailTexture", WHITE, -1000, 300)
    detailed = op(mat, unreal.MaterialExpressionMultiply, base, detail, -550, -100, b_out="RGB")
    # Moss gathers on upward-facing surfaces.
    up = single(mat, unreal.MaterialExpressionComponentMask, node(mat, unreal.MaterialExpressionVertexNormalWS, -1000, 550),
                -800, 550, r=False, g=False, b=True, a=False)
    moss_mask = single(mat, unreal.MaterialExpressionSaturate,
                       op(mat, unreal.MaterialExpressionMultiply, up, scalar(mat, "MossAmount", 0.6, -800, 700), -600, 600),
                       -450, 600)
    final = lerp(mat, detailed, vec(mat, "MossColor", (0.12, 0.3, 0.05), -550, 250), moss_mask, -300, 0)
    MEL.connect_material_property(final, "", MP.MP_BASE_COLOR)
    MEL.connect_material_property(scalar(mat, "Roughness", 0.85, -300, 300), "", MP.MP_ROUGHNESS)
    finish(mat)


def make_magic_glow():
    mat = new_material("/Game/Materials/Enchanted", "M_MagicGlow")
    if not mat:
        return
    color = vec(mat, "GlowColor", (0.2, 1.0, 0.85), -900, -100)
    intensity = scalar(mat, "GlowIntensity", 8.0, -900, 50)
    # Gentle pulse: 0.75 + 0.25 * sin(time * speed)
    time = node(mat, unreal.MaterialExpressionTime, -1100, 200)
    sine = single(mat, unreal.MaterialExpressionSine, op(mat, unreal.MaterialExpressionMultiply, time,
                  scalar(mat, "PulseSpeed", 2.0, -1100, 320), -900, 250), -750, 250)
    pulse = op(mat, unreal.MaterialExpressionAdd,
               op(mat, unreal.MaterialExpressionMultiply, sine, scalar(mat, "PulseAmount", 0.25, -750, 380), -600, 280),
               node(mat, unreal.MaterialExpressionConstant, -600, 420, r=0.75), -450, 300)
    glow = op(mat, unreal.MaterialExpressionMultiply,
              op(mat, unreal.MaterialExpressionMultiply, color, intensity, -650, -50), pulse, -300, 0)
    MEL.connect_material_property(vec(mat, "SurfaceColor", (0.05, 0.08, 0.06), -300, -250), "", MP.MP_BASE_COLOR)
    MEL.connect_material_property(glow, "", MP.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(scalar(mat, "Roughness", 0.4, -300, 200), "", MP.MP_ROUGHNESS)
    finish(mat)


def make_tower_beam():
    mat = new_material("/Game/Materials/Towers", "M_TowerBeam")
    if not mat:
        return
    set_props(mat, blend_mode=unreal.BlendMode.BLEND_ADDITIVE,
              shading_model=unreal.MaterialShadingModel.MSM_UNLIT, two_sided=True)
    uv = node(mat, unreal.MaterialExpressionTextureCoordinate, -1200, 0)
    time = op(mat, unreal.MaterialExpressionMultiply, node(mat, unreal.MaterialExpressionTime, -1200, 150),
              scalar(mat, "ScrollSpeed", 1.5, -1200, 250), -1000, 180)
    panner = node(mat, unreal.MaterialExpressionPanner, -850, 50, speed_x=0.0, speed_y=1.0)
    link(uv, "", panner, "Coordinate")
    link(time, "", panner, "Time")
    sample = tex(mat, "BeamTexture", NOISE, -650, 50)
    link(panner, "", sample, "UVs")
    # Soft edges across the beam width: 1 - |u * 2 - 1|
    u = single(mat, unreal.MaterialExpressionComponentMask, uv, -1000, -250, r=True, g=False, b=False, a=False)
    centred = op(mat, unreal.MaterialExpressionSubtract,
                 op(mat, unreal.MaterialExpressionMultiply, u, node(mat, unreal.MaterialExpressionConstant, -1000, -120, r=2.0), -850, -220),
                 node(mat, unreal.MaterialExpressionConstant, -850, -100, r=1.0), -700, -200)
    edge = single(mat, unreal.MaterialExpressionOneMinus, single(mat, unreal.MaterialExpressionAbs, centred, -550, -200), -420, -200)
    energy = op(mat, unreal.MaterialExpressionMultiply, edge, sample, -300, -100, b_out="R")
    colour = op(mat, unreal.MaterialExpressionMultiply, vec(mat, "BeamColor", (1.0, 0.78, 0.2), -450, 150),
                scalar(mat, "EmissiveIntensity", 12.0, -450, 300), -300, 200)
    MEL.connect_material_property(op(mat, unreal.MaterialExpressionMultiply, energy, colour, -150, 0), "", MP.MP_EMISSIVE_COLOR)
    finish(mat)


def make_corruption():
    mat = new_material("/Game/Materials/Corruption", "M_Corruption")
    if not mat:
        return
    world = node(mat, unreal.MaterialExpressionWorldPosition, -1400, 0)
    moving = op(mat, unreal.MaterialExpressionAdd,
                op(mat, unreal.MaterialExpressionMultiply, world, scalar(mat, "NoiseScale", 0.01, -1400, 150), -1200, 50),
                op(mat, unreal.MaterialExpressionMultiply, node(mat, unreal.MaterialExpressionTime, -1400, 300),
                   scalar(mat, "AnimSpeed", 0.4, -1400, 400), -1200, 330), -1000, 150)
    noise = node(mat, unreal.MaterialExpressionNoise, -850, 150, levels=4, output_min=0.0, output_max=1.0)
    link(moving, "", noise, "Position")
    base = lerp(mat, vec(mat, "DarkColor", (0.01, 0.0, 0.02), -650, -250),
                vec(mat, "CorruptionColor", (0.18, 0.03, 0.3), -650, -100), noise, -450, -150)
    # Glowing edges where the noise crosses the threshold.
    distance = single(mat, unreal.MaterialExpressionAbs,
                      op(mat, unreal.MaterialExpressionSubtract, noise, scalar(mat, "EdgeThreshold", 0.5, -650, 350), -500, 300),
                      -380, 300)
    band = single(mat, unreal.MaterialExpressionOneMinus, single(mat, unreal.MaterialExpressionSaturate,
                  op(mat, unreal.MaterialExpressionDivide, distance, scalar(mat, "EdgeWidth", 0.06, -380, 450), -250, 350),
                  -150, 350), -50, 350)
    glow = op(mat, unreal.MaterialExpressionMultiply, band,
              op(mat, unreal.MaterialExpressionMultiply, vec(mat, "EdgeColor", (0.6, 0.15, 1.0), -250, 550),
                 scalar(mat, "EdgeIntensity", 10.0, -250, 700), -100, 600), 50, 450)
    MEL.connect_material_property(base, "", MP.MP_BASE_COLOR)
    MEL.connect_material_property(glow, "", MP.MP_EMISSIVE_COLOR)
    MEL.connect_material_property(scalar(mat, "Roughness", 0.6, -250, -350), "", MP.MP_ROUGHNESS)
    finish(mat)


def make_foliage():
    mat = new_material("/Game/Materials/Environment", "M_Foliage_Master")
    if not mat:
        return
    set_props(mat, blend_mode=unreal.BlendMode.BLEND_MASKED, two_sided=True, opacity_mask_clip_value=0.33)
    leaf = tex(mat, "LeafTexture", WHITE, -1000, 0)
    # Per-object colour variation from the object's world position.
    obj = op(mat, unreal.MaterialExpressionMultiply, node(mat, unreal.MaterialExpressionObjectPositionWS, -1200, 300),
             scalar(mat, "VariationScale", 0.01, -1200, 420), -1000, 330)
    noise = node(mat, unreal.MaterialExpressionNoise, -850, 330, levels=1, output_min=0.0, output_max=1.0)
    link(obj, "", noise, "Position")
    tint = lerp(mat, vec(mat, "LeafTintA", (0.18, 0.42, 0.08), -650, 200),
                vec(mat, "LeafTintB", (0.35, 0.55, 0.12), -650, 350), noise, -450, 300)
    MEL.connect_material_property(op(mat, unreal.MaterialExpressionMultiply, leaf, tint, -250, 100, a_out="RGB"),
                                  "", MP.MP_BASE_COLOR)
    MEL.connect_material_property(leaf, "A", MP.MP_OPACITY_MASK)
    MEL.connect_material_property(scalar(mat, "Roughness", 0.7, -250, 300), "", MP.MP_ROUGHNESS)
    finish(mat)


# ---------- material instances ----------
def make_instance(name, parent_path, scalars=None, vectors=None):
    folder = "/Game/Materials/Instances"
    full = f"{folder}/{name}"
    if EAL.does_asset_exist(full):
        SKIPPED.append(full)
        return
    parent = unreal.load_asset(parent_path)
    if not parent:
        FAILED.append(f"{full} (missing parent {parent_path})")
        return
    mi = AT.create_asset(name, folder, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, parent)
    for key, value in (scalars or {}).items():
        MEL.set_material_instance_scalar_parameter_value(mi, key, value)
    for key, rgb in (vectors or {}).items():
        MEL.set_material_instance_vector_parameter_value(mi, key, unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    EAL.save_loaded_asset(mi)
    CREATED.append(full)


# ---------- Niagara systems (duplicated from engine templates) ----------
TEMPLATES = "/Niagara/DefaultAssets/Templates/Systems/"
NIAGARA = {
    "/Game/VFX/Towers/NS_TowerFire": "DirectionalBurstLightweight",
    "/Game/VFX/Projectiles/NS_ProjectileTrail": "MinimalLightweight",
    "/Game/VFX/Enemies/NS_EnemyDeath": "RadialBurst",
    "/Game/VFX/Enemies/NS_EnemySpawn": "SimpleExplosion",
    "/Game/VFX/Towers/NS_TowerUpgrade": "FountainLightweight",
    "/Game/VFX/Environment/NS_WaveTransition": "RadialBurst",
    "/Game/VFX/Gameplay/NS_Victory": "FountainLightweight",
    "/Game/VFX/Gameplay/NS_Defeat": "MinimalLightweight",
}


def make_niagara():
    for target, template in NIAGARA.items():
        if EAL.does_asset_exist(target):
            SKIPPED.append(target)
            continue
        # Load the template directly: headless runs don't scan plugin content into the asset registry.
        source = unreal.load_asset(f"{TEMPLATES}{template}.{template}")
        folder, name = target.rsplit("/", 1)
        system = AT.duplicate_asset(name, folder, source) if source else None
        if not system:
            FAILED.append(f"{target} (template {template} not found)")
            continue
        EAL.save_loaded_asset(system)
        CREATED.append(target)


def main():
    for folder in FOLDERS:
        EAL.make_directory(folder)

    make_environment()
    make_magic_glow()
    make_tower_beam()
    make_corruption()
    make_foliage()

    make_instance("MI_Env_ForestGround", "/Game/Materials/Environment/M_Environment_Master",
                  {"MossAmount": 0.7}, {"BaseColorDark": (0.07, 0.12, 0.04), "BaseColorLight": (0.2, 0.26, 0.09)})
    make_instance("MI_Env_MossyRock", "/Game/Materials/Environment/M_Environment_Master",
                  {"MossAmount": 0.45, "Roughness": 0.9}, {"BaseColorDark": (0.12, 0.12, 0.11), "BaseColorLight": (0.25, 0.25, 0.22)})
    make_instance("MI_Glow_Turquoise", "/Game/Materials/Enchanted/M_MagicGlow", {"GlowIntensity": 8.0}, {"GlowColor": (0.2, 1.0, 0.85)})
    make_instance("MI_Glow_Gold", "/Game/Materials/Enchanted/M_MagicGlow", {"GlowIntensity": 10.0}, {"GlowColor": (1.0, 0.78, 0.2)})
    make_instance("MI_Glow_Green", "/Game/Materials/Enchanted/M_MagicGlow", {"GlowIntensity": 6.0}, {"GlowColor": (0.45, 1.0, 0.35)})
    make_instance("MI_Beam_Gold", "/Game/Materials/Towers/M_TowerBeam", {"EmissiveIntensity": 12.0}, {"BeamColor": (1.0, 0.78, 0.2)})
    make_instance("MI_Beam_Turquoise", "/Game/Materials/Towers/M_TowerBeam", {"EmissiveIntensity": 10.0}, {"BeamColor": (0.2, 1.0, 0.85)})
    make_instance("MI_Corruption_Purple", "/Game/Materials/Corruption/M_Corruption", {"EdgeIntensity": 10.0}, {"EdgeColor": (0.6, 0.15, 1.0)})
    make_instance("MI_Foliage_Leaves", "/Game/Materials/Environment/M_Foliage_Master", {}, {"LeafTintA": (0.18, 0.42, 0.08)})

    make_niagara()

    unreal.log(f"VISUAL FOUNDATION: created {len(CREATED)}, skipped {len(SKIPPED)} existing, failed {len(FAILED)}")
    for path in CREATED:
        unreal.log(f"  created {path}")
    for path in FAILED:
        unreal.log_error(f"  FAILED {path}")


main()
