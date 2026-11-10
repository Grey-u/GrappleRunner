"""
Grapple Runner: turn the downloaded ODM-gear model into game pieces (harness v3).

SOURCE MODEL (not ours: kept OUT of the public repo, see .gitignore)
- "SNK 3D maneuver gear" from Sketchfab; texture signature "@besugochan".
  Downloaded by Naomi as snk-3d-maneuver-gear.zip (OBJ + 4K PBR textures).
  Licence: check and record on the Sketchfab page before any public release, and credit the
  author in the game's credits.

WHAT THIS SCRIPT DOES
1. Loads the OBJ (one mesh of 141 loose parts) and splits it by part:
     - the GEAR: gas/reel unit, X-struts, blade boxes, gas tanks  -> SM_ODM_Gear
     - the two SWORDS (handle + blade, without their cables)     -> SM_ODM_Sword_L / _R
     - the cables are dropped: the game draws its own ropes.
2. Moves each piece's origin to where the game attaches it:
     gear  -> the hip bone (pelvis): reel unit sits at the small of the back
     sword -> the middle of its grip (for the hand, later)
3. Exports FBX files (metres -> Unreal centimetres, same settings as our own meshes) and a
   preview render of the gear together with our climbing-harness straps.

HOW TO RUN
    python make_odm_gear.py <folder with obj/ and textures/> <out folder> [<straps folder>]

HOW TO READ THIS (no coding experience needed)
- Lines starting with # are notes for humans. "def" = a recipe; "for" = repeat for each item.
- Directions (Blender): -Y = the wearer's forward, +Z = up, X = sideways. Sizes in metres.
"""
import os
import sys

import bpy
from mathutils import Vector

args = [a for a in sys.argv[sys.argv.index("--") + 1:]] if "--" in sys.argv else sys.argv[1:]
SRC, OUT = args[0], args[1]
STRAPS = args[2] if len(args) > 2 else None
os.makedirs(OUT, exist_ok=True)

# Where the hip bone sits inside the gear model (found from renders: the reel unit's front face
# is at y = -0.13 m, so the body's hip centre is ~9 cm in front of it, level with the belt).
GEAR_ORIGIN = Vector((0.0, -0.22, 0.98))

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.wm.obj_import(filepath=os.path.join(SRC, "obj", "snk_obj.obj"), forward_axis="NEGATIVE_Z", up_axis="Y")
whole = bpy.context.selected_objects[0]
whole.scale = (0.01, 0.01, 0.01)                       # the OBJ is in centimetres
bpy.ops.object.transform_apply(scale=True)
bpy.context.view_layer.objects.active = whole
bpy.ops.object.mode_set(mode="EDIT")
bpy.ops.mesh.select_all(action="SELECT")
bpy.ops.mesh.separate(type="LOOSE")                    # one object per loose part
bpy.ops.object.mode_set(mode="OBJECT")

# One material, named so Unreal's import script can find the textures for it.
mat = bpy.data.materials.new("M_ODMGear")
for o in [o for o in bpy.data.objects if o.type == "MESH"]:
    o.data.materials.clear()
    o.data.materials.append(mat)


def bounds(o):
    pts = [o.matrix_world @ Vector(c) for c in o.bound_box]
    return (Vector([min(p[i] for p in pts) for i in range(3)]),
            Vector([max(p[i] for p in pts) for i in range(3)]))


gear, sword_l, sword_r, cables = [], [], [], []
for o in [o for o in bpy.data.objects if o.type == "MESH"]:
    lo, hi = bounds(o)
    if hi.y < -0.3:                       # in front of the body, by the hands: sword parts
        (sword_l if lo.x + hi.x > 0 else sword_r).append(o)   # +X = the wearer's left
    elif lo.z < 0.65 and hi.y < -0.1:     # hangs down from hand to box: a cable
        cables.append(o)
    else:
        gear.append(o)
print("gear parts", len(gear), "| left sword", len(sword_l), "| right sword", len(sword_r), "| cables", len(cables))
for o in cables:
    bpy.data.objects.remove(o)


def merge(name, objs, origin):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    ob = bpy.context.active_object
    ob.name = ob.data.name = name
    bpy.context.scene.cursor.location = origin
    bpy.ops.object.origin_set(type="ORIGIN_CURSOR")
    ob.location = (0, 0, 0)                              # origin is now the attach point
    bpy.ops.object.shade_smooth()
    return ob


def grip_centre(objs):
    """The handle is the chunky part near the bottom of the sword (above the cable)."""
    best = max(objs, key=lambda o: len(o.data.polygons) if bounds(o)[1].z < 1.3 else 0)
    lo, hi = bounds(best)
    return (lo + hi) / 2


