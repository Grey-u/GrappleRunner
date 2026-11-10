"""
Grapple Runner: harness v2 (ODM-style gear on a real climbing harness).

WHAT THIS IS
- A Blender script that builds the grapple harness out of simple shapes, in three pieces,
  so each piece can follow the bone it is strapped to:
    SM_Harness_Waist   -> follows the hips (pelvis bone): padded waist belt, buckle,
                          belay loop, gear loops, haul loop, gas engine box at the lower
                          back with fan grille, valves and hoses, and the two cable
                          launchers (with wound cable reels) on the hips.
    SM_Harness_ThighA  -> follows one thigh bone: padded leg loop with buckle, rise strap,
    SM_Harness_ThighB     extra ODM thigh straps, blade box (with blade slots) and gas tank.
                          A = outer side is +X, B = outer side is -X (mirror image).
- Design references (see Docs/PORTFOLIO and Docs/media/harness/):
    * Attack on Titan ODM gear cosplay builds: blade boxes and gas canisters ride on the
      thighs, cable "guns" sit in the belt at the hips, a central gas box at the back.
    * Rock-climbing harnesses (REI anatomy guide): padded waistbelt above the hip bones,
      leg loops, belay loop (the strongest point), buckles, gear loops, haul loop, rise straps.
  Why both: the climbing harness is what a real person would need to survive the rope
  forces our physics produces (several times body weight), and the ODM parts are the
  fantasy look. The ropes leave from the hips, close to the centre of mass, like the physics.

HOW TO RUN
    python make_harness.py <out_dir>          (with the "bpy" Python module installed)
    blender -b -P make_harness.py -- <out_dir>
Writes SM_Harness_Waist.fbx, SM_Harness_ThighA.fbx, SM_Harness_ThighB.fbx,
a preview render (harness_preview.png) and the .blend file.

HOW TO READ THIS (no coding experience needed)
- Lines starting with # are notes for humans; the computer skips them.
- "def name(...):" defines a reusable recipe; the indented lines under it are its steps.
- "for x in ...:" repeats the indented steps once per item.
- Sizes here are in METRES (Blender). Unreal turns them into centimetres on import.
- Directions: -Y is the wearer's FORWARD, +Z is up, X is left/right. (-Y because the
  import into Unreal flips Y, and the Paragon body faces +Y inside Unreal.)
"""
import math
import os
import sys

import bpy
import bmesh
from mathutils import Vector, Matrix

argv = sys.argv
OUT = argv[argv.index("--") + 1] if "--" in argv else (argv[1] if len(argv) > 1 else ".")
os.makedirs(OUT, exist_ok=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
FWD = -1.0   # multiply a Y value by this to mean "forward" (see note above)

# STRAPS ONLY (harness v3, 10 Nov 2026): the gas unit, blade boxes, tanks and launchers now
# come from a properly sculpted and textured ODM-gear model (Art/ThirdParty/make_odm_gear.py),
# so this script only makes the climbing-harness STRAPS that hold that gear on the body.
# Set to False to get the all-in-one v2 harness back (it's kept for the portfolio's process story).
STRAPS_ONLY = "--full" not in argv


# ------------------------------------------------------------------ materials
# Same names as the weapon kit, so Unreal re-uses the materials it already has.
def mat(name, color, metal=0.0, rough=0.5):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*color, 1.0)
    b.inputs["Metallic"].default_value = metal
    b.inputs["Roughness"].default_value = rough
    return m


M_STEEL = mat("M_Steel", (0.78, 0.80, 0.82), 1.0, 0.22)
M_DARK = mat("M_DarkMetal", (0.06, 0.065, 0.07), 0.9, 0.45)
M_WEB = mat("M_GripPolymer", (0.03, 0.03, 0.03), 0.0, 0.75)    # black nylon webbing
M_LEATHER = mat("M_Leather", (0.17, 0.09, 0.05), 0.0, 0.6)     # padding / leather (same as the weapon kit)
M_ORANGE = mat("M_Accent", (1.0, 0.22, 0.04), 0.0, 0.4)        # Grapple Runner orange
M_BRASS = mat("M_Brass", (0.75, 0.55, 0.25), 1.0, 0.3)


# ------------------------------------------------------------------ basic shapes
def link(ob):
    bpy.context.collection.objects.link(ob)
    return ob


