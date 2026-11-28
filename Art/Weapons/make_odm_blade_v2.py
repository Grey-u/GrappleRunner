"""
Grapple Runner: ODM blade, version 2 (replaces the plain slab-and-box sword).

WHAT THIS MAKES
  SM_ODMSword.fbx  - one blade + trigger handle, used in both hands. Our own design, so we own it.
  Previews: odm_v2_side.png (whole blade), odm_v2_handle.png (close-up of the handle).

  What is different from version 1:
  - The blade has a real cross-section: a flat face, a long ground bevel down to the edge,
    and a narrow polished micro-bevel, so light runs along it like real steel.
  - Snap-off segments are cut INTO the steel as V-grooves, angled like the tip.
  - The tip is an angled chisel cut, and the blade sits in a clamp with two bolts.
  - The handle is shaped for a hand: an oval grip angled down from the blade line, with
    finger swells, a guard, two triggers inside a loop, an orange Arcadium accent line,
    and a swivel cap where the gas cable plugs in.

  Same conventions as version 1, so the game code needs no change:
    ORIGIN = middle of the grip (where your fist closes), blade along +Y, sharp edge on +X.
    Sockets: SOCKET_Tip, SOCKET_Base, SOCKET_Edge.

Run: python3 make_odm_blade_v2.py <out_dir>      (Blender's Python module, bpy)

HOW TO READ THIS (no coding experience needed)
- Lines starting with # are notes. "def name(...):" is a reusable recipe.
- Sizes are metres (Blender). Unreal turns them into centimetres on import.
"""
import math
import os
import sys

import bpy
import bmesh
from mathutils import Vector, Matrix

OUT = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith("-") else "."
os.makedirs(OUT, exist_ok=True)
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene


# ------------------------------------------------------------------ 1. materials
def mat(name, color, metal=0.0, rough=0.5, aniso=0.0, bump=0.0, bump_scale=200.0, stretch=(1, 1, 1)):
    """A surface. aniso > 0 gives brushed metal; bump adds fine texture (knurling, brushing)."""
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    b = nt.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*color, 1.0)
    b.inputs["Metallic"].default_value = metal
    b.inputs["Roughness"].default_value = rough
    if aniso and "Anisotropic" in b.inputs:
        b.inputs["Anisotropic"].default_value = aniso
    if bump:
        tc = nt.nodes.new("ShaderNodeTexCoord")
        mp = nt.nodes.new("ShaderNodeMapping")
        mp.inputs["Scale"].default_value = stretch
        nz = nt.nodes.new("ShaderNodeTexNoise")
        nz.inputs["Scale"].default_value = bump_scale
        nz.inputs["Detail"].default_value = 8
        bp = nt.nodes.new("ShaderNodeBump")
        bp.inputs["Strength"].default_value = bump
        nt.links.new(tc.outputs["Object"], mp.inputs["Vector"])
        nt.links.new(mp.outputs["Vector"], nz.inputs["Vector"])
        nt.links.new(nz.outputs["Fac"], bp.inputs["Height"])
        nt.links.new(bp.outputs["Normal"], b.inputs["Normal"])
    return m


M_STEEL = mat("M_SwordSteel", (0.62, 0.64, 0.67), 1.0, 0.24, aniso=0.6, bump=0.04, bump_scale=60, stretch=(40, 1, 40))
M_EDGE = mat("M_SwordEdge", (0.90, 0.91, 0.93), 1.0, 0.08)
M_GROOVE = mat("M_SwordGroove", (0.30, 0.31, 0.33), 1.0, 0.45)
M_FRAME = mat("M_SwordDark", (0.06, 0.065, 0.075), 1.0, 0.32, bump=0.02, bump_scale=400)
M_GRIP = mat("M_SwordGrip", (0.012, 0.011, 0.010), 0.0, 0.62, bump=0.18, bump_scale=900)
M_ACCENT = mat("M_SwordAccent", (1.0, 0.30, 0.04), 0.0, 0.35)
M_BRASS = mat("M_SwordBrass", (0.80, 0.58, 0.30), 1.0, 0.28)

