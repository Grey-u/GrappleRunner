"""
Grapple Runner: Floating Islands greybox level builder.

Run inside the Unreal Editor (UE 5.x) with the Python Editor Script Plugin enabled:
    Output Log > Cmd box >  py "C:/path/to/build_floating_islands.py"

What it does
- Creates a NEW level (never overwrites): /Game/GrappleRunner/Maps/L_FloatingIslands(_N)
- Finds your floating-island / rock meshes in /Game by name and uses them.
  If none are found it falls back to engine cubes/spheres so you still get a greybox.
- Lays out a swing route along +X:
    landing islands every 45-70 m, with 2-3 grapple anchors between each pair,
    anchors 11-21 m above the route and 7-16 m apart (same numbers as the physics bench).
  Every anchor is tagged "GrappleAnchor" so the C++ grapple can find them later.
- Adds a ring of large background islands, sky, sun, volumetric clouds and low fog
  for the misty look from your screenshot.
- Adds a PlayerStart on the first island and saves the level.

Tuning: change the numbers in the SETTINGS block. Re-running makes a new level each time.
Units are Unreal units (1 uu = 1 cm).

HOW TO READ THIS (no coding experience needed)
- Lines starting with # are notes for humans; the computer skips them.
- Text inside triple quotes (like this block) is also just a note.
- "import" = load a toolbox. "unreal" is Unreal Engine's own Python toolbox.
- "def name(...):" defines a reusable recipe; the indented lines under it are its steps.
- "for x in ...:" repeats the indented steps once per item; "if ...:" only runs them when true.
- Unreal distances are in centimetres (100 = 1 metre).
"""

import math
import random
import unreal

# ---------------------------------------------------------------- SETTINGS
SEED = 7                      # change for a different layout
ROUTE_ISLANDS = 14            # landing islands on the swing route
ISLAND_GAP = (4500, 7000)     # cm between landing islands along X
ROUTE_DRIFT_Y = 2500          # cm sideways wiggle of the route
ROUTE_RISE = (-600, 1200)     # cm height change per island (gentle climb overall)
LANDING_SIZE = (1500, 2800)   # cm target width of landing islands
ANCHOR_HEIGHT = (1100, 2100)  # cm above the route line (bench: 11-21 m)
ANCHOR_SPACING = (700, 1600)  # cm between anchors (bench: 7-16 m)
ANCHOR_SIZE = (250, 450)      # cm target width of anchor rocks
BACKDROP_ISLANDS = 45         # big scenery islands around the route
BACKDROP_RADIUS = (25000, 80000)
BACKDROP_SIZE = (4000, 12000)
FOG_HEIGHT = -6000            # cm, fog sits below the islands

ISLAND_WORDS = ["floatingisland", "floatingmountain", "island", "skyrock"]
ROCK_WORDS = ["rock_0", "stalagtite", "stalactite", "cliff_spiky", "boulder", "spire", "pillar"]
SKIP_WORDS = ["pebble", "scree", "snow", "desert", "slab", "beach", "dry", "mud"]
BACKDROP_EXTRA_WORDS = ["cliff_large"]
# Brushify Floating Islands blueprints (islands with their dressing) are used first if present
BRUSHIFY_ISLAND_BPS = [
    "/Game/Brushify/Meshes/FloatingIslands/FloatingIsland_0{0}/BP_FloatingIsland_0{0}".format(i)
    for i in range(1, 5)]
LEVEL_BASE = "/Game/GrappleRunner/Maps/L_FloatingIslands"
# -------------------------------------------------------------------------

random.seed(SEED)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
log = unreal.log