def from_bmesh(name, bm, material):
    """Turn a bmesh (a mesh being built in memory) into a real object."""
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = link(bpy.data.objects.new(name, me))
    ob.data.materials.append(material)
    return ob


def prim(kind, name, material, **kw):
    getattr(bpy.ops.mesh, "primitive_%s_add" % kind)(**kw)
    ob = bpy.context.active_object
    ob.name = name
    ob.data.materials.clear()
    ob.data.materials.append(material)
    return ob


def bevel(ob, width, segments=2):
    m = ob.modifiers.new("Bevel", "BEVEL")
    m.width = width
    m.segments = segments
    m.limit_method = "ANGLE"
    m.angle_limit = math.radians(35)
    return ob


def box(name, size, loc, material, bev=0.0, rot=(0, 0, 0)):
    ob = prim("cube", name, material, size=1.0, location=loc, rotation=rot)
    ob.scale = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return bevel(ob, bev) if bev else ob


def cyl(name, r, depth, loc, material, rot=(0, 0, 0), verts=32, bev=0.0):
    ob = prim("cylinder", name, material, radius=r, depth=depth, location=loc,
              rotation=rot, vertices=verts)
    return bevel(ob, bev) if bev else ob


def sphere(name, r, loc, material, scale=(1, 1, 1)):
    ob = prim("uv_sphere", name, material, radius=r, location=loc, segments=24, ring_count=12)
    ob.scale = scale
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    return ob


def ring(name, major, minor, loc, material, rot=(0, 0, 0), seg=40):
    return prim("torus", name, material, major_radius=major, minor_radius=minor,
                location=loc, rotation=rot, major_segments=seg, minor_segments=10)


# ------------------------------------------------------------------ sweeps
# A "sweep" slides a 2D outline (the profile) along a 3D path, like squeezing icing from a
# piping bag. Straps, belts and hoses are all sweeps.
def frames(path, closed, axis=None):
    """For each point on the path: the direction it travels (T), and two sideways
    directions (S = 'outward', U = 'up') that the profile is drawn in."""
    n = len(path)
    out = []
    prev_s = None
    for i in range(n):
        a = path[(i - 1) % n] if (closed or i > 0) else path[i]
        b = path[(i + 1) % n] if (closed or i < n - 1) else path[i]
        t = (b - a).normalized()
        if axis is not None:
            s = t.cross(axis).normalized()          # loops around an axis: S points outward
        else:
            # parallel transport: keep the profile from twisting along a hose
            if prev_s is None:
                ref = Vector((0, 0, 1)) if abs(t.z) < 0.9 else Vector((1, 0, 0))
                s = t.cross(ref).normalized()
            else:
                s = (prev_s - t * prev_s.dot(t)).normalized()
            prev_s = s
        u = s.cross(t).normalized()
        out.append((t, s, u))
    return out


def sweep(name, path, profile, material, closed=True, axis=None, caps=True):
    """path: list of Vectors. profile: list of (s, u) points (a closed outline)."""
    bm = bmesh.new()
    fr = frames(path, closed, axis)
    rings = []
    for p, (t, s, u) in zip(path, fr):
        rings.append([bm.verts.new(p + s * ps + u * pu) for ps, pu in profile])
    m = len(profile)
    count = len(rings) if closed else len(rings) - 1
    for i in range(count):
        r0, r1 = rings[i], rings[(i + 1) % len(rings)]
        for j in range(m):
            bm.faces.new((r0[j], r0[(j + 1) % m], r1[(j + 1) % m], r1[j]))
    if not closed and caps:
        bm.faces.new(list(reversed(rings[0])))
        bm.faces.new(rings[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return from_bmesh(name, bm, material)


def rounded_rect(w, h, r, steps=3):
    """A strap's cross-section: w thick, h tall, corners rounded by r."""
    pts = []
    for cx, cy, a0 in ((w / 2 - r, h / 2 - r, 0), (-w / 2 + r, h / 2 - r, 90),
                       (-w / 2 + r, -h / 2 + r, 180), (w / 2 - r, -h / 2 + r, 270)):
        for k in range(steps + 1):
            a = math.radians(a0 + 90 * k / steps)
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def padded(w, h):
    """Padded belt section: thick soft middle, thin webbing edges (where the stitching is)."""
    return [(w * 0.15, -h / 2), (w * 0.55, -h * 0.42), (w * 0.85, -h * 0.25), (w, 0),
            (w * 0.85, h * 0.25), (w * 0.55, h * 0.42), (w * 0.15, h / 2),
            (-w * 0.15, h / 2), (-w * 0.2, 0), (-w * 0.15, -h / 2)]


def circle(r, n=12):
    return [(r * math.cos(2 * math.pi * k / n), r * math.sin(2 * math.pi * k / n)) for k in range(n)]


def ellipse_path(cx, cy, cz, rx, ry, n=64, tilt=None):
    """Points round an ellipse (a belt or leg loop). tilt = optional rotation matrix."""
    pts = []
    for k in range(n):
        a = 2 * math.pi * k / n
        v = Vector((rx * math.cos(a), ry * math.sin(a), 0))
        if tilt is not None:
            v = tilt @ v
        pts.append(Vector((cx, cy, cz)) + v)
    return pts


def curve_path(points, n=24):
    """A smooth path through a few control points (Catmull-Rom spline)."""
    pts = [Vector(p) for p in points]
    out = []
    for i in range(len(pts) - 1):
        p0 = pts[max(i - 1, 0)]
        p1, p2 = pts[i], pts[i + 1]
        p3 = pts[min(i + 2, len(pts) - 1)]
        for k in range(n):
            t = k / n
            out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t
                              + (-p0 + 3 * p1 - 3 * p2 + p3) * t ** 3))
    out.append(pts[-1])
    return out


