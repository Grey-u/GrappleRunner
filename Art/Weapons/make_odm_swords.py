"""
Grapple Runner: the ODM sword (one model, used in both hands).

WHAT THIS MAKES
  SM_ODMSword.fbx - a trigger handle with a long, single-edged, snap-segment blade
  (the Attack on Titan style). Everything is our own geometry, so we own it fully.

  The model's ORIGIN (its 0,0,0 point) is the middle of the grip, where your fist closes.
  Three invisible marker points ("sockets") are saved inside the model so the game knows
  which way the blade points and which side is sharp, however Unreal turns it on import:
    SOCKET_Tip    the very end of the blade
    SOCKET_Base   where the blade comes out of the handle
    SOCKET_Edge   a point on the sharp edge, halfway up

Run headless:  blender -b -P make_odm_swords.py -- <out_dir>
or with the bpy module:  python make_odm_swords.py <out_dir>
Writes SM_ODMSword.fbx plus a preview picture (odm_sword_preview.png).

HOW TO READ THIS (no coding experience needed)
- Lines starting with # are notes for humans; the computer skips them.
- "def name(...):" defines a reusable recipe; the indented lines under it are its steps.
- Sizes are in metres here (Blender). Unreal turns them into centimetres on import.
- The blade is built pointing along +Y, with the sharp edge on the +X side.
"""
import math
import os
import sys

import bpy
from mathutils import Vector

argv = sys.argv
OUT = argv[argv.index("--") + 1] if "--" in argv else (argv[1] if len(argv) > 1 else ".")
os.makedirs(OUT, exist_ok=True)

# ------------------------------------------------------------------ 0. empty scene, metres
bpy.ops.wm.read_factory_settings(use_empty=True)
scene = bpy.context.scene
scene.unit_settings.system = "METRIC"
scene.unit_settings.scale_length = 1.0


# ------------------------------------------------------------------ 1. materials (colours)
def mat(name, color, metal=0.0, rough=0.5):
    """A simple surface: colour, how metallic, how rough (0 = mirror, 1 = chalk)."""
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*color, 1.0)
    b.inputs["Metallic"].default_value = metal
    b.inputs["Roughness"].default_value = rough
    return m


M_STEEL = mat("M_SwordSteel", (0.80, 0.82, 0.85), 1.0, 0.18)    # the blade's flat
M_EDGE = mat("M_SwordEdge", (0.97, 0.98, 1.00), 1.0, 0.05)      # the honed, shiny edge
M_DARK = mat("M_SwordDark", (0.05, 0.055, 0.06), 0.9, 0.4)      # gunmetal handle parts
M_GRIP = mat("M_SwordGrip", (0.035, 0.03, 0.028), 0.0, 0.8)     # rubbery grip
M_ACCENT = mat("M_SwordAccent", (1.0, 0.22, 0.04), 0.0, 0.4)    # orange stripe (Arcadium colour)
M_BRASS = mat("M_SwordBrass", (0.75, 0.55, 0.25), 1.0, 0.3)     # cable port


# ------------------------------------------------------------------ 2. small helpers
def link(obj):
    bpy.context.collection.objects.link(obj)
    return obj


def mesh_obj(name, verts, faces, material):
    """Build an object straight from a list of corner points and the faces between them."""
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


def bev(ob, width, segments=2):
    """Round off sharp corners a little so the metal catches the light."""
    m = ob.modifiers.new("Bevel", "BEVEL")
    m.width = width
    m.segments = segments
    m.limit_method = "ANGLE"
    m.angle_limit = math.radians(35)


def box(name, size, loc, material, bevel=0.0, rot=(0, 0, 0)):
    ob = prim("cube", name, material, size=1.0, location=loc, rotation=rot)
    ob.scale = size
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    if bevel:
        bev(ob, bevel)
    return ob


def cyl(name, r, depth, loc, material, rot=(math.radians(90), 0, 0), verts=32, bevel=0.0):
    """A cylinder lying along Y (the sword's length) unless told otherwise."""
    ob = prim("cylinder", name, material, radius=r, depth=depth, location=loc, rotation=rot, vertices=verts)
    if bevel:
        bev(ob, bevel)
    return ob


def smooth(ob, angle=35):
    """Smooth shading, but keep crisp edges sharper than `angle` degrees."""
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


def join(name, parts):
    """Apply the bevels, glue all parts into one model, unwrap it for textures."""
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
    scene.cursor.location = (0, 0, 0)            # origin = middle of the grip
    bpy.ops.object.origin_set(type="ORIGIN_CURSOR")
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=0.02)
    bpy.ops.object.mode_set(mode="OBJECT")
    smooth(ob)
    ob.select_set(False)
    return ob


