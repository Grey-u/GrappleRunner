# =====================================================================================
#  fix_floating.py  -  finds and fixes everything floating in both levels
# =====================================================================================
#  WHAT THIS IS
#  Grey's playtests showed plants, trees, rocks and ruins hovering above the islands or
#  sticking out past ledges. This script checks every one of them against the real ground
#  (a ray fired straight down), then:
#    - something hovering over ground      -> dropped down until it sits on it
#    - a plant/tree hanging off a ledge    -> removed (no grass in mid-air)
#    - a plant/tree with no ground at all  -> removed
#    - ruins/big props with no ground      -> NOT touched, only listed (they may be on purpose)
#  The islands themselves, grapple anchor rocks, clouds, mist, vines, planets, orbs and
#  waterfalls are meant to float, so they are skipped.
#  It also checks the baked grass and trees (GRStylizedNature) instance by instance.
#
#  It writes a report with exact numbers to Saved/fix_floating_report.txt and saves both
#  levels. Ctrl+Z won't undo a saved level, so commit to GitHub first if you want a backup.
#
#  HOW TO RUN: it runs by itself the next time the editor opens (init_unreal.py sees
#  Saved/fix_floating_on_start.flag). Or paste into the Python box:
#    exec(open("C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/fix_floating.py").read())
# =====================================================================================
import unreal, os

LEVELS = ["/Game/GrappleRunner/Maps/L_FloatingIslands_2", "/Game/GrappleRunner/Maps/L_Endless"]
WAIT_TICKS = 120          # let a freshly loaded level finish building its collision
HOVER_CM = 12             # more than this gap under something counts as floating
LEDGE_CM = 60             # a footprint corner more than this above the ground = over a ledge

# names that mean "this is supposed to float" -> never touched
FLOATS = ("floatingisland", "floatingmountain", "landing_", "backdrop_", "mist", "cloud", "sky",
          "planet", "vine", "glow", "orb", "waterfall", "fog", "gaspod", "anchor", "harness")
# names that mean "plant or tree" -> may be dropped or removed
PLANTS = ("plant_", "tree_", "fern", "banana", "bigplant", "tropical", "bush", "grass", "flower",
          "clover", "heather", "sm_fol", "sm_tree", "beech", "foliage")

_project = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
_saved = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
_lsub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
_asub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report = []


def log(msg):
    unreal.log("FIX FLOATING: " + msg)
    report.append(msg)


def name_of(actor):
    """Label + class + mesh name, lower case, so one word test covers all three."""
    bits = [actor.get_actor_label(), actor.get_class().get_name()]
    if isinstance(actor, unreal.StaticMeshActor):
        m = actor.static_mesh_component.static_mesh
        if m:
            bits.append(m.get_path_name())
    return " ".join(bits).lower()


def kind(actor):
    """'float' (leave alone), 'plant', 'prop' (ruins, rocks, props) or None (not checked)."""
    if actor.actor_has_tag("GrappleAnchor"):
        return "float"
    n = name_of(actor)
    if any(w in n for w in FLOATS):
        return "float"
    if not isinstance(actor, unreal.StaticMeshActor):
        return None
    if any(w in n for w in PLANTS):
        return "plant"
    return "prop"


def ground_below(world, x, y, top_z, skip, depth=4000):
    """Fire a ray down from top_z. Looks straight through plants and the things in 'skip'
    (so a fern doesn't count as ground for another fern). Returns the ground height or None."""
    z = top_z
    for _ in range(8):
        hit = unreal.SystemLibrary.line_trace_single(
            world, unreal.Vector(x, y, z), unreal.Vector(x, y, top_z - depth),
            unreal.TraceTypeQuery.ECC_VISIBILITY, True, [], unreal.DrawDebugTrace.NONE, True)
        if not hit:
            return None
        h = hit.to_tuple()
        p, a = h[5], h[9]
        if a is None or a in skip or kind(a) == "plant" or a.get_class().get_name() == "GRStylizedNature":
            z = p.z - 2            # look through it and keep going down
            continue
        if kind(a) == "float" and "anchor" not in name_of(a) and not a.actor_has_tag("GrappleAnchor") \
                and not any(w in name_of(a) for w in ("floatingisland", "floatingmountain", "landing_", "backdrop_")):
            z = p.z - 2            # clouds, mist, orbs etc. are not ground
            continue
        return p.z
    return None


