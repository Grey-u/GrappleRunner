"""
Grapple Runner: import the Aeloria music and play it in the level on a loop.

Run with the Floating Islands level open (not while playing):
    py "C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/add_music.py"

- Imports every .wav in <Project>/Art/Audio/ into /Game/GrappleRunner/Audio.
- Sets MUS_Aeloria_Idea to loop and puts a "Music_Aeloria" sound in the level that
  starts by itself when you press Play. It plays "flat" (not from a point in space),
  like a film soundtrack.
- To try one of the other sketches, change TRACK below and run it again.
- MUSIC RIGHTS: the tracks were made by a friend for this game. They are kept OUT of the
  public GitHub repo (see .gitignore) unless the composer says they can be shared.

HOW TO READ THIS (no coding experience needed)
- Lines starting with # are notes for humans; the computer skips them.
- "import unreal" loads Unreal Engine's own Python toolbox.
"""
import os
import unreal

TRACK = "MUS_Aeloria_Idea"     # which imported track plays in the level
VOLUME = 0.6                   # 1.0 = full volume
PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJECT, "Art", "Audio")
DEST = "/Game/GrappleRunner/Audio"
FOLDER = "GrappleRunner/Audio"

# 1. import the .wav files
tasks = []
for name in sorted(os.listdir(SRC)) if os.path.isdir(SRC) else []:
    if name.lower().endswith(".wav"):
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", os.path.join(SRC, name))
        t.set_editor_property("destination_path", DEST)
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("save", True)
        tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
unreal.log("Grapple Runner music: imported {} tracks".format(len(tasks)))

# 2. make the chosen track loop
path = DEST + "/" + TRACK
sound = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
if sound is None:
    unreal.log_warning("Grapple Runner music: " + path + " not found")
else:
    sound.set_editor_property("looping", True)
    unreal.EditorAssetLibrary.save_asset(path)

    # 3. one music player in the level (replace the old one if this is run again)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    old = [a for a in actors.get_all_level_actors() if str(a.get_folder_path()) == FOLDER]
    if old:
        actors.destroy_actors(old)
    player = actors.spawn_actor_from_class(unreal.AmbientSound, unreal.Vector(0, 0, 300))
    player.set_actor_label("Music_Aeloria")
    player.set_folder_path(FOLDER)
    audio = player.get_component_by_class(unreal.AudioComponent)
    audio.set_editor_property("sound", sound)
    audio.set_editor_property("volume_multiplier", VOLUME)
    audio.set_editor_property("auto_activate", True)
    audio.set_editor_property("allow_spatialization", False)   # "flat" soundtrack, not from a point
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
    unreal.log("Grapple Runner music: " + TRACK + " will play on loop")
