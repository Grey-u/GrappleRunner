"""
Grapple Runner: offline model of the rope, used to PREDICT test results before
running them in Unreal (test T-09 in Docs/TESTS.md).

HOW TO READ THIS (no coding experience needed)
- This is a copy of the rope maths in GrappleRig.cpp (Simulate, steps 1 to 3),
  written in Python, so it can try many settings in a second without Unreal.
- Two situations are tested, each with the old stiff rope and the new soft rope:
    A. You fire a hook while already falling away from it at 17 m/s, so the rope
       snaps tight (the jolt measured in test T-04).
    B. You hang from a hook and reel in at 9 m/s.
- "g" means "times the pull of gravity". 1 g is what you feel standing still.
  A roller coaster peaks at about 4 to 5 g.
- "Felt" acceleration leaves gravity out, because your inner ear can't feel
  gravity while you're falling. It is what makes your stomach drop.
Run:  python Tools/rope_model.py
"""
import numpy as np

STEP = 1 / 120          # physics step, same as the game (seconds)
G, DRAG = 980.0, 0.06   # gravity (cm/s^2) and air drag (per second), same as the game
REEL_PULL = 1400.0      # extra tug toward the hook while reeling (ReelPullAccel), same as the game
MAX_SPEED = 4500.0      # speed limit (MaxSpeed), same as the game


def simulate(start_pos, start_vel, rope, soft, reel=0.0, k=64.0, zeta=0.5,
             max_stretch=150.0, stall=20.0, seconds=3.0, history=False, prog=0.0, swing=1.0):
    """Run the rope physics for a few seconds. The anchor is at (0, 0, 0).
    prog  = progressive spring: the rope gets stiffer as it stretches; at 'prog' cm of
            stretch it is twice as stiff, at 2x 'prog' five times (0 = off).
    swing = swing exaggeration: extra gravity along the swing direction only (1 = real).
    Returns: peak felt acceleration (g), largest stretch (cm), speed at the end (cm/s).
    With history=True it also returns the felt acceleration and stretch for every step."""
    p = np.array(start_pos, float)
    v = np.array(start_vel, float)
    length = rope
    omega = np.sqrt(k)                    # how fast the rope springs
    damping = 2 * zeta * omega            # standard spring-damper formula
    felt, stretch = [], []
    for _ in range(int(seconds / STEP)):
        v_before = v.copy()
        dist = np.linalg.norm(p)
        # STEP 1: gravity and drag
        v[2] -= G * STEP
        v -= v * DRAG * STEP
        # swing exaggeration: extra gravity, but only the part along the swing (not along the rope)
        if swing != 1.0 and dist >= length - 5.0:
            n = p / dist
            extra = np.array([0.0, 0.0, -G * (swing - 1.0)])
            v += (extra - n * np.dot(extra, n)) * STEP
        # Reeling in (with the soft rope the reel stalls once the rope is stretched 'stall' cm)
        if reel:
            length = max(250.0, length - reel * STEP)
            if soft:
                length = max(length, dist - stall)
            v += (-p / dist) * REEL_PULL * STEP          # the reel tugs you toward the hook
        # STEP 2b: soft rope pulls back like a spring + damper (pull only, never push)
        if soft and dist > length:
            n = p / dist
            s_ = dist - length
            stiff = k * (1.0 + (s_ / prog) ** 2) if prog > 0 else k   # progressive: stiffer the more it stretches
            pull = stiff * s_ + damping * np.dot(v, n)
            if pull > 0:
                v -= n * pull * STEP
        # STEP 2 + 3: obey the speed limit, move, then the hard limit
        speed = np.linalg.norm(v)
        if speed > MAX_SPEED:
            v *= MAX_SPEED / speed
        new = p + v * STEP
        d = np.linalg.norm(new)
        limit = length + (max_stretch if soft else 0.0)
        if d > limit:
            n = new / d
            new = n * limit
            out = np.dot(v, n)
            if out > 0:
                v -= n * out
        p = new
        a = (v - v_before) / STEP + np.array([0.0, 0.0, G])   # felt = total change minus gravity's part
        felt.append(np.linalg.norm(a) / G)
        stretch.append(max(0.0, np.linalg.norm(p) - length))
    if history:
        return max(felt), max(stretch), np.linalg.norm(v), np.array(felt), np.array(stretch)
    return max(felt), max(stretch), np.linalg.norm(v)


def best_settings():
    """Try a grid of stiffness and damping values for situation A and list the gentlest
    ones that stay safely inside the stretch limit (under 130 of the 150 cm)."""
    results = []
    for k in [25, 36, 49, 64, 100, 144]:
        for zeta in [0.3, 0.4, 0.5, 0.6, 0.7, 1.0]:
            a, s, _ = simulate([1200, 0, -3330], [300, 0, -1700], 3530, soft=True, k=k, zeta=zeta)
            if s < 130:
                results.append((a, k, zeta, s))
    return sorted(results)[:5]


if __name__ == "__main__":
    row = "%-36s %14s %12s %14s"
    print(row % ("", "peak felt (g)", "stretch (cm)", "end speed (m/s)"))
    print("A. Hook fired while falling away from it at 17 m/s")
    for name, kw in [("stiff rope (before)", dict(soft=False)),
                     ("soft rope k=64, damping 0.5 (new)", dict(soft=True)),
                     ("soft rope k=64, damping 0.7", dict(soft=True, zeta=0.7)),
                     ("soft rope k=36 (softer)", dict(soft=True, k=36)),
                     ("soft rope k=144 (stiffer)", dict(soft=True, k=144))]:
        a, s, v = simulate([1200, 0, -3330], [300, 0, -1700], 3530, **kw)
        print(row % ("  " + name, "%.1f" % a, "%.0f" % s, "%.1f" % (v / 100)))
    print("B. Hanging still, then reeling in at 9 m/s")
    for name, kw in [("stiff rope (before)", dict(soft=False)),
                     ("soft rope, no reel stall", dict(soft=True, stall=1e9)),
                     ("soft rope, reel stalls at 20 cm (new)", dict(soft=True))]:
        a, s, v = simulate([0, 0, -3730], [0, 0, 0], 3730, reel=900, **kw)
        print(row % ("  " + name, "%.1f" % a, "%.0f" % s, "%.1f" % (v / 100)))
    print("\nGentlest settings for A that keep the stretch under 130 cm:")
    for a, k, zeta, s in best_settings():
        print("  k=%-4d damping %.1f -> peak %.1f g, stretch %.0f cm" % (k, zeta, a, s))
