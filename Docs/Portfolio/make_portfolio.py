"""
Grapple Runner portfolio (Project 02): one layout -> an InDesign file (.idml) AND a PDF preview.

Run:  python make_portfolio.py <idml template folder> <fonts folder> <out folder> [<extra images folder>]

- Pages are 960 x 540 pt (16:9), the same as the EXIT SUIT portfolio, in the same style:
  off-white paper, Inter for words, IBM Plex Mono capitals for small labels, one orange accent,
  thin rules, "FIG n.n" captions, running header and footer.
- The .idml opens in InDesign with real paragraph styles (P Body, P H1, ...) and swatches
  (P Ink, P Orange, ...), every text in its own editable frame, and the images EMBEDDED, so
  nothing needs relinking. Fonts: Inter and IBM Plex Mono (free; install them first).
- The layout below (PAGES) is the single source: change a word or a frame here and both the
  PDF and the IDML change together. Or just edit the IDML in InDesign from now on.
"""
import base64
import copy
import io
import os
import re
import shutil
import sys
import zipfile
from xml.sax.saxutils import escape

from PIL import Image
from reportlab.lib.colors import HexColor
from reportlab.lib.enums import TA_LEFT, TA_RIGHT
from reportlab.lib.styles import ParagraphStyle
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.pdfgen import canvas
from reportlab.platypus import Paragraph

HERE = os.path.dirname(os.path.abspath(__file__))
TEMPLATE, FONTS, OUT = sys.argv[1], sys.argv[2], sys.argv[3]
EXTRA = sys.argv[4] if len(sys.argv) > 4 else None
REPO = os.path.abspath(os.path.join(HERE, "..", ".."))
MEDIA = os.path.join(REPO, "Docs", "media")
LINKS = os.path.join(HERE, "Links")
os.makedirs(OUT, exist_ok=True)
W, H = 960.0, 540.0
L, R = 38.0, 922.0                      # left and right margins

# ------------------------------------------------------------------ colours (RGB 0-255)
COLORS = {"P Paper": (238, 237, 233), "P Ink": (20, 20, 20), "P Quiet": (110, 110, 106),
          "P Rule": (189, 188, 182), "P Orange": (240, 78, 35)}

# ------------------------------------------------------------------ paragraph styles
# name: (font, face, size, leading, colour, tracking, caps, space_after, single_line)
STYLES = {
    "P Header": ("IBM Plex Mono", "Regular", 6.5, 8, "P Quiet", 60, True, 0, True),
    "P Kicker": ("IBM Plex Mono", "Regular", 6.5, 8, "P Ink", 60, True, 8, True),
    "P Display": ("Inter", "Bold", 96, 82, "P Ink", -45, False, 0, True),
    "P H1": ("Inter", "SemiBold", 30, 31, "P Ink", -25, False, 14, False),
    "P Lead": ("Inter", "Regular", 13.5, 18.5, "P Ink", -10, False, 8, False),
    "P Body": ("Inter", "Regular", 8.4, 12.2, "P Ink", 0, False, 6, False),
    "P Quiet": ("Inter", "Regular", 8.4, 12.2, "P Quiet", 0, False, 6, False),
    "P Item": ("Inter", "SemiBold", 10.5, 13, "P Ink", -5, False, 3, False),
    "P Label": ("IBM Plex Mono", "Regular", 6, 8, "P Quiet", 60, True, 2, False),
    "P Caption": ("IBM Plex Mono", "Regular", 5.6, 7, "P Quiet", 60, True, 0, True),
    "P Big": ("Inter", "SemiBold", 24, 26, "P Ink", -25, False, 2, True),
    "P Num": ("Inter", "SemiBold", 16, 18, "P Orange", -10, False, 0, True),
    "P Accent": ("Inter", "Medium", 8.4, 12.2, "P Orange", 0, False, 6, False),
}
FONT_FILES = {("Inter", "Regular"): "Inter-Regular.ttf", ("Inter", "Medium"): "Inter-Medium.ttf",
              ("Inter", "SemiBold"): "Inter-SemiBold.ttf", ("Inter", "Bold"): "Inter-Bold.ttf",
              ("IBM Plex Mono", "Regular"): "IBMPlexMono-Regular.ttf", ("IBM Plex Mono", "Medium"): "IBMPlexMono-Medium.ttf"}


# ------------------------------------------------------------------ the layout
def T(x, y, w, h, *paras, right=False):
    """A text frame. paras = (style, text) pairs; '\\n' inside a text = a line break."""
    return {"kind": "text", "box": (x, y, w, h), "paras": list(paras), "right": right}


def IMG(x, y, w, h, src, crop=None, caption=None):
    return {"kind": "image", "box": (x, y, w, h), "src": src, "crop": crop, "caption": caption}


def RULE(x1, y1, x2, y2, color="P Rule", weight=0.5):
    return {"kind": "line", "pts": (x1, y1, x2, y2), "color": color, "weight": weight}


