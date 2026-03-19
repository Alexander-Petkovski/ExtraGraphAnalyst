#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "DataLoader.h"
#include <string>

// ─── WorkspaceState ───────────────────────────────────────────────────────────
// Everything needed to fully restore a saved session.
struct WorkspaceState {
    bool              sourceIsYahoo = true;
    std::wstring      ticker;         // Yahoo ticker symbol, or CSV file path
    std::wstring      period;         // "1y", "6mo", "max", etc.
    std::wstring      timeframe;      // "Daily", "1H", "Weekly", etc.
    bool              isCandlestick  = true;
    IndicatorSettings indSettings;
    PredictorSettings predSettings;
    std::vector<Candle> candles;
};

// ─── WorkspaceFile ────────────────────────────────────────────────────────────
// Saves and loads .ega workspace files.
// Format is a plain UTF-8 text file with INI-style sections.
// Candle data is embedded so no re-fetch is needed when opening.
class WorkspaceFile {
public:
    static bool save(const std::wstring& path,
                     const WorkspaceState& ws,
                     std::wstring& errMsg);

    static bool load(const std::wstring& path,
                     WorkspaceState& ws,
                     std::wstring& errMsg);
};
