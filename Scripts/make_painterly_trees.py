# =====================================================================================
#  make_painterly_trees.py  -  grows Grapple Runner's painterly anime trees and bushes (Blender)
# =====================================================================================
#  python Scripts/make_painterly_trees.py            -> Art/Stylized/SM_GR_Tree_*.fbx, SM_GR_LeafBush_*.fbx
#  python Scripts/make_painterly_trees.py --preview  -> also renders Docs/trees_preview.png
#
#  HOW A TREE IS BUILT (Grey's reference: a painted anime tree, lime tops, teal shadows)
#  1. TRUNK AND BRANCHES: a trunk that splits again and again (3 levels), each branch a bent,
#     tapering tube with 10 sides. Bark colour is stored per vertex (darker near the ground).
#  2. LEAF CLUMPS: at the branch tips, round clumps made of ~100 flat "cards" each, every card
#     showing a painted leaf clump from T_GR_LeafBrush.png (make_leaf_texture.py). Cards near the
#     outside face outwards, so from any side you see leaves, not edges.
#  3. THE TRICK THAT MAKES IT LOOK PAINTED: each card's NORMAL (the direction lighting thinks it
#     faces) is bent to point out from the middle of its clump and of the whole crown, as if
#     the crown were a few big soft balls. So the light falls on the canopy in big simple
#     shapes: lime where the sun hits, teal-blue in the shade, like a painting, not noisy like
#     thousands of separate leaves.
#  4. VERTEX COLOUR: red = how deep inside the crown (darker inside, like ambient occlusion),
#     alpha = how much it sways in the wind (0 at the trunk base, 1 at the crown's edge).
#  Sizes in metres (they arrive in Unreal in centimetres). Same seed = same tree every time.
# =====================================================================================
import bpy, bmesh, math, random, os, sys
from mathutils import Vector, Matrix, Quaternion

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "Art", "Stylized")
TEX = os.path.join(OUT, "Textures", "T_GR_LeafBrush.png")
PREVIEW = "--preview" in sys.argv

BARK_LOW, BARK_HIGH = (0.36, 0.30, 0.28), (0.56, 0.49, 0.45)

def clear():
    bpy.ops.wm.read_factory_settings(use_empty=True)

class Part:
    """Collects vertices / faces / UVs / colours / custom normals for one mesh."""
    def __init__(self): self.v, self.f, self.uv, self.col, self.nrm = [], [], [], [], []
    def add_face(self, idx, uvs, cols, nrms):
        self.f.append(idx); self.uv.append(uvs); self.col.append(cols); self.nrm.append(nrms)

def build_object(name, bark, leaves, mats):
    """Join bark + leaves into one mesh with two material slots, custom normals and colours."""
    me = bpy.data.meshes.new(name)
    verts, faces, uvs, cols, nrms, mids = [], [], [], [], [], []
    for slot, part in enumerate((bark, leaves)):
        base = len(verts)
        verts += [tuple(p) for p in part.v]
        for f, u, c, n in zip(part.f, part.uv, part.col, part.nrm):
            faces.append([base + i for i in f]); uvs.append(u); cols.append(c); nrms.append(n); mids.append(slot)
    me.from_pydata(verts, [], faces)
    me.update()
    uvl = me.uv_layers.new(name="UVMap")
    ca = me.color_attributes.new("Col", 'BYTE_COLOR', 'CORNER')
    loop_normals = [None] * len(me.loops)
    for pi, poly in enumerate(me.polygons):
        poly.material_index = mids[pi]
        poly.use_smooth = True
        for k, li in enumerate(poly.loop_indices):
            uvl.data[li].uv = uvs[pi][k]
            ca.data[li].color = cols[pi][k]
            loop_normals[li] = nrms[pi][k]
    me.normals_split_custom_set(loop_normals)
    for m in mats:
        me.materials.append(m)
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    return ob