def fix_actors(world):
    actors = _asub.get_all_level_actors()
    dropped = removed = listed = checked = 0
    to_remove = []
    for a in actors:
        k = kind(a)
        if k not in ("plant", "prop"):
            continue
        checked += 1
        o, e = a.get_actor_bounds(False)
        bottom = o.z - e.z
        start = o.z + e.z + 50
        skip = {a}
        centre = ground_below(world, o.x, o.y, start, skip)
        # footprint: four points at 35% of the width, to catch things hanging off ledges
        corners = [ground_below(world, o.x + dx * e.x * 0.35, o.y + dy * e.y * 0.35, start, skip)
                   for dx, dy in ((1, 1), (1, -1), (-1, 1), (-1, -1))]
        supported = sum(1 for c in corners if c is not None and bottom - c < LEDGE_CM)
        if centre is None:
            if k == "plant":
                to_remove.append(a); removed += 1
            else:
                listed += 1; log("  no ground under (left alone): " + a.get_actor_label())
            continue
        if k == "plant" and supported < 2 and e.x < 1500:
            to_remove.append(a); removed += 1          # hanging off a ledge
            continue
        gap = bottom - centre
        if gap > HOVER_CM:
            loc = a.get_actor_location()
            a.set_actor_location(unreal.Vector(loc.x, loc.y, loc.z - gap - 3), False, False)
            dropped += 1
    for a in to_remove:
        _asub.destroy_actor(a)
    log("actors checked %d | dropped onto ground %d | removed (mid-air or off a ledge) %d | ruins/props left alone %d"
        % (checked, dropped, removed, listed))
    return dropped + removed


def fix_instances(world):
    """The baked grass and trees: one check per instance at its root."""
    total_rm = 0
    natures = [a for a in _asub.get_all_level_actors() if a.get_class().get_name() == "GRStylizedNature"]
    for nat in natures:
        for comp in nat.get_components_by_class(unreal.InstancedStaticMeshComponent):
            n = comp.get_instance_count()
            if n == 0:
                continue
            bad = []
            is_tree = "tree" in comp.get_name().lower()
            for i in range(n):
                t = comp.get_instance_transform(i, True)
                if isinstance(t, tuple):
                    t = t[-1]
                p = t.translation
                g = ground_below(world, p.x, p.y, p.z + 40, {nat}, depth=400)
                if g is None or p.z - g > (30 if is_tree else HOVER_CM):
                    bad.append(i)
            if bad:
                comp.remove_instances(bad)
            log("  %s / %s: %d of %d instances were floating, removed" % (nat.get_actor_label(), comp.get_name(), len(bad), n))
            total_rm += len(bad)
    return total_rm


_st = {"i": 0, "wait": 0, "loaded": False, "handle": None, "busy": False}


def _step(dt):
    if _st["busy"]:
        return
    _st["busy"] = True
    try:
        _inner()
    except Exception as ex:
        log("ERROR: %s" % ex)
        unreal.unregister_slate_post_tick_callback(_st["handle"])
        _finish()
    finally:
        _st["busy"] = False


def _inner():
    if _st["i"] >= len(LEVELS):
        unreal.unregister_slate_post_tick_callback(_st["handle"])
        _finish()
        return
    lvl = LEVELS[_st["i"]]
    if not _st["loaded"]:
        log("loading " + lvl)
        _lsub.load_level(lvl)
        _st["loaded"] = True; _st["wait"] = 0
        return
    _st["wait"] += 1
    if _st["wait"] < WAIT_TICKS:
        return
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    log("== " + lvl)
    fix_actors(world)
    fix_instances(world)
    _lsub.save_current_level()
    log("saved " + lvl)
    _st["i"] += 1; _st["loaded"] = False


def _finish():
    unreal._gr_fix_running = False
    log("DONE")
    with open(os.path.join(_saved, "fix_floating_report.txt"), "w") as f:
        f.write("\n".join(report))


if getattr(unreal, "_gr_fix_running", False):
    unreal.log_warning("FIX FLOATING: already running")
else:
    unreal._gr_fix_running = True
    _st["handle"] = unreal.register_slate_post_tick_callback(_step)
