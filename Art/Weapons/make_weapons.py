"""
Grapple Runner: weapon kit v1 (procedural, game-ready base meshes).

Builds, in metres, origin at the grip / attach point:
  SM_Blade         - long single-edged snap-segment blade
  SM_BladeHandle   - trigger handle that the blade slots into
  SM_GrappleGear   - hip rig: central gas unit, two grapple launchers with reels,
                     two gas cylinders, two blade scabbards
  SM_Pistol        - universal sidearm (any character can use it)

Run headless:  blender -b -P make_weapons.py -- <out_dir>
or with the bpy module:  python make_weapons.py <out_dir>
Writes one FBX per asset plus a preview render.

HOW TO READ THIS (no coding experience needed)
- Lines starting with # are notes for humans; the computer skips them.
- Text inside triple quotes (like this block) is also just a note.
- "import" = load a toolbox. "unreal" is Unreal Engine's own Python toolbox.
- "def name(...):" defines a reusable recipe; the indented lines under it are its steps.
- "for x in ...:" repeats the indented steps once per item; "if ...:" only runs them when true.
- Unreal distances are in centimetres (100 = 1 metre).
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

# ------------------------------------------------------------------ scene reset
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = "METRIC"
scene.unit_settings.scale_length = 1.0


# ------------------------------------------------------------------ materials
def mat(name, color, metal=0.0, rough=0.5):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*color, 1.0)
    b.inputs["Metallic"].default_value = metal
    b.inputs["Roughness"].default_value = rough
    return m


M_STEEL = mat("M_Steel", (0.78, 0.80, 0.82), 1.0, 0.22)
M_EDGE = mat("M_SteelEdge", (0.95, 0.96, 0.97), 1.0, 0.08)
M_DARK = mat("M_DarkMetal", (0.06, 0.065, 0.07), 0.9, 0.45)
M_GRIP = mat("M_GripPolymer", (0.03, 0.03, 0.03), 0.0, 0.75)
M_LEATHER = mat("M_Leather", (0.17, 0.09, 0.05), 0.0, 0.6)
M_ORANGE = mat("M_Accent", (1.0, 0.22, 0.04), 0.0, 0.4)
M_BRASS = mat("M_Brass", (0.75, 0.55, 0.25), 1.0, 0.3)


# ------------------------------------------------------------------ helpers
def link(obj):
    bpy.context.collection.objects.link(obj)
    return obj


def mesh_obj(name, verts, faces, material):
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    me.update()
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


def box(name, size, loc, material, bevel=0.0, rot=(0, 0, 0)):
    ob = prim("cube", name, material, size=1.0, location=loc, rotation=rot)
    ob.scale = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel:
        bev(ob, bevel)
    return ob


def cyl(name, r, depth, loc, material, rot=(0, 0, 0), verts=32, bevel=0.0):
    ob = prim("cylinder", name, material, radius=r, depth=depth, location=loc,
              rotation=rot, vertices=verts)
    if bevel:
        bev(ob, bevel)
    return ob


def bev(ob, width, segments=2):
    m = ob.modifiers.new("Bevel", "BEVEL")
    m.width = width
    m.segments = segments
    m.limit_method = "ANGLE"
    m.angle_limit = math.radians(35)
    return m


def smooth(ob, angle=35):
    """Shade smooth with sharp edges kept (works in Blender 4.1+ and 5.x)."""
    bpy.context.view_layer.objects.active = ob
    ob.select_set(True)
    try:
        bpy.ops.object.shade_auto_smooth(angle=math.radians(angle))
    except Exception:
        try:
            bpy.ops.object.shade_smooth_by_angle(angle=math.radians(angle))
        except Exception:
            bpy.ops.object.shade_smooth()
    ob.select_set(False)


def join(name, parts, origin=(0, 0, 0)):
    """Apply modifiers, join parts into one mesh object, set origin, smart-UV it."""
    bpy.ops.object.select_all(action="DESELECT")
    for p in parts:
        p.select_set(True)
        bpy.context.view_layer.objects.active = p
        for m in list(p.modifiers):
            bpy.ops.object.modifier_apply(modifier=m.name)
    bpy.context.view_layer.objects.active = parts[0]
    bpy.ops.object.join()
    ob = bpy.context.active_object
    ob.name = name
    ob.data.name = name
    scene.cursor.location = origin
    bpy.ops.object.origin_set(type="ORIGIN_CURSOR")
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.02)
    bpy.ops.object.mode_set(mode="OBJECT")
    smooth(ob)
    ob.select_set(False)
    return ob


# ------------------------------------------------------------------ 1. blade
def make_blade():
    """Single-edged blade along +Y. Spine is thick, cutting edge thin, angled tip.
    Snap-segment score lines are inset strips (separate material = easy to read in VR)."""
    L, W = 0.90, 0.062           # length, width
    edge_len = 0.80              # where the angled tip starts on the cutting edge
    t_spine, t_edge = 0.0032, 0.0004
    # profile: (x across width, y along length); x=0 spine, x=W cutting edge
    prof = [(0.0, 0.0), (W, 0.0), (W, edge_len), (0.0, L)]
    # add intermediate points so the bevel between spine and edge has geometry
    rows = []
    n = 8
    for i in range(n + 1):
        y_spine = L * i / n
        y_edge = edge_len * i / n
        rows.append([(0.0, y_spine), (W * 0.45, (y_spine + y_edge) / 2 * 0.0 + y_spine - (y_spine - y_edge) * 0.45), (W, y_edge)])
    verts, faces = [], []

    def th(x):
        return t_spine if x < W * 0.5 else t_spine + (t_edge - t_spine) * (x - W * 0.45) / (W * 0.55)

    for r in rows:
        for (x, y) in r:
            verts.append((x, y, th(x)))
            verts.append((x, y, -th(x)))
    cols = 3

    def vi(row, col, side):
        return (row * cols + col) * 2 + side

    for i in range(n):
        for c in range(cols - 1):
            for side in (0, 1):
                a, b = vi(i, c, side), vi(i, c + 1, side)
                cc, d = vi(i + 1, c + 1, side), vi(i + 1, c, side)
                faces.append((a, b, cc, d) if side == 0 else (d, cc, b, a))
    for i in range(n):  # spine side and edge side walls
        faces.append((vi(i, 0, 1), vi(i, 0, 0), vi(i + 1, 0, 0), vi(i + 1, 0, 1)))
        faces.append((vi(i, 2, 0), vi(i, 2, 1), vi(i + 1, 2, 1), vi(i + 1, 2, 0)))
    for c in range(cols - 1):  # base and tip caps
        faces.append((vi(0, c, 1), vi(0, c + 1, 1), vi(0, c + 1, 0), vi(0, c, 0)))
        faces.append((vi(n, c, 0), vi(n, c + 1, 0), vi(n, c + 1, 1), vi(n, c, 1)))
    blade = mesh_obj("blade_body", verts, faces, M_STEEL)
    bpy.context.view_layer.objects.active = blade
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.mode_set(mode="OBJECT")

    parts = [blade]
    # snap-segment score lines, angled like a box-cutter blade
    for k in range(1, 8):
        y = 0.1 * k
        for s in (1, -1):
            strip = box("score", (0.07, 0.0016, 0.0006), (W / 2, y, s * (t_spine + 0.0001)),
                        M_DARK, rot=(0, 0, math.radians(-28)))
            parts.append(strip)
    # bright honed edge strip
    edge = box("edge", (0.006, edge_len, 0.0012), (W - 0.003, edge_len / 2, 0), M_EDGE)
    parts.append(edge)
    # tang that slots into the handle
    parts.append(box("tang", (0.03, 0.05, 0.004), (0.02, -0.025, 0), M_DARK, bevel=0.001))
    return join("SM_Blade", parts, origin=(0.02, -0.05, 0))


# ------------------------------------------------------------------ 2. handle
def make_handle():
    """Trigger handle held in a fist, blade housing on top, grip along -Y."""
    parts = []
    parts.append(cyl("grip", 0.019, 0.13, (0, -0.075, 0), M_GRIP, rot=(math.radians(90), 0, 0), bevel=0.003))
    # grip rings
    for k in range(4):
        parts.append(cyl("ring", 0.0205, 0.004, (0, -0.03 - k * 0.028, 0), M_DARK, rot=(math.radians(90), 0, 0)))
    parts.append(box("housing", (0.075, 0.06, 0.045), (0.018, 0.01, 0), M_DARK, bevel=0.006))
    parts.append(box("accent", (0.077, 0.012, 0.047), (0.018, 0.03, 0), M_ORANGE, bevel=0.002))
    # trigger lever (releases spent blades)
    parts.append(box("trigger", (0.012, 0.07, 0.01), (0.03, -0.05, 0), M_STEEL, bevel=0.003,
                     rot=(0, 0, math.radians(8))))
    # pommel cable port to the gas unit
    parts.append(cyl("pommel", 0.022, 0.02, (0, -0.145, 0), M_STEEL, rot=(math.radians(90), 0, 0), bevel=0.003))
    parts.append(cyl("port", 0.008, 0.03, (0, -0.165, 0), M_BRASS, rot=(math.radians(90), 0, 0)))
    return join("SM_BladeHandle", parts, origin=(0, -0.075, 0))


# ------------------------------------------------------------------ 3. hip grapple gear
def make_gear():
    """Worn at the waist. +Y is the player's forward, Z up, origin at the belt centre."""
    parts = []
    # central gas / engine unit at the lower back
    parts.append(box("core", (0.22, 0.09, 0.13), (0, -0.16, 0), M_DARK, bevel=0.012))
    parts.append(box("core_plate", (0.2, 0.01, 0.11), (0, -0.21, 0), M_STEEL, bevel=0.004))
    parts.append(box("core_stripe", (0.205, 0.012, 0.02), (0, -0.212, 0.035), M_ORANGE, bevel=0.002))
    parts.append(cyl("fan", 0.04, 0.02, (0, -0.22, -0.01), M_STEEL, rot=(math.radians(90), 0, 0), bevel=0.003))
    # two gas tanks lying across the lower back, below the core
    for s in (1, -1):
        rot = (0, math.radians(90), math.radians(6) * s)
        parts.append(cyl("gas", 0.036, 0.3, (s * 0.17, -0.19, -0.1), M_STEEL, rot=rot, bevel=0.006))
        parts.append(cyl("gas_cap", 0.026, 0.035, (s * 0.335, -0.18, -0.1), M_BRASS, rot=rot, bevel=0.004))
        parts.append(cyl("gas_band", 0.0375, 0.015, (s * 0.12, -0.19, -0.1), M_ORANGE, rot=rot))
    # grapple launchers on each hip, pointing forward-outward
    for s in (1, -1):
        x = s * 0.2
        rot = (math.radians(90), 0, math.radians(-15) * s)
        parts.append(cyl("launcher", 0.042, 0.2, (x, -0.02, 0.0), M_DARK, rot=rot, bevel=0.006))
        parts.append(cyl("muzzle", 0.026, 0.05, (x + s * 0.03, 0.1, 0.0), M_STEEL, rot=rot, bevel=0.004))
        parts.append(cyl("anchor_tip", 0.012, 0.04, (x + s * 0.04, 0.14, 0.0), M_BRASS, rot=rot))
        # cable reel drum on the outside
        parts.append(cyl("reel", 0.055, 0.03, (x + s * 0.045, -0.04, 0.0), M_STEEL,
                         rot=(0, math.radians(90), 0), verts=40, bevel=0.004))
        parts.append(cyl("reel_hub", 0.02, 0.034, (x + s * 0.046, -0.04, 0.0), M_ORANGE,
                         rot=(0, math.radians(90), 0)))
        # strut back to the core
        parts.append(box("strut", (0.16, 0.025, 0.025), (s * 0.12, -0.1, 0.0), M_DARK, bevel=0.004,
                         rot=(0, 0, math.radians(35) * s)))
    # belt
    belt = prim("torus", "belt", M_LEATHER, major_radius=0.17, minor_radius=0.012,
                location=(0, 0, 0.05), major_segments=48, minor_segments=8)
    belt.scale = (1.0, 1.15, 2.2)
    bpy.ops.object.transform_apply(scale=True)
    parts.append(belt)
    # blade boxes on each thigh: long axis runs backward and angles down (holds 4 spare blades)
    for s in (1, -1):
        rot = (math.radians(28), 0, math.radians(4) * s)
        parts.append(box("scabbard", (0.075, 0.62, 0.15), (s * 0.27, -0.12, -0.24), M_DARK, bevel=0.012, rot=rot))
        parts.append(box("scab_trim", (0.077, 0.03, 0.152), (s * 0.27, 0.15, -0.095), M_ORANGE, bevel=0.003, rot=rot))
        parts.append(box("scab_mount", (0.03, 0.05, 0.16), (s * 0.235, 0.02, -0.08), M_LEATHER, bevel=0.004))
    return join("SM_GrappleGear", parts, origin=(0, 0, 0))