def find_meshes(words):
    """Static meshes under /Game whose asset name contains any of the words."""
    reg = unreal.AssetRegistryHelpers.get_asset_registry()
    try:
        flt = unreal.ARFilter(
            class_paths=[unreal.TopLevelAssetPath("/Script/Engine", "StaticMesh")],
            package_paths=["/Game"], recursive_paths=True)
    except Exception:  # UE 5.0 style
        flt = unreal.ARFilter(class_names=["StaticMesh"],
                              package_paths=["/Game"], recursive_paths=True)
    found = []
    for data in reg.get_assets(flt):
        name = str(data.asset_name).lower()
        if any(w in name for w in words) and not any(s in name for s in SKIP_WORDS):
            mesh = data.get_asset()
            if mesh:
                found.append(mesh)
    return found


def load_existing(paths):
    out = []
    for p in paths:
        if unreal.EditorAssetLibrary.does_asset_exist(p):
            a = unreal.load_asset(p)
            if a:
                out.append(a)
    return out


def bounds(actor):
    origin, ext = actor.get_actor_bounds(False)
    return origin, ext


def place(asset, loc, target_width, folder, yaw=None, tags=None, label=None,
          top_align=False):
    """Spawn a static mesh or blueprint, scale it to target_width (cm) and
    optionally move it so its top surface sits at loc.z (for landing islands)."""
    rot = unreal.Rotator(roll=0.0, pitch=0.0,
                         yaw=random.uniform(0, 360) if yaw is None else yaw)
    actor = actors.spawn_actor_from_object(asset, loc, rot)
    if actor is None:
        unreal.log_warning("Grapple Runner: could not spawn " + asset.get_name())
        return None
    _, ext = bounds(actor)
    width = max(2.0 * max(ext.x, ext.y), 1.0)
    s = target_width / width
    actor.set_actor_scale3d(unreal.Vector(s, s, s))
    if top_align:
        origin, ext = bounds(actor)
        dz = loc.z - (origin.z + ext.z)
        actor.set_actor_location(actor.get_actor_location() + unreal.Vector(0, 0, dz),
                                 False, False)
    actor.set_folder_path(folder)
    if tags:
        actor.set_editor_property("tags", [unreal.Name(t) for t in tags])
    if label:
        actor.set_actor_label(label)
    return actor


def spawn_class(cls, loc, rot=None, folder="GrappleRunner/Lighting", label=None):
    a = actors.spawn_actor_from_class(cls, loc, rot or unreal.Rotator())
    a.set_folder_path(folder)
    if label:
        a.set_actor_label(label)
    return a


# ---- 1. level: if a Floating Islands level is already open, clear what this
# script built before and rebuild it in place; otherwise make a new level
# (never overwrites another level).
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
current = world.get_path_name().split(".")[0] if world else ""
if current.startswith(LEVEL_BASE):
    path = current
    old = [a for a in actors.get_all_level_actors()
           if str(a.get_folder_path()).startswith("GrappleRunner")]
    if old:
        actors.destroy_actors(old)
    log("Grapple Runner: rebuilding {} (cleared {} actors)".format(path, len(old)))
else:
    path, n = LEVEL_BASE, 1
    while unreal.EditorAssetLibrary.does_asset_exist(path):
        n += 1
        path = "{}_{}".format(LEVEL_BASE, n)
    levels.new_level(path)
    log("Grapple Runner: building " + path)

# ---- 2. meshes
islands = load_existing(BRUSHIFY_ISLAND_BPS) or find_meshes(ISLAND_WORDS)
rocks = find_meshes(ROCK_WORDS)
backdrop = islands + find_meshes(BACKDROP_EXTRA_WORDS)
cube = unreal.load_asset("/Engine/BasicShapes/Cube")
sphere = unreal.load_asset("/Engine/BasicShapes/Sphere")
if not islands:
    islands = rocks or [cube]
    backdrop = islands
if not rocks:
    rocks = [sphere]
log("Grapple Runner: {} island assets, {} anchor rocks, {} backdrop assets".format(
    len(islands), len(rocks), len(backdrop)))

