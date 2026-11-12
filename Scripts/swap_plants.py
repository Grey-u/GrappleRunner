"""
Grapple Runner: swap every old plant in the level for photoscanned, wind-animated ones.

Run (Unreal Output Log, Python box):
    exec(open("C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/swap_plants.py").read())

WHAT IT DOES
1. Finds the new plant meshes from the four Project Nature packs (free on Fab, photoscanned,
   with wind and "bend when you walk through" built into their materials):
     PN_tropicalGroundPlants, PN_Fern, PN_Banana, PN_Pandanus
2. Finds every plant our scripts placed earlier (folders GrappleRunner/Foliage, /Ruins, /Trees):
   the Brushify grass and bushes, the Kite Demo and Infinity Blade ferns, the beech saplings.
   (Big beech trees, stones, ruins and hanging vines are left alone.)
3. Replaces each one IN PLACE with a new plant of a similar size:
     small (under 1.2 m)  -> tropical ground plants and small ferns
     medium (1.2 - 3 m)   -> big ferns, young banana, pandanus bushes
     tall (over 3 m)      -> banana trees, pandanus trees
4. COLOUR ZONES ("different colours in different areas", Log Horizon style): the world is cut
   into 120 m squares, and each square gets its own mix of plants and its own leaf tint:
     Banana Grove (fresh green), Fern Gully (deep blue-green), Pandanus Marsh (golden green),
     Wild Mix (lime). Tints are new material copies in /Game/GrappleRunner/Plants/Tints,
     so the packs' own files are never changed.
5. GLOWING PLANTS: about 1 in 6 plants in the Fern Gully and Marsh zones get a glowing rim
   (an "overlay" material) that is invisible by day and lights up at night, reading the
   day/night clock (MPC_World.Night). Cyan, teal and violet.
6. Saves the level. Writes Saved/Tests/plant_swap.csv (what was swapped where).

HOW TO READ THIS (no coding experience needed)
- Lines starting with # are notes for humans; the computer skips them.
- "def name(...):" is a reusable recipe; the indented lines under it are its steps.
- Unreal distances are centimetres (100 = 1 metre).
"""
import math
import os
import random

import unreal

random.seed(11)   # the same result every run
assets = unreal.EditorAssetLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
P = unreal.MaterialProperty

FOLDERS = ["GrappleRunner/Foliage", "GrappleRunner/Ruins", "GrappleRunner/Trees"]
OLD_PLANT_WORDS = ["Foliage", "Fern", "Bush", "Grass", "Flora", "Flower", "Sapling", "Plant", "Leaf"]
KEEP_WORDS = ["Vine", "Beech_Forest", "Stone", "Column", "Wall", "Arch", "Tower", "Statue", "Fountain", "Water", "Root"]
TINT_DIR = "/Game/GrappleRunner/Plants/Tints"
GLOW_DIR = "/Game/GrappleRunner/Plants"
ZONE_SIZE = 12000.0   # 120 m


def meshes_in(folder, words):
    """All static meshes in a pack folder whose names contain one of the words."""
    out = []
    for path in assets.list_assets(folder, recursive=True, include_folder=False):
        name = path.split("/")[-1].split(".")[0]
        if any(w.lower() in name.lower() for w in words):
            a = unreal.load_asset(path.split(".")[0])
            if isinstance(a, unreal.StaticMesh):
                out.append(a)
    return out


def height(mesh):
    b = mesh.get_bounds()
    return max(1.0, b.box_extent.z * 2.0)


# ---- 1. the new plants, sorted by size ----
small_ground = meshes_in("/Game/PN_tropicalGroundPlants", ["tropicalPlant_"]) + meshes_in("/Game/PN_Pandanus", ["groundPlant_"])
ferns = meshes_in("/Game/PN_Fern", ["fern_"])
banana = meshes_in("/Game/PN_Banana", ["banana_"])
pandanus_big = meshes_in("/Game/PN_Pandanus", ["bigplant_"])
pandanus_tree = meshes_in("/Game/PN_Pandanus", ["treeplant_"])
if not (small_ground and ferns and banana and pandanus_big):
    raise RuntimeError("Grapple Runner plants: a Project Nature pack is missing - add it from Fab first")