# ------------------------------------------------------------------ 3. the measurements
GRIP_LEN = 0.125        # where your fist goes (a hand is about 9-10 cm wide, plus room)
GRIP_R = 0.0175         # grip radius: 3.5 cm thick, about a broom handle
HOUSING_Y = 0.085       # centre of the box the blade slots into
BLADE_Y0 = 0.115        # where the blade comes out of the housing
BLADE_LEN = 0.95        # 95 cm of blade
BLADE_W0 = 0.062        # width at the base...
BLADE_W1 = 0.050        # ...narrowing a little toward the tip
TIP_CUT = 0.11          # the angled box-cutter tip is this long on the edge side
T_SPINE = 0.0034        # half-thickness at the back of the blade
T_EDGE = 0.0004         # half-thickness at the sharp edge


# ------------------------------------------------------------------ 4. the blade
def make_blade():
    """The blade is a 'ribbon' of rows across its width. Each row has 4 points:
    spine, two inside points (so the bevel down to the edge is smooth), and the edge.
    The back (spine, x=0) is thick, the edge (x=width) is razor thin."""
    rows_n = 24
    cols = [0.0, 0.35, 0.7, 1.0]                     # across the width, 0 = spine, 1 = edge
    verts, faces = [], []
    for i in range(rows_n + 1):
        t = i / rows_n
        y_spine = BLADE_Y0 + BLADE_LEN * t
        w = BLADE_W0 + (BLADE_W1 - BLADE_W0) * t
        # The edge side stops TIP_CUT short of the spine: that makes the angled tip.
        y_edge = BLADE_Y0 + (BLADE_LEN - TIP_CUT) * t
        for c in cols:
            x = w * c
            y = y_spine + (y_edge - y_spine) * c
            th = T_SPINE if c < 0.35 else T_SPINE + (T_EDGE - T_SPINE) * (c - 0.35) / 0.65
            verts.append((x, y, th))
            verts.append((x, y, -th))
    nc = len(cols)

    def vi(r, c, side):
        return (r * nc + c) * 2 + side

    for r in range(rows_n):
        for c in range(nc - 1):
            for side in (0, 1):
                a, b = vi(r, c, side), vi(r, c + 1, side)
                cc, d = vi(r + 1, c + 1, side), vi(r + 1, c, side)
                faces.append((a, b, cc, d) if side == 0 else (d, cc, b, a))
        faces.append((vi(r, 0, 1), vi(r, 0, 0), vi(r + 1, 0, 0), vi(r + 1, 0, 1)))                      # spine wall
        faces.append((vi(r, nc - 1, 0), vi(r, nc - 1, 1), vi(r + 1, nc - 1, 1), vi(r + 1, nc - 1, 0)))  # edge wall
    for c in range(nc - 1):
        faces.append((vi(0, c, 1), vi(0, c + 1, 1), vi(0, c + 1, 0), vi(0, c, 0)))                      # base cap
        faces.append((vi(rows_n, c, 0), vi(rows_n, c + 1, 0), vi(rows_n, c + 1, 1), vi(rows_n, c, 1)))  # tip cap
    blade = mesh_obj("blade_body", verts, faces, M_STEEL)
    bpy.context.view_layer.objects.active = blade
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.normals_make_consistent(inside=False)
    bpy.ops.object.mode_set(mode="OBJECT")
    parts = [blade]

    # Snap-segment score lines: the blade is made of segments you snap off when blunt.
    for k in range(1, 9):
        y = BLADE_Y0 + 0.105 * k
        w = BLADE_W0 + (BLADE_W1 - BLADE_W0) * (0.105 * k / BLADE_LEN)
        for s in (1, -1):
            parts.append(box("score", (w * 1.05, 0.0018, 0.0006), (w / 2, y, s * (T_SPINE + 0.00005)),
                             M_DARK, rot=(0, 0, math.radians(-30))))
    # A bright honed strip along the whole edge.
    edge_len = BLADE_LEN - TIP_CUT
    parts.append(box("edge", (0.005, edge_len, 0.0011),
                     ((BLADE_W0 + BLADE_W1) / 2 - 0.0035, BLADE_Y0 + edge_len / 2, 0), M_EDGE))
    # A thin raised spine bar (makes the back of the blade read clearly in VR).
    parts.append(box("spine", (0.004, BLADE_LEN * 0.97, 0.0075), (0.002, BLADE_Y0 + BLADE_LEN * 0.485, 0),
                     M_STEEL, bevel=0.0012))
    return parts