# ------------------------------------------------------------------ 2. measurements (metres)
GRIP_LEN = 0.12
GRIP_TILT = math.radians(12)      # grip angled down from the blade line, like a pistol grip
HOUSING_Y = 0.088                 # centre of the block the blade slots into
BLADE_Y0 = 0.118                  # where the steel leaves the clamp
BLADE_LEN = 0.86
W0, W1 = 0.056, 0.047             # blade width at the base and at the tip
TIP_CUT = 0.085                   # the chisel tip runs back this far on the edge side
T_SPINE = 0.0021                  # half thickness at the back (4.2 mm blade)
BEVEL_START = 0.58                # the edge bevel starts 58% of the way across the width
T_EDGE = 0.00012                  # half thickness at the very edge


def link(ob):
    bpy.context.collection.objects.link(ob)
    return ob


def finish(ob, smooth_angle=30):
    """Smooth shading with crisp edges kept crisp."""
    for p in ob.data.polygons:
        p.use_smooth = True
    try:
        ob.data.set_sharp_from_angle(angle=math.radians(smooth_angle))
    except Exception:
        pass
    return ob


# ------------------------------------------------------------------ 3. the blade
def blade():
    """Built as rows across the width. Columns: spine edge, flat face, bevel start, micro-bevel,
    edge. Each row exists on the front (+z) and back (-z)."""
    rows = 64
    cols = [(0.0, T_SPINE * 0.92), (0.04, T_SPINE), (BEVEL_START, T_SPINE), (0.93, 0.00045), (1.0, T_EDGE)]
    bm = bmesh.new()
    grid = []
    for i in range(rows + 1):
        t = i / rows
        w = W0 + (W1 - W0) * t
        y_sp = BLADE_Y0 + BLADE_LEN * t
        y_ed = BLADE_Y0 + (BLADE_LEN - TIP_CUT) * t
        row = []
        for c, th in cols:
            x = w * c
            y = y_sp + (y_ed - y_sp) * c
            row.append((bm.verts.new((x, y, th)), bm.verts.new((x, y, -th))))
        grid.append(row)
    nc = len(cols)
    for i in range(rows):
        for c in range(nc - 1):
            bm.faces.new((grid[i][c][0], grid[i][c + 1][0], grid[i + 1][c + 1][0], grid[i + 1][c][0]))
            bm.faces.new((grid[i + 1][c][1], grid[i + 1][c + 1][1], grid[i][c + 1][1], grid[i][c][1]))
        bm.faces.new((grid[i][0][1], grid[i][0][0], grid[i + 1][0][0], grid[i + 1][0][1]))           # spine
        bm.faces.new((grid[i][-1][0], grid[i][-1][1], grid[i + 1][-1][1], grid[i + 1][-1][0]))       # edge
    for c in range(nc - 1):
        bm.faces.new((grid[0][c][1], grid[0][c + 1][1], grid[0][c + 1][0], grid[0][c][0]))           # base
        bm.faces.new((grid[rows][c][0], grid[rows][c + 1][0], grid[rows][c + 1][1], grid[rows][c][1]))  # tip
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new("blade")
    bm.to_mesh(me)
    bm.free()
    ob = link(bpy.data.objects.new("blade", me))
    ob.data.materials.append(M_STEEL)
    ob.data.materials.append(M_EDGE)
    for p in ob.data.polygons:      # the micro-bevel faces (outermost column) get the polished edge material
        if p.center.x > (W1 * 0.92) and abs(p.normal.z) > 0.2:
            p.material_index = 1
    finish(ob, 20)
    parts = [ob]
    # Snap-off grooves: thin dark V strips set into both faces, angled like the chisel tip.
    ang = math.atan2(TIP_CUT, W0)
    for k in range(1, 10):
        y = BLADE_Y0 + 0.082 * k
        w = W0 + (W1 - W0) * ((y - BLADE_Y0) / BLADE_LEN)
        L = (w * BEVEL_START) / math.cos(ang)
        for s in (1, -1):
            bpy.ops.mesh.primitive_cube_add(size=1, location=(w * BEVEL_START / 2, y - math.tan(ang) * w * BEVEL_START / 2,
                                                              s * (T_SPINE - 0.00005)),
                                            rotation=(0, 0, -ang))
            g = bpy.context.active_object
            g.scale = (L, 0.0011, 0.00025)
            bpy.ops.object.transform_apply(scale=True)
            g.data.materials.append(M_GROOVE)
            parts.append(g)
    return parts