def buckle(name, loc, rot, w=0.05, h=0.045):
    """A steel double-back buckle: a rectangular frame with a middle bar."""
    t = 0.006
    from mathutils import Euler
    R = Euler(rot).to_matrix()
    pieces = [((w, t, t), (0, 0, h / 2)), ((w, t, t), (0, 0, -h / 2)),        # top and bottom rails
              ((t, t, h), (w / 2, 0, 0)), ((t, t, h), (-w / 2, 0, 0)),        # side rails
              ((t * 0.8, t, h), (0, 0, 0))]                                    # middle bar
    parts = []
    for size, off in pieces:
        b = box(name, size, (0, 0, 0), M_STEEL, bev=0.002)
        b.rotation_euler = rot
        b.location = Vector(loc) + R @ Vector(off)   # turn the offset with the buckle
        parts.append(b)
    return parts


def rivets(name, centres, r=0.0045, material=None, rot=(0, 0, 0)):
    return [cyl(name, r, 0.004, c, material or M_BRASS, rot=rot, verts=10, bev=0.001) for c in centres]


def join(name, parts):
    """Apply modifiers, merge all the parts into one mesh, unwrap UVs, smooth shading."""
    bpy.ops.object.select_all(action="DESELECT")
    for p in parts:
        bpy.context.view_layer.objects.active = p
        for m in list(p.modifiers):
            bpy.ops.object.modifier_apply(modifier=m.name)
        p.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    ob = bpy.context.active_object
    ob.name = ob.data.name = name
    scene.cursor.location = (0, 0, 0)
    bpy.ops.object.origin_set(type="ORIGIN_CURSOR")
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.02)
    bpy.ops.object.mode_set(mode="OBJECT")
    try:
        bpy.ops.object.shade_auto_smooth(angle=math.radians(35))
    except Exception:
        bpy.ops.object.shade_smooth()
    ob.select_set(False)
    return ob


