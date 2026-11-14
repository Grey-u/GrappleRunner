# =====================================================================================
#  make_endless_and_look.py  -  run inside the Unreal editor (Output Log > Python)
# =====================================================================================
#
#  WHAT IT DOES (14 Nov)
#  1. M_GasGlow: a glowing material for the gas canisters ("Color" + "Glow" parameters).
#  2. M_AnimeOutline: the "anime realism" post-process. Characters (anything with custom
#     depth: you, the AI partner) get a thin ink outline and softly banded, cel-like
#     shading; the world stays realistic. This is the FF7 Remake / anime-film idea:
#     anime characters drawn into a realistically lit world.
#  3. L_Endless: the solo endless level. Made from L_FloatingIslands_2 (opened, stripped,
#     then saved under the new name, so the original is not touched): only the start island
#     area, the sky, lights and game helpers remain. The spawner builds the rest ahead of you.
#  4. In L_FloatingIslands_2 (now the multiplayer map), the endless spawner no longer runs
#     when you press Play, and both levels get the anime outline.
#
#  Safe to run twice: things that already exist are reused, not made again.
# =====================================================================================
import unreal

MAT_DIR = "/Game/GrappleRunner/Materials"
ISLANDS = "/Game/GrappleRunner/Maps/L_FloatingIslands_2"
ENDLESS = "/Game/GrappleRunner/Maps/L_Endless"
KEEP_RADIUS = 6000.0   # cm round the player start that stays in L_Endless (the start island)

assets = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary


def new_material(name):
    path = MAT_DIR + "/" + name
    if eal.does_asset_exist(path):
        return unreal.load_asset(path), False
    return assets.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew()), True


# ---- 1. glowing canister material ------------------------------------------------------
glow, made = new_material("M_GasGlow")
if made:
    glow.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    col = mel.create_material_expression(glow, unreal.MaterialExpressionVectorParameter, -500, 0)
    col.set_editor_property("parameter_name", "Color")
    col.set_editor_property("default_value", unreal.LinearColor(0.35, 0.95, 0.35, 1.0))
    amt = mel.create_material_expression(glow, unreal.MaterialExpressionScalarParameter, -500, 200)
    amt.set_editor_property("parameter_name", "Glow")
    amt.set_editor_property("default_value", 6.0)
    mul = mel.create_material_expression(glow, unreal.MaterialExpressionMultiply, -250, 100)
    mel.connect_material_expressions(col, "", mul, "A")
    mel.connect_material_expressions(amt, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(glow)
    eal.save_loaded_asset(glow)
print("M_GasGlow", "made" if made else "already there")

# ---- 2. anime outline post-process -------------------------------------------------------
OUTLINE_CODE = r"""
float2 uv = GetDefaultSceneTextureUV(Parameters, 14);
float2 px = View.ViewSizeAndInvSize.zw * Thickness;
float c  = SceneTextureLookup(uv, 13, false).r;                 // custom depth here (13 = CustomDepth)
float sd = SceneTextureLookup(uv, 1, false).r;                  // scene depth here (1 = SceneDepth)
float l = SceneTextureLookup(uv + float2(-px.x, 0), 13, false).r;
float r = SceneTextureLookup(uv + float2( px.x, 0), 13, false).r;
float t = SceneTextureLookup(uv + float2(0, -px.y), 13, false).r;
float b = SceneTextureLookup(uv + float2(0,  px.y), 13, false).r;
float nearD = min(min(min(l, r), min(t, b)), c);
float farD  = max(max(max(l, r), max(t, b)), c);
float edge = saturate((farD - nearD) / max(nearD, 1.0) * 6.0 - 0.3);   // a big jump in depth = an outline
float seen = nearD <= sd + 5.0 ? 1.0 : 0.0;                           // not when hidden behind a wall
float isChar = (c < 100000.0 && c <= sd + 5.0) ? 1.0 : 0.0;
float3 col = Scene.rgb + CD.r * 0.0;
float lum = dot(col, float3(0.299, 0.587, 0.114));
float band = floor(lum * 3.0 + 0.5) / 3.0;                            // three soft tone bands
col = lerp(col, col * (max(band, 0.08) / max(lum, 0.001)), Cel * isChar);
col = lerp(col, Ink.rgb, edge * seen * Strength);
return col;
"""
outline, made = new_material("M_AnimeOutline")
if made:
    outline.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
    for name in ("BL_SCENE_COLOR_AFTER_TONEMAPPING", "BL_AFTER_TONEMAPPING"):
        if hasattr(unreal.BlendableLocation, name):
            outline.set_editor_property("blendable_location", getattr(unreal.BlendableLocation, name))
            break
    scene = mel.create_material_expression(outline, unreal.MaterialExpressionSceneTexture, -800, 0)
    scene.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    depth = mel.create_material_expression(outline, unreal.MaterialExpressionSceneTexture, -800, 150)
    depth.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_CUSTOM_DEPTH)
    custom = mel.create_material_expression(outline, unreal.MaterialExpressionCustom, -350, 0)
    custom.set_editor_property("code", OUTLINE_CODE)
    custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    names = ["Scene", "CD", "Thickness", "Cel", "Strength", "Ink"]
    ins = []
    for n in names:
        ci = unreal.CustomInput()
        ci.set_editor_property("input_name", n)
        ins.append(ci)
    custom.set_editor_property("inputs", ins)

    def param(cls, name, value, y):
        e = mel.create_material_expression(outline, cls, -800, y)
        e.set_editor_property("parameter_name", name)
        e.set_editor_property("default_value", value)
        return e
    thick = param(unreal.MaterialExpressionScalarParameter, "Thickness", 1.5, 300)
    cel = param(unreal.MaterialExpressionScalarParameter, "Cel", 0.35, 400)
    strength = param(unreal.MaterialExpressionScalarParameter, "Strength", 0.9, 500)
    ink = param(unreal.MaterialExpressionVectorParameter, "Ink", unreal.LinearColor(0.035, 0.03, 0.045, 1.0), 600)
    mel.connect_material_expressions(scene, "Color", custom, "Scene")
    mel.connect_material_expressions(depth, "Color", custom, "CD")
    mel.connect_material_expressions(thick, "", custom, "Thickness")
    mel.connect_material_expressions(cel, "", custom, "Cel")
    mel.connect_material_expressions(strength, "", custom, "Strength")
    mel.connect_material_expressions(ink, "", custom, "Ink")
    mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.recompile_material(outline)
    eal.save_loaded_asset(outline)
