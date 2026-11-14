# =====================================================================================
#  bake_grass.py  -  run inside the Unreal editor (paste into the Python box at the bottom):
#     exec(open("C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/bake_grass.py").read())
# =====================================================================================
#  WHY (Grey, 14 Nov: "still no grass"): the grass used to be planted only while the game was
#  running, so the editor never showed it, and the island you start on was skipped. This puts
#  a "StylizedNature" actor in each level and tells it to PLANT THE GRASS NOW (StyleHex grass,
#  flowers, bushes and stones, knee-high), saved into the level: you see it in the editor,
#  and the game uses the same grass. Run it again after you move islands around.
# =====================================================================================
import unreal

LEVELS = ["/Game/GrappleRunner/Maps/L_Endless", "/Game/GrappleRunner/Maps/L_FloatingIslands_2"]
asub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
lsub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
eal = unreal.EditorAssetLibrary

for lvl in LEVELS:
    if not eal.does_asset_exist(lvl):
        continue
    lsub.load_level(lvl)
    nature = [a for a in asub.get_all_level_actors() if a.get_class().get_name() == "GRStylizedNature"]
    if nature:
        n = nature[0]
    else:
        n = asub.spawn_actor_from_class(unreal.GRStylizedNature, unreal.Vector(0, 0, 0))
        n.set_actor_label("StylizedNature")
    n.call_method("PlantGrassNow")
    lsub.save_current_level()
    unreal.log("BAKE GRASS: %s done (see 'baked N clumps' above)" % lvl.split("/")[-1])
unreal.log("BAKE GRASS: DONE - the grass is in the levels now; look in the viewport")
