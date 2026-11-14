"""
Grapple Runner: icons for the floating menu (our own drawings, so we own them fully).

The menu is inspired by the Sword Art Online style: round white buttons that turn amber when
picked, thin grey panels, a body outline with gear slots round it. These icons are drawn here
from simple shapes (circles, rectangles, lines), in white on a see-through background, so the
game can tint them grey (not picked) or white on amber (picked).

Run:  python make_menu_icons.py <out_dir>
Writes T_Icon_*.png (256 x 256) and T_BodyOutline.png (512 x 1024).

HOW TO READ THIS (no coding experience needed)
- Lines starting with # are notes for humans; the computer skips them.
- "def name(...):" defines a recipe; the indented lines under it are its steps.
- Every icon is drawn 4 times bigger and then shrunk, which smooths the edges (anti-aliasing).
- Coordinates: (0, 0) is the top-left corner; x goes right, y goes down.
"""
import math
import os
import sys

from PIL import Image, ImageDraw

OUT = sys.argv[1] if len(sys.argv) > 1 else "."
os.makedirs(OUT, exist_ok=True)
S = 4                       # draw 4x bigger, then shrink: smooth edges
N = 256 * S                 # canvas size while drawing
W = (255, 255, 255, 255)    # white, fully solid


def canvas(w=N, h=N):
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    return img, ImageDraw.Draw(img)


def save(img, name, size=(256, 256)):
    img.resize(size, Image.LANCZOS).save(os.path.join(OUT, name))
    print("wrote", name)


def p(x):
    """A position given as a fraction of the icon (0..1) -> pixels on the big canvas."""
    return x * N


def person(d, cx, cy, s):
    """A head and shoulders: the 'player' symbol. cx, cy = centre, s = size (fractions)."""
    r = 0.16 * s
    d.ellipse([p(cx - r), p(cy - 0.30 * s - r), p(cx + r), p(cy - 0.30 * s + r)], fill=W)
    d.rounded_rectangle([p(cx - 0.30 * s), p(cy - 0.06 * s), p(cx + 0.30 * s), p(cy + 0.36 * s)],
                        radius=p(0.16 * s), fill=W)


def icon_player():
    img, d = canvas()
    person(d, 0.5, 0.52, 1.0)
    save(img, "T_Icon_Player.png")


def icon_party():
    img, d = canvas()
    person(d, 0.37, 0.55, 0.78)
    person(d, 0.63, 0.55, 0.78)
    save(img, "T_Icon_Party.png")


def icon_items():
    """A satchel: rounded body, flap, handle."""
    img, d = canvas()
    d.rounded_rectangle([p(0.22), p(0.36), p(0.78), p(0.80)], radius=p(0.07), fill=W)
    d.arc([p(0.36), p(0.20), p(0.64), p(0.48)], 180, 360, fill=W, width=int(p(0.05)))
    d.rectangle([p(0.22), p(0.50), p(0.78), p(0.53)], fill=(0, 0, 0, 0))     # the flap's edge
    d.rounded_rectangle([p(0.45), p(0.47), p(0.55), p(0.58)], radius=p(0.02), fill=W)   # buckle
    save(img, "T_Icon_Items.png")


def blade(d, angle_deg, cx, cy, length, width):
    """One sword: a long thin blade, a guard bar and a grip, turned by angle_deg."""
    a = math.radians(angle_deg)
    ux, uy = math.cos(a), math.sin(a)        # along the blade
    vx, vy = -uy, ux                         # across the blade

    def pt(along, across):
        return (p(cx + ux * along + vx * across), p(cy + uy * along + vy * across))

    L, w = length, width
    d.polygon([pt(-0.05, -w), pt(L * 0.62, -w), pt(L * 0.72, 0), pt(L * 0.62, w), pt(-0.05, w)], fill=W)   # blade
    d.polygon([pt(-0.07, -w * 3.2), pt(-0.03, -w * 3.2), pt(-0.03, w * 3.2), pt(-0.07, w * 3.2)], fill=W)   # guard
    d.polygon([pt(-0.07, -w * 0.9), pt(-0.24, -w * 0.9), pt(-0.24, w * 0.9), pt(-0.07, w * 0.9)], fill=W)   # grip


def icon_skills():
    img, d = canvas()
    blade(d, -45, 0.36, 0.64, 0.58, 0.035)
    blade(d, -135, 0.64, 0.64, 0.58, 0.035)
    save(img, "T_Icon_Skills.png")