small_ferns = [m for m in ferns if height(m) < 120]
big_ferns = [m for m in ferns if height(m) >= 120] or ferns
young_banana = [m for m in banana if height(m) < 300] or banana
tall_banana = [m for m in banana if height(m) >= 300] or banana

# Each zone: (name, small plants, medium plants, tall plants, leaf tint, glows at night?)
ZONES = [
    ("Banana Grove", small_ground, young_banana + big_ferns, tall_banana, (0.95, 1.08, 0.9), False),
    ("Fern Gully", small_ferns or small_ground, big_ferns, pandanus_tree or tall_banana, (0.82, 1.0, 1.05), True),
    ("Pandanus Marsh", small_ground, pandanus_big, pandanus_tree or tall_banana, (1.08, 1.05, 0.78), True),
    ("Wild Mix", small_ground + small_ferns, big_ferns + pandanus_big + young_banana, tall_banana + pandanus_tree, (0.95, 1.15, 0.75), False),
]
GLOW_COLOURS = [(0.1, 0.9, 1.0), (0.2, 1.0, 0.6), (0.6, 0.35, 1.0)]


def zone_of(loc):
    gx = int(math.floor(loc.x / ZONE_SIZE))
    gy = int(math.floor(loc.y / ZONE_SIZE))
    return ZONES[(gx * 7 + gy * 13) % len(ZONES)]


# ---- 4. tinted copies of the pack materials (one per zone and material) ----
tint_cache = {}


def tinted(material, zone_index, tint):
    """A copy of a plant material with this zone's leaf colour (if the material has a colour knob)."""
    key = (material.get_path_name(), zone_index)
    if key in tint_cache:
        return tint_cache[key]
    result = material
    if isinstance(material, unreal.MaterialInstance):
        knobs = [str(p) for p in mel.get_vector_parameter_names(material)]
        colour_knobs = [k for k in knobs if any(w in k.lower() for w in ("color", "colour", "tint", "albedo"))]
        if colour_knobs:
            name = "%s_Z%d" % (material.get_name(), zone_index)
            path = TINT_DIR + "/" + name
            if assets.does_asset_exist(path):
                mi = unreal.load_asset(path)
            else:
                mi = tools.create_asset(name, TINT_DIR, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
                mi.set_editor_property("parent", material)
            for k in colour_knobs:
                base = mel.get_material_instance_vector_parameter_value(material, k)
                mel.set_material_instance_vector_parameter_value(
                    mi, k, unreal.LinearColor(base.r * tint[0], base.g * tint[1], base.b * tint[2], base.a))
            mel.update_material_instance(mi)
            assets.save_asset(path)
            result = mi
        elif zone_index == 0:
            unreal.log("Grapple Runner plants: no colour knob on %s (knobs: %s)" % (material.get_name(), ", ".join(knobs) or "none"))
    tint_cache[key] = result
    return result


# ---- 5. night-glow overlay materials ----
def world_params():
    return unreal.load_asset("/Game/GrappleRunner/Materials/MPC_World")


def glow_overlay(i, rgb):
    path = "%s/M_PlantGlow_%d" % (GLOW_DIR, i)
    if assets.does_asset_exist(path):
        return unreal.load_asset(path)
    m = tools.create_asset("M_PlantGlow_%d" % i, GLOW_DIR, unreal.Material, unreal.MaterialFactoryNew())
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("two_sided", True)
    colour = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -700, 0)
    colour.set_editor_property("constant", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    rim = mel.create_material_expression(m, unreal.MaterialExpressionFresnel, -700, 160)   # brightest at the leaf edges
    rim.set_editor_property("exponent", 2.5)
    lit = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -450, 60)
    mel.connect_material_expressions(colour, "", lit, "A")
    mel.connect_material_expressions(rim, "", lit, "B")
    night = mel.create_material_expression(m, unreal.MaterialExpressionCollectionParameter, -700, 320)
    mpc = world_params()
    if mpc:
        night.set_editor_property("collection", mpc)
        night.set_editor_property("parameter_name", "Night")
    strength = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -450, 300)
    strength.set_editor_property("const_a", 0.0)     # day: invisible
    strength.set_editor_property("const_b", 6.0)     # night: glowing
    mel.connect_material_expressions(night, "", strength, "Alpha")
    out = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -200, 120)
    mel.connect_material_expressions(lit, "", out, "A")
    mel.connect_material_expressions(strength, "", out, "B")
    mel.connect_material_property(out, "", P.MP_EMISSIVE_COLOR)
    mel.recompile_material(m)
    assets.save_asset(path)
    return m