# ---------------------------------------------------------------- 1. trunk and branches
def tube(part, pts, radii, ground_z, top_z, sides=10):
    """A smooth tapering tube through the points."""
    rings = []
    for i, p in enumerate(pts):
        d = (pts[min(i + 1, len(pts) - 1)] - pts[max(i - 1, 0)]).normalized()
        side = d.orthogonal().normalized(); up = d.cross(side)
        ring = []
        for s in range(sides):
            a = 2 * math.pi * s / sides
            n = (side * math.cos(a) + up * math.sin(a)).normalized()
            ring.append((len(part.v), n))
            part.v.append(p + n * radii[i])
        rings.append(ring)
    for i in range(len(rings) - 1):
        for s in range(sides):
            a, b = rings[i][s], rings[i][(s + 1) % sides]
            c, d = rings[i + 1][(s + 1) % sides], rings[i + 1][s]
            idx = [a[0], b[0], c[0], d[0]]
            cl = []
            for vi in idx:
                t = max(0.0, min(1.0, (part.v[vi].z - ground_z) / max(0.1, top_z - ground_z)))
                c3 = tuple(BARK_LOW[k] + (BARK_HIGH[k] - BARK_LOW[k]) * t for k in range(3))
                cl.append(c3 + (t * 0.3,))
            u0, u1 = s / sides, (s + 1) / sides
            part.add_face(idx, [(u0, i * 0.25), (u1, i * 0.25), (u1, (i + 1) * 0.25), (u0, (i + 1) * 0.25)], cl,
                          [a[1], b[1], c[1], d[1]])

def grow(rng, part, start, direction, length, radius, depth, max_depth, tips, ground_z, top_z, spread):
    """One branch, then its children. Returns nothing; collects clump positions in tips."""
    steps = 5
    pts, radii = [start], [radius]
    d = direction.normalized()
    p = start.copy()
    for i in range(1, steps + 1):
        bend = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-0.3, 0.6))) * 0.18
        d = (d + bend).normalized()
        p = p + d * (length / steps)
        pts.append(p.copy()); radii.append(radius * (1 - 0.45 * i / steps))
    tube(part, pts, radii, ground_z, top_z)
    end_r = radii[-1]
    if depth >= max_depth:
        tips.append((p, d))
        return
    kids = rng.choice([2, 2, 3]) if depth > 0 else rng.choice([3, 3, 4])
    for k in range(kids):
        yaw = 2 * math.pi * k / kids + rng.uniform(-0.5, 0.5)
        tilt = math.radians(rng.uniform(*spread))
        side = Vector((math.cos(yaw), math.sin(yaw), 0))
        nd = (d * math.cos(tilt) + side * math.sin(tilt)).normalized()
        nd.z = max(nd.z, 0.15)
        grow(rng, part, p - d * 0.05, nd, length * rng.uniform(0.62, 0.78), end_r * 1.05, depth + 1, max_depth,
             tips, ground_z, top_z, spread)
    if depth >= max_depth - 1 and rng.random() < 0.5:
        tips.append((p, d))   # a clump right at the fork too, so the crown is full