assets = [merge("SM_ODM_Gear", gear, GEAR_ORIGIN)]
if sword_l:
    assets.append(merge("SM_ODM_Sword_L", sword_l, grip_centre(sword_l)))
if sword_r:
    assets.append(merge("SM_ODM_Sword_R", sword_r, grip_centre(sword_r)))

for ob in assets:
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, ob.name + ".fbx"), use_selection=True,
                             apply_scale_options="FBX_SCALE_ALL", mesh_smooth_type="FACE",
                             add_leaf_bones=False, bake_space_transform=True,
                             axis_forward="-Z", axis_up="Y")
    print("exported", ob.name, len(ob.data.polygons), "faces")

# ------------------------------------------------------------------ preview: gear + our straps
mat.use_nodes = True
nt = mat.node_tree
bsdf = nt.nodes["Principled BSDF"]


def tex(name, colour=False):
    n = nt.nodes.new("ShaderNodeTexImage")
    n.image = bpy.data.images.load(os.path.join(SRC, "textures", "snk_texture_%s.jpg" % name))
    if not colour:
        n.image.colorspace_settings.name = "Non-Color"
    return n


nt.links.new(tex("BaseColor", True).outputs[0], bsdf.inputs["Base Color"])
nt.links.new(tex("Metallic").outputs[0], bsdf.inputs["Metallic"])
nt.links.new(tex("Roughness").outputs[0], bsdf.inputs["Roughness"])
nmap = nt.nodes.new("ShaderNodeNormalMap")
nt.links.new(tex("Normal").outputs[0], nmap.inputs["Color"])
nt.links.new(nmap.outputs[0], bsdf.inputs["Normal"])
for ob in assets[1:]:
    ob.hide_render = True

scene = bpy.context.scene
gear_ob = assets[0]
gear_ob.location = (0, 0, 1.0)
if STRAPS:
    for n, at in (("SM_Harness_Waist", (0, 0, 1.0)), ("SM_Harness_ThighA", (0.09, 0, 0.94)),
                  ("SM_Harness_ThighB", (-0.09, 0, 0.94))):
        f = os.path.join(STRAPS, n + ".fbx")
        if os.path.exists(f):
            bpy.ops.import_scene.fbx(filepath=f, axis_forward="-Z", axis_up="Y")
            for o in bpy.context.selected_objects:
                o.location = at
ghost = bpy.data.materials.new("ghost")
ghost.use_nodes = True
ghost.node_tree.nodes["Principled BSDF"].inputs["Base Color"].default_value = (0.42, 0.44, 0.47, 1)
ghost.node_tree.nodes["Principled BSDF"].inputs["Roughness"].default_value = 0.8
bpy.ops.mesh.primitive_cylinder_add(radius=0.15, depth=0.55, location=(0, 0, 1.25), vertices=40)
bpy.context.active_object.scale = (1.12, 0.85, 1)
bpy.context.active_object.data.materials.append(ghost)
for sx in (1, -1):
    bpy.ops.mesh.primitive_cylinder_add(radius=0.075, depth=0.5, location=(sx * 0.095, 0, 0.72), vertices=32)
    bpy.context.active_object.data.materials.append(ghost)
cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam"))
scene.collection.objects.link(cam)
scene.camera = cam
cam.data.lens = 45
ctr = Vector((0, 0.1, 0.98))
for nm, loc, e in (("key", (2, -2, 3), 600), ("fill", (-3, -1.5, 1.5), 220), ("rim", (0, 2.5, 2.5), 450)):
    light = bpy.data.lights.new(nm, "AREA")
    light.energy, light.size = e, 2
    lo_ = bpy.data.objects.new(nm, light)
    scene.collection.objects.link(lo_)
    lo_.location = Vector(loc) + ctr
    lo_.rotation_euler = (ctr - lo_.location).to_track_quat("-Z", "Y").to_euler()
world = bpy.data.worlds.new("w")
world.use_nodes = True
world.node_tree.nodes["Background"].inputs[0].default_value = (0.55, 0.57, 0.6, 1)
scene.world = world
scene.render.engine = "CYCLES"
scene.cycles.samples = 32
scene.render.resolution_x, scene.render.resolution_y = 1400, 1000
for view, d in (("front", (0.6, -1, 0.35)), ("back", (-0.7, 1, 0.45)), ("side", (1, 0.05, 0.12))):
    cam.location = ctr + Vector(d).normalized() * 1.9
    cam.rotation_euler = (ctr - cam.location).to_track_quat("-Z", "Y").to_euler()
    scene.render.filepath = os.path.join(OUT, "harness_v3_%s.png" % view)
    bpy.ops.render.render(write_still=True)
print("done")
