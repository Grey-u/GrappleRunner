"""
Grapple Runner: split the two downloaded weapon packs into one clean FBX per weapon.

WHAT THIS IS
Naomi downloaded two free fan-made weapon packs from Sketchfab (the "Kuronami" and the
"Araxys" Valorant skin lines). Each pack is ONE big FBX with every gun and knife side by
side, pointing in different directions, with the magazine and scope as separate pieces.
Unreal wants one mesh per weapon, pointing the same way, with its pivot where your hand
holds it. This script does that in Blender, with no clicking:

  1. open a pack, find the pieces of each weapon (gun body + magazine + scope)
  2. join them into one mesh and clean it (duplicate points, broken faces)
  3. turn it so the barrel points along +X and the top points up (+Z)
  4. move the pivot to the grip, where your hand goes
  5. add two "sockets" (named points Unreal reads from the FBX):
        SOCKET_Muzzle = the end of the barrel (bullets and the muzzle flash start here)
        SOCKET_Sight  = the point you look through when aiming down sights (ADS)
  6. export SM_<Pack>_<Weapon>.fbx and write weapons.json with the numbers

THESE MODELS ARE NOT OURS
They are fan models of Riot Games' designs. They stay on Naomi's PC only: the output folder
(Art/ThirdParty/Weapons/) and the Unreal folder (Content/ThirdParty/) are in .gitignore,
so they never go to the public GitHub repo. Fine for a private prototype; not for release.

HOW TO RUN
  python3 make_skin_weapons.py <kuronami pack folder> <araxys pack folder> <output folder>
(uses Blender's Python module "bpy"; each pack folder has source/*.fbx and textures/).

HOW TO READ THIS (no coding experience needed)
- Lines starting with # are notes for humans; the computer skips them.
- "def name(...):" is a reusable recipe; the indented lines under it are its steps.
- Blender works in metres; Unreal in centimetres. The FBX export converts (1 m -> 100 cm).
- Axes: Blender X = Unreal X (forward), Blender Z = Unreal Z (up), Blender +Y = Unreal -Y.
"""
import glob
import json
import math
import os
import sys

import bpy      # Blender itself (must load before bmesh)
import bmesh    # Blender's mesh-editing toolbox
from mathutils import Matrix, Vector

# ---------------------------------------------------------------------------------------
# Which pieces make each weapon. Key = the name we give it; value = words that must all be
# in a piece's name. "kind" tells the game how it behaves (see GrappleRig.h, FGRWeaponSlot).
# ---------------------------------------------------------------------------------------
PACKS = {
    "Kuronami": {
        "Rifle":    {"parts": [["GN_AK_Ninja"], ["GN_AK_Magazine_Ninja"]], "kind": "rifle"},
        "Marksman": {"parts": [["GN_Leversniper_Ninja"], ["GN_Leversniper_SniperScope"]], "kind": "sniper"},
        "SMG":      {"parts": [["GN_MP5_Ninja"], ["GN_MP5_Magazine"]], "kind": "smg"},
        "Revolver": {"parts": [["GN_Revolver"]], "kind": "pistol"},
        "Knife":    {"parts": [["EQ_Melee"]], "kind": "knife"},
    },
    "Araxys": {
        "Rifle":    {"parts": [["GN_AK_Antares"], ["GN_AK_Magazine_Antares"]], "kind": "rifle"},
        "Sniper":   {"parts": [["GN_Boltsniper_Antares"], ["GN_Boltsniper_Magazine"], ["GN_Boltsniper_SniperScope"]], "kind": "sniper"},
        "Burst":    {"parts": [["GN_Burst_Antares"], ["GN_Burst_Magazine"]], "kind": "burst"},
        "Sidearm":  {"parts": [["GN_Slim_Antares"]], "kind": "pistol"},
        "Knife":    {"parts": [["EQ_Melee_Antares"]], "kind": "knife"},
    },
}


def mesh_objects():
    return [o for o in bpy.data.objects if o.type == "MESH"]