# ====================================================================================
#  1. WAIST PIECE (origin = pelvis bone, roughly the middle of the hips)
# ====================================================================================
def make_waist():
    P = []
    BELT_Z, RX, RY = 0.06, 0.175, 0.135        # belt sits ~6 cm above the hip joints
    # padded waist belt (leather padding) + webbing band (nylon) over it
    P.append(sweep("belt_pad", ellipse_path(0, 0, BELT_Z, RX, RY, 96), padded(0.016, 0.075),
                   M_LEATHER, axis=Vector((0, 0, 1))))
    P.append(sweep("belt_web", ellipse_path(0, 0, BELT_Z, RX + 0.012, RY + 0.012, 96),
                   rounded_rect(0.006, 0.038, 0.002), M_WEB, axis=Vector((0, 0, 1))))
    # orange stitching lines along the top and bottom of the padding
    for dz in (0.034, -0.034):
        P.append(sweep("stitch", ellipse_path(0, 0, BELT_Z + dz, RX + 0.004, RY + 0.004, 96),
                       circle(0.0018, 6), M_ORANGE, axis=Vector((0, 0, 1))))
    # front buckle (where the belt closes)
    front = Vector((0, FWD * (RY + 0.02), BELT_Z))
    P += buckle("buckle", front, (0, 0, 0))
    # belay loop: the strongest point; a vertical loop at the front joining belt and leg loops
    P.append(sweep("belay_loop", ellipse_path(0, FWD * (RY + 0.03), BELT_Z - 0.07, 0.022, 0.05, 40,
                                              tilt=Matrix.Rotation(math.radians(90), 3, "Y")),
                   rounded_rect(0.012, 0.022, 0.004), M_WEB, axis=Vector((1, 0, 0))))
    P.append(ring("belay_steel", 0.018, 0.0045, front + Vector((0, FWD * 0.012, -0.11)), M_STEEL,
                  rot=(0, math.radians(90), 0), seg=28))
    # gear loops: four stiff plastic arcs hanging off the sides of the belt
    for sx in (1, -1):
        for a_deg in (40, 75):
            # a U-shaped loop: from one point on the belt, down, and back up a little further round
            pts = []
            for k in range(13):
                f = k / 12
                a = math.radians(a_deg + 16 * f)
                r = 1.06 + 0.12 * math.sin(math.pi * f)              # bulges outward in the middle
                pts.append(Vector((sx * RX * math.sin(a) * r, FWD * RY * math.cos(a) * r,
                                   BELT_Z - 0.03 - 0.045 * math.sin(math.pi * f))))
            P.append(sweep("gear_loop", pts, rounded_rect(0.005, 0.012, 0.002), M_WEB, closed=False))
    # haul loop at the back
    P.append(sweep("haul_loop", ellipse_path(0, -FWD * (RY + 0.018), BELT_Z + 0.03, 0.018, 0.028, 30,
                                             tilt=Matrix.Rotation(math.radians(90), 3, "Y")),
                   rounded_rect(0.006, 0.016, 0.002), M_WEB, axis=Vector((1, 0, 0))))

    if STRAPS_ONLY:
        return join("SM_Harness_Waist", P)

    # ---- gas engine box at the lower back (ODM "central unit")
    back = -FWD * (RY + 0.06)
    P.append(box("engine", (0.2, 0.075, 0.13), (0, back, BELT_Z - 0.01), M_DARK, bev=0.014))
    P.append(box("engine_plate", (0.18, 0.008, 0.105), (0, back - FWD * 0.04, BELT_Z - 0.01), M_STEEL, bev=0.004))
    P.append(box("engine_stripe", (0.182, 0.01, 0.014), (0, back - FWD * 0.043, BELT_Z + 0.035), M_ORANGE, bev=0.002))
    P += rivets("engine_bolt", [(sx * 0.08, back - FWD * 0.045, BELT_Z - 0.01 + sz * 0.045)
                                for sx in (1, -1) for sz in (1, -1)],
                rot=(math.radians(90), 0, 0))
    # fan grille: a ring with eight blades, like the gas turbine on the anime gear
    fan_c = Vector((0, back - FWD * 0.046, BELT_Z - 0.02))
    P.append(ring("fan_ring", 0.035, 0.005, fan_c, M_STEEL, rot=(math.radians(90), 0, 0)))
    P.append(cyl("fan_hub", 0.011, 0.012, fan_c, M_BRASS, rot=(math.radians(90), 0, 0), bev=0.002))
    for k in range(8):
        a = math.radians(45 * k)
        b = box("fan_blade", (0.03, 0.002, 0.008), fan_c + Vector((0.017 * math.cos(a), 0, 0.017 * math.sin(a))),
                M_STEEL, rot=(math.radians(25), -a, 0))
        P.append(b)
    # gas valves (knurled knobs) on top of the engine, one per side
    for sx in (1, -1):
        v = Vector((sx * 0.06, back, BELT_Z + 0.065))
        P.append(cyl("valve_stem", 0.008, 0.02, v, M_STEEL, verts=12))
        P.append(cyl("valve_knob", 0.016, 0.012, v + Vector((0, 0, 0.014)), M_BRASS, verts=24, bev=0.002))
        # high-pressure hose from the valve, round the hip, down toward the thigh gas tank
        hose = curve_path([v + Vector((0, 0, 0.01)),
                           Vector((sx * 0.13, back * 0.9, BELT_Z + 0.06)),
                           Vector((sx * (RX + 0.05), -FWD * 0.04, BELT_Z - 0.02)),
                           Vector((sx * (RX + 0.06), FWD * 0.0, BELT_Z - 0.08))], 14)
        P.append(sweep("hose", hose, circle(0.0065, 10), M_WEB, closed=False))
        for t in (0.25, 0.6):
            i = int(len(hose) * t)
            P.append(sweep("hose_clamp", ellipse_path(0, 0, 0, 0.0085, 0.0085, 14), circle(0.0022, 6),
                           M_BRASS, axis=Vector((0, 0, 1))))
            c = P[-1]
            d = (hose[i + 1] - hose[i]).normalized()
            c.rotation_euler = d.to_track_quat("Z", "Y").to_euler()
            c.location = hose[i]

    # ---- cable launchers on each hip (the ropes come out of these)
    for sx in (1, -1):
        hip = Vector((sx * (RX + 0.035), FWD * 0.0, BELT_Z - 0.01))
        aim = (math.radians(90), 0, math.radians(-12) * sx * FWD)   # points forward, a bit outward
        # mounting bracket bolted through the belt
        P.append(box("launch_bracket", (0.02, 0.07, 0.06), hip - Vector((sx * 0.025, 0, 0)), M_DARK, bev=0.004))
        P += rivets("bracket_bolt", [hip - Vector((sx * 0.036, FWD * dy, 0)) for dy in (-0.02, 0.02)],
                    rot=(0, math.radians(90), 0))
        # housing (barrel) along the forward direction
        P.append(cyl("launch_body", 0.03, 0.17, hip + Vector((0, FWD * 0.02, 0)), M_DARK, rot=aim, verts=32, bev=0.006))
        P.append(cyl("launch_band", 0.0315, 0.012, hip + Vector((0, FWD * 0.06, 0)), M_ORANGE, rot=aim))
        P.append(cyl("launch_band", 0.0315, 0.012, hip + Vector((0, FWD * -0.03, 0)), M_STEEL, rot=aim))
        # muzzle and the anchor (hook) sitting in it, with three barbs
        muzzle = hip + Vector((sx * 0.012 * FWD * -1, FWD * 0.115, 0))
        P.append(cyl("muzzle", 0.022, 0.03, muzzle, M_STEEL, rot=aim, bev=0.003))
        tip = prim("cone", "anchor_tip", M_STEEL, radius1=0.014, radius2=0.0, depth=0.05,
                   location=muzzle + Vector((0, FWD * 0.035, 0)),
                   rotation=(math.radians(90) * -FWD, 0, 0), vertices=16)
        P.append(tip)
        for k in range(3):
            a = math.radians(120 * k)
            P.append(box("barb", (0.004, 0.022, 0.004),
                         muzzle + Vector((0.012 * math.cos(a), FWD * 0.02, 0.012 * math.sin(a))),
                         M_STEEL, rot=(0, a, 0)))
        # cable reel on the outside of the launcher, with wound cable (stacked rings)
        reel_c = hip + Vector((sx * 0.04, FWD * -0.01, 0))
        P.append(cyl("reel_drum", 0.045, 0.036, reel_c, M_STEEL, rot=(0, math.radians(90), 0), verts=48, bev=0.003))
        P.append(cyl("reel_side", 0.052, 0.004, reel_c + Vector((sx * 0.02, 0, 0)), M_DARK,
                     rot=(0, math.radians(90), 0), verts=48))
        P.append(cyl("reel_side", 0.052, 0.004, reel_c - Vector((sx * 0.02, 0, 0)), M_DARK,
                     rot=(0, math.radians(90), 0), verts=48))
        for k in range(6):
            P.append(ring("cable_wind", 0.047, 0.0028, reel_c + Vector((sx * (-0.014 + 0.0056 * k), 0, 0)),
                          M_WEB, rot=(0, math.radians(90), 0), seg=40))
        P.append(cyl("reel_hub", 0.016, 0.046, reel_c, M_ORANGE, rot=(0, math.radians(90), 0), verts=6, bev=0.002))
        # spokes in the outer reel plate
        for k in range(5):
            a = math.radians(72 * k)
            P.append(box("reel_spoke", (0.003, 0.034, 0.006),
                         reel_c + Vector((sx * 0.023, 0.02 * math.cos(a), 0.02 * math.sin(a))),
                         M_STEEL, rot=(a, 0, 0)))
    return join("SM_Harness_Waist", P)


