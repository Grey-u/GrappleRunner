"""
Grapple Runner: import the harness v2 pieces into Unreal.

Run in Unreal (Output Log, set the box at the bottom to "Python" or type "py " first):
    py "C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/import_harness.py"

What it does
- Reads Art/Harness/SM_Harness_Waist.fbx, SM_Harness_ThighA.fbx and SM_Harness_ThighB.fbx
  (made by Art/Harness/make_harness.py) and imports them to /Game/GrappleRunner/Harness.
- Gives each colour slot the weapon kit's existing material with the same name
  (M_Steel, M_Leather, ...), so the harness matches the blades and pistol.
- (No collision needed: the game switches it off on the harness parts; the physics uses the capsule.)
- Re-running replaces the old versions, so it's safe to run again after changing the design.

HOW TO READ THIS (no coding experience needed)
- Lines starting with # are notes for humans; the computer skips them.
- "import" = load a toolbox. "unreal" is Unreal Engine's own Python toolbox.
- "for x in ...:" repeats the indented steps once per item; "if ...:" only runs them when true.
"""
import os
import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJECT, "Art", "Harness")
DEST = "/Game/GrappleRunner/Harness"
MATERIALS = "/Game/GrappleRunner/Weapons"     # where the weapon kit's materials already live
NAMES = ["SM_Harness_Waist", "SM_Harness_ThighA", "SM_Harness_ThighB"]

# ---- 1. import the three FBX files
tasks = []
for n in NAMES:
    f = os.path.join(SRC, n + ".fbx")
    if not os.path.exists(f):
        unreal.log_warning("Harness: missing " + f)
        continue
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", f)
    t.set_editor_property("destination_path", DEST)
    t.set_editor_property("automated", True)          # no pop-up questions
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

# ---- 2. use the weapon kit's existing materials
for n in NAMES:
    path = "{}/{}".format(DEST, n)
    mesh = unreal.load_asset(path)
    if not mesh:
        unreal.log_warning("Harness: import failed for " + n)
        continue
    slots = mesh.get_editor_property("static_materials")
    for i, slot in enumerate(slots):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        # the slot is named after the Blender material, sometimes with a suffix added
        base = slot_name.split(".")[0]
        for candidate in (base, base.rsplit("_", 1)[0]):
            m = unreal.load_asset("{}/{}".format(MATERIALS, candidate))
            if m:
                mesh.set_material(i, m)
                break
        else:
            unreal.log_warning("Harness: no material found for slot " + slot_name)
    unreal.EditorAssetLibrary.save_asset(path)
    unreal.log("Harness: ready " + path + " ({} colour slots)".format(len(slots)))

unreal.log("Harness: done. Press Play and look down.")
