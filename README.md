# ExtraGraphAnalyst

A personal technical analysis tool for financial markets, built in C++ with a Win32/GDI+ GUI and Python embedded for all the heavy computation, also supporting Python scripting. It is early days for this project. It was built primarily as a custom tool for personal use and is not intended to be a polished production ready application. That said, the core loop works well and it is actively being developed.

---

## What it does

ExtraGraphAnalyst loads financial price data, either from a local CSV/Excel file or directly from Yahoo Finance via a ticker symbol, and renders it as an interactive candlestick or line chart. From there you can overlay a range of technical indicators, run forecasting predictors, and write your own Python scripts that plug directly into the chart data.

The GUI is a single dark themed window with a chart area that supports mouse wheel zoom and click drag panning. Buttons are very oldschool, however very simple and easy to find, it's a look I prefer. The toolbar is kept compact with category dropdown menus for indicators and predictors so it does not feel cluttered.

---

## Data sources

**Yahoo Finance** is the primary way to get data. Type a ticker symbol (AAPL, BTC-USD, ^GSPC, etc.) into the toolbar, pick a timeframe and period, and hit Fetch. The status bar will show the instrument name, currency, and market cap alongside the bar count.

**CSV and Excel files** are also supported. Click Open CSV and point it at any OHLCV file. The Python data loader handles both pandas based parsing and a plain stdlib fallback.

---

## Indicators

Indicators are organised into three dropdown menus. Each button shows how many are currently active.

**Moving Averages** covers SMA 20, SMA 50, SMA 200, EMA 12, EMA 26, and WMA 20.

**Oscillators** covers RSI (14), MACD, Stochastic, ATR (14), and the Volume sub-panel.

**Bands and Levels** covers Bollinger Bands, VWAP, Fibonacci retracement, Support/Resistance levels, and Ichimoku Cloud.

All indicator computation runs in Python via an embedded interpreter, with results piped back to the C++ chart renderer.

---

## Predictors

The Predictors dropdown contains four forecasting methods that project price forward from the last candle:

- Linear Regression
- EMA Projection
- Momentum
- Holt-Winters exponential smoothing

The system automatically flags the highest confidence predictor based on a validation holdout. These are experimental and should not be used for any real financial decision making.

---

## Python scripting

A Python console at the bottom of the window lets you run arbitrary code against the current chart data. The objects `ega.closes`, `ega.opens`, `ega.highs`, `ega.lows`, `ega.volumes`, and `ega.labels` are available in the console and in any user script.

Drop Python files into the `scripts/` folder and they will appear in the Script dropdown in the toolbar. The `scripts/README.md` file documents the full API. Two example scripts are included: `example_indicator.py` (Donchian Channel) and `example_predictor.py` (volatility mean reversion forecast).

---

## Building

Requirements: MinGW-w64, CMake, Ninja, Python 3.6 or newer with pip, and a Windows SDK that includes GDI+ (Windows 10 or later covers this out of the box).

Run `build.bat` from the project root. It installs the required Python packages and builds the executable with CMake and Ninja. The output lands in the `build/` directory along with a copy of the `analysis/` and `scripts/` folders.

```
build.bat
```

The build links against gdiplus, gdi32, user32, comctl32, comdlg32, shell32, advapi32, shlwapi, uxtheme, and dwmapi. There are no external C++ dependencies.

---

## Current state

This is v1.1.2. The core functionality works: fetch data, view the chart, toggle indicators, run predictors, use the console and scripts. A few honest caveats:

There is no settings persistence. Every time you open the app you start fresh with no indicators selected and no remembered ticker. There is no chart export. The predictor confidence scores are derived from walk-forward backtests against a held-out portion of the loaded data. The Python console has no persistent state between sessions.

It has only been tested on Windows 10 and Windows 11 with Python 3.10 and Python 3.12. MinGW-w64 is the expected toolchain. MSVC has not been tested and would likely need minor adjustments to the CMakeLists.

---

## License

No license is attached. This is personal tooling. Feel free to read the code but please do not redistribute or repackage it.
