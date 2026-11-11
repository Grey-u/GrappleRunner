"""
Grapple Runner: import the planet pictures and build the planet material.

Run (in Unreal's Output Log, Python box):
    py "C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/add_planets.py"

WHAT IT DOES
1. Imports Art/Planets/T_Planet_*.png (painted by Art/Planets/make_planets.py) into
   /Game/GrappleRunner/Sky/Planets.
2. Builds M_Planet, ONE material every planet shares. The C++ (GRPlanets.cpp) makes a copy per
   planet and fills in its settings:
     Surface  the planet's picture
     SunDir   which way the sun is, so the lit side faces it (crescents at dawn and dusk)
     Shade    1 = shade it like a ball (planets), 0 = flat (the ring)
     Glow     how bright (dim by day, bright at night)
     Opacity  how solid (planets fade a little into a bright day sky)
   It's "unlit" (it makes its own light, like a TV screen), "translucent" (can be see-through,
   for the ring) and ignores the fog, so the mist doesn't swallow it.

HOW TO READ THIS (no coding experience needed)
- Lines starting with # are notes for humans; the computer skips them.
- A material is a little graph: boxes ("expressions") wired into the surface's inputs.
"""
import os

import unreal

PROJECT = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(PROJECT, "Art", "Planets")
DEST = "/Game/GrappleRunner/Sky/Planets"
tools = unreal.AssetToolsHelpers.get_asset_tools()
assets = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
P = unreal.MaterialProperty

# ---- 1. the pictures ----
jobs = []
for f in sorted(os.listdir(SRC)):
    if f.startswith("T_Planet_") and f.endswith(".png"):
        t = unreal.AssetImportTask()
        t.set_editor_property("filename", os.path.join(SRC, f))
        t.set_editor_property("destination_path", DEST)
        t.set_editor_property("automated", True)
        t.set_editor_property("replace_existing", True)
        t.set_editor_property("save", True)
        jobs.append(t)
tools.import_asset_tasks(jobs)

# ---- 2. the material ----
path = DEST + "/M_Planet"
if assets.does_asset_exist(path):
    m = unreal.load_asset(path)
    mel.delete_all_material_expressions(m)
else:
    m = tools.create_asset("M_Planet", DEST, unreal.Material, unreal.MaterialFactoryNew())
m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
m.set_editor_property("two_sided", True)            # the ring is seen from both sides
for prop in ("use_translucency_vertex_fog", "apply_fogging", "apply_cloud_fogging", "compute_fog_per_pixel"):
    try:
        m.set_editor_property(prop, False)          # don't fade into the fog (name differs between versions)
    except Exception:
        pass


def param(kind, name, value, x, y):
    node = mel.create_material_expression(m, kind, x, y)
    node.set_editor_property("parameter_name", name)
    node.set_editor_property("default_value", value)
    return node


tex = mel.create_material_expression(m, unreal.MaterialExpressionTextureSampleParameter2D, -900, 0)
tex.set_editor_property("parameter_name", "Surface")
first = unreal.load_asset(DEST + "/T_Planet_Vael")
if first:
    tex.set_editor_property("texture", first)
sun = param(unreal.MaterialExpressionVectorParameter, "SunDir", unreal.LinearColor(0, 0, 1, 0), -900, 260)
shade_on = param(unreal.MaterialExpressionScalarParameter, "Shade", 1.0, -900, 400)
glow = param(unreal.MaterialExpressionScalarParameter, "Glow", 1.0, -500, -120)
opacity = param(unreal.MaterialExpressionScalarParameter, "Opacity", 1.0, -500, 520)

# light = how much this point faces the sun: dot(surface direction, sun direction), never below 0
normal = mel.create_material_expression(m, unreal.MaterialExpressionVertexNormalWS, -900, 160)
dot = mel.create_material_expression(m, unreal.MaterialExpressionDotProduct, -700, 200)
mel.connect_material_expressions(normal, "", dot, "A")
mel.connect_material_expressions(sun, "", dot, "B")
sat = mel.create_material_expression(m, unreal.MaterialExpressionSaturate, -560, 200)
mel.connect_material_expressions(dot, "", sat, "")
# a little light on the dark side too (planetshine), so it never vanishes completely
lift = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -420, 200)
lift.set_editor_property("const_a", 0.08)
lift.set_editor_property("const_b", 1.0)
mel.connect_material_expressions(sat, "", lift, "Alpha")
# Shade 0 (the ring) = always fully lit
pick = mel.create_material_expression(m, unreal.MaterialExpressionLinearInterpolate, -280, 260)
pick.set_editor_property("const_a", 1.0)
mel.connect_material_expressions(lift, "", pick, "B")
mel.connect_material_expressions(shade_on, "", pick, "Alpha")

lit = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -150, 60)
mel.connect_material_expressions(tex, "RGB", lit, "A")
mel.connect_material_expressions(pick, "", lit, "B")
bright = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, 0, 0)
mel.connect_material_expressions(lit, "", bright, "A")
mel.connect_material_expressions(glow, "", bright, "B")
mel.connect_material_property(bright, "", P.MP_EMISSIVE_COLOR)

op = mel.create_material_expression(m, unreal.MaterialExpressionMultiply, -150, 460)
mel.connect_material_expressions(tex, "A", op, "A")
mel.connect_material_expressions(opacity, "", op, "B")
mel.connect_material_property(op, "", P.MP_OPACITY)

mel.recompile_material(m)
assets.save_asset(path)
unreal.log("Grapple Runner planets: %d pictures imported, M_Planet built" % len(jobs))
