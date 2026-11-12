"""
Grapple Runner: import the twin ODM blade model (Art/Weapons/SM_ODMSword.fbx).

Run in Unreal (Output Log, Python mode):
    exec(open("C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/import_odm_swords.py").read())

It lands at /Game/GrappleRunner/Weapons/SM_ODMSword. Re-running replaces it.
The model carries three marker points ("sockets": Tip, Base, Edge). The game reads them to
know which way the blade points and which side is sharp, and the script checks they arrived.

HOW TO READ THIS (no coding experience needed)
- Lines starting with # are notes for humans; the computer skips them.
- "import unreal" loads Unreal Engine's own Python toolbox.
- Unreal distances are in centimetres (100 = 1 metre).
"""
import os
import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
FBX = os.path.join(PROJECT, "Art", "Weapons", "SM_ODMSword.fbx")
DEST = "/Game/GrappleRunner/Weapons"

# 1. How to import: one static model, no auto-made collision (a held blade never blocks).
options = unreal.FbxImportUI()
options.set_editor_property("import_mesh", True)
options.set_editor_property("import_as_skeletal", False)
options.set_editor_property("import_materials", True)
options.set_editor_property("import_textures", False)
sm = options.get_editor_property("static_mesh_import_data")
sm.set_editor_property("combine_meshes", True)
sm.set_editor_property("auto_generate_collision", False)

task = unreal.AssetImportTask()
task.set_editor_property("filename", FBX)
task.set_editor_property("destination_path", DEST)
task.set_editor_property("automated", True)
task.set_editor_property("replace_existing", True)
task.set_editor_property("save", True)
task.set_editor_property("options", options)

# 2. Import it.
if not os.path.exists(FBX):
    unreal.log_error("Grapple Runner blades: missing " + FBX)
else:
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = unreal.load_asset(DEST + "/SM_ODMSword")
    # 3. Check the marker points came through.
    names = [str(s.get_editor_property("socket_name")) for s in mesh.get_editor_property("sockets")] if mesh else []
    unreal.log("Grapple Runner blades: imported SM_ODMSword, sockets = " + ", ".join(names))
    if not all(n in names for n in ("Tip", "Base", "Edge")):
        unreal.log_warning("Grapple Runner blades: sockets missing; the game will guess the blade direction from the model's box")
