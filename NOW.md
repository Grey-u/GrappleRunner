# NOW (updated 9 Nov 2026, late)

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

- Swing exaggeration (x1.35, +17% speed) and a speed-driven wind sound. The progressive soft rope fixed F6.
- Packs in the project: Paragon Kwang, Infinity Blade Grass Lands, Infinity Blade Adversaries, Realistic Starter VFX Pack Vol 2, Paragon Grux.

## Half done
- Downloading: European Beech, Paragon Agora & Monolith (queued in the launcher; check they finished).
- New packs aren't placed in the level yet. Older UE4 packs may need a fix-up pass on first open.

## Decided
- First person is the main view. Optional third person comes later with no advantage: see "Camera rule" in CLAUDE.md.
- Comfort: reduce motion sickness and help players know their limits. This will be measured with the Simulator Sickness Questionnaire once the headset is back.
- Auto-pull is the default. Pull-on-demand stays available as a setting.
- Base avatar: start from Unreal's Manny, or Naomi's own Paragon character if that's the one she owns.
- The harness (grapple gear) goes on the avatar's hips, not floating on the display rack.

## Next
1. Wind: listen in game and tune volume / pitch.
2. First-person body: show your own legs and hands, and put the grapple gear on the hips. Redesign the harness from real ODM-gear cosplay and climbing-harness references.
3. Level: place Grass Lands ruins and Agora/Monolith structures on the islands for cover and retreats; trees; water.
4. PvE: one Adversaries monster that chases you and can be killed, then blood (VFX pack), then slicing.
5. F8: cheaper plants (instancing, LODs).
- Modes (design): 1v1, 2v2, 3v3, PvE. Realistic gore means an 18+ age rating.

## Tried and dropped
- Physical Sky Atmosphere: it renders black below the horizon in this forward-shaded VR setup, so we use Brushify's sky dome instead.
- First guess for the fall-through bug ("islands have no collision") was wrong: disproved by `diagnose_collision.py`. The cause was the solid sky dome.
- Typing editor commands with select-all and backspace: unsafe when another window can take focus. Paste from the clipboard instead.
