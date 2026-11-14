# =====================================================================================
#  make_stylized_meshes.py  -  builds Grapple Runner's own anime-style nature meshes in Blender
# =====================================================================================
#  Run with Blender's Python (or "pip install bpy"):  python make_stylized_meshes.py
#  Writes FBX files to Art/Stylized/. Everything here is made from scratch by this script:
#  simple shapes (bent blades, petal fans, lumpy spheres) coloured with VERTEX COLOURS, so
#  no textures are needed and the colours stay flat and bright like an anime background.
#
#  The vertex colour's ALPHA is used by the materials in Unreal as "how much this bit moves":
#    grass: 0 at the root, 1 at the tip  -> the wind sways tips, roots stay put
#    wings: 0 at the body, 1 at the wing tip -> wings flap
#  Sizes are in metres here; they arrive in Unreal in centimetres.
# =====================================================================================
import bpy, bmesh, math, random, os
from mathutils import Vector, noise

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Art", "Stylized")
os.makedirs(OUT, exist_ok=True)
R = random.Random(7)

def lerp(a, b, t): return tuple(a[i] + (b[i] - a[i]) * t for i in range(len(a)))

class Builder:
    """Collects triangles with a colour (RGBA) at every corner, then makes one mesh."""
    def __init__(self): self.v = []; self.f = []; self.c = []
    def tri(self, a, b, c, ca, cb, cc):
        i = len(self.v); self.v += [a, b, c]; self.f.append((i, i + 1, i + 2)); self.c += [ca, cb, cc]
    def quad(self, a, b, c, d, ca, cb, cc, cd):
        self.tri(a, b, c, ca, cb, cc); self.tri(a, c, d, ca, cc, cd)
    def export(self, name, smooth=False):
        me = bpy.data.meshes.new(name); me.from_pydata([tuple(p) for p in self.v], [], self.f); me.update()
        bm = bmesh.new(); bm.from_mesh(me); bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0001); bm.to_mesh(me); bm.free()
        # (remove_doubles keeps loop colours only if we set them after) -> set colours per corner by nearest source vertex
        col = me.color_attributes.new("Col", 'BYTE_COLOR', 'CORNER')
        lookup = {}
        for p, cc in zip(self.v, self.c): lookup[tuple(round(x, 4) for x in p)] = cc
        for loop in me.loops:
            p = tuple(round(x, 4) for x in me.vertices[loop.vertex_index].co)
            col.data[loop.index].color = lookup.get(p, (1, 0, 1, 1))
        for poly in me.polygons: poly.use_smooth = smooth
        ob = bpy.data.objects.new(name, me); bpy.context.scene.collection.objects.link(ob)
        bpy.ops.object.select_all(action='DESELECT'); ob.select_set(True); bpy.context.view_layer.objects.active = ob
        bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, name + ".fbx"), use_selection=True, apply_unit_scale=True,
                                 mesh_smooth_type='FACE', colors_type='SRGB', bake_anim=False, add_leaf_bones=False)
        bpy.data.objects.remove(ob); print("wrote", name, len(me.polygons), "tris")

# ---- colours (anime meadow palette) ----
ROOT = (0.10, 0.34, 0.20); TIP = (0.60, 0.88, 0.36); TIP_WARM = (0.80, 0.90, 0.42); TIP_COOL = (0.42, 0.80, 0.52)

def blade(B, base, h, w, lean_dir, lean, root=ROOT, tip=TIP, segs=3):
    """One grass blade: a tapering strip that bends over, root dark, tip bright."""
    side = Vector((-lean_dir.y, lean_dir.x, 0)) * (w / 2)
    pts = []
    for s in range(segs + 1):
        t = s / segs
        centre = base + Vector((0, 0, h * t)) + lean_dir * (lean * h * t * t)
        half = side * (1 - t) ** 0.8
        col = lerp(root, tip, t ** 0.7) + (t,)
        pts.append((centre - half, centre + half, col))
    for s in range(segs):
        a0, a1, ca = pts[s]; b0, b1, cb = pts[s + 1]
        if s == segs - 1:
            B.tri(a0, a1, (b0 + b1) / 2, ca, ca, cb)
        else:
            B.quad(a0, a1, b1, b0, ca, ca, cb, cb)

def grass_clump(B, n, r, hmin, hmax, wmin=0.025, wmax=0.045):
    for _ in range(n):
        a = R.uniform(0, 2 * math.pi); d = r * math.sqrt(R.random())
        base = Vector((math.cos(a) * d, math.sin(a) * d, -0.02))
        out = Vector((math.cos(a), math.sin(a), 0)) if d > 0.01 else Vector((1, 0, 0))
        out = (out + Vector((R.uniform(-.5, .5), R.uniform(-.5, .5), 0))).normalized()
        tip = R.choice([TIP, TIP, TIP_WARM, TIP_COOL])
        blade(B, base, R.uniform(hmin, hmax), R.uniform(wmin, wmax), out, R.uniform(0.2, 0.55), tip=tip)

