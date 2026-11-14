# =====================================================================================
#  anime_world.py  -  run inside the Unreal editor (paste into the Python box at the bottom):
#     exec(open("C:/Users/User/Documents/Unreal Projects/GrappleRunner/Scripts/anime_world.py").read())
# =====================================================================================
#
#  WHAT IT DOES (Grey, 14 Nov: "no genshin art style, dull sky, no clouds, rocks look like paper")
#  1. M_AnimeWorld: one post-process material that turns the WHOLE picture anime (the old
#     M_GenshinToon only touched characters). Step by step, for every pixel:
#       - PAINTERLY: a "Kuwahara" filter smooths photo-texture noise into flat brush strokes
#         but keeps edges sharp (the classic painted-anime trick)
#       - TOON LIGHT: works out how much light each pixel gets (final colour / surface colour)
#         and snaps it to two tones, lit and shadow, with a soft edge; shadows go lilac
#       - COLOUR: richer saturation
#       - SKY: an anime sky in place of the dull one: deep blue overhead fading to pale cyan,
#         soft painted clouds that drift, a pale sea of cloud below the horizon; it still
#         darkens with the day/night clock
#       - HAZE: far islands fade into the sky colour (anime "aerial perspective")
#       - OUTLINES: thin coloured lines where depth jumps, near you only
#       - CHARACTERS: a warm rim light round anyone with custom depth (you, the partner)
#  2. Puts it on both levels in place of M_GenshinToon / M_AnimeOutline, and thins the fog.
#  3. PAPER ROCKS: finds rocks that are open shells (scanned cliff faces with no back, or
#     flat slabs) and swaps each for a rock that's solid all the way round, the same size.
#
#  TWEAKING: open M_AnimeWorld (Content/GrappleRunner/Materials) and change the green
#  parameter nodes. ToonAmount 0 = no toon light, Brush 0 = no painterly, Outline 0 = no lines,
#  SkyAmount 0 = original sky. Safe to run again.
# =====================================================================================
import unreal, random

MAT_DIR = "/Game/GrappleRunner/Materials"
LEVELS = ["/Game/GrappleRunner/Maps/L_FloatingIslands_2", "/Game/GrappleRunner/Maps/L_Endless"]
mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
def log(m): unreal.log("ANIME WORLD: " + str(m))

