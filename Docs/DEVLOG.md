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
