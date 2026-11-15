# =====================================================================================
#  genshin_grass.py  -  run inside the Unreal editor (paste into the Python box at the bottom):
#     exec(open("C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/genshin_grass.py").read())
# =====================================================================================
#
#  WHAT IT DOES (Grey's Genshin references, 15 Nov)
#  Makes M_GR_GenshinGrass and puts it on the StyleHex grass in the open level. The look:
#    - each blade runs from a deeper colour at the root to a bright tip
#    - the meadow changes colour in big soft PATCHES: mostly fresh green, with golden-yellow,
#      orange-red and brown patches (worked out from where the grass is in the world, so
#      neighbouring clumps match and the patches are 10-20 m across)
#    - slight LUMINOSITY: the tips glow faintly, and light coming from behind shines through
#      the blades ("two-sided foliage" shading), like the sunlit grass in Genshin
#    - it sways in the wind (tips most)
#  The blade SHAPES still come from the pack's grass texture (T_Grass_Opacity).
#
#  TWEAKING: open M_GR_GenshinGrass (Content/GrappleRunner/Nature) and change the green
#  parameter nodes: Glow (how much the tips glow), PatchSize, GreenShare (how much of the
#  meadow stays green), FlipUV (if the bright end is at the roots, set it to 1).
#  Run again to remake it; it is re-applied to the grass in whichever level is open.
# =====================================================================================
import unreal

DEST = "/Game/GrappleRunner/Nature"
NAME = "M_GR_GenshinGrass"
OPACITY = "/Game/StyleHex_Studio/Free_Packs/FREE_Stylized_Forest_Sample/Textures/Foliage/T_Grass_Opacity"
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

COLOUR = r"""
float h = saturate(FlipUV > 0.5 ? UV.y : 1.0 - UV.y);      // 0 at the root, 1 at the tip
// soft value noise over the ground: big patches of colour
float2 p = WP.xy / PatchSize;
float2 i = floor(p); float2 f = frac(p); f = f * f * (3.0 - 2.0 * f);
float a = frac(sin(dot(i, float2(127.1, 311.7))) * 43758.5453);
float b = frac(sin(dot(i + float2(1, 0), float2(127.1, 311.7))) * 43758.5453);
float c = frac(sin(dot(i + float2(0, 1), float2(127.1, 311.7))) * 43758.5453);
float d = frac(sin(dot(i + float2(1, 1), float2(127.1, 311.7))) * 43758.5453);
float n = lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
// a second, smaller noise for clump-to-clump variety
float2 q = WP.xy / (PatchSize * 0.23) + 17.0;
float2 j = floor(q); float2 g = frac(q); g = g * g * (3.0 - 2.0 * g);
float n2 = lerp(lerp(frac(sin(dot(j, float2(12.9, 78.2))) * 43758.5), frac(sin(dot(j + float2(1, 0), float2(12.9, 78.2))) * 43758.5), g.x),
                lerp(frac(sin(dot(j + float2(0, 1), float2(12.9, 78.2))) * 43758.5), frac(sin(dot(j + float2(1, 1), float2(12.9, 78.2))) * 43758.5), g.x), g.y);
// palettes: root colour, tip colour
float3 greenR = float3(0.10, 0.32, 0.05), greenT = float3(0.55, 0.92, 0.18);
float3 goldR  = float3(0.38, 0.30, 0.04), goldT  = float3(0.98, 0.82, 0.22);
float3 orngR  = float3(0.40, 0.12, 0.03), orngT  = float3(1.00, 0.48, 0.12);
float3 brwnR  = float3(0.22, 0.14, 0.07), brwnT  = float3(0.66, 0.48, 0.28);
// which palette here: mostly green, then gold, orange-red, brown
float g1 = smoothstep(GreenShare - 0.05, GreenShare + 0.05, n);            // leaving green
float g2 = smoothstep(GreenShare + 0.12, GreenShare + 0.20, n);            // into orange
float g3 = smoothstep(0.88, 0.95, n);                                      // brown at the very top
float3 root = lerp(lerp(lerp(greenR, goldR, g1), orngR, g2), brwnR, g3);
float3 tip  = lerp(lerp(lerp(greenT, goldT, g1), orngT, g2), brwnT, g3);
tip *= lerp(0.85, 1.12, n2);                                               // clumps a little lighter/darker
float3 col = lerp(root, tip, pow(h, 0.75));
return col;
"""

