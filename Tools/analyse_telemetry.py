"""
Grapple Runner: test analysis for one telemetry recording.

HOW TO READ THIS (no coding experience needed)
- The game writes a spreadsheet (CSV) of every frame to Saved/Tests/ when you stop
  playing (see "TEST RECORDING" in Source/GrappleRunner/GrappleRig.cpp).
- This script reads that spreadsheet, measures things, compares them with the
  settings the game was supposed to use, prints the results, and draws charts
  into Docs/media/tests/. Those numbers and charts are the evidence in Docs/TESTS.md.
- Lines starting with # are notes for humans; the computer skips them.
- "def name(...):" is a reusable recipe; the indented lines are its steps.

Run (on any computer with Python 3, pandas, numpy and matplotlib):
    python Tools/analyse_telemetry.py Saved/Tests/telemetry_2026.11.08-16.30.51.csv
"""
import sys
import os
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use("Agg")                      # draw to image files, no window
import matplotlib.pyplot as plt

# ---- chart style: plain, readable, same colours in every chart
BLUE, ORANGE = "#2a78d6", "#eb6834"        # series 1 and series 2
INK, INK2, GRID = "#0b0b0b", "#52514e", "#e6e5e1"
plt.rcParams.update({
    "figure.facecolor": "#fcfcfb", "axes.facecolor": "#fcfcfb",
    "axes.edgecolor": GRID, "axes.labelcolor": INK2, "xtick.color": INK2, "ytick.color": INK2,
    "axes.grid": True, "grid.color": GRID, "grid.linewidth": 0.8,
    "axes.spines.top": False, "axes.spines.right": False,
    "font.size": 11, "axes.titlesize": 13, "axes.titleweight": "bold", "axes.titlecolor": INK,
    "lines.linewidth": 2, "legend.frameon": False,
})
OUT = os.path.join(os.path.dirname(__file__), "..", "Docs", "media", "tests")


def save(fig, name):
    """Save a chart as a PNG next to the other test evidence."""
    fig.tight_layout()
    fig.savefig(os.path.join(OUT, name), dpi=160)
    plt.close(fig)
    print("  chart:", name)


def settings(path):
    """Read the '# Gravity=... ' line at the top of the file into a dictionary."""
    with open(path) as f:
        for line in f:
            if line.startswith("# Gravity"):
                out = {}
                for part in line[2:].split(","):
                    if "=" in part:
                        k, v = part.split("=", 1)
                        try:
                            out[k.strip()] = float(v.split()[0])
                        except ValueError:
                            pass
                return out
    return {}


path = sys.argv[1]
S = settings(path)
d = pd.read_csv(path, comment="#")
print("Recording:", os.path.basename(path), "|", len(d), "frames,", round(d.time_s.max(), 1), "s")

# ---------------------------------------------------------------- frame time
ft = d[d.time_s > 5].frame_ms               # skip the first 5 s (loading)
print("\nFRAME TIME  median %.2f ms (%.1f fps), 95th pct %.2f ms, 99th pct %.2f ms, "
      "frames over 11.1 ms (90 Hz budget): %.1f%%"
      % (ft.median(), 1000 / ft.median(), ft.quantile(.95), ft.quantile(.99), 100 * (ft > 1000 / 90).mean()))
fig, ax = plt.subplots(figsize=(7, 3.6))
ax.hist(ft.clip(upper=20), bins=np.arange(8, 20.25, 0.25), color=BLUE, edgecolor="#fcfcfb", linewidth=1)
ax.axvline(1000 / 90, color=INK2, linestyle="--", linewidth=1.2)
ax.text(1000 / 90 + 0.15, ax.get_ylim()[1] * 0.9, "90 Hz budget (11.1 ms)", color=INK2, fontsize=10)
ax.set_title("Frame time, desktop play-in-editor")
ax.set_xlabel("Frame time (ms), longer than 20 ms shown at 20")
ax.set_ylabel("Frames")
save(fig, "T08_frame_time_histogram.png")

