# =====================================================================================
#  gr_look.py  -  check the baked grass: counts it and flies the editor camera to it
# =====================================================================================
#  exec(open("C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/gr_look.py").read())
#  Prints (Output Log, lines starting GRLOOK) how many clumps of each plant the level's
#  StylizedNature actor holds, then moves the viewport camera next to some of them, looking
#  down a little, so you can see the grass at once (it's hidden beyond ~65 m).
# =====================================================================================
import unreal
asub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
nat = [a for a in asub.get_all_level_actors() if a.get_class().get_name() == "GRStylizedNature"]
unreal.log("GRLOOK: nature actors %d" % len(nat))
target = None
for n in nat:
    for c in n.get_components_by_class(unreal.InstancedStaticMeshComponent):
        cnt = c.get_instance_count()
        unreal.log("GRLOOK: %s mesh=%s count=%d visible=%s" % (c.get_name(), c.static_mesh.get_name() if c.static_mesh else None, cnt, c.is_visible()))
        if cnt > 0 and target is None:
            t = c.get_instance_transform(cnt // 2, True)   # one clump from the middle of the list
            target = t.translation
if target:
    cam = unreal.Vector(target.x - 250, target.y, target.z + 160)   # 2.5 m back, 1.6 m up
    ues.set_level_viewport_camera_info(cam, unreal.Rotator(0, -30, 0))   # (roll, pitch, yaw): look 30 degrees down
    unreal.log("GRLOOK: camera moved near %s" % target)
