"""
Grapple Runner: make Aeloria much denser. More islands, giant islands, floating ruins, vines.

Run with the Floating Islands level open (not while playing), AFTER the C++ rebuild
(the vines need the GRVine class):
    py "C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/expand_world.py"
It takes a few minutes. Unreal may look frozen while it builds new trees: leave it.

What it adds (Naomi's brief, 10 Nov: "way more islands, some very large with structures,
floating structures, vines connecting the trees, way more dense")
1. MID ISLANDS: ~30 more islands, 18-55 m wide, either side of the swing route, at different
   heights, so the sky is full and there are side paths to explore.
2. GIANT ISLANDS: 3 islands 150-240 m wide with temple complexes (the dressing script puts
   up to 8 ruin clusters on each): places to fight, hide and retreat.
3. GRAPPLE ROCKS: on every new island, and a chain of rocks leading out to it from the route,
   so every island can actually be reached by swinging (hook range 60 m).
4. FLOATING RUINS: ~28 broken columns, arches, walls and tower pieces drifting in the air on
   chunks of rock, tilted. They're solid and tagged as grapple points: swing off them.
5. Re-runs Scripts/dress_islands.py, so the new islands get ruins, trees and ponds too.
6. VINES: hanging vines between neighbouring trees, and from some floating ruins down to
   the nearest island (GRVine, not solid, so they never snag you).
Keeps the main route clear: nothing new within 15 m of the route's centre line.
Re-running clears everything it made before (folder GrappleRunner/Expansion and Vines).
Units: centimetres (100 = 1 metre).

HOW TO READ THIS (no coding experience needed)
- Lines starting with # are notes for humans; the computer skips them.
- "def name(...):" defines a reusable recipe; "for x in ...:" repeats for each item.
"""
import math
import os
import random
import unreal

# ---------------------------------------------------------------- SETTINGS
SEED = 11
MID_ISLANDS = 30
MID_SIZE = (1800, 5500)          # cm wide
MID_SIDE = (4000, 20000)         # cm sideways from the route
MID_HEIGHT = (-2500, 3500)       # cm above/below the route
GIANT_ISLANDS = 3
GIANT_SIZE = (15000, 24000)
GIANT_SIDE = (14000, 24000)
FLOATING_RUINS = 28
FLOAT_SIDE = (1500, 12000)       # cm sideways from the route (never on the route itself)
FLOAT_HEIGHT = (500, 3000)       # cm above the route
ANCHOR_HEIGHT = (1100, 2100)     # same as the route: 11-21 m above the island top
CHAIN_STEP = (1200, 1600)        # cm between rocks on a chain out to an island
ROUTE_CLEAR = 1500               # cm kept clear either side of the route line
VINES_PER_TREE = 3            # 10 Nov: more connecting vines
VINE_LENGTH = (700, 2500)        # cm: tree pairs this far apart get a vine
FOLDERS = ["GrappleRunner/Expansion/Islands", "GrappleRunner/Expansion/Anchors",
           "GrappleRunner/Expansion/FloatingRuins", "GrappleRunner/Vines"]
ISLAND_BPS = ["/Game/Brushify/Meshes/FloatingIslands/FloatingIsland_0{0}/BP_FloatingIsland_0{0}".format(i)
              for i in range(1, 5)]
R = "/Game/InfinityBladeGrassLands/Environments/Plains/Env_Plains_Ruins/StaticMesh/"
S = "/Game/InfinityBladeGrassLands/Environments/Plains/Env_Plains_Statues/StaticMesh/"
# floating pieces: (mesh, target height in cm)
FLOAT_PIECES = [(R + "SM_Plains_Column_Large_Broken_01", 700), (R + "SM_Plains_Column_Large_Broken_02", 650),
                (R + "SM_Plains_CastleArch_Ruin_01", 800), (R + "SM_Plains_CastleArch_Ruin_02_Broken", 750),
                (R + "SM_Plains_CastleWall_Ruin_02", 550), (R + "SM_Plains_CastleWall_Ruin_03", 550),
                (R + "SM_Plains_CastleTower_Ruin_02", 1200), (R + "SM_Plains_Colunnade_Mid_01", 700),
                (S + "SM_Plains_Angel_Statue_01", 500)]
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
# -------------------------------------------------------------------------

random.seed(SEED)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()


def log(text):
    unreal.log("Grapple Runner expand: " + text)