print("M_AnimeOutline", "made" if made else "already there")

actor_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level_sub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def add_outline_to_level():
    for a in actor_sub.get_all_level_actors():
        if isinstance(a, unreal.PostProcessVolume):
            s = a.get_editor_property("settings")
            wb = s.get_editor_property("weighted_blendables")
            arr = list(wb.get_editor_property("array"))
            if not any(x.get_editor_property("object") == outline for x in arr):
                arr.append(unreal.WeightedBlendable(1.0, outline))
                wb.set_editor_property("array", arr)
                s.set_editor_property("weighted_blendables", wb)
                a.set_editor_property("settings", s)
                print("outline added to", a.get_actor_label())


# ---- 3. the endless level -----------------------------------------------------------------
# Open the islands level, strip it down to the start island + sky, then "Save As" L_Endless.
# (Duplicating the map asset and opening the copy crashes the editor in 5.8, so we don't.)
# The islands level on disk is never saved here, so it stays exactly as it was.
def strip_to_start():
    actors = actor_sub.get_all_level_actors()
    start = None
    for a in actors:
        if isinstance(a, unreal.PlayerStart) or a.get_class().get_name() == "GrappleRig":
            start = a.get_actor_location()
            break
    if start is None:
        start = unreal.Vector(0, 0, 0)
    keep_types = (unreal.Light, unreal.SkyLight, unreal.ExponentialHeightFog, unreal.PostProcessVolume,
                  unreal.PlayerStart, unreal.WorldSettings, unreal.SkyAtmosphere, unreal.VolumetricCloud)
    keep_names = ("GrappleRig", "GREndlessSpawner", "GRDayNight", "GRPlanets", "GRAmbientLife", "Sky")
    doomed = []
    for a in actors:
        if isinstance(a, keep_types) or any(k in a.get_class().get_name() for k in keep_names):
            continue
        loc = a.get_actor_location()
        if (loc.x - start.x) ** 2 + (loc.y - start.y) ** 2 < KEEP_RADIUS ** 2:
            continue   # the start island and what grows on it
        doomed.append(a)
    print("L_Endless: removing", len(doomed), "of", len(actors), "actors")
    actor_sub.destroy_actors(doomed)
    for a in actor_sub.get_all_level_actors():
        if a.get_class().get_name() == "GREndlessSpawner":
            a.set_editor_property("run_in_editor_play", True)


if not eal.does_asset_exist(ENDLESS):
    level_sub.load_level(ISLANDS)
    strip_to_start()
    add_outline_to_level()
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    unreal.EditorLoadingAndSavingUtils.save_map(world, ENDLESS)
    print("L_Endless saved")
else:
    print("L_Endless already there")

# ---- 4. the islands level: multiplayer map now -------------------------------------------
level_sub.load_level(ISLANDS)
for a in actor_sub.get_all_level_actors():
    if a.get_class().get_name() == "GREndlessSpawner":
        a.set_editor_property("run_in_editor_play", False)
        print("islands: endless spawner off in editor Play")
add_outline_to_level()
level_sub.save_current_level()
print("DONE make_endless_and_look")