def DOT(x, y, r=2.2, color="P Orange"):
    return {"kind": "dot", "c": (x, y), "r": r, "color": color}


def frame_chrome(n, total):
    return [T(L, 22, 400, 10, ("P Header", "Project 02 — Grapple Runner")),
            T(R - 200, 22, 200, 10, ("P Header", "%02d / %02d" % (n, total)), right=True),
            T(L, 508, 300, 10, ("P Header", "Naomi Umoru")),
            T(R - 200, 508, 200, 10, ("P Header", "Portfolio 2026"), right=True)]


def section(kicker, title, *body):
    items = [T(L, 70, 250, 12, ("P Kicker", kicker)),
             T(L, 84, 262, 80, ("P H1", title))]
    if body:
        items.append(T(L, 160, 252, 300, *[("P Body", b) for b in body]))
    return items


EDITOR_VIEW = (0.235, 0.12, 0.995, 0.935)     # the game view inside an editor screenshot
V3 = os.path.join(EXTRA, "harness_v3_front.png") if EXTRA else None


def meta(y, key, value):
    return [T(L, y, 100, 12, ("P Label", key)), T(L + 104, y - 1.5, 440, 14, ("P Body", value))]


PAGES = []

# ---- 1. cover
p = [T(L, 92, 520, 10, ("P Kicker", "VR game for the EXIT SUIT · 8–10 November 2026")),
     T(L, 108, 560, 180, ("P Display", "GRAPPLE\nRUNNER")),
     T(L, 306, 470, 80, ("P Lead", "A first-person VR swinging game, built to test the EXIT SUIT: twin "
                                    "grapple ropes, rope physics you can trust, and a world of floating "
                                    "ruins. Every physics change is checked against an offline model "
                                    "before it ships.")),
     RULE(L, 392, 560, 392)]
for i, (k, v) in enumerate([("Role", "Solo developer and designer"),
                            ("Engine", "Unreal Engine 5.8 · C++ gameplay · Python editor tools"),
                            ("Methods", "Fixed-step physics · telemetry · offline model · Blender · Slate UI"),
                            ("Evidence", "14 recorded tests, T-01 to T-14, in Docs/TESTS.md"),
                            ("Status", "Version 1 (Swing) playable on desktop; VR headset tests next")]):
    p += meta(402 + i * 15.5, k, v)
p += [IMG(596, 60, 326, 420, "level_sky_after.jpg", crop=(0.08, 0.0, 0.66, 1.0),
          caption="Fig 0.1 — Aeloria, a floating island, Unreal Engine 5.8")]
PAGES.append(p)

# ---- 2. concept
p = section("01 · Concept", "The game is\nthe test rig",
            "Swinging in VR is one of the fastest ways to make a player sick. That is exactly why the "
            "EXIT SUIT needs it: the suit's job is to reduce motion sickness and help players learn "
            "their own limits, and it can only be tested against motion that pushes them.",
            "So the game is built like an instrument. One C++ class, GrappleRig, runs the physics at a "
            "fixed 120 steps a second and records every frame. A Python copy of the same maths "
            "predicts each test before it runs.",
            "The premise grows from there: five-person teams, swords or guns, eliminate the other team "
            "or take the objective. Modes: 1v1, 2v2, 3v3, 5v5 and PvE against monsters.")
p += [IMG(322, 72, 600, 0, "G1_system.png", caption="Fig 1.1 — How the parts fit together")]
PAGES.append(p)

# ---- 3. physics
p = section("02 · Physics", "A rope you\ncan trust",
            "A rope that never stretches is physically correct but brutal: when a slack rope snaps tight "
            "the player stops in one physics step. Three changes made it comfortable without faking it.")
y = 232
for n, title, text in [("01", "Soft rope", "Stretches up to 150 cm like a climbing rope, taking the load over 0.3 s. Jolt 209 g to 14.7 g (T-09)."),
                       ("02", "Progressive spring", "Stiffens as it stretches, so short ropes no longer hit the limit: 272 g to 22.7 g (T-13)."),
                       ("03", "Swing exaggeration", "Gravity x1.35 along the swing only: arcs 17% faster at the bottom, as predicted (T-14).")]:
    p += [T(L, y, 30, 20, ("P Num", n)), T(L + 30, y + 2, 222, 60, ("P Item", title), ("P Body", text))]
    y += 66
p += [IMG(322, 72, 600, 0, "G2_jolt.png", caption="Fig 2.1 — The same snap, three ropes (offline model; the game matched it within 0.12 g)"),
      RULE(322, 392, 922, 392)]
for i, (big, lab) in enumerate([("209 → 15 g", "Peak jolt, stiff to soft"), ("360 steps", "Game matched model, every step"),
                                ("+17%", "Swing speed at the bottom"), ("0.00 cm", "Largest stretch, stiff rope")]):
    x = 322 + i * 152
    p += [T(x, 402, 148, 30, ("P Big", big)), T(x, 432, 148, 20, ("P Label", lab))]
PAGES.append(p)