def find_parts(words_list):
    """Every mesh whose name contains all the words of one entry (first match per entry)."""
    found = []
    for words in words_list:
        hits = [o for o in mesh_objects() if all(w.lower() in o.name.lower() for w in words) and o not in found]
        # "GN_AK_Ninja" must not also grab "GN_AK_Magazine_Ninja": prefer names without "Magazine"/"Scope"
        if not any(x in "".join(words) for x in ("Magazine", "Scope")):
            hits = [o for o in hits if "Magazine" not in o.name and "Scope" not in o.name] or hits
        if hits:
            found.append(hits[0])
    return found


def bake(obj):
    """Freeze a piece where it is drawn: apply its armature pose, unparent it, apply its transform."""
    bpy.ops.object.select_all(action="DESELECT")
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    for m in list(obj.modifiers):            # skinned meshes: keep the shape you see, drop the bones
        try:
            bpy.ops.object.modifier_apply(modifier=m.name)
        except RuntimeError:
            obj.modifiers.remove(m)
    bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)


def join(parts):
    for p in parts:
        bake(p)
    bpy.ops.object.select_all(action="DESELECT")
    for p in parts:
        p.select_set(True)
    bpy.context.view_layer.objects.active = parts[0]
    if len(parts) > 1:
        bpy.ops.object.join()
    return parts[0]


def clean(obj):
    """Same clean-up that fixed the ODM gear import ("Bad MeshDescription")."""
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.00005)
    bmesh.ops.dissolve_degenerate(bm, edges=bm.edges, dist=0.00005)
    loose = [v for v in bm.verts if not v.link_faces]
    bmesh.ops.delete(bm, geom=loose, context="VERTS")
    bm.to_mesh(obj.data)
    bm.free()
    if obj.data.has_custom_normals:
        bpy.context.view_layer.objects.active = obj
        bpy.ops.mesh.customdata_custom_splitnormals_clear()


def verts(obj):
    return [v.co.copy() for v in obj.data.vertices]


def end_height(pts, axis, lo, hi, at_high):
    """How tall the weapon is in the last 8% of its length at one end (thin = barrel tip)."""
    span = hi - lo
    if at_high:
        sel = [p for p in pts if p[axis] > hi - 0.08 * span]
    else:
        sel = [p for p in pts if p[axis] < lo + 0.08 * span]
    zs = [p.z for p in sel] or [0.0]
    return max(zs) - min(zs)


def orient(obj, mag_box, kind=""):
    """Turn the weapon so the barrel/blade tip points along +X; return its points and the turn."""
    pts = verts(obj)
    ext = [max(p[i] for p in pts) - min(p[i] for p in pts) for i in range(2)]
    axis = 0 if ext[0] >= ext[1] else 1                      # long axis in the ground plane: X or Y
    lo = min(p[axis] for p in pts)
    hi = max(p[axis] for p in pts)
    # No magazine: the barrel tip is the THIN end (a stock or a grip is tall).
    tip_high = end_height(pts, axis, lo, hi, True) < end_height(pts, axis, lo, hi, False)
    if mag_box is not None:
        # With a magazine: the pistol grip hangs down right BEHIND the magazine. Look 1-30 cm
        # either side of it; the side with the lower-hanging part is the back of the gun.
        m0, m1 = mag_box[0][axis], mag_box[1][axis]
        before = [p.z for p in pts if m0 - 0.30 < p[axis] < m0 - 0.01] or [9.0]
        after = [p.z for p in pts if m1 + 0.01 < p[axis] < m1 + 0.30] or [9.0]
        tip_high = min(before) < min(after)       # grip on the low side -> barrel tip on the high side
    if kind == "pistol":
        # a pistol's grip hangs down at the back: the barrel tip is the end FURTHEST from the lowest point
        low = min(pts, key=lambda p: p.z)
        tip_high = (low[axis] - lo) < (hi - low[axis])
    # rotation that sends the tip direction to +X (Z stays up)
    yaw = {(0, True): 0.0, (0, False): math.pi, (1, True): -math.pi / 2, (1, False): math.pi / 2}[(axis, tip_high)]
    obj.data.transform(Matrix.Rotation(yaw, 4, "Z"))
    obj.data.update()
    return verts(obj), yaw