def icon_equipment():
    """A shield with a single sword."""
    img, d = canvas()
    d.polygon([(p(0.30), p(0.22)), (p(0.70), p(0.22)), (p(0.70), p(0.52)), (p(0.50), p(0.82)), (p(0.30), p(0.52))], fill=W)
    d.polygon([(p(0.36), p(0.28)), (p(0.64), p(0.28)), (p(0.64), p(0.50)), (p(0.50), p(0.73)), (p(0.36), p(0.50))], fill=(0, 0, 0, 0))
    blade(d, -90, 0.5, 0.80, 0.62, 0.03)
    save(img, "T_Icon_Equipment.png")


def icon_map():
    """A map pin."""
    img, d = canvas()
    d.ellipse([p(0.30), p(0.16), p(0.70), p(0.56)], fill=W)
    d.polygon([(p(0.33), p(0.45)), (p(0.67), p(0.45)), (p(0.50), p(0.84))], fill=W)
    d.ellipse([p(0.42), p(0.28), p(0.58), p(0.44)], fill=(0, 0, 0, 0))
    save(img, "T_Icon_Map.png")


def icon_settings():
    """A gear: a ring with eight teeth."""
    img, d = canvas()
    for k in range(8):
        a = math.radians(k * 45)
        cx, cy = 0.5 + 0.27 * math.cos(a), 0.5 + 0.27 * math.sin(a)
        corners = []
        for du, dv in ((-0.07, -0.06), (0.07, -0.06), (0.07, 0.06), (-0.07, 0.06)):
            corners.append((p(cx + du * math.cos(a) - dv * math.sin(a)), p(cy + du * math.sin(a) + dv * math.cos(a))))
        d.polygon(corners, fill=W)
    d.ellipse([p(0.24), p(0.24), p(0.76), p(0.76)], fill=W)
    d.ellipse([p(0.39), p(0.39), p(0.61), p(0.61)], fill=(0, 0, 0, 0))
    save(img, "T_Icon_Settings.png")


def icon_logout():
    """A door frame with an arrow leaving it."""
    img, d = canvas()
    lw = int(p(0.05))
    d.line([(p(0.46), p(0.22)), (p(0.24), p(0.22)), (p(0.24), p(0.78)), (p(0.46), p(0.78))], fill=W, width=lw, joint="curve")
    d.line([(p(0.38), p(0.50)), (p(0.76), p(0.50))], fill=W, width=lw)
    d.polygon([(p(0.80), p(0.50)), (p(0.64), p(0.38)), (p(0.64), p(0.62))], fill=W)
    save(img, "T_Icon_Logout.png")


def icon_quest():
    """A scroll: a sheet with rolled ends and three lines of writing."""
    img, d = canvas()
    d.rounded_rectangle([p(0.28), p(0.20), p(0.72), p(0.80)], radius=p(0.04), fill=W)
    for y in (0.36, 0.48, 0.60):
        d.rectangle([p(0.36), p(y), p(0.64), p(y + 0.04)], fill=(0, 0, 0, 0))
    d.rounded_rectangle([p(0.22), p(0.16), p(0.78), p(0.24)], radius=p(0.04), fill=W)
    d.rounded_rectangle([p(0.22), p(0.76), p(0.78), p(0.84)], radius=p(0.04), fill=W)
    save(img, "T_Icon_Quest.png")


def body_outline():
    """The figure in the gear panel: a simple standing person, soft grey, made of rounded shapes."""
    w, h = 512 * S, 1024 * S
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    g = (255, 255, 255, 255)

    def r(x0, y0, x1, y1, rad):
        d.rounded_rectangle([x0 * w, y0 * h, x1 * w, y1 * h], radius=rad * w, fill=g)

    d.ellipse([0.40 * w, 0.03 * h, 0.60 * w, 0.13 * h], fill=g)        # head
    r(0.46, 0.12, 0.54, 0.16, 0.02)                                     # neck
    r(0.30, 0.155, 0.70, 0.47, 0.08)                                    # torso
    r(0.17, 0.17, 0.29, 0.48, 0.05)                                     # left arm
    r(0.71, 0.17, 0.83, 0.48, 0.05)                                     # right arm
    r(0.16, 0.47, 0.29, 0.54, 0.05)                                     # hands
    r(0.71, 0.47, 0.84, 0.54, 0.05)
    r(0.31, 0.46, 0.49, 0.94, 0.07)                                     # legs
    r(0.51, 0.46, 0.69, 0.94, 0.07)
    r(0.28, 0.92, 0.49, 0.97, 0.03)                                     # feet
    r(0.51, 0.92, 0.72, 0.97, 0.03)
    img.resize((512, 1024), Image.LANCZOS).save(os.path.join(OUT, "T_BodyOutline.png"))
    print("wrote T_BodyOutline.png")


for f in (icon_player, icon_party, icon_items, icon_skills, icon_equipment, icon_map, icon_settings,
          icon_logout, icon_quest, body_outline):
    f()