CODE = r"""
float2 uv = GetDefaultSceneTextureUV(Parameters, 14);
float2 px = View.ViewSizeAndInvSize.zw;
float3 lw = float3(0.299, 0.587, 0.114);
float3 scene = SceneTextureLookup(uv, 14, false).rgb + (Scene.r + CD.r + BC.r + SD.r) * 0.0;
float d = SceneTextureLookup(uv, 1, false).r;
float sky = d > 380000.0 ? 1.0 : 0.0;              // the sky dome is 4 km away

// 1. painterly (Kuwahara): of 4 little squares round the pixel, take the calmest one's average
float3 pick = scene;
float best = 1e9;
[unroll] for (int k = 0; k < 4; k++)
{
    float2 o = float2((k & 1) ? 0.0 : -2.0, (k & 2) ? 0.0 : -2.0);
    float3 mu = 0; float3 sq = 0;
    [unroll] for (int i = 0; i < 3; i++)
    [unroll] for (int j = 0; j < 3; j++)
    {
        float3 c = SceneTextureLookup(uv + (o + float2(i, j)) * px * Brush, 14, false).rgb;
        mu += c; sq += c * c;
    }
    mu /= 9.0;
    float3 var = abs(sq / 9.0 - mu * mu);
    float v = var.r + var.g + var.b;
    if (v < best) { best = v; pick = mu; }
}

// 2. toon light: light = what you see / the surface's own colour, snapped to two tones
float3 alb = pow(max(SceneTextureLookup(uv, 5, false).rgb, 0.0001), 1.0 / 2.2);
float shade = dot(pick, lw) / max(dot(alb, lw), 0.04);
float band = smoothstep(Threshold - 0.08, Threshold + 0.08, shade);
float3 toon = alb * lerp(ShadowTint.rgb * ShadowLevel, float3(LitLevel, LitLevel, LitLevel), band);
toon = lerp(toon, pick, 0.35);                      // keep a little real light so shapes read
float nearW = 1.0 - smoothstep(FadeStart, FadeEnd, d);
float3 col = lerp(pick, toon, ToonAmount * nearW * (1.0 - sky));

// 3. richer colour
float g = dot(col, lw);
col = lerp(float3(g, g, g), col, Saturation);

// 4. anime sky (Genshin-style: cobalt overhead, crisp puffy clouds with white tops and
//    blue bases, a sea of clouds below the horizon). Previewed against Grey's references.
struct GRNoise
{
    float h(float2 p) { return frac(abs(sin(dot(p, float2(127.1, 311.7))) * 43758.5453)); }
    float fbm(float2 q)
    {
        float n = 0.0; float a = 0.5;
        for (int o = 0; o < 5; o++)
        {
            float2 i = floor(q); float2 f = frac(q); f = f * f * (3.0 - 2.0 * f);
            n += a * lerp(lerp(h(i), h(i + float2(1, 0)), f.x), lerp(h(i + float2(0, 1)), h(i + float2(1, 1)), f.x), f.y);
            q = q * 2.03 + 11.7; a *= 0.5;
        }
        return n;
    }
};
GRNoise N;
float3 dir = -normalize(V);
float up = dir.z;
float3 skyCol = lerp(HorizonColor.rgb, ZenithColor.rgb, pow(saturate(up * 1.3), 0.55));
float2 drift = float2(T * 0.006, T * 0.003);
float2 q = dir.xy / max(up + 0.12, 0.04) * 0.85 + drift;          // clouds above: a layer overhead
float n = N.fbm(q);
float nb = N.fbm(q + float2(0.0, 0.10));                           // a step towards the horizon
float cm = smoothstep(0.535, 0.55, n) * smoothstep(-0.02, 0.10, up) * CloudAmount;   // crisp edges
float lit = smoothstep(-0.03, 0.05, n - nb);                       // tops white, bases blue
float3 cc = lerp(float3(0.60, 0.74, 0.93), float3(1.0, 1.0, 1.0), 0.25 + 0.75 * lit);
skyCol = lerp(skyCol, cc, cm * 0.97);
float2 p2 = dir.xy / max(-up + 0.03, 0.03) * 0.35 + drift * 0.5;  // the sea of clouds below
float n2 = N.fbm(p2);
float nb2 = N.fbm(p2 - float2(0.0, 0.12));
float m2 = smoothstep(0.40, 0.42, n2);
float lit2 = smoothstep(-0.03, 0.06, n2 - nb2);
float3 sea = lerp(float3(0.50, 0.68, 0.92), lerp(float3(0.70, 0.82, 0.96), float3(1.0, 1.0, 1.0), 0.3 + 0.7 * lit2), m2);
sea = lerp(sea, HorizonColor.rgb, (1.0 - smoothstep(0.0, 0.25, -up)) * 0.75);   // hazy near the horizon
skyCol = lerp(skyCol, sea, 1.0 - smoothstep(-0.02, 0.0, up));
float bright = saturate(dot(scene, lw) * 1.8);     // follows the day/night clock
skyCol = lerp(scene, skyCol * lerp(0.35, 1.0, bright), lerp(0.4, 1.0, bright) * SkyAmount);
col = lerp(col, skyCol, sky);

// 5. haze: far islands fade into the horizon colour
float haze = smoothstep(15000.0, 200000.0, d) * (1.0 - sky) * HazeAmount * bright;
col = lerp(col, HorizonColor.rgb, haze);

// 6. outlines where depth jumps, near you only
float l = SceneTextureLookup(uv + float2(-px.x, 0) * Thickness, 1, false).r;
float r = SceneTextureLookup(uv + float2( px.x, 0) * Thickness, 1, false).r;
float t = SceneTextureLookup(uv + float2(0, -px.y) * Thickness, 1, false).r;
float b = SceneTextureLookup(uv + float2(0,  px.y) * Thickness, 1, false).r;
float mn = min(min(l, r), min(t, b)); float mx = max(max(l, r), max(t, b));
float edge = saturate((mx - mn) / max(mn, 1.0) * 4.0 - EdgeThreshold) * (1.0 - smoothstep(2500.0, 6000.0, d)) * (1.0 - sky);
col = lerp(col, col * 0.35 + Ink.rgb, edge * Outline);

// 7. characters: warm rim light (custom depth)
float c = SceneTextureLookup(uv, 13, false).r;
float isChar = (c < 100000.0 && c <= d + 5.0) ? 1.0 : 0.0;
float2 rp = px * 4.0;
float farR = max(max(SceneTextureLookup(uv + float2(-rp.x, 0), 13, false).r, SceneTextureLookup(uv + float2(rp.x, 0), 13, false).r), SceneTextureLookup(uv + float2(0, -rp.y), 13, false).r);
float rim = isChar * saturate((farR - c) / max(c, 1.0) * 4.0 - 0.3);
col += RimColor.rgb * rim * RimStrength;
return col;
"""

