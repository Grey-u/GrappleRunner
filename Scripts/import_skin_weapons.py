"""
Grapple Runner: import the Kuronami and Araxys weapons and give them proper materials.

Run (in Unreal's Output Log, Python box):
    py "C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/import_skin_weapons.py"

WHAT IT DOES
1. Imports every SM_<Pack>_<Weapon>.fbx from Art/ThirdParty/Weapons (made by
   Art/ThirdParty/make_skin_weapons.py: barrel along +X, pivot at the grip, sockets
   "Muzzle" and "Sight") into /Game/ThirdParty/SkinWeapons.
2. Imports the textures from Art/ThirdParty/Weapons/Textures and sets them up:
   DF = colour (sRGB), NM = normal map, MRS = metal (red) / roughness (green) / specular (blue).
3. Builds one material per texture set, plus glow materials for the Kuronami "water" and the
   Araxys light strips, and puts each one in the right slot of each weapon.
4. Makes sure every weapon has its Muzzle and Sight sockets (adds them from weapons.json if the
   importer skipped them). GRArsenal uses them to aim down the sights and start tracers.
5. Makes M_WeaponGlow (tracers, sparks) in /Game/GrappleRunner/Weapons/FX. That one is OUR
   material, so it lives with our content.
6. Lays the weapons out on a rack next to the PlayerStart so you can look at them.

THESE MODELS ARE NOT OURS
Fan models of Riot Games' designs, downloaded free from Sketchfab. Art/ThirdParty/Weapons and
Content/ThirdParty are in .gitignore: they stay on this PC and never go to GitHub.

HOW TO READ THIS (no coding experience needed)
- Lines starting with # are notes for humans; the computer skips them.
- "def name(...):" is a reusable recipe; the indented lines under it are its steps.
- Unreal distances are in centimetres (100 = 1 metre).
"""
import json
import os

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJECT, "Art", "ThirdParty", "Weapons")
TEX_SRC = os.path.join(SRC, "Textures")
DEST = "/Game/ThirdParty/SkinWeapons"
TEX_DEST = DEST + "/Textures"
FX_DEST = "/Game/GrappleRunner/Weapons/FX"
RACK = "GrappleRunner/SkinWeaponRack"

tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
S = unreal.MaterialSamplerType
P = unreal.MaterialProperty
C = unreal.TextureCompressionSettings

if not os.path.isdir(SRC):
    raise RuntimeError("Grapple Runner weapons: put the weapon files in " + SRC + " first")
with open(os.path.join(SRC, "weapons.json")) as f:
    INFO = json.load(f)


def import_files(paths, dest):
    jobs = []
    for full in paths:
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", full)
        t.set_editor_property("destination_path", dest)
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("save", True)
        jobs.append(t)
    tools.import_asset_tasks(jobs)


# ---- 1 + 2. meshes and textures ------------------------------------------------------
import_files([os.path.join(SRC, n + ".fbx") for n in INFO if os.path.exists(os.path.join(SRC, n + ".fbx"))], DEST)
import_files([os.path.join(TEX_SRC, f) for f in sorted(os.listdir(TEX_SRC)) if f.lower().endswith(".png")], TEX_DEST)


def texture(name):
    """Load one texture and set it up: colour maps stay sRGB, data maps (normal, masks) don't."""
    path = TEX_DEST + "/" + name
    if not assets.does_asset_exist(path):
        return None
    tex = unreal.load_asset(path)
    if name.endswith("_NM"):
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("compression_settings", C.TC_NORMALMAP)
    elif name.endswith("_MRS"):
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("compression_settings", C.TC_MASKS)
    assets.save_asset(path)
    return tex


# ---- 3. materials ---------------------------------------------------------------------
def fresh_material(folder, name):
    """An empty material asset (re-running the script rebuilds it from scratch)."""
    path = folder + "/" + name
    if assets.does_asset_exist(path):
        m = unreal.load_asset(path)
        if isinstance(m, unreal.Material):
            mel.delete_all_material_expressions(m)
            return m
        assets.delete_asset(path)   # only ever our own generated assets in our own folder
    return tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())


