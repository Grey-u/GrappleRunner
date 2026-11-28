# =====================================================================================
#  blade_look.py  -  makes the ODM blades read as real steel, not white plastic
# =====================================================================================
#  WHAT THIS IS
#  In game the blades looked like flat white planks: a near-white, very shiny metal just
#  mirrors the bright sky. Real blade steel is darker and slightly satin, with fine lengthwise
#  polishing lines that catch the light, and a brighter honed strip along the edge.
#  This builds two materials and puts them on SM_ODMSword:
#    M_GR_BladeSteel  satin steel with fine lengthwise brushing (the flat of the blade)
#    M_GR_BladeEdge   brighter, smoother steel (the sharpened edge strip)
#    M_GR_Glove       dark leather for the hands that hold the blades (GRArsenal uses it)
#  The handle keeps its dark materials.
#
#  HOW TO RUN (Python box at the bottom of the editor):
#    exec(open("C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/blade_look.py").read())
# =====================================================================================
import unreal

DEST = "/Game/GrappleRunner/Weapons/Materials"
SWORD = "/Game/GrappleRunner/Weapons/SM_ODMSword"
mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def make(name, base, rough_lo, rough_hi, streak_scale, metal=1.0):
    path = DEST + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    m = tools.create_asset(name, DEST, unreal.Material, unreal.MaterialFactoryNew())
    # colour: steel grey with the faintest cool tint
    col = mel.create_material_expression(m, unreal.MaterialExpressionConstant3Vector, -600, -200)
    col.set_editor_property("constant", unreal.LinearColor(base[0], base[1], base[2], 1))
    mel.connect_material_property(col, "", unreal.MaterialProperty.MP_BASE_COLOR)
    met = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -600, -50)
    met.set_editor_property("r", metal)
    mel.connect_material_property(met, "", unreal.MaterialProperty.MP_METALLIC)
    # brushing: noise stretched hugely along the blade (UV U) and tight across it (UV V),
    # which gives fine parallel polishing lines; it varies the roughness between lo and hi
    uv = mel.create_material_expression(m, unreal.MaterialExpressionTextureCoordinate, -1000, 100)
    uv.set_editor_property("u_tiling", 2.0)
    uv.set_editor_property("v_tiling", streak_scale)
    noise = mel.create_material_expression(m, unreal.MaterialExpressionNoise, -800, 100)
    noise.set_editor_property("scale", 1.0)
    noise.set_editor_property("levels", 2)
    noise.set_editor_property("output_min", 0.0)
    noise.set_editor_property("output_max", 1.0)
    app = mel.create_material_expression(m, unreal.MaterialExpressionAppendVector, -900, 100)
    zero = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -1000, 250)
    mel.connect_material_expressions(uv, "", app, "A")
    mel.connect_material_expressions(zero, "", app, "B")
    mel.connect_material_expressions(app, "", noise, "Position")
    lerp = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -400, 100)
    lo = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -600, 60)
    lo.set_editor_property("r", rough_lo)
    hi = mel.create_material_expression(m, unreal.MaterialExpressionConstant, -600, 160)
    hi.set_editor_property("r", rough_hi)
    mel.connect_material_expressions(lo, "", lerp, "A")
    mel.connect_material_expressions(hi, "", lerp, "B")
    mel.connect_material_expressions(noise, "", lerp, "Alpha")
    mel.connect_material_property(lerp, "", unreal.MaterialProperty.MP_ROUGHNESS)
    mel.recompile_material(m)
    unreal.EditorAssetLibrary.save_asset(path)
    return m


steel = make("M_GR_BladeSteel", (0.46, 0.48, 0.51), 0.26, 0.33, 600.0)
edge = make("M_GR_BladeEdge", (0.80, 0.82, 0.85), 0.10, 0.14, 900.0)

# Gloves for the hands holding the blades: dark brown leather, not mannequin plastic.
glove = make("M_GR_Glove", (0.035, 0.025, 0.02), 0.55, 0.75, 40.0, metal=0.0)


mesh = unreal.load_asset(SWORD)
done = []
if mesh:
    for i, sm in enumerate(mesh.get_editor_property("static_materials")):
        n = str(sm.get_editor_property("material_slot_name")).lower()
        mat_name = sm.get_editor_property("material_interface").get_name().lower() if sm.get_editor_property("material_interface") else ""
        if "edge" in n or "edge" in mat_name:
            mesh.set_material(i, edge); done.append("%d edge" % i)
        elif "steel" in n or "steel" in mat_name:
            mesh.set_material(i, steel); done.append("%d steel" % i)
    unreal.EditorAssetLibrary.save_asset(SWORD)
unreal.log("BLADE LOOK: DONE, slots changed: " + ", ".join(done))
