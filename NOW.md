# NOW (updated 8 Nov 2026, evening)

## Works
- UE 5.8.4 project (VR / OpenXR template) with Brushify Floating Islands.
- Procedural level `L_FloatingIslands_2`: 14 landing islands, 53 `GrappleAnchor` rocks, backdrop, sky dome, mist, waterfalls.
- Weapon kit v1 imported: blade, handle, hip grapple gear, sidearm.
- `GrappleRig` C++ compiles (T-01) and is placed in the level. On desktop you can:
  - stand on islands
  - aim (the aim-assist dots work)
  - fire and hold a hook, reel in, swing and release
  - fall, and respawn safely.
- Telemetry: every play session saves a CSV to `Saved/Tests/`. Analyse it with `python Tools/analyse_telemetry.py <csv>`.
- Test log `Docs/TESTS.md`: T-01 to T-08 written up with charts, screenshots and raw data.

## Half done
- None.

## Decided
- First person is the main view. Optional third person comes later with no advantage: see "Camera rule" in CLAUDE.md.
- Goal for comfort: reduce motion sickness and help players know their limits. This will be measured with the Simulator Sickness Questionnaire once the headset is back (plan at the end of TESTS.md).

## Next
1. Day 2, feel tuning: soften the rope snap (finding F1). Re-run the analysis and compare peak deceleration before and after.
2. Decide the hook range (F2: 60 m in code, 18 m in the brief). Give the rope an orange material (F3).
3. Telemetry: record any hit during a frame, not just the last physics step (F4).
4. Day 3: speed vignette (comfort rule).

## Tried and dropped
- Physical Sky Atmosphere: it renders black below the horizon in this forward-shaded VR setup, so we use Brushify's sky dome instead.
- First guess for the fall-through bug ("islands have no collision") was wrong: disproved by `diagnose_collision.py`. The cause was the solid sky dome.
