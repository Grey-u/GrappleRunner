# NOW (updated 11 Nov 2026)

## Works
- UE 5.8.4 project (VR / OpenXR template), level `L_FloatingIslands_2`: 47 islands, 70 overgrown ruin clusters, about 1,550 plants, 250 snappable vines, rose atmosphere, Aeloria music.
- Start menu (`L_MainMenu`): EXIT SUIT cover title card, then Endless Runner / 1v1 / 2v2 / 3v3 / 5v5 / PvE, then a loading screen. Every mode plays solo for now.
- `GrappleRig`: twin hooks (auto-pull, Shift = swing), soft flexible rope, swing exaggeration, wind, Kwang body with ODM gear on the hips, vines snap when you fly through.
- **New 11 Nov (compiled and tested in Play):**
  - `GREndlessSpawner` (endless island chunks, distance score)
  - `GRDayNight` (each match runs dawn to night)
  - `GRPlanets` (4 planets, glow at night)
  - `GRArsenal`: 11 weapons. ODM swords, then Kuronami Rifle, SMG, Marksman, Revolver and Blade, then Araxys Rifle, Burst, Sniper, Sidearm and Knife. Each has switching, aim, scope, fire, tracers, recoil, reload, slash and stab.
- Desktop controls: Q/E hooks, LMB fire or slash, RMB aim or stab, 1-0 or the wheel to switch, X holster, T reload, Shift swing, Space jump/boost, R respawn.
- Testing: telemetry CSV, `gr.Test snap|reel|catch|swing|perf`, T-01 to T-14 in `Docs/TESTS.md`.

## Half done
- Aim down sights and the scope are written but not yet checked by hand. Hold RMB with the rifle, then with the Araxys Sniper (key 9). Tune `HipOffset` / `AimDistance` per weapon in the Details of the spawned `GRArsenal` (or in `SetDefaultWeapons`).
- Endless Runner: start it from the menu (or press Play) and swing past the last island to check that chunks appear. Not yet played end to end.
- Day/night: watch a full 12 min match once and check that night isn't too dark.
- **Bug:** "InverseFast non-invertible matrix" logged every frame during Play, since the 10 Nov vines work. Find the zero-size piece (vines or rope links).
- Weapons in VR: they are held by the controllers, but trigger-to-fire waits for the headset step.
- ODM model licence: get the Sketchfab link from Naomi. The Kuronami and Araxys models are fan models of Riot designs: private prototype only.
- Beech trees use 8K textures: cap them at 2048 for VR (needs Naomi's OK, it changes pack assets).
- Restore Packages prompt on start: M_VineLeaf / M_VineStem autosaves from the crash. Skipped; the saved versions are fine.

## Decided
- Art direction (11 Nov): **sepia haze**. One warm family (peach, amber, umber) through thick dusty mist, ruins as silhouettes, a low sun disc, planets small and far off behind the haze. `color_grade.py` MOOD = "haze".
- First person is the main view. Optional third person later with no advantage (CLAUDE.md camera rule). Shots trace from the eyes.
- A match runs from day to night instead of a countdown timer.
- Third-party models stay out of the public repo.
- Auto-pull is the default.

## Next
0. Sepia haze polish: wet reflective stone underfoot; find the magenta light near a floating rock (not one of ours); check grapple rocks stay readable in the thicker fog.
1. Hand-check aim and scope, and tune hip/aim offsets (one weapon at a time).
2. Fix the per-frame InverseFast error.
3. PvE: one Adversaries monster that chases you and takes damage from `GRArsenal` (ApplyPointDamage is already sent), then blood VFX, then slicing.
4. In-world ammo counter on the weapon, gas gauge on the hip (VR comfort rule: no screen HUD).
5. Performance: `gr.Test perf` (T-15), cap beech textures.

## Tried and dropped
- Physical Sky Atmosphere: it renders black below the horizon in this setup, so we use Brushify's sky dome.
- "Islands have no collision" was the wrong guess for the fall-through bug; the cause was the solid sky dome.
- Typing editor commands with select-all and backspace is unsafe. Paste from the clipboard instead. The Output Log box is in Python mode, so use `exec(open("...py").read())`, not `py "..."`.
- Weapon orientation by "the thin end is the barrel" alone: it got the revolver and the burst rifle backwards. It's now decided by the grip and magazine.
