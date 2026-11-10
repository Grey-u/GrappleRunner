"""
Grapple Runner: dress the landing islands with ruins, trees and water (jungle / lost-temple feel).

Run with the Floating Islands level open (not while playing):
    py "C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/dress_islands.py"

What it does
- RUINS (Infinity Blade: Grass Lands): one or two "clusters" per island, built from recipes:
    temple   - four columns (some broken) round a square, rubble, sometimes a fountain or statue
    wall     - a broken wall line with gaps to weave through, a buttress and rubble (cover)
    gate     - a ruined arch with short columns either side
    tower    - a ruined tower with stairs and a wall stub (a lookout / retreat point)
  Ruins are SOLID, so you can hide behind them and hook onto them.
- TREES (European Beech): tall forest trees and saplings around the ruins. The big trees are
  solid (hookable trunks); saplings are not (they'd only snag you).
- WATER: shallow ponds ringed with stones on some islands, plus extra waterfalls.
- Keeps clear: 6 m around the PlayerStart and 7 m around every grapple rock, so hooks always
  have a clear line. Pieces only go on an island's fairly flat TOP.
- Writes Saved/Tests/dressing_layout.csv (what went where) for the portfolio map, and saves
  a screenshot.
- Re-running clears the previous ruins / trees / ponds first. Change SEED for a new layout.
Units: centimetres (100 = 1 metre).

HOW TO READ THIS (no coding experience needed)
- Lines starting with # are notes for humans; the computer skips them.
- "def name(...):" defines a reusable recipe; the indented lines under it are its steps.
- "for x in ...:" repeats the indented steps once per item; "if ...:" only runs them when true.
"""
import csv
import math
import os
import random
import unreal

# ---------------------------------------------------------------- SETTINGS
SEED = 7
CLEAR_START = 600          # cm kept empty round the PlayerStart
CLEAR_ANCHOR = 700         # cm kept empty round each grapple rock (clear hook lines)
TREES_PER_ISLAND = 3       # tall beech trees
SAPLINGS_PER_ISLAND = 4
POND_ISLANDS = 4           # how many islands get a pond
EXTRA_WATERFALLS = 4
TREE_DRAW_DISTANCE = 30000  # cm: trees further than 300 m aren't drawn (saves frame time)

R = "/Game/InfinityBladeGrassLands/Environments/Plains/Env_Plains_Ruins/StaticMesh/"
S = "/Game/InfinityBladeGrassLands/Environments/Plains/Env_Plains_Statues/StaticMesh/"
O = "/Game/InfinityBladeGrassLands/Environments/Plains/Env_Plains_Organic/StaticMesh/"
T = "/Game/EuropeanBeech/Geometry/SimpleWind/"
WF = "/Game/Brushify/Maps/FloatingIslands/PFX/"

# Families: every piece in a family gets the SAME scale, worked out from one reference piece,
# so a broken column stays shorter than a whole one. (reference mesh, target height in cm)
FAMILIES = {
    "column": (R + "SM_Plains_Column_Large_01", 600),
    "wall": (R + "SM_Plains_CastleWall_Ruin_01", 450),
    "arch": (R + "SM_Plains_CastleArch_Ruin_01", 650),
    "tower": (R + "SM_Plains_CastleTower_Ruin_01", 1300),
    "rubble": (R + "SM_Plains_StoneBlock_01", 110),
    "small": (R + "SM_Plains_ShortColumn_01", 160),
    "statue": (S + "SM_Plains_Angel_Statue_01", 420),
    "fountain": (R + "SM_Plains_Castle_Fountain_01", 150),
    "tree": (T + "SM_EuropeanBeech_Forest_01", 1500),
    "sapling": (T + "SM_EuropeanBeech_Sapling_01", 450),
}
COLUMNS = ["SM_Plains_Column_Large_01", "SM_Plains_Column_Large_Broken_01",
           "SM_Plains_Column_Large_Broken_02", "SM_Plains_Column_Large_Broken_03"]
WALLS = ["SM_Plains_CastleWall_Ruin_01", "SM_Plains_CastleWall_Ruin_02",
         "SM_Plains_CastleWall_Ruin_03", "SM_Plains_CastleWall_Ruin_04"]