def load(path):
    return unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None


# ---- 0. clear what a previous run made
old = [a for a in actors.get_all_level_actors() if str(a.get_folder_path()) in FOLDERS]
if old:
    actors.destroy_actors(old)
    log("cleared {} actors from the last run".format(len(old)))

# ---- the existing route: landing islands Landing_00, Landing_01, ... in order
route_islands = sorted([a for a in actors.get_all_level_actors()
                        if a.get_actor_label().startswith("Landing_") and a.get_actor_label()[8:].isdigit()],
                       key=lambda a: a.get_actor_label())
route = []
for a in route_islands:
    o, e = a.get_actor_bounds(False)
    route.append(unreal.Vector(o.x, o.y, o.z + e.z))           # centre of each island's top
anchor_actors = [a for a in actors.get_all_level_actors() if a.actor_has_tag("GrappleAnchor")]
rock_mesh = None
rock_width = 350.0
for a in anchor_actors:
    c = a.get_component_by_class(unreal.StaticMeshComponent)
    if c and c.static_mesh:
        rock_mesh = c.static_mesh
        break
island_assets = [m for m in (load(p) for p in ISLAND_BPS) if m]
log("route: {} islands, {} anchors; {} island types".format(len(route), len(anchor_actors), len(island_assets)))
if not route or not island_assets:
    raise RuntimeError("Grapple Runner expand: open L_FloatingIslands_2 first (no route found)")


def route_at(x):
    """The route's centre (y, top z) at position x, by blending between landing islands."""
    if x <= route[0].x:
        return route[0].y, route[0].z
    for a, b in zip(route, route[1:]):
        if a.x <= x <= b.x:
            f = (x - a.x) / max(1.0, b.x - a.x)
            return a.y + (b.y - a.y) * f, a.z + (b.z - a.z) * f
    return route[-1].y, route[-1].z


occupied = [(a.get_actor_bounds(False)[0], max(a.get_actor_bounds(False)[1].x, a.get_actor_bounds(False)[1].y))
            for a in route_islands]          # (centre, radius) of everything big, to avoid overlaps


def free(p, radius, gap=1500):
    return all((unreal.Vector(p.x, p.y, 0) - unreal.Vector(c.x, c.y, 0)).length() > radius + r + gap
               or abs(p.z - c.z) > radius + r for c, r in occupied)


def spawn_scaled(asset, loc, width, folder, label, yaw=None, top_at=None, tags=None, tilt=0.0):
    rot = unreal.Rotator(roll=random.uniform(-tilt, tilt), pitch=random.uniform(-tilt, tilt),
                         yaw=random.uniform(0, 360) if yaw is None else yaw)
    a = actors.spawn_actor_from_object(asset, loc, rot)
    if not a:
        return None
    o, e = a.get_actor_bounds(False)
    s = width / max(1.0, 2 * max(e.x, e.y))
    a.set_actor_scale3d(unreal.Vector(s, s, s))
    if top_at is not None:                               # move so its top surface is at this height
        o, e = a.get_actor_bounds(False)
        a.set_actor_location(a.get_actor_location() + unreal.Vector(0, 0, top_at - (o.z + e.z)), False, False)
    a.set_folder_path(folder)
    a.set_actor_label(label)
    if tags:
        a.set_editor_property("tags", [unreal.Name(t) for t in tags])
    return a


new_anchors = 0


def anchor(loc):
    global new_anchors
    if not rock_mesh:
        return
    spawn_scaled(rock_mesh, loc, random.uniform(250, 450), FOLDERS[1], "Anchor_X{:03d}".format(new_anchors),
                 tags=["GrappleAnchor"])
    new_anchors += 1


def anchors_on(island, count):
    """Rocks floating 11-21 m above an island's top, spread round it."""
    o, e = island.get_actor_bounds(False)
    for k in range(count):
        ang = 2 * math.pi * k / count + random.uniform(-0.3, 0.3)
        r = random.uniform(0.2, 0.8) * min(e.x, e.y)
        anchor(unreal.Vector(o.x + math.cos(ang) * r, o.y + math.sin(ang) * r,
                             o.z + e.z + random.uniform(*ANCHOR_HEIGHT)))


