# =====================================================================================
#  make_agents.py : builds the ten Grapple Runner agents on our shared base body
# -------------------------------------------------------------------------------------
#  WHAT THIS IS
#  Every agent is the SAME semi-realistic base (make_base.py: MakeHuman CC0 body with our
#  "animated film hero" proportions). What changes per agent comes from the roster doc
#  ("Grapple Runner agents: roster", 22 Nov 2026): gender mix, build, ancestry, height,
#  skin tone, hair style and colour, and their ability colour on the suit.
#  No Final Fantasy (or any other game's) models are used.
#
#  HOW TO READ IT
#  1. AGENTS     one entry per agent: the numbers that make them look like themselves
#  2. body()     mixes the base-body shape targets for that agent and scales to height
#  3. hair()     grows hair out of the scalp: picks scalp faces with a per-style rule,
#                pushes them outwards, adds extra shapes (braid, knot, ponytail)
#  4. suit()     MakeHuman's skin-tight "tights" layer, tinted with the agent's colours
#  5. bottom     builds all ten in a line, renders a line-up and saves the .blend
#  First pass: bodies, skin, hair shapes, base suit. Jackets, faces and textures next.
# =====================================================================================
import bpy, bmesh, os, math, random
from mathutils import Vector
import make_base as mb
import agent_hair

OUT = mb.OUT


def lin(hexcol):
    """'#rrggbb' (what you'd pick in a paint program) -> Blender's linear colour."""
    c = [int(hexcol[i:i + 2], 16) / 255 for i in (1, 3, 5)]
    return tuple(x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c)


# ---- 1. the roster, as numbers -------------------------------------------------------
#  gender: list of (male/female, share). Non-binary agents sit between the two.
#  build: (muscle, weight) corner of MakeHuman's body space.
AGENTS = [
    dict(name="Orishi 'Relay' Osagie", gender=(("male", .55), ("female", .45)), build=("averagemuscle", "minweight"),
         anc=(("african", 1.0),), cm=185, skin="#5a3825", eye="#b5762a", hair="twists", hcol="#141010", tip="#a3a8b0", col="#39ff6a"),
    dict(name="Arnav 'Monsoon' Iyer", gender=(("male", 1.0),), build=("maxmuscle", "averageweight"),
         anc=(("caucasian", .55), ("african", .25), ("asian", .2)), cm=178, skin="#8d5a3b", eye="#3b2314", hair="knot", hcol="#120e0c", col="#14c8b4"),
    dict(name="Mateo 'Ashlar' Quispe", gender=(("male", 1.0),), build=("maxmuscle", "maxweight"),
         anc=(("asian", .55), ("caucasian", .45)), cm=170, skin="#a8714b", eye="#2a1a10", hair="undercut", hcol="#141110", col="#e8b23a"),
    dict(name="Saule 'Zenith' Bekova", gender=(("female", 1.0),), build=("averagemuscle", "minweight"),
         anc=(("asian", .8), ("caucasian", .2)), cm=168, skin="#b77f55", eye="#7d8f74", hair="braid", hcol="#151110", col="#ffb02e"),
    dict(name="Wanjiru 'Halo' Kamau", gender=(("female", 1.0),), build=("maxmuscle", "averageweight"),
         anc=(("african", 1.0),), cm=175, skin="#3e2618", eye="#9a7a3f", hair="afro", hcol="#16110e", col="#fff4dc"),
    dict(name="Hoa 'Lantern' Tran", gender=(("female", .6), ("male", .4)), build=("averagemuscle", "minweight"),
         anc=(("asian", 1.0),), cm=158, skin="#cf9c74", eye="#3a2516", hair="bob", hcol="#151012", tip="#8e1020", col="#e0203a"),
    dict(name="Noor 'Mirage' Haddad", gender=(("male", .5), ("female", .5)), build=("averagemuscle", "minweight"),
         anc=(("caucasian", 1.0),), cm=172, skin="#b8835e", eye="#8a9196", hair="swept", hcol="#231915", tip="#b49ad8", col="#9b5cff"),
    dict(name="Rowan 'Gale' MacLeod", gender=(("male", .6), ("female", .4)), build=("averagemuscle", "minweight"),
         anc=(("caucasian", 1.0),), cm=180, skin="#dcae90", eye="#9cc3dc", hair="shag", hcol="#a2441c", col="#8fd8ff"),
    dict(name="Ji-ho 'Rebound' Park", gender=(("male", 1.0),), build=("maxmuscle", "averageweight"),
         anc=(("asian", 1.0),), cm=174, skin="#d09e78", eye="#24170f", hair="twoblock", hcol="#121012", tip="#ff7a1a", col="#ff7a1a"),
    dict(name="Valentina 'Rigger' Cruz", gender=(("female", 1.0),), build=("averagemuscle", "minweight"),
         anc=(("caucasian", .6), ("asian", .4)), cm=183, skin="#b98058", eye="#5a3a22", hair="ponytail", hcol="#1b1310", tip="#d4207f", col="#e8248f"),
]


