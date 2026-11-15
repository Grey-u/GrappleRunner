# =====================================================================================
#  bake_grass.py  -  run inside the Unreal editor (paste into the Python box at the bottom):
#     exec(open("C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/bake_grass.py").read())
# =====================================================================================
#  WHAT IT DOES: puts a "StylizedNature" actor in each level and tells it to PLANT THE GRASS
#  NOW (StyleHex grass, flowers, bushes, pebbles and trees), saved into the level: you see it
#  in the editor, and the game uses the same grass. Run it again after moving islands.
#
#  WHY IT WAITS (15 Nov): straight after a level is loaded, Unreal hasn't built its collision
#  yet, so the planter found "no ground" and planted almost nothing. So this works step by
#  step, a little each editor frame: load a level, wait ~2 seconds, plant, count; if it
#  planted almost nothing, wait and try again (up to 3 times); save; next level.
#  Watch the Output Log for "BAKE GRASS" lines; the last one says DONE.
# =====================================================================================
import unreal

LEVELS = ["/Game/GrappleRunner/Maps/L_Endless", "/Game/GrappleRunner/Maps/L_FloatingIslands_2"]
WAIT_TICKS = 120          # editor frames to wait after loading (about 2 seconds)

_asub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
_lsub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
_eal = unreal.EditorAssetLibrary
_bake = {"i": 0, "wait": 0, "tries": 0, "loaded": False, "handle": None, "busy": False}

def _nature():
    n = [a for a in _asub.get_all_level_actors() if a.get_class().get_name() == "GRStylizedNature"]
    if n:
        return n[0]
    a = _asub.spawn_actor_from_class(unreal.GRStylizedNature, unreal.Vector(0, 0, 0))
    a.set_actor_label("StylizedNature")
    return a

def _bake_step(dt):
    st = _bake
    # Loading a level makes the editor tick, which would call this again INSIDE itself, again
    # and again until it crashed (stack overflow, 15 Nov). So: one step at a time.
    if st["busy"]:
        return
    st["busy"] = True
    try:
        _bake_step_inner(st)
    finally:
        st["busy"] = False

def _bake_step_inner(st):
    if st["i"] >= len(LEVELS):
        unreal.unregister_slate_post_tick_callback(st["handle"])
        unreal._gr_bake_running = False
        unreal.log("BAKE GRASS: DONE - the grass is in the levels now; look in the viewport")
        return
    lvl = LEVELS[st["i"]]
    name = lvl.split("/")[-1]
    if not st["loaded"]:
        if not _eal.does_asset_exist(lvl):
            st["i"] += 1
            return
        _lsub.load_level(lvl)
        st["loaded"], st["wait"], st["tries"] = True, WAIT_TICKS, 0
        return
    if st["wait"] > 0:
        st["wait"] -= 1
        return
    n = _nature()
    n.call_method("PlantGrassNow")
    total = sum(c.get_instance_count() for c in n.get_components_by_class(unreal.InstancedStaticMeshComponent))
    st["tries"] += 1
    unreal.log("BAKE GRASS: %s try %d: %d clumps" % (name, st["tries"], total))
    if total < 2000 and st["tries"] < 3:
        st["wait"] = WAIT_TICKS            # collision not ready yet: wait and try again
        return
    _lsub.save_current_level()
    unreal.log("BAKE GRASS: %s saved" % name)
    st["i"] += 1
    st["loaded"] = False

if getattr(unreal, "_gr_bake_running", False):
    unreal.log("BAKE GRASS: already running, not starting a second one")
else:
    unreal._gr_bake_running = True     # (one bake at a time; cleared when it finishes)
    _bake["handle"] = unreal.register_slate_post_tick_callback(_bake_step)
    unreal.log("BAKE GRASS: started (it runs over the next few seconds per level)")