# ------------------------------------------------------------------ 5. the handle
def make_handle():
    """Grip in the middle (origin), housing above it, pommel and cable port below."""
    parts = []
    parts.append(cyl("grip", GRIP_R, GRIP_LEN, (0, 0, 0), M_GRIP, bevel=0.003))
    for k in range(5):                                                   # finger rings
        parts.append(cyl("ring", GRIP_R + 0.0012, 0.003, (0, -0.05 + k * 0.025, 0), M_DARK))
    # Guard plate between grip and housing
    parts.append(box("guard", (0.08, 0.008, 0.05), (0.012, GRIP_LEN / 2 + 0.004, 0), M_DARK, bevel=0.003))
    # Blade housing with the orange stripe
    parts.append(box("housing", (0.078, 0.052, 0.042), (0.016, HOUSING_Y, 0), M_DARK, bevel=0.006))
    parts.append(box("accent", (0.08, 0.01, 0.044), (0.016, HOUSING_Y + 0.012, 0), M_ACCENT, bevel=0.002))
    # Two trigger levers on the front of the grip (edge side): they fire the ODM hooks
    for k, y in enumerate((0.025, -0.005)):
        parts.append(box("trigger", (0.014, 0.022, 0.01), (GRIP_R + 0.009, y, 0), M_DARK if k else M_ACCENT,
                         bevel=0.003, rot=(0, 0, math.radians(-12))))
    # Finger loop that protects the triggers
    loop = prim("torus", "loop", M_DARK, major_radius=0.032, minor_radius=0.0035,
                location=(GRIP_R + 0.002, 0.01, 0), rotation=(0, math.radians(90), 0))
    loop.scale = (1.0, 1.0, 0.75)
    bpy.ops.object.transform_apply(location=False, rotation=False, scale=True)
    parts.append(loop)
    # Pommel and the brass port where the gas cable plugs in
    parts.append(cyl("pommel", GRIP_R + 0.004, 0.02, (0, -GRIP_LEN / 2 - 0.01, 0), M_DARK, bevel=0.004))
    parts.append(cyl("port", 0.007, 0.03, (0, -GRIP_LEN / 2 - 0.035, 0), M_BRASS))
    return parts


sword = join("SM_ODMSword", make_blade() + make_handle())


# ------------------------------------------------------------------ 6. sockets (marker points)
def socket(name, loc):
    e = link(bpy.data.objects.new(name, None))
    e.empty_display_size = 0.02
    e.location = loc
    e.parent = sword
    return e


w_mid = (BLADE_W0 + BLADE_W1) / 2
sockets = [
    socket("SOCKET_Tip", (0.0, BLADE_Y0 + BLADE_LEN, 0.0)),
    socket("SOCKET_Base", (0.0, BLADE_Y0, 0.0)),
    socket("SOCKET_Edge", (w_mid, BLADE_Y0 + BLADE_LEN * 0.5, 0.0)),
]

# ------------------------------------------------------------------ 7. export
bpy.ops.object.select_all(action="DESELECT")
sword.select_set(True)
for s in sockets:
    s.select_set(True)
bpy.context.view_layer.objects.active = sword
bpy.ops.export_scene.fbx(
    filepath=os.path.join(OUT, "SM_ODMSword.fbx"),
    use_selection=True, apply_scale_options="FBX_SCALE_ALL",
    mesh_smooth_type="FACE", add_leaf_bones=False, bake_space_transform=True,
    axis_forward="-Z", axis_up="Y")
print("exported SM_ODMSword", len(sword.data.polygons), "faces")

# ------------------------------------------------------------------ 8. preview picture
sword.location = (0, 0, 0.3)
sword.rotation_euler = (0, math.radians(90), math.radians(-60))
twin = sword.copy()
twin.data = sword.data
link(twin)
twin.location = (0.25, 0.25, 0.3)
twin.rotation_euler = (0, math.radians(90), math.radians(-40))
floor = prim("plane", "floor", mat("M_Floor", (0.9, 0.9, 0.88), 0.0, 0.9), size=8, location=(0, 0, 0.0))
cam_data = bpy.data.cameras.new("cam")
cam = link(bpy.data.objects.new("cam", cam_data))
cam.location = (0.2, -1.6, 1.3)
cam.rotation_euler = (Vector((0.15, 0.1, 0.25)) - Vector(cam.location)).to_track_quat("-Z", "Y").to_euler()
cam_data.lens = 45
scene.camera = cam
for name, loc, energy, size in [("key", (2, -2, 3), 600, 2), ("fill", (-3, -1.5, 1.5), 200, 3),
                                 ("rim", (0, 2.5, 2.5), 400, 1.5)]:
    ld = bpy.data.lights.new(name, "AREA")
    ld.energy = energy
    ld.size = size
    lo = link(bpy.data.objects.new(name, ld))
    lo.location = loc
    lo.rotation_euler = (Vector((0, 0, 0.3)) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
world = bpy.data.worlds.new("w")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.92, 0.92, 0.9, 1)
world.node_tree.nodes["Background"].inputs[1].default_value = 0.6
scene.world = world
scene.render.engine = "CYCLES"
scene.cycles.device = "CPU"
scene.cycles.samples = 32
scene.render.resolution_x = 1400
scene.render.resolution_y = 800
scene.render.filepath = os.path.join(OUT, "odm_sword_preview.png")
bpy.ops.render.render(write_still=True)
print("done")
