"""
Grapple Runner: the "Aeloria" colour pass. Makes the level warm, lush and saturated instead of grey.

Run with the Floating Islands level open (not while playing):
    py "C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/color_grade.py"

THE PALETTE (why each change)
- Golden-hour sun: warm yellow-orange light, so lit surfaces glow and shadows read clearly.
- Blue-tinted sky light: shadows pick up a cool blue, and warm-against-cool is what makes an
  image look colourful rather than grey (the "teal and orange" look of films and Valorant maps).
- Turquoise mist below, with a golden glow in the fog toward the sun: depth that isn't grey.
- Colour grade (post-process): more saturation (greens and blues pop), a bit more contrast,
  cool shadows, warm highlights. No vignette: in VR a dark edge on the screen feels wrong.
Every number is at the top, so you can tune it: change one, re-run the script, compare.
Re-running is safe: it updates the same lights instead of adding new ones.

HOW TO READ THIS (no coding experience needed)
- Lines starting with # are notes for humans; the computer skips them.
- Colours are (Red, Green, Blue), 0 to 255 for lights, 0 to 1 for the grade.
"""
import unreal

# ---------------------------------------------------------------- SETTINGS
SUN_COLOR = (255, 208, 150)        # golden hour
SUN_LUX = 10.0
SKY_COLOR = (175, 205, 255)        # cool blue fill light in the shadows
SKY_INTENSITY = 1.6
FOG_COLOR = (0.30, 0.62, 0.78)     # turquoise mist
FOG_SUN_GLOW = (1.0, 0.72, 0.42)   # warm glow in the fog when looking toward the sun
SATURATION = 1.28                  # 1 = unchanged
CONTRAST = 1.12
SHADOW_TINT = (0.94, 1.0, 1.10)    # multiply shadows: a touch of blue
HIGHLIGHT_TINT = (1.07, 1.02, 0.92)  # multiply highlights: a touch of gold
MIDTONE_GREEN_BOOST = 1.10         # extra saturation on green, for the jungle
# -------------------------------------------------------------------------

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(text):
    unreal.log("Grapple Runner colour: " + text)


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
    unreal.log_warning("Grapple Runner colour: could not set {}".format(names[0]))
    return False


def col(rgb):
    return unreal.Color(r=rgb[0], g=rgb[1], b=rgb[2], a=255)


sun = first(unreal.DirectionalLight)
if sun:
    c = sun.get_component_by_class(unreal.DirectionalLightComponent)
    try_set(c, ["light_color"], col(SUN_COLOR))
    try_set(c, ["intensity"], SUN_LUX)
    log("sun: golden")

sky = first(unreal.SkyLight)
if sky:
    c = sky.get_component_by_class(unreal.SkyLightComponent)
    try_set(c, ["light_color"], col(SKY_COLOR))
    try_set(c, ["intensity"], SKY_INTENSITY)
    try:
        c.recapture_sky()
    except Exception:
        pass
    log("sky light: cool blue fill")

for fog in [a for a in actors.get_all_level_actors() if isinstance(a, unreal.ExponentialHeightFog)]:
    c = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
    try_set(c, ["fog_inscattering_luminance", "fog_inscattering_color"], unreal.LinearColor(*FOG_COLOR, 1.0))
    try_set(c, ["directional_inscattering_luminance", "directional_inscattering_color"],
            unreal.LinearColor(*FOG_SUN_GLOW, 1.0))
    try_set(c, ["directional_inscattering_exponent"], 6.0)
    log("fog: turquoise with a golden sun glow")

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
        ("override_color_gain_shadows", "color_gain_shadows", V(*SHADOW_TINT, 1.0)),
        ("override_color_gain_highlights", "color_gain_highlights", V(*HIGHLIGHT_TINT, 1.0)),
        ("override_color_saturation_midtones", "color_saturation_midtones", V(1.0, MIDTONE_GREEN_BOOST, 1.0, 1.0)),
        ("override_white_temp", "white_temp", 6500.0),          # neutral: the tints above do the warming
        ("override_bloom_intensity", "bloom_intensity", 0.65),
        ("override_vignette_intensity", "vignette_intensity", 0.0)]:
    try:
        s.set_editor_property(flag, True)
        s.set_editor_property(prop, val)
    except Exception as e:
        unreal.log_warning("Grapple Runner colour: {} skipped ({})".format(prop, e))
pp.set_editor_property("settings", s)
log("colour grade: saturated, cool shadows, warm highlights")

unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
try:
    unreal.AutomationLibrary.take_high_res_screenshot(1920, 1080, "GR_ColourPass.png")
except Exception:
    pass
log("done. Compare Saved/Screenshots before and after.")