def chain_to(island):
    """A line of rocks from the nearest point on the route out to the island, so you can swing there."""
    o, e = island.get_actor_bounds(False)
    ry, rz = route_at(o.x)
    start = unreal.Vector(o.x, ry, rz)
    goal = unreal.Vector(o.x, o.y, o.z + e.z)
    dist = (goal - start).length() - min(e.x, e.y) * 0.8
    if dist < 2500:
        return
    step_dir = (goal - start) / max(1.0, (goal - start).length())
    t = random.uniform(*CHAIN_STEP)
    while t < dist:
        p = start + step_dir * t
        anchor(unreal.Vector(p.x + random.uniform(-300, 300), p.y, p.z + random.uniform(*ANCHOR_HEIGHT)))
        t += random.uniform(*CHAIN_STEP)


# ---- 1 + 2. new islands
x_lo, x_hi = route[0].x - 5000, route[-1].x + 5000
made = []
for kind, count, size, side, height in (("Giant", GIANT_ISLANDS, GIANT_SIZE, GIANT_SIDE, (-1500, 500)),
                                        ("Mid", MID_ISLANDS, MID_SIZE, MID_SIDE, MID_HEIGHT)):
    for i in range(count):
        for _ in range(40):                                   # try spots until one is free
            x = random.uniform(x_lo, x_hi) if kind == "Mid" else x_lo + (x_hi - x_lo) * (i + 0.5) / count
            ry, rz = route_at(x)
            y = ry + random.choice([-1, 1]) * random.uniform(*side)
            w = random.uniform(*size)
            top = rz + random.uniform(*height)
            if free(unreal.Vector(x, y, top), w / 2):
                break
        else:
            continue
        a = spawn_scaled(random.choice(island_assets), unreal.Vector(x, y, top), w, FOLDERS[0],
                         "Landing_{}_{:02d}".format(kind, i), top_at=top, tags=["LandingIsland"])
        if not a:
            continue
        occupied.append((unreal.Vector(x, y, top), w / 2))
        made.append(a)
        anchors_on(a, 8 if kind == "Giant" else 3)
        chain_to(a)
log("{} new islands, {} new grapple rocks".format(len(made), new_anchors))

# ---- 4. floating ruins on rock chunks
floats = 0
pieces = [(load(p), h) for p, h in FLOAT_PIECES]
pieces = [(m, h) for m, h in pieces if m]
for i in range(FLOATING_RUINS if pieces else 0):
    for _ in range(40):
        x = random.uniform(x_lo, x_hi)
        ry, rz = route_at(x)
        y = ry + random.choice([-1, 1]) * random.uniform(max(ROUTE_CLEAR, FLOAT_SIDE[0]), FLOAT_SIDE[1])
        z = rz + random.uniform(*FLOAT_HEIGHT)
        if free(unreal.Vector(x, y, z), 600, gap=800) and all(
                (a.get_actor_location() - unreal.Vector(x, y, z)).length() > 900 for a in anchor_actors):
            break
    else:
        continue
    mesh, h = random.choice(pieces)
    b = mesh.get_bounds()
    s = h / max(1.0, 2 * b.box_extent.z)
    tilt_r, tilt_p, yaw = random.uniform(-14, 14), random.uniform(-14, 14), random.uniform(0, 360)
    ruin = actors.spawn_actor_from_object(mesh, unreal.Vector(x, y, z), unreal.Rotator(roll=tilt_r, pitch=tilt_p, yaw=yaw))
    if not ruin:
        continue
    ruin.set_actor_scale3d(unreal.Vector(s, s, s))
    ruin.set_folder_path(FOLDERS[2])
    ruin.set_actor_label("FloatingRuin_{:02d}".format(i))
    ruin.set_editor_property("tags", [unreal.Name("GrappleAnchor"), unreal.Name("FloatingRuin")])
    o, e = ruin.get_actor_bounds(False)
    if rock_mesh:                                         # a chunk of rock it's still sitting on
        spawn_scaled(rock_mesh, unreal.Vector(o.x, o.y, o.z - e.z), 1.3 * 2 * max(e.x, e.y), FOLDERS[2],
                     "FloatingRuin_{:02d}_Rock".format(i), tags=["GrappleAnchor"], tilt=10)
    occupied.append((unreal.Vector(x, y, z), max(e.x, e.y)))
    floats += 1
log("{} floating ruins".format(floats))