GLOWS = [glow_overlay(i, c) for i, c in enumerate(GLOW_COLOURS)]


def old_plant(actor):
    comp = actor.get_component_by_class(unreal.StaticMeshComponent) if isinstance(actor, unreal.StaticMeshActor) else None
    mesh = comp.get_editor_property("static_mesh") if comp else None
    if not mesh:
        return None
    path = mesh.get_path_name()
    if "/PN_" in path:
        return None                            # already new
    if any(w.lower() in path.lower() for w in KEEP_WORDS):
        return None
    if not any(w.lower() in path.lower() for w in OLD_PLANT_WORDS):
        return None
    return comp


# ---- 2 + 3. swap ----
todo = [a for a in actors.get_all_level_actors() if str(a.get_folder_path()) in FOLDERS]
rows = ["label,zone,old,new,height_m,glow"]
swapped = glowing = 0
with unreal.ScopedSlowTask(len(todo), "Swapping plants") as task:
    task.make_dialog(True)
    for a in todo:
        task.enter_progress_frame(1)
        comp = old_plant(a)
        if not comp:
            continue
        origin, extent = a.get_actor_bounds(False)
        old_h = extent.z * 2.0
        loc = a.get_actor_location()
        zi = ZONES.index(zone_of(loc))
        name, small, medium, tall, tint, glows = ZONES[zi]
        pool = small if old_h < 120 else (medium if old_h < 300 else tall)
        new_mesh = random.choice(pool)
        # size: about as tall as the old plant (a little bigger: lusher), within sensible limits
        target = max(50.0, min(old_h * random.uniform(1.0, 1.35), 900.0))
        s = target / height(new_mesh)
        s = max(0.5, min(s, 2.2))
        old_name = comp.get_editor_property("static_mesh").get_name()
        comp.set_editor_property("static_mesh", new_mesh)
        comp.set_editor_property("override_materials", [])
        for i, mat in enumerate(new_mesh.get_editor_property("static_materials")):
            m = mat.get_editor_property("material_interface")
            if m:
                comp.set_material(i, tinted(m, zi, tint))
        a.set_actor_scale3d(unreal.Vector(s, s, s))
        a.set_actor_rotation(unreal.Rotator(roll=0.0, pitch=0.0, yaw=random.uniform(0, 360)), False)
        comp.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)   # plants never block a swing
        g = ""
        if glows and random.random() < 0.17:
            comp.set_editor_property("overlay_material", random.choice(GLOWS))
            glowing += 1
            g = "yes"
        else:
            comp.set_editor_property("overlay_material", None)
        a.set_actor_label("Plant_%s_%04d" % (name.replace(" ", ""), swapped))
        rows.append("%s,%s,%s,%s,%.1f,%s" % (a.get_actor_label(), name, old_name, new_mesh.get_name(), target / 100.0, g))
        swapped += 1

out = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_saved_dir()), "Tests")
os.makedirs(out, exist_ok=True)
with open(os.path.join(out, "plant_swap.csv"), "w") as f:
    f.write("\n".join(rows))
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
unreal.log("Grapple Runner plants: swapped %d plants (%d glow at night) into %d zones" % (swapped, glowing, len(ZONES)))
