"""
Grapple Runner: collision check for the landing islands (a test tool).

Run with the Floating Islands level open (not while playing):
    py "C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/diagnose_collision.py"

Why this exists
- In test T-02 the player fell straight through the first island after pressing Play.
  This script measures WHY: for every landing island it lists each visible part,
  whether that part is solid, and whether it blocks the "Pawn" channel that the
  grapple rig's body uses for collision. It also fires an invisible ray straight
  down from the PlayerStart to see what (if anything) is underneath the player.
- Results are printed to the Output Log AND written to
  Saved/Tests/collision_report.txt so they can be kept as test evidence.

HOW TO READ THIS (no coding experience needed)
- Lines starting with # are notes for humans; the computer skips them.
- Text inside triple quotes (like this block) is also just a note.
- "import" = load a toolbox. "unreal" is Unreal Engine's own Python toolbox.
- "def name(...):" defines a reusable recipe; the indented lines under it are its steps.
- "for x in ...:" repeats the indented steps once per item; "if ...:" only runs them when true.
- Unreal distances are in centimetres (100 = 1 metre).
"""
import os
import unreal

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
lines = []   # every line of the report is collected here, then saved to a file at the end


def out(text):
    """Print one line to the Output Log and remember it for the report file."""
    unreal.log("Collision check: " + text)
    lines.append(text)


def describe(comp):
    """One line about one part: its name, whether it's solid, and what it does to the Pawn channel."""
    try:
        enabled = comp.get_collision_enabled()           # NoCollision / QueryOnly / PhysicsOnly / QueryAndPhysics
    except Exception:
        enabled = "?"
    try:
        profile = comp.get_collision_profile_name()      # a named preset such as BlockAll
    except Exception:
        profile = "?"
    try:
        pawn = comp.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN)  # Block / Overlap / Ignore
    except Exception:
        pawn = "?"
    mesh = ""
    if isinstance(comp, unreal.StaticMeshComponent):
        sm = comp.static_mesh
        mesh = sm.get_name() if sm else "(no mesh)"
        # A mesh can only be solid if it has a collision shape. Count its simple shapes.
        try:
            body = sm.get_editor_property("body_setup") if sm else None
            agg = body.get_editor_property("agg_geom") if body else None
            shapes = 0
            if agg:
                for key in ("box_elems", "sphyl_elems", "sphere_elems", "convex_elems"):
                    shapes += len(agg.get_editor_property(key))
            trace = body.get_editor_property("collision_trace_flag") if body else "?"
            mesh += " | simple shapes {} | trace flag {}".format(shapes, trace)
        except Exception as e:
            mesh += " | shape info unavailable ({})".format(e)
    return "{:<34} enabled={} profile={} pawn={} {}".format(
        comp.get_name(), enabled, profile, pawn, mesh)


# ---- 1. every landing island, part by part
islands = sorted([a for a in actors.get_all_level_actors()
                  if a.get_actor_label().startswith("Landing_")],
                 key=lambda a: a.get_actor_label())
out("{} landing islands found".format(len(islands)))
for a in islands[:1]:                                     # the first island is enough to see the pattern
    origin, ext = a.get_actor_bounds(False)
    out("--- {} ({}) top of bounds z={:.0f}".format(a.get_actor_label(), a.get_class().get_name(),
                                                   origin.z + ext.z))
    for c in a.get_components_by_class(unreal.PrimitiveComponent):
        out("   " + describe(c))

# ---- 2. what is under the PlayerStart?
start = None
for a in actors.get_all_level_actors():
    if isinstance(a, unreal.PlayerStart):
        start = a.get_actor_location()
if start:
    out("PlayerStart at z={:.0f}".format(start.z))
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    for channel_name, channel in (("Visibility", unreal.TraceTypeQuery.ECC_VISIBILITY),
                                  ("Camera", unreal.TraceTypeQuery.ECC_CAMERA)):
        hit = unreal.SystemLibrary.line_trace_single(
            world, start, start - unreal.Vector(0, 0, 20000), channel, False, [],
            unreal.DrawDebugTrace.NONE, True)
        if hit:
            t = hit.to_tuple()
            # to_tuple order: blocking_hit, initial_overlap, time, distance, location, impact_point, ...
            # ... normal, impact_normal, phys_mat, hit_actor, hit_component, ...
            out("ray down ({}): hit {} / {} at z={:.0f}, {:.0f} cm below the start".format(
                channel_name, t[9].get_actor_label() if t[9] else "?",
                t[10].get_name() if t[10] else "?", t[5].z, t[3]))
        else:
            out("ray down ({}): hit NOTHING within 200 m".format(channel_name))

    # A ray is infinitely thin. The player's body is a capsule (a pill shape: 30 cm radius,
    # 85 cm half-height), and the grapple rig moves it with a "sweep": it slides that capsule
    # along a path and asks "what would it bump into?". So test a capsule sweep too.
    # If the ray hits but the capsule sweep doesn't, the problem is with shape sweeps.
    for drop_from in (0.0, 600.0):            # from the PlayerStart, and from 6 m higher
        a = start + unreal.Vector(0, 0, drop_from)
        b = a - unreal.Vector(0, 0, 20000)
        hit = unreal.SystemLibrary.capsule_trace_single(
            world, a, b, 30.0, 85.0, unreal.TraceTypeQuery.ECC_VISIBILITY, False, [],
            unreal.DrawDebugTrace.NONE, True)
        if hit:
            t = hit.to_tuple()
            out("capsule sweep from z={:.0f}: hit {} / {} at z={:.0f} (start inside something: {})".format(
                a.z, t[9].get_actor_label() if t[9] else "?", t[10].get_name() if t[10] else "?",
                t[5].z, t[1]))
        else:
            out("capsule sweep from z={:.0f}: hit NOTHING within 200 m".format(a.z))

# ---- 3. save the report as test evidence
folder = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "Tests")
os.makedirs(folder, exist_ok=True)
path = os.path.join(folder, "collision_report.txt")
with open(path, "w") as f:
    f.write("\n".join(lines) + "\n")
unreal.log("Collision check: report saved to " + path)