# ---- 2. the body ---------------------------------------------------------------------
def read_groups():
    """Faces of the body AND the tights layer (the tights become the base suit)."""
    verts, faces, group = [], {"body": [], "helper-tights": [], "joint-l-eye": [], "joint-r-eye": []}, None
    for line in open(os.path.join(mb.MH, "3dobjs", "base.obj")):
        if line.startswith("v "):
            verts.append([float(x) for x in line.split()[1:4]])
        elif line.startswith("g "):
            group = line.split()[1]
        elif line.startswith("f ") and group in faces:
            faces[group].append([int(p.split("/")[0]) - 1 for p in line.split()[1:]])
    return verts, faces


def body(a):
    verts, faces = read_groups()
    muscle, weight = a["build"]
    for g, gw in a["gender"]:
        mixes = [("universal-%s-young-%s-%s" % (g, muscle, weight), "macrodetails", gw),
                 ("%s-young-%s-%s-idealproportions" % (g, muscle, weight), "macrodetails/proportions", gw)]
        for an, aw in a["anc"]:
            mixes.append(("%s-%s-young" % (an, g), "macrodetails", gw * aw))
        for name, folder, w in mixes:
            for i, d in mb.read_target(name, folder).items():
                verts[i][0] += d[0] * w; verts[i][1] += d[1] * w; verts[i][2] += d[2] * w
    # MakeHuman (Y-up, decimetres) -> Blender (Z-up, metres)
    co = [Vector((v[0] / 10, -v[2] / 10, v[1] / 10)) for v in verts]
    # house proportions, worked out from the BODY only and applied to everything
    used = sorted({i for f in faces["body"] for i in f})
    zs = [co[i].z for i in used]
    bot, top = min(zs), max(zs); H = top - bot
    hip = bot + H * 0.48
    for c in co:
        if c.z < hip:
            c.z = hip - (hip - c.z) * 1.06       # legs 6% longer
    top = max(co[i].z for i in used)
    neck = top - H * 0.135
    for c in co:
        if c.z > neck:
            c.xyz = Vector((0, 0, neck)) + (c - Vector((0, 0, neck))) * 0.95   # head 5% smaller
    bot = min(co[i].z for i in used); top = max(co[i].z for i in used)
    s = (a["cm"] / 100.0) / (top - bot)          # scale to the agent's height
    co = [Vector((c.x * s, c.y * s, (c.z - bot) * s)) for c in co]
    return co, faces


def mesh_obj(name, co, faces, x):
    used = sorted({i for f in faces for i in f})
    remap = {old: n for n, old in enumerate(used)}
    me = bpy.data.meshes.new(name)
    me.from_pydata([co[i] for i in used], [], [[remap[i] for i in f] for f in faces])
    me.update()
    for p in me.polygons:
        p.use_smooth = True
    ob = bpy.data.objects.new(name, me)
    bpy.context.collection.objects.link(ob)
    ob.location.x = x
    return ob


def principled(name, colour, rough=0.5, sss=0.0, emit=None, strength=0.0):
    m = bpy.data.materials.new(name)
    b = m.node_tree.nodes.get("Principled BSDF") if m.node_tree else None
    if b is None:
        m.use_nodes = True; b = m.node_tree.nodes["Principled BSDF"]
    b.inputs["Base Color"].default_value = (*colour, 1)
    b.inputs["Roughness"].default_value = rough
    if sss:
        b.inputs["Subsurface Weight"].default_value = sss
        b.inputs["Subsurface Radius"].default_value = (1.0, 0.4, 0.25)
        b.inputs["Subsurface Scale"].default_value = 0.01
    if emit:
        b.inputs["Emission Color"].default_value = (*emit, 1)
        b.inputs["Emission Strength"].default_value = strength
    return m


