# Scripts

Drop your `.py` files into this folder. After running `build.bat` they will be copied into the build output and appear in the Script dropdown inside the application.

If you want to add a script without rebuilding, copy the file directly into `build/scripts/` and hit Refresh in the toolbar.

---

## Available data

Your script has access to the currently loaded chart data through the `ega` object:

| Variable | Type | Description |
|---|---|---|
| `ega.closes` | `list[float]` | Close prices |
| `ega.opens` | `list[float]` | Open prices |
| `ega.highs` | `list[float]` | High prices |
| `ega.lows` | `list[float]` | Low prices |
| `ega.volumes` | `list[float]` | Volume |
| `ega.labels` | `list[str]` | Date or time labels |

Use `print()` to send output to the console pane at the bottom of the window.

---

## Example

```python
import numpy as np

closes = ega.closes
returns = np.diff(closes) / np.array(closes[:-1])

print(f"Bars loaded   : {len(closes)}")
print(f"Current close : {closes[-1]:.2f}")
print(f"Annualised vol: {np.std(returns) * np.sqrt(252) * 100:.1f}%")
```

---

## Notes

Scripts can import `numpy`, `pandas`, and `scipy` if they are installed. Files starting with `example_` are skipped by the script scanner.