# ------------------------------------------------------------------ 4. the handle
def ring_loft(name, rings, material, cap=True):
    """Skin a tube through a list of rings: (centre Vector, x-radius, z-radius, n-sides)."""
    bm = bmesh.new()
    loops = []
    for cen, rx, rz, n in rings:
        loop = []
        for k in range(n):
            a = 2 * math.pi * k / n
            loop.append(bm.verts.new(cen + Vector((math.cos(a) * rx, 0, math.sin(a) * rz))))
        loops.append(loop)
    for a, b in zip(loops, loops[1:]):
        n = len(a)
        for k in range(n):
            bm.faces.new((a[k], a[(k + 1) % n], b[(k + 1) % n], b[k]))
    if cap:
        bm.faces.new(list(reversed(loops[0])))
        bm.faces.new(loops[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = link(bpy.data.objects.new(name, me))
    ob.data.materials.append(material)
    sub = ob.modifiers.new("sub", "SUBSURF")
    sub.levels = 2
    return finish(ob, 60)


def box(name, size, loc, material, bevel=0.002, rot=(0, 0, 0)):
    bpy.ops.mesh.primitive_cube_add(size=1, location=loc, rotation=rot)
    ob = bpy.context.active_object
    ob.name = name
    ob.scale = size
    bpy.ops.object.transform_apply(scale=True)
    ob.data.materials.append(material)
    if bevel:
        m = ob.modifiers.new("bev", "BEVEL")
        m.width = bevel
        m.segments = 3
    return finish(ob)


def cyl(name, r, depth, loc, material, rot=(math.radians(90), 0, 0), verts=40, bevel=0.0):
    bpy.ops.mesh.primitive_cylinder_add(radius=r, depth=depth, location=loc, rotation=rot, vertices=verts)
    ob = bpy.context.active_object
    ob.name = name
    ob.data.materials.append(material)
    if bevel:
        m = ob.modifiers.new("bev", "BEVEL")
        m.width = bevel
        m.segments = 3
    return finish(ob)


def handle():
    parts = []
    # Grip: an oval tube with three finger swells, tilted down from the blade line.
    tilt = Matrix.Rotation(GRIP_TILT, 3, "Z")       # tilts toward the edge side (+X), like a pistol grip
    rings = []
    steps = 14
    for k in range(steps + 1):
        t = k / steps
        y = -GRIP_LEN / 2 + GRIP_LEN * t
        swell = 0.0016 * math.sin(t * math.pi * 3.0) ** 2 * (1 - abs(2 * t - 1) * 0.3)
        rx = 0.0158 + swell + 0.002 * (1 - t)       # a little fuller at the bottom, for the palm
        rz = 0.0128 + swell * 0.6
        rings.append((tilt @ Vector((0, y, 0)), rx, rz, 24))
    parts.append(ring_loft("grip", rings, M_GRIP))
    # Orange accent line down the back of the grip (spine side)
    for k in range(steps):
        t = (k + 0.5) / steps
        y = -GRIP_LEN / 2 + GRIP_LEN * t
        p = tilt @ Vector((-0.0172, y, 0))
    acc = box("accent_line", (0.003, GRIP_LEN * 0.82, 0.006), tilt @ Vector((-0.0168, 0.004, 0)), M_ACCENT, bevel=0.001,
              rot=(0, 0, GRIP_TILT))
    parts.append(acc)
    # Guard plate and housing that holds the blade
    parts.append(box("guard", (0.060, 0.007, 0.036), (0.016, GRIP_LEN / 2 + 0.004, 0), M_FRAME, bevel=0.003))
    parts.append(box("housing", (0.052, BLADE_Y0 - GRIP_LEN / 2 - 0.006, 0.030),
                     (0.024, (BLADE_Y0 + GRIP_LEN / 2 + 0.006) / 2, 0), M_FRAME, bevel=0.005))
    parts.append(box("clamp", (0.064, 0.016, 0.009), (0.026, BLADE_Y0 - 0.010, 0), M_FRAME, bevel=0.002))
    parts.append(box("accent_band", (0.054, 0.005, 0.032), (0.024, HOUSING_Y - 0.004, 0), M_ACCENT, bevel=0.0012))
    # Clamp bolts on both faces
    for s in (1, -1):
        for bx in (0.012, 0.040):
            parts.append(cyl("bolt", 0.0036, 0.003, (bx, BLADE_Y0 - 0.010, s * 0.0055), M_BRASS,
                             rot=(0, 0, 0), verts=6, bevel=0.0006))
    # Triggers inside a loop on the edge side
    for k, y in enumerate((0.030, 0.006)):
        parts.append(box("trigger", (0.011, 0.016, 0.008), tilt @ Vector((0.026, y, 0)), M_FRAME if k else M_ACCENT,
                         bevel=0.003, rot=(0, 0, GRIP_TILT - math.radians(10))))
    cu = bpy.data.curves.new("tguard", "CURVE")
    cu.dimensions = "3D"
    cu.bevel_depth = 0.0028
    cu.bevel_resolution = 4
    sp = cu.splines.new("BEZIER")
    pts = [tilt @ Vector(v) for v in ((0.040, GRIP_LEN / 2 + 0.002, 0), (0.046, 0.018, 0), (0.030, -0.022, 0), (0.016, -0.030, 0))]
    sp.bezier_points.add(len(pts) - 1)
    for bp, p in zip(sp.bezier_points, pts):
        bp.co = p
        bp.handle_left_type = bp.handle_right_type = "AUTO"
    gob = link(bpy.data.objects.new("tguard", cu))
    gob.data.materials.append(M_FRAME)
    bpy.context.view_layer.objects.active = gob
    gob.select_set(True)
    bpy.ops.object.convert(target="MESH")
    gob.select_set(False)
    parts.append(finish(gob))
    # Pommel with the swivel where the gas cable plugs in
    bottom = tilt @ Vector((0, -GRIP_LEN / 2, 0))
    parts.append(cyl("pommel", 0.0185, 0.016, bottom + Vector((0, -0.006, 0)), M_FRAME, bevel=0.003))
    parts.append(cyl("swivel", 0.0085, 0.014, bottom + Vector((0, -0.020, 0)), M_BRASS, bevel=0.001))
    parts.append(cyl("cable_stub", 0.0045, 0.02, bottom + Vector((0, -0.034, 0)), M_GRIP))
    return parts


parts = blade() + handle()

# Join everything into one model, origin at the middle of the grip.
for p in parts:
    bpy.context.view_layer.objects.active = p
    for m in list(p.modifiers):
        bpy.ops.object.modifier_apply(modifier=m.name)
bpy.ops.object.select_all(action="DESELECT")
for p in parts:
    p.select_set(True)
bpy.context.view_layer.objects.active = parts[0]
bpy.ops.object.join()
sword = bpy.context.active_object
sword.name = sword.data.name = "SM_ODMSword"
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.select_all(action="SELECT")
bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.02)
bpy.ops.object.mode_set(mode="OBJECT")


def socket(name, loc):
    e = link(bpy.data.objects.new(name, None))
    e.empty_display_size = 0.02
    e.location = loc
    e.parent = sword
    return e


sockets = [socket("SOCKET_Tip", (0, BLADE_Y0 + BLADE_LEN, 0)), socket("SOCKET_Base", (0, BLADE_Y0, 0)),
           socket("SOCKET_Edge", ((W0 + W1) / 2, BLADE_Y0 + BLADE_LEN * 0.5, 0))]

bpy.ops.object.select_all(action="DESELECT")
sword.select_set(True)
for s in sockets:
    s.select_set(True)
bpy.context.view_layer.objects.active = sword
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, "SM_ODMSword.fbx"), use_selection=True,
                         apply_scale_options="FBX_SCALE_ALL", mesh_smooth_type="FACE", add_leaf_bones=False,
                         bake_space_transform=True, axis_forward="-Z", axis_up="Y")