# ---- 3. swing route
route = []
x, y, z = 0.0, 0.0, 0.0
for i in range(ROUTE_ISLANDS):
    if i:
        x += random.uniform(*ISLAND_GAP)
        y = max(-ROUTE_DRIFT_Y, min(ROUTE_DRIFT_Y, y + random.uniform(-1500, 1500)))
        z += random.uniform(*ROUTE_RISE)
    loc = unreal.Vector(x, y, z)
    route.append(loc)
    w = random.uniform(*LANDING_SIZE) * (1.4 if i in (0, ROUTE_ISLANDS - 1) else 1.0)
    place(random.choice(islands), loc, w, "GrappleRunner/Route",
          tags=["LandingIsland"], label="Landing_{:02d}".format(i), top_align=True)

# anchors between each pair of landing islands
count = 0
for a, b in zip(route, route[1:]):
    span = b.x - a.x
    t = random.uniform(*ANCHOR_SPACING)
    while t < span - 600:
        f = t / span
        base_z = a.z + (b.z - a.z) * f
        loc = unreal.Vector(a.x + t,
                            a.y + (b.y - a.y) * f + random.uniform(-500, 500),
                            base_z + random.uniform(*ANCHOR_HEIGHT))
        place(random.choice(rocks), loc, random.uniform(*ANCHOR_SIZE),
              "GrappleRunner/Anchors", tags=["GrappleAnchor"],
              label="Anchor_{:03d}".format(count))
        count += 1
        t += random.uniform(*ANCHOR_SPACING)
log("Grapple Runner: {} grapple anchors".format(count))

# ---- 4. backdrop islands around the route
mid_x = route[-1].x / 2.0
for i in range(BACKDROP_ISLANDS):
    ang = math.radians(random.uniform(0, 360))
    r = random.uniform(*BACKDROP_RADIUS)
    loc = unreal.Vector(mid_x + r * math.cos(ang), r * math.sin(ang),
                        random.uniform(-4000, 9000))
    place(random.choice(backdrop), loc, random.uniform(*BACKDROP_SIZE),
          "GrappleRunner/Backdrop", label="Backdrop_{:02d}".format(i))

# ---- 5. sky, sun, clouds, fog
sun = spawn_class(unreal.DirectionalLight, unreal.Vector(0, 0, 5000),
                  unreal.Rotator(roll=0, pitch=-32, yaw=-40), label="Sun")
sun_comp = sun.get_component_by_class(unreal.DirectionalLightComponent)
sun_comp.set_editor_property("intensity", 8.0)
try:
    sun_comp.set_editor_property("atmosphere_sun_light", True)
except Exception:
    pass

spawn_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), label="SkyAtmosphere")
sky = spawn_class(unreal.SkyLight, unreal.Vector(0, 0, 3000), label="SkyLight")
sky_comp = sky.get_component_by_class(unreal.SkyLightComponent)
try:
    sky_comp.set_editor_property("real_time_capture", True)
except Exception:
    pass
spawn_class(unreal.VolumetricCloud, unreal.Vector(0, 0, 0), label="Clouds")
fog = spawn_class(unreal.ExponentialHeightFog, unreal.Vector(mid_x, 0, FOG_HEIGHT),
                  label="LowFog")
fog_comp = fog.get_component_by_class(unreal.ExponentialHeightFogComponent)
def try_set(comp, names, value):
    """Set the first property name that exists (names change between engine versions)."""
    for n in names:
        try:
            comp.set_editor_property(n, value)
            return True
        except Exception:
            pass
    unreal.log_warning("Grapple Runner: could not set {} on {}".format(names[0], comp.get_name()))
    return False


try_set(fog_comp, ["fog_density"], 0.05)
try_set(fog_comp, ["fog_height_falloff"], 0.02)
try_set(fog_comp, ["volumetric_fog", "enable_volumetric_fog", "b_enable_volumetric_fog"], True)

# ---- 6. player start on the first island
first = route[0]
spawn_class(unreal.PlayerStart, unreal.Vector(first.x, first.y, first.z + 200),
            folder="GrappleRunner", label="PlayerStart")

levels.save_current_level()
unreal.log("Grapple Runner: done. Level saved at " + path)
