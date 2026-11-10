"""
Grapple Runner: a light vine mesh for GRVine (one straight 1 m length that the game bends).

Run:  python make_vine.py <out_dir>   (with the bpy module)   or   blender -b -P make_vine.py -- <out_dir>
Writes SM_VineStem.fbx.

Why our own mesh (10 Nov): the vine mesh from the Brushify pack is heavy, and every vine bends
5-8 copies of it. Hundreds of vines made Unreal run out of memory twice. This one is about
1,500 triangles: a slightly twisting stem with small leaves along it.

The mesh runs along +X from 0 to 1 m, which is how a "spline mesh" expects it: the game
stretches each copy between two points on the hanging curve.
Two colour slots: VineStem (bark) and VineLeaf (leaves); the import script gives them materials.

HOW TO READ THIS (no coding experience needed)
- Lines starting with # are notes for humans. "for" repeats for each item.
- Sizes are metres (Unreal turns them into centimetres).
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

LENGTH = 1.0          # metres along X
RINGS = 16            # rings along the stem
SIDES = 7             # sides round the stem
RADIUS = 0.022        # 2.2 cm thick stem
LEAVES = 36           # leaves along the metre


def material(name, rgb):
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    m.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (*rgb, 1)
    return m


bm = bmesh.new()
uv = bm.loops.layers.uv.new("UVMap")

# ---- the stem: a tube that wobbles and twists a little, so it doesn't look like a pipe
rings = []
for i in range(RINGS + 1):
    t = i / RINGS
    x = t * LENGTH
    wobble = Vector((0, 0.006 * math.sin(t * 9.0), 0.006 * math.cos(t * 7.0)))
    r = RADIUS * (1.0 + 0.15 * math.sin(t * 23.0))
    ring = []
    for k in range(SIDES):
        a = 2 * math.pi * k / SIDES + t * 1.2                  # the twist
        ring.append(bm.verts.new(Vector((x, r * math.cos(a), r * math.sin(a))) + wobble))
    rings.append(ring)
for i in range(RINGS):
    for k in range(SIDES):
        f = bm.faces.new((rings[i][k], rings[i][(k + 1) % SIDES], rings[i + 1][(k + 1) % SIDES], rings[i + 1][k]))
        f.material_index = 0
        for loop, (u, v) in zip(f.loops, ((k / SIDES, i / RINGS), ((k + 1) / SIDES, i / RINGS),
                                          ((k + 1) / SIDES, (i + 1) / RINGS), (k / SIDES, (i + 1) / RINGS))):
            loop[uv].uv = (u, v * 4)

# ---- leaves: small pointed leaves sticking out at different angles all along the stem
for n in range(LEAVES):
    t = (n + 0.5) / LEAVES
    base = Vector((t * LENGTH, 0, 0))
    ang = n * 2.4                                          # the golden angle spreads them evenly round
    out = Vector((0.25, math.cos(ang), math.sin(ang))).normalized()
    side = out.cross(Vector((1, 0, 0))).normalized()
    size = 0.07 + 0.035 * math.sin(n * 1.7)
    tip = base + out * size * 1.6
    pts = [base + out * RADIUS, base + out * size * 0.6 + side * size * 0.45, tip,
           base + out * size * 0.6 - side * size * 0.45]
    vs = [bm.verts.new(p) for p in pts]
    f = bm.faces.new(vs)                                   # one face; its material is two-sided in Unreal
    f.material_index = 1
    for loop, c in zip(f.loops, ((0.5, 0), (1, 0.5), (0.5, 1), (0, 0.5))):
        loop[uv].uv = c

me = bpy.data.meshes.new("SM_VineStem")
bm.normal_update()
bm.to_mesh(me)
bm.free()
ob = bpy.data.objects.new("SM_VineStem", me)
bpy.context.collection.objects.link(ob)
me.materials.append(material("VineStem", (0.10, 0.08, 0.04)))
me.materials.append(material("VineLeaf", (0.12, 0.35, 0.08)))
bpy.context.view_layer.objects.active = ob
ob.select_set(True)
bpy.ops.object.shade_smooth()
bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, "SM_VineStem.fbx"), use_selection=True,
                         apply_scale_options="FBX_SCALE_ALL", mesh_smooth_type="FACE",
                         add_leaf_bones=False, bake_space_transform=True, axis_forward="-Z", axis_up="Y")
print("exported SM_VineStem", sum(len(p.vertices) - 2 for p in me.polygons), "triangles")
