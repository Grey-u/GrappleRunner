# NOW (updated 8 Nov 2026)

## Works
- UE 5.8.4 project (VR / OpenXR template) with Brushify Floating Islands.
- Procedural level `L_FloatingIslands_2`: 14 landing islands, 53 `GrappleAnchor` rocks, backdrop, sky dome, mist, waterfalls.
- Weapon kit v1 imported: blade, handle, hip grapple gear, sidearm.
- `GrappleRig` C++ written: twin hooks, rope constraint, reel, gas, VR and desktop input.

## Half done
- C++ module can't compile until the MSVC v14.42 build tools are installed (UE 5.8 bans 14.44).

## Next
1. Install MSVC 14.42, reopen the project and accept the rebuild. Fix any compile errors.
2. Run `Scripts/place_grapple_rig.py`, press Play, and test-swing with the mouse.
3. Tune the feel. Add the speed vignette (comfort rule).

## Tried and dropped
- Physical Sky Atmosphere: it renders black below the horizon in this forward-shaded VR setup, so we use Brushify's sky dome instead.