# ---------------------------------------------------------------- rope never stretches
h = d[d.hookL == 1]
if len(h):
    excess = h.ropeL_dist - h.ropeL_len
    print("\nROPE CONSTRAINT  %d frames hooked (%.1f s): largest stretch beyond rope length %.2f cm"
          % (len(h), h.time_s.max() - h.time_s.min(), excess.max()))
    fig, ax = plt.subplots(figsize=(7, 3.8))
    t0 = h.time_s.min()
    ax.plot(h.time_s - t0, h.ropeL_len / 100, color=BLUE, label="Rope length (limit)")
    ax.plot(h.time_s - t0, h.ropeL_dist / 100, color=ORANGE, label="Actual distance to anchor")
    ax.set_title("The body never goes past the end of the rope")
    ax.set_xlabel("Seconds after the hook landed")
    ax.set_ylabel("Metres")
    ax.legend(loc="upper right")
    save(fig, "T04_rope_constraint.png")

# ---------------------------------------------------------------- reel speed
r = d[(d.hookL == 1) & (d.reel == 1)]
if len(r) > 10:
    slope, icpt = np.polyfit(r.time_s, r.ropeL_len, 1)
    target = S.get("ReelSpeed", float("nan"))
    print("\nREEL SPEED  measured %.2f cm/s vs setting %.0f cm/s (error %.3f%%), over %.2f s"
          % (-slope, target, 100 * abs(-slope - target) / target, r.time_s.max() - r.time_s.min()))
    fig, ax = plt.subplots(figsize=(7, 3.6))
    t0 = r.time_s.min()
    ax.plot(r.time_s - t0, r.ropeL_len / 100, color=BLUE, label="Measured rope length")
    ax.plot(r.time_s - t0, (slope * r.time_s + icpt) / 100, color=ORANGE, linestyle="--", linewidth=1.5,
            label="Best-fit line: %.1f cm/s" % -slope)
    ax.set_title("Reeling in shortens the rope at the set speed")
    ax.set_xlabel("Seconds of reeling")
    ax.set_ylabel("Rope length (m)")
    ax.legend(loc="upper right")
    save(fig, "T05_reel_rate.png")

# ---------------------------------------------------------------- free fall vs the physics model
# Longest stretch with no hook, not grounded, no respawn jump, under the speed cap.
free = (d.hookL == 0) & (d.hookR == 0) & (d.grounded == 0) & (d.speed_cms < S.get("MaxSpeed", 4500) - 10)
best, run, start = (0, 0), 0, 0
for i, ok in enumerate(free.values):
    if ok and (i == 0 or abs(d.z_cm.values[i] - d.z_cm.values[i - 1]) < 1000):
        if run == 0:
            start = i
        run += 1
        if run > best[1] - best[0]:
            best = (start, start + run)
    else:
        run = 0