PARAMS = [  # name, kind, default
    ("Brush", "s", 1.0), ("Threshold", "s", 0.55), ("ShadowLevel", "s", 0.62), ("LitLevel", "s", 1.06),
    ("ToonAmount", "s", 0.7), ("FadeStart", "s", 8000.0), ("FadeEnd", "s", 40000.0), ("Saturation", "s", 1.3),
    ("SkyAmount", "s", 1.0), ("CloudAmount", "s", 0.9), ("HazeAmount", "s", 0.5),
    ("Thickness", "s", 1.0), ("EdgeThreshold", "s", 0.6), ("Outline", "s", 0.55), ("RimStrength", "s", 0.35),
    ("ShadowTint", "v", unreal.LinearColor(0.72, 0.80, 0.98, 1)),
    ("ZenithColor", "v", unreal.LinearColor(0.12, 0.40, 0.86, 1)),
    ("HorizonColor", "v", unreal.LinearColor(0.80, 0.92, 0.99, 1)),
    ("Ink", "v", unreal.LinearColor(0.03, 0.03, 0.06, 1)),
    ("RimColor", "v", unreal.LinearColor(1.0, 0.94, 0.84, 1)),
]

# ---------------- 1. the material ----------------
path = MAT_DIR + "/M_AnimeWorld"
if eal.does_asset_exist(path):
    eal.delete_asset(path)   # remake so changes to this script apply (it's only made by this script)