# ====================================================================================
#  2. THIGH PIECE (origin = the hip joint, where the thigh bone starts)
#     side = +1: the outer side of this leg is +X.  side = -1: mirror image.
# ====================================================================================
def make_thigh(side, name):
    P = []
    LOOP_Z = -0.14
    # the leg loop hugs the thigh just below the hip joint
    loop = ellipse_path(0, 0, LOOP_Z, 0.088, 0.082, 64, tilt=Matrix.Rotation(math.radians(-8) * side, 3, "Y"))
    P.append(sweep("leg_pad", loop, padded(0.013, 0.05), M_LEATHER, axis=Vector((0, 0, 1))))
    P.append(sweep("leg_web", ellipse_path(0, 0, LOOP_Z, 0.098, 0.092, 64,
                                           tilt=Matrix.Rotation(math.radians(-8) * side, 3, "Y")),
                   rounded_rect(0.005, 0.026, 0.002), M_WEB, axis=Vector((0, 0, 1))))
    P += buckle("leg_buckle", Vector((side * 0.03, FWD * 0.1, LOOP_Z)), (0, 0, math.radians(15) * side),
                w=0.035, h=0.03)
    # rise strap: the strap from the back of the leg loop up toward the waist belt
    P.append(sweep("rise", curve_path([Vector((0, -FWD * 0.09, LOOP_Z + 0.01)),
                                       Vector((-side * 0.02, -FWD * 0.12, LOOP_Z + 0.1)),
                                       Vector((-side * 0.05, -FWD * 0.13, 0.04))], 10),
                   rounded_rect(0.004, 0.022, 0.002), M_WEB, closed=False))
    # two extra ODM straps lower on the thigh (the anime gear's many thigh belts)
    for z, w in ((-0.26, 0.02), (-0.36, 0.018)):
        r = 0.083 - (z + 0.26) * 0.12
        P.append(sweep("thigh_strap", ellipse_path(0, 0, z, r, r * 0.95, 56,
                                                   tilt=Matrix.Rotation(math.radians(-10) * side, 3, "Y")),
                       rounded_rect(0.005, w, 0.002), M_LEATHER, axis=Vector((0, 0, 1))))
        P += buckle("thigh_buckle", Vector((side * 0.02, FWD * (r + 0.004), z)), (0, 0, 0), w=0.024, h=w + 0.006)

    if STRAPS_ONLY:
        return join(name, P)

    # blade box on the outside of the thigh: long, angled back and down, open at the back
    # Everything on the box is placed in the BOX'S OWN directions (bx + R @ offset), so the
    # trim, slots and tank stay stuck to it whatever angle the box is tilted at.
    # Box's own axes: x = sideways, y = along its length (+y = toward the back), z = up.
    from mathutils import Euler
    bx = Vector((side * 0.135, -FWD * 0.02, -0.22))
    tilt = Euler((math.radians(-18), 0, 0))          # back end lower than the front end
    R = tilt.to_matrix()
    def at(x, y, z):
        return bx + R @ Vector((x, -FWD * y, z))
    def put(ob):
        ob.rotation_euler = tilt
        return ob
    P.append(put(box("blade_box", (0.07, 0.48, 0.13), at(0, 0, 0), M_DARK, bev=0.012)))
    P.append(put(box("blade_box_panel", (0.004, 0.40, 0.095), at(side * 0.036, 0, 0), M_STEEL, bev=0.0015)))
    P.append(put(box("blade_box_trim", (0.074, 0.022, 0.134), at(0, -0.2, 0), M_ORANGE, bev=0.003)))
    P += rivets("panel_rivet", [at(side * 0.039, y, z) for y in (-0.17, 0.17) for z in (-0.035, 0.035)],
                rot=(0, math.radians(90), 0))
    # the open back end, showing four spare blade ends sticking out of their slots
    for k in range(4):
        P.append(put(box("blade_slot", (0.05, 0.02, 0.004), at(0, 0.245, -0.042 + 0.028 * k), M_STEEL, bev=0.001)))
    # clamp arms: how the box hangs off the leg strap (bridge from box to the leg loop)
    for y in (-0.1, 0.1):
        P.append(put(box("box_clamp", (0.05, 0.03, 0.02), at(-side * 0.05, y, 0.03), M_DARK, bev=0.004)))
    # gas tank riding on top of the box, parallel to it, with domed ends and a brass valve
    trot = (tilt.x + math.radians(90), 0, 0)
    axis_v = R @ Vector((0, -FWD, 0))                 # the box's length direction
    tank_c = at(0, 0.02, 0.1)
    P.append(cyl("gas_tank", 0.033, 0.3, tank_c, M_STEEL, rot=trot, verts=40, bev=0.003))
    for e in (1, -1):
        P.append(sphere("tank_dome", 0.033, tank_c + axis_v * e * 0.15, M_STEEL))
    P.append(cyl("tank_valve", 0.011, 0.04, tank_c - axis_v * 0.19, M_BRASS, rot=trot, verts=16, bev=0.002))
    for t in (-0.1, 0.0, 0.1):
        P.append(cyl("tank_band", 0.0345, 0.012, tank_c + axis_v * t, M_ORANGE if t == 0 else M_DARK,
                     rot=trot, verts=40))
    # two straps holding the tank down onto the box
    for t in (-0.07, 0.07):
        P.append(put(box("tank_strap", (0.075, 0.016, 0.004), at(0, t, 0.067), M_LEATHER, bev=0.001)))
    return join(name, P)


