"""Frame the weapon display next to the PlayerStart and save a screenshot."""
import unreal
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
disp = [a for a in actors.get_all_level_actors() if a.get_actor_label().startswith("Display_SM_GrappleGear")]
if disp:
    c = disp[0].get_actor_location()
    unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).set_level_viewport_camera_info(
        c + unreal.Vector(-260.0, 0.0, 40.0), unreal.Rotator(roll=0.0, pitch=-4.0, yaw=0.0))
    unreal.log("Grapple Runner: camera at weapons {}".format(c))
