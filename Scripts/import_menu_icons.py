"""
Grapple Runner: import the menu icons (Art/UI/Menu/*.png, drawn by Art/UI/make_menu_icons.py).

Run in Unreal (Output Log, Python mode):
    exec(open("C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/import_menu_icons.py").read())

They land in /Game/GrappleRunner/UI/Menu as UI textures: no mipmaps (always sharp), no
compression blur, never streamed out (so the menu never shows a blurry icon).

HOW TO READ THIS: lines starting with # are notes; "for x in ...:" repeats the steps per file.
"""
import glob
import os
import unreal

SRC = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Art", "UI", "Menu")
DEST = "/Game/GrappleRunner/UI/Menu"

tasks = []
for png in sorted(glob.glob(os.path.join(SRC, "*.png"))):
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", png)
    t.set_editor_property("destination_path", DEST)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", False)
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

done = 0
for png in sorted(glob.glob(os.path.join(SRC, "*.png"))):
    name = os.path.splitext(os.path.basename(png))[0]
    tex = unreal.load_asset(DEST + "/" + name)
    if not tex:
        continue
    tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)   # crisp, full colour
    tex.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    tex.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    tex.set_editor_property("never_stream", True)
    unreal.EditorAssetLibrary.save_loaded_asset(tex)
    done += 1
unreal.log("Grapple Runner menu: imported {} icons into {}".format(done, DEST))
