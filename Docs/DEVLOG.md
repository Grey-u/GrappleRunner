# Grapple Runner: Dev Log

One entry per work session: what was built, what broke, and what was learned. Newest at the bottom.

---

## Before Nov 2026: Swing physics test bench
- Built `Tools/physics-bench.html`, a 2D canvas pendulum and rope simulation in real units (70 kg player, 1/120 s fixed step, live rope tension readout).
- **Why:** to tune the swing feel cheaply before writing any Unreal code. Settled on rope 18 m, reel speed 2 m/s, hooks 11–21 m high and 7–16 m apart.

## 7 Nov 2026: Project setup and procedural level
- Installed **Unreal Engine 5.8.4**. Created the GrappleRunner project from the VR (OpenXR) template and added the *Brushify Floating Islands* pack.
- Wrote `Scripts/build_floating_islands.py`, which generates the level procedurally: 14 landing islands on a climbing route, 53 grapple anchors (tagged `GrappleAnchor`) spaced using the bench numbers, 45 backdrop islands, sky, fog and a spawn point.
- **Problems fixed:** `Rotator.rotate_vector` doesn't exist in UE 5.8 Python (replaced with trigonometry). A fog property was renamed in 5.8 (added a version-tolerant setter). The editor launched with admin rights and wouldn't accept input (relaunched normally).

## 7 Nov 2026: Lighting, sky and mist
- `setup_lighting.py` sets up a dynamic sun and sky light (no lighting build needed), fixed VR exposure, and post-processing.
- **Problem:** the physical sky rendered black below the horizon with the VR template's forward renderer.
- **Fix:** `sky_and_mist.py` swaps it for Brushify's HDR sky dome, as the pack's own demo does. It adds 54 animated mist cards in three layers under the islands, plus 5 Niagara waterfalls.

## 7 Nov 2026: Weapon kit v1 (Blender)
- `Art/Weapons/make_weapons.py` builds all four weapons procedurally in Blender Python, in real-world metres with origins at the grip:
  - snap-segment blade
  - trigger handle
  - hip grapple gear (gas unit, tanks, launchers with reels, thigh blade boxes)
  - universal sidearm
- Exported as FBX and imported into Unreal with `import_weapons.py`.
- Designed the **agent roster** (Kestrel, Lark, Mistral, Anchor). Weapons are shared like Valorant; abilities modify the grapple.

## 8 Nov 2026: Grapple in C++
- Wrote `Source/GrappleRunner/GrappleRig` (about 600 lines of C++):
  - two independent hooks with ray trace and cone aim assist onto anchor rocks
  - a rope-length constraint that removes outward velocity (the same model as the bench)
  - reel-in with pull, gas boost, capsule sweep collision with sliding and ground detection
  - kill-plane respawn and controller haptics
  - input for Vive, Index, Quest and Windows MR controllers, plus a mouse and keyboard test mode
- Every tuning value is exposed in the Details panel.
- **Blocker:** UE 5.8 rejects the MSVC 14.44 compiler (banned for a compiler bug), and 14.36 is too old. Installing MSVC 14.42.

### Next
- Compile and place the rig, then test-swing on desktop.
- VR test once the headset is back.
- Attach the weapons to the player, add a speed vignette for comfort, and tune the feel.

## 8 Nov 2026 (afternoon): First compile, first swing, first test log
- **Compiled.** The C++ grapple built first time on UE 5.8: 0 errors, 0 warnings, 89 s. Placed the rig in the level and played on desktop.
- **Bug: the player fell through the world.**
  - Tested three possible causes one at a time with a new tool, `Scripts/diagnose_collision.py`. It found the islands' collision was fine.
  - Added frame-by-frame **telemetry** to the C++. It writes a CSV of position, velocity, rope length, distance to anchor, gas and collisions to `Saved/Tests/`.
  - Telemetry showed the real cause: the sky dome was still solid, so the player was "pushed out" 3.6 km through it. Fixed it, and made the scripts check their own changes.
- **Measured the physics against its settings** with `Tools/analyse_telemetry.py`:
  - the rope never stretched: 0.00 cm over 687 frames;
  - reel speed 900.04 vs 900 cm/s;
  - gravity 979.8 vs 980 cm/s²;
  - drag 0.0600 vs 0.060.
