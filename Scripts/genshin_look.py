# =====================================================================================
#  genshin_look.py  -  run inside the Unreal editor (Output Log > Python, or paste the line below)
#     exec(open("C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/genshin_look.py").read())
# =====================================================================================
#
#  WHAT IT DOES (14 Nov, Grey: "a little more anime, kinda like Genshin")
#  Makes M_GenshinToon, a post-process material, and swaps it in for M_AnimeOutline on both
#  levels (L_Endless and L_FloatingIslands_2). On characters only (anything with custom depth:
#  your body, the partner, later other players):
#    - TWO-TONE SHADING: each pixel is either "lit" or "in shadow", with a soft edge between
#      (the key to the Genshin look: it isn't a hard cartoon cut, about a 0.1 blend)
#    - TINTED SHADOWS: shadows go soft purple instead of grey
#    - RIM LIGHT: a warm glow just inside the character's silhouette, so they pop off the sky
#    - COLOURED OUTLINES: a dark shade of the colour underneath, not pure black
#    - MORE SATURATED colour on the character
#  The whole picture also gets a little more colour saturation (post-process volume setting).
#  The world itself keeps its realistic lighting.
#
#  TWEAKING: open M_GenshinToon in the Content Browser (GrappleRunner/Materials) and change the
#  green parameter nodes: Threshold (where shadow starts), ShadowTint, RimColor, RimStrength,
#  Saturation, Thickness (outline width in pixels), Strength (outline darkness).
#
#  Safe to run twice. M_AnimeOutline is kept (not deleted), just taken off the levels.
# =====================================================================================
import unreal

MAT_DIR = "/Game/GrappleRunner/Materials"
LEVELS = ["/Game/GrappleRunner/Maps/L_Endless", "/Game/GrappleRunner/Maps/L_FloatingIslands_2"]
SCENE_SATURATION = 1.12   # whole picture: 1 = unchanged

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary

CODE = r"""
float2 uv = GetDefaultSceneTextureUV(Parameters, 14);
float2 px = View.ViewSizeAndInvSize.zw * Thickness;
float c  = SceneTextureLookup(uv, 13, false).r;                     // custom depth (characters)
float sd = SceneTextureLookup(uv, 1, false).r;                      // scene depth
float l = SceneTextureLookup(uv + float2(-px.x, 0), 13, false).r;
float r = SceneTextureLookup(uv + float2( px.x, 0), 13, false).r;
float t = SceneTextureLookup(uv + float2(0, -px.y), 13, false).r;
float b = SceneTextureLookup(uv + float2(0,  px.y), 13, false).r;
float nearD = min(min(min(l, r), min(t, b)), c);
float farD  = max(max(max(l, r), max(t, b)), c);
float edge = saturate((farD - nearD) / max(nearD, 1.0) * 6.0 - 0.3);   // outline where depth jumps
float seen = nearD <= sd + 5.0 ? 1.0 : 0.0;                           // not through walls
float isChar = (c < 100000.0 && c <= sd + 5.0) ? 1.0 : 0.0;

// Rim: look 4x further out; if that's background, this pixel is near the silhouette.
float2 rp = px * 4.0;
float rl = SceneTextureLookup(uv + float2(-rp.x, 0), 13, false).r;
float rr = SceneTextureLookup(uv + float2( rp.x, 0), 13, false).r;
float rt = SceneTextureLookup(uv + float2(0, -rp.y), 13, false).r;
float farR = max(max(rl, rr), rt);
float rim = isChar * saturate((farR - c) / max(c, 1.0) * 4.0 - 0.3) * (1.0 - edge);

float3 col = Scene.rgb + CD.r * 0.0;
float lum = dot(col, float3(0.299, 0.587, 0.114));
float lit = smoothstep(Threshold - 0.05, Threshold + 0.05, lum);     // two tones, soft edge
float3 toon = lerp(col * ShadowTint.rgb * 1.5, col * 1.08, lit);     // purple-ish shadows
float g = dot(toon, float3(0.299, 0.587, 0.114));
toon = lerp(float3(g, g, g), toon, Saturation);                      // richer colour
toon += RimColor.rgb * rim * RimStrength;                            // rim light
col = lerp(col, toon, isChar);
float3 ink = col * 0.3 + Ink.rgb;                                    // coloured outline
col = lerp(col, ink, edge * seen * Strength);
return col;
"""

PARAMS = [  # name, kind, default
    ("Thickness", "s", 1.2), ("Strength", "s", 0.85), ("Threshold", "s", 0.33),
    ("Saturation", "s", 1.25), ("RimStrength", "s", 0.35),
    ("ShadowTint", "v", unreal.LinearColor(0.78, 0.70, 0.92, 1.0)),
    ("RimColor", "v", unreal.LinearColor(1.0, 0.94, 0.84, 1.0)),
    ("Ink", "v", unreal.LinearColor(0.02, 0.015, 0.03, 1.0)),
]

path = MAT_DIR + "/M_GenshinToon"
if eal.does_asset_exist(path):
    mat = unreal.load_asset(path)
    print("M_GenshinToon already there")
else:
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_GenshinToon", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    for name in ("BL_SCENE_COLOR_AFTER_TONEMAPPING", "BL_AFTER_TONEMAPPING"):
        if hasattr(unreal.BlendableLocation, name):
            mat.set_editor_property("blendable_location", getattr(unreal.BlendableLocation, name))
            break
    scene = mel.create_material_expression(mat, unreal.MaterialExpressionSceneTexture, -900, 0)
    scene.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    depth = mel.create_material_expression(mat, unreal.MaterialExpressionSceneTexture, -900, 150)
    depth.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_CUSTOM_DEPTH)
    custom = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -350, 0)
    custom.set_editor_property("code", CODE)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    inputs = []
    for n in ["Scene", "CD"] + [p[0] for p in PARAMS]:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        inputs.append(ci)
    custom.set_editor_property("inputs", inputs)
    mel.connect_material_expressions(scene, "Color", custom, "Scene")
    mel.connect_material_expressions(depth, "Color", custom, "CD")
    y = 300
    for name, kind, value in PARAMS:
        cls = unreal.MaterialExpressionScalarParameter if kind == "s" else unreal.MaterialExpressionVectorParameter
        e = mel.create_material_expression(mat, cls, -900, y)
        e.set_editor_property("parameter_name", name)
        e.set_editor_property("default_value", value)
        mel.connect_material_expressions(e, "", custom, name)
        y += 100
    mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    print("M_GenshinToon made")

old = unreal.load_asset(MAT_DIR + "/M_AnimeOutline")
actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
for lvl in LEVELS:
    if not eal.does_asset_exist(lvl):
        continue
    level_sub.load_level(lvl)
    for a in actor_sub.get_all_level_actors():
        if not isinstance(a, unreal.PostProcessVolume):
            continue
        s = a.get_editor_property("settings")
        wb = s.get_editor_property("weighted_blendables")
        arr = [x for x in wb.get_editor_property("array") if x.get_editor_property("object") not in (old, mat)]
        arr.append(unreal.WeightedBlendable(1.0, mat))
        wb.set_editor_property("array", arr)
        s.set_editor_property("weighted_blendables", wb)
        s.set_editor_property("override_color_saturation", True)
        s.set_editor_property("color_saturation", unreal.Vector4(SCENE_SATURATION, SCENE_SATURATION, SCENE_SATURATION, 1.0))
        a.set_editor_property("settings", s)
        print("Genshin look on", lvl.split("/")[-1], a.get_actor_label())
    level_sub.save_current_level()
print("DONE genshin_look: press Play to see it")
