"""
Grapple Runner: Brushify-style sky dome, sea of mist and waterfalls (UE 5.8, forward-shaded VR project).

Brushify's own Floating Islands map lights its sky with a textured sky dome mesh,
not the physical Sky Atmosphere, so this does the same:
- removes Sky Atmosphere + Volumetric Cloud (they render black below the horizon here)
- adds Brushify's S_Skydome with its HDR sky material, around the whole route
- lays animated fog cards (Brushify's mist planes) in layers under the islands
- adds Niagara waterfalls pouring off a few landing islands
- softens the height fog so far islands stay readable instead of washing out white
- recaptures the sky light, saves, and writes Saved/Screenshots/.../GR_Sky.png
Safe to re-run.

HOW TO READ THIS (no coding experience needed)
- Lines starting with # are notes for humans; the computer skips them.
- Text inside triple quotes (like this block) is also just a note.
- "import" = load a toolbox. "unreal" is Unreal Engine's own Python toolbox.
- "def name(...):" defines a reusable recipe; the indented lines under it are its steps.
- "for x in ...:" repeats the indented steps once per item; "if ...:" only runs them when true.
- Unreal distances are in centimetres (100 = 1 metre).
"""
import random
import unreal

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
random.seed(11)

SKYDOME = "/Game/Brushify/Materials/Sky/S_Skydome"
SKY_MI = "/Game/Brushify/Maps/FloatingIslands/MaterialOverrides/MI_SkyHDR_Inst"
SKY_MI_FALLBACK = "/Game/Brushify/Materials/Sky/MI_SkyHDR_Inst"
FOG_CARD_MIS = ["/Game/Brushify/Maps/FloatingIslands/PFX/FogCards/MI_AnimatedCard_Slow",
                "/Game/Brushify/Maps/FloatingIslands/PFX/FogCards/MI_AnimatedCard_Fast"]
PLANE = "/Engine/BasicShapes/Plane"
WATERFALLS = ["/Game/Brushify/Maps/FloatingIslands/PFX/NS_WaterFall_Large",
              "/Game/Brushify/Maps/FloatingIslands/PFX/NS_WaterFall_Huge",
              "/Game/Brushify/Maps/FloatingIslands/PFX/NS_WaterFall_Small"]
FOLDER_SKY = "GrappleRunner/Lighting"
FOLDER_MIST = "GrappleRunner/Mist"
FOLDER_FX = "GrappleRunner/Waterfalls"


def log(m):
    unreal.log("Grapple Runner sky: " + m)


def load(path):
    return unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None


def try_set(obj, names, value):
    for n in names:
        try:
            obj.set_editor_property(n, value)
            return True
        except Exception:
            pass
    unreal.log_warning("Grapple Runner sky: could not set {} on {}".format(names[0], obj.get_name()))
    return False


all_actors = actors.get_all_level_actors()

# clear previous runs of this script and the physical sky
remove = [a for a in all_actors
          if isinstance(a, (unreal.SkyAtmosphere, unreal.VolumetricCloud))
          or str(a.get_folder_path()) in (FOLDER_MIST, FOLDER_FX)
          or a.get_actor_label().startswith("SkyDome")]
if remove:
    actors.destroy_actors(remove)
log("removed {} old sky/mist actors".format(len(remove)))

# route extent from the landing islands
landing = [a for a in actors.get_all_level_actors() if a.get_actor_label().startswith("Landing_")]
xs = [a.get_actor_location().x for a in landing] or [0.0, 80000.0]
mid_x = (min(xs) + max(xs)) / 2.0

# ---- sky dome
dome_mesh = load(SKYDOME)
if dome_mesh:
    dome = actors.spawn_actor_from_object(dome_mesh, unreal.Vector(mid_x, 0, -20000), unreal.Rotator())
    dome.set_actor_label("SkyDome")
    dome.set_folder_path(FOLDER_SKY)
    _, ext = dome.get_actor_bounds(False)
    radius = max(ext.x, ext.y, 1.0)
    s = 400000.0 / radius               # 4 km radius, well past the backdrop islands
    dome.set_actor_scale3d(unreal.Vector(s, s, s))
    smc = dome.get_component_by_class(unreal.StaticMeshComponent)
    mi = load(SKY_MI) or load(SKY_MI_FALLBACK)
    if mi:
        smc.set_material(0, mi)
    try_set(smc, ["cast_shadow"], False)
    # The dome surrounds the whole world, so it must never be solid: if it is, the
    # player starts "inside" it and gets pushed 3.6 km out (found in test T-02).
    smc.set_collision_profile_name("NoCollision")
    smc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    log("sky dome placed, scale {:.1f}".format(s))
