"""
Grapple Runner: dress the islands with the free photoscanned Megascans assets (12 Nov 2026).

Run with the Floating Islands level open (not while playing), Output Log in Python mode:
    exec(open("C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/dress_megascans.py").read())

WHAT IT DOES (fixes "it's too bare" and "it's low poly")
- Finds every Megascans model you added with the Fab panel (anything under /Game/Fab), and sorts
  it by its name into: cliffs, rocks, roots, plants, hanging plants.
- UNDERSIDES: under every landing island it hangs 3-6 scanned cliffs upside down, so each island
  is a craggy floating rock instead of a smooth low-poly blob.
- RIMS: scanned boulders round the edge of each island top.
- TOPS: dense scanned plants (big leaves, tree ferns, palms) and roots, in clumps, like a jungle.
- EDGES: vines and creepers spilling over the sides.
- Keeps clear: 6 m round the PlayerStart and 7 m round every grapple rock, as before.
- Re-running clears its own previous pieces first (folder "Dressing/Megascans"). Change SEED
  for a different layout.
Units: centimetres (100 = 1 m).

HOW TO READ THIS (no coding experience needed)
- Lines starting with # are notes for humans; the computer skips them.
- "def name(...):" defines a reusable recipe; the indented lines under it are its steps.
- "for x in ...:" repeats the indented steps once per item; "if ...:" only runs them when true.
"""
import math
import random
import unreal

# ---------------------------------------------------------------- SETTINGS
SEED = 12
UNDERSIDE_CLIFFS = (3, 6)      # how many cliffs hang under each island (min, max)
RIM_ROCKS = (5, 10)
PLANT_CLUMPS = (6, 12)         # clumps of plants per island top...
PLANTS_PER_CLUMP = (4, 9)      # ...and plants in each clump
ROOTS_PER_ISLAND = (2, 4)
HANGERS = (5, 10)              # vines over the edge
CLEAR_START = 600
CLEAR_ANCHOR = 700
PLANT_DRAW = 20000             # plants further than 200 m aren't drawn (frame time in VR)
FOLDER = "Dressing/Megascans"

# Which word in a model's name puts it in which group (first match wins).
GROUPS = [
    ("hang", ["vine", "creeping", "ivy", "hanging", "fig"]),
    ("roots", ["root"]),
    ("cliff", ["cliff", "formation", "coastal", "ledge"]),
    ("rock", ["rock", "boulder", "stone"]),
    ("plant", ["fern", "palm", "elephant", "knotweed", "ginkgo", "cherry", "plant", "bush", "leaf", "megaplant",
               "banana", "pandanus", "pn_"]),
]

random.seed(SEED)
world = unreal.EditorLevelLibrary.get_editor_world()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)


def log(text):
    unreal.log("Grapple Runner Megascans: " + text)


# ---------------------------------------------------------------- 1. find the models
registry = unreal.AssetRegistryHelpers.get_asset_registry()
found = {g: [] for g, _ in GROUPS}
# Our photoscanned Project Nature ferns (the bushes Naomi liked) are used for the plant clumps too.
for root in ("/Game/Fab", "/Game/Megascans", "/Game/PN_Fern"):   # 12 Nov: no banana/pandanus/tropical (Naomi didn't like them)
    if not unreal.EditorAssetLibrary.does_directory_exist(root):
        continue
    for data in registry.get_assets_by_path(root, recursive=True):
        if str(data.asset_class_path.asset_name) != "StaticMesh":
            continue
        path = str(data.package_name)
        name = path.lower()
        for group, words in GROUPS:
            if any(w in name for w in words):
                found[group].append(path)
                break
for g, items in found.items():
    log("{}: {} models".format(g, len(items)))


def mesh(path):
    return unreal.load_asset(path)


# ---------------------------------------------------------------- 2. clear the last run
old = [a for a in actors.get_all_level_actors() if str(a.get_folder_path()).startswith(FOLDER)]
if old:
    actors.destroy_actors(old)

start = None
for a in actors.get_all_level_actors():
    if isinstance(a, unreal.PlayerStart):
        start = a.get_actor_location()
anchors = [a.get_actor_location() for a in actors.get_all_level_actors() if a.actor_has_tag("GrappleAnchor")]
islands = sorted([a for a in actors.get_all_level_actors() if a.get_actor_label().startswith("Landing_")],
                 key=lambda a: a.get_actor_label())
log("{} islands".format(len(islands)))


def ground(island, x, y):
    """Straight down at (x, y): the point on THIS island's top, or None."""
    origin, ext = island.get_actor_bounds(False)
    hit = unreal.SystemLibrary.line_trace_single(
        world, unreal.Vector(x, y, origin.z + ext.z + 3000), unreal.Vector(x, y, origin.z - ext.z - 100),
        unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], unreal.DrawDebugTrace.NONE, True)
    if not hit:
        return None
    h = hit.to_tuple()
    point, normal, actor = h[5], h[7], h[9]
    if actor != island or normal.z < 0.6:
        return None
    return point


def is_clear(p, radius):
    if start and (p - start).length() < CLEAR_START + radius:
        return False
    return not any((p - a).length() < CLEAR_ANCHOR + radius for a in anchors)