mat = unreal.AssetToolsHelpers.get_asset_tools().create_asset("M_AnimeWorld", MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
mat.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
for name in ("BL_SCENE_COLOR_AFTER_TONEMAPPING", "BL_AFTER_TONEMAPPING"):
    if hasattr(unreal.BlendableLocation, name):
        mat.set_editor_property("blendable_location", getattr(unreal.BlendableLocation, name)); break
custom = mel.create_material_expression(mat, unreal.MaterialExpressionCustom, -350, 0)
custom.set_editor_property("code", CODE)
custom.set_editor_property("output_type", unreal.CustomMaterialOutputType.CMOT_FLOAT3)
names = ["Scene", "CD", "BC", "SD", "V", "T"] + [p[0] for p in PARAMS]
ins = []
for n in names:
    ci = unreal.CustomInput(); ci.set_editor_property("input_name", n); ins.append(ci)
custom.set_editor_property("inputs", ins)
y = 0
for pin, tex in (("Scene", "PPI_POST_PROCESS_INPUT0"), ("CD", "PPI_CUSTOM_DEPTH"), ("BC", "PPI_BASE_COLOR"), ("SD", "PPI_SCENE_DEPTH")):
    st = mel.create_material_expression(mat, unreal.MaterialExpressionSceneTexture, -900, y)
    st.set_editor_property("scene_texture_id", getattr(unreal.SceneTextureId, tex))
    mel.connect_material_expressions(st, "Color", custom, pin); y += 120
cam = mel.create_material_expression(mat, unreal.MaterialExpressionCameraVectorWS, -900, y); y += 100
mel.connect_material_expressions(cam, "", custom, "V")
tm = mel.create_material_expression(mat, unreal.MaterialExpressionTime, -900, y); y += 100
mel.connect_material_expressions(tm, "", custom, "T")
for name, kind, value in PARAMS:
    cls = unreal.MaterialExpressionScalarParameter if kind == "s" else unreal.MaterialExpressionVectorParameter
    e = mel.create_material_expression(mat, cls, -900, y); y += 90
    e.set_editor_property("parameter_name", name)
    e.set_editor_property("default_value", value)
    mel.connect_material_expressions(e, "", custom, name)
mel.connect_material_property(custom, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
mel.recompile_material(mat)
eal.save_loaded_asset(mat)
log("M_AnimeWorld made")

# ---------------- 3 (helpers). which rocks are paper? ----------------
def edge_open_ratio(mesh):
    """Share of edges with a face on one side only (0 = closed, solid all round)."""
    try:
        md = mesh.get_static_mesh_description(0)
        n = md.get_edge_count()
        if n <= 0:
            return None
        step = max(1, n // 15000)
        bnd = cnt = 0
        for i in range(0, n, step):
            try:
                e = unreal.EdgeID(i)
            except Exception:
                e = unreal.EdgeID(id_value=i)
            if not md.is_edge_valid(e):
                continue
            cnt += 1
            if md.get_num_edge_connected_polygons(e) < 2:
                bnd += 1
        return bnd / float(max(cnt, 1))
    except Exception as ex:
        log("  (edge test unavailable for %s: %s)" % (mesh.get_name(), ex))
        return None

ROCKY = ("Rock", "Cliff", "Stone", "Boulder", "Megascans", "Fab/", "Slab", "Plane")
SKIP = ("Fern", "Foliage", "Plant", "Tree", "Beech", "Grass", "Root", "Vine", "Ivy", "Moss_", "Nature/SM_GR")
verdict = {}
def is_paper(mesh):
    p = mesh.get_path_name()
    if p in verdict:
        return verdict[p]
    res = False
    if any(s in p for s in SKIP) or not any(s in p for s in ROCKY):
        res = False
    elif "Granite_Cliff" in p or "Nordic_Coastal_Cliff" in p:
        res = True                                   # scanned cliff faces: no back
    else:
        bb = mesh.get_bounding_box()
        ext = sorted([abs(bb.max.x - bb.min.x), abs(bb.max.y - bb.min.y), abs(bb.max.z - bb.min.z)])
        if ext[2] > 100 and ext[0] / ext[2] < 0.05:
            res = True                               # a flat slab
        else:
            ratio = edge_open_ratio(mesh)
            res = ratio is not None and ratio > 0.002
            log("  %s open-edge ratio %s" % (mesh.get_name(), "?" if ratio is None else "%.4f" % ratio))
    verdict[p] = res
    if res:
        log("  PAPER: " + p)
    return res

def mesh_size(mesh):
    bb = mesh.get_bounding_box()
    return max(abs(bb.max.x - bb.min.x), abs(bb.max.y - bb.min.y), abs(bb.max.z - bb.min.z), 1.0)

# ---------------- 2 + 3. per level ----------------
asub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
lsub = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
old_mats = [m for m in (unreal.load_asset(MAT_DIR + "/" + n) for n in ("M_GenshinToon", "M_AnimeOutline")) if m]
rng = random.Random(14)
for lvl in LEVELS:
    if not eal.does_asset_exist(lvl):
        continue
    lsub.load_level(lvl)
    actors = asub.get_all_level_actors()
    # post-process + fog
    for a in actors:
        if isinstance(a, unreal.PostProcessVolume):
            s = a.get_editor_property("settings")
            wb = s.get_editor_property("weighted_blendables")
            arr = [x for x in wb.get_editor_property("array") if x.get_editor_property("object") not in old_mats + [mat]]
            arr.append(unreal.WeightedBlendable(1.0, mat))
            wb.set_editor_property("array", arr)
            s.set_editor_property("weighted_blendables", wb)
            s.set_editor_property("override_color_saturation", True)
            s.set_editor_property("color_saturation", unreal.Vector4(1.0, 1.0, 1.0, 1.0))   # the material does saturation now
            a.set_editor_property("settings", s)
            log("anime post-process on " + lvl.split("/")[-1])
        elif isinstance(a, unreal.ExponentialHeightFog) and not a.actor_has_tag("AnimeFog"):
            fc = a.get_component_by_class(unreal.ExponentialHeightFogComponent)
            dens = fc.get_editor_property("fog_density")
            fc.set_editor_property("fog_density", dens * 0.45)
            a.set_editor_property("tags", list(a.get_editor_property("tags")) + [unreal.Name("AnimeFog")])
            log("fog %.4f -> %.4f" % (dens, dens * 0.45))
    # solid rocks to swap in: the grapple rocks' mesh, plus any rock in the level that tests solid
    solid = []
    for a in actors:
        if a.actor_has_tag("GrappleAnchor"):
            c = a.get_component_by_class(unreal.StaticMeshComponent)
            if c and c.static_mesh and c.static_mesh not in solid:
                solid.append(c.static_mesh)
    swaps = 0
    for a in actors:
        if not isinstance(a, unreal.StaticMeshActor):
            continue
        c = a.static_mesh_component
        m = c.static_mesh if c else None
        if not m or not is_paper(m):
            continue
        cand = [s for s in solid if not is_paper(s)]
        if not cand:
            log("no solid rock to swap in; hiding " + a.get_actor_label())
            a.set_actor_hidden_in_game(True); a.set_is_temporarily_hidden_in_editor(True)
            continue
        new = rng.choice(cand)
        k = mesh_size(m) / mesh_size(new)
        c.set_static_mesh(new)
        sc = a.get_actor_scale3d()
        a.set_actor_scale3d(unreal.Vector(sc.x * k, sc.y * k, sc.z * k))
        swaps += 1
    log("%s: swapped %d paper rocks for solid ones" % (lvl.split("/")[-1], swaps))
    lsub.save_current_level()
log("DONE: press Play")
