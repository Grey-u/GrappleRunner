"""
Grapple Runner: import the weapon kit FBXs and lay them out on the first island.

Run:  py "C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/import_weapons.py"
FBXs are read from <Project>/Art/Weapons/ and imported to /Game/GrappleRunner/Weapons.
Re-running re-imports (replaces) them and refreshes the display.

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

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJECT, "Art", "Weapons")
DEST = "/Game/GrappleRunner/Weapons"
NAMES = ["SM_Blade", "SM_BladeHandle", "SM_GrappleGear", "SM_Pistol"]
FOLDER = "GrappleRunner/WeaponDisplay"

tools = unreal.AssetToolsHelpers.get_asset_tools()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
tasks = []
for n in NAMES:
    f = os.path.join(SRC, n + ".fbx")
    if not os.path.exists(f):
        unreal.log_warning("Grapple Runner weapons: missing " + f)
        continue
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", f)
    t.set_editor_property("destination_path", DEST)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    tasks.append(t)
tools.import_asset_tasks(tasks)
for t in tasks:
    unreal.log("Grapple Runner weapons: imported " + ", ".join(str(p) for p in t.get_editor_property("imported_object_paths")))

# display rack on the first landing island, next to the PlayerStart
old = [a for a in actors.get_all_level_actors() if str(a.get_folder_path()) == FOLDER]
if old:
    actors.destroy_actors(old)
start = None
for a in actors.get_all_level_actors():
    if isinstance(a, unreal.PlayerStart):
        start = a.get_actor_location()
        break
base = (start or unreal.Vector(0, 0, 0)) + unreal.Vector(250.0, 0.0, -60.0)
layout = {
    "SM_GrappleGear": (unreal.Vector(0, 0, 100), unreal.Rotator(0, 0, 180)),
    "SM_Blade": (unreal.Vector(0, -90, 120), unreal.Rotator(roll=90, pitch=0, yaw=90)),
    "SM_BladeHandle": (unreal.Vector(0, -90, 120), unreal.Rotator(roll=90, pitch=0, yaw=90)),
    "SM_Pistol": (unreal.Vector(0, 90, 130), unreal.Rotator(0, 0, 180)),
}
for n, (off, rot) in layout.items():
    path = "{}/{}".format(DEST, n)
    if not unreal.EditorAssetLibrary.does_asset_exist(path):
        continue
    a = actors.spawn_actor_from_object(unreal.load_asset(path), base + off, rot)
    a.set_actor_label("Display_" + n)
    a.set_folder_path(FOLDER)
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
unreal.log("Grapple Runner weapons: done")
