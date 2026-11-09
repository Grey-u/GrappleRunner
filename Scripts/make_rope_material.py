"""
Grapple Runner: make M_Rope, a simple material with a "Color" setting, so the ropes can be orange.

Run (once):  py "C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/make_rope_material.py"

Why: test T-03 found the ropes drawing grey. The C++ asks the rope's material for a
setting called "Color", but Unreal's default shape material doesn't have one. This makes
a tiny material that does. GrappleRig loads /Game/GrappleRunner/Materials/M_Rope automatically.
Safe to re-run: it does nothing if M_Rope already exists.

HOW TO READ THIS (no coding experience needed)
- Lines starting with # are notes for humans; the computer skips them.
- "import unreal" loads Unreal Engine's own Python toolbox.
- A material is built from "nodes" wired together, like in the Material Editor. Here we
  make two nodes in code: a colour setting (wired to Base Color) and a number (wired to
  Roughness, i.e. how shiny the rope is), exactly as you would by dragging wires.
"""
import unreal

FOLDER, NAME = "/Game/GrappleRunner/Materials", "M_Rope"
PATH = FOLDER + "/" + NAME
lib = unreal.MaterialEditingLibrary

if unreal.EditorAssetLibrary.does_asset_exist(PATH):
    unreal.log("Grapple Runner: M_Rope already exists, nothing to do")
else:
    # 1. a new, empty material
    mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        NAME, FOLDER, unreal.Material, unreal.MaterialFactoryNew())
    # 2. a colour setting called "Color" (the C++ changes this to RopeColor), wired to Base Color
    colour = lib.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -400, 0)
    colour.set_editor_property("parameter_name", "Color")
    colour.set_editor_property("default_value", unreal.LinearColor(1.0, 0.25, 0.04, 1.0))
    lib.connect_material_property(colour, "", unreal.MaterialProperty.MP_BASE_COLOR)
    # 3. roughness 0.6 = a matte, rope-like surface (0 = mirror, 1 = chalk)
    rough = lib.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, 200)
    rough.set_editor_property("r", 0.6)
    lib.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)
    # 4. build it and save it
    lib.recompile_material(mat)
    unreal.EditorAssetLibrary.save_asset(PATH)
    unreal.log("Grapple Runner: made " + PATH)