def flower(B, at, petal_col, size, petals=5):
    """A flat open flower: a fan of petals around a yellow centre, tilted to face up-ish."""
    centre_col = (1.0, 0.78, 0.25, 1.0); pc = petal_col + (1.0,)
    up = Vector((R.uniform(-.3, .3), R.uniform(-.3, .3), 1)).normalized()
    t1 = up.orthogonal().normalized(); t2 = up.cross(t1)
    for i in range(petals):
        a0 = 2 * math.pi * i / petals; a1 = a0 + 2 * math.pi / petals
        am = (a0 + a1) / 2
        p0 = at + (t1 * math.cos(a0 - .25) + t2 * math.sin(a0 - .25)) * size * 0.55
        p1 = at + (t1 * math.cos(am) + t2 * math.sin(am)) * size + up * size * 0.15
        p2 = at + (t1 * math.cos(a1 + .25) + t2 * math.sin(a1 + .25)) * size * 0.55
        B.tri(at + up * 0.005, p0, p1, centre_col, pc, pc); B.tri(at + up * 0.005, p1, p2, centre_col, pc, pc)
    c = size * 0.25
    B.tri(at + up * 0.01 + t1 * c, at + up * 0.01 + t2 * c, at + up * 0.01 - t1 * c, centre_col, centre_col, centre_col)
    B.tri(at + up * 0.01 - t1 * c, at + up * 0.01 - t2 * c, at + up * 0.01 + t1 * c, centre_col, centre_col, centre_col)

def lumpy(B, centre, radius, top_col, bottom_col, subdiv=2, bumps=0.25, flat_bottom=None, alpha=0.0, seed=0):
    """A lumpy faceted ball (rock, bush or cloud puff); colour blends from bottom to top."""
    bm = bmesh.new(); bmesh.ops.create_icosphere(bm, subdivisions=subdiv, radius=1.0)
    for v in bm.verts:
        n = noise.noise(v.co * 1.7 + Vector((seed, seed * 2, seed * 3)))
        v.co *= radius * (1 + bumps * n)
        if flat_bottom is not None and v.co.z < flat_bottom * radius: v.co.z = flat_bottom * radius + (v.co.z - flat_bottom * radius) * 0.15
    bm.normal_update()
    for f in bm.faces:
        ps = [centre + v.co for v in f.verts]
        cs = []
        for v in f.verts:
            t = max(0.0, min(1.0, (v.co.z / radius) * 0.5 + 0.5))
            cs.append(lerp(bottom_col, top_col, t ** 1.4) + (alpha,))
        for i in range(1, len(ps) - 1): B.tri(ps[0], ps[i], ps[i + 1], cs[0], cs[i], cs[i + 1])
    bm.free()

# ---- 1. grass ----
B = Builder(); grass_clump(B, 46, 0.45, 0.22, 0.50); B.export("SM_GR_Grass")
B = Builder(); grass_clump(B, 26, 0.35, 0.60, 1.00, 0.03, 0.055); B.export("SM_GR_GrassTall")

# ---- 2. flowers (short grass + flowers on stems) ----
for name, col in [("Pink", (1.0, 0.52, 0.70)), ("White", (0.98, 0.97, 0.92)), ("Yellow", (1.0, 0.86, 0.28)), ("Blue", (0.45, 0.66, 1.0))]:
    B = Builder(); grass_clump(B, 16, 0.30, 0.15, 0.30)
    for _ in range(7):
        a = R.uniform(0, 2 * math.pi); d = 0.3 * math.sqrt(R.random()); h = R.uniform(0.28, 0.48)
        base = Vector((math.cos(a) * d, math.sin(a) * d, 0))
        blade(B, base, h, 0.018, Vector((math.cos(a), math.sin(a), 0)), 0.15, root=ROOT, tip=(0.30, 0.62, 0.30), segs=2)
        flower(B, base + Vector((math.cos(a), math.sin(a), 0)) * 0.15 * h + Vector((0, 0, h)), col, R.uniform(0.05, 0.08))
    B.export("SM_GR_Flowers_" + name)

# ---- 3. bush and rocks (alpha 0 = no wind on rocks; bushes sway a little) ----
B = Builder()
for i in range(7):
    c = Vector((R.uniform(-.35, .35), R.uniform(-.35, .35), R.uniform(0.25, 0.6)))
    lumpy(B, c, R.uniform(0.28, 0.42), (0.55, 0.85, 0.38), (0.10, 0.36, 0.22), subdiv=1, bumps=0.3, alpha=0.25, seed=i)