else:
    unreal.log_warning("Grapple Runner sky: Brushify S_Skydome not found")

# ---- sky light: capture the dome
for a in actors.get_all_level_actors():
    if isinstance(a, unreal.SkyLight):
        kc = a.get_component_by_class(unreal.SkyLightComponent)
        try_set(kc, ["real_time_capture"], False)
        try_set(kc, ["source_type"], unreal.SkyLightSourceType.SLS_CAPTURED_SCENE)
        try_set(kc, ["intensity"], 1.4)
        try_set(kc, ["lower_hemisphere_is_black"], False)
        try:
            kc.recapture_sky()
        except Exception:
            pass

# ---- height fog: gentler so distant islands keep their shape
for a in actors.get_all_level_actors():
    if isinstance(a, unreal.ExponentialHeightFog):
        a.set_actor_location(unreal.Vector(mid_x, 0, -3000.0), False, False)
        fc = a.get_component_by_class(unreal.ExponentialHeightFogComponent)
        try_set(fc, ["fog_density"], 0.012)
        try_set(fc, ["fog_height_falloff"], 0.0025)
        try_set(fc, ["fog_inscattering_luminance", "fog_inscattering_color"],
                unreal.LinearColor(0.62, 0.7, 0.8, 1.0))
        try_set(fc, ["start_distance"], 3000.0)
        try_set(fc, ["fog_max_opacity"], 0.92)

# ---- sea of mist: layers of animated fog cards under the route
plane = load(PLANE)
cards = [m for m in (load(p) for p in FOG_CARD_MIS) if m]
n = 0
if plane and cards:
    span = (max(xs) - min(xs)) + 60000.0
    for layer, z in enumerate([-2500.0, -4500.0, -7000.0]):
        for i in range(6):
            for j in (-1, 0, 1):
                loc = unreal.Vector(min(xs) - 30000.0 + span * (i + 0.5) / 6.0 + random.uniform(-3000, 3000),
                                    j * 22000.0 + random.uniform(-3000, 3000),
                                    z + random.uniform(-400, 400))
                card = actors.spawn_actor_from_object(plane, loc, unreal.Rotator(yaw=random.uniform(0, 360)))
                size = random.uniform(200.0, 300.0)          # plane is 1 m: 200-300 m cards
                card.set_actor_scale3d(unreal.Vector(size, size, 1.0))
                smc = card.get_component_by_class(unreal.StaticMeshComponent)
                smc.set_material(0, cards[(layer + i + j) % len(cards)])
                try_set(smc, ["cast_shadow"], False)
                try_set(smc, ["collision_enabled"], unreal.CollisionEnabled.NO_COLLISION)
                card.set_actor_label("Mist_{:02d}".format(n))
                card.set_folder_path(FOLDER_MIST)
                n += 1
log("{} mist cards".format(n))

# ---- waterfalls off a few landing islands
falls = [f for f in (load(p) for p in WATERFALLS) if f]
w = 0
if falls and landing:
    for a in random.sample(landing, min(5, len(landing))):
        origin, ext = a.get_actor_bounds(False)
        side = random.choice([1, -1])
        loc = unreal.Vector(origin.x + random.uniform(-0.3, 0.3) * ext.x,
                            origin.y + side * ext.y * 0.85,
                            origin.z + ext.z * 0.7)
        fx = actors.spawn_actor_from_object(random.choice(falls), loc, unreal.Rotator(yaw=90 * side))
        if fx:
            fx.set_actor_label("Waterfall_{:02d}".format(w))
            fx.set_folder_path(FOLDER_FX)
            w += 1
log("{} waterfalls".format(w))

ues.set_level_viewport_camera_info(unreal.Vector(-6000.0, -4200.0, 2800.0),
                                   unreal.Rotator(roll=0.0, pitch=-6.0, yaw=24.0))
levels.save_current_level()
try:
    unreal.AutomationLibrary.take_high_res_screenshot(1920, 1080, "GR_Sky.png")
except Exception as e:
    unreal.log_warning("Grapple Runner sky: screenshot skipped ({})".format(e))
log("done")
