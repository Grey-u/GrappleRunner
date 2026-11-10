"""
Grapple Runner: build the start-menu level (L_MainMenu) and import the title card.

Run in Unreal (Output Log, Python), after the C++ has been rebuilt:
    py "C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/make_main_menu.py"

What it does
1. Imports Art/UI/T_TitleCard.png (page 1 of the EXIT SUIT portfolio, 1920 x 1080) to
   /Game/GrappleRunner/UI, with UI settings: no mipmaps, no blurry compression.
2. Creates an empty level /Game/GrappleRunner/Maps/L_MainMenu whose game mode is
   GRMenuGameMode (the C++ start menu). The packaged game starts here
   (Config/DefaultEngine.ini: GameDefaultMap).
3. Re-opens the floating islands, so you're back where you were.
To see the menu: open L_MainMenu (Content Drawer, GrappleRunner, Maps) and press Play.

HOW TO READ THIS (no coding experience needed)
- Lines starting with # are notes for humans; the computer skips them.
"""
import os
import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
UI = "/Game/GrappleRunner/UI"
MENU = "/Game/GrappleRunner/Maps/L_MainMenu"
GAME = "/Game/GrappleRunner/Maps/L_FloatingIslands_2"
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

# ---- 1. title card picture
t = unreal.AssetImportTask()
t.set_editor_property("filename", os.path.join(PROJECT, "Art", "UI", "T_TitleCard.png"))
t.set_editor_property("destination_path", UI)
t.set_editor_property("automated", True)
t.set_editor_property("replace_existing", True)
t.set_editor_property("save", True)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([t])
card = unreal.load_asset(UI + "/T_TitleCard")
if card:
    card.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)  # "UserInterface2D"
    card.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
    card.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
    unreal.EditorAssetLibrary.save_asset(card.get_path_name())
    unreal.log("Menu: title card imported")
else:
    unreal.log_warning("Menu: title card import failed")

# ---- 2. the menu level
levels.save_current_level()                                   # keep any work in the islands level
if unreal.EditorAssetLibrary.does_asset_exist(MENU):
    levels.load_level(MENU)
else:
    levels.new_level(MENU)
world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
menu_mode = unreal.load_class(None, "/Script/GrappleRunner.GRMenuGameMode")
if menu_mode:
    world.get_world_settings().set_editor_property("default_game_mode", menu_mode)
    unreal.log("Menu: L_MainMenu uses GRMenuGameMode")
else:
    unreal.log_warning("Menu: GRMenuGameMode not found. Rebuild the C++ first, then re-run.")
levels.save_current_level()

# ---- 3. back to the islands
levels.load_level(GAME)
unreal.log("Menu: done. Open L_MainMenu and press Play to see it.")
