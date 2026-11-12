# NOW (updated 12 Nov 2026, afternoon)

## Works
- UE 5.8.4 project (VR / OpenXR template), level `L_FloatingIslands_2`, start menu `L_MainMenu`, Endless Runner mode, day/night match clock, 4 planets, pollen/spores (`GRAmbientLife`), haptic glove bridge (`GRHandFeel`, untested without gloves).
- `GrappleRig`: twin hooks, soft rope, swing, Kwang body with ODM gear, vines snap.
- Weapons (`GRArsenal`): 1 twin ODM blades (new, see below), 2 rifle with the animated arms pack (draw/fire/reload montages), 3 bolt-action sniper. Desktop hands reload; VR manual reload written for the headset.
- 8,709 plants swapped to Project Nature photoscans in four colour zones, 680 glowing at night.

## Half done
- **Blades work** (built and tested 12 Nov): key 1, LMB/V slash, RMB/C cross-cut, F9 slow motion. Cutting works on plants. Still to check by hand: felling a tree (2-5 hacks on the trunk), the grip with `GripSlide` 4, and how it feels at full speed with a real mouse.
- **Rendering is now deferred + Lumen** (instanced stereo off). Check frame rate in the headset when it arrives; if it's under 90, drop Lumen to Lite / lower scalability.
- **Islands dressed** with free Megascans (cliffs under, rocks round, roots) plus 1,474 more photoscanned plants. Plant actors are now about 10,000: convert to instanced foliage for memory and VR frame time.
- **Disk space: C: has under 5 GB free.** Free some space before adding more assets (high-quality Megascans are 4K).
- Cutting in a packaged build needs "Allow CPU Access" on each cuttable mesh (works in the editor without it, with a warning). Ask Naomi first.
- The arms pack offsets read from the demo still need baking into the defaults (`gr.Arms` / `gr.Gun` for now).
- **Bug:** "InverseFast non-invertible matrix" logged every frame since the 10 Nov vines work.

## Decided
- **Game objective (12 Nov, Claude Doc "Grapple Runner: game objective"):** keep the run alive as long as you can. Score = metres × flow multiplier (×1-×5, rises with hooks in a row without touching ground). Gas drains with reel/boost; slicing glowing gas pods refills it; trees take several hacks and felling one opens a route. One second-wind canister per run. Runs of 3-8 min, three missions at a time, a daily run with the same islands for everyone. No kill quotas.
- Graphics are a selling point: move from forward shading to deferred + Lumen Lite + Nanite for PC VR (RTX 4070 Ti). Waiting on Naomi's go-ahead; turn instanced stereo off first (5.8 bug).
- Free assets only for now (12 Nov). Shortlist: Claude Sheets "Grapple Runner asset shortlist".
- Art direction: living Pandora / Log Horizon (giant plants, ivy-covered ruins, colour zones, glowing at night), rendered as realistically as PC VR allows.
- First person is the main view; third person later with no advantage. Shots and cuts trace from the eyes/hands.
- Third-party models stay out of the public repo (`.gitignore` now lists every Fab pack folder).

## Next
1. Objective, step 1: gas gauge on the hip, gas drain, glowing gas pods to slice, flow multiplier on the wrist.
2. Convert plant actors to instanced foliage (memory, frame time).
3. Free disk space, then more free assets: scanned vines over the island edges, Goddess Temple / Dark Ruins pieces, Megaplants ginkgo and cherry for colour zones.
4. Fix the InverseFast error.

## Tried and dropped
- Physical Sky Atmosphere (renders black below the horizon here).
- Typing editor commands with select-all/backspace (unsafe); the Output Log box needs `exec(open("...py").read())`.
- Weapon orientation from "the thin end is the barrel" alone.
- A left hand playing the right-hand grasp animation: it stays open. Use the right hand mirrored.
- Hitting things with a ball swept out from the eyes: it can't reach plants (no collision) and doesn't know the blade's angle. Replaced by tracing the blade's own path (`GRCutter`).
