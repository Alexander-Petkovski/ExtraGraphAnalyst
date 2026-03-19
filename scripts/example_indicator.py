"""
ExtraGraphAnalyst – Example Custom Indicator
=============================================
This file is an EXAMPLE – it is not loaded by the script runner.
Rename it (remove the 'example_' prefix) to make it active.

This example computes a custom Donchian Channel and prints a summary.
"""

# ── Access chart data ──────────────────────────────────────────────────────────
closes  = ega.closes   # noqa: F821  (injected at runtime)
highs   = ega.highs    # noqa: F821
lows    = ega.lows     # noqa: F821

# ── Donchian Channel ──────────────────────────────────────────────────────────
PERIOD = 20
n      = len(closes)

upper  = []
lower  = []
middle = []

for i in range(n):
    if i < PERIOD - 1:
        upper.append(float('nan'))
        lower.append(float('nan'))
        middle.append(float('nan'))
    else:
        h = max(highs[i - PERIOD + 1 : i + 1])
        l = min(lows[i  - PERIOD + 1 : i + 1])
        upper.append(h)
        lower.append(l)
        middle.append((h + l) / 2.0)

# ── Output ────────────────────────────────────────────────────────────────────
last_upper  = upper[-1]
last_lower  = lower[-1]
last_middle = middle[-1]
last_close  = closes[-1]

print(f"Donchian Channel ({PERIOD}-bar)")
print(f"  Upper  : {last_upper:.4f}")
print(f"  Middle : {last_middle:.4f}")
print(f"  Lower  : {last_lower:.4f}")
print(f"  Close  : {last_close:.4f}")

if last_close >= last_upper * 0.99:
    print("  Signal : NEAR UPPER BAND – potential breakout or resistance")
elif last_close <= last_lower * 1.01:
    print("  Signal : NEAR LOWER BAND – potential breakdown or support")
else:
    print("  Signal : Within channel – no breakout signal")
