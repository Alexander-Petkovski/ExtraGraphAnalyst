# ExtraGraphAnalyst – User Scripts

Drop your `.py` scripts in this folder to extend ExtraGraphAnalyst with custom indicators and predictors.

## How it works

1. Write a `.py` script (see examples below).
2. Click **Refresh** in the "User Script" section of the toolbar.
3. Select your script from the dropdown and click **Run**.

Your script runs inside the embedded Python interpreter with full access to the chart data through the `ega` namespace (populated automatically).

---

## Available variables in your script

| Variable | Type | Description |
|---|---|---|
| `ega.closes` | `list[float]` | All close prices |
| `ega.opens`  | `list[float]` | All open prices |
| `ega.highs`  | `list[float]` | All high prices |
| `ega.lows`   | `list[float]` | All low prices |
| `ega.volumes`| `list[float]` | All volumes |
| `ega.labels` | `list[str]`   | Date/time labels |

---

## Adding a custom indicator

```python
# my_indicator.py

# Access the chart data
closes  = ega.closes
highs   = ega.highs
lows    = ega.lows

# Compute something custom (e.g., a simple momentum oscillator)
n      = len(closes)
period = 10
result = []
for i in range(n):
    if i < period:
        result.append(float('nan'))
    else:
        result.append(closes[i] - closes[i - period])

# Print a summary
print(f"Custom Momentum (period={period})")
print(f"Last value: {result[-1]:.4f}")
print(f"Previous 5 values: {result[-5:]}")
```

---

## Adding a custom predictor

```python
# my_predictor.py

closes = ega.closes

# Simple mean-reversion forecast
n_bars = 10
mean   = sum(closes[-20:]) / 20
last   = closes[-1]
step   = (mean - last) / n_bars

forecast = [last + step * (i + 1) for i in range(n_bars)]

print("Mean-Reversion Forecast (next 10 bars):")
for i, f in enumerate(forecast):
    print(f"  Bar {i+1}: {f:.2f}")
```

---

## Tips

- Scripts can `import numpy`, `import pandas`, `import scipy` if installed.
- Use `print()` to send output to the console pane.
- Files starting with `example_` are ignored by the script scanner.
- The embedded Python shares the same process, so heavy computations will freeze the UI.
  For large datasets, consider using `threading` or keeping scripts fast.
