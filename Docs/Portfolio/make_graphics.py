"""
Grapple Runner portfolio: the diagrams and charts, drawn in the EXIT SUIT portfolio style.

Run:  python make_graphics.py <fonts folder> [<dressing_layout.csv>]
Writes PNGs (300 dpi, transparent-free, off-white page colour) into Links/ next to this file.

Style (matches the EXIT SUIT portfolio): off-white paper, near-black ink, one orange accent,
thin rules, Inter for words, IBM Plex Mono in capitals for small labels.
"""
import csv
import math
import os
import sys

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import font_manager
from matplotlib.patches import FancyBboxPatch, FancyArrowPatch, Polygon

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "Links")
os.makedirs(OUT, exist_ok=True)
FONTS = sys.argv[1]
LAYOUT_CSV = sys.argv[2] if len(sys.argv) > 2 else None
sys.path.insert(0, os.path.join(HERE, "..", "..", "Tools"))

for root, _, files in os.walk(FONTS):
    for f in files:
        if f.endswith(".ttf") and (f.startswith("Inter-") or f.startswith("IBMPlexMono-")):
            font_manager.fontManager.addfont(os.path.join(root, f))

PAPER = "#EEEDE9"
INK = "#141414"
QUIET = "#6E6E6A"
RULE = "#BDBCB6"
ORANGE = "#F04E23"
SANS = "Inter"
MONO = "IBM Plex Mono"
plt.rcParams.update({"font.family": SANS, "font.size": 9, "text.color": INK, "axes.edgecolor": INK,
                     "axes.labelcolor": INK, "xtick.color": QUIET, "ytick.color": QUIET,
                     "axes.facecolor": PAPER, "figure.facecolor": PAPER, "savefig.facecolor": PAPER})


def mono(ax, x, y, text, size=6.5, color=QUIET, **kw):
    ax.text(x, y, text.upper(), fontfamily=MONO, fontsize=size, color=color, **kw)


def save(fig, name):
    fig.savefig(os.path.join(OUT, name), dpi=300, bbox_inches="tight", pad_inches=0.06)
    plt.close(fig)
    print("wrote", name)


def box(ax, x, y, w, h, title, lines=(), accent=False, label=None):
    ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0,rounding_size=1.2",
                                linewidth=1.6 if accent else 0.8, edgecolor=ORANGE if accent else INK,
                                facecolor=ORANGE if accent else "none", alpha=1, zorder=2))
    if accent:
        ax.add_patch(FancyBboxPatch((x, y), w, h, boxstyle="round,pad=0,rounding_size=1.2",
                                    linewidth=0, facecolor=PAPER, alpha=0.93, zorder=2.1))
    if label:
        mono(ax, x + 2, y + h - 3.2, label, size=5.5, color=ORANGE if accent else QUIET, va="top", zorder=3)
    ax.text(x + 2, y + h - (8 if label else 3.5), title, fontsize=8.5, fontweight="semibold", va="top", zorder=3)
    for i, line in enumerate(lines):
        ax.text(x + 2, y + h - (14 if label else 9) - 3.3 * i, line, fontsize=6.0, color=QUIET, va="top", zorder=3)


def arrow(ax, a, b, color=INK, rad=0.0, text=None, toff=(0, 1.5)):
    ax.add_patch(FancyArrowPatch(a, b, arrowstyle="-|>", mutation_scale=7, linewidth=0.8, color=color,
                                 connectionstyle="arc3,rad=%s" % rad, zorder=1, shrinkA=0, shrinkB=0))
    if text:
        mono(ax, (a[0] + b[0]) / 2 + toff[0], (a[1] + b[1]) / 2 + toff[1], text, size=5.2, ha="center")