# ------------------------------------------------------------------ 4. pistol
def make_pistol():
    """Compact sidearm. Barrel along +Y, origin at the grip."""
    parts = []
    parts.append(box("slide", (0.03, 0.19, 0.034), (0, 0.06, 0.06), M_DARK, bevel=0.003))
    # slide serrations
    for k in range(6):
        parts.append(box("serr", (0.031, 0.003, 0.026), (0, -0.02 + k * 0.007, 0.06), M_STEEL))
    parts.append(box("frame", (0.028, 0.17, 0.022), (0, 0.055, 0.032), M_GRIP, bevel=0.003))
    parts.append(cyl("barrel", 0.007, 0.03, (0, 0.165, 0.065), M_STEEL, rot=(math.radians(90), 0, 0)))
    grip = box("grip", (0.03, 0.05, 0.11), (0, -0.012, -0.03), M_GRIP, bevel=0.006,
               rot=(math.radians(-16), 0, 0))
    parts.append(grip)
    parts.append(box("mag", (0.022, 0.04, 0.012), (0, -0.03, -0.088), M_STEEL, bevel=0.002,
                     rot=(math.radians(-16), 0, 0)))
    guard = prim("torus", "guard", M_GRIP, major_radius=0.022, minor_radius=0.0035,
                 location=(0, 0.04, 0.012), rotation=(0, math.radians(90), 0),
                 major_segments=32, minor_segments=8)
    parts.append(guard)
    parts.append(box("trigger", (0.006, 0.006, 0.02), (0, 0.035, 0.012), M_STEEL, bevel=0.001))
    parts.append(box("sight_f", (0.004, 0.006, 0.006), (0, 0.15, 0.08), M_ORANGE))
    parts.append(box("sight_r", (0.016, 0.006, 0.006), (0, -0.02, 0.08), M_DARK))
    parts.append(box("accent", (0.031, 0.04, 0.004), (0, 0.11, 0.045), M_ORANGE))
    return join("SM_Pistol", parts, origin=(0, -0.012, -0.03))