# ---- 4. testing
p = section("03 · Testing", "Evidence,\nnot feel",
            "Playing by hand is never the same twice, so the important cases are scripted: gr.Test sets an "
            "exact position, speed and rope, then lets the physics run with no input.",
            "Every session is recorded frame by frame to a spreadsheet, and Tools/analyse_telemetry.py "
            "turns each recording into numbers and charts. 13 of 14 tests passed or were fixed; one is a "
            "baseline.")
p += [T(322, 72, 40, 10, ("P Label", "Test")), T(366, 72, 360, 10, ("P Label", "Question")),
      T(735, 72, 187, 10, ("P Label", "Result")), RULE(322, 84, 922, 84, color="P Ink")]
tests = [("T-14", "Does exaggeration speed up arcs without breaking the physics?", "Pass: +17% (16% predicted)"),
         ("T-13", "Does the progressive spring fix the short-rope jolt?", "Pass: 271.6 to 22.7 g"),
         ("T-12", "Does auto-pull carry the player to the hook?", "Pass"),
         ("T-11", "Do the flexible and soft ropes work in real play?", "Visuals pass; found F6"),
         ("T-10", "What do 805 plants cost in frame time?", "+0.32 ms; flagged for VR"),
         ("T-09", "Does the soft rope reduce the snap jolt?", "Pass: 209 to 14.7 g"),
         ("T-08", "Frame-time baseline in the editor", "Baseline recorded"),
         ("T-07", "Does falling off the world respawn safely?", "Pass"),
         ("T-06", "Do gravity and air drag match the model?", "Pass: within 0.03%"),
         ("T-05", "Does reel-in speed match its setting?", "Pass: 900.04 of 900 cm/s"),
         ("T-04", "Does the rope ever stretch?", "Pass: 0.00 cm"),
         ("T-03", "Does aim assist find anchors?", "Pass"),
         ("T-02", "Does the player stand on the first island?", "Fail, cause found, then pass"),
         ("T-01", "Does the C++ module compile in UE 5.8?", "Pass after a compiler change")]
for i, (tid, q, r) in enumerate(tests):
    y = 90 + i * 19
    p += [T(322, y, 40, 14, ("P Label", tid)), T(366, y - 2, 360, 16, ("P Body", q)),
          T(735, y - 2, 187, 16, ("P Accent" if r.startswith("Pass") else "P Body", r)),
          RULE(322, y + 14, 922, y + 14)]
p += [IMG(322, 372, 600, 0, "G3_test_loop.png", caption="Fig 3.1 — The loop every change goes through")]
PAGES.append(p)

# ---- 5. harness
p = section("04 · Harness", "Three harnesses\nin a day",
            "The gear had to look worn, not floating. Each version was designed from real references: "
            "Attack on Titan ODM-gear cosplay builds and climbing-harness anatomy.",
            "Rules kept through every version: the ropes leave the hips, near the centre of mass, so "
            "the pull matches the physics; the thigh parts ride on the thigh bones so they swing with "
            "the legs; each piece is placed upright in the body's directions before it is attached.")
cols = [("weapons_preview.png", (0.33, 0.05, 0.72, 0.62), "V1 · Simple shapes", "One block at the hips. Proved the ropes could leave there, but looked low-poly and floated.",
         "Fig 4.1 — v1, Blender, code-built"),
        (os.path.join(MEDIA, "harness", "harness_v2_front.png"), (0.22, 0.25, 0.80, 0.98), "V2 · Real anatomy", "Climbing belt, belay loop, leg loops, blade boxes and tanks, in three pieces. Flat colours fell short of the brief.",
         "Fig 4.2 — v2, generated from references"),
        (V3, (0.12, 0.18, 0.92, 0.95), "V3 · Sculpted gear", "A sculpted, textured ODM-gear model (4K materials) held on by the v2 straps. Being fitted in game now.",
         "Fig 4.3 — v3, gear model by @besugochan (Sketchfab)")]
for i, (src, crop, title, text, cap) in enumerate(cols):
    x = 322 + i * 204
    if src and os.path.exists(src if os.path.isabs(src) else os.path.join(MEDIA, src)):
        p += [IMG(x, 72, 192, 250, src, crop=crop, caption=cap)]
    p += [T(x, 344, 192, 14, ("P Item", title)), T(x, 362, 192, 60, ("P Body", text))]
PAGES.append(p)

# ---- 6. world
p = section("05 · World", "Aeloria",
            "A floating city of islands above a sea of mist. The swing route runs 740 m past 14 landing "
            "islands, with 53 grapple rocks 11 to 21 m above it, the same spacing as the 2D test bench.",
            "Editor scripts dress every island from free asset packs: ruin clusters for cover and "
            "retreats, beech trees, ponds and waterfalls, all kept 7 m clear of the grapple rocks so "
            "every hook has a clear line. Music: Aeloria, by a musician friend.")
p += [IMG(322, 72, 600, 222, "level_lighting_pass1.png", crop=(0.0, 0.12, 1.0, 0.80),
          caption="Fig 5.1 — A landing island in the sky pass")]