TREES = ["SM_EuropeanBeech_Forest_0%d" % i for i in range(1, 9)]
SAPLINGS = ["SM_EuropeanBeech_Sapling_0%d" % i for i in range(1, 5)]
FOLDERS = {"ruin": "GrappleRunner/Ruins", "tree": "GrappleRunner/Trees", "water": "GrappleRunner/Water"}
# -------------------------------------------------------------------------

random.seed(SEED)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
layout = []          # rows for the CSV: what, mesh, x, y, z, island


def log(text):
    unreal.log("Grapple Runner dressing: " + text)


def mesh(path):
    return unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None


def height_of(m):
    b = m.get_bounds()
    return max(1.0, b.box_extent.z * 2.0)


# Work out each family's scale once. Clamped, in case a reference is a strange size.
SCALE = {}
for fam, (path, target) in FAMILIES.items():
    m = mesh(path)
    SCALE[fam] = min(3.0, max(0.25, target / height_of(m))) if m else 1.0
    log("{} scale {:.2f}{}".format(fam, SCALE[fam], "" if m else " (reference missing)"))

# ---- clear the previous run
old = [a for a in actors.get_all_level_actors() if str(a.get_folder_path()) in FOLDERS.values()]
if old:
    actors.destroy_actors(old)

start = None
for a in actors.get_all_level_actors():
    if isinstance(a, unreal.PlayerStart):
        start = a.get_actor_location()
anchors = [a.get_actor_location() for a in actors.get_all_level_actors() if a.actor_has_tag("GrappleAnchor")]
islands = sorted([a for a in actors.get_all_level_actors() if a.get_actor_label().startswith("Landing_")],
                 key=lambda a: a.get_actor_label())
used = []            # (point, radius) of everything big placed so far


def ground(island, x, y):
    """Fire a ray straight down at (x, y). Returns the point on THIS island's top, or None."""
    origin, ext = island.get_actor_bounds(False)
    hit = unreal.SystemLibrary.line_trace_single(
        world, unreal.Vector(x, y, origin.z + ext.z + 3000), unreal.Vector(x, y, origin.z - ext.z - 100),
        unreal.TraceTypeQuery.ECC_VISIBILITY, True, [], unreal.DrawDebugTrace.NONE, True)
    if not hit:
        return None
    h = hit.to_tuple()
    point, normal, actor = h[5], h[7], h[9]
    if actor != island or normal.z < 0.75:
        return None
    return point


def is_clear(p, radius):
    if start and (p - start).length() < CLEAR_START + radius:
        return False
    if any((p - a).length() < CLEAR_ANCHOR + radius for a in anchors):
        return False
    return all((p - q).length() > radius + r for q, r in used)