waist = make_waist()
thigh_a = make_thigh(1, "SM_Harness_ThighA")
thigh_b = make_thigh(-1, "SM_Harness_ThighB")
assets = [waist, thigh_a, thigh_b]

# ------------------------------------------------------------------ export (same settings as the weapon kit)
for ob in assets:
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, ob.name + ".fbx"), use_selection=True,
                             apply_scale_options="FBX_SCALE_ALL", mesh_smooth_type="FACE",
                             add_leaf_bones=False, bake_space_transform=True,
                             axis_forward="-Z", axis_up="Y")
    tris = sum(len(p.vertices) - 2 for p in ob.data.polygons)
    print("exported", ob.name, len(ob.data.polygons), "faces,", tris, "triangles")

# ------------------------------------------------------------------ preview render on a simple mannequin
# Thighs placed where they'd sit on a standing person (hip joints 9 cm either side, 6 cm down).
thigh_a.location = (0.09, 0, -0.06)
thigh_b.location = (-0.09, 0, -0.06)
for o in assets:
    o.location.z += 1.0
ghost = mat("M_Ghost", (0.55, 0.57, 0.6), 0.0, 0.8)
body = [cyl("torso", 0.15, 0.5, (0, 0, 1.2), ghost, verts=32)]
body[-1].scale = (1.12, 0.85, 1)
for sx in (1, -1):
    t = cyl("thigh", 0.075, 0.45, (sx * 0.095, 0, 0.72), ghost, verts=24)
    t.rotation_euler = (0, math.radians(-4) * sx, 0)
    body.append(t)
