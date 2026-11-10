"""
Grapple Runner: atmosphere and colour. Makes Aeloria an ethereal fantasy place, not a white sky.

Run with the Floating Islands level open (not while playing), AFTER expand_world.py:
    py "C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/color_grade.py"

THREE MOODS. Change MOOD below and re-run to switch (it updates the same lights every time):
  "rose"   - rose and violet light through mist, cool teal shadows: flooded temple ruins
  "amber"  - low amber sun burning through dusty haze, long light shafts
  "teal"   - deep teal forest gloom, crimson light shafts, glowing orbs: a bioluminescent jungle

What it does
- SUN: low in the sky, coloured, with light shafts (god rays) through the fog and the trees.
- FOG: thick, coloured, VOLUMETRIC (it catches light, so beams show up in the air), with a
  warm glow toward the sun. This is what removes the "white-washed" look.
- SKY LIGHT: tinted, and dimmer, so shadows are deep and coloured instead of grey.
- GRADE: more saturation and contrast; shadows pushed toward the mood's cool colour,
  highlights toward its warm one.
- GLOW: small glowing orbs hanging on short vines from the tree canopies, and soft
  coloured lights by the ruins (no shadows, so they're cheap), like the light in a
  bioluminescent forest.
- Removes the OLD harness model from the weapon stand on the first island (Naomi's request).
Everything is numbers at the top: tweak, re-run, compare the screenshots in Saved/Screenshots.

HOW TO READ THIS (no coding experience needed)
- Lines starting with # are notes for humans; the computer skips them.
- Colours: lights use 0-255 (Red, Green, Blue); fog and grade use 0-1.
"""
import math
import random
import unreal

MOOD = "rose"

MOODS = {
    #            sun colour       sun lux pitch  sky light colour  sky   fog colour          fog glow (toward the sun)  fog density  shadows tint        highlights tint      glow colours (orbs and lights)
    "rose":  dict(sun=(255, 176, 190), lux=9.0, pitch=-14, sky=(150, 170, 255), skyi=0.9, fog=(0.42, 0.30, 0.52), glow=(1.0, 0.55, 0.62), dens=0.045,
                  shadows=(0.86, 0.98, 1.12), highs=(1.10, 0.98, 1.02), orbs=[(1.0, 0.35, 0.55), (0.35, 0.9, 1.0), (0.75, 0.45, 1.0)]),
    "amber": dict(sun=(255, 190, 120), lux=10.0, pitch=-10, sky=(170, 150, 140), skyi=0.8, fog=(0.62, 0.42, 0.30), glow=(1.0, 0.70, 0.40), dens=0.05,
                  shadows=(0.95, 0.92, 1.00), highs=(1.12, 1.02, 0.88), orbs=[(1.0, 0.65, 0.25), (1.0, 0.45, 0.2)]),
    "teal":  dict(sun=(255, 90, 80), lux=7.0, pitch=-18, sky=(90, 200, 210), skyi=0.8, fog=(0.10, 0.38, 0.40), glow=(0.95, 0.25, 0.25), dens=0.055,
                  shadows=(0.80, 1.02, 1.08), highs=(1.08, 0.96, 0.98), orbs=[(0.2, 1.0, 0.85), (0.6, 0.4, 1.0), (1.0, 0.55, 0.2)]),
}
SATURATION = 1.35
CONTRAST = 1.15
BLOOM = 1.1
ORB_COUNT = 90                 # glowing orbs hanging from tree canopies
ORB_LIGHT_EVERY = 3            # every 3rd orb also gives off real light (cheap: no shadows)
RUIN_LIGHTS = 30               # soft coloured lights tucked by ruins
GLOW_FOLDER = "GrappleRunner/Glow"
SEED = 3
# -------------------------------------------------------------------------

M = MOODS[MOOD]
random.seed(SEED)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary


def log(text):
    unreal.log("Grapple Runner atmosphere: " + text)


def first(cls):
    for a in actors.get_all_level_actors():
        if isinstance(a, cls):
            return a
    return None


def try_set(obj, names, value):
    for n in names:
        try:
            obj.set_editor_property(n, value)
            return True
        except Exception:
            pass
    unreal.log_warning("Grapple Runner atmosphere: could not set {}".format(names[0]))
    return False


def col(rgb):
    return unreal.Color(r=rgb[0], g=rgb[1], b=rgb[2], a=255)


# ---- 0. remove the old harness from the weapon stand
old = [a for a in actors.get_all_level_actors() if a.get_actor_label() == "Display_SM_GrappleGear"]
if old:
    actors.destroy_actors(old)
    log("removed the old harness from the weapon stand")

