# Grapple Runner VR: project brief for Claude

Read NOW.md first, every session.

## What this is
- A VR endless runner built around Attack on Titan-style grapple swinging. Single-player first; multiplayer comes later.
- Engine: Unreal Engine 5, C++ project started from the Virtual Reality template.
- Setting: Aeloria, a floating city of spires and platforms.
- Headset: [fill in, e.g. Quest 3 via Link cable]
- PC / graphics card: [fill in]

## About me
- I'm a beginner in Unreal. I learn best from working examples I can change, so explain *why*, not just *what*.
- For anything done in the editor (Blueprints, menus, settings), give numbered click-by-click steps and tell me what I should see after each one.
- Keep tasks small: one change, then I test it in the headset.
- I have ADHD and perfectionism. Stop me if I start polishing something that isn't on the current version.

## Rules
- Shipped beats perfect. Build the smallest version that works first.
- Don't start anything on the "Later" list unless I ask.
- C++ vs Blueprints: gameplay logic goes in C++ that you write and explain. Expose tuning values (rope length, reel speed, gravity) as UPROPERTY(EditAnywhere) so I can tweak them in the editor without code.
- Unreal units are centimetres: 1 m = 100 units.
- Never delete or overwrite anything in Content/ without asking. Remind me to commit before big changes.

- **Comment every code file for a complete beginner.** Each file opens with a plain-English "what this is / how to read it" block, and every step gets a comment saying what it does and why. Update the comments whenever the code changes.
- **Log all code to GitHub** (github.com/Grey-u/GrappleRunner) and add an entry to Docs/DEVLOG.md for every work session (portfolio record).

## VR comfort rules
- No HUD stuck to the screen. Put information in the world: gas gauge on the hip gear, distance on the wrist, hooks glow when in range.
- Add a vignette that narrows vision at high speed to reduce motion sickness.
- Keep the frame rate at the headset's refresh rate. Performance beats graphics.

## Starting physics numbers (from the 2D test bench)
- Max rope length 18 m, reel-in speed 2 m/s, gravity 9.81 m/s^2, air drag 0.01
- Hook heights 11 to 21 m; gaps between hooks 7 to 16 m
- Player mass 70 kg (for rope tension and haptics strength)

## Versions
1. **Swing:** grey-box level, the trigger fires the grapple from the controller, swing, release.
2. **Run:** level chunks spawn ahead endlessly, distance score.
3. **Feel:** controller vibration when the hook lands, then Exit Suit and tendon-finger haptics.
- **Later:** enemies, slicing, weapons with weight, multiplayer, high-end graphics, optional third-person camera.

## Camera rule (decided 8 Nov 2026)
- First person is the main way to play. Third person will be an option, but it must give **no advantage** over first person.
- So every gameplay check (aiming, what a hook can reach, which anchors glow, what you can see or hit) is traced from the character's own eyes or hands, never from the camera. The camera only shows the picture.
- Third-person camera: short over-the-shoulder distance, same field of view as first person, and it can't pass through walls.
- Multiplayer: other players are only drawn if your character's eyes could see them, so the camera can't peek round corners.

## Session routine
1. Read NOW.md.
2. Do one task from "Next".
3. Before ending, rewrite NOW.md: what works, what's half done, what's next, and what we tried and dropped.