# ---- 5. ground plants (70% more), then dress every island with overgrown ruins, trees, water
log("planting and dressing all islands: this is the slow part")
exec(open(os.path.join(PROJECT, "Scripts", "scatter_foliage.py")).read(), {"__name__": "scatter_foliage"})
# (run in its own space, so its settings don't overwrite this script's)
exec(open(os.path.join(PROJECT, "Scripts", "dress_islands.py")).read(), {"__name__": "dress_islands"})

# ---- 6. vines
vine_class = unreal.load_class(None, "/Script/GrappleRunner.GRVine")
vines = 0


def vine(a, b, sag_ratio):
    global vines
    v = actors.spawn_actor_from_class(vine_class, a, unreal.Rotator())
    if not v:
        return
    v.set_editor_property("sag", (b - a).length() * sag_ratio)
    v.set_editor_property("end_point", b - a)          # setting this redraws the vine
    v.set_folder_path(FOLDERS[3])
    v.set_actor_label("Vine_{:03d}".format(vines))
    vines += 1


if not vine_class:
    unreal.log_warning("Grapple Runner expand: GRVine not found. Rebuild the C++, then re-run for vines.")
else:
    trees = [a for a in actors.get_all_level_actors()
             if str(a.get_folder_path()) == "GrappleRunner/Trees" and "_tree_" in a.get_actor_label()]
    links = {t.get_name(): 0 for t in trees}
    for t in trees:
        if links[t.get_name()] >= VINES_PER_TREE:
            continue
        tp = t.get_actor_location()
        near = sorted([u for u in trees if u != t and links[u.get_name()] < VINES_PER_TREE
                       and VINE_LENGTH[0] < (u.get_actor_location() - tp).length() < VINE_LENGTH[1]
                       and abs(u.get_actor_location().z - tp.z) < 600],
                      key=lambda u: (u.get_actor_location() - tp).length())
        for u in near[:VINES_PER_TREE - links[t.get_name()]]:
            _, te = t.get_actor_bounds(False)
            _, ue = u.get_actor_bounds(False)
            a = tp + unreal.Vector(0, 0, 2 * te.z * random.uniform(0.45, 0.7))   # into the canopy
            b = u.get_actor_location() + unreal.Vector(0, 0, 2 * ue.z * random.uniform(0.45, 0.7))
            vine(a, b, random.uniform(0.12, 0.25))
            links[t.get_name()] += 1
            links[u.get_name()] += 1
    # vines from the tops of tall ruins to the nearest tree: nature tying the buildings down
    tall = [a for a in actors.get_all_level_actors() if str(a.get_folder_path()) == "GrappleRunner/Ruins"
            and any(k in a.get_actor_label() for k in ("_column_", "_arch_", "_tower_", "_wall_"))]
    for ruin in tall:
        if random.random() > 0.55:
            continue
        o, e = ruin.get_actor_bounds(False)
        top = unreal.Vector(o.x, o.y, o.z + e.z * 0.85)
        near = [t for t in trees if 600 < (t.get_actor_location() - top).length() < 2500]
        if not near:
            continue
        t = min(near, key=lambda u: (u.get_actor_location() - top).length())
        _, te = t.get_actor_bounds(False)
        vine(top, t.get_actor_location() + unreal.Vector(0, 0, 2 * te.z * random.uniform(0.4, 0.65)),
             random.uniform(0.12, 0.22))
    # some floating ruins hang a vine down to the nearest island
    islands_now = [a for a in actors.get_all_level_actors() if a.get_actor_label().startswith("Landing_")]
    for r in [a for a in actors.get_all_level_actors() if a.actor_has_tag("FloatingRuin")]:
        if random.random() > 0.45:
            continue
        o, e = r.get_actor_bounds(False)
        base = unreal.Vector(o.x, o.y, o.z - e.z * 0.6)
        best = min(islands_now, key=lambda i: (i.get_actor_bounds(False)[0] - base).length())
        io, ie = best.get_actor_bounds(False)
        to_isl = unreal.Vector(io.x, io.y, 0) - unreal.Vector(base.x, base.y, 0)
        edge = unreal.Vector(io.x, io.y, io.z + ie.z) - to_isl / max(1.0, to_isl.length()) * min(ie.x, ie.y) * 0.7
        if (edge - base).length() < 5000:
            vine(base, edge, 0.08)
    log("{} vines".format(vines))

unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
try:
    unreal.AutomationLibrary.take_high_res_screenshot(1920, 1080, "GR_Expanded.png")
except Exception:
    pass
log("done. Press Play, then run gr.Test perf to measure the frame time (T-15).")