# ---- 1. sun: low, coloured, with light shafts
sun = first(unreal.DirectionalLight)
if sun:
    r = sun.get_actor_rotation()
    sun.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=M["pitch"], yaw=r.yaw), False)
    c = sun.get_component_by_class(unreal.DirectionalLightComponent)
    try_set(c, ["light_color"], col(M["sun"]))
    try_set(c, ["intensity"], M["lux"])
    try_set(c, ["enable_light_shaft_bloom"], True)          # the glowing beams around the sun
    try_set(c, ["bloom_scale"], 0.35)
    try_set(c, ["bloom_threshold"], 0.0)
    try_set(c, ["enable_light_shaft_occlusion"], True)      # beams cut by islands and trees
    try_set(c, ["light_shaft_override_direction"], unreal.Vector(0, 0, 0))
    try_set(c, ["cast_volumetric_shadow"], True)            # shadows inside the fog = visible shafts
    try_set(c, ["volumetric_scattering_intensity"], 2.5)
    log("sun: low and coloured, with light shafts")

# ---- 2. sky light: tinted and dimmer, so shadows are coloured, not grey
sky = first(unreal.SkyLight)
if sky:
    c = sky.get_component_by_class(unreal.SkyLightComponent)
    try_set(c, ["light_color"], col(M["sky"]))
    try_set(c, ["intensity"], M["skyi"])
    try_set(c, ["volumetric_scattering_intensity"], 0.6)
    try:
        c.recapture_sky()
    except Exception:
        pass
    log("sky light: tinted")

# ---- 3. fog: thick, coloured, volumetric
for fog in [a for a in actors.get_all_level_actors() if isinstance(a, unreal.ExponentialHeightFog)]:
    c = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    try_set(c, ["fog_density"], M["dens"])
    try_set(c, ["fog_height_falloff"], 0.0022)
    try_set(c, ["fog_inscattering_luminance", "fog_inscattering_color"], unreal.LinearColor(*M["fog"], 1.0))
    try_set(c, ["directional_inscattering_luminance", "directional_inscattering_color"], unreal.LinearColor(*M["glow"], 1.0))
    try_set(c, ["directional_inscattering_exponent"], 5.0)
    try_set(c, ["start_distance"], 400.0)
    try_set(c, ["fog_max_opacity"], 0.97)
    try_set(c, ["volumetric_fog", "enable_volumetric_fog"], True)
    try_set(c, ["volumetric_fog_scattering_distribution"], 0.75)   # light scatters forward: bright beams toward the sun
    try_set(c, ["volumetric_fog_albedo"], col((int(255 * M["fog"][0] + 120), int(255 * M["fog"][1] + 120), int(255 * M["fog"][2] + 120))))
    try_set(c, ["volumetric_fog_extinction_scale"], 1.6)
    try_set(c, ["volumetric_fog_distance"], 9000.0)
    log("fog: coloured and volumetric")

# ---- 4. colour grade
pp = first(unreal.PostProcessVolume)
if pp is None:
    pp = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
    pp.set_actor_label("PostProcess_Global")
    pp.set_folder_path("GrappleRunner/Lighting")
try_set(pp, ["unbound", "infinite_extent"], True)
s = pp.get_editor_property("settings")
V = unreal.Vector4
for flag, prop, val in [
        ("override_color_saturation", "color_saturation", V(1.0, 1.0, 1.0, SATURATION)),
        ("override_color_contrast", "color_contrast", V(1.0, 1.0, 1.0, CONTRAST)),
        ("override_color_gain_shadows", "color_gain_shadows", V(*M["shadows"], 1.0)),
        ("override_color_gain_highlights", "color_gain_highlights", V(*M["highs"], 1.0)),
        ("override_color_saturation_midtones", "color_saturation_midtones", V(1.0, 1.12, 1.0, 1.0)),
        ("override_white_temp", "white_temp", 6500.0),
        ("override_bloom_intensity", "bloom_intensity", BLOOM),
        ("override_vignette_intensity", "vignette_intensity", 0.0)]:     # no vignette: wrong in VR
    try:
        s.set_editor_property(flag, True)
        s.set_editor_property(prop, val)
    except Exception as e:
        unreal.log_warning("Grapple Runner atmosphere: {} skipped ({})".format(prop, e))
pp.set_editor_property("settings", s)
log("colour grade: mood '{}'".format(MOOD))


