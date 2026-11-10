"""
Grapple Runner: redraw every vine (after the vine code changes).

Run with the Floating Islands level open (not while playing):
    py "C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/refresh_vines.py"

Why: a vine draws its pieces when it is placed or a setting changes. After a code fix
(10 Nov: pieces made movable so they stay attached), changing one setting on each vine (its number of pieces,
8 -> 5, which is also cheaper) makes it redraw with the new code.
"""
import unreal

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
cls = unreal.load_class(None, "/Script/GrappleRunner.GRVine")
n = 0
for a in actors.get_all_level_actors():
    if cls and isinstance(a, cls):
        # Changing the number of pieces makes the vine redraw (once). 5 pieces is still smooth and
        # costs the graphics card less than 8 (there are hundreds of vines).
        segs = a.get_editor_property("segments")
        a.set_editor_property("segments", 5 if segs != 5 else 6)
        n += 1
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
unreal.log("Grapple Runner vines: redrew {} vines".format(n))
