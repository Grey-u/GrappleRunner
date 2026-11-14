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
#    M_GR_ToonLeaves / M_GR_ToonBark  the painterly trees and leafy bushes (make_painterly_trees.py):
#                   their own two-tone light from the sun direction (lime facing the sun, teal
#                   away), painted leaf texture with see-through edges, wind sway
#    MPC_GRToon     a 'parameter collection' holding the sun's direction; the day/night clock
#                   (GRDayNight) updates it every frame so the trees' light follows the sun
#  The game code (GRStylizedNature) then plants and flies them when a level starts.
#
#  TWEAKING: open a material in Content/GrappleRunner/Nature and change the numbers in its
#  "Custom" node (e.g. 9.0 = how far grass sways, in cm). Safe to run again: it re-imports the
#  meshes (new ones like the moss get added) and keeps the materials that already exist.
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
        return unreal.load_asset(path)               # already made (deleting it while meshes use it would ask questions)
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
    "Foliage": make_material("M_GR_Foliage", True, 0.0, WIND, ("A", "T", "P")),
    "Cloud": make_material("M_GR_Cloud", False, 0.0),
    "Bird": make_material("M_GR_Bird", True, 0.0, FLAP_BIRD, ("A", "T", "R")),
    "Butterfly": make_material("M_GR_Butterfly", True, 0.0, FLAP_FLY, ("A", "T", "R")),
}

# ---- 1b. the painted leaf texture, the sun-direction collection, the toon tree materials ----
TEX_SRC = os.path.join(SRC, "Textures", "T_GR_LeafBrush.png")
tt = unreal.AssetImportTask()
tt.set_editor_property("filename", TEX_SRC); tt.set_editor_property("destination_path", DEST)
tt.set_editor_property("automated", True); tt.set_editor_property("replace_existing", True); tt.set_editor_property("save", True)
tools.import_asset_tasks([tt])
leaf_tex = unreal.load_asset(DEST + "/T_GR_LeafBrush")

MPC_PATH = DEST + "/MPC_GRToon"
if eal.does_asset_exist(MPC_PATH):
    mpc = unreal.load_asset(MPC_PATH)