print("exported", len(sword.data.polygons), "faces")

# ------------------------------------------------------------------ 5. preview renders
world = bpy.data.worlds.new("w")
world.use_nodes = True
bg = world.node_tree.nodes["Background"]
grad = world.node_tree.nodes.new("ShaderNodeTexGradient")
tc = world.node_tree.nodes.new("ShaderNodeTexCoord")
ramp = world.node_tree.nodes.new("ShaderNodeValToRGB")
ramp.color_ramp.elements[0].color = (0.02, 0.022, 0.026, 1)
ramp.color_ramp.elements[1].color = (0.35, 0.4, 0.48, 1)
sep = world.node_tree.nodes.new("ShaderNodeSeparateXYZ")
world.node_tree.links.new(tc.outputs["Generated"], sep.inputs[0])
world.node_tree.links.new(sep.outputs["Z"], ramp.inputs["Fac"])
world.node_tree.links.new(ramp.outputs["Color"], bg.inputs["Color"])
bg.inputs["Strength"].default_value = 0.3
scene.world = world
for name, loc, energy, size in [("key", (0.6, -0.8, 1.2), 60, 0.8), ("strip", (-0.8, 0.3, 0.9), 120, 0.08),
                                ("rim", (0.2, 1.2, 0.6), 80, 0.4)]:
    ld = bpy.data.lights.new(name, "AREA")
    ld.energy = energy
    ld.size = size
    lo = link(bpy.data.objects.new(name, ld))
    lo.location = loc
    lo.rotation_euler = (Vector((0, 0.4, 0)) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
bpy.ops.mesh.primitive_plane_add(size=6, location=(0, 0, -0.06))
floor = bpy.context.active_object
floor.data.materials.append(mat("M_Floor", (0.05, 0.05, 0.055), 0.0, 0.35))
cam_data = bpy.data.cameras.new("cam")
cam = link(bpy.data.objects.new("cam", cam_data))
scene.camera = cam
scene.render.engine = "CYCLES"
scene.cycles.device = "CPU"
scene.cycles.samples = 96
scene.view_settings.view_transform = "AgX"
scene.view_settings.look = "AgX - Medium High Contrast"


def shoot(file, cam_loc, target, lens, rx, ry, sword_rot):
    sword.rotation_euler = sword_rot
    cam.location = cam_loc
    cam.rotation_euler = (Vector(target) - Vector(cam_loc)).to_track_quat("-Z", "Y").to_euler()
    cam_data.lens = lens
    scene.render.resolution_x, scene.render.resolution_y = rx, ry
    scene.render.filepath = os.path.join(OUT, file)
    bpy.ops.render.render(write_still=True)


# whole blade, lying across the frame, tilted so the bevel catches the light
shoot("odm_v2_side.png", (0.5, 0.0, 1.3), (0.5, 0.0, 0.0), 32, 1800, 900,
      (math.radians(18), 0, math.radians(-90)))
# handle close-up, three-quarter view
shoot("odm_v2_handle.png", (0.18, -0.20, 0.16), (0.01, 0.04, 0.0), 50, 1400, 1000,
      (math.radians(-25), math.radians(10), math.radians(-20)))
print("done")
