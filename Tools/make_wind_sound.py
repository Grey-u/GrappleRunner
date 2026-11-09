"""
Grapple Runner: make the wind sound from scratch (no recording, no licence needed).

HOW TO READ THIS (no coding experience needed)
- Wind is basically random noise ("shhhh") with most of the high hiss removed, getting
  louder and softer in slow gusts. This script builds exactly that, as numbers:
    1. random noise for each ear (slightly different, so it sounds wide);
    2. a filter that keeps the low rumble and the "whoosh" band and softens the hiss;
    3. slow waves of loudness (gusts) that repeat a whole number of times per loop,
       so the end of the file joins the start with no click;
    4. a short crossfade at the loop point, then save as a .wav file.
- In the game, GrappleRig plays it on a loop and makes it louder and higher-pitched the
  faster you go (UpdateWind in GrappleRig.cpp).
Run:  python Tools/make_wind_sound.py      ->  writes Art/SFX/SFX_Wind.wav
"""
import os
import numpy as np
from scipy.io import wavfile
from scipy.signal import butter, sosfiltfilt

RATE = 44100          # samples per second (CD quality)
SECONDS = 16          # loop length
rng = np.random.default_rng(7)   # fixed seed = the same sound every time
n = RATE * SECONDS
t = np.arange(n) / RATE


def filtered_noise():
    """One ear's worth of wind-coloured noise."""
    white = rng.standard_normal(n + RATE)                      # one extra second for the crossfade
    rumble = sosfiltfilt(butter(2, 180, "lowpass", fs=RATE, output="sos"), white) * 2.5
    whoosh = sosfiltfilt(butter(2, [250, 1400], "bandpass", fs=RATE, output="sos"), white)
    hiss = sosfiltfilt(butter(2, 3000, "highpass", fs=RATE, output="sos"), white) * 0.08
    return rumble + whoosh + hiss


def gusts():
    """Slow swells of loudness. Each wave fits a whole number of times into the loop."""
    env = 0.65
    for cycles, depth, phase in [(1, 0.18, 0.0), (3, 0.12, 1.3), (5, 0.07, 2.1), (11, 0.04, 0.4)]:
        env = env + depth * np.sin(2 * np.pi * cycles * t / SECONDS + phase)
    return np.clip(env, 0.2, 1.0)


channels = []
for _ in range(2):                                             # left ear, right ear
    noise = filtered_noise()
    body, tail = noise[:n].copy(), noise[n:]
    fade = np.linspace(0.0, 1.0, RATE)                         # 1-second crossfade at the loop point
    body[:RATE] = body[:RATE] * fade + tail * (1 - fade)      # the start blends in from what follows the end
    channels.append(body * gusts())

stereo = np.stack(channels, axis=1)
stereo /= np.abs(stereo).max() / 0.7                           # leave headroom (peak at 70%)
out = os.path.join(os.path.dirname(__file__), "..", "Art", "SFX", "SFX_Wind.wav")
os.makedirs(os.path.dirname(out), exist_ok=True)
wavfile.write(out, RATE, (stereo * 32767).astype(np.int16))
print("wrote", os.path.normpath(out), "-", SECONDS, "s loop")