for name_, loc, rot in (("front", (0.9, -2.0, 1.25), None), ("back", (-1.2, 1.6, 1.35), None)):
    pass
floor = prim("plane", "floor", mat("M_Floor", (0.9, 0.9, 0.88), 0.0, 0.9), size=8, location=(0, 0, 0))
cam = link(bpy.data.objects.new("cam", bpy.data.cameras.new("cam")))
cam.data.lens = 50
scene.camera = cam
for name_, loc, energy, size in (("key", (2, -2, 3), 700, 2), ("fill", (-3, -1.5, 1.5), 250, 3),
                                 ("rim", (0, 2.5, 2.5), 500, 1.5)):
    ld = bpy.data.lights.new(name_, "AREA")
    ld.energy, ld.size = energy, size
    lo = link(bpy.data.objects.new(name_, ld))
    lo.location = loc
    lo.rotation_euler = (Vector((0, 0, 1)) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
world = bpy.data.worlds.new("w")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.92, 0.92, 0.9, 1)
world.node_tree.nodes["Background"].inputs[1].default_value = 0.6
scene.world = world
scene.render.engine = "CYCLES"
scene.cycles.device = "CPU"
scene.cycles.samples = 32
scene.render.resolution_x, scene.render.resolution_y = 1400, 1000
for view, loc in (("front", (0.85, -1.9, 1.35)), ("back", (-1.0, 1.7, 1.4)), ("side", (2.0, -0.2, 1.1))):
    cam.location = loc
    cam.rotation_euler = (Vector((0, 0, 0.95)) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
    scene.render.filepath = os.path.join(OUT, "harness_v2_%s.png" % view)
    bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "GrappleRunner_Harness.blend"))
print("done")
