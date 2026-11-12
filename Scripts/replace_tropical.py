"""
Grapple Runner: swap the tropical plants for bushes and ferns (12 Nov 2026).

Naomi: "I like the bushes but don't like how the tropical plants look."
So every banana, pandanus and big-leaf tropical ground plant in the level becomes a bushy fern
(Project Nature's photoscanned ferns, the dense green bushes she liked), roughly the same size.
Plants that were glowing at night keep their glow.

Run with the Floating Islands level open (not while playing), Output Log in Python mode:
    exec(open("C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/replace_tropical.py").read())

Writes Saved/Tests/tropical_swap.csv (what was swapped, so it can be undone).

HOW TO READ THIS (no coding experience needed)
- Lines starting with # are notes for humans; the computer skips them.
- "for x in ...:" repeats the indented steps once per item; "if ...:" only runs them when true.
- Unreal sizes are centimetres (100 = 1 metre).
"""
import os
import random
import unreal

random.seed(21)
TROPICAL = ("/Game/PN_Banana/", "/Game/PN_Pandanus/", "/Game/PN_tropicalGroundPlants/")
MAX_FERN_SCALE = 1.8      # a fern blown up more than this looks wrong; bigger plants get a cluster instead

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
registry = unreal.AssetRegistryHelpers.get_asset_registry()


def meshes_under(folder):
    out = []
    for d in registry.get_assets_by_path(folder, recursive=True):
        if str(d.asset_class_path.asset_name) == "StaticMesh":
            out.append(unreal.load_asset(str(d.package_name)))
    return out


ferns = [m for m in meshes_under("/Game/PN_Fern") if m]
ferns.sort(key=lambda m: m.get_bounds().box_extent.z)
if not ferns:
    raise RuntimeError("No fern meshes found under /Game/PN_Fern")
fern_h = [max(1.0, m.get_bounds().box_extent.z * 2.0) for m in ferns]
unreal.log("Grapple Runner tropical swap: {} fern/bush models to use".format(len(ferns)))

rows = ["label,old,new,old_height_cm"]
swapped = 0
extra = 0
for a in actors.get_all_level_actors():
    if not isinstance(a, unreal.StaticMeshActor):
        continue
    c = a.static_mesh_component
    m = c.static_mesh if c else None
    if not m or not m.get_path_name().startswith(TROPICAL):
        continue
    scale = a.get_actor_scale3d()
    old_h = m.get_bounds().box_extent.z * 2.0 * scale.z
    # Pick the fern whose own height is closest to the old plant's, within the scale limit.
    i = min(range(len(ferns)), key=lambda k: abs(fern_h[k] * MAX_FERN_SCALE - old_h) if fern_h[k] * MAX_FERN_SCALE < old_h else abs(fern_h[k] - old_h))
    s = max(0.7, min(MAX_FERN_SCALE, old_h / fern_h[i])) * random.uniform(0.9, 1.1)
    overlay = c.get_editor_property("overlay_material")
    c.set_static_mesh(ferns[i])
    for slot in range(c.get_num_materials()):
        c.set_material(slot, None)          # back to the fern's own materials (no tropical tint copies)
    c.set_editor_property("overlay_material", overlay)
    a.set_actor_scale3d(unreal.Vector(s, s, s))
    a.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=random.uniform(0, 360)), False)
    rows.append("{},{},{},{:.0f}".format(a.get_actor_label(), m.get_name(), ferns[i].get_name(), old_h))
    swapped += 1
    # A tall banana or pandanus tree left a gap no single fern fills: add two more round it.
    if old_h > fern_h[i] * MAX_FERN_SCALE * 1.5:
        loc = a.get_actor_location()
        for k in range(2):
            p = unreal.Vector(loc.x + random.uniform(-150, 150), loc.y + random.uniform(-150, 150), loc.z)
            b = actors.spawn_actor_from_object(ferns[i], p, unreal.Rotator(roll=0.0, pitch=0.0, yaw=random.uniform(0, 360)))
            if b:
                b.set_actor_scale3d(unreal.Vector(s * 0.9, s * 0.9, s * 0.9))
                bc = b.static_mesh_component
                bc.set_collision_profile_name("NoCollision")
                bc.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
                bc.set_editor_property("ld_max_draw_distance", 20000.0)
                b.set_folder_path(str(a.get_folder_path()))
                extra += 1

out_dir = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "Tests")
os.makedirs(out_dir, exist_ok=True)
with open(os.path.join(out_dir, "tropical_swap.csv"), "w") as f:
    f.write("\n".join(rows))
unreal.log("Grapple Runner tropical swap: {} tropical plants became ferns/bushes, {} extra filled tall gaps".format(swapped, extra))
unreal.EditorLevelLibrary.save_current_level()