# ---- 3. hair: see agent_hair.py (real strands)


# ---- 4. base suit --------------------------------------------------------------------
def suit(a, co, faces, x):
    """MakeHuman's skin-tight layer as a base undersuit: dark technical knit, with thin
    glowing seams down the sides in the agent's ability colour (jackets go on top later)."""
    s = mesh_obj(a["name"] + "_Suit", co, faces["helper-tights"], x)
    m = principled("Suit_" + a["name"], lin("#20232b"), 0.62)
    nt = m.node_tree; b = nt.nodes["Principled BSDF"]
    geo = nt.nodes.new("ShaderNodeNewGeometry"); sep = nt.nodes.new("ShaderNodeSeparateXYZ")
    ab = nt.nodes.new("ShaderNodeMath"); ab.operation = "ABSOLUTE"
    ramp = nt.nodes.new("ShaderNodeValToRGB")
    ramp.color_ramp.elements[0].position = 0.965; ramp.color_ramp.elements[1].position = 0.985
    nt.links.new(geo.outputs["Normal"], sep.inputs[0]); nt.links.new(sep.outputs["X"], ab.inputs[0])
    nt.links.new(ab.outputs[0], ramp.inputs["Fac"])
    mul = nt.nodes.new("ShaderNodeMath"); mul.operation = "MULTIPLY"; mul.inputs[1].default_value = 4.0
    nt.links.new(ramp.outputs["Color"], mul.inputs[0]); nt.links.new(mul.outputs[0], b.inputs["Emission Strength"])
    b.inputs["Emission Color"].default_value = (*lin(a["col"]), 1)
    # fine knit texture
    tex = nt.nodes.new("ShaderNodeTexNoise"); tex.inputs["Scale"].default_value = 900
    bump = nt.nodes.new("ShaderNodeBump"); bump.inputs["Strength"].default_value = 0.15
    nt.links.new(tex.outputs["Fac"], bump.inputs["Height"]); nt.links.new(bump.outputs["Normal"], b.inputs["Normal"])
    s.data.materials.append(m)
    push = s.modifiers.new("push", "DISPLACE"); push.strength = 0.004; push.mid_level = 0.0
    s.modifiers.new("smooth", "SUBSURF").levels = 1
    return s


def eyes(a, co, faces, x):
    """Eyeballs at MakeHuman's eye joints: white, iris in the agent's eye colour, pupil."""
    for side in ("joint-l-eye", "joint-r-eye"):
        idx = {i for f in faces[side] for i in f}
        c = sum((co[i] for i in idx), Vector()) / len(idx)
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.0118 * a["cm"] / 175, location=(c.x + x, c.y, c.z), segments=32, ring_count=16)
        e = bpy.context.active_object; bpy.ops.object.shade_smooth()
        m = bpy.data.materials.new("Eye_" + a["name"]); m.use_nodes = True; nt = m.node_tree
        b = nt.nodes["Principled BSDF"]; b.inputs["Roughness"].default_value = 0.05
        tc = nt.nodes.new("ShaderNodeTexCoord"); sp = nt.nodes.new("ShaderNodeSeparateXYZ")
        ramp = nt.nodes.new("ShaderNodeValToRGB"); cr = ramp.color_ramp
        cr.elements[0].position = 0.0; cr.elements[0].color = (0.01, 0.01, 0.01, 1)
        cr.elements[1].position = 0.12; cr.elements[1].color = (0.01, 0.01, 0.01, 1)
        e2 = cr.elements.new(0.14); e2.color = (*lin(a["eye"]), 1)
        e3 = cr.elements.new(0.30); e3.color = (*[v * 0.6 for v in lin(a["eye"])], 1)
        e4 = cr.elements.new(0.33); e4.color = (0.8, 0.78, 0.74, 1)
        # distance from the front of the eye: 0 at the pupil
        dist = nt.nodes.new("ShaderNodeVectorMath"); dist.operation = "DISTANCE"
        dist.inputs[1].default_value = (0, -1, 0)
        nt.links.new(tc.outputs["Normal"], dist.inputs[0]); nt.links.new(dist.outputs["Value"], ramp.inputs["Fac"])
        nt.links.new(ramp.outputs["Color"], b.inputs["Base Color"])
        e.data.materials.append(m)