def constant(m, value, x, y):
    node = mel.create_material_expression(m, unreal.MaterialExpressionConstant, x, y)
    node.set_editor_property("r", value)
    return node


def colour(m, rgb, x, y):
    node = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, x, y)
    node.set_editor_property("constant", unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0))
    return node


def textured_material(set_name):
    """Colour + normal + (metal/roughness from the MRS mask, or sensible numbers without one)."""
    df, nm, mrs = texture("T_%s_DF" % set_name), texture("T_%s_NM" % set_name), texture("T_%s_MRS" % set_name)
    if not df:
        return None
    m = fresh_material(DEST, "M_" + set_name)
    n = mel.create_material_expression(m, unreal.MaterialExpressionTextureSample, -500, 0)
    n.set_editor_property("texture", df)
    mel.connect_material_property(n, "RGB", P.MP_BASE_COLOR)
    if nm:
        n = mel.create_material_expression(m, unreal.MaterialExpressionTextureSample, -500, 260)
        n.set_editor_property("texture", nm)
        n.set_editor_property("sampler_type", S.SAMPLERTYPE_NORMAL)
        mel.connect_material_property(n, "RGB", P.MP_NORMAL)
    if mrs:
        # Assumed layout (checked by eye: red is high on bare metal, green mid on paint):
        # R = metallic, G = roughness, B = specular.
        n = mel.create_material_expression(m, unreal.MaterialExpressionTextureSample, -500, 520)
        n.set_editor_property("texture", mrs)
        n.set_editor_property("sampler_type", S.SAMPLERTYPE_MASKS)
        mel.connect_material_property(n, "R", P.MP_METALLIC)
        mel.connect_material_property(n, "G", P.MP_ROUGHNESS)
    else:
        mel.connect_material_property(constant(m, 0.35, -300, 520), "", P.MP_METALLIC)
        mel.connect_material_property(constant(m, 0.45, -300, 620), "", P.MP_ROUGHNESS)
    mel.recompile_material(m)
    assets.save_asset(m.get_path_name())
    return m


def glow_material(folder, name, base, glow, strength, rough=0.2, metal=0.0):
    """A solid surface that glows (the light strips and water)."""
    m = fresh_material(folder, name)
    mel.connect_material_property(colour(m, base, -400, 0), "", P.MP_BASE_COLOR)
    g = colour(m, glow, -600, 200)
    k = constant(m, strength, -600, 320)
    mul = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -350, 240)
    mel.connect_material_expressions(g, "", mul, "A")
    mel.connect_material_expressions(k, "", mul, "B")
    mel.connect_material_property(mul, "", P.MP_EMISSIVE_COLOR)
    mel.connect_material_property(constant(m, rough, -300, 420), "", P.MP_ROUGHNESS)
    mel.connect_material_property(constant(m, metal, -300, 500), "", P.MP_METALLIC)
    mel.recompile_material(m)
    assets.save_asset(m.get_path_name())
    return m


KURONAMI_WATER = glow_material(DEST, "M_Kuronami_Water", (0.01, 0.05, 0.15), (0.15, 0.55, 1.0), 4.0, rough=0.1)
ARAXYS_GLOW = glow_material(DEST, "M_Araxys_Glow", (0.02, 0.1, 0.07), (0.25, 1.0, 0.7), 6.0)
ARAXYS_GLASS = glow_material(DEST, "M_Araxys_Glass", (0.01, 0.02, 0.02), (0.1, 0.5, 0.4), 0.3, rough=0.05, metal=0.9)

# Tracers and sparks: unlit, adds light on top of whatever is behind it. Colour set by the code.
fx = fresh_material(FX_DEST, "M_WeaponGlow")
fx.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
fx.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
p_col = mel.create_material_expression(fx, unreal.MaterialExpressionVectorParameter, -600, 0)
p_col.set_editor_property("parameter_name", "Colour")
p_col.set_editor_property("default_value", unreal.LinearColor(1.0, 0.55, 0.15, 1.0))
p_str = mel.create_material_expression(fx, unreal.MaterialExpressionScalarParameter, -600, 200)
p_str.set_editor_property("parameter_name", "Strength")
p_str.set_editor_property("default_value", 25.0)
fx_mul = mel.create_material_expression(fx, unreal.MaterialExpressionMultiply, -300, 100)
mel.connect_material_expressions(p_col, "", fx_mul, "A")
mel.connect_material_expressions(p_str, "", fx_mul, "B")
mel.connect_material_property(fx_mul, "", P.MP_EMISSIVE_COLOR)
mel.recompile_material(fx)
assets.save_asset(fx.get_path_name())

