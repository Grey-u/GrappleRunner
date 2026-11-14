# =====================================================================================
#  make_leaf_texture.py  -  paints the leaf "brush stroke" texture for the anime trees
# =====================================================================================
#  python Scripts/make_leaf_texture.py  ->  Art/Stylized/Textures/T_GR_LeafBrush.png
#  A 2048 x 2048 picture split into 4 squares (2 x 2), each a different clump of leaves made
#  of painted dabs: little rounded leaf shapes, a few long streaks, ragged edges. The colour is
#  grey-white with gentle variation (the game tints it lime / teal); the ALPHA (see-through
#  part) is the leaf shape, so each flat card in the tree looks like a painted leaf clump.
# =====================================================================================
import numpy as np, os, math, random
from PIL import Image, ImageDraw, ImageFilter

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Art", "Stylized", "Textures")
os.makedirs(OUT, exist_ok=True)
SIZE = 2048
Q = SIZE // 2
R = random.Random(3)

def leaf_poly(cx, cy, length, width, angle):
    """A pointed leaf outline (like an almond with a tip)."""
    pts = []
    for i in range(24):
        t = i / 23.0 * 2 * math.pi
        x = math.cos(t) * length
        y = math.sin(t) * width * (1.0 - 0.35 * math.cos(t))   # pointier at one end
        ca, sa = math.cos(angle), math.sin(angle)
        pts.append((cx + x * ca - y * sa, cy + x * sa + y * ca))
    return pts

alpha = Image.new("L", (SIZE, SIZE), 0)
shade = Image.new("L", (SIZE, SIZE), 0)
da, ds = ImageDraw.Draw(alpha), ImageDraw.Draw(shade)
for qy in range(2):
    for qx in range(2):
        ox, oy = qx * Q, qy * Q
        cx, cy = ox + Q / 2, oy + Q / 2
        n = 150 + 25 * (qx + qy)
        for i in range(n):
            # dabs fill a rounded clump, denser in the middle, ragged at the edge
            a = R.uniform(0, 2 * math.pi)
            r = (Q * 0.36) * R.random() ** 0.6
            x = cx + math.cos(a) * r
            y = cy + math.sin(a) * r * 0.9
            ang = a + R.uniform(-0.9, 0.9)                      # leaves point outward-ish
            ln = R.uniform(70, 125) * (1.3 - 0.55 * r / (Q * 0.42))
            wd = ln * R.uniform(0.45, 0.75)
            v = int(170 + 85 * (1 - r / (Q * 0.42)) * R.uniform(0.6, 1.0))   # brighter in the middle
            poly = leaf_poly(x, y, ln, wd, ang)
            da.polygon(poly, fill=255)
            ds.polygon(poly, fill=max(0, min(255, v)))
        # a few long brush streaks, like the reference's flicks
        for i in range(10):
            a = R.uniform(0, 2 * math.pi); r = R.uniform(0.15, 0.4) * Q
            x0, y0 = cx + math.cos(a) * r, cy + math.sin(a) * r
            ln = R.uniform(80, 160); ang = a + R.uniform(-0.4, 0.4)
            pts = [(x0 + math.cos(ang) * t * ln, y0 + math.sin(ang) * t * ln + math.sin(t * 3) * 4) for t in np.linspace(0, 1, 8)]
            da.line(pts, fill=255, width=R.randint(7, 12)); ds.line(pts, fill=210, width=R.randint(7, 12))
# soften very slightly so mip-maps don't sparkle, then make the edge crisp again
alpha = alpha.filter(ImageFilter.GaussianBlur(1.2)).point(lambda p: 255 if p > 110 else 0)
shade = shade.filter(ImageFilter.GaussianBlur(2.0))
rgb = Image.merge("RGB", (shade, shade, shade))
img = Image.merge("RGBA", (*rgb.split(), alpha))
img.save(os.path.join(OUT, "T_GR_LeafBrush.png"))
print("wrote T_GR_LeafBrush.png")
