# NOW (updated 12 Nov 2026)

## Works
- UE 5.8.4 project (VR / OpenXR template), level `L_FloatingIslands_2`, start menu `L_MainMenu`, Endless Runner mode, day/night match clock, 4 planets, pollen/spores (`GRAmbientLife`), haptic glove bridge (`GRHandFeel`, untested without gloves).
- `GrappleRig`: twin hooks, soft rope, swing, Kwang body with ODM gear, vines snap.
- Weapons (`GRArsenal`): 1 twin ODM blades (new, see below), 2 rifle with the animated arms pack (draw/fire/reload montages), 3 bolt-action sniper. Desktop hands reload; VR manual reload written for the headset.
- 8,709 plants swapped to Project Nature photoscans in four colour zones, 680 glowing at night.

## Half done (12 Nov: written, copied to the PC, NOT compiled yet)
- **Twin blades + cutting + one pair of hands.** New files `GRCutter.h/.cpp`; changes in `GRArsenal`, `GrappleRig`, `GrappleRunner.Build.cs` (+ `ProceduralMeshComponent`), `GrappleRunner.uproject` (+ ProceduralMeshComponent plugin). Model: `Art/Weapons/SM_ODMSword.fbx`.
- **To build it (Naomi, about 5 minutes):**
  1. Close Unreal.
  2. Double-click `GrappleRunner.uproject`. It says modules are missing or out of date: click **Yes** to rebuild. (If it fails, open `GrappleRunner.slnx` in Visual Studio and press Build, then send Claude the error lines.)
  3. When the editor opens: Output Log, Python box, paste
     `exec(open("C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/import_odm_swords.py").read())`
     The log should say `sockets = Tip, Base, Edge`.
  4. Press Play. Key 1 = blades. Left click slashes (alternating), right click cross-cuts. Hit a plant (1 hit), a tree trunk (2-5 hacks).
- **What to look for:** do the fists close round the grips? If the hand sits too high or low on the grip, change `GripSlide` on the spawned GRArsenal (Details panel, Arsenal|Blades); if it's turned, `GripTwist`.
- **Cutting in a packaged build** needs "Allow CPU Access" ticked on each cuttable mesh (in the editor it works without, with a warning). Ask Naomi before changing pack assets.
- Kwang's arms, sword and head are hidden (`HiddenBodyBones` on the rig). Check there is now only one pair of hands.
- The arms pack offsets read from the demo (hands (7, 0, -162.6) turn (0, -90, 0); gun on hand_r (-8.66, 2.84, 0.83) turn (-10.45, 167.6, 11.11)) still need baking into the defaults; until then use `gr.Arms` / `gr.Gun` in Play.
- **Bug:** "InverseFast non-invertible matrix" logged every frame since the 10 Nov vines work.

## Decided
- **Game objective (12 Nov, Claude Doc "Grapple Runner: game objective"):** keep the run alive as long as you can. Score = metres × flow multiplier (×1-×5, rises with hooks in a row without touching ground). Gas drains with reel/boost; slicing glowing gas pods refills it; trees take several hacks and felling one opens a route. One second-wind canister per run. Runs of 3-8 min, three missions at a time, a daily run with the same islands for everyone. No kill quotas.
- Graphics are a selling point: move from forward shading to deferred + Lumen Lite + Nanite for PC VR (RTX 4070 Ti). Waiting on Naomi's go-ahead; turn instanced stereo off first (5.8 bug).
- Free assets only for now (12 Nov). Shortlist: Claude Sheets "Grapple Runner asset shortlist".
- Art direction: living Pandora / Log Horizon (giant plants, ivy-covered ruins, colour zones, glowing at night), rendered as realistically as PC VR allows.
- First person is the main view; third person later with no advantage. Shots and cuts trace from the eyes/hands.
- Third-party models stay out of the public repo (`.gitignore` now lists every Fab pack folder).

## Next
1. Build and test the blades (steps above). Fix compile errors if any.
2. Rendering switch: deferred + Lumen Lite + Nanite + virtual shadow maps (commit first; Naomi's OK).
3. "Too bare": add the free Megascans cliffs/rocks/roots under and around the islands, Goddess Temple / Dark Ruins pieces, scanned tropical plants and vines, Megaplants ginkgo and cherry; denser scatter near the route. Convert the 8,709 plant actors to instanced foliage (memory warning).
4. Objective, step 1: gas gauge on the hip + gas drain + glowing gas pods to slice + flow multiplier on the wrist.
5. Fix the InverseFast error.

## Tried and dropped
- Physical Sky Atmosphere (renders black below the horizon here).
- Typing editor commands with select-all/backspace (unsafe); the Output Log box needs `exec(open("...py").read())`.
- Weapon orientation from "the thin end is the barrel" alone.
- A left hand playing the right-hand grasp animation: it stays open. Use the right hand mirrored.
- Hitting things with a ball swept out from the eyes: it can't reach plants (no collision) and doesn't know the blade's angle. Replaced by tracing the blade's own path (`GRCutter`).