WIND = r"""
float h = saturate(FlipUV > 0.5 ? UV.y : 1.0 - UV.y);
float w = h * h;
float ph = (WP.x + WP.y) * 0.004;
float gst = 0.6 + 0.4 * sin(T * 0.35 + WP.x * 0.0007);
return float3(sin(T * 1.8 + ph) * 6.0 * gst, cos(T * 1.3 + ph * 1.3) * 3.5 * gst, 0) * w;
"""

PARAMS = [("Glow", 0.06), ("PatchSize", 1500.0), ("GreenShare", 0.62), ("FlipUV", 0.0)]

path = DEST + "/" + NAME
if eal.does_asset_exist(path):
    eal.delete_asset(path)
mat = tools.create_asset(NAME, DEST, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
mat.set_editor_property("two_sided", True)
mat.set_editor_property("used_with_instanced_static_meshes", True)
mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_TWO_SIDED_FOLIAGE)   # light shines through the blades

uv = mel.create_material_expression(mat, unreal.MaterialExpressionTextureCoordinate, -1100, 0)
wp = mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -1100, 120)
tm = mel.create_material_expression(mat, unreal.MaterialExpressionTime, -1100, 240)
pn = {}
y = 360
for name, val in PARAMS:
    e = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -1100, y); y += 100
    e.set_editor_property("parameter_name", name); e.set_editor_property("default_value", val)
    pn[name] = e

def custom(code, ins, x, yy):
    c = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, x, yy)
    c.set_editor_property("code", code)
    c.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    lst = []
    for n in ins:
        ci = unreal.CustomInput(); ci.set_editor_property("input_name", n); lst.append(ci)
    c.set_editor_property("inputs", lst)
    return c

col = custom(COLOUR, ["UV", "WP", "PatchSize", "GreenShare", "FlipUV"], -600, 0)
mel.connect_material_expressions(uv, "", col, "UV")
mel.connect_material_expressions(wp, "", col, "WP")
for n in ("PatchSize", "GreenShare", "FlipUV"):
    mel.connect_material_expressions(pn[n], "", col, n)
mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)

# light through the blades: a lighter, warmer version of the colour
sss = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 150)
sss.set_editor_property("const_b", 0.8)
mel.connect_material_expressions(col, "", sss, "A")
mel.connect_material_property(sss, "", unreal.MaterialProperty.MP_SUBSURFACE_COLOR)

# slight glow on the tips (the level's exposure is very bright, so this stays small)
glow = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -300, 260)
mel.connect_material_expressions(col, "", glow, "A")
mel.connect_material_expressions(pn["Glow"], "", glow, "B")
mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)

rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -300, 380)
rough.set_editor_property("r", 0.85)
mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)

tex = unreal.load_asset(OPACITY)
if tex:
    ts = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -600, 400)
    ts.set_editor_property("texture", tex)
    ts.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)
    mel.connect_material_expressions(uv, "", ts, "UVs")
    mel.connect_material_property(ts, "R", unreal.MaterialProperty.MP_OPACITY_MASK)

wind = custom(WIND, ["UV", "WP", "T", "FlipUV"], -600, 600)
mel.connect_material_expressions(uv, "", wind, "UV")
mel.connect_material_expressions(wp, "", wind, "WP")
mel.connect_material_expressions(tm, "", wind, "T")
mel.connect_material_expressions(pn["FlipUV"], "", wind, "FlipUV")
mel.connect_material_property(wind, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

mel.recompile_material(mat)
eal.save_loaded_asset(mat)

# put it on the grass in the open level
n_done = 0
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
    if a.get_class().get_name() != "GRStylizedNature":
        continue
    for c in a.get_components_by_class(unreal.InstancedStaticMeshComponent):
        if c.static_mesh and "Grass_Clump" in c.static_mesh.get_name():
            for slot in range(c.get_num_materials()):
                c.set_material(slot, mat)
            n_done += 1
unreal.log("GENSHIN GRASS: material made, put on %d grass groups" % n_done)
