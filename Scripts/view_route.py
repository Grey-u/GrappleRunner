"""Point the editor camera down the Grapple Runner swing route from behind the first island."""
import unreal
ues = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
ues.set_level_viewport_camera_info(unreal.Vector(-5500.0, -3500.0, 2600.0),
                                   unreal.Rotator(roll=0.0, pitch=-8.0, yaw=22.0))