def place(island, kind, fam, path, x, y, yaw, solid=True, shadow=True, draw=0, jitter=1.0):
    """Drop one mesh onto the island at (x, y), sitting on the ground, scaled by its family."""
    m = mesh(path)
    p = ground(island, x, y)
    if not m or not p:
        return None
    s = SCALE[fam] * jitter
    b = m.get_bounds()
    z = p.z - (b.origin.z - b.box_extent.z) * s - 8.0       # bottom on the ground, sunk 8 cm
    a = actors.spawn_actor_from_object(m, unreal.Vector(p.x, p.y, z), unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    if not a:
        return None
    a.set_actor_scale3d(unreal.Vector(s, s, s))
    c = a.get_component_by_class(unreal.StaticMeshComponent)
    if not solid:
        c.set_collision_profile_name("NoCollision")
        c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    c.set_editor_property("cast_shadow", shadow)
    if draw:
        c.set_editor_property("ld_max_draw_distance", float(draw))
    a.set_folder_path(FOLDERS[kind])
    a.set_actor_label("{}_{}_{:03d}".format(kind.capitalize(), fam, len(layout)))
    layout.append((kind, path.split("/")[-1], round(p.x), round(p.y), round(p.z), island.get_actor_label()))
    return a


def local(cx, cy, yaw, dx, dy):
    """A point (dx, dy) metres away from a cluster centre, turned with the cluster."""
    r = math.radians(yaw)
    return (cx + (dx * math.cos(r) - dy * math.sin(r)) * 100.0,
            cy + (dx * math.sin(r) + dy * math.cos(r)) * 100.0)


def rubble(island, cx, cy, yaw, n):
    for _ in range(n):
        x, y = local(cx, cy, yaw, random.uniform(-5, 5), random.uniform(-5, 5))
        place(island, "ruin", random.choice(["rubble", "small"]),
              R + random.choice(["SM_Plains_StoneBlock_01", "SM_Plains_ShortColumn_Half_01",
                                 "SM_Plains_Column_Small_Broken_01", "SM_Plains_Column_Small_Broken_02"]),
              x, y, random.uniform(0, 360), jitter=random.uniform(0.8, 1.2))


def temple(island, cx, cy, yaw):
    cols = COLUMNS[:]
    for i, (dx, dy) in enumerate([(-3.5, -3.5), (3.5, -3.5), (3.5, 3.5), (-3.5, 3.5)]):
        name = cols[0] if i == 0 else random.choice(cols[1:]) if i % 2 else random.choice(cols)
        x, y = local(cx, cy, yaw, dx, dy)
        place(island, "ruin", "column", R + name, x, y, yaw + random.uniform(-8, 8))
    centre = random.random()
    if centre < 0.5:
        place(island, "ruin", "fountain", R + "SM_Plains_Castle_Fountain_01", cx, cy, yaw)   # water feature
    elif centre < 0.8:
        place(island, "ruin", "statue", S + "SM_Plains_Angel_Statue_01", cx, cy, yaw + 180)
    rubble(island, cx, cy, yaw, 4)


def wall(island, cx, cy, yaw):
    for i, dx in enumerate((-5.5, 0.0, 5.5)):
        if random.random() < 0.2:
            continue                                       # a missing section: another gap
        x, y = local(cx, cy, yaw, dx + random.uniform(-0.5, 0.5), random.uniform(-0.4, 0.4))
        place(island, "ruin", "wall", R + random.choice(WALLS), x, y, yaw + random.uniform(-6, 6))
    x, y = local(cx, cy, yaw, 8.5, 0.0)
    place(island, "ruin", "wall", R + "SM_Plains_Buttress_01", x, y, yaw)
    rubble(island, cx, cy, yaw, 5)


def gate(island, cx, cy, yaw):
    place(island, "ruin", "arch", R + random.choice(["SM_Plains_CastleArch_Ruin_01", "SM_Plains_CastleArch_Ruin_03",
                                                     "SM_Plains_CastleArch_Ruin_02_Broken"]), cx, cy, yaw)
    for dx in (-4.5, 4.5):
        x, y = local(cx, cy, yaw, dx, 0.0)
        place(island, "ruin", "small", R + "SM_Plains_ShortColumn_01", x, y, yaw)
    rubble(island, cx, cy, yaw, 3)


def tower(island, cx, cy, yaw):
    place(island, "ruin", "tower", R + random.choice(["SM_Plains_CastleTower_Ruin_01", "SM_Plains_CastleTower_Ruin_02"]),
          cx, cy, yaw)
    x, y = local(cx, cy, yaw, 0.0, -5.5)
    place(island, "ruin", "wall", R + "SM_Plains_Castle_Stairs_01", x, y, yaw)
    x, y = local(cx, cy, yaw, 6.0, 1.0)
    place(island, "ruin", "wall", R + random.choice(WALLS), x, y, yaw + 90)
    rubble(island, cx, cy, yaw, 3)


RECIPES = [(temple, 900), (wall, 1000), (gate, 700), (tower, 900)]   # (recipe, footprint radius cm)


def find_spot(island, radius, tries=60):
    origin, ext = island.get_actor_bounds(False)
    for _ in range(tries):
        x = origin.x + random.uniform(-0.6, 0.6) * ext.x
        y = origin.y + random.uniform(-0.6, 0.6) * ext.y
        p = ground(island, x, y)
        if p and is_clear(p, radius):
            return p
    return None


# ---- 1. ruins
clusters = 0
for i, island in enumerate(islands):
    origin, ext = island.get_actor_bounds(False)
    count = 2 if min(ext.x, ext.y) > 2800 else 1
    for k in range(count):
        recipe, radius = RECIPES[(i + k) % len(RECIPES)] if random.random() < 0.7 else random.choice(RECIPES)
        p = find_spot(island, radius)
        if not p:
            continue
        recipe(island, p.x, p.y, random.uniform(0, 360))
        used.append((p, radius))
        clusters += 1
log("{} ruin clusters".format(clusters))

# ---- 2. trees (around the ruins, not on them)
trees = 0
for island in islands:
    for n, (fam, names, solid) in enumerate([("tree", TREES, True), ("sapling", SAPLINGS, False)]):
        want = TREES_PER_ISLAND if fam == "tree" else SAPLINGS_PER_ISLAND
        for _ in range(want):
            p = find_spot(island, 350 if fam == "tree" else 120, tries=25)
            if not p:
                continue
            if place(island, "tree", fam, T + random.choice(names), p.x, p.y, random.uniform(0, 360),
                     solid=solid, shadow=(fam == "tree"), draw=TREE_DRAW_DISTANCE,
                     jitter=random.uniform(0.8, 1.2)):
                used.append((p, 250 if fam == "tree" else 80))
                trees += 1
log("{} trees and saplings".format(trees))

# ---- 3. water: ponds ringed with stones, and more waterfalls
plane = mesh(O + "SM_WaterPlain")
ponds = 0
if plane:
    b = plane.get_bounds()
    pond_scale = min(4.0, max(0.05, 500.0 / max(1.0, 2 * max(b.box_extent.x, b.box_extent.y))))  # ~5 m across
    for island in random.sample(islands, min(POND_ISLANDS, len(islands))):
        p = find_spot(island, 450)
        if not p:
            continue
        a = actors.spawn_actor_from_object(plane, unreal.Vector(p.x, p.y, p.z + 4.0), unreal.Rotator(yaw=random.uniform(0, 360)))
        if not a:
            continue
        a.set_actor_scale3d(unreal.Vector(pond_scale, pond_scale, 1.0))
        c = a.get_component_by_class(unreal.StaticMeshComponent)
        c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        c.set_editor_property("cast_shadow", False)
        a.set_folder_path(FOLDERS["water"])
        a.set_actor_label("Pond_{:02d}".format(ponds))
        layout.append(("water", "SM_WaterPlain", round(p.x), round(p.y), round(p.z), island.get_actor_label()))
        for k in range(9):                                  # a ring of stones round the edge
            ang = 2 * math.pi * k / 9 + random.uniform(-0.15, 0.15)
            place(island, "water", "rubble", R + "SM_Plains_StoneBlock_01",
                       p.x + math.cos(ang) * 270, p.y + math.sin(ang) * 270, random.uniform(0, 360),
                       jitter=random.uniform(0.5, 0.8))
        used.append((p, 400))
        ponds += 1
log("{} ponds".format(ponds))

falls = [m for m in (mesh(WF + n) for n in ("NS_WaterFall_Large", "NS_WaterFall_Small")) if m]
wf = 0
if falls:
    for island in random.sample(islands, min(EXTRA_WATERFALLS, len(islands))):
        origin, ext = island.get_actor_bounds(False)
        side = random.choice([1, -1])
        loc = unreal.Vector(origin.x + random.uniform(-0.3, 0.3) * ext.x, origin.y + side * ext.y * 0.85,
                            origin.z + ext.z * 0.7)
        fx = actors.spawn_actor_from_object(random.choice(falls), loc, unreal.Rotator(yaw=90 * side))
        if fx:
            fx.set_actor_label("Waterfall_Extra_{:02d}".format(wf))
            fx.set_folder_path(FOLDERS["water"])
            layout.append(("water", "waterfall", round(loc.x), round(loc.y), round(loc.z), island.get_actor_label()))
            wf += 1
log("{} extra waterfalls".format(wf))

# ---- 4. save the level, the layout table and a screenshot (portfolio evidence)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
folder = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "Tests")
os.makedirs(folder, exist_ok=True)
with open(os.path.join(folder, "dressing_layout.csv"), "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["kind", "mesh", "x_cm", "y_cm", "z_cm", "island"])
    w.writerows(layout)
    for p in anchors:
        w.writerow(["anchor", "GrappleAnchor", round(p.x), round(p.y), round(p.z), ""])
    if start:
        w.writerow(["start", "PlayerStart", round(start.x), round(start.y), round(start.z), ""])
try:
    unreal.AutomationLibrary.take_high_res_screenshot(1920, 1080, "GR_Dressed.png")
except Exception as e:
    unreal.log_warning("Grapple Runner dressing: screenshot skipped ({})".format(e))
log("done: {} pieces placed. Layout saved to Saved/Tests/dressing_layout.csv".format(len(layout)))
