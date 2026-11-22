# =====================================================================================
#  make_base.py : builds Grapple Runner's own semi-realistic base bodies in Blender
# -------------------------------------------------------------------------------------
#  WHAT THIS IS
#  Every agent shares one original base body, so they all have the same semi-realistic
#  look (realistic anatomy, slightly idealised like a 3D anime film) without copying any
#  game's characters. The body comes from MakeHuman's base mesh and shape targets, which
#  are CC0 (public domain): free to use, change and sell, no credit needed.
#
#  HOW TO READ IT
#  1. read_obj()      loads the MakeHuman base mesh (body only, not the helper geometry)
#  2. read_target()   loads a "target": a list of vertex nudges that change the shape
#  3. build_body()    mixes targets (gender, build, ancestry, height) into one body
#  4. stylise()       our own semi-real touch: longer legs, slimmer waist, smaller head
#  5. the bottom part makes the bodies, renders a preview and saves the .blend
#
#  Run with Blender's Python: python3 make_base.py
# =====================================================================================
import bpy, os, math, sys
from mathutils import Vector

MH = "/home/claude/agents/src/mh/makehuman/data"
OUT = "/home/claude/agents/out"
os.makedirs(OUT, exist_ok=True)


def read_obj(path):
    """Read vertices and the faces of the 'body' group only (step 1)."""
    verts, faces, group = [], [], None
    for line in open(path):
        if line.startswith("v "):
            verts.append([float(x) for x in line.split()[1:4]])
        elif line.startswith("g "):
            group = line.split()[1]
        elif line.startswith("f ") and group == "body":
            faces.append([int(p.split("/")[0]) - 1 for p in line.split()[1:]])
    return verts, faces


def read_target(name, folder="macrodetails"):
    """A target file: each line is 'vertex-index dx dy dz' (step 2)."""
    path = os.path.join(MH, "targets", folder, name + ".target")
    out = {}
    for line in open(path):
        if line[:1].isdigit():
            i, x, y, z = line.split()
            out[int(i)] = (float(x), float(y), float(z))
    return out


def build_body(gender, muscle="averagemuscle", weight="averageweight",
               ancestry=(("caucasian", 1.0),), height=0.5):
    """Mix the shape targets into one list of vertex positions (step 3).
    gender: 'male' or 'female' (non-binary agents use a mix, e.g. 0.5 of each)."""
    verts, faces = read_obj(os.path.join(MH, "3dobjs", "base.obj"))
    genders = gender if isinstance(gender, tuple) else ((gender, 1.0),)
    for g, gw in genders:
        mixes = [("universal-%s-young-%s-%s" % (g, muscle, weight), "macrodetails", gw)]
        for a, aw in ancestry:   # ancestry shapes face and build a little
            mixes.append(("%s-%s-young" % (a, g), "macrodetails", gw * aw))
        mixes.append(("%s-young-%s-%s-maxheight" % (g, muscle, weight), "macrodetails/height", gw * height))
        mixes.append(("%s-young-%s-%s-idealproportions" % (g, muscle, weight), "macrodetails/proportions", gw * 1.0))
        for name, folder, w in mixes:
            for i, d in read_target(name, folder).items():
                verts[i][0] += d[0] * w; verts[i][1] += d[1] * w; verts[i][2] += d[2] * w
    return verts, faces


def stylise(obj):
    """Our house semi-real look (step 4): legs a touch longer, head a touch smaller,
    like an animated film hero rather than a photo scan. Gentle on purpose."""
    zs = [v.co.z for v in obj.data.vertices]
    top, bot = max(zs), min(zs)
    h = top - bot
    hip = bot + h * 0.48
    for v in obj.data.vertices:
        if v.co.z < hip:   # stretch below the hips by 6%
            v.co.z = hip - (hip - v.co.z) * 1.06
    zs = [v.co.z for v in obj.data.vertices]; top = max(zs)
    neck = top - h * 0.135
    centre = Vector((0, 0, neck))
    for v in obj.data.vertices:
        if v.co.z > neck:   # head 5% smaller, scaled from the neck
            v.co = centre + (v.co - centre) * 0.95


def make_object(name, verts, faces, x):
    me = bpy.data.meshes.new(name)
    # MakeHuman is Y-up in decimetres; Blender is Z-up in metres
    me.from_pydata([(v[0] / 10, -v[2] / 10, v[1] / 10) for v in verts], [], faces)
    me.update()
    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    stylise(ob)
    zs = [v.co.z for v in ob.data.vertices]
    for v in ob.data.vertices:   # feet on the floor
        v.co.z -= min(zs)
    ob.location.x = x
    for p in me.polygons:
        p.use_smooth = True
    ob.modifiers.new("smooth", "SUBSURF").levels = 1
    return ob


def skin(tone, name):
    m = bpy.data.materials.new(name); m.use_nodes = True
    b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*tone, 1)
    b.inputs["Roughness"].default_value = 0.45
    for key in ("Subsurface Weight", "Subsurface"):
        if key in b.inputs: b.inputs[key].default_value = 0.25; break
    if "Subsurface Radius" in b.inputs: b.inputs["Subsurface Radius"].default_value = (1.0, 0.35, 0.2)
    return m


if __name__ == "__main__":
    bpy.ops.wm.read_factory_settings(use_empty=True)
    base = os.path.join(MH, "3dobjs", "base.obj")
    bodies = [
        ("Base_Male",   build_body("male", "maxmuscle"),   -0.6, (0.62, 0.45, 0.35)),
        ("Base_Female", build_body("female", "averagemuscle"), 0.6, (0.62, 0.45, 0.35)),
    ]
    for name, (v, f), x, tone in bodies:
        ob = make_object(name, v, f, x)
        ob.data.materials.append(skin(tone, "Skin_" + name))
    # simple studio: floor, key light, camera
    bpy.ops.mesh.primitive_plane_add(size=20)
    sc = bpy.context.scene
    sc.world = bpy.data.worlds.new("w"); sc.world.color = (0.55, 0.6, 0.68)
    ld = bpy.data.lights.new("key", "AREA"); ld.energy = 900; ld.size = 3
    lo = bpy.data.objects.new("key", ld); sc.collection.objects.link(lo)
    lo.location = (2.5, -3.5, 3.5); lo.rotation_euler = (math.radians(55), 0, math.radians(35))
    cd = bpy.data.cameras.new("cam"); cd.lens = 60
    co = bpy.data.objects.new("cam", cd); sc.collection.objects.link(co)
    co.location = (0, -7.2, 1.0); co.rotation_euler = (math.radians(90), 0, 0)
    sc.camera = co
    sc.render.engine = "CYCLES"; sc.cycles.samples = 48; sc.cycles.device = "CPU"
    sc.render.resolution_x, sc.render.resolution_y = 1000, 1100
    sc.render.filepath = os.path.join(OUT, "base_bodies.png")
    bpy.ops.render.render(write_still=True)
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "GR_BaseBodies.blend"))
    print("DONE")