assets = [make_blade(), make_handle(), make_gear(), make_pistol()]

# ------------------------------------------------------------------ export
for ob in assets:
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    loc = ob.location.copy()
    ob.location = (0, 0, 0)
    bpy.ops.export_scene.fbx(
        filepath=os.path.join(OUT, ob.name + ".fbx"),
        use_selection=True, apply_scale_options="FBX_SCALE_ALL",
        mesh_smooth_type="FACE", add_leaf_bones=False, bake_space_transform=True,
        axis_forward="-Z", axis_up="Y")
    ob.location = loc
    print("exported", ob.name, len(ob.data.polygons), "faces")

# ------------------------------------------------------------------ preview render
layout = {"SM_GrappleGear": ((0.0, 0.0, 0.55), (0, 0, math.radians(200)), 1.0),
          "SM_Blade": ((0.55, -0.35, 0.05), (math.radians(90), math.radians(-90), 0), 1.0),
          "SM_BladeHandle": ((0.47, -0.35, 0.05), (0, math.radians(90), 0), 1.0),
          "SM_Pistol": ((-0.75, -0.2, 0.45), (0, 0, math.radians(-70)), 2.0)}
for ob in assets:
    loc, rot, sc = layout[ob.name]
    ob.location, ob.rotation_euler, ob.scale = loc, rot, (sc, sc, sc)

