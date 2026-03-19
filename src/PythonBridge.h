#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#include <vector>
#include "DataLoader.h"

// Forward-declare Python types without including Python.h in headers
struct _object;
typedef _object PyObject;

// ─── Singleton Python bridge ─────────────────────────────────────────────────
class PythonBridge {
public:
    static PythonBridge& instance();

    void init();
    void shutdown();
    bool isReady() const { return m_ready; }

    // Load CSV / Excel via analysis/data_loader.py
    // Returns true on success and fills `out`
    bool loadFile(const std::wstring& path, std::vector<Candle>& out, std::wstring& errMsg);

    // Compute all active indicators via analysis/indicators.py
    bool computeIndicators(const std::vector<Candle>& candles,
                           const IndicatorSettings&   settings,
                           IndicatorData&             out,
                           std::wstring&              errMsg);

    // Compute all active predictors via analysis/predictors.py
    bool computePredictors(const std::vector<Candle>&   candles,
                           const PredictorSettings&     settings,
                           std::vector<PredictorResult>& out,
                           std::wstring&                errMsg);

    // ── Yahoo Finance ──────────────────────────────────────────────────────────
    // Download OHLCV data for a ticker via analysis/yahoo_finance.py
    // period:   "1mo" "3mo" "6mo" "1y" "2y" "5y" "10y" "max"
    // tfLabel:  ExtraGraphAnalyst timeframe label ("Daily", "1H", etc.)
    bool fetchTicker(const std::wstring& ticker,
                     const std::wstring& period,
                     const std::wstring& tfLabel,
                     std::vector<Candle>& out,
                     std::wstring& errMsg);

    // Search tickers matching a query; returns list of "SYMBOL|Name|Exchange|Type"
    std::vector<std::wstring> searchTicker(const std::wstring& query);

    // Get basic info string for a ticker: "Name|Sector|Currency|MarketCap|Exchange"
    std::wstring tickerInfo(const std::wstring& ticker);

    // ── Console / scripts ─────────────────────────────────────────────────────
    // Run arbitrary Python code in console, captures stdout+stderr
    std::wstring runConsoleCode(const std::wstring& code);

    // List user scripts in scripts/ folder
    std::vector<std::wstring> listUserScripts();

    // Run a named user script from scripts/ folder
    std::wstring runUserScript(const std::wstring& scriptName);

private:
    PythonBridge() = default;
    ~PythonBridge() = default;
    PythonBridge(const PythonBridge&) = delete;
    PythonBridge& operator=(const PythonBridge&) = delete;

    bool         m_ready = false;
    std::wstring m_exeDir;

    // Helper: call a Python function in a module, get string result
    std::string callPyFunc(const char* module, const char* func, const char* argStr);

    // Helper: parse the pipe-delimited double array format
    static std::vector<double> parseDblArray(const std::string& s);

    // Helper: get executable directory
    static std::wstring getExeDir();

    // Helper: wstring <-> UTF-8
    static std::string  wstrToUtf8(const std::wstring& ws);
    static std::wstring utf8ToWstr(const std::string&  s);
};