fr = d.iloc[best[0] + 2:best[1] - 1]
if len(fr) > 60:
    t, vz, vx = fr.time_s.values, fr.vz.values, fr.vx.values
    # Fit the textbook equation for falling with air drag to the measured vertical speed:
    #     vz(t) = -g/k + (vz0 + g/k) * e^(-k t)
    # and find the g (gravity) and k (drag) that match the data best. Fitting the whole
    # curve at once is much less sensitive to frame-timing jitter than frame-to-frame slopes.
    from scipy.optimize import curve_fit
    T0 = t - t[0]
    def falling(tt, g_, k_, v0):
        return -g_ / k_ + (v0 + g_ / k_) * np.exp(-k_ * tt)
    (g_meas, k_meas, _), _ = curve_fit(falling, T0, vz, p0=(900, 0.05, vz[0]))
    # Sideways speed only feels drag: vx(t) = vx0 * e^(-k t)  ->  a straight line on a log scale.
    kx = -np.polyfit(T0, np.log(np.abs(vx)), 1)[0]
    # Replay the same physics rules in Python, from the same starting point, and compare.
    g, k, step = S.get("Gravity", 980), S.get("AirDragPerSecond", 0.06), 1 / 120
    v = fr[["vx", "vy", "vz"]].values[0].astype(float)
    p = fr[["x_cm", "y_cm", "z_cm"]].values[0].astype(float)
    T, tt, pred = t - t[0], 0.0, []
    for ti in T:
        while tt < ti - 1e-9:
            v[2] -= g * step
            v -= v * k * step
            p = p + v * step
            tt += step
        pred.append(p.copy())
    pred = np.array(pred)
    act = fr[["x_cm", "y_cm", "z_cm"]].values
    err = np.linalg.norm(pred - act, axis=1)
    drop = act[0, 2] - act[-1, 2]
    print("\nFREE FALL  %.2f s, %d frames, %.1f m drop" % (T[-1], len(T), drop / 100))
    print("  gravity measured %.2f cm/s2 vs setting %.0f (error %.3f%%)" % (g_meas, g, 100 * abs(g_meas - g) / g))
    print("  drag measured %.4f /s (vertical), %.4f /s (horizontal) vs setting %.3f" % (k_meas, kx, k))
    print("  model-vs-game position: max difference %.1f cm (%.2f%% of the drop)" % (err.max(), 100 * err.max() / drop))
    fig, ax = plt.subplots(figsize=(7, 3.8))
    ax.plot(T, act[:, 2] / 100, color=BLUE, label="Game (measured)")
    ax.plot(T, pred[:, 2] / 100, color=ORANGE, linestyle="--", linewidth=1.5, label="Physics model (predicted)")
    ax.set_title("Falling matches the physics model")
    ax.set_xlabel("Seconds of free fall")
    ax.set_ylabel("Height (m)")
    ax.legend(loc="upper right")
    save(fig, "T06_free_fall_model_vs_game.png")

# ---------------------------------------------------------------- sudden speed losses while hooked
# Two possible causes: the rope SNAPPING tight (it was slack, now it is exactly at full
# length) or the body HITTING an island. Tell them apart and report both.
if len(h):
    hh = h.copy()
    hh["dv"] = hh.speed_cms.diff()
    hh["was_slack"] = (hh.ropeL_len - hh.ropeL_dist).shift(1) > 5
    hh["now_taut"] = (hh.ropeL_len - hh.ropeL_dist) < 0.5
    events = hh[hh.dv < -500]
    snaps, hits = [], []
    for _, row in events.iterrows():
        before = hh.loc[:row.name].iloc[-2].speed_cms
        lost = 100 * (before - row.speed_cms) / before
        if row.was_slack and row.now_taut:
            kind = "ROPE SNAP"
            snaps.append(row)
        else:
            kind = "COLLISION with " + (row.last_hit if row.last_hit != "none" else "an object (not caught by the last-step log)")
            hits.append(row)
        print("\n%s at %.3f s: speed %.0f -> %.0f cm/s in one frame (%.0f%% lost)"
              % (kind, row.time_s, before, row.speed_cms, lost))
    if snaps:
        t0 = h.time_s.min()
        fig, ax = plt.subplots(figsize=(7, 3.6))
        ax.plot(h.time_s - t0, h.speed_cms * 0.036, color=BLUE)
        for row in snaps:
            x, y = row.time_s - t0, row.speed_cms * 0.036
            ax.annotate("rope snaps tight", (x, y), xytext=(x - 0.15, max(y - 14, 2)), color=INK2, ha="right",
                        arrowprops=dict(arrowstyle="->", color=INK2))
        for row in hits:
            x, y = row.time_s - t0, row.speed_cms * 0.036
            spot = (x + 0.25, y + 15) if y > 40 else (x - 1.25, y + 10)   # keep the label off the line
            ax.annotate("hits island", (x, y), xytext=spot, color=INK2,
                        arrowprops=dict(arrowstyle="->", color=INK2))
        ax.set_title("Speed while hooked")
        ax.set_xlabel("Seconds after the hook landed")
        ax.set_ylabel("Speed (km/h)")
        save(fig, "F1_rope_snap_speed.png")