# ---- 5. build the line-up ------------------------------------------------------------
if __name__ == "__main__":
    bpy.ops.wm.read_factory_settings(use_empty=True)
    sc = bpy.context.scene
    gap = 0.95
    for n, a in enumerate(AGENTS):
        x = (n - 4.5) * gap
        co, faces = body(a)
        ob = mesh_obj(a["name"], co, faces["body"], x)
        sk = principled("Skin_" + a["name"], lin(a["skin"]), 0.52, sss=0.3)
        nt = sk.node_tree; nz = nt.nodes.new("ShaderNodeTexNoise"); nz.inputs["Scale"].default_value = 2500
        bp = nt.nodes.new("ShaderNodeBump"); bp.inputs["Strength"].default_value = 0.08
        nt.links.new(nz.outputs["Fac"], bp.inputs["Height"]); nt.links.new(bp.outputs["Normal"], nt.nodes["Principled BSDF"].inputs["Normal"])
        ob.data.materials.append(sk)
        ob.modifiers.new("smooth", "SUBSURF").levels = 1
        hm = agent_hair.hair_material("Hair_" + a["name"], lin(a["hcol"]))
        tm = agent_hair.hair_material("Tint_" + a["name"], lin(a["tip"])) if a.get("tip") else None
        agent_hair.make(a, ob, hm, tm, seed=n + 7)
        suit(a, co, faces, x)
        eyes(a, co, faces, x)
        print("built", a["name"])
    # studio
    bpy.ops.mesh.primitive_plane_add(size=40)
    bpy.context.active_object.data.materials.append(principled("Floor", lin("#d8dbe0"), 0.8))
    sc.world = bpy.data.worlds.new("w"); sc.world.color = lin("#8a97a8")
    for name, e, loc, rot in (("key", 1100, (3, -6, 5), (50, 0, 25)), ("fill", 350, (-5, -5, 2.5), (70, 0, -45)),
                              ("rim", 900, (0, 4, 4), (-55, 0, 0))):
        ld = bpy.data.lights.new(name, "AREA"); ld.energy = e; ld.size = 4
        lo = bpy.data.objects.new(name, ld); sc.collection.objects.link(lo)
        lo.location = loc; lo.rotation_euler = [math.radians(r) for r in rot]
    cd = bpy.data.cameras.new("cam"); cd.lens = 32
    co_ = bpy.data.objects.new("cam", cd); sc.collection.objects.link(co_)
    co_.location = (0, -9.2, 1.05); co_.rotation_euler = (math.radians(89), 0, 0); sc.camera = co_
    sc.render.engine = "CYCLES"; sc.cycles.samples = 64; sc.cycles.device = "CPU"
    sc.view_settings.view_transform = "AgX"
    sc.render.resolution_x, sc.render.resolution_y = 3200, 1400
    sc.render.filepath = os.path.join(OUT, "agents_lineup.png")
    if not os.environ.get("HEADS_ONLY"):
        bpy.ops.render.render(write_still=True)
    from PIL import Image
    shots = []
    sc.render.resolution_x, sc.render.resolution_y = 560, 700; sc.cycles.samples = 48
    cd.lens = 85
    for n, a in enumerate(AGENTS):
        x = (n - 4.5) * gap
        ht = a["cm"] / 100.0
        co_.location = (x, -1.15, ht - 0.12); co_.rotation_euler = (math.radians(90), 0, 0)
        sc.render.filepath = os.path.join(OUT, "head_%02d.png" % n)
        bpy.ops.render.render(write_still=True); shots.append(sc.render.filepath)
    sheet = Image.new("RGB", (560 * 5, 700 * 2))
    for k, f in enumerate(shots):
        sheet.paste(Image.open(f).convert("RGB"), ((k % 5) * 560, (k // 5) * 700))
    sheet.save(os.path.join(OUT, "agents_heads.png"))
    bpy.ops.wm.save_as_mainfile(filepath=os.path.join(OUT, "GR_Agents.blend"))
    print("DONE")
