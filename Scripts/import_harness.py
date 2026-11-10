"""
Grapple Runner: import the harness v2 pieces into Unreal.

Run in Unreal (Output Log, set the box at the bottom to "Python" or type "py " first):
    py "C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/import_harness.py"

What it does
- Reads Art/Harness/SM_Harness_Waist.fbx, SM_Harness_ThighA.fbx and SM_Harness_ThighB.fbx
  (made by Art/Harness/make_harness.py) and imports them to /Game/GrappleRunner/Harness.
- Gives each colour slot the weapon kit's existing material with the same name
  (M_Steel, M_Leather, ...), so the harness matches the blades and pistol.
- (No collision needed: the game switches it off on the harness parts; the physics uses the capsule.)
- Re-running replaces the old versions, so it's safe to run again after changing the design.

HOW TO READ THIS (no coding experience needed)
- Lines starting with # are notes for humans; the computer skips them.
- "import" = load a toolbox. "unreal" is Unreal Engine's own Python toolbox.
- "for x in ...:" repeats the indented steps once per item; "if ...:" only runs them when true.
"""
import os
import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJECT, "Art", "Harness")
DEST = "/Game/GrappleRunner/Harness"
MATERIALS = "/Game/GrappleRunner/Weapons"     # where the weapon kit's materials already live
NAMES = ["SM_Harness_Waist", "SM_Harness_ThighA", "SM_Harness_ThighB"]

# ---- 1. import the three FBX files
tasks = []
for n in NAMES:
    f = os.path.join(SRC, n + ".fbx")
    if not os.path.exists(f):
        unreal.log_warning("Harness: missing " + f)
        continue
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", f)
    t.set_editor_property("destination_path", DEST)
    t.set_editor_property("automated", True)          # no pop-up questions
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    tasks.append(t)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)

# ---- 2. use the weapon kit's existing materials
for n in NAMES:
    path = "{}/{}".format(DEST, n)
    mesh = unreal.load_asset(path)
    if not mesh:
        unreal.log_warning("Harness: import failed for " + n)
        continue
    slots = mesh.get_editor_property("static_materials")
    for i, slot in enumerate(slots):
        slot_name = str(slot.get_editor_property("material_slot_name"))
        # the slot is named after the Blender material, sometimes with a suffix added
        base = slot_name.split(".")[0]
        for candidate in (base, base.rsplit("_", 1)[0]):
            m = unreal.load_asset("{}/{}".format(MATERIALS, candidate))
            if m:
                mesh.set_material(i, m)
                break
        else:
            unreal.log_warning("Harness: no material found for slot " + slot_name)
    unreal.EditorAssetLibrary.save_asset(path)
    unreal.log("Harness: ready " + path + " ({} colour slots)".format(len(slots)))

# ---- 3. the ODM gear and swords (harness v3), from the downloaded Sketchfab model.
# (First remove the broken material the importer made on the first attempt, 10 Nov.)
if unreal.EditorAssetLibrary.does_asset_exist(DEST + "/M_ODMGear"):
    unreal.EditorAssetLibrary.delete_asset(DEST + "/M_ODMGear")
