# NOW (updated 9 Nov 2026, evening)

## Works
- UE 5.8.4 project (VR / OpenXR template) with Brushify Floating Islands.
- Level `L_FloatingIslands_2`:
  - 14 landing islands and 53 `GrappleAnchor` rocks
  - backdrop, sky dome, mist and waterfalls
  - **805 plants** on the islands' tops
  - **Aeloria music** on a loop.
- `GrappleRig` (C++), on desktop:
  - twin hooks with **auto-pull** (hold Shift / grip to stop pulling and swing)
  - **soft rope** (`bSoftRope`)
  - **flexible orange rope**
  - rope pull in newtons, which drives the VR vibration
  - gas boost, walking, respawn.
- Testing:
  - telemetry CSV every session
  - `gr.Test snap|reel [stiff|soft]` and `gr.Test perf`
  - `Tools/analyse_telemetry.py` and `Tools/rope_model.py`
  - T-01 to T-12 written up in `Docs/TESTS.md`.

## Half done
- F7: the rope pull now adds the reel's pull plus the spring's per step. That code is on the PC but **not compiled yet**. It compiles on the next open: if Unreal asks to rebuild, say Yes.

## Decided
- First person is the main view. Optional third person comes later with no advantage: see "Camera rule" in CLAUDE.md.
- Comfort: reduce motion sickness and help players know their limits. This will be measured with the Simulator Sickness Questionnaire once the headset is back.
- Auto-pull is the default. Pull-on-demand stays available as a setting.
- Base avatar: start from Unreal's Manny, or Naomi's own Paragon character if that's the one she owns.
- The harness (grapple gear) goes on the avatar's hips, not floating on the display rack.

## Next
1. Confirm the owned character (the Fab library shows "Paragon: Minions"). Add it to the project with Naomi's OK.
2. First-person body: show your own legs and hands, and put the grapple gear on the hips. Redesign the harness from real ODM-gear cosplay and climbing-harness references.
3. F6: stop the soft rope reaching its limit in fast short-rope swings (progressive spring), plus a `gr.Test swing` case.
4. F8: cheaper plants (instancing, LODs). Then water and jungle dressing.

## Tried and dropped
- Physical Sky Atmosphere: it renders black below the horizon in this forward-shaded VR setup, so we use Brushify's sky dome instead.
- First guess for the fall-through bug ("islands have no collision") was wrong: disproved by `diagnose_collision.py`. The cause was the solid sky dome.
- Typing editor commands with select-all and backspace: unsafe when another window can take focus. Paste from the clipboard instead.
