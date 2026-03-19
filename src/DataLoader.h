#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <vector>

// ─── OHLCV candle ───────────────────────────────────────────────────────────
struct Candle {
    double      open, high, low, close, volume;
    std::wstring label;   // date/time string for axis
};

// ─── Indicator arrays (one value per candle, NaN = not computed yet) ────────
struct IndicatorData {
    // Overlay indicators (drawn on price chart)
    std::vector<double> sma20,  sma50,  sma200;
    std::vector<double> ema12,  ema26;
    std::vector<double> wma20;
    std::vector<double> bbUpper, bbMiddle, bbLower;
    std::vector<double> vwap;

    // Ichimoku
    std::vector<double> ichimokuTenkan;
    std::vector<double> ichimokuKijun;
    std::vector<double> ichimokuSpanA;
    std::vector<double> ichimokuSpanB;
    std::vector<double> ichimokuChikou;

    // Fibonacci horizontal levels (absolute price values)
    std::vector<double> fiboLevels;

    // Support / Resistance levels
    std::vector<double> srLevels;

    // Sub-panel indicators
    std::vector<double> rsi14;
    std::vector<double> macdLine, signalLine, macdHist;
    std::vector<double> stochK, stochD;
    std::vector<double> atr14;
};

// ─── Predictor result (N bars ahead forecast) ───────────────────────────────
struct PredictorResult {
    std::wstring         name;
    std::vector<double>  forecast;   // projected close values, attached to last candle
    bool                 recommended = false;
    double               confidence  = 0.0;
};

// ─── Which indicators / predictors are active ───────────────────────────────
struct IndicatorSettings {
    bool sma20   = false, sma50 = false, sma200 = false;
    bool ema12   = false, ema26 = false;
    bool wma20   = false;
    bool bb      = false;
    bool rsi     = false;
    bool macd    = false;
    bool stoch   = false;
    bool atr     = false;
    bool vwap    = false;
    bool fibo    = false;
    bool sr      = false;
    bool ichimoku = false;
    bool volume  = true;   // volume sub-panel (on by default)
};

struct PredictorSettings {
    bool linReg     = false;
    bool emaProj    = false;
    bool momentum   = false;
    bool holtWinters = false;
};

// ─── Complete chart dataset ──────────────────────────────────────────────────
struct ChartData {
    std::vector<Candle>          candles;
    IndicatorData                indicators;
    std::vector<PredictorResult> predictors;
    std::wstring                 filePath;
    std::wstring                 timeframe;
    bool                         loaded = false;
};