if os.path.exists(os.path.join(LINKS, "G5_world_map.png")):
    p += [IMG(322, 318, 600, 0, "G5_world_map.png", caption="Fig 5.2 — The dressed route from above (dressing_layout.csv)")]
for i, (big, lab) in enumerate([("14", "Landing islands"), ("53", "Grapple rocks"), ("57", "Ruin pieces"), ("805", "Plants")]):
    x = L + (i % 2) * 128
    yy = 400 + (i // 2) * 44
    p += [T(x, yy, 120, 26, ("P Big", big)), T(x, yy + 26, 120, 12, ("P Label", lab))]
PAGES.append(p)

# ---- 7. what went wrong
p = section("06 · Problems", "What went\nwrong",
            "Six problems, each with its cause, its fix and the rule it left behind. Most came from "
            "assuming something worked instead of reading it back.")
issues = [("01", "Fell through the world", "A sky dome was solid, so the player spawned inside it.", "Made it non-solid; scripts read back every change.", "Test each possible cause on its own."),
          ("02", "Short ropes still jolted", "The scripted test only covered a long rope.", "Progressive spring; the real-play case added to gr.Test.", "Real play finds cases scripts miss."),
          ("03", "988 objects deleted", "Another window took keyboard focus mid-automation.", "Undo restored the level; nothing saved yet.", "Never send delete keys from a script."),
          ("04", "Power cut mid-install", "Two asset packs were half-installed.", "Re-added from Fab; autosave every 5 minutes.", "Save and commit every session."),
          ("05", "Harness looked cheap", "Shapes built in code, flat colours.", "Sculpted, textured model on real straps.", "Code for function, artists for looks."),
          ("06", "Trees froze the editor", "The tree pack ships 8K textures.", "Cap to 2K for VR (next).", "Check asset budgets before placing.")]
for i, (n, title, cause, fix, lesson) in enumerate(issues):
    x = 322 + (i % 2) * 308
    y = 72 + (i // 2) * 138
    p += [RULE(x, y, x + 292, y, color="P Ink"), T(x, y + 8, 30, 20, ("P Num", n)), T(x + 30, y + 10, 262, 16, ("P Item", title))]
    for k, (key, val) in enumerate([("Cause", cause), ("Fix", fix), ("Lesson", lesson)]):
        yy = y + 34 + k * 27
        p += [T(x + 30, yy + 1.5, 46, 10, ("P Label", key)), T(x + 78, yy, 214, 26, ("P Body", val))]
PAGES.append(p)

# ---- 8. next
p = section("07 · Next", "Where it goes",
            "Version 1, the swing, is playable. Each next version adds one thing and ends the same way: "
            "people play it in the headset and the comfort data decides what changes.")
p += [IMG(322, 72, 600, 0, "G6_roadmap.png", caption="Fig 7.1 — Versions, one feature each")]
for i, (title, text) in enumerate([("Measure", "The Simulator Sickness Questionnaire (Kennedy et al., 1993), before and after each session, plus the time to first discomfort."),
                                    ("Compare", "Each friend plays the same route with and without the speed vignette, the soft rope and the suit, in a shuffled order."),
                                    ("Link", "Symptom scores matched against the telemetry: peak jolts and time above a speed, to find which moments make people ill.")]):
    x = 322 + i * 204
    p += [RULE(x, 300, x + 192, 300, color="P Ink"), T(x, 308, 192, 14, ("P Item", title)), T(x, 326, 192, 80, ("P Body", text))]
p += [T(L, 300, 252, 120, ("P Label", "Comfort study"),
        ("P Quiet", "Planned for when the headset is back. Volunteers can stop at any time, sessions are short, and no names go in the published data."))]
PAGES.append(p)

TOTAL = len(PAGES)
for n, page in enumerate(PAGES, 1):
    page[0:0] = frame_chrome(n, TOTAL)


# ------------------------------------------------------------------ images: crop to each frame
def src_path(src):
    for base in ("", LINKS, MEDIA):
        cand = src if os.path.isabs(src) else os.path.join(base, src)
        if os.path.exists(cand):
            return cand
    raise FileNotFoundError(src)


prepared = {}


def prepare(el):
    """Crop the picture to exactly fill its frame; frames with height 0 take the picture's own shape."""
    x, y, w, h = el["box"]
    im = Image.open(src_path(el["src"])).convert("RGB")
    if el["crop"]:
        a, b, c, d = el["crop"]
        im = im.crop((int(a * im.width), int(b * im.height), int(c * im.width), int(d * im.height)))
    if h == 0:
        h = w * im.height / im.width
        el["box"] = (x, y, w, h)
    target = w / h
    if im.width / im.height > target:                     # too wide: trim the sides
        nw = int(im.height * target)
        im = im.crop(((im.width - nw) // 2, 0, (im.width - nw) // 2 + nw, im.height))
    else:                                                 # too tall: trim top and bottom
        nh = int(im.width / target)
        im = im.crop((0, (im.height - nh) // 2, im.width, (im.height - nh) // 2 + nh))
    scale = min(1.0, (w * 3.0) / im.width)                 # ~216 ppi at size is plenty
    im = im.resize((max(1, int(im.width * scale)), max(1, int(im.height * scale))), Image.LANCZOS)
    buf = io.BytesIO()
    im.save(buf, "JPEG", quality=88, dpi=(72, 72))
    el["jpeg"] = buf.getvalue()
    el["px"] = im.size


for page in PAGES:
    for el in page:
        if el["kind"] == "image":
            prepare(el)

# ------------------------------------------------------------------ PDF preview
def font_name(fam, face):
    return (fam + "-" + face).replace(" ", "")


for (fam, face), f in FONT_FILES.items():
    for root, _, files in os.walk(FONTS):
        if f in files:
            pdfmetrics.registerFont(TTFont(font_name(fam, face), os.path.join(root, f)))
            break


def hexcol(name):
    return HexColor("#%02X%02X%02X" % COLORS[name])


def draw_pdf(path):
    c = canvas.Canvas(path, pagesize=(W, H))
    c.setTitle("Grapple Runner — Naomi Umoru portfolio 2026")
    for page in PAGES:
        c.setFillColor(hexcol("P Paper"))
        c.rect(0, 0, W, H, stroke=0, fill=1)
        for el in page:
            if el["kind"] == "line":
                x1, y1, x2, y2 = el["pts"]
                c.setStrokeColor(hexcol(el["color"]))
                c.setLineWidth(el["weight"])
                c.line(x1, H - y1, x2, H - y2)
            elif el["kind"] == "dot":
                c.setFillColor(hexcol(el["color"]))
                c.circle(el["c"][0], H - el["c"][1], el["r"], stroke=0, fill=1)
            elif el["kind"] == "image":
                x, y, w, h = el["box"]
                from reportlab.lib.utils import ImageReader
                c.drawImage(ImageReader(io.BytesIO(el["jpeg"])), x, H - y - h, w, h)
                if el["caption"]:
                    draw_text(c, T(x, y + h + 6, w, 10, ("P Caption", el["caption"])))
            elif el["kind"] == "text":
                draw_text(c, el)
        c.showPage()
    c.save()


def draw_text(c, el):
    x, y, w, h = el["box"]
    cursor = y
    for style, text in el["paras"]:
        fam, face, size, lead, col, track, caps, after, single = STYLES[style]
        if caps:
            text = text.upper()
        fn = font_name(fam, face)
        if single:
            for line in text.split("\n"):
                cursor += size * 0.96 if cursor == y else lead
                t = c.beginText()
                t.setFont(fn, size)
                t.setFillColor(hexcol(col))
                t.setCharSpace(track / 1000.0 * size)
                lw = pdfmetrics.stringWidth(line, fn, size) + track / 1000.0 * size * (len(line) - 1)
                t.setTextOrigin(x + w - lw if el["right"] else x, H - cursor)
                t.textLine(line)
                t.setCharSpace(0)                 # letter spacing persists in a PDF: reset it
                c.drawText(t)
            cursor += after
        else:
            ps = ParagraphStyle(style, fontName=fn, fontSize=size, leading=lead, textColor=hexcol(col),
                                alignment=TA_RIGHT if el["right"] else TA_LEFT)
            para = Paragraph(escape(text).replace("\n", "<br/>"), ps)
            _, ph = para.wrap(w, 1000)
            para.drawOn(c, x, H - cursor - ph + (lead - size) * 0.2)
            cursor += ph + after


# ------------------------------------------------------------------ IDML
uid = [0]


def new_id(prefix="p"):
    uid[0] += 1
    return "%s%x" % (prefix, uid[0])


TX = "1 0 0 1 0 %s" % (-H / 2)                      # page coords -> spread coords (single page)


def rect_path(x, y, w, h):
    pts = [(x, y), (x, y + h), (x + w, y + h), (x + w, y)]
    return ("<PathGeometry><GeometryPathType PathOpen=\"false\"><PathPointArray>" +
            "".join('<PathPointType Anchor="{0} {1}" LeftDirection="{0} {1}" RightDirection="{0} {1}"/>'.format(a, b)
                    for a, b in pts) + "</PathPointArray></GeometryPathType></PathGeometry>")


def story_xml(sid, paras, right):
    rng = []
    for i, (style, text) in enumerate(paras):
        lines = text.split("\n")
        parts = []
        for j, line in enumerate(lines):
            parts.append("<Content>%s</Content>" % escape(line))
            if j < len(lines) - 1:
                parts.append("<Content>&#x2028;</Content>")          # forced line break
        if i < len(paras) - 1:
            parts.append("<Br/>")
        just = ' Justification="RightAlign"' if right else ""
        rng.append('<ParagraphStyleRange AppliedParagraphStyle="ParagraphStyle/%s"%s><CharacterStyleRange '
                   'AppliedCharacterStyle="CharacterStyle/$ID/[No character style]">%s</CharacterStyleRange>'
                   '</ParagraphStyleRange>' % (style, just, "".join(parts)))
    return ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<idPkg:Story xmlns:idPkg="http://ns.adobe.com/AdobeInDesign/idml/1.0/packaging" DOMVersion="7.5">'
            '<Story Self="%s" AppliedTOCStyle="n" TrackChanges="false" StoryTitle="$ID/" AppliedNamedGrid="n">'
            '<StoryPreference OpticalMarginAlignment="false" OpticalMarginSize="12" FrameType="TextFrameType" StoryOrientation="Horizontal" StoryDirection="LeftToRightDirection"/>'
            '<InCopyExportOption IncludeGraphicProxies="true" IncludeAllResources="false"/>%s</Story></idPkg:Story>' % (sid, "".join(rng)))


def text_frame_xml(el, sid):
    x, y, w, h = el["box"]
    return ('<TextFrame Self="%s" ParentStory="%s" PreviousTextFrame="n" NextTextFrame="n" ContentType="TextType" ItemLayer="ub3" '
            'Locked="false" FillColor="Swatch/None" StrokeWeight="0" StrokeColor="Swatch/None" '
            'AppliedObjectStyle="ObjectStyle/$ID/[Normal Text Frame]" Visible="true" Name="$ID/" ItemTransform="%s">'
            '<Properties>%s</Properties><TextFramePreference TextColumnCount="1" FirstBaselineOffset="AscentOffset" '
            'VerticalJustification="TopAlign" AutoSizingType="Off"><Properties><InsetSpacing type="list">'
            '<ListItem type="unit">0</ListItem><ListItem type="unit">0</ListItem><ListItem type="unit">0</ListItem>'
            '<ListItem type="unit">0</ListItem></InsetSpacing></Properties></TextFramePreference></TextFrame>'
            % (new_id("t"), sid, TX, rect_path(x, y, w, h + 4)))


def image_xml(el):
    x, y, w, h = el["box"]
    pw, ph = el["px"]
    s = w / pw
    data = base64.b64encode(el["jpeg"]).decode()
    return ('<Rectangle Self="%s" ContentType="GraphicType" ItemLayer="ub3" Locked="false" FillColor="Swatch/None" StrokeWeight="0" '
            'StrokeColor="Swatch/None" AppliedObjectStyle="ObjectStyle/$ID/[None]" Visible="true" Name="$ID/" ItemTransform="%s">'
            '<Properties>%s</Properties><Image Self="%s" Space="$ID/#Links_RGB" ActualPpi="72 72" EffectivePpi="%d %d" '
            'ImageRenderingIntent="UseColorSettings" LocalDisplaySetting="Default" ImageTypeName="$ID/JPEG" '
            'AppliedObjectStyle="ObjectStyle/$ID/[None]" ItemTransform="%s 0 0 %s %s %s" Visible="true" Name="$ID/">'
            '<Properties><Contents><![CDATA[%s]]></Contents><Profile type="string">$ID/Embedded</Profile>'
            '<GraphicBounds Left="0" Top="0" Right="%d" Bottom="%d"/></Properties>'
            '<Link Self="%s" AssetURL="$ID/" AssetID="$ID/" LinkResourceURI="file:%s.jpg" LinkResourceFormat="$ID/JPEG" '
            'StoredState="Embedded" LinkClassID="35906" LinkClientID="257" LinkResourceModified="false" LinkObjectModified="false" '
            'ShowInUI="true" CanEmbed="true" CanUnembed="true" CanPackage="true" ImportPolicy="NoAutoImport" ExportPolicy="NoAutoExport" '
            'LinkImportStamp="file 0 %d" LinkImportModificationTime="2026-11-10T19:00:00" LinkImportTime="2026-11-10T19:00:00" '
            'LinkResourceSize="0~%x"/></Image></Rectangle>'
            % (new_id("r"), TX, rect_path(x, y, w, h), new_id("i"), int(72 / s), int(72 / s), s, s, x, y, data, pw, ph,
               new_id("l"), os.path.splitext(os.path.basename(el["src"]))[0], len(el["jpeg"]), len(el["jpeg"])))


def line_xml(el):
    x1, y1, x2, y2 = el["pts"]
    return ('<GraphicLine Self="%s" ItemLayer="ub3" Locked="false" StrokeWeight="%s" StrokeColor="Color/%s" FillColor="Swatch/None" '
            'AppliedObjectStyle="ObjectStyle/$ID/[None]" Visible="true" Name="$ID/" ItemTransform="%s"><Properties><PathGeometry>'
            '<GeometryPathType PathOpen="true"><PathPointArray>'
            '<PathPointType Anchor="{0} {1}" LeftDirection="{0} {1}" RightDirection="{0} {1}"/>'
            '<PathPointType Anchor="{2} {3}" LeftDirection="{2} {3}" RightDirection="{2} {3}"/>'
            '</PathPointArray></GeometryPathType></PathGeometry></Properties></GraphicLine>'
            % (new_id("g"), el["weight"], el["color"], TX)).format(x1, y1, x2, y2)


def paper_xml():
    return ('<Rectangle Self="%s" ContentType="Unassigned" ItemLayer="ub3" Locked="true" FillColor="Color/P Paper" StrokeWeight="0" '
            'StrokeColor="Swatch/None" AppliedObjectStyle="ObjectStyle/$ID/[None]" Visible="true" Name="Paper" ItemTransform="%s">'
            '<Properties>%s</Properties></Rectangle>' % (new_id("bg"), TX, rect_path(0, 0, W, H)))


def write_idml(path):
    work = os.path.join(OUT, "_idml")
    shutil.rmtree(work, ignore_errors=True)
    shutil.copytree(TEMPLATE, work)
    for sub in ("Spreads", "Stories"):
        shutil.rmtree(os.path.join(work, sub))
        os.makedirs(os.path.join(work, sub))
    spreads, stories = [], []
    for n, page in enumerate(PAGES, 1):
        items = [paper_xml()]
        for el in page:
            if el["kind"] == "text":
                sid = new_id("s")
                stories.append(sid)
                open(os.path.join(work, "Stories", "Story_%s.xml" % sid), "w").write(story_xml(sid, el["paras"], el["right"]))
                items.append(text_frame_xml(el, sid))
            elif el["kind"] == "image":
                items.append(image_xml(el))
                if el["caption"]:
                    x, y, w, h = el["box"]
                    cap = T(x, y + h + 6, w, 10, ("P Caption", el["caption"]))
                    sid = new_id("s")
                    stories.append(sid)
                    open(os.path.join(work, "Stories", "Story_%s.xml" % sid), "w").write(story_xml(sid, cap["paras"], False))
                    items.append(text_frame_xml(cap, sid))
            elif el["kind"] == "line":
                items.append(line_xml(el))
        spid = new_id("sp")
        spreads.append(spid)
        xml = ('<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<idPkg:Spread xmlns:idPkg="http://ns.adobe.com/AdobeInDesign/idml/1.0/packaging" DOMVersion="7.5">'
               '<Spread Self="%s" FlattenerOverride="Default" AllowPageShuffle="false" ItemTransform="1 0 0 1 0 0" ShowMasterItems="true" '
               'PageCount="1" BindingLocation="0" PageTransitionType="None" PageTransitionDirection="NotApplicable" PageTransitionDuration="Medium">'
               '<Page Self="%s" GeometricBounds="0 0 %s %s" ItemTransform="%s" Name="%d" AppliedTrapPreset="TrapPreset/$ID/kDefaultTrapStyleName" '
               'OverrideList="" AppliedMaster="mpg" MasterPageTransform="1 0 0 1 0 0" TabOrder="" GridStartingPoint="TopOutside" UseMasterGrid="true">'
               '<Properties><PageColor type="enumeration">UseMasterColor</PageColor></Properties>'
               '<MarginPreference ColumnCount="1" ColumnGutter="12" Top="60" Bottom="40" Left="38" Right="38" ColumnDirection="Horizontal" ColumnsPositions="0 884"/></Page>'
               '%s</Spread></idPkg:Spread>' % (spid, new_id("pg"), H, W, TX, n, "".join(items)))
        open(os.path.join(work, "Spreads", "Spread_%s.xml" % spid), "w").write(xml)

    # master: one plain page, 960 x 540
    for f in os.listdir(os.path.join(work, "MasterSpreads")):
        os.remove(os.path.join(work, "MasterSpreads", f))
    open(os.path.join(work, "MasterSpreads", "MasterSpread_mpg.xml"), "w").write(
        '<?xml version="1.0" encoding="UTF-8" standalone="yes"?>\n<idPkg:MasterSpread xmlns:idPkg="http://ns.adobe.com/AdobeInDesign/idml/1.0/packaging" DOMVersion="7.5">'
        '<MasterSpread Self="mpg" ItemTransform="1 0 0 1 0 0" OverriddenPageItemProps="" Name="A-Page" NamePrefix="A" BaseName="Page" ShowMasterItems="true" PageCount="1">'
        '<Properties><PageColor type="enumeration">UseMasterColor</PageColor></Properties>'
        '<Page Self="mpgA" GeometricBounds="0 0 %s %s" ItemTransform="%s" Name="A" AppliedTrapPreset="TrapPreset/$ID/kDefaultTrapStyleName" '
        'OverrideList="" AppliedMaster="n" MasterPageTransform="1 0 0 1 0 0" TabOrder="" GridStartingPoint="TopOutside" UseMasterGrid="true">'
        '<Properties><PageColor type="enumeration">UseMasterColor</PageColor></Properties>'
        '<MarginPreference ColumnCount="1" ColumnGutter="12" Top="60" Bottom="40" Left="38" Right="38" ColumnDirection="Horizontal" ColumnsPositions="0 884"/></Page>'
        '</MasterSpread></idPkg:MasterSpread>' % (H, W, TX))

    # page size, single pages
    pf = os.path.join(work, "Resources", "Preferences.xml")
    s = open(pf).read()
    s = re.sub(r'PageHeight="[^"]*"', 'PageHeight="%s"' % H, s, count=1)
    s = re.sub(r'PageWidth="[^"]*"', 'PageWidth="%s"' % W, s, count=1)
    s = re.sub(r'PagesPerDocument="[^"]*"', 'PagesPerDocument="%d"' % TOTAL, s, count=1)
    s = re.sub(r'FacingPages="[^"]*"', 'FacingPages="false"', s, count=1)
    open(pf, "w").write(s)

    # swatches (RGB)
    gf = os.path.join(work, "Resources", "Graphic.xml")
    s = open(gf).read()
    sw = "".join('<Color Self="Color/%s" Model="Process" Space="RGB" ColorValue="%d %d %d" ColorOverride="Normal" '
                 'AlternateSpace="NoAlternateColor" AlternateColorValue="" Name="%s" ColorEditable="true" ColorRemovable="true" '
                 'Visible="true" SwatchCreatorID="7937"/>' % (n, r, g, b, n) for n, (r, g, b) in COLORS.items())
    i = s.find("<Color ")
    s = s[:i] + sw + "\n\t" + s[i:]
    open(gf, "w").write(s)

    # paragraph styles
    st = os.path.join(work, "Resources", "Styles.xml")
    s = open(st).read()
    ps = ""
    for name, (fam, face, size, lead, col, track, caps, after, _) in STYLES.items():
        ps += ('<ParagraphStyle Self="ParagraphStyle/%s" Name="%s" Imported="false" NextStyle="ParagraphStyle/%s" KeyboardShortcut="0 0" '
               'FontStyle="%s" PointSize="%s" FillColor="Color/%s" Tracking="%d" Capitalization="%s" SpaceAfter="%s" '
               'AppliedLanguage="$ID/English: UK" Hyphenation="false"><Properties><BasedOn type="string">$ID/[No paragraph style]</BasedOn>'
               '<PreviewColor type="enumeration">Nothing</PreviewColor><AppliedFont type="string">%s</AppliedFont>'
               '<Leading type="unit">%s</Leading></Properties></ParagraphStyle>\n\t\t'
               % (name, name, name, face, size, col, track, "AllCaps" if caps else "Normal", after, fam, lead))
    i = s.find('<ParagraphStyle Self="ParagraphStyle/$ID/NormalParagraphStyle"')
    s = s[:i] + ps + s[i:]
    open(st, "w").write(s)

    # designmap: the new spreads, stories and master
    dm = os.path.join(work, "designmap.xml")
    s = open(dm).read()
    s = re.sub(r'StoryList="[^"]*"', 'StoryList="%s ubs"' % " ".join(stories), s, count=1)
    s = re.sub(r'\s*<idPkg:(Spread|Story|MasterSpread) src="[^"]*"/>', "", s)
    add = '\t<idPkg:MasterSpread src="MasterSpreads/MasterSpread_mpg.xml"/>\n' + \
          "".join('\t<idPkg:Spread src="Spreads/Spread_%s.xml"/>\n' % sp for sp in spreads) + \
          "".join('\t<idPkg:Story src="Stories/Story_%s.xml"/>\n' % st_ for st_ in stories)
    i = s.find("<idPkg:BackingStory")
    s = s[:i] + add + "\t" + s[i:]
    first_page = re.search(r'<Page Self="([^"]+)"', open(os.path.join(work, "Spreads", "Spread_%s.xml" % spreads[0])).read()).group(1)
    s = re.sub(r'(<Section [^>]*PageStart=")[^"]*"', lambda m: m.group(1) + first_page + '"', s, count=1)
    s = re.sub(r'(<Section [^>]*Length=")[^"]*"', lambda m: m.group(1) + str(TOTAL) + '"', s, count=1)
    open(dm, "w").write(s)

    # zip: mimetype first, uncompressed
    if os.path.exists(path):
        os.remove(path)
    with zipfile.ZipFile(path, "w") as z:
        z.write(os.path.join(work, "mimetype"), "mimetype", compress_type=zipfile.ZIP_STORED)
        for root, _, files in os.walk(work):
            for f in files:
                full = os.path.join(root, f)
                rel = os.path.relpath(full, work)
                if rel != "mimetype":
                    z.write(full, rel, compress_type=zipfile.ZIP_DEFLATED)
    shutil.rmtree(work)


pdf = os.path.join(OUT, "Naomi_Umoru_Grapple_Runner_Portfolio.pdf")
idml = os.path.join(OUT, "Naomi_Umoru_Grapple_Runner_Portfolio.idml")
draw_pdf(pdf)
write_idml(idml)
print("wrote", pdf, os.path.getsize(pdf) // 1024, "KB")
print("wrote", idml, os.path.getsize(idml) // 1024, "KB")