# ---- 5. glow: a glowing material, then orbs hanging from the canopies and lights by the ruins
def glow_material(name, rgb):
    """A material that gives off its own light (emissive), in one colour. Made once, re-coloured on re-runs."""
    path = "/Game/GrappleRunner/Materials/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        m = unreal.load_asset(path)
        mel.delete_all_material_expressions(m)
    else:
        m = tools.create_asset(name, "/Game/GrappleRunner/Materials", unreal.Material, unreal.MaterialFactoryNew())
    colour = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -400, 0)
    colour.set_editor_property("constant", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    strength = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -400, 160)
    strength.set_editor_property("r", 40.0)                      # bright enough to bloom
    times = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -200, 60)
    mel.connect_material_expressions(colour, "", times, "A")
    mel.connect_material_expressions(strength, "", times, "B")
    mel.connect_material_property(times, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_asset(path)
    return m


old = [a for a in actors.get_all_level_actors() if str(a.get_folder_path()) == GLOW_FOLDER]
if old:
    actors.destroy_actors(old)
glows = [(glow_material("M_Glow_%d" % i, rgb), rgb) for i, rgb in enumerate(M["orbs"])]
sphere = unreal.load_asset("/Engine/BasicShapes/Sphere")
vine_class = unreal.load_class(None, "/Script/GrappleRunner.GRVine")
trees = [a for a in actors.get_all_level_actors()
         if str(a.get_folder_path()) == "GrappleRunner/Trees" and "_tree_" in a.get_actor_label()]
random.shuffle(trees)
orbs = 0
for t in trees[:ORB_COUNT]:
    loc = t.get_actor_location()
    _, te = t.get_actor_bounds(False)
    ang = random.uniform(0, 2 * math.pi)
    reach = random.uniform(200, 420)
    branch = loc + unreal.Vector(math.cos(ang) * reach, math.sin(ang) * reach, 2 * te.z * random.uniform(0.45, 0.62))
    drop = random.uniform(120, 320)
    orb_at = branch - unreal.Vector(0, 0, drop)
    mat, rgb = random.choice(glows)
    if vine_class:                                         # the short vine it hangs from (never snaps)
        v = actors.spawn_actor_from_class(vine_class, branch, unreal.Rotator())
        if v:
            try_set(v, ["can_snap", "b_can_snap"], False)          # Python drops the "b" from true/false settings
            v.set_editor_property("sag", 0.0)
            v.set_editor_property("segments", 2)
            v.set_editor_property("thickness", 0.5)
            v.set_editor_property("end_point", unreal.Vector(0, 0, -drop))
            v.set_folder_path(GLOW_FOLDER)
    orb = actors.spawn_actor_from_object(sphere, orb_at, unreal.Rotator())
    if not orb:
        continue
    size = random.uniform(0.18, 0.4)                        # the engine sphere is 1 m: 18-40 cm orbs
    orb.set_actor_scale3d(unreal.Vector(size, size, size))
    c = orb.get_component_by_class(unreal.StaticMeshComponent)
    c.set_material(0, mat)
    c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    c.set_editor_property("cast_shadow", False)
    orb.set_folder_path(GLOW_FOLDER)
    orb.set_actor_label("GlowOrb_{:03d}".format(orbs))
    if orbs % ORB_LIGHT_EVERY == 0:
        light = actors.spawn_actor_from_class(unreal.PointLight, orb_at)
        lc = light.get_component_by_class(unreal.PointLightComponent)
        try_set(lc, ["light_color"], col((int(rgb[0] * 255), int(rgb[1] * 255), int(rgb[2] * 255))))
        try_set(lc, ["intensity"], 800.0)
        try_set(lc, ["attenuation_radius"], 900.0)
        try_set(lc, ["cast_shadows"], False)
        try_set(lc, ["volumetric_scattering_intensity"], 3.0)   # a soft halo in the fog
        light.set_folder_path(GLOW_FOLDER)
    orbs += 1
log("{} glowing orbs".format(orbs))

ruins = [a for a in actors.get_all_level_actors() if str(a.get_folder_path()) == "GrappleRunner/Ruins"
         and any(k in a.get_actor_label() for k in ("_column_", "_arch_", "_tower_", "_statue_", "_fountain_"))]
random.shuffle(ruins)
for r in ruins[:RUIN_LIGHTS]:
    o, e = r.get_actor_bounds(False)
    rgb = random.choice(M["orbs"])
    light = actors.spawn_actor_from_class(unreal.PointLight, unreal.Vector(o.x, o.y, o.z - e.z + 80))
    lc = light.get_component_by_class(unreal.PointLightComponent)
    try_set(lc, ["light_color"], col((int(rgb[0] * 255), int(rgb[1] * 255), int(rgb[2] * 255))))
    try_set(lc, ["intensity"], 1500.0)
    try_set(lc, ["attenuation_radius"], 1200.0)
    try_set(lc, ["cast_shadows"], False)
    try_set(lc, ["volumetric_scattering_intensity"], 2.0)
    light.set_folder_path(GLOW_FOLDER)
log("{} ruin lights".format(min(RUIN_LIGHTS, len(ruins))))

unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
try:
    unreal.AutomationLibrary.take_high_res_screenshot(1920, 1080, "GR_Atmosphere_%s.png" % MOOD)
except Exception:
    pass
log("done. Press Play and look around. To try another mood, change MOOD at the top and re-run.")