B.export("SM_GR_Bush")
for k, s in enumerate(["A", "B"]):
    B = Builder(); lumpy(B, Vector((0, 0, 0.35)), 0.6, (0.45, 0.72, 0.38), (0.52, 0.55, 0.66), subdiv=2, bumps=0.45, flat_bottom=-0.45, seed=10 + k)
    B.export("SM_GR_Rock_" + s)

# ---- 3b. moss: a soft lumpy cushion (flattened), deep green with bright tops; drapes on rock
#      tops and sides. Alpha 0 = it doesn't sway.
for k, s_ in enumerate(["A", "B"]):
    B = Builder()
    for i in range(5 + k * 2):
        c = Vector((R.uniform(-.25, .25), R.uniform(-.25, .25), 0.0))
        bm_r = R.uniform(0.16, 0.26)
        lumpy(B, c, bm_r, (0.50, 0.78, 0.30), (0.12, 0.36, 0.18), subdiv=2, bumps=0.35, flat_bottom=-0.1, seed=40 + i + k * 7)
    # squash it flat so it hugs the surface
    for j, p in enumerate(B.v):
        B.v[j] = Vector((p.x, p.y, max(-0.01, p.z * 0.32)))
    B.export("SM_GR_Moss_" + s_)

# ---- 4. clouds: puffy white tops, cool blue undersides (like Genshin's) ----
for k, s in enumerate(["A", "B", "C"]):
    B = Builder(); n = 6 + k * 2
    for i in range(n):
        x = (i / (n - 1) - 0.5) * (14 + k * 6) + R.uniform(-2, 2)
        r = R.uniform(3.5, 6.5) * (1.2 - abs(i / (n - 1) - 0.5))
        lumpy(B, Vector((x, R.uniform(-3, 3), r * 0.35)), r, (1.0, 1.0, 1.0), (0.62, 0.76, 0.93), subdiv=2, bumps=0.12, flat_bottom=-0.25, seed=20 + i + k * 9)
    B.export("SM_GR_Cloud_" + s, smooth=True)

# ---- 5. bird: a simple gull shape facing +X, wings flap (alpha = wing weight) ----
B = Builder()
W = (0.97, 0.97, 0.95); T = (0.25, 0.28, 0.34)
nose = Vector((0.22, 0, 0)); tail = Vector((-0.25, 0, 0)); top = Vector((0, 0, 0.05)); bot = Vector((0, 0, -0.04))
for a, b in [(nose, top), (top, tail), (tail, bot), (bot, nose)]:
    for y in (0.05, -0.05):
        B.tri(a, b, Vector((0, y, 0)), W + (0.0,), W + (0.0,), W + (0.0,))
for y in (1, -1):
    root_f = Vector((0.08, 0.04 * y, 0.01)); root_b = Vector((-0.08, 0.04 * y, 0.01))
    mid_f = Vector((0.05, 0.38 * y, 0.06)); mid_b = Vector((-0.10, 0.36 * y, 0.06))
    tip = Vector((-0.12, 0.70 * y, 0.02))
    B.quad(root_f, mid_f, mid_b, root_b, W + (0.0,), W + (0.5,), W + (0.5,), W + (0.0,))
    B.tri(mid_f, tip, mid_b, W + (0.5,), T + (1.0,), W + (0.5,))
B.export("SM_GR_Bird")

# ---- 6. butterflies: two pairs of rounded wings, 8 cm across ----
for name, col in [("Pink", (1.0, 0.55, 0.75)), ("Blue", (0.40, 0.70, 1.0)), ("Yellow", (1.0, 0.85, 0.30))]:
    B = Builder(); edge = lerp(col, (1, 1, 1), 0.5); body = (0.15, 0.12, 0.18)
    B.tri(Vector((0.02, 0, 0)), Vector((-0.025, 0.004, 0)), Vector((-0.025, -0.004, 0)), body + (0.0,), body + (0.0,), body + (0.0,))
    for y in (1, -1):
        for (cx, r, sx) in [(0.008, 0.028, 1.0), (-0.014, 0.020, 0.8)]:
            c0 = Vector((cx, 0.003 * y, 0)); steps = 8
            for i in range(steps):
                a0 = math.pi * i / steps; a1 = math.pi * (i + 1) / steps
                p0 = c0 + Vector((math.cos(a0) * r * sx * 0.8, math.sin(a0) * r * y * 1.6, 0))
                p1 = c0 + Vector((math.cos(a1) * r * sx * 0.8, math.sin(a1) * r * y * 1.6, 0))
                B.tri(c0, p0, p1, col + (0.0,), edge + (1.0,), edge + (1.0,))
    B.export("SM_GR_Butterfly_" + name)
print("ALL DONE")
