"""
Grapple Runner: scatter bushes, heather, flowers, clover and grass on the landing islands.

Run with the Floating Islands level open (not while playing):
    py "C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/scatter_foliage.py"

What it does
- For each landing island, picks random spots, fires an invisible ray straight down, and
  if it lands on the island's TOP (a surface facing mostly upward), plants something there.
- Mostly grass and clover, some flowers, a few bushes and heather: like a real meadow.
- Plants are NOT solid (no collision): they must never stop the player or block a hook.
- Keeps a clear circle around the PlayerStart so you never spawn inside a bush.
- Everything goes in the GrappleRunner/Foliage folder of the Outliner. Re-running clears
  the old plants first, so you can change the numbers below and try again.
Units: centimetres (100 = 1 metre).

HOW TO READ THIS (no coding experience needed)
- Lines starting with # are notes for humans; the computer skips them.
- "import" loads a toolbox. "unreal" is Unreal Engine's own Python toolbox.
- "def name(...):" defines a reusable recipe; the indented lines are its steps.
- "for x in ...:" repeats the indented steps once per item; "if ...:" only runs them when true.
"""
import random
import unreal

# ---------------------------------------------------------------- SETTINGS
SEED = 21                    # change for a different arrangement
TRIES_PER_ISLAND = 90        # random spots tried per island (some miss the top and are skipped)
CLEAR_RADIUS = 400           # cm kept empty around the PlayerStart
FOLDER = "GrappleRunner/Foliage"
F = "/Game/Brushify/Materials/Landscape/ProceduralMeshes/Foliage/"
B = "/Game/Brushify/Meshes/Foliage/Bushes/"
# (mesh path, how often to pick it, smallest scale, biggest scale)
PLANTS = [
    (F + "Grass/S_LongGrass", 10, 0.8, 1.4),
    (F + "Grass/S_ShortGrass", 10, 0.8, 1.3),
    (F + "Grass/S_Grass", 8, 0.8, 1.3),
    (F + "Clovers/S_Clovers_01", 5, 0.8, 1.2),
    (F + "Clovers/S_Clovers_02", 5, 0.8, 1.2),
    (F + "Flowers/S_Flower_02_Blue", 3, 0.8, 1.2),
    (F + "Flowers/S_Flower_02_Pink", 3, 0.8, 1.2),
    (F + "Flowers/S_Flower_02_Yellow", 3, 0.8, 1.2),
    (F + "Flowers/S_Flowers_01_Small", 3, 0.8, 1.2),
    (B + "MoorlandHeather/S_MoorlandHeather_Purple_01", 2, 0.7, 1.1),
    (B + "MoorlandHeather/S_MoorlandHeather_Ferns_01", 2, 0.7, 1.1),
    (B + "GenericBush/S_GenericBush_Small", 2, 0.7, 1.0),
    (B + "GenericBush/S_GenericBush_Medium", 1, 0.6, 0.9),
]
# -------------------------------------------------------------------------

random.seed(SEED)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

# load the meshes that exist (skip any that this copy of the pack doesn't have)
plants, weights = [], []
for path, weight, lo, hi in PLANTS:
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        plants.append((unreal.load_asset(path), lo, hi))
        weights.append(weight)
    else:
        unreal.log_warning("Grapple Runner foliage: missing " + path)

# clear the plants from a previous run
old = [a for a in actors.get_all_level_actors() if str(a.get_folder_path()) == FOLDER]
if old:
    actors.destroy_actors(old)

start = None
for a in actors.get_all_level_actors():
    if isinstance(a, unreal.PlayerStart):
        start = a.get_actor_location()

islands = [a for a in actors.get_all_level_actors() if a.get_actor_label().startswith("Landing_")]
placed = 0
for island in islands:
    origin, ext = island.get_actor_bounds(False)
    for _ in range(TRIES_PER_ISLAND):
        # a random spot within the middle 80% of the island, seen from above
        x = origin.x + random.uniform(-0.8, 0.8) * ext.x
        y = origin.y + random.uniform(-0.8, 0.8) * ext.y
        top = unreal.Vector(x, y, origin.z + ext.z + 500)
        bottom = unreal.Vector(x, y, origin.z - ext.z)
        hit = unreal.SystemLibrary.line_trace_single(
            world, top, bottom, unreal.TraceTypeQuery.ECC_VISIBILITY, True, [],
            unreal.DrawDebugTrace.NONE, True)
        if not hit:
            continue
        h = hit.to_tuple()
        point, normal, actor = h[5], h[7], h[9]
        if actor != island or normal.z < 0.8:      # must be THIS island's top, fairly flat
            continue
        if start and (point - start).length() < CLEAR_RADIUS:
            continue                                # keep the spawn spot clear
        mesh, lo, hi = random.choices(plants, weights)[0]
        rot = unreal.Rotator(roll=0.0, pitch=0.0, yaw=random.uniform(0, 360))
        plant = actors.spawn_actor_from_object(mesh, point, rot)
        if not plant:
            continue
        s = random.uniform(lo, hi)
        plant.set_actor_scale3d(unreal.Vector(s, s, s))
        comp = plant.get_component_by_class(unreal.StaticMeshComponent)
        comp.set_collision_profile_name("NoCollision")             # never blocks the player or hooks
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        comp.set_editor_property("cast_shadow", False)            # cheaper for VR frame rate
        plant.set_folder_path(FOLDER)
        placed += 1

unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
unreal.log("Grapple Runner foliage: planted {} plants on {} islands".format(placed, len(islands)))