- **Found a comfort problem (F1).** When a slack rope snaps tight, the player loses about 73% of their speed in one frame. A sudden deceleration like that is a known motion-sickness trigger, and it's the first thing to fix in feel tuning.
- Full write-up with charts, screenshots and raw data: `Docs/TESTS.md` (T-01 to T-08).
- **Design decision:** first person is the main view. An optional third person may come later, with all aiming and visibility traced from the character's eyes so it can't give an advantage.

## 9 Nov 2026: Rope comfort, flexible rope, auto-pull, plants and music
- **Soft rope (comfort).**
  - Predicted with an offline Python model first (`Tools/rope_model.py`), then tested in Unreal with a new repeatable console test (`gr.Test`).
  - The rope-snap jolt fell from 209 g to 14.7 g. The game and the model agreed to within 0.12 g on every one of 360 physics steps (T-09).
  - The model also caught a problem before it reached the game: with a soft rope, reeling in would catapult you. Fixed with a "reel stall".
- **Flexible rope.** The rope is now a 24-link chain that sags, swings and whips, in orange. Rope pull is measured in newtons from a 70 kg body and drives VR controller vibration, so you feel your weight on the rope.
- **Auto-pull (Naomi's request).** Hooks reel you in by themselves, like Attack on Titan's gear. Hold grip or Shift to stop pulling and swing (T-12).
- **Honest result (F6).** In a real, hand-played swing, the soft rope was too soft for a short rope and hit its safety limit (27 times body weight). The scripted test hadn't covered that case. Next: a progressive spring, then add that case to `gr.Test`.
- **Level.**
  - 805 plants (grass, clover, flowers, heather, bushes) scattered on the islands' tops: no collision, no shadows. Measured cost: +0.3 ms per frame (T-10).
  - The **Aeloria** music by a friend is in the level on a loop. The tracks are kept out of the public repo unless the composer agrees.
- **Mistake and lesson.**
  - While Naomi was typing, the Claude window took keyboard focus. A "select all + backspace" meant for Unreal's command box went to the level instead, and deleted 988 actors in the editor.
  - Nothing had been saved, so Edit, then Undo, restored everything. A check afterwards confirmed the level was intact.
  - New rule for editor automation: never send select-all or delete keys; paste commands from the clipboard instead.

## 9 Nov 2026 (evening): Bigger swings, wind, and new content packs
- **Swing exaggeration.** While the rope is tight, extra gravity acts only along the swing, so arcs are 17% faster at the bottom, exactly as theory predicts (T-14).
- **Wind.**
  - Made a 16-second seamless wind loop from scratch in Python (`Tools/make_wind_sound.py`: filtered noise plus gusts that loop cleanly). No recording, no licence.
  - In game it gets louder and higher-pitched with speed (`UpdateWind`).
- **F6 fixed.**
  - The soft rope now stiffens the more it stretches.
  - A grid search in the model picked the settings, and a new `gr.Test catch` case reproduced the T-11 hard case. It now stays 32 cm inside the stretch limit, at 22.8 g against 272.7 g for the stiff rope (T-13).
- **Content from Naomi's Fab library**, all free and already owned:
  - Paragon: Kwang (the base avatar)
  - Infinity Blade: Grass Lands (ruins)
  - Infinity Blade: Adversaries and Paragon: Grux (monsters)
  - Realistic Starter VFX Pack (blood and impacts)
  - European Beech (trees) and Paragon: Agora & Monolith (ancient structures), queued.
  - Older Unreal 4 packs were added using the closest engine version offered (5.4 or 5.6). MAE Oak Forest wouldn't add to a 5.8 project, so it was skipped.
- **Design notes from Naomi.** Game modes: 1v1, 2v2, 3v3 and PvE against monsters, with realistic gore.

## 9 Nov 2026 (night): Visible body and hip harness (not yet compiled)
- **What was added.** Kwang (Paragon) now stands under your eyes in first person. His head is hidden so the camera is never inside it. He switches between idle, run and in-air animations by speed and whether he's on the ground.
- **Harness.** The grapple gear (`SM_GrappleGear`) is strapped to the hip bone (`pelvis`), and the ropes leave from its left and right launchers (`RopeStart`).
- **Fix made while reviewing the code.** Parts made while the game runs were attached with `SetupAttachment`, which only works for parts built in the constructor. The body and harness now use `AttachToComponent`, and the harness is bound to the hip bone. Both are listed under the actor (`AddInstanceComponent`) so they show in the Outliner.
- **Status.** Code written and reviewed, but not compiled or played yet. Next: rebuild in Unreal, check legs, hands and harness position from the first-person view, then tune `HarnessOffset`, `HarnessRotation` and `BodyScale`.
- **Why it's on a branch.** The code is on the `body-and-harness` branch until it compiles, so `main` stays a version that works.

## 10 Nov 2026: Real ODM gear, ruins and trees, colour pass, start menu
- **Body check.** Naomi played and saw Kwang's legs. Hand and leg tracking will come from real sensors later.
- **Harness, three versions in one day (the design process).**
  - **v2:** all-in-one harness generated in code (`Art/Harness/make_harness.py`), designed from Attack on Titan ODM-gear cosplay builds and climbing-harness anatomy: padded waist belt, belay loop, gear loops, leg loops, plus blade boxes, gas tanks and launchers. Three pieces (hips + each thigh) so the leg parts move with the legs. Renders in `Docs/media/harness/`.
  - **Feedback:** "not up to Half-Life: Alyx / Valorant standard." Fair: shapes generated in code, with flat colours, can't reach a sculpted, hand-textured look.
  - **v3:** Naomi found a sculpted, textured ODM-gear model on Sketchfab (4K PBR textures). `Art/ThirdParty/make_odm_gear.py` splits it into the gear and two swords, sets each origin to its attach point, and drops the cables (the game draws its own ropes). The code straps keep holding it on (the script now builds straps only). The model stays out of the public repo until its licence is confirmed.
  - In game: the gear sits on the hip bone, tilted 12° so the blade boxes hang back and down, and widened 1.2× to clear the thighs. The ropes now fire from the gear's nozzles.
- **Level dressing** (`Scripts/dress_islands.py`): 13 ruin clusters from Infinity Blade Grass Lands (temples, wall lines for cover, gates, towers), European Beech trees and saplings, ponds, extra waterfalls, all kept 7 m clear of grapple rocks.
  - Problem found: the beech trees ship with 8K textures, and Unreal froze for minutes building them on first use. They need smaller texture sizes for VR.
- **Colour** (`Scripts/color_grade.py`): Naomi said the game looked colourless and dull. Golden-hour sun, cool blue shadow fill, turquoise mist with a golden sun glow, a warm/cool grade and more saturation.
- **Start menu** (`GRMenuGameMode`, C++/Slate): the EXIT SUIT portfolio cover is the title card. PLAY leads to 1v1 / 2v2 / 3v3 / 5v5 / PvE, then a loading screen. Multiplayer isn't built, so every mode plays solo for now. The packaged game starts at `L_MainMenu`.

## 10 Nov 2026 (evening): Ethereal fantasy pass and snapping vines
- **Brief from Naomi:** nature taking over architecture, an ethereal fantasy feel with atmosphere, no "white-washed" look, 70% more greenery, and vines that snap when you fly through them. The old harness on the weapon stand is to go.
- **Snapping vines (C++).** Each frame, `AGrappleRig::SnapVines` checks the path your body took. It first checks a sphere round each vine, then the exact distance from the body's path to each short piece of the vine. If you hit a vine faster than 4 m/s, it breaks there. The two halves drop for 0.35 s, then swing like pendulums whose swing dies away over about 1.6 s (period = 2π√(length / g)), and the controllers give a light buzz. The vines are still not solid, so they never stop the player.
- **Overgrown ruins** (`dress_islands.py`): every column, wall, arch, tower and statue gets roots draped over its top, one or two hanging vines and ferns or bushes at its base. 60% of temple courtyards are flooded with still water. Trees, saplings and undergrowth are up 70%. Ground plants go from 90 to 153 tries per island, scaled up for big islands, with ferns added.
- **More vines** (`expand_world.py`): up to 3 per tree, plus vines from the tops of tall ruins to the nearest tree.
- **Atmosphere** (`color_grade.py`, now three moods: rose, amber and teal):
  - a low coloured sun with light shafts;
  - thick coloured volumetric fog that glows toward the sun;
  - a tinted, dimmer sky light, so shadows are coloured rather than grey;
  - a stronger grade (cool shadows, warm highlights);
  - about 90 glowing orbs hanging from the tree canopies, plus soft coloured lights by the ruins, with no shadows to keep them cheap.
- **Performance risk:** all of this adds draw calls and overdraw. Measure with `gr.Test perf` (T-15) before VR testing.

## 11 Nov 2026: Endless Runner, day-to-night matches, planets, and the weapon system
- **Brief from Naomi:**
  - every mode is its own menu choice, with Endless Runner among them;
  - build the endless runner;
  - planets in the sky;
  - each match runs from day to night instead of using a Valorant-style timer;
  - use the downloaded Kuronami and Araxys weapons, switched with the mouse wheel or number keys;
  - every weapon (knives included) can be used, with aim and scope animation.
- **Endless Runner** (`GREndlessSpawner`). It builds island "chunks" 300 m ahead of you and removes them 150 m behind you, so memory stays flat however far you swing. Each chunk has a Brushify island 18–40 m wide, 45–70 m after the last one, with grapple rocks 11–21 m up and 7–16 m apart (the spacing the 2D bench and tests T-01 to T-14 proved swingable). Trees, ferns, an overgrown ruin and a snappable vine are added on top. It keeps a distance score and your best run.
- **Day-to-night match clock** (`GRDayNight`). The sun rises, crosses the sky and sets over one match (12 min by default), then the moon light takes over. Sun colour, sky light and fog are blended between dawn, noon, dusk and night looks. In Endless mode the day loops.
- **Planets** (`GRPlanets`, `Art/Planets/make_planets.py`). Four worlds are painted in code from layered noise: Vael, a ringed gas giant in Arcadium orange; Lun, a cratered moon; Iskra, an ice world; Merrow, an ocean world. Every frame each one is placed 6 km from the camera in a fixed direction, so it behaves like it's infinitely far away (it never gets closer, however far you swing). Their lit side faces the real sun, so dawn and dusk give crescents. They are faint by day and bright at night.
- **Weapons** (`GRArsenal`, a new class; the weapon code moved out of the rig):
  - **Preparing the models** (`Art/ThirdParty/make_skin_weapons.py`, Blender scripted). Each pack was one FBX with every weapon facing a different way, plus separate magazines and scopes.
    - The script joins each weapon's parts, cleans the mesh and points the barrel along +X.
    - Which way the barrel points: for a gun with a magazine, the pistol grip hangs right behind the magazine. For pistols, the barrel tip is the end furthest from the grip. Knives are laid out by PCA (their main length direction).
    - The pivot goes on the grip, and the script saves two sockets: `Muzzle` and `Sight`.
    - First attempts got the revolver and the burst rifle backwards and saw the Araxys knife side-on. A side-view contact sheet showed this, and each rule was fixed.
  - **Import** (`Scripts/import_skin_weapons.py`): one material per texture set (colour, normal, and metal/roughness from the MRS mask), glowing materials for the Kuronami water and the Araxys light strips, and a display rack by the spawn.
  - **Animation is "procedural".** The models have no animations of their own, so the code works out the weapon's place each frame:
    - a blend from hip to aim (the gun moves so its `Sight` socket sits on the eye line);
    - recoil kick;
    - sway that lags behind the mouse, and a walk bob;
    - switching (lower, swap, raise) and a reload tilt;
    - knife slash and heavy-stab curves.
  - **Snipers.** Aiming zooms in and hides the gun behind a reticle and a dark vignette. Mouse sensitivity scales with the zoom.
  - **Shooting.**
    - Shots are line traces from the eyes, following the CLAUDE.md camera rule.
    - Spread is smaller when aiming and bigger in the air.
    - A hit deals Unreal point damage (ready for enemies) and shoves loose physics objects.
    - Shots and blades snap vines.
    - Each shot draws a glowing tracer and a muzzle flash light.
  - **Desktop controls:** 1–0 or the mouse wheel switch weapon, X holsters, left mouse fires or slashes, right mouse aims or heavy-stabs, T reloads, and Q/E fire the hooks. With the weapon holstered, the mouse buttons fire the hooks again.
- **Verified on the PC.** It compiled first time, the planets and weapons imported, and in Play the Kuronami rifle, marksman and blade showed in hand and the rifle fired. Vael and its ring hang over the route.
- **Found while testing.** Every frame of Play logs "InverseFast: non-invertible matrix". The 10 Nov evening log has it too, so it predates this work. The likely cause is a vine or rope piece of zero size; to investigate.
- **Licensing.** The weapons are fan models of Riot Games designs. They stay on Naomi's PC (`Art/ThirdParty/Weapons`, `Content/ThirdParty` are gitignored) and are fine for a private prototype, but can't ship in a release.

## 11 Nov 2026 (evening): Art direction changed to "sepia haze"
- **Brief from Naomi.** A fantasy setting in one warm, hazy, almost colourless palette: peach, amber and umber light through thick dusty mist. Ruins read as dark silhouettes against a bright haze, with a low sun disc. Planets should sit far off in the distance, not loom over you.
- **What changed:**
  - **Day/night looks** (`GRDayNight`): all four keys are now in one warm family. Night is a dark umber, not blue. The sun stays lower (32° at noon), so the light is long and hazy all day.
  - **Grade** (`color_grade.py`, new mood `haze`):
    - saturation drops to 0.4 and an overall warm gain is added, so pinks and greens turn into dusty beige and brown;
    - thicker fog that starts at your feet and reaches higher, with more volumetric extinction, so distant islands become silhouettes;
    - a fine film grain, a softer bloom, and warm, dimmer orb and ruin lights.
  - **Planets** (`GRPlanets`): about 2–2.5× smaller and lower toward the horizon. They're blended toward the current fog colour (60% by day, 30% at night), so they read as far away behind the mist.
  - **Sun disc:** the planets actor now draws a small bright disc where the sunlight comes from. The bloom makes its halo, and at night it becomes a pale moon.
- **First try came out lavender-grey.** Desaturating alone let the pale sky dome dominate. An overall warm colour gain fixed it.
- **Still to do for this look:**
  - wet, reflective stone underfoot;
  - a magenta glow near one floating rock, which isn't from our lights (0 recoloured); probably a light inside a Brushify island, to find;
  - the fog may need tuning so grapple rocks 20 m away stay readable.

## 11 Nov 2026 (night): Art direction changed again to "living Pandora", and haptic glove bridge
- **Naomi's correction.** Sepia wasn't what she meant. What she wants is realism or hyper-realism graphics with a semi-realistic style, plants that feel green, and a world that feels alive and real but fantastical, like Avatar (both the films and the animated series).
- **What changed:**
  - **Day/night keys** (`GRDayNight`): clean daylight, humid blue-green distance, golden dawn and dusk, and a deep teal night.
  - **Bioluminescence:** a new `MPC_World.Night` value (driven by `GRDayNight`) makes the glowing orbs faint by day and bright at night.
  - **Grade:** a new `color_grade.py` mood, "pandora".
- **Problem found.** The Brushify floating islands' rock and hanging-moss materials are red. Raising saturation made the whole scene read red ("wayyy too much red"). The grade now mutes the red channel and lifts green.
  - Proper fix: our own material instances for the islands, plus photoreal plants from Fab (Quixel Megaplants, free).
- **Haptic gloves** (`GRHandFeel`):
  - Fingertip positions come from OpenXR hand tracking (`GEngine->XRSystem->GetHandTrackingState`).
  - A 0.9 cm touch sphere sits on each tip.
  - When a tip touches something, the code sends force feedback to LucidGloves through the OpenGloves pipe `\\.\pipe\vrapplication\ffb\curl\left|right`: five int16 values, 0–1000, thumb to pinky.
  - Plants, vines and water are soft (25%); rock and ruins are firm (100%).
  - A small controller tick plays on first contact, and there is a hook for the EXIT SUIT.
  - **Build fixes in 5.8:**
    - `HeadMountedDisplayFunctionLibrary.h` isn't available, so the code uses `IXRTrackingSystem`.
    - `FOverlapResult` needs `Engine/OverlapResult.h`.

## 12 Nov 2026 (early hours): Three weapons with hands that reload, a living atmosphere, and the plant hunt
- **Weapons cut to three** (Naomi's call): 1 knife, 2 rifle, 3 sniper. Switching is now a quick eased dip (16 cm down, a small roll) instead of a big 50° flip.
- **Hands:** the VR template's hand models are drawn on the weapon, right hand on the grip and left on the foregrip.
  - **Desktop reload:** the left hand grabs the magazine, drops to the belt, comes back with a fresh one and slaps it in.
  - **Sniper:** bolt action; the left hand works the bolt after every shot.
- **VR manual reload** (ready for the headset):
  - grip near the magazine and pull it out;
  - let go to drop it;
  - grip at the left hip for a fresh one;
  - bring it to the magwell and it clicks in;
  - grab the sniper bolt and pull it back.
  - Haptic clicks on each step. Grip state comes from the rig (`IsGripping`).
- **`GRAmbientLife`:** 350 tiny specks drift with the wind around the player as one instanced mesh. By day they're pollen and dust; at night they turn into glowing, twinkling spores (reads the day/night clock).
- **Plant hunt on Fab** (Megascans became free on 6 Nov 2026). Shortlisted photoscanned packs: Project Nature tropical and fern sets with wind and touch interaction, Megascans palms and ferns, Megaplants bamboo, plus a matching sci-fi weapon set.
  - Blocked on signing in to Fab in the agent's browser. Claude doesn't type passwords, and the launcher's Fab search box didn't accept automated typing.
- **Play started at dawn,** whose warm light turned the island rock red. The match now starts mid-morning (`StartAt` 0.18), and the dawn light is less orange.

## 12 Nov 2026 (morning): animated rifle arms, real plants, the asset shortlist
- **Rifle arms:** the First Person Rifle Animations pack (Fab) now drives the rifle: female arms and half gloves running the pack's own `FirstPerson_AnimBP`, the rigged `SKM_Weapon_Full` on `hand_r` running `ABP_Weapon_Rifle`, and draw / fire / reload montages played on top, like the pack's demo character. Exact offsets were read from that demo (`Scripts/tools/inspect_demo_character.py`); live tuning with `gr.Arms` / `gr.Gun`.
- **Plants:** `Scripts/swap_plants.py` replaced 8,709 old plants with Project Nature photoscans (tropical ground plants, fern, banana, pandanus), in four 120 m zones, with 680 of them glowing at night. The editor warned about memory afterwards (8,709 separate actors; to convert to instanced foliage later).
- **Naomi's verdict:** still too bare and too low-poly. Main cause: the project uses forward shading (VR template default), which switches Lumen off. Switching to deferred + Lumen Lite + Nanite is planned (needs her go-ahead; known 5.8 bug with instanced stereo + deferred).
- **Asset shortlist** (Claude Sheets, "Grapple Runner asset shortlist"): 30 Fab assets checked for price, licence, UE version and whether Naomi owns them. Free picks: Megascans cliffs, rocks and roots for the island undersides, Goddess Temple and Dark Ruins samples, scanned tropical plants and vines, Megaplants Ginkgo and Cherry, a free sci-fi sniper with separate parts. Best paid pick (not bought): Scifi Jungle Biome.

## 12 Nov 2026 (day): twin ODM blades that really cut, one pair of hands, and the game's objective
- **Naomi's brief:** "fix the knives as weapons... the 2 swords", hands gripping them properly, slash pretty much everything, trees take several hacks; the hands kept clipping with Kwang, there were two pairs of hands, and Kwang always held his weapon.
- **Our own sword model** (`Art/Weapons/make_odm_swords.py` → `SM_ODMSword.fbx`): trigger handle, guard, finger loop, 95 cm snap-segment blade with a honed edge. Three sockets (Tip, Base, Edge) tell the game which way the blade points and which side is sharp, however Unreal turns the model on import. Import: `Scripts/import_odm_swords.py`.
- **Twin blades** (`GRArsenal`, key 1, now the start weapon): left click = slashes that alternate right/left (wind up out to the side, sweep across, follow through); right click = a two-handed X cross-cut. Each move is a few key poses blended with quaternion slerp.
- **The grip is fitted, not guessed** (`FitHandToBlade`): in the grasp pose each finger's three joints sit on a circle round the hollow of the fist. The circle's centre (the "circumcentre") for each of the four fingers gives the line the grip must lie on; the middle knuckles say which way the edge faces. The hand is moved so that line sits on the sword's grip. The left hand is the right hand model mirrored (scale -1), because the template's grasp animation only curls right-hand fingers: that's why the old left hand looked open.
- **Cutting** (`GRCutter`, new): on the first frame it lists every static object that could be cut in an 8 m grid (plants have no collision, so physics can't see them). Each frame of a swing, the blade's path is checked against the nearby ones. Plants and props: 1 hit. Boulders and statues: 2. Trees and pillars: 2 to 5 hacks by height, and only the trunk counts. Islands, cliffs and big walls can't be cut. A cut copies the model into a procedural mesh, hides the original, slices it along the blade's plane (with caps) and lets the top fall with physics, pushed the way you swung. Pieces disappear after 30 s. Chips fly (green for plants, wood for trees, sparks for stone).
- **One pair of hands:** Kwang's arms, sword and head are hidden in first person (`HiddenBodyBones`). In VR the template's hand models step aside while the blades are out.
- **Game objective** (Claude Doc "Grapple Runner: game objective"): keep the run alive as long as you can; score = metres × flow multiplier; gas is the pressure and slicing pods refills it; no kill quotas. Based on six case studies (Attack on Titan VR: Unbreakable, Swarm, Windlands 2, Sprint Vector, Subway Surfers, Gorilla Tag) and research on motivation, session length, comfort and haptics.
- **Not built or tested yet:** computer control was stopped, so the code is on the PC but not compiled. The build steps are in NOW.md.

## 12 Nov 2026 (afternoon): built, tested, deferred + Lumen on, islands dressed with Megascans
- **Built:** one compile error (a local variable named `Owner` hid `AActor::Owner`, warning C4458 treated as an error); renamed to `Holder`. Everything else compiled first time.
- **Blades tested in Play:** draw, ready stance, alternating slashes and cut plants all work ("cuts 1" in the status line; the Message Log shows the copy-to-procedural-mesh step for banana and tropical plants). The ready stance was too low (hands off screen), so it's raised to (40, 21, -17). The hands sat low on the grips: `GripSlide` default is now 4 cm.
- **Testing aids:** F9 toggles 10% slow motion; the status line shows swing time and number of cuts; V = slash and C = cross-cut on the keyboard. (Automated clicks from the agent's computer control are often too short for Unreal to see; a real mouse is fine.)
- **Rendering switched** (Naomi's go-ahead): `r.ForwardShading=False` (deferred, so Lumen GI and reflections work), `vr.InstancedStereo=False` (5.8 bug with deferred), anti-aliasing TSR. About 2,200 shaders recompiled on first open.
- **Lumen made the island undersides pitch black**, because no sky light reaches under them. `Scripts/tools/lighting_fix.py` sets Lumen skylight leaking to 0.12 on the post-process volume.
- **Free Megascans via the Fab panel in the editor** (Naomi OK'd accepting the Fab licence for free assets): Huge Nordic Coastal Cliff, Granite Cliff, Mossy Rock Formation (high quality), Mossy Forest Roots.
- **`Scripts/dress_megascans.py`** on all 47 landing islands: 221 scanned cliffs hung upside down under the islands, 271 rocks round the rims, 1,474 more plants in clumps (Project Nature photoscans), 102 root sprawls.
- **Disk:** Unreal warned that the C: drive has under 5 GB free. Stopped downloading; high-quality Megascans use 4K textures.
- **Tropical plants out** (Naomi: likes the bushes, not the tropical plants). `Scripts/replace_tropical.py` turned all 6,141 banana, pandanus and big-leaf tropical plants into Project Nature's bushy photoscanned ferns of a similar size, kept their night glow, and added 524 extra ferns where a tall banana or pandanus left a gap. The island dressing now uses ferns only. List of swaps: `Saved/Tests/tropical_swap.csv`.
- **Video memory:** Play showed "video memory has been exhausted". The 4K Megascans textures and about 10,000 separate plant actors are the likely cause; to fix with texture size caps and instanced foliage.
