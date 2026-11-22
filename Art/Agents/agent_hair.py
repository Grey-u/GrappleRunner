# =====================================================================================
#  agent_hair.py : real strand hair for the agents (Blender hair curves)
# -------------------------------------------------------------------------------------
#  WHAT THIS IS
#  The first pass grew hair as a solid shell, which looked like a helmet. This grows
#  thousands of actual strands from the scalp, like a hairdresser would style them:
#    - roots are scattered over the scalp where that style has hair
#    - each strand grows out, then is combed (towards a knot, to one side, forwards)
#      and/or falls under gravity, and is pushed back out if it would go into the head
#    - coloured parts (silver tips, red ends, lilac streaks...) are a second object
#  Rendered with Blender's hair shader, which is what makes it read as real hair.
#
#  HOW TO READ IT
#  Head      a stretched sphere fitted to the head, used for "don't go inside the head"
#  scalp()   random root points on the scalp, more where the surface is bigger
#  grow()    the strand builder every style uses (out, comb, fall, frizz)
#  STYLE_*   one function per hairstyle from the roster doc
#  make()    builds the curves objects for one agent
# =====================================================================================
import bpy, math, random
from mathutils import Vector

DOWN = Vector((0, 0, -1))


class Head:
    """Measures one agent's head: chin/top heights, back of head, and a fitted ellipsoid."""
    def __init__(self, ob):
        me = ob.data
        zs = [v.co.z for v in me.vertices]; self.top = max(zs); H = self.top - min(zs)
        self.chin = self.top - H * 0.125
        hv = [v.co for v in me.vertices if v.co.z > self.chin + 0.02]
        lo = Vector((min(c.x for c in hv), min(c.y for c in hv), min(c.z for c in hv)))
        hi = Vector((max(c.x for c in hv), max(c.y for c in hv), self.top))
        self.c = (lo + hi) / 2
        self.r = (hi - lo) / 2
        self.c.z -= 0.012; self.r.z += 0.006
        self.ymin, self.ymax, self.xr = lo.y, hi.y, max(abs(lo.x), abs(hi.x))
        self.H = self.top - self.chin
        self.ob = ob

    def frame(self, p):
        """u: 0 chin .. 1 crown;  f: 0 face .. 1 back;  s: -1 .. 1 side to side."""
        return ((p.z - self.chin) / self.H, (p.y - self.ymin) / (self.ymax - self.ymin), p.x / self.xr)

    def in_face(self, p):
        D = self.ymax - self.ymin
        return (p.y < self.ymin + 0.42 * D and self.chin - 0.04 < p.z < self.chin + 0.64 * self.H
                and abs(p.x - self.c.x) < 0.78 * self.xr)

    def guard(self, p):
        """Keep hair from hanging in front of the face: inside the face box, slide it
        out to the side of the cheek."""
        D = self.ymax - self.ymin
        brow = self.chin + 0.64 * self.H
        if p.y < self.ymin + 0.42 * D and self.chin - 0.04 < p.z < brow:
            dx = p.x - self.c.x; lim = 0.78 * self.xr
            if abs(dx) < lim:
                p = Vector((self.c.x + math.copysign(lim, dx if dx else 1.0), p.y, p.z))
        return p

    def collide(self, p, margin):
        q = Vector(((p.x - self.c.x) / self.r.x, (p.y - self.c.y) / self.r.y, (p.z - self.c.z) / self.r.z))
        m = q.length
        lim = 1.0 + margin
        if m < lim and m > 1e-6:
            q = q / m * lim
            return Vector((self.c.x + q.x * self.r.x, self.c.y + q.y * self.r.y, self.c.z + q.z * self.r.z))
        return p


