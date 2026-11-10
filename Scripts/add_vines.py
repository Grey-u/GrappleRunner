"""
Grapple Runner: hang vines between trees, from ruins to trees, and from floating ruins down to islands.

Run with the Floating Islands level open (not while playing), after expand_world.py:
    py "C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/add_vines.py"

Why a separate script (10 Nov): placing ~600 vines in one go, after thousands of trees, made
Unreal run out of memory (79 GB) and crash. So vines now have their own step, with a cap
(MAX_VINES), 5 pieces each, and a clean-up (garbage collection) and a save every 50 vines.
Re-running clears the old vines first.
Units: centimetres (100 = 1 metre).

HOW TO READ THIS (no coding experience needed)
- Lines starting with # are notes for humans; the computer skips them.
- "def name(...):" defines a reusable recipe; "for x in ...:" repeats for each item.
"""
import random
import unreal

SEED = 12
MAX_VINES = 250            # enough to read as "connected"; each vine is 5 bent pieces for the GPU
VINES_PER_TREE = 3
VINE_LENGTH = (700, 2500)  # cm: tree pairs this far apart get a vine
SEGMENTS = 5
FOLDER = "GrappleRunner/Vines"
FOLDERS = [None, None, None, FOLDER]   # (index 3 = vines, as in expand_world)

random.seed(SEED)
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def log(text):
    unreal.log("Grapple Runner vines: " + text)


old = [a for a in actors.get_all_level_actors() if str(a.get_folder_path()) == FOLDER]
if old:
    actors.destroy_actors(old)
vine_class = unreal.load_class(None, "/Script/GrappleRunner.GRVine")
vines = 0


def vine(a, b, sag_ratio):
    global vines
    if vines >= MAX_VINES:
        return
    v = actors.spawn_actor_from_class(vine_class, a, unreal.Rotator())
    if not v:
        return
    v.set_editor_property("segments", SEGMENTS)
    v.set_editor_property("sag", (b - a).length() * sag_ratio)
    v.set_editor_property("end_point", b - a)          # setting this redraws the vine
    v.set_folder_path(FOLDERS[3])
    v.set_actor_label("Vine_{:03d}".format(vines))
    vines += 1
    if vines % 50 == 0:                     # tidy memory and save as we go
        unreal.SystemLibrary.collect_garbage()
        levels.save_current_level()
        log("{} vines so far".format(vines))


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

levels.save_current_level()
log("done: {} vines".format(vines))
