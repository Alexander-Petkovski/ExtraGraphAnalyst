"""
ExtraGraphAnalyst – Example Custom Predictor
=============================================
This file is an EXAMPLE – it is not loaded by the script runner.
Rename it (remove the 'example_' prefix) to make it active.

This example computes a simple volatility-adjusted mean-reversion forecast.
"""
import math

# ── Access chart data ──────────────────────────────────────────────────────────
closes  = ega.closes   # noqa: F821  (injected at runtime)
highs   = ega.highs    # noqa: F821
lows    = ega.lows     # noqa: F821

N_BARS  = 15
LOOKBACK = 30

# ── Compute volatility (ATR-based) ────────────────────────────────────────────
n = len(closes)
atr_vals = []
for i in range(1, n):
    tr = max(highs[i] - lows[i],
             abs(highs[i] - closes[i-1]),
             abs(lows[i]  - closes[i-1]))
    atr_vals.append(tr)

atr = sum(atr_vals[-LOOKBACK:]) / min(LOOKBACK, len(atr_vals)) if atr_vals else 0.001

# ── Mean-reversion forecast ───────────────────────────────────────────────────
recent_closes = closes[-LOOKBACK:]
mean = sum(recent_closes) / len(recent_closes)
last = closes[-1]
dist = last - mean   # how far price is from mean

# Forecast: price reverts toward mean, step size ∝ atr
forecast = []
current  = last
for i in range(N_BARS):
    reversion = -dist * 0.1          # 10% reversion per bar
    noise_est = atr * 0.1 * (i + 1)  # increasing uncertainty
    current  += reversion
    dist     = current - mean
    forecast.append(current)

# ── Output ────────────────────────────────────────────────────────────────────
print(f"Volatility Mean-Reversion Forecast ({N_BARS} bars)")
print(f"  Current price : {last:.4f}")
print(f"  {LOOKBACK}-bar mean : {mean:.4f}")
print(f"  ATR (avg)     : {atr:.4f}")
print()
print("  Bar  |  Forecast")
print("  -----|----------")
for i, f in enumerate(forecast):
    direction = "↑" if f > (forecast[i-1] if i > 0 else last) else "↓"
    print(f"  {i+1:3d}  |  {f:.4f}  {direction}")