def spawn(path, loc, rot, scale, solid, sub, draw=0):
    m = mesh(path)
    if not m:
        return None
    a = actors.spawn_actor_from_object(m, loc, rot)
    if not a:
        return None
    a.set_actor_scale3d(unreal.Vector(scale, scale, scale))
    c = a.get_component_by_class(unreal.StaticMeshComponent)
    if not solid:
        c.set_collision_profile_name("NoCollision")
        c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    if draw:
        c.set_editor_property("ld_max_draw_distance", float(draw))
    a.set_folder_path(FOLDER + "/" + sub)
    return a


def size_of(path):
    b = mesh(path).get_bounds()
    return b.box_extent * 2.0


counts = {"underside": 0, "rim": 0, "plant": 0, "roots": 0, "hang": 0}
for island in islands:
    origin, ext = island.get_actor_bounds(False)
    width = max(ext.x, ext.y) * 2.0
    top = origin.z + ext.z
    bottom = origin.z - ext.z

    # ---- 3. undersides: cliffs upside down, poking out of the bottom of the island
    cliffs = found["cliff"] or found["rock"]
    for i in range(random.randint(*UNDERSIDE_CLIFFS) if cliffs else 0):
        path = random.choice(cliffs)
        s = size_of(path)
        scale = max(0.4, min(4.0, width * random.uniform(0.35, 0.6) / max(s.x, s.y, 1.0)))
        ang = random.uniform(0, 360)
        r = random.uniform(0.0, 0.3) * width
        loc = unreal.Vector(origin.x + math.cos(math.radians(ang)) * r,
                            origin.y + math.sin(math.radians(ang)) * r,
                            bottom + ext.z * 0.6)        # top of the cliff buried in the island
        rot = unreal.Rotator(roll=random.uniform(-15, 15), pitch=180.0 + random.uniform(-15, 15), yaw=random.uniform(0, 360))
        if spawn(path, loc, rot, scale, True, "Undersides"):
            counts["underside"] += 1

    # ---- 4. rims: boulders round the edge of the top
    rim_set = found["rock"] or found["cliff"]          # no separate boulders? small cliff pieces work too
    for i in range(random.randint(*RIM_ROCKS) if rim_set else 0):
        ang = random.uniform(0, 360)
        for frac in (0.85, 0.75, 0.65):
            p = ground(island, origin.x + math.cos(math.radians(ang)) * ext.x * frac,
                       origin.y + math.sin(math.radians(ang)) * ext.y * frac)
            if p and is_clear(p, 300):
                path = random.choice(rim_set)
                s = size_of(path)
                scale = max(0.3, min(3.0, random.uniform(150, 450) / max(s.x, s.y, s.z, 1.0)))
                if spawn(path, unreal.Vector(p.x, p.y, p.z - 20), unreal.Rotator(roll=0.0, pitch=0.0, yaw=random.uniform(0, 360)),
                         scale, True, "Rocks"):
                    counts["rim"] += 1
                break

    # ---- 5. tops: clumps of plants (non-solid, so they never snag a swing)
    for c in range(random.randint(*PLANT_CLUMPS) if found["plant"] else 0):
        cx = origin.x + random.uniform(-0.7, 0.7) * ext.x
        cy = origin.y + random.uniform(-0.7, 0.7) * ext.y
        kind = random.choice(found["plant"])           # one species per clump reads as natural
        for k in range(random.randint(*PLANTS_PER_CLUMP)):
            x = cx + random.gauss(0, 180)
            y = cy + random.gauss(0, 180)
            p = ground(island, x, y)
            if not p or not is_clear(p, 80):
                continue
            path = kind if random.random() < 0.75 else random.choice(found["plant"])
            if spawn(path, unreal.Vector(p.x, p.y, p.z - 5), unreal.Rotator(roll=0.0, pitch=0.0, yaw=random.uniform(0, 360)),
                     random.uniform(0.8, 1.3), False, "Plants", PLANT_DRAW):
                counts["plant"] += 1

    # ---- 6. roots sprawling over the top
    for i in range(random.randint(*ROOTS_PER_ISLAND) if found["roots"] else 0):
        p = ground(island, origin.x + random.uniform(-0.6, 0.6) * ext.x, origin.y + random.uniform(-0.6, 0.6) * ext.y)
        if p and is_clear(p, 200):
            if spawn(random.choice(found["roots"]), unreal.Vector(p.x, p.y, p.z - 15),
                     unreal.Rotator(roll=0.0, pitch=0.0, yaw=random.uniform(0, 360)), random.uniform(1.0, 1.8), False, "Roots"):
                counts["roots"] += 1

    # ---- 7. vines spilling over the edge
    hang = found["hang"] or []
    for i in range(random.randint(*HANGERS) if hang else 0):
        ang = random.uniform(0, 360)
        p = ground(island, origin.x + math.cos(math.radians(ang)) * ext.x * 0.92,
                   origin.y + math.sin(math.radians(ang)) * ext.y * 0.92)
        if p:
            if spawn(random.choice(hang), unreal.Vector(p.x, p.y, p.z + 10),
                     unreal.Rotator(roll=0.0, pitch=random.uniform(60, 100), yaw=ang), random.uniform(1.0, 1.6), False, "Vines", PLANT_DRAW):
                counts["hang"] += 1

log("placed " + ", ".join("{} {}".format(k, v) for k, v in counts.items()))
unreal.EditorLevelLibrary.save_current_level()
