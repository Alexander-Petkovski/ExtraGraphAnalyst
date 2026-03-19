#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "WorkspaceFile.h"
#include <stdio.h>
#include <sstream>
#include <string>
#include <algorithm>

// ─── Internal UTF-8 helpers ───────────────────────────────────────────────────
static std::string toUtf8(const std::wstring& ws) {
    if (ws.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1,
                                nullptr, 0, nullptr, nullptr);
    std::string s(n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

static std::wstring fromUtf8(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring ws(n - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], n);
    return ws;
}

static std::string trimStr(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

// ─── save ─────────────────────────────────────────────────────────────────────
bool WorkspaceFile::save(const std::wstring& path,
                         const WorkspaceState& ws,
                         std::wstring& errMsg) {
    FILE* f = _wfopen(path.c_str(), L"wb");
    if (!f) {
        errMsg = L"Cannot open file for writing";
        return false;
    }

    // Shorthand lambdas
    auto ln  = [&](const char* s)               { fputs(s, f); fputc('\n', f); };
    auto kv  = [&](const char* k, const char* v){ fprintf(f, "%s=%s\n", k, v); };
    auto kvb = [&](const char* k, bool v)        { fprintf(f, "%s=%d\n", k, v ? 1 : 0); };

    // Header
    ln("[EGA_WORKSPACE]");
    ln("version=1.1.0");
    fputc('\n', f);

    // Session
    ln("[SESSION]");
    kv("source",    ws.sourceIsYahoo ? "yahoo" : "csv");
    kv("ticker",    toUtf8(ws.ticker).c_str());
    kv("period",    toUtf8(ws.period).c_str());
    kv("timeframe", toUtf8(ws.timeframe).c_str());
    kv("chartmode", ws.isCandlestick ? "candle" : "line");
    fputc('\n', f);

    // Indicators
    ln("[INDICATORS]");
    kvb("sma20",    ws.indSettings.sma20);
    kvb("sma50",    ws.indSettings.sma50);
    kvb("sma200",   ws.indSettings.sma200);
    kvb("ema12",    ws.indSettings.ema12);
    kvb("ema26",    ws.indSettings.ema26);
    kvb("wma20",    ws.indSettings.wma20);
    kvb("bb",       ws.indSettings.bb);
    kvb("rsi",      ws.indSettings.rsi);
    kvb("macd",     ws.indSettings.macd);
    kvb("stoch",    ws.indSettings.stoch);
    kvb("atr",      ws.indSettings.atr);
    kvb("vwap",     ws.indSettings.vwap);
    kvb("fibo",     ws.indSettings.fibo);
    kvb("sr",       ws.indSettings.sr);
    kvb("ichimoku", ws.indSettings.ichimoku);
    kvb("volume",   ws.indSettings.volume);
    fputc('\n', f);

    // Predictors
    ln("[PREDICTORS]");
    kvb("linreg",      ws.predSettings.linReg);
    kvb("emaproj",     ws.predSettings.emaProj);
    kvb("momentum",    ws.predSettings.momentum);
    kvb("holtwinters", ws.predSettings.holtWinters);
    fputc('\n', f);

    // Candles
    ln("[CANDLES]");
    fprintf(f, "count=%zu\n", ws.candles.size());
    for (const auto& c : ws.candles) {
        // label|open|high|low|close|volume
        fprintf(f, "%s|%.6f|%.6f|%.6f|%.6f|%.2f\n",
                toUtf8(c.label).c_str(),
                c.open, c.high, c.low, c.close, c.volume);
    }

    fclose(f);
    return true;
}

// ─── load ─────────────────────────────────────────────────────────────────────
bool WorkspaceFile::load(const std::wstring& path,
                         WorkspaceState& ws,
                         std::wstring& errMsg) {
    FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f) {
        errMsg = L"Cannot open file for reading";
        return false;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);

    if (sz <= 0) {
        fclose(f);
        errMsg = L"File is empty";
        return false;
    }

    std::string buf(sz, '\0');
    fread(&buf[0], 1, sz, f);
    fclose(f);

    std::istringstream ss(buf);
    std::string line, section;
    bool inCandles = false;

    while (std::getline(ss, line)) {
        line = trimStr(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;

        // Section header
        if (line[0] == '[') {
            section   = line;
            inCandles = (section == "[CANDLES]");
            continue;
        }

        // Candle data rows
        if (inCandles) {
            if (line.rfind("count=", 0) == 0) {
                ws.candles.reserve(std::stoul(line.substr(6)));
                continue;
            }
            // label|open|high|low|close|volume
            std::istringstream cs(line);
            std::string tok;
            Candle c{};
            int field = 0;
            while (std::getline(cs, tok, '|')) {
                try {
                    switch (field) {
                    case 0: c.label  = fromUtf8(tok);   break;
                    case 1: c.open   = std::stod(tok);  break;
                    case 2: c.high   = std::stod(tok);  break;
                    case 3: c.low    = std::stod(tok);  break;
                    case 4: c.close  = std::stod(tok);  break;
                    case 5: c.volume = std::stod(tok);  break;
                    }
                } catch (...) { /* skip malformed field */ }
                ++field;
            }
            if (field >= 6) ws.candles.push_back(c);
            continue;
        }

        // Key=value pairs
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trimStr(line.substr(0, eq));
        std::string val = trimStr(line.substr(eq + 1));

        if (section == "[SESSION]") {
            if      (key == "source")    ws.sourceIsYahoo  = (val == "yahoo");
            else if (key == "ticker")    ws.ticker         = fromUtf8(val);
            else if (key == "period")    ws.period         = fromUtf8(val);
            else if (key == "timeframe") ws.timeframe      = fromUtf8(val);
            else if (key == "chartmode") ws.isCandlestick  = (val == "candle");
        }
        else if (section == "[INDICATORS]") {
            bool b = (val == "1");
            if      (key == "sma20")    ws.indSettings.sma20    = b;
            else if (key == "sma50")    ws.indSettings.sma50    = b;
            else if (key == "sma200")   ws.indSettings.sma200   = b;
            else if (key == "ema12")    ws.indSettings.ema12    = b;
            else if (key == "ema26")    ws.indSettings.ema26    = b;
            else if (key == "wma20")    ws.indSettings.wma20    = b;
            else if (key == "bb")       ws.indSettings.bb       = b;
            else if (key == "rsi")      ws.indSettings.rsi      = b;
            else if (key == "macd")     ws.indSettings.macd     = b;
            else if (key == "stoch")    ws.indSettings.stoch    = b;
            else if (key == "atr")      ws.indSettings.atr      = b;
            else if (key == "vwap")     ws.indSettings.vwap     = b;
            else if (key == "fibo")     ws.indSettings.fibo     = b;
            else if (key == "sr")       ws.indSettings.sr       = b;
            else if (key == "ichimoku") ws.indSettings.ichimoku = b;
            else if (key == "volume")   ws.indSettings.volume   = b;
        }
        else if (section == "[PREDICTORS]") {
            bool b = (val == "1");
            if      (key == "linreg")      ws.predSettings.linReg      = b;
            else if (key == "emaproj")     ws.predSettings.emaProj     = b;
            else if (key == "momentum")    ws.predSettings.momentum    = b;
            else if (key == "holtwinters") ws.predSettings.holtWinters = b;
        }
    }

    if (ws.candles.empty()) {
        errMsg = L"No candle data found in workspace file";
        return false;
    }
    return true;
}