def scalp(head, rule, count, rnd):
    """Area-weighted random roots on the scalp triangles where rule(u, f, s) is True."""
    me = head.ob.data
    me.calc_loop_triangles()
    tris = []
    for t in me.loop_triangles:
        a, b, c = (me.vertices[i].co for i in t.vertices)
        cen = (a + b + c) / 3
        if cen.z < head.chin - 0.01:
            continue
        if rule(*head.frame(cen)):
            tris.append((a.copy(), b.copy(), c.copy(), t.normal.copy(), t.area))
    total = sum(t[4] for t in tris)
    roots = []
    if not tris:
        return roots
    acc, cum = 0.0, []
    for t in tris:
        acc += t[4]; cum.append(acc)
    import bisect
    for _ in range(count):
        k = bisect.bisect_left(cum, rnd.random() * total)
        a, b, c, n, _ar = tris[min(k, len(tris) - 1)]
        r1, r2 = rnd.random(), rnd.random()
        if r1 + r2 > 1: r1, r2 = 1 - r1, 1 - r2
        roots.append((a + (b - a) * r1 + (c - a) * r2, n))
    return roots


def grow(head, root, n, length, segs=12, lift=0.25, comb=None, comb_w=0.0, gravity=0.6,
         frizz=0.0, margin=0.05, rnd=random, target=None, tail=0.0, tail_frizz=0.05):
    """One strand. Starts along the scalp normal (lift), bends towards a comb direction,
    then falls under gravity. With target: travels over the head to that point first
    (knot, ponytail, braid), then hangs down as a tail."""
    pts = [root.copy()]
    p = root + n * 0.0015
    pts[0] = p.copy()
    d = n.copy()
    if target is not None:
        # travel to the gather point over the head surface
        steps = max(4, segs // 2)
        for i in range(1, steps + 1):
            t = i / steps
            q = p.lerp(target, t)
            q = head.collide(q, margin * (1 - t))
            pts.append(q)
        p = pts[-1]; d = DOWN.copy()
        segs_t = max(3, segs // 2)
        for i in range(segs_t):
            w = Vector((rnd.gauss(0, 1), rnd.gauss(0, 1), rnd.gauss(0, 1))) * tail_frizz
            d = (d * 0.7 + DOWN * 0.3 + w).normalized()
            p = p + d * (tail / segs_t)
            pts.append(head.collide(p, margin))
        return pts
    step = length / segs
    for i in range(segs):
        t = (i + 1) / segs
        want = n * max(0.0, lift - t)
        if comb is not None:
            want += comb * comb_w * min(1.0, t * 2.5)
        want += DOWN * gravity * t
        if frizz:
            want += Vector((rnd.gauss(0, 1), rnd.gauss(0, 1), rnd.gauss(0, 1))) * frizz
        if want.length < 1e-6:
            want = n
        d = (d * 0.55 + want.normalized() * 0.45).normalized()
        p = p + d * step
        p = head.collide(p, margin)
        if head.in_face(p):          # hair stops where it would hang over the face
            break
        pts.append(p.copy())
    return pts


def tangent_towards(head, p, aim):
    """Direction along the head surface from p towards an aim direction."""
    out = (p - head.c).normalized()
    t = aim - out * aim.dot(out)
    return t.normalized() if t.length > 1e-6 else aim


# ---- styles: each returns {"main": [strands], "tip": [strands]} -----------------------
def cap(u, f, s):
    return u > 0.80 - 0.62 * f


def stubble(head, rnd, rule, n=3000, L=0.004):
    return [grow(head, r, nn, L, segs=2, lift=1.0, gravity=0, margin=0.0, rnd=rnd) for r, nn in scalp(head, rule, n, rnd)]


def STYLE_twists(head, rnd):                       # Orishi: short twists, silver tips, faded sides
    top = lambda u, f, s: u > 0.70 - 0.2 * f and (abs(s) < 0.9 or u > 0.88)
    main, tip = [], []
    centres = scalp(head, top, 750, rnd)
    for c, n in centres:
        L = rnd.uniform(0.05, 0.07)
        path = grow(head, c, n, L, segs=9, lift=0.7, gravity=0.55, frizz=0.12, margin=0.05, rnd=rnd)
        side = (n.cross(Vector((0, 0, 1))) if abs(n.z) < 0.9 else n.cross(Vector((1, 0, 0)))).normalized()
        up = n.cross(side).normalized()
        for k in range(16):
            ang0 = rnd.uniform(0, 6.283); rad = rnd.uniform(0.0, 0.0045)
            s = []
            for i, q in enumerate(path):
                t = i / (len(path) - 1)
                a = ang0 + t * 9.0                     # the twist
                r = rad * (1 - 0.6 * t)
                s.append(q + (side * math.cos(a) + up * math.sin(a)) * r)
            cut = int(len(s) * 0.8)
            main.append(s[:cut + 1]); tip.append(s[cut:])
    main += stubble(head, rnd, lambda u, f, s: cap(u, f, s) and not top(u, f, s), 2500)
    return main, tip


def STYLE_afro(head, rnd):                         # Wanjiru: short natural afro (crescent shave on one side)
    rule = lambda u, f, s: u > 0.74 - 0.45 * f
    main = []
    for r, n in scalp(head, rule, 18000, rnd):
        out = ((r - head.c).normalized() + n).normalized()      # round shape: grow away from the head's centre
        main.append(grow(head, r, out, rnd.uniform(0.05, 0.065), segs=8, lift=2.0, gravity=0.05, frizz=0.7,
                         margin=0.30, rnd=rnd))
    tip = []
    return main, tip


def STYLE_bob(head, rnd):                          # Hoa: jaw-length bob, blunt fringe, red ends
    main, tip = [], []
    for r, n in scalp(head, cap, 14000, rnd):
        u, f, s = head.frame(r)
        if f < 0.28:                               # fringe: forward and down to the brow
            L = max(0.015, (r.z - (head.chin + 0.68 * head.H)) + 0.015)
            st = grow(head, r, n, L, segs=8, lift=0.3, comb=Vector((0, -1, 0)), comb_w=0.6, gravity=1.0, margin=0.07, rnd=rnd)
        else:
            L = (r.z - (head.chin + 0.12 * head.H)) + 0.04
            away = Vector((math.copysign(1.0, s if s else 1.0), 0.6, 0)).normalized()
            st = grow(head, r, n, L, segs=14, lift=0.35, comb=away, comb_w=0.35, gravity=1.4, frizz=0.04, margin=0.10, rnd=rnd)
        cut = int(len(st) * 0.72)
        if f >= 0.28:
            main.append(st[:cut + 1]); tip.append(st[cut:])
        else:
            main.append(st)
    return main, tip


def STYLE_knot(head, rnd):                         # Arnav: thick hair tied in a short knot
    knot = Vector((head.c.x, head.ymax + 0.012, head.top - 0.25 * head.H))
    main = []
    for r, n in scalp(head, cap, 12000, rnd):
        tgt = knot + Vector((rnd.gauss(0, .008), rnd.gauss(0, .004), rnd.gauss(0, .008)))
        main.append(grow(head, r, n, 0, segs=10, margin=0.06, rnd=rnd, target=tgt, tail=0.0))
    for _ in range(2500):                          # the knot itself: looping strands
        a = rnd.uniform(0, 6.283); b = rnd.uniform(-1, 1)
        pts = []
        for i in range(10):
            t = i / 9 * 6.283
            pts.append(knot + Vector((math.cos(a + t) * 0.03, 0.012 + math.sin(t) * 0.02 * b, math.sin(a + t) * 0.026)))
        main.append(pts)
    return main, []


def STYLE_undercut(head, rnd):                     # Mateo: undercut, top tied back into a short braid
    top = lambda u, f, s: u > 0.74 - 0.1 * f and abs(s) < 0.8
    gather = Vector((head.c.x, head.ymax + 0.006, head.top - 0.45 * head.H))
    main = []
    for r, n in scalp(head, top, 9000, rnd):
        main.append(grow(head, r, n, 0, segs=10, margin=0.05, rnd=rnd, target=gather + Vector((rnd.gauss(0, .004), 0, rnd.gauss(0, .004))), tail=0.0))
    main += braid(head, gather, 0.13, 0.010, rnd, 1500)
    main += stubble(head, rnd, lambda u, f, s: cap(u, f, s) and not top(u, f, s), 3500, 0.003)
    return main, []


def braid(head, start, length, width, rnd, n):
    """Three plaited bundles hanging down from start."""
    out = []
    for k in range(n):
        b = k % 3; ph = b * 2.094
        ang = rnd.uniform(0, 6.283); rr = rnd.uniform(0, width * 0.45)
        pts = []
        for i in range(24):
            t = i / 23
            w = width * (1 - 0.35 * t)
            cx = math.sin(t * 25 + ph) * w * 0.6
            cy = math.cos(t * 25 + ph) * w * 0.25 + 0.01 + t * 0.03
            q = start + Vector((cx + math.cos(ang) * rr * (1 - .4 * t), cy + math.sin(ang) * rr * 0.6, -t * length))
            pts.append(head.collide(q, 0.12))
        out.append(pts)
    return out


def STYLE_braid(head, rnd):                        # Saule: long single thick braid
    nape = Vector((head.c.x, head.ymax + 0.004, head.chin + 0.35 * head.H))
    main = []
    for r, n in scalp(head, cap, 12000, rnd):
        main.append(grow(head, r, n, 0, segs=12, margin=0.05, rnd=rnd, target=nape + Vector((rnd.gauss(0, .006), 0, rnd.gauss(0, .004))), tail=0.0))
    main += braid(head, nape, 0.55, 0.026, rnd, 4500)
    return main, []


def STYLE_swept(head, rnd):                        # Noor: shaved one side, long and swept over the other, lilac streaks
    rule = lambda u, f, s: cap(u, f, s) and not (s > 0.5 and u < 0.9)
    main, tip = [], []
    aim = Vector((-1, 0.4, 0.0)).normalized()
    for r, n in scalp(head, rule, 13000, rnd):
        L = rnd.uniform(0.15, 0.2)
        st = grow(head, r, n, L, segs=14, lift=0.3, comb=tangent_towards(head, r, aim), comb_w=0.55,
                  gravity=1.3, frizz=0.08, margin=0.09, rnd=rnd)
        (tip if rnd.random() < 0.14 else main).append(st)
    main += stubble(head, rnd, lambda u, f, s: cap(u, f, s) and s > 0.5 and u < 0.9, 1800, 0.003)
    return main, tip


def STYLE_shag(head, rnd):                         # Rowan: messy copper shag, white streak at the front
    main, tip = [], []
    for r, n in scalp(head, cap, 14000, rnd):
        u, f, s = head.frame(r)
        if f < 0.3:
            st = grow(head, r, n, rnd.uniform(0.05, 0.07), segs=10, lift=0.5, comb=Vector((0, -1, 0)), comb_w=0.6,
                      gravity=0.9, frizz=0.12, margin=0.10, rnd=rnd)
        else:
            st = grow(head, r, n, rnd.uniform(0.08, 0.12), segs=12, lift=0.6, gravity=1.1, frizz=0.12, margin=0.12, rnd=rnd)
        (tip if (f < 0.25 and abs(s) < 0.22) else main).append(st)
    return main, tip


def STYLE_twoblock(head, rnd):                     # Ji-ho: two-block cut, orange fringe
    top = lambda u, f, s: u > 0.66 - 0.08 * f and abs(s) < 0.82
    main, tip = [], []
    for r, n in scalp(head, top, 11000, rnd):
        u, f, s = head.frame(r)
        if f < 0.45:
            st = grow(head, r, n, rnd.uniform(0.06, 0.08), segs=10, lift=0.6, comb=Vector((0, -1, -0.2)), comb_w=0.8,
                      gravity=0.6, frizz=0.15, margin=0.10, rnd=rnd)
        else:
            st = grow(head, r, n, rnd.uniform(0.05, 0.07), segs=10, lift=0.8, comb=Vector((0, 1, 0)), comb_w=0.5,
                      gravity=0.5, frizz=0.2, margin=0.10, rnd=rnd)
        (tip if f < 0.22 else main).append(st)
    main += stubble(head, rnd, lambda u, f, s: cap(u, f, s) and not top(u, f, s), 3000, 0.004)
    return main, tip


def STYLE_ponytail(head, rnd):                     # Valentina: long high ponytail, magenta underlayer
    tie = Vector((head.c.x, head.ymax + 0.01, head.top - 0.18 * head.H))
    main, tip = [], []
    for r, n in scalp(head, cap, 12000, rnd):
        tgt = tie + Vector((rnd.gauss(0, .006), 0, rnd.gauss(0, .006)))
        st = grow(head, r, n, 0, segs=16, margin=0.05, rnd=rnd, target=tgt, tail=rnd.uniform(0.32, 0.42), tail_frizz=0.04)
        # the tail flows out and down from the tie, not straight into the neck
        for i in range(len(st)):
            if st[i].z < tie.z:
                st[i] = st[i] + Vector((0, 0.04 + (tie.z - st[i].z) * 0.25, 0))
        (tip if r.z < head.chin + 0.45 * head.H and rnd.random() < 0.7 else main).append(st)
    return main, tip


STYLES = {"twists": STYLE_twists, "afro": STYLE_afro, "bob": STYLE_bob, "knot": STYLE_knot,
          "undercut": STYLE_undercut, "braid": STYLE_braid, "swept": STYLE_swept, "shag": STYLE_shag,
          "twoblock": STYLE_twoblock, "ponytail": STYLE_ponytail}


def hair_material(name, colour, rough=0.32):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    for nd in list(nt.nodes):
        if nd.type == "BSDF_PRINCIPLED": nt.nodes.remove(nd)
    h = nt.nodes.new("ShaderNodeBsdfHairPrincipled")
    h.parametrization = "COLOR"
    h.inputs["Color"].default_value = (*colour, 1)
    h.inputs["Roughness"].default_value = rough
    if "Radial Roughness" in h.inputs: h.inputs["Radial Roughness"].default_value = 0.5
    out = nt.nodes["Material Output"]
    nt.links.new(h.outputs[0], out.inputs["Surface"])
    return m


def curves_object(name, strands, mat, loc, radius=0.00045):
    strands = [s for s in strands if len(s) >= 2]
    hc = bpy.data.hair_curves.new(name)
    hc.add_curves([len(s) for s in strands])
    flat = [c for s in strands for p in s for c in p]
    hc.position_data.foreach_set("vector", flat)
    rad = hc.attributes.get("radius") or hc.attributes.new("radius", "FLOAT", "POINT")
    rv = []
    for s in strands:
        n = len(s)
        rv += [radius * (1 - 0.6 * i / max(1, n - 1)) for i in range(n)]
    rad.data.foreach_set("value", rv)
    hc.materials.append(mat)
    ob = bpy.data.objects.new(name, hc)
    bpy.context.collection.objects.link(ob)
    ob.location = loc
    return ob


def make(agent, body_ob, mat_main, mat_tip, seed):
    rnd = random.Random(seed)
    head = Head(body_ob)
    main, tip = STYLES[agent["hair"]](head, rnd)
    obs = [curves_object(body_ob.name + "_Hair", main, mat_main, body_ob.location)]
    if tip:
        obs.append(curves_object(body_ob.name + "_HairTint", tip, mat_tip or mat_main, body_ob.location))
    if agent["hair"] == "braid":   # Saule's copper rings down the braid
        for k in range(3):
            nape = Vector((head.c.x, head.ymax + 0.004, head.chin + 0.35 * head.H))
            bpy.ops.mesh.primitive_torus_add(major_radius=0.02, minor_radius=0.004,
                                             location=body_ob.location + nape + Vector((0, 0.01 + 0.012 * (k + 1), -0.12 - 0.13 * k)))
            t = bpy.context.active_object
            cu = bpy.data.materials.new("Copper"); cu.use_nodes = True
            b = cu.node_tree.nodes["Principled BSDF"]; b.inputs["Base Color"].default_value = (0.85, 0.42, 0.22, 1)
            b.inputs["Metallic"].default_value = 1.0; b.inputs["Roughness"].default_value = 0.3
            t.data.materials.append(cu)
    return obs
