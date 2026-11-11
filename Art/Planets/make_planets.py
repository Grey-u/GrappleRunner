"""
Grapple Runner: paint the planets that hang in Aeloria's sky (all procedural, all ours).

WHAT THIS MAKES (in this folder, as PNG images)
  T_Planet_Vael.png      a big banded gas giant in Arcadium orange and cream
  T_Planet_Vael_Ring.png its ring: a see-through band, drawn on a flat square
  T_Planet_Lun.png       a pale cratered moon
  T_Planet_Iskra.png     a small violet-blue ice world with white swirls
  T_Planet_Merrow.png    a far-off teal ocean world with cloud streaks

HOW THE IMAGES WORK
A planet picture is wrapped round a ball, like a world map round a globe: left-to-right is
all the way round (longitude), top-to-bottom is pole to pole (latitude). So the images are
twice as wide as they are tall. The patterns come from "noise": random numbers smoothed
at several sizes and added up, which looks like clouds, storms and rock.

HOW TO RUN
  python3 make_planets.py            (needs numpy and Pillow)
Then Scripts/add_planets.py imports them into Unreal, and GRPlanets.cpp puts them in the sky.

HOW TO READ THIS (no coding experience needed)
- Lines starting with # are notes for humans; the computer skips them.
- "def name(...):" is a reusable recipe; the indented lines under it are its steps.
- numpy ("np") works on whole images at once: one line changes every pixel.
"""
import os

import numpy as np
from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
W, H = 1024, 512
rng = np.random.default_rng(7)   # fixed seed: the same planets every time you run it


def noise(w, h, cells, wrap=True):
    """Smooth random pattern: a coarse grid of random numbers, blown up smoothly to w x h."""
    grid = rng.random((cells[1] + 1, cells[0] + 1))
    if wrap:
        grid[:, -1] = grid[:, 0]   # left and right edges match, so there's no seam round the back
    img = Image.fromarray((grid * 255).astype(np.uint8)).resize((w, h), Image.BICUBIC)
    return np.asarray(img, dtype=np.float32) / 255.0


def fractal(w, h, base=4, octaves=5):
    """Several noises added together, each twice as fine and half as strong (clouds, rock)."""
    out = np.zeros((h, w), np.float32)
    amp, total = 1.0, 0.0
    for o in range(octaves):
        c = base * 2 ** o
        out += amp * noise(w, h, (c * 2, c))
        total += amp
        amp *= 0.5
    return out / total


def palette(t, stops):
    """Turn numbers 0..1 into colours by blending between colour stops [(position, (r,g,b)), ...]."""
    t = np.clip(t, 0, 1)
    out = np.zeros(t.shape + (3,), np.float32)
    for (p0, c0), (p1, c1) in zip(stops[:-1], stops[1:]):
        m = (t >= p0) & (t <= p1)
        a = ((t - p0) / max(1e-6, p1 - p0))[m][:, None]
        out[m] = np.array(c0) * (1 - a) + np.array(c1) * a
    return out


def save(name, rgb, alpha=None):
    arr = np.clip(rgb * 255, 0, 255).astype(np.uint8)
    if alpha is not None:
        arr = np.dstack([arr, np.clip(alpha * 255, 0, 255).astype(np.uint8)])
    Image.fromarray(arr).save(os.path.join(HERE, name))
    print("wrote", name)


lat = np.linspace(-1, 1, H)[:, None] * np.ones((1, W))   # -1 = south pole ... +1 = north pole

# ---- Vael: gas giant. Bands = a wave in latitude, bent sideways by noise (the storms). ----
warp = fractal(W, H, base=3, octaves=4)
bands = 0.5 + 0.5 * np.sin(lat * 14.0 + (warp - 0.5) * 5.0)
bands = 0.65 * bands + 0.35 * fractal(W, H, base=8, octaves=3)
vael = palette(bands, [(0.0, (0.35, 0.12, 0.05)), (0.35, (0.94, 0.31, 0.14)), (0.6, (1.0, 0.72, 0.35)),
                       (0.8, (0.98, 0.9, 0.75)), (1.0, (0.8, 0.55, 0.4))])
