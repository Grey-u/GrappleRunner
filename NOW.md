# NOW (updated 8 Nov 2026)

## Works
- UE 5.8.4 project (VR / OpenXR template) with Brushify Floating Islands.
- Procedural level `L_FloatingIslands_2`: 14 landing islands, 53 `GrappleAnchor` rocks, backdrop, sky dome, mist, waterfalls.
- Weapon kit v1 imported: blade, handle, hip grapple gear, sidearm.
- `GrappleRig` C++ written: twin hooks, rope constraint, reel, gas, VR and desktop input.

- C++ module compiles cleanly on UE 5.8: 0 errors, 0 warnings, 89 s (test T-01 in `Docs/TESTS.md`).
- Scientific test log started: `Docs/TESTS.md`, evidence in `Docs/media/tests/`.

## Half done
- Grapple rig not yet placed in the level (place_grapple_rig.py hasn't run yet).

## Decided
- First person is the main view. Optional third person comes later with no advantage: see "Camera rule" in CLAUDE.md.

## Next
1. Run `Scripts/place_grapple_rig.py`, press Play, and test-swing with the mouse.
2. Record the swing tests (T-02 onward) with screenshots and recordings.
3. Tune the feel. Add the speed vignette (comfort rule).

## Tried and dropped
- Physical Sky Atmosphere: it renders black below the horizon in this forward-shaded VR setup, so we use Brushify's sky dome instead.