# ---------------------------------------------------------------- 2. leaf clumps
def clump(rng, part, centre, radius, crown_centre, crown_radius, ground_z, height, cards):
    for i in range(cards):
        u = Vector((rng.gauss(0, 1), rng.gauss(0, 1), rng.gauss(0, 1) * 0.8)).normalized()
        r = radius * (0.6 + 0.4 * rng.random() ** 0.5)           # on the outside of the clump (its 'skin')
        pos = centre + u * r
        size = rng.uniform(0.8, 1.2) * radius * 0.75
        # card faces outward, with some randomness
        facing = (u + Vector((rng.uniform(-.6, .6), rng.uniform(-.6, .6), rng.uniform(-.4, .6)))).normalized()
        tang = facing.orthogonal().normalized()
        tang = (Quaternion(facing, rng.uniform(0, 2 * math.pi)) @ tang).normalized()
        bit = facing.cross(tang)
        corners = [pos + (tang * sx + bit * sy) * size * 0.5 for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        q = rng.randrange(4); ou, ov = (q % 2) * 0.5, (q // 2) * 0.5
        uvs = [(ou, ov), (ou + 0.5, ov), (ou + 0.5, ov + 0.5), (ou, ov + 0.5)]
        idx, cols, nrms = [], [], []
        for c in corners:
            idx.append(len(part.v)); part.v.append(c)
            # the painted-look normal: out from the clump and the crown, a little upward
            n = ((pos - centre).normalized() * 0.85 + (c - crown_centre).normalized() * 0.15 + Vector((0, 0, 0.05))).normalized()
            nrms.append(n)
            depth = max(0.0, min(1.0, (c - crown_centre).length / crown_radius))
            under = max(0.0, min(1.0, 0.5 - (c - centre).z / (radius * 2)))   # 1 under the puff, 0 on top
            ao = (0.45 + 0.55 * depth ** 0.8) * (1.0 - 0.25 * under)   # darker deep inside and underneath
            sway = max(0.0, min(1.0, (c.z - ground_z) / height)) * (0.4 + 0.6 * depth)
            cols.append((ao, ao, ao, sway))
        part.add_face(idx, uvs, cols, nrms)

def make_tree(name, seed, height, trunk_r, levels, clump_r, cards, spread=(28, 48)):
    rng = random.Random(seed)
    bark, leaves = Part(), Part()
    tips = []
    grow(rng, bark, Vector((0, 0, -0.1)), Vector((rng.uniform(-.1, .1), rng.uniform(-.1, .1), 1)), height * 0.42,
         trunk_r, 0, levels, tips, 0.0, height, spread)
    crown = sum((t[0] for t in tips), Vector()) / len(tips)
    crown_r = max((t[0] - crown).length for t in tips) + clump_r
    for p, d in tips:
        c = p + d * clump_r * 0.35 + Vector((0, 0, clump_r * 0.15))
        clump(rng, leaves, c, clump_r * rng.uniform(0.85, 1.15), crown, crown_r, 0.0, height, cards)
        for extra in range(rng.choice([1, 1, 2])):   # smaller puffs beside it: a lumpy crown with gaps
            off = Vector((rng.uniform(-1, 1), rng.uniform(-1, 1), rng.uniform(-0.6, 0.3))).normalized() * clump_r * 1.1
            clump(rng, leaves, c + off, clump_r * rng.uniform(0.5, 0.7), crown, crown_r, 0.0, height, int(cards * 0.5))
    return bark, leaves

def make_bush(name, seed, radius, clumps, cards):
    rng = random.Random(seed)
    bark, leaves = Part(), Part()
    centres = []
    for i in range(clumps):
        a = rng.uniform(0, 2 * math.pi); d = radius * 0.55 * math.sqrt(rng.random())
        c = Vector((math.cos(a) * d, math.sin(a) * d, radius * rng.uniform(0.45, 0.85)))
        centres.append(c)
        tube(bark, [Vector((c.x * 0.3, c.y * 0.3, -0.05)), c * 0.8], [0.04, 0.02], 0.0, radius)
    crown = sum(centres, Vector()) / len(centres)
    for c in centres:
        clump(rng, leaves, c, radius * 0.55, crown, radius * 1.1, 0.0, radius * 1.5, cards)
    return bark, leaves

# ---------------------------------------------------------------- materials (Blender preview)
def toon_material(name, lit, shadow, textured):
    """Preview only: two-tone light from the (custom) normals, like the game's look."""
    m = bpy.data.materials.new(name); m.use_nodes = True
    nt = m.node_tree; N = nt.nodes; L = nt.links
    for n in list(N): N.remove(n)
    out = N.new("ShaderNodeOutputMaterial")
    geo = N.new("ShaderNodeNewGeometry")
    dot = N.new("ShaderNodeVectorMath"); dot.operation = 'DOT_PRODUCT'
    dot.inputs[1].default_value = Vector((0.45, -0.35, 0.82)).normalized()
    L.new(geo.outputs["Normal"], dot.inputs[0])
    ramp = N.new("ShaderNodeValToRGB"); ramp.color_ramp.interpolation = 'EASE'
    ramp.color_ramp.elements[0].position = 0.52; ramp.color_ramp.elements[0].color = shadow + (1,)
    mid = ramp.color_ramp.elements.new(0.585); mid.color = tuple(shadow[k] * 0.45 + lit[k] * 0.55 for k in range(3)) + (1,)
    ramp.color_ramp.elements[2].position = 0.62; ramp.color_ramp.elements[2].color = lit + (1,)
    mr = N.new("ShaderNodeMapRange"); mr.inputs["From Min"].default_value = -1; mr.inputs["From Max"].default_value = 1
    L.new(dot.outputs["Value"], mr.inputs["Value"]); L.new(mr.outputs["Result"], ramp.inputs["Fac"])
    vc = N.new("ShaderNodeVertexColor"); vc.layer_name = "Col"
    mul = N.new("ShaderNodeMix"); mul.data_type = 'RGBA'; mul.blend_type = 'MULTIPLY'; mul.inputs["Factor"].default_value = 1.0
    L.new(ramp.outputs["Color"], mul.inputs[6]); L.new(vc.outputs["Color"], mul.inputs[7])
    em = N.new("ShaderNodeEmission")
    if textured:
        tx = N.new("ShaderNodeTexImage"); tx.image = bpy.data.images.load(TEX)
        mul2 = N.new("ShaderNodeMix"); mul2.data_type = 'RGBA'; mul2.blend_type = 'MULTIPLY'; mul2.inputs["Factor"].default_value = 0.6
        L.new(mul.outputs[2], mul2.inputs[6]); L.new(tx.outputs["Color"], mul2.inputs[7])
        L.new(mul2.outputs[2], em.inputs["Color"])
        tr = N.new("ShaderNodeBsdfTransparent"); mix = N.new("ShaderNodeMixShader")
        L.new(tx.outputs["Alpha"], mix.inputs["Fac"]); L.new(tr.outputs[0], mix.inputs[1]); L.new(em.outputs[0], mix.inputs[2])
        L.new(mix.outputs[0], out.inputs["Surface"])
    else:
        L.new(mul.outputs[2], em.inputs["Color"]); L.new(em.outputs[0], out.inputs["Surface"])
    return m

LIME, TEAL = (0.78, 1.00, 0.16), (0.20, 0.38, 0.42)
SPECS = [
    ("SM_GR_Tree_A", "tree", dict(seed=11, height=8.0, trunk_r=0.32, levels=3, clump_r=1.15, cards=70, spread=(32, 55))),
    ("SM_GR_Tree_B", "tree", dict(seed=27, height=11.0, trunk_r=0.38, levels=3, clump_r=1.3, cards=75, spread=(22, 40))),
    ("SM_GR_Tree_C", "tree", dict(seed=5, height=5.5, trunk_r=0.22, levels=2, clump_r=1.0, cards=70, spread=(32, 55))),
    ("SM_GR_LeafBush_A", "bush", dict(seed=3, radius=1.0, clumps=5, cards=50)),
    ("SM_GR_LeafBush_B", "bush", dict(seed=9, radius=0.7, clumps=4, cards=40)),
]

clear()
bark_m = toon_material("M_Bark", (0.62, 0.54, 0.50), (0.30, 0.28, 0.30), False)
leaf_m = toon_material("M_Leaves", LIME, TEAL, True)
objs = []
for name, kind, kw in SPECS:
    if kind == "tree":
        bark, leaves = make_tree(name, **kw)
    else:
        bark, leaves = make_bush(name, **kw)
    ob = build_object(name, bark, leaves, [bark_m, leaf_m])
    bpy.ops.object.select_all(action='DESELECT'); ob.select_set(True); bpy.context.view_layer.objects.active = ob
    bpy.ops.export_scene.fbx(filepath=os.path.join(OUT, name + ".fbx"), use_selection=True, apply_unit_scale=True,
                             mesh_smooth_type='OFF', colors_type='SRGB', bake_anim=False, add_leaf_bones=False, path_mode='STRIP')
    print("wrote %s: %d faces" % (name, len(ob.data.polygons)))
    objs.append(ob)

if PREVIEW:
    x = 0.0
    for ob in objs:
        w = ob.dimensions.x
        ob.location = (x + w / 2, 0, 0); x += w + 1.0
    sc = bpy.context.scene
    sc.render.engine = 'CYCLES'; sc.cycles.samples = 24; sc.cycles.device = 'CPU'
    sc.cycles.transparent_max_bounces = 48; sc.cycles.max_bounces = 2
    sc.world = bpy.data.worlds.new("w"); sc.world.use_nodes = True
    sc.world.node_tree.nodes["Background"].inputs[0].default_value = (0.10, 0.14, 0.18, 1)
    cam = bpy.data.objects.new("cam", bpy.data.cameras.new("cam")); sc.collection.objects.link(cam); sc.camera = cam
    mid = x / 2
    cam.location = (mid, -max(30.0, x * 0.95), 6.5); cam.rotation_euler = (math.radians(86), 0, 0); cam.data.lens = 35
    sc.render.resolution_x, sc.render.resolution_y = 1600, 800
    sc.view_settings.view_transform = 'Standard'
    sc.render.filepath = os.path.join(HERE, "..", "Docs", "trees_preview.png")
    bpy.ops.render.render(write_still=True)
    print("rendered preview")
print("ALL DONE")