# one great storm: a pale oval
yy, xx = np.mgrid[0:H, 0:W]
storm = np.exp(-(((xx - 640) / 70.0) ** 2 + ((yy - 300) / 26.0) ** 2))
vael = vael * (1 - storm[..., None] * 0.6) + np.array((1.0, 0.92, 0.82)) * storm[..., None] * 0.6
save("T_Planet_Vael.png", vael)

# ---- Vael's ring: seen on a square, transparent outside a band between two circles ----
R = 1024
yy, xx = np.mgrid[0:R, 0:R]
r = np.hypot(xx - R / 2, yy - R / 2) / (R / 2)          # 0 at the centre, 1 at the edge
fine = 0.5 + 0.5 * np.sin(r * 180.0) * np.sin(r * 47.0)  # many thin ringlets
band = np.clip((r - 0.55) / 0.04, 0, 1) * np.clip((0.97 - r) / 0.04, 0, 1)
gap = 1 - 0.85 * np.exp(-((r - 0.78) / 0.012) ** 2)      # a dark gap in the ring
ring_a = band * gap * (0.35 + 0.65 * fine)
ring_rgb = palette(r, [(0.0, (1, 0.8, 0.6)), (0.6, (0.95, 0.75, 0.55)), (0.8, (0.85, 0.6, 0.45)), (1.0, (1, 0.85, 0.7))])
save("T_Planet_Vael_Ring.png", ring_rgb, ring_a * 0.85)

# ---- Lun: moon. Grey rock + craters (dark floors, bright rims). ----
rock = fractal(W, H, base=6, octaves=6)
lun = palette(rock, [(0.0, (0.35, 0.34, 0.36)), (0.5, (0.62, 0.6, 0.6)), (1.0, (0.85, 0.83, 0.8))])
shade = np.zeros((H, W), np.float32)
for _ in range(90):
    cx, cy, cr = rng.uniform(0, W), rng.uniform(H * 0.1, H * 0.9), rng.uniform(4, 34)
    d = np.hypot(xx[:H, :W] - cx, yy[:H, :W] - cy) / cr   # distance from the crater's middle, in crater radii
    shade -= 0.25 * np.clip(1 - d, 0, 1) ** 0.5                      # dark bowl
    shade += 0.18 * np.exp(-((d - 1.0) / 0.12) ** 2)                 # bright rim
lun = lun * (1 + shade[..., None])
save("T_Planet_Lun.png", lun)

# ---- Iskra: ice world. Violet-blue base, white swirls pulled along by latitude. ----
swirl = fractal(W, H, base=5, octaves=5)
streak = noise(W, H, (6, 48))   # few cells across, many down = long sideways streaks
ice = palette(0.6 * swirl + 0.4 * streak, [(0.0, (0.22, 0.2, 0.55)), (0.45, (0.42, 0.45, 0.9)),
                                           (0.7, (0.75, 0.82, 1.0)), (1.0, (1.0, 1.0, 1.0))])
cap = np.clip((np.abs(lat) - 0.75) / 0.15, 0, 1)[..., None]       # white polar caps
save("T_Planet_Iskra.png", ice * (1 - cap) + cap)

# ---- Merrow: ocean world. Teal sea, a few green islands, long cloud streaks. ----
land = fractal(W, H, base=4, octaves=6)
sea = palette(land, [(0.0, (0.02, 0.18, 0.28)), (0.55, (0.05, 0.45, 0.5)), (0.6, (0.3, 0.55, 0.3)), (1.0, (0.5, 0.6, 0.35))])
cloud = np.clip((noise(W, H, (8, 36)) * 0.6 + fractal(W, H, base=8, octaves=3) * 0.4 - 0.52) * 4, 0, 1)[..., None]
save("T_Planet_Merrow.png", sea * (1 - cloud * 0.85) + cloud * 0.85)