floor = prim("plane", "floor", mat("M_Floor", (0.9, 0.9, 0.88), 0.0, 0.9), size=8, location=(0, 0, -0.05))
cam_data = bpy.data.cameras.new("cam")
cam = link(bpy.data.objects.new("cam", cam_data))
cam.location = (1.1, -2.7, 1.25)
cam.rotation_euler = (Vector((0, 0, 0.35)) - Vector(cam.location)).to_track_quat("-Z", "Y").to_euler()
cam_data.lens = 40
scene.camera = cam
for name, loc, energy, size in [("key", (2, -2, 3), 600, 2), ("fill", (-3, -1.5, 1.5), 200, 3),
                                 ("rim", (0, 2.5, 2.5), 400, 1.5)]:
    ld = bpy.data.lights.new(name, "AREA")
    ld.energy = energy
    ld.size = size
    lo = link(bpy.data.objects.new(name, ld))
    lo.location = loc
    lo.rotation_euler = (Vector((0, 0, 0.4)) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
world = bpy.data.worlds.new("w")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.92, 0.92, 0.9, 1)
world.node_tree.nodes["Background"].inputs[1].default_value = 0.6
scene.world = world
scene.render.engine = "CYCLES"
scene.cycles.device = "CPU"
scene.cycles.samples = 48
scene.render.resolution_x = 1600
scene.render.resolution_y = 900
scene.render.filepath = os.path.join(OUT, "weapons_preview.png")
bpy.ops.render.render(write_still=True)
bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "GrappleRunner_Weapons.blend"))
print("done")