# ------------------------------------------------------------------ G1: how the parts fit together
def system_diagram():
    fig = plt.figure(figsize=(6.2, 3.8))
    ax = fig.add_axes([0, 0, 1, 1])
    ax.set_xlim(0, 124)
    ax.set_ylim(6, 88)
    ax.axis("off")
    box(ax, 2, 58, 30, 26, "Player", ["VR controllers or", "mouse and keyboard", "trigger = fire, grip = swing"], label="A · Input")
    box(ax, 44, 52, 36, 32, "GrappleRig (C++)", ["Fixed-step physics, 120 Hz", "rope limit + soft spring", "swing exaggeration x1.35",
                                                 "auto-pull, gas boost"], accent=True, label="B · Core")
    box(ax, 92, 58, 30, 26, "Level", ["14 landing islands", "53 tagged grapple rocks", "ruins, trees, water"], label="C · World")
    box(ax, 2, 22, 30, 26, "Body + gear", ["Kwang, first person", "ODM gear on the hips", "ropes leave the hips"], label="D · Avatar")
    box(ax, 46, 18, 32, 26, "Feedback", ["24-link flexible rope", "rope pull in newtons", "-> haptics, wind sound"], label="E · Senses")
    box(ax, 92, 22, 30, 26, "Test loop", ["telemetry CSV each frame", "Python model replay", "TESTS.md, T-01 to T-14"], label="F · Evidence")
    arrow(ax, (32, 73), (44, 73), text="aim + buttons")
    arrow(ax, (92, 73), (80, 73), text="anchors")
    arrow(ax, (50, 52), (24, 48))
    arrow(ax, (62, 52), (62, 44))
    ax.add_patch(FancyArrowPatch((74, 52), (100, 48), arrowstyle="<|-|>", mutation_scale=7, linewidth=1.1,
                                 color=ORANGE, zorder=1, shrinkA=0, shrinkB=0))
    mono(ax, 31, 52, "pose", size=5.2)
    mono(ax, 63.5, 47.2, "tension", size=5.2)
    mono(ax, 84, 53.5, "every step  /  tuned settings", size=5.2, color=ORANGE)
    ax.text(62, 12, "Settings change only after the offline model and the game agree.", fontsize=6.6,
            color=ORANGE, ha="center", fontweight="medium")
    save(fig, "G1_system.png")


# ------------------------------------------------------------------ G2: the jolt, stiff vs soft vs progressive
def jolt_chart():
    from rope_model import simulate
    runs = [("Stiff rope", dict(soft=False), INK, 1.0, "-"),
            ("Soft rope (T-09)", dict(soft=True), QUIET, 1.0, "--"),
            ("Progressive (now)", dict(soft=True, prog=100.0, zeta=0.6), ORANGE, 1.8, "-")]
    fig = plt.figure(figsize=(6.4, 3.0))
    ax = fig.add_axes([0.09, 0.17, 0.88, 0.72])
    for name, kw, col, lw, ls in runs:
        peak, stretch, _, felt, _ = simulate([1200, 0, -3330], [300, 0, -1700], 3530, history=True, seconds=1.5, **kw)
        t = [i / 120.0 for i in range(len(felt))]
        ax.plot(t, felt, color=col, linewidth=lw, linestyle=ls, label="%s  peak %.1f g" % (name, peak))
    ax.set_yscale("log")
    ax.set_ylim(0.5, 400)
    ax.set_xlim(0, 1.5)
    ax.set_yticks([1, 3, 10, 30, 100, 300])
    ax.set_yticklabels(["1 g", "3 g", "10 g", "30 g", "100 g", "300 g"], fontfamily=MONO, fontsize=6)
    ax.set_xticks([0, 0.25, 0.5, 0.75, 1.0, 1.25, 1.5])
    ax.set_xticklabels(["0 s", "0.25", "0.5", "0.75", "1.0", "1.25", "1.5 s"], fontfamily=MONO, fontsize=6)
    for s in ("top", "right"):
        ax.spines[s].set_visible(False)
    ax.spines["left"].set_linewidth(0.6)
    ax.spines["bottom"].set_linewidth(0.6)
    ax.grid(axis="y", color=RULE, linewidth=0.4)
    ax.axhline(3, color=ORANGE, linewidth=0.5, linestyle=":")
    mono(ax, 0.55, 3.35, "3 g: about a roller coaster", size=5.2, color=ORANGE)
    ax.legend(frameon=False, fontsize=6.8, loc="upper right")
    fig.text(0.09, 0.93, "FELT ACCELERATION WHEN A SLACK 35 M ROPE SNAPS TIGHT AT 17 M/S (LOG SCALE)",
             fontfamily=MONO, fontsize=6, color=QUIET)
    save(fig, "G2_jolt.png")