else:
    mpc = tools.create_asset("MPC_GRToon", DEST, unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    vp = unreal.CollectionVectorParameter()
    vp.set_editor_property("parameter_name", "SunDir")
    vp.set_editor_property("default_value", unreal.LinearColor(0.45, -0.35, 0.82, 0.0))
    mpc.set_editor_property("vector_parameters", [vp])
    eal.save_loaded_asset(mpc)

TOON = r"""
float d = dot(normalize(N), normalize(S.xyz));          // facing the sun = 1, away = -1
float lit = smoothstep(0.05, 0.16, d);                  // crisp edge between the two tones
float mid = smoothstep(-0.10, 0.05, d) * (1.0 - lit);   // a thin mid tone just before the edge
float3 c = lerp(Shadow.rgb, Lit.rgb, lit) + (lerp(Shadow.rgb, Lit.rgb, 0.5) - Shadow.rgb) * mid;
return c * lerp(0.8, 1.05, Tex.r) * lerp(0.7, 1.0, AO.r);
"""

def toon_material(name, lit, shadow, leaves):
    path = DEST + "/" + name
    if eal.does_asset_exist(path):
        return unreal.load_asset(path)
    mat = tools.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    mat.set_editor_property("used_with_instanced_static_meshes", True)
    if leaves:
        mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_MASKED)
        mat.set_editor_property("two_sided", True)
    vc = mel.create_material_expression(mat, unreal.MaterialExpressionVertexColor, -900, 0)
    nrm = mel.create_material_expression(mat, unreal.MaterialExpressionVertexNormalWS, -900, 150)
    sun = mel.create_material_expression(mat, unreal.MaterialExpressionCollectionParameter, -900, 250)
    sun.set_editor_property("collection", mpc); sun.set_editor_property("parameter_name", "SunDir")
    c = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -450, 0)
    c.set_editor_property("code", TOON)
    c.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    ins = []
    for n in ("N", "S", "Tex", "AO", "Lit", "Shadow"):
        ci = unreal.CustomInput(); ci.set_editor_property("input_name", n); ins.append(ci)
    c.set_editor_property("inputs", ins)
    mel.connect_material_expressions(nrm, "", c, "N")
    mel.connect_material_expressions(sun, "", c, "S")
    mel.connect_material_expressions(vc, "", c, "AO")
    for i, (pname, val) in enumerate((("Lit", lit), ("Shadow", shadow))):
        v = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -900, 400 + i * 100)
        v.set_editor_property("parameter_name", pname); v.set_editor_property("default_value", val)
        mel.connect_material_expressions(v, "", c, pname)
    if leaves:
        tx = mel.create_material_expression(mat, unreal.MaterialExpressionTextureSample, -900, 650)
        tx.set_editor_property("texture", leaf_tex)
        mel.connect_material_expressions(tx, "", c, "Tex")
        mel.connect_material_property(tx, "A", unreal.MaterialProperty.MP_OPACITY_MASK)
    else:
        one = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -900, 650)
        one.set_editor_property("r", 1.0)
        mel.connect_material_expressions(one, "", c, "Tex")
    mel.connect_material_property(c, "", unreal.MaterialProperty.MP_BASE_COLOR)
    glow = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -200, 150)
    glow.set_editor_property("const_b", 0.0)            # no glow: the level's bright fixed exposure turned any glow white
    mel.connect_material_expressions(c, "", glow, "A")
    mel.connect_material_property(glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -200, 300)
    rough.set_editor_property("r", 0.9)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    w = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -450, 500)
    w.set_editor_property("code", WIND.replace("9.0", "14.0").replace("5.0", "8.0") if leaves else "return float3(0, 0, 0);")
    w.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
    wi = []
    for n in ("A", "T", "P"):
        ci = unreal.CustomInput(); ci.set_editor_property("input_name", n); wi.append(ci)
    w.set_editor_property("inputs", wi)
    mel.connect_material_expressions(vc, "A", w, "A")
    mel.connect_material_expressions(mel.create_material_expression(mat, unreal.MaterialExpressionTime, -900, 800), "", w, "T")
    mel.connect_material_expressions(mel.create_material_expression(mat, unreal.MaterialExpressionWorldPosition, -900, 900), "", w, "P")
    mel.connect_material_property(w, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)
    mel.recompile_material(mat)
    eal.save_loaded_asset(mat)
    return mat

M["ToonLeaves"] = toon_material("M_GR_ToonLeaves", unreal.LinearColor(0.78, 1.0, 0.16, 1), unreal.LinearColor(0.20, 0.38, 0.42, 1), True)
M["ToonBark"] = toon_material("M_GR_ToonBark", unreal.LinearColor(0.62, 0.54, 0.50, 1), unreal.LinearColor(0.30, 0.28, 0.32, 1), False)

# ---- 1c. NO GLOW (14 Nov): the level uses a very bright fixed exposure, so even a little glow
#      blew our colours out to white (white clouds, pale moss, invisible grass). Turn it off
#      in materials made by earlier runs of this script too.
for n in ("M_GR_Foliage", "M_GR_Cloud", "M_GR_Bird", "M_GR_Butterfly", "M_GR_ToonLeaves", "M_GR_ToonBark"):
    m = unreal.load_asset(DEST + "/" + n)
    if not m:
        continue
    try:
        node = mel.get_material_property_input_node(m, unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        if node and isinstance(node, unreal.MaterialExpressionMultiply):
            node.set_editor_property("const_b", 0.0)
            mel.recompile_material(m)
            eal.save_loaded_asset(m)
            print("no glow:", n)
    except Exception as ex:
        print("could not turn glow off in", n, ex)

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
    sm.set_editor_property("normal_import_method", unreal.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS)   # keep the painted-look normals
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
    if name.startswith("SM_GR_Tree") or name.startswith("SM_GR_LeafBush"):
        mesh.set_material(0, M["ToonBark"])      # slot 0 = trunk and branches
        mesh.set_material(1, M["ToonLeaves"])    # slot 1 = leaf cards
        eal.save_loaded_asset(mesh)
        done += 1
        continue
    kind = "Cloud" if "Cloud" in name else "Bird" if "Bird" in name else "Butterfly" if "Butterfly" in name else "Foliage"
    mesh.set_material(0, M[kind])
    eal.save_loaded_asset(mesh)
    done += 1
print("DONE import_stylized: %d meshes in %s. Press Play." % (done, DEST))
