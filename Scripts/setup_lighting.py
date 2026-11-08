"""
Grapple Runner: lighting + sky pass for the Floating Islands level (UE 5.8, VR template).

Run with the Floating Islands level open:
    py "C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/setup_lighting.py"

- Makes sun and sky light fully dynamic, so the level never needs a lighting build
  (gets rid of "LIGHTING NEEDS TO BE REBUILT").
- Moves the planet ground far below the islands so the horizon is hazy sky, not black.
- Thick low fog = the sea of mist under the islands; light fog higher up for depth.
- Adds an unbound post-process volume: fixed exposure (stable in VR), soft bloom,
  slightly warm grade for the semi-real look.
- Points the editor camera down the route and saves a screenshot to
  Saved/Screenshots/ so the result can be checked without opening the editor.
Safe to re-run: it updates the actors it finds instead of adding duplicates.

HOW TO READ THIS (no coding experience needed)
- Lines starting with # are notes for humans; the computer skips them.
- Text inside triple quotes (like this block) is also just a note.
- "import" = load a toolbox. "unreal" is Unreal Engine's own Python toolbox.
- "def name(...):" defines a reusable recipe; the indented lines under it are its steps.
- "for x in ...:" repeats the indented steps once per item; "if ...:" only runs them when true.
- Unreal distances are in centimetres (100 = 1 metre).
"""
import unreal

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
FOLDER = "GrappleRunner/Lighting"

SUN_PITCH, SUN_YAW = -24.0, -35.0     # low warm sun = long shadows under the islands
SUN_LUX = 9.0
FOG_Z = -2500.0                       # fog base, below the route (route starts at z=0)


def log(msg):
    unreal.log("Grapple Runner lighting: " + msg)


def try_set(obj, names, value):
    for n in names:
        try:
            obj.set_editor_property(n, value)
            return True
        except Exception:
            pass
    unreal.log_warning("Grapple Runner lighting: could not set {} on {}".format(names[0], obj.get_name()))
    return False


def find_or_spawn(cls, label, loc=unreal.Vector(0, 0, 0), rot=unreal.Rotator()):
    for a in actors.get_all_level_actors():
        if isinstance(a, cls):
            return a
    a = actors.spawn_actor_from_class(cls, loc, rot)
    a.set_actor_label(label)
    a.set_folder_path(FOLDER)
    return a


def movable(comp):
    try:
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
    except Exception:
        try_set(comp, ["mobility"], unreal.ComponentMobility.MOVABLE)


# ---- sun
sun = find_or_spawn(unreal.DirectionalLight, "Sun")
sun.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=SUN_PITCH, yaw=SUN_YAW), False)
sc = sun.get_component_by_class(unreal.DirectionalLightComponent)
movable(sc)
try_set(sc, ["intensity"], SUN_LUX)
try_set(sc, ["light_color"], unreal.Color(r=255, g=236, b=210, a=255))
try_set(sc, ["light_source_angle"], 1.2)
try_set(sc, ["atmosphere_sun_light"], True)
try_set(sc, ["dynamic_shadow_distance_movable_light"], 25000.0)
try_set(sc, ["use_ray_traced_distance_field_shadows", "ray_traced_distance_field_shadows"], True)
try_set(sc, ["distance_field_shadow_distance"], 60000.0)
log("sun set")

# ---- sky atmosphere: put the planet surface 6 km below the islands
sky_atmo = find_or_spawn(unreal.SkyAtmosphere, "SkyAtmosphere")
ac = sky_atmo.get_component_by_class(unreal.SkyAtmosphereComponent)
try_set(ac, ["transform_mode"], unreal.SkyAtmosphereTransformMode.PLANET_TOP_AT_COMPONENT_TRANSFORM)
sky_atmo.set_actor_location(unreal.Vector(0, 0, -600000.0), False, False)
try_set(ac, ["ground_albedo"], unreal.Color(r=150, g=158, b=165, a=255))
try_set(ac, ["aerial_pespective_view_distance_scale", "aerial_perspective_view_distance_scale"], 2.5)
log("sky atmosphere set")

# ---- sky light, real-time capture of the atmosphere
sky = find_or_spawn(unreal.SkyLight, "SkyLight", unreal.Vector(0, 0, 3000))
kc = sky.get_component_by_class(unreal.SkyLightComponent)
movable(kc)
try_set(kc, ["real_time_capture"], True)
try_set(kc, ["intensity"], 1.2)
try_set(kc, ["lower_hemisphere_is_black"], False)
log("sky light set")

# ---- clouds
find_or_spawn(unreal.VolumetricCloud, "Clouds")

# ---- fog: sea of mist below, light haze above
fog = find_or_spawn(unreal.ExponentialHeightFog, "LowFog")
fog.set_actor_location(unreal.Vector(40000.0, 0, FOG_Z), False, False)
fc = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
try_set(fc, ["fog_density"], 0.035)
try_set(fc, ["fog_height_falloff"], 0.0035)
try_set(fc, ["fog_inscattering_luminance", "fog_inscattering_color"],
        unreal.LinearColor(0.55, 0.62, 0.72, 1.0))
try_set(fc, ["start_distance"], 1500.0)
try_set(fc, ["volumetric_fog", "enable_volumetric_fog"], True)
log("fog set")

# ---- post process
pp = None
for a in actors.get_all_level_actors():
    if isinstance(a, unreal.PostProcessVolume):
        pp = a
        break
if pp is None:
    pp = actors.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
    pp.set_actor_label("PostProcess_Global")
    pp.set_folder_path(FOLDER)
try_set(pp, ["unbound", "infinite_extent"], True)
s = pp.get_editor_property("settings")
for flag, prop, val in [
        ("override_auto_exposure_method", "auto_exposure_method", unreal.AutoExposureMethod.AEM_MANUAL),
        ("override_auto_exposure_bias", "auto_exposure_bias", 10.5),
        ("override_bloom_intensity", "bloom_intensity", 0.5),
        ("override_white_temp", "white_temp", 6200.0),
        ("override_color_saturation", "color_saturation", unreal.Vector4(1.05, 1.05, 1.05, 1.0)),
        ("override_vignette_intensity", "vignette_intensity", 0.0)]:
    try:
        s.set_editor_property(flag, True)
        s.set_editor_property(prop, val)
    except Exception as e:
        unreal.log_warning("Grapple Runner lighting: post-process {} skipped ({})".format(prop, e))
pp.set_editor_property("settings", s)
log("post process set")

# ---- camera + screenshot for checking
ues.set_level_viewport_camera_info(unreal.Vector(-6000.0, -4200.0, 2800.0),
                                   unreal.Rotator(roll=0.0, pitch=-6.0, yaw=24.0))
levels.save_current_level()
try:
    unreal.AutomationLibrary.take_high_res_screenshot(1920, 1080, "GR_Lighting.png")
    log("screenshot requested: Saved/Screenshots/.../GR_Lighting.png")
except Exception as e:
    unreal.log_warning("Grapple Runner lighting: screenshot skipped ({})".format(e))
log("done")