# ------------------------------------------------------------------ G3: the test loop
def test_loop():
    fig = plt.figure(figsize=(6.2, 1.05))
    ax = fig.add_axes([0, 0, 1, 1])
    ax.set_xlim(0, 124)
    ax.set_ylim(5, 25)
    ax.axis("off")
    steps = [("01", "Predict", "Python model"), ("02", "Play", "scripted gr.Test"), ("03", "Record", "every frame to CSV"),
             ("04", "Compare", "game vs model, 360 steps"), ("05", "Decide", "PASS, or a finding")]
    w, gap = 21.5, 3
    for i, (n, t, s) in enumerate(steps):
        x = 2 + i * (w + gap)
        box(ax, x, 6, w, 18, t, [s], accent=(i == 3), label=n)
        if i < len(steps) - 1:
            arrow(ax, (x + w, 15), (x + w + gap, 15))
    save(fig, "G3_test_loop.png")


# ------------------------------------------------------------------ G5: the level from above
def world_map():
    if not LAYOUT_CSV or not os.path.exists(LAYOUT_CSV):
        print("no layout CSV: skipping G5")
        return
    rows = list(csv.DictReader(open(LAYOUT_CSV)))
    fig = plt.figure(figsize=(6.4, 1.6))
    ax = fig.add_axes([0.02, 0.2, 0.96, 0.6])
    style = {"anchor": (INK, 4, "o", "Grapple rock"), "ruin": (ORANGE, 10, "s", "Ruin"),
             "tree": ("#55704A", 7, "^", "Tree"), "water": ("#3E8EA8", 14, "o", "Water"), "start": (ORANGE, 60, "*", "Start")}
    for kind, (col, size, mk, name) in style.items():
        pts = [(float(r["x_cm"]) / 100, float(r["y_cm"]) / 100) for r in rows if r["kind"] == kind]
        if pts:
            ax.scatter([p[0] for p in pts], [p[1] for p in pts], s=size, c=col, marker=mk, linewidths=0,
                       label="%s (%d)" % (name, len(pts)), zorder=3 if kind != "tree" else 2)
    ax.set_aspect("equal")
    for s in ("top", "right", "left"):
        ax.spines[s].set_visible(False)
    ax.set_yticks([])
    ax.tick_params(axis="x", labelsize=6)
    ax.spines["bottom"].set_linewidth(0.6)
    ax.set_xlabel("METRES ALONG THE SWING ROUTE", fontfamily=MONO, fontsize=6, color=QUIET)
    ax.legend(frameon=False, fontsize=6.5, ncol=5, loc="upper left", bbox_to_anchor=(0, 1.18))
    save(fig, "G5_world_map.png")


# ------------------------------------------------------------------ G6: roadmap
def roadmap():
    fig = plt.figure(figsize=(6.4, 1.9))
    ax = fig.add_axes([0, 0, 1, 1])
    ax.set_xlim(0, 128)
    ax.set_ylim(0, 38)
    ax.axis("off")
    phases = [("V1", "Swing", "grapple, rope physics,", "body and gear", True),
              ("V2", "Run", "endless chunks,", "distance score", False),
              ("V3", "Feel", "haptics, then the", "EXIT SUIT", False),
              ("V4", "Fight", "PvE monsters,", "swords and guns", False),
              ("V5", "Teams", "1v1 to 5v5,", "objective modes", False)]
    w = 24
    for i, (code, name, l1, l2, done) in enumerate(phases):
        x = 2 + i * (w + 1.2)
        pts = [(x, 10), (x + w - 4, 10), (x + w, 21), (x + w - 4, 32), (x, 32)] if i == 0 else \
              [(x, 10), (x + w - 4, 10), (x + w, 21), (x + w - 4, 32), (x, 32), (x + 4, 21)]
        ax.add_patch(Polygon(pts, closed=True, facecolor=ORANGE if done else "none",
                             alpha=0.16 if done else 1, edgecolor="none"))
        ax.add_patch(Polygon(pts, closed=True, facecolor="none", edgecolor=ORANGE if done else INK,
                             linewidth=1.4 if done else 0.7))
        mono(ax, x + (3 if i == 0 else 6), 28.6, code + (" · now" if done else ""), size=5.5,
             color=ORANGE if done else QUIET)
        ax.text(x + (3 if i == 0 else 6), 22.5, name, fontsize=9, fontweight="semibold")
        ax.text(x + (3 if i == 0 else 6), 17.2, l1, fontsize=6.2, color=QUIET)
        ax.text(x + (3 if i == 0 else 6), 13.4, l2, fontsize=6.2, color=QUIET)
    mono(ax, 2, 4, "every phase ends with a comfort test in the headset (simulator sickness questionnaire)",
         size=5.5, color=INK)
    save(fig, "G6_roadmap.png")


system_diagram()
jolt_chart()
test_loop()
world_map()
roadmap()