def bins_along_x(pts, step=0.01):
    """Slice the weapon into 1 cm slices along X; for each slice keep its lowest and highest point."""
    out = {}
    for p in pts:
        k = int(math.floor(p.x / step))
        lo, hi = out.get(k, (p.z, p.z))
        out[k] = (min(lo, p.z), max(hi, p.z))
    return out, step


def measure(pts, kind, mag_box, scope_box):
    """Find the grip, muzzle and sight points (all in the oriented weapon's space, metres)."""
    xs = [p.x for p in pts]
    x0, x1 = min(xs), max(xs)
    length = x1 - x0
    tip = [p for p in pts if p.x > x1 - 0.04 * length]
    bore_z = sorted(p.z for p in tip)[len(tip) // 2]        # height of the barrel line at the tip
    y_mid = 0.5 * (min(p.y for p in pts) + max(p.y for p in pts))
    slices, step = bins_along_x(pts)

    if kind == "knife":
        # grip = a hand's width in from the back of the handle; the "muzzle" is the blade tip
        grip_x = x0 + 0.12 * length
        col = [p.z for p in pts if abs(p.x - grip_x) < 0.02] or [bore_z]
        grip = Vector((grip_x, y_mid, 0.5 * (min(col) + max(col))))
        return grip, Vector((x1, y_mid, bore_z)), Vector((grip_x, y_mid, max(col)))

    # Search window for the pistol grip: behind the magazine (rifles) or the back half (pistols)
    if mag_box is not None:
        a, b = mag_box[0].x - 0.30, mag_box[0].x - 0.01
    else:
        a, b = x0, x0 + 0.55 * length
    window = {k: v for k, v in slices.items() if a <= (k + 0.5) * step <= b}
    if not window:
        window = slices
    k_grip = min(window, key=lambda k: window[k][0])           # the slice that hangs lowest = grip
    grip_x = (k_grip + 0.5) * step - 0.02                       # palm sits just behind the lowest point
    grip_bottom = window[k_grip][0]
    top_here = max(v[1] for k, v in slices.items() if abs((k + 0.5) * step - grip_x) < 0.03)
    grip_z = top_here - 0.35 * (top_here - grip_bottom)         # a third of the way down the grip
    grip = Vector((grip_x, y_mid, grip_z))

    muzzle = Vector((x1, y_mid, bore_z))
    if scope_box is not None:
        # look straight down the middle of the scope, from its back end
        sight = Vector((scope_box[0].x, y_mid, 0.5 * (scope_box[0].z + scope_box[1].z)))
    else:
        # iron sights: the highest point over the gun's back half, on the centre line
        back = {k: v for k, v in slices.items() if grip_x - 0.05 <= (k + 0.5) * step <= grip_x + 0.25}
        top = max(v[1] for v in back.values()) if back else bore_z
        sight = Vector((grip_x, y_mid, top))
    return grip, muzzle, sight


def box(obj_pts):
    return (Vector((min(p.x for p in obj_pts), min(p.y for p in obj_pts), min(p.z for p in obj_pts))),
            Vector((max(p.x for p in obj_pts), max(p.y for p in obj_pts), max(p.z for p in obj_pts))))


def add_socket(name, at, parent):
    e = bpy.data.objects.new("SOCKET_" + name, None)
    e.empty_display_size = 0.02
    bpy.context.scene.collection.objects.link(e)
    e.parent = parent
    e.location = at
    return e


def export(obj, sockets, path):
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    for s in sockets:
        s.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(
        filepath=path, use_selection=True, object_types={"MESH", "EMPTY"},
        axis_forward="-Z", axis_up="Y", apply_unit_scale=True, apply_scale_options="FBX_SCALE_UNITS",
        mesh_smooth_type="FACE", use_tspace=False, path_mode="STRIP", embed_textures=False,
        bake_anim=False, add_leaf_bones=False)


def principal_axes(pts):
    """The directions the points spread out along most, 2nd most and least (PCA)."""
    import numpy as np
    a = np.array([[p.x, p.y, p.z] for p in pts])
    c = a.mean(axis=0)
    w, v = np.linalg.eigh(np.cov((a - c).T))
    order = w.argsort()[::-1]
    return Vector(c), [Vector(v[:, i]) for i in order]


def run_knife(pack, name, obj, out_dir, report):
    """Knives: some packs hold a PAIR (one per hand). Split them, lay each blade along +X."""
    import numpy as np
    bake(obj)
    for o in list(bpy.data.objects):
        if o is not obj:
            bpy.data.objects.remove(o)
    clean(obj)
    # split into loose pieces, then into two groups if there are clearly two blades side by side
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    bpy.ops.mesh.separate(type="LOOSE")
    bpy.ops.object.mode_set(mode="OBJECT")
    pieces = [o for o in bpy.data.objects if o.type == "MESH"]
    cents = np.array([[*(sum((v.co for v in o.data.vertices), Vector()) / max(1, len(o.data.vertices)))] for o in pieces])
    # Two blades side by side leave an empty gap between them. Look across the knife (along the
    # direction the pieces' centres spread most) for the widest empty band of points.
    groups = [pieces]
    if len(pieces) > 1:
        centre, axes = principal_axes([Vector(c) for c in cents])
        across = axes[0]
        proj = sorted((v.co - centre).dot(across) for o in pieces for v in o.data.vertices)
        n = len(proj)
        best_gap, cut = 0.0, None
        for k in range(int(n * 0.25), int(n * 0.75)):      # only cuts that leave a decent share each side
            g = proj[k + 1] - proj[k]
            if g > best_gap:
                best_gap, cut = g, 0.5 * (proj[k] + proj[k + 1])
        if best_gap > 0.02:
            right = [o for o, c in zip(pieces, cents) if (Vector(c) - centre).dot(across) >= cut]
            left = [o for o, c in zip(pieces, cents) if (Vector(c) - centre).dot(across) < cut]
            if left and right:
                groups = [right, left]
    for gi, group in enumerate(groups):
        bpy.ops.object.select_all(action="DESELECT")
        for o in group:
            o.select_set(True)
        bpy.context.view_layer.objects.active = group[0]
        if len(group) > 1:
            bpy.ops.object.join()
        blade = group[0]
        pts = verts(blade)
        c, (ax_len, ax_wide, ax_thin) = principal_axes(pts)
        # tip = the thinner end along the blade's length
        proj = [(p - c).dot(ax_len) for p in pts]
        lo, hi = min(proj), max(proj)
        def width(sel):
            ws = [(p - c).dot(ax_wide) for p, d in zip(pts, proj) if sel(d)] or [0.0]
            return max(ws) - min(ws)
        if width(lambda d: d > hi - 0.1 * (hi - lo)) > width(lambda d: d < lo + 0.1 * (hi - lo)):
            ax_len = -ax_len
            lo, hi = -hi, -lo
        # the blade's broad side faces sideways and its edge points up/down, like holding a knife
        up = ax_wide - ax_len * ax_wide.dot(ax_len)
        up.normalize()
        side_ax = up.cross(ax_len)
        # rows = where each of the knife's own directions goes: length -> X, side -> Y, width -> Z
        rot = Matrix((ax_len, side_ax, up))
        grip = c + ax_len * (lo + 0.12 * (hi - lo))
        blade.data.transform(Matrix.Translation(-grip))
        blade.data.transform(rot.to_4x4())
        blade.data.update()
        pts = verts(blade)
        x1 = max(p.x for p in pts)
        nm = "SM_%s_%s%s" % (pack, name, "" if gi == 0 else "_L")
        blade.name = nm
        blade.data.name = nm
        tip = Vector((x1, 0, 0))
        top = Vector((0, 0, max(p.z for p in pts if abs(p.x) < 0.03) if any(abs(p.x) < 0.03 for p in pts) else 0.05))
        socks = [add_socket("Muzzle", tip, blade), add_socket("Sight", top, blade)]
        export(blade, socks, os.path.join(out_dir, nm + ".fbx"))
        # a .blend holding only this blade and its sockets (to open and check by eye)
        bpy.data.libraries.write(os.path.join(out_dir, nm + ".blend"), {blade, *socks})
        lo_b, hi_b = box(pts)
        report[nm] = {"pack": pack, "weapon": name + ("" if gi == 0 else " (left hand)"), "kind": "knife",
                      "materials": [m.name for m in blade.data.materials if m],
                      "muzzle_cm": [round(x1 * 100, 2), 0.0, 0.0], "sight_cm": [0.0, 0.0, round(top.z * 100, 2)],
                      "length_cm": round((hi_b.x - lo_b.x) * 100, 1),
                      "tris": sum(len(p.vertices) - 2 for p in blade.data.polygons),
                      "pair": len(groups) > 1}
        print("DONE", nm, {k: v for k, v in report[nm].items() if k != "materials"})
        for o in socks:
            bpy.data.objects.remove(o)
        blade.hide_set(True)
        blade.hide_render = True


def run_pack(pack, src_dir, out_dir, report):
    fbx = glob.glob(os.path.join(src_dir, "source", "*.fbx"))[0]
    for name, spec in PACKS[pack].items():
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.fbx(filepath=fbx)
        parts = find_parts(spec["parts"])
        if not parts:
            print("MISSING", pack, name)
            continue
        if spec["kind"] == "knife":
            run_knife(pack, name, parts[0], out_dir, report)
            continue
        # remember where the magazine / scope are before joining (they guide the grip and sight)
        extras = {}
        for p in parts[1:]:
            bake(p)
            key = "scope" if "Scope" in p.name else "mag"
            extras[key] = [p.matrix_world @ v.co for v in p.data.vertices]
        body = join(parts)
        bpy.context.view_layer.objects.active = body
        bpy.ops.object.select_all(action="DESELECT")
        for o in list(bpy.data.objects):          # throw away every other weapon, light and camera
            if o is not body:
                bpy.data.objects.remove(o)
        clean(body)
        pts, yaw = orient(body, box(extras["mag"]) if "mag" in extras else None, spec["kind"])
        # turn the magazine and scope boxes the same way the body was turned
        rot = Matrix.Rotation(yaw, 3, "Z")
        mag_box = box([rot @ v for v in extras["mag"]]) if "mag" in extras else None
        scope_box = box([rot @ v for v in extras["scope"]]) if "scope" in extras else None
        grip, muzzle, sight = measure(pts, spec["kind"], mag_box, scope_box)
        # move the pivot to the grip
        body.data.transform(Matrix.Translation(-grip))
        body.data.update()
        muzzle -= grip
        sight -= grip
        body.name = "SM_%s_%s" % (pack, name)
        body.data.name = body.name
        socks = [add_socket("Muzzle", muzzle, body), add_socket("Sight", sight, body)]
        path = os.path.join(out_dir, body.name + ".fbx")
        export(body, socks, path)
        bpy.ops.wm.save_as_mainfile(filepath=os.path.join(out_dir, body.name + ".blend"))
        pts = verts(body)
        lo, hi = box(pts)
        report[body.name] = {
            "pack": pack, "weapon": name, "kind": spec["kind"],
            "materials": [m.name for m in body.data.materials if m],
            # Unreal centimetres, Unreal axes (Y flipped)
            "muzzle_cm": [round(muzzle.x * 100, 2), round(-muzzle.y * 100, 2), round(muzzle.z * 100, 2)],
            "sight_cm": [round(sight.x * 100, 2), round(-sight.y * 100, 2), round(sight.z * 100, 2)],
            "length_cm": round((hi.x - lo.x) * 100, 1),
            "tris": sum(len(p.vertices) - 2 for p in body.data.polygons),
        }
        print("DONE", body.name, report[body.name])


def main():
    args = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else sys.argv[1:]
    kuronami, araxys, out_dir = args[:3]
    os.makedirs(out_dir, exist_ok=True)
    report = {}
    run_pack("Kuronami", kuronami, out_dir, report)
    run_pack("Araxys", araxys, out_dir, report)
    with open(os.path.join(out_dir, "weapons.json"), "w") as f:
        json.dump(report, f, indent=2)
    print("WROTE", len(report), "weapons to", out_dir)


main()
