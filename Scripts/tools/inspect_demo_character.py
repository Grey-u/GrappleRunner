"""Print how the First Person Rifle Animation demo character attaches its camera, hands and gun
(so GRArsenal can copy the exact same offsets). Spawns it, logs, deletes it again."""
import unreal
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
cls = unreal.load_object(None, "/Game/FirstPersonRifleAnimations/Blueprints/BP_DemoCharacter.BP_DemoCharacter_C")
a = actors.spawn_actor_from_class(cls, unreal.Vector(0, 0, 100000))
for c in a.get_components_by_class(unreal.SceneComponent):
    p = c.get_attach_parent()
    unreal.log("DEMO %s | parent=%s socket=%s | loc=%s rot=%s | mesh=%s" % (
        c.get_name(), p.get_name() if p else None, c.get_attach_socket_name(),
        c.get_editor_property("relative_location"), c.get_editor_property("relative_rotation"),
        (c.get_editor_property("skeletal_mesh_asset").get_name() if isinstance(c, unreal.SkeletalMeshComponent) and c.get_editor_property("skeletal_mesh_asset") else "")))
actors.destroy_actor(a)