# Which texture set each material slot (named in the original download) uses.
SETS = {
    "AK_Ninja": "Kuronami_AK", "LeverSniper_Ninja": "Kuronami_LeverSniper",
    "LeverSniper_Ninja_Scope": "Kuronami_LeverSniper_Scope", "MP5_Ninja": "Kuronami_MP5",
    "Revolver_Ninja_3p": "Kuronami_Revolver", "Melee_Ninja": "Kuronami_Melee",
    "AK_Antares": "Araxys_AK", "BoltSniper_Antares": "Araxys_BoltSniper",
    "BoltSniper_Antares_Scope": "Araxys_BoltSniper_Scope", "Burst_Antares": "Araxys_Burst",
    "Slim_Antares": "Araxys_Slim", "Melee_Antares": "Araxys_Melee",
}
built = {}


def material_for(slot):
    s = slot.replace("_MI", "")
    if "Water" in s:
        return KURONAMI_WATER
    if "Emissive" in s or "IdleBeams" in s:
        return ARAXYS_GLOW
    if "Glass" in s:
        return ARAXYS_GLASS
    set_name = SETS.get(s)
    if not set_name:
        return None
    if set_name not in built:
        built[set_name] = textured_material(set_name)
    return built[set_name]


# ---- 3 + 4. put materials in the slots, check the sockets ------------------------------
for name, info in INFO.items():
    path = DEST + "/" + name
    mesh = unreal.load_asset(path)
    if not mesh:
        unreal.log_warning("Grapple Runner weapons: not imported " + name)
        continue
    for i, sm in enumerate(mesh.get_editor_property("static_materials")):
        slot = str(sm.get_editor_property("material_slot_name"))
        mat = material_for(slot)
        if mat:
            mesh.set_material(i, mat)
        else:
            unreal.log_warning("Grapple Runner weapons: no material for slot %s on %s" % (slot, name))
    for sock, key in (("Muzzle", "muzzle_cm"), ("Sight", "sight_cm")):
        if mesh.find_socket(sock) is None:
            s = unreal.new_object(unreal.StaticMeshSocket, outer=mesh)
            s.set_editor_property("socket_name", sock)
            s.set_editor_property("relative_location", unreal.Vector(*info[key]))
            mesh.add_socket(s)
    assets.save_asset(path)
    unreal.log("Grapple Runner weapons: ready %s (%s, %d tris)" % (name, info["kind"], info["tris"]))

# ---- 6. a rack next to the PlayerStart, so you can see them all -------------------------
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
old = [a for a in actors.get_all_level_actors() if str(a.get_folder_path()) == RACK]
if old:
    actors.destroy_actors(old)
start = next((a.get_actor_location() for a in actors.get_all_level_actors() if isinstance(a, unreal.PlayerStart)), unreal.Vector(0, 0, 0))
count = {"Kuronami": 0, "Araxys": 0}
row = 0
for name, info in INFO.items():
    mesh = unreal.load_asset(DEST + "/" + name)
    if not mesh:
        continue
    # Two columns, one per pack, 3 m in front of the start; each weapon side-on, barrel to the right.
    pack = info["pack"]
    column_y = -160.0 if pack == "Kuronami" else 60.0
    at = start + unreal.Vector(300.0, column_y, 30.0 + count[pack] * 40.0)
    count[pack] += 1
    a = actors.spawn_actor_from_object(mesh, at, unreal.Rotator(roll=0.0, pitch=0.0, yaw=90.0))
    a.set_actor_scale3d(unreal.Vector(0.75, 0.75, 0.75))
    a.set_actor_label("Rack_" + name)
    a.set_folder_path(RACK)
    a.get_component_by_class(unreal.StaticMeshComponent).set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    row += 1
unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
unreal.log("Grapple Runner weapons: %d weapons imported and racked" % row)