# Its files live in Art/ThirdParty/ODM (kept out of the public repo, see .gitignore):
#   SM_ODM_Gear.fbx, SM_ODM_Sword_L.fbx, SM_ODM_Sword_R.fbx  (made by Art/ThirdParty/make_odm_gear.py)
#   snk_texture_BaseColor / Normal / Metallic / Roughness / AO .jpg  (4K textures from the download)
ODM = os.path.join(PROJECT, "Art", "ThirdParty", "ODM")
ODM_MESHES = ["SM_ODM_Gear", "SM_ODM_Sword_L", "SM_ODM_Sword_R"]
if os.path.isdir(ODM):
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    jobs = []
    for f in [n + ".fbx" for n in ODM_MESHES] + ["snk_texture_%s.jpg" % t for t in
                                                  ("BaseColor", "Normal", "Metallic", "Roughness", "AO")]:
        full = os.path.join(ODM, f)
        if os.path.exists(full):
            t = unreal.AssetImportTask()
            t.set_editor_property("filename", full)
            t.set_editor_property("destination_path", DEST)
            t.set_editor_property("automated", True)
            t.set_editor_property("replace_existing", True)
            t.set_editor_property("save", True)
            jobs.append(t)
    tools.import_asset_tasks(jobs)

    # Texture settings. Colour stays sRGB; the others are DATA (numbers, not colours), so sRGB off.
    # The normal map is flipped: the download uses OpenGL's convention, Unreal uses DirectX's.
    def texture(name, srgb, compression, flip=False):
        tex = unreal.load_asset("{}/snk_texture_{}".format(DEST, name))
        if not tex:
            return None
        tex.set_editor_property("srgb", srgb)
        tex.set_editor_property("compression_settings", compression)
        if flip:
            tex.set_editor_property("flip_green_channel", True)
        unreal.EditorAssetLibrary.save_asset(tex.get_path_name())
        return tex

    C = unreal.TextureCompressionSettings
    tex_base = texture("BaseColor", True, C.TC_DEFAULT)
    tex_norm = texture("Normal", False, C.TC_NORMALMAP, flip=True)
    tex_metal = texture("Metallic", False, C.TC_GRAYSCALE)
    tex_rough = texture("Roughness", False, C.TC_GRAYSCALE)
    tex_ao = texture("AO", False, C.TC_GRAYSCALE)

    # A material that plugs each texture into its slot (built in code, so it's reproducible).
    mel = unreal.MaterialEditingLibrary
    mat_path = DEST + "/M_ODMGear_PBR"      # (not "M_ODMGear": the importer once made a broken asset by that name)
    if unreal.EditorAssetLibrary.does_asset_exist(mat_path):
        material = unreal.load_asset(mat_path)
        if isinstance(material, unreal.Material):
            mel.delete_all_material_expressions(material)
        else:
            unreal.EditorAssetLibrary.delete_asset(mat_path)
            material = tools.create_asset("M_ODMGear_PBR", DEST, unreal.Material, unreal.MaterialFactoryNew())
    else:
        material = tools.create_asset("M_ODMGear_PBR", DEST, unreal.Material, unreal.MaterialFactoryNew())
    S = unreal.MaterialSamplerType
    P = unreal.MaterialProperty
    for i, (tex, sampler, out, prop) in enumerate([
            (tex_base, S.SAMPLERTYPE_COLOR, "RGB", P.MP_BASE_COLOR),
            (tex_norm, S.SAMPLERTYPE_NORMAL, "RGB", P.MP_NORMAL),
            (tex_metal, S.SAMPLERTYPE_GRAYSCALE, "R", P.MP_METALLIC),
            (tex_rough, S.SAMPLERTYPE_GRAYSCALE, "R", P.MP_ROUGHNESS),
            (tex_ao, S.SAMPLERTYPE_GRAYSCALE, "R", P.MP_AMBIENT_OCCLUSION)]):
        if not tex:
            continue
        node = mel.create_material_expression(material, unreal.MaterialExpressionTextureSample, -400, i * 260)
        if node is None:
            unreal.log_warning("Harness: could not add a texture node to the ODM material")
            continue
        node.set_editor_property("texture", tex)
        node.set_editor_property("sampler_type", sampler)
        mel.connect_material_property(node, out, prop)
    mel.recompile_material(material)
    unreal.EditorAssetLibrary.save_asset(mat_path)

    for n in ODM_MESHES:
        mesh = unreal.load_asset("{}/{}".format(DEST, n))
        if mesh:
            for i in range(len(mesh.get_editor_property("static_materials"))):
                mesh.set_material(i, material)
            unreal.EditorAssetLibrary.save_asset(mesh.get_path_name())
            unreal.log("Harness: ready {}/{} (ODM gear material)".format(DEST, n))
else:
    unreal.log_warning("Harness: no Art/ThirdParty/ODM folder, so no ODM gear this time")

unreal.log("Harness: done. Press Play and look down.")
