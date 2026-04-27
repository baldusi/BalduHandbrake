# BalduHandrake
#	Open Source Hydraulic Simracing Handbrake
#	Copyright (c) 2026 Alejandro Belluscio
#	Additional copyright holders listed inline below.
#	This file is licensed under the Apache 2.0 license
#	Full licence text: see LICENSE in this repository. 
#
###############################################################################
# BalduHandbrake LUT generation code.
###############################################################################
#
# Outputs straight C arrays for use in ./curves.h
#
# -----------------------------------------------------------------------------
# Pre-computed LUTs for the handbrake response curves. Each table maps a
# normalized input (0–4095, subdivided into 1024 segments + 1 duplicate
# sentinel) to a curved output (0–4095).
#
# The sensor pipeline indexes these with 2-bit fractional interpolation:
#   index = normValue >> 2       (0–1023)
#   frac  = normValue & 3        (0–3)
#   result = y0 + (y1 - y0) * frac / 4
# The extra 1025th entry (duplicate of entry 1024) avoids a bounds check
# when interpolating the last segment.
#
# LUT index mapping (see curveToLutIndex() in config.h):
#   LUT 0 = Rally Soft    (t^2.2)
#   LUT 1 = Rally Aggressive (t^0.75)
#   LUT 2 = Wet           (t^3.5)
#   LUT 3 = S-Curve       (1 / (1 + exp(-8*(t-0.5))))
#
# Linear and Drift Snap are computed directly — they don't use LUTs.

import math

def normalize_curve(f, num_points=1024):
    values = [f(j / (num_points - 1.0)) for j in range(num_points)]
    f_min, f_max = min(values), max(values)
    if f_max == f_min: return [0] * num_points
    return [max(0, min(4095, round(((v - f_min) / (f_max - f_min)) * 4095)))
            for v in values]

curves = [
    ("Rally Soft",       lambda t: t ** 2.2),
    ("Rally Aggressive", lambda t: t ** 0.75),
    ("Wet",              lambda t: t ** 3.5),
    ("S-Curve",          lambda t: 1 / (1 + math.exp(-8 * (t - 0.5)))),
]

for i, (name, f) in enumerate(curves):
    arr = normalize_curve(f)
    arr.append(arr[-1])  # sentinel for interpolation
    print(f"// LUT {i}: {name}")
    print(f"const uint16_t CURVE_LUT_{i}[1025] = {{")
    for row_start in range(0, len(arr), 16):
        print("    " + ", ".join(f"{v:4d}" for v in arr[row_start:row_start+16]) + ",")
    print("};")
    print()