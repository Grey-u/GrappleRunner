"""Place the C++ GrappleRig in the open level (once) and make the mist cards non-solid.
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
cls = unreal.load_class(None, "/Script/GrappleRunner.GrappleRig")
if cls is None:
    unreal.log_error("Grapple Runner: GrappleRig class not found - did the C++ module compile?")
else:
    existing = [a for a in actors.get_all_level_actors() if a.get_class() == cls]
    if not existing:
        rig = actors.spawn_actor_from_class(cls, unreal.Vector(0, 0, 0))
        rig.set_actor_label("GrappleRig")
        rig.set_folder_path("GrappleRunner")
        unreal.log("Grapple Runner: GrappleRig placed")
    else:
        unreal.log("Grapple Runner: GrappleRig already in level")

# mist cards, sky dome and waterfalls must not block the player or the hooks
# (Test T-02 found the sky dome still solid after this was first run, so now we:
#  1. call modify() so Unreal knows the object changed and really saves it,
#  2. switch collision off two ways, and
#  3. read it back to PROVE it is off, warning if not.)
n, still_solid = 0, []
for a in actors.get_all_level_actors():
    label = a.get_actor_label()
    if label.startswith(("Mist_", "SkyDome", "Waterfall_")):
        a.modify()                                              # "this actor is about to change: save it"
        for c in a.get_components_by_class(unreal.PrimitiveComponent):
            c.modify()
            c.set_collision_profile_name("NoCollision")         # the named preset...
            c.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)  # ...and the switch itself
            if c.get_collision_enabled() != unreal.CollisionEnabled.NO_COLLISION:
                still_solid.append(label)                       # the check: did it actually work?
            n += 1
unreal.log("Grapple Runner: {} non-solid components".format(n))
if still_solid:
    unreal.log_warning("Grapple Runner: still solid: " + ", ".join(sorted(set(still_solid))))
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
unreal.log("Grapple Runner: rig setup done")
