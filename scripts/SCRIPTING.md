# ExtraGraphAnalyst Scripting Library

When you run a script or type code into the Python console, ExtraGraphAnalyst automatically injects the `ega` object into your session. This is your live connection to whatever chart data is currently loaded.

---

## The ega object

| Attribute | Type | Description |
|---|---|---|
| `ega.closes` | `list[float]` | Close prices for every bar |
| `ega.opens` | `list[float]` | Open prices for every bar |
| `ega.highs` | `list[float]` | High prices for every bar |
| `ega.lows` | `list[float]` | Low prices for every bar |
| `ega.volumes` | `list[float]` | Volume for every bar |
| `ega.labels` | `list[str]` | Date or time label for every bar |

All lists are the same length and are ordered oldest to newest. Index `0` is the earliest bar, index `-1` is the most recent.

---

## Quick examples

**Print a basic summary:**
```python
print(f"Bars loaded : {len(ega.closes)}")
print(f"First bar   : {ega.labels[0]}  {ega.closes[0]:.2f}")
print(f"Last bar    : {ega.labels[-1]}  {ega.closes[-1]:.2f}")
```

**Compute annualised volatility:**
```python
import numpy as np
returns = np.diff(ega.closes) / np.array(ega.closes[:-1])
print(f"Annualised vol: {np.std(returns) * np.sqrt(252) * 100:.1f}%")
```

**Find the highest close and when it occurred:**
```python
peak = max(ega.closes)
day  = ega.labels[ega.closes.index(peak)]
print(f"Peak close: {peak:.2f} on {day}")
```

**Simple moving average:**
```python
import numpy as np
period = 20
closes = ega.closes
sma = [
    float('nan') if i < period else np.mean(closes[i - period:i])
    for i in range(len(closes))
]
print(f"SMA{period} last value: {sma[-1]:.2f}")
```

**Volume spike scan:**
```python
import numpy as np
period    = 20
threshold = 2.0
volumes   = ega.volumes
labels    = ega.labels

for i in range(period, len(volumes)):
    avg = np.mean(volumes[i - period:i])
    if avg > 0 and volumes[i] > avg * threshold:
        print(f"{labels[i]}  {volumes[i] / avg:.1f}x avg volume")
```

---

## Available libraries

The following are installed and ready to import:

| Library | Import |
|---|---|
| NumPy | `import numpy as np` |
| pandas | `import pandas as pd` |
| SciPy | `import scipy` |

---

## Notes

The `ega` object is refreshed from the currently loaded chart every time you hit Run. If no data is loaded the object will not be available and you will get a NameError. Load a chart first via Fetch or Open, then run your script.

Scripts must be `.py` files placed in this `scripts/` folder. Hit Refresh in the toolbar to pick up any new files without restarting the application.
