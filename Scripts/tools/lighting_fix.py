"""
Grapple Runner: brighten the shadowed undersides of the floating islands under Lumen (12 Nov 2026).

With deferred rendering + Lumen, the space under an island gets almost no sky light, so the
undersides went pitch black. "Skylight leaking" lets a little sky light into those places, the
way bounce light off clouds and air would in real life. Also sets the blade grip slide default.

Run: exec(open("C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/tools/lighting_fix.py").read())

HOW TO READ THIS: lines starting with # are notes; "for x in ...:" repeats the indented steps.
"""
import unreal

LEAK = 0.12   # 0 = none, 0.1-0.2 = soft fill in deep shadow

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
n = 0
for a in actors.get_all_level_actors():
    if isinstance(a, unreal.PostProcessVolume):
        s = a.get_editor_property("settings")
        s.set_editor_property("override_lumen_skylight_leaking", True)
        s.set_editor_property("lumen_skylight_leaking", LEAK)
        a.set_editor_property("settings", s)
        n += 1
unreal.log("Grapple Runner lighting: skylight leaking {} on {} post-process volumes".format(LEAK, n))
