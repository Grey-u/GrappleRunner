# =====================================================================================
#  init_unreal.py  -  Unreal runs this by itself every time the editor opens
# =====================================================================================
#  It only does something if Saved/bake_grass_on_start.flag exists (Claude creates that
#  file when the grass needs re-baking). Then, once the editor has finished opening, it runs
#  Scripts/bake_grass.py (plants the grass into the levels) and deletes the flag, so it
#  happens once. Nothing to paste.
# =====================================================================================
import unreal, os

_saved = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir())
_project = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
_FLAG = os.path.join(_saved, "bake_grass_on_start.flag")

if os.path.exists(_FLAG):
    _state = {"ticks": 0, "handle": None}

    def _gr_bake_when_ready(dt):
        _state["ticks"] += 1
        if _state["ticks"] < 300:          # give the editor a few seconds to finish opening
            return
        unreal.unregister_slate_post_tick_callback(_state["handle"])
        try:
            os.remove(_FLAG)
        except OSError:
            pass
        unreal.log("BAKE GRASS: starting automatically (flag found)")
        exec(open(os.path.join(_project, "Scripts", "bake_grass.py")).read(), {})

    _state["handle"] = unreal.register_slate_post_tick_callback(_gr_bake_when_ready)


# ---- second job: fix floating objects once (flag: Saved/fix_floating_on_start.flag) ----
#  Waits a little longer than the grass bake so the two never run at the same time.
_FIX_FLAG = os.path.join(_saved, "fix_floating_on_start.flag")
if os.path.exists(_FIX_FLAG) and not os.path.exists(_FLAG):
    _fx = {"ticks": 0, "handle": None}

    def _gr_fix_when_ready(dt):
        _fx["ticks"] += 1
        if _fx["ticks"] < 400:
            return
        unreal.unregister_slate_post_tick_callback(_fx["handle"])
        try:
            os.remove(_FIX_FLAG)
        except OSError:
            pass
        unreal.log("FIX FLOATING: starting automatically (flag found)")
        exec(open(os.path.join(_project, "Scripts", "fix_floating.py")).read(), {})

    _fx["handle"] = unreal.register_slate_post_tick_callback(_gr_fix_when_ready)
