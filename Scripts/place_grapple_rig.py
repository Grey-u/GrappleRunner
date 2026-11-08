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
n = 0
for a in actors.get_all_level_actors():
    label = a.get_actor_label()
    if label.startswith(("Mist_", "SkyDome", "Waterfall_")):
        for c in a.get_components_by_class(unreal.PrimitiveComponent):
            c.set_collision_profile_name("NoCollision")
            n += 1
unreal.log("Grapple Runner: {} non-solid components".format(n))
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
unreal.log("Grapple Runner: rig setup done")
