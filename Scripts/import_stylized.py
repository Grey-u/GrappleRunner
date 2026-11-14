# =====================================================================================
#  import_stylized.py  -  run inside the Unreal editor (paste into the Python box at the bottom):
#     exec(open("C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/import_stylized.py").read())
# =====================================================================================
#
#  WHAT IT DOES
#  Brings our own anime-style nature meshes (Art/Stylized/*.fbx, made by
#  make_stylized_meshes.py) into Content/GrappleRunner/Nature, and makes their materials:
#    M_GR_Foliage   grass, flowers, bushes, stones: colour from the mesh's vertex colours,
#                   two-sided, and a WIND SWAY that moves blade tips (vertex alpha = how much)
#    M_GR_Cloud     soft white clouds, lifted so their shadows stay pastel, not grey
#    M_GR_Bird      wings flap up and down (wing tips marked by vertex alpha)
#    M_GR_Butterfly the same, small and fast
#  The game code (GRStylizedNature) then plants and flies them when a level starts.
#
#  TWEAKING: open a material in Content/GrappleRunner/Nature and change the numbers in its
#  "Custom" node (e.g. 9.0 = how far grass sways, in cm). Safe to run again.
# =====================================================================================
import unreal, os, glob

DEST = "/Game/GrappleRunner/Nature"
SRC = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Art", "Stylized")
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()

# ---- 1. materials ----
WIND = r"""
float w = pow(saturate(A), 1.5);                 // 0 at the root, 1 at the tip
float ph = (P.x + P.y) * 0.004;                  // neighbours sway slightly out of step
float g = 0.6 + 0.4 * sin(T * 0.35 + P.x * 0.0007);   // slow gusts roll across the field
return float3(sin(T * 1.8 + ph) * 9.0 * g, cos(T * 1.3 + ph * 1.3) * 5.0 * g, 0) * w;
"""
FLAP_BIRD = "return float3(0, 0, A * 38.0 * sin(T * 7.0 + R * 6.283));"
FLAP_FLY = "return float3(0, 0, A * 5.0 * sin(T * 24.0 + R * 6.283));"

def make_material(name, two_sided, glow, wpo_code=None, wpo_inputs=()):
    path = DEST + "/" + name
    if eal.does_asset_exist(path):
        eal.delete_asset(path)                       # remake so changes to this script apply
    mat = tools.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("two_sided", two_sided)
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -800, 0)
    rgb = mel.create_material_expression(mat, unreal.MaterialExpressionComponentMask, -600, 0)
    for ch, on in (("r", True), ("g", True), ("b", True), ("a", False)):
        rgb.set_editor_property(ch, on)
    mel.connect_material_expressions(vc, "", rgb, "")
    mel.connect_material_property(rgb, "", unreal.MaterialProperty.MP_BASE_COLOR)
    lift = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -400, 120)
    lift.set_editor_property("const_b", glow)        # a little self-light: soft anime shadows
    mel.connect_material_expressions(rgb, "", lift, "A")
    mel.connect_material_property(lift, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 240)
    rough.set_editor_property("r", 0.9)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    spec = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 320)
    spec.set_editor_property("r", 0.15)
    mel.connect_material_property(spec, "", unreal.MaterialProperty.MP_SPECULAR)
    if wpo_code:
        c = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -400, 450)
        c.set_editor_property("code", wpo_code)
        c.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
        ins = []
        for n in wpo_inputs:
            ci = unreal.CustomInput(); ci.set_editor_property("input_name", n); ins.append(ci)
        c.set_editor_property("inputs", ins)
        src = {"A": (vc, "A"),
               "T": (mel.create_material_expression(mat, unreal.MaterialExpressionTime, -800, 450), ""),
               "P": (mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -800, 550), ""),
               "R": (mel.create_material_expression(mat, unreal.MaterialExpressionPerInstanceRandom, -800, 650), "")}
        for n in wpo_inputs:
            node, pin = src[n]
            mel.connect_material_expressions(node, pin, c, n)
        mel.connect_material_property(c, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat

eal.make_directory(DEST)
M = {
    "Foliage": make_material("M_GR_Foliage", True, 0.18, WIND, ("A", "T", "P")),
    "Cloud": make_material("M_GR_Cloud", False, 0.45),
    "Bird": make_material("M_GR_Bird", True, 0.25, FLAP_BIRD, ("A", "T", "R")),
    "Butterfly": make_material("M_GR_Butterfly", True, 0.3, FLAP_FLY, ("A", "T", "R")),
}

# ---- 2. meshes ----
# Use the classic FBX importer for this (it keeps the options below exactly), then put the
# setting back.
unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 0")
tasks = []
for f in sorted(glob.glob(os.path.join(SRC, "*.fbx"))):
    ui = unreal.FbxImportUI()
    ui.set_editor_property("import_mesh", True)
    ui.set_editor_property("import_as_skeletal", False)
    ui.set_editor_property("import_materials", False)
    ui.set_editor_property("import_textures", False)
    ui.set_editor_property("mesh_type_to_import", unreal.FBXImportType.FBXIT_STATIC_MESH)
    sm = ui.static_mesh_import_data
    sm.set_editor_property("combine_meshes", True)
    sm.set_editor_property("vertex_color_import_option", unreal.VertexColorImportOption.REPLACE)
    sm.set_editor_property("auto_generate_collision", False)
    sm.set_editor_property("generate_lightmap_u_vs", False)
    t = unreal.AssetImportTask()
    t.set_editor_property("filename", f)
    t.set_editor_property("destination_path", DEST)
    t.set_editor_property("automated", True)
    t.set_editor_property("replace_existing", True)
    t.set_editor_property("save", True)
    t.set_editor_property("options", ui)
    tasks.append(t)
tools.import_asset_tasks(tasks)
unreal.SystemLibrary.execute_console_command(None, "Interchange.FeatureFlags.Import.FBX 1")

# ---- 3. give each mesh its material ----
done = 0
for f in sorted(glob.glob(os.path.join(SRC, "*.fbx"))):
    name = os.path.splitext(os.path.basename(f))[0]
    mesh = unreal.load_asset(DEST + "/" + name)
    if not mesh:
        print("NOT IMPORTED:", name); continue
    kind = "Cloud" if "Cloud" in name else "Bird" if "Bird" in name else "Butterfly" if "Butterfly" in name else "Foliage"
    mesh.set_material(0, M[kind])
    eal.save_loaded_asset(mesh)
    done += 1
print("DONE import_stylized: %d meshes in %s. Press Play." % (done, DEST))
