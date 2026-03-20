// Python.h must come first on Windows to avoid macro conflicts
#include <Python.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlwapi.h>
#include "PythonBridge.h"
#include <sstream>
#include <algorithm>
#include <limits>
#include <cstring>

// ─── Singleton ───────────────────────────────────────────────────────────────
PythonBridge& PythonBridge::instance() {
    static PythonBridge inst;
    return inst;
}

// ─── Helpers ─────────────────────────────────────────────────────────────────
std::wstring PythonBridge::getExeDir() {
    wchar_t buf[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    PathRemoveFileSpecW(buf);
    return std::wstring(buf);
}

std::string PythonBridge::wstrToUtf8(const std::wstring& ws) {
    if (ws.empty()) return {};
    int n = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string s(n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), -1, &s[0], n, nullptr, nullptr);
    return s;
}

std::wstring PythonBridge::utf8ToWstr(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    std::wstring ws(n - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], n);
    return ws;
}

std::vector<double> PythonBridge::parseDblArray(const std::string& s) {
    std::vector<double> result;
    if (s.empty() || s == "ERROR" || s.rfind("ERR", 0) == 0) return result;
    std::stringstream ss(s);
    std::string token;
    while (std::getline(ss, token, '|')) {
        try {
            if (!token.empty() && token != "nan" && token != "NaN" && token != "None")
                result.push_back(std::stod(token));
            else
                result.push_back(std::numeric_limits<double>::quiet_NaN());
        } catch (...) {
            result.push_back(std::numeric_limits<double>::quiet_NaN());
        }
    }
    return result;
}

// ─── init ────────────────────────────────────────────────────────────────────
void PythonBridge::init() {
    if (m_ready) return;

    Py_Initialize();
    if (!Py_IsInitialized()) return;

    m_exeDir = getExeDir();

    // Add exeDir, exeDir\analysis, and exeDir\scripts to sys.path
    PyObject* sysPath = PySys_GetObject("path");  // borrowed ref
    if (sysPath) {
        // exeDir itself (so we can do 'import analysis')
        {
            PyObject* p = PyUnicode_FromWideChar(m_exeDir.c_str(), -1);
            PyList_Insert(sysPath, 0, p);
            Py_DECREF(p);
        }
        // exeDir\analysis
        {
            std::wstring ap = m_exeDir + L"\\analysis";
            PyObject* p = PyUnicode_FromWideChar(ap.c_str(), -1);
            PyList_Insert(sysPath, 0, p);
            Py_DECREF(p);
        }
        // project root\scripts (one level up from exe in build\)
        {
            std::wstring sp = m_exeDir + L"\\..\\scripts";
            PyObject* p = PyUnicode_FromWideChar(sp.c_str(), -1);
            PyList_Insert(sysPath, 0, p);
            Py_DECREF(p);
        }
    }

    // Set up output capture module
    PyRun_SimpleString(
        "import sys, io\n"
        "class _EGACapture:\n"
        "    def __init__(self): self._buf = io.StringIO()\n"
        "    def write(self, s): self._buf.write(s)\n"
        "    def flush(self): pass\n"
        "    def getvalue(self): return self._buf.getvalue()\n"
        "    def reset(self): self._buf = io.StringIO()\n"
        "_ega_stdout = _EGACapture()\n"
        "_ega_stderr = _EGACapture()\n"
    );

    m_ready = true;
}

void PythonBridge::shutdown() {
    if (m_ready) {
        Py_Finalize();
        m_ready = false;
    }
}

// ─── callPyFunc: call module.func(argStr) -> string result ───────────────────
std::string PythonBridge::callPyFunc(const char* moduleName,
                                      const char* funcName,
                                      const char* argStr) {
    if (!m_ready) return "ERROR:not_ready";

    PyObject* mod = PyImport_ImportModule(moduleName);
    if (!mod) {
        PyErr_Print();
        return std::string("ERROR:import_") + moduleName;
    }

    PyObject* func = PyObject_GetAttrString(mod, funcName);
    if (!func || !PyCallable_Check(func)) {
        Py_XDECREF(func);
        Py_DECREF(mod);
        return std::string("ERROR:func_") + funcName;
    }

    PyObject* arg  = PyUnicode_FromString(argStr);
    PyObject* ret  = PyObject_CallFunctionObjArgs(func, arg, nullptr);
    Py_DECREF(arg);
    Py_DECREF(func);
    Py_DECREF(mod);

    if (!ret) {
        PyErr_Print();
        return "ERROR:call_failed";
    }

    std::string result;
    if (PyUnicode_Check(ret)) {
        const char* s = PyUnicode_AsUTF8(ret);
        if (s) result = s;
    }
    Py_DECREF(ret);
    return result;
}

// ─── loadFile ────────────────────────────────────────────────────────────────
bool PythonBridge::loadFile(const std::wstring& path,
                             std::vector<Candle>& out,
                             std::wstring& errMsg) {
    if (!m_ready) { errMsg = L"Python not ready"; return false; }

    std::string pathUtf8 = wstrToUtf8(path);
    std::string result   = callPyFunc("data_loader", "load_file_flat", pathUtf8.c_str());

    if (result.empty() || result.rfind("ERROR", 0) == 0) {
        errMsg = utf8ToWstr(result.empty() ? "Unknown error" : result);
        return false;
    }

    // Format: one line per candle: label|open|high|low|close|volume
    out.clear();
    std::stringstream ss(result);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        std::stringstream ls(line);
        std::string tok;
        std::vector<std::string> parts;
        while (std::getline(ls, tok, '|')) parts.push_back(tok);
        if (parts.size() < 6) continue;
        Candle c;
        c.label  = utf8ToWstr(parts[0]);
        try {
            c.open   = std::stod(parts[1]);
            c.high   = std::stod(parts[2]);
            c.low    = std::stod(parts[3]);
            c.close  = std::stod(parts[4]);
            c.volume = std::stod(parts[5]);
        } catch (...) { continue; }
        out.push_back(c);
    }
    return !out.empty();
}

// ─── computeIndicators ───────────────────────────────────────────────────────
bool PythonBridge::computeIndicators(const std::vector<Candle>& candles,
                                      const IndicatorSettings&   s,
                                      IndicatorData&             out,
                                      std::wstring&              errMsg) {
    if (!m_ready || candles.empty()) return false;

    // Build a compact pipe-delimited candle string: open|high|low|close|volume per line
    std::ostringstream cs;
    for (auto& c : candles)
        cs << c.open << '|' << c.high << '|' << c.low << '|'
           << c.close << '|' << c.volume << '\n';

    // Build settings flags string
    std::ostringstream fs;
    fs << (s.sma20 ? 1 : 0) << '|' << (s.sma50 ? 1 : 0) << '|' << (s.sma200 ? 1 : 0) << '|'
       << (s.ema12 ? 1 : 0) << '|' << (s.ema26 ? 1 : 0) << '|' << (s.wma20 ? 1 : 0)  << '|'
       << (s.bb    ? 1 : 0) << '|' << (s.rsi   ? 1 : 0) << '|' << (s.macd  ? 1 : 0)  << '|'
       << (s.stoch ? 1 : 0) << '|' << (s.atr   ? 1 : 0) << '|' << (s.vwap  ? 1 : 0)  << '|'
       << (s.fibo  ? 1 : 0) << '|' << (s.sr    ? 1 : 0) << '|' << (s.ichimoku ? 1 : 0);

    // Combined arg: candles section + "---" divider + settings
    std::string arg = cs.str() + "---\n" + fs.str();
    std::string result = callPyFunc("indicators", "compute_all", arg.c_str());

    if (result.rfind("ERROR", 0) == 0) {
        errMsg = utf8ToWstr(result);
        return false;
    }

    // Parse result: sections separated by "---"
    // Each section is: SECTION_NAME\nvalues|values|...
    std::stringstream ss(result);
    std::string line;
    std::string section;
    std::vector<double>* target = nullptr;

    auto getTarget = [&](const std::string& name) -> std::vector<double>* {
        if (name == "SMA20")           return &out.sma20;
        if (name == "SMA50")           return &out.sma50;
        if (name == "SMA200")          return &out.sma200;
        if (name == "EMA12")           return &out.ema12;
        if (name == "EMA26")           return &out.ema26;
        if (name == "WMA20")           return &out.wma20;
        if (name == "BB_UPPER")        return &out.bbUpper;
        if (name == "BB_MIDDLE")       return &out.bbMiddle;
        if (name == "BB_LOWER")        return &out.bbLower;
        if (name == "VWAP")            return &out.vwap;
        if (name == "RSI14")           return &out.rsi14;
        if (name == "MACD_LINE")       return &out.macdLine;
        if (name == "MACD_SIGNAL")     return &out.signalLine;
        if (name == "MACD_HIST")       return &out.macdHist;
        if (name == "STOCH_K")         return &out.stochK;
        if (name == "STOCH_D")         return &out.stochD;
        if (name == "ATR14")           return &out.atr14;
        if (name == "ICHITENKAN")      return &out.ichimokuTenkan;
        if (name == "ICHIKIJUN")       return &out.ichimokuKijun;
        if (name == "ICHISPANA")       return &out.ichimokuSpanA;
        if (name == "ICHISPANB")       return &out.ichimokuSpanB;
        if (name == "FIBOLEVELS")      return &out.fiboLevels;
        if (name == "SRLEVELS")        return &out.srLevels;
        return nullptr;
    };

    while (std::getline(ss, line)) {
        if (line == "---") { target = nullptr; continue; }
        if (!line.empty() && line.find('|') == std::string::npos && line.back() != '|') {
            // Section header
            section = line;
            target  = getTarget(section);
        } else if (target) {
            *target = parseDblArray(line);
        }
    }
    return true;
}

// ─── computePredictors ───────────────────────────────────────────────────────
bool PythonBridge::computePredictors(const std::vector<Candle>&    candles,
                                      const PredictorSettings&      s,
                                      std::vector<PredictorResult>& out,
                                      std::wstring&                 errMsg) {
    if (!m_ready || candles.empty()) return false;

    std::ostringstream cs;
    for (auto& c : candles)
        cs << c.close << '\n';

    std::ostringstream fs;
    fs << (s.linReg      ? 1 : 0) << '|'
       << (s.emaProj     ? 1 : 0) << '|'
       << (s.momentum    ? 1 : 0) << '|'
       << (s.holtWinters ? 1 : 0);

    std::string arg    = cs.str() + "---\n" + fs.str();
    std::string result = callPyFunc("predictors", "compute_all", arg.c_str());

    if (result.rfind("ERROR", 0) == 0) {
        errMsg = utf8ToWstr(result);
        return false;
    }

    // Format: NAME|RECOMMENDED|CONFIDENCE\nval|val|val|...
    out.clear();
    std::stringstream ss(result);
    std::string line;
    PredictorResult* current = nullptr;

    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        if (line.rfind("PRED:", 0) == 0) {
            // Header: PRED:name|recommended|confidence
            out.emplace_back();
            current = &out.back();
            std::string hdr = line.substr(5);
            std::vector<std::string> parts;
            std::stringstream hs(hdr);
            std::string tok;
            while (std::getline(hs, tok, '|')) parts.push_back(tok);
            if (parts.size() >= 3) {
                current->name        = utf8ToWstr(parts[0]);
                current->recommended = parts[1] == "1";
                try { current->confidence = std::stod(parts[2]); } catch (...) {}
            }
        } else if (current) {
            current->forecast = parseDblArray(line);
            current = nullptr;
        }
    }
    return true;
}

// ─── runConsoleCode ───────────────────────────────────────────────────────────
std::wstring PythonBridge::runConsoleCode(const std::wstring& code) {
    if (!m_ready) return L"Python not initialised.";

    std::string codeUtf8 = wstrToUtf8(code);

    // Redirect stdout/stderr
    PyRun_SimpleString("_ega_stdout.reset(); _ega_stderr.reset()");
    PyRun_SimpleString("import sys; sys.stdout = _ega_stdout; sys.stderr = _ega_stderr");

    // Run the code
    PyObject* main_mod  = PyImport_AddModule("__main__");
    PyObject* main_dict = PyModule_GetDict(main_mod);
    PyObject* result    = PyRun_String(codeUtf8.c_str(), Py_file_input, main_dict, main_dict);

    // Restore stdout/stderr
    PyRun_SimpleString("sys.stdout = sys.__stdout__; sys.stderr = sys.__stderr__");

    std::wstring out;
    if (!result) {
        // Get traceback
        PyErr_Print();
        PyObject* errCap = PyObject_GetAttrString(PyImport_AddModule("__main__"), "_ega_stderr");
        if (errCap) {
            PyObject* val = PyObject_CallMethod(errCap, "getvalue", nullptr);
            if (val && PyUnicode_Check(val))
                out = utf8ToWstr(PyUnicode_AsUTF8(val));
            Py_XDECREF(val);
            Py_DECREF(errCap);
        }
        if (out.empty()) out = L"Error running code.";
    } else {
        Py_DECREF(result);
        PyObject* mainMod  = PyImport_AddModule("__main__");
        PyObject* outCap   = PyObject_GetAttrString(mainMod, "_ega_stdout");
        if (outCap) {
            PyObject* val = PyObject_CallMethod(outCap, "getvalue", nullptr);
            if (val && PyUnicode_Check(val))
                out = utf8ToWstr(PyUnicode_AsUTF8(val));
            Py_XDECREF(val);
            Py_DECREF(outCap);
        }
    }
    return out;
}

// ─── listUserScripts ─────────────────────────────────────────────────────────
std::vector<std::wstring> PythonBridge::listUserScripts() {
    std::vector<std::wstring> scripts;
    std::wstring pattern = m_exeDir + L"\\..\\scripts\\*.py";
    WIN32_FIND_DATAW fd;
    HANDLE hFind = FindFirstFileW(pattern.c_str(), &fd);
    if (hFind == INVALID_HANDLE_VALUE) return scripts;
    do {
        std::wstring name = fd.cFileName;
        if (name.rfind(L"example_", 0) != 0)   // skip example files
            scripts.push_back(name);
    } while (FindNextFileW(hFind, &fd));
    FindClose(hFind);
    return scripts;
}

// ─── fetchTicker ─────────────────────────────────────────────────────────────
bool PythonBridge::fetchTicker(const std::wstring& ticker,
                                const std::wstring& period,
                                const std::wstring& tfLabel,
                                std::vector<Candle>& out,
                                std::wstring& errMsg) {
    if (!m_ready) { errMsg = L"Python not ready"; return false; }

    // Build arg: "TICKER|PERIOD|TFLABEL"
    std::string arg = wstrToUtf8(ticker) + "|"
                    + wstrToUtf8(period) + "|"
                    + wstrToUtf8(tfLabel);

    std::string result = callPyFunc("yahoo_finance", "fetch_ticker", arg.c_str());

    if (result.empty() || result.rfind("ERROR", 0) == 0) {
        errMsg = utf8ToWstr(result.empty() ? "No data returned" : result);
        return false;
    }

    // Same flat format as loadFile: label|open|high|low|close|volume
    out.clear();
    std::stringstream ss(result);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        std::stringstream ls(line);
        std::string tok;
        std::vector<std::string> parts;
        while (std::getline(ls, tok, '|')) parts.push_back(tok);
        if (parts.size() < 6) continue;
        Candle c;
        c.label  = utf8ToWstr(parts[0]);
        try {
            c.open   = std::stod(parts[1]);
            c.high   = std::stod(parts[2]);
            c.low    = std::stod(parts[3]);
            c.close  = std::stod(parts[4]);
            c.volume = std::stod(parts[5]);
        } catch (...) { continue; }
        out.push_back(c);
    }

    if (out.empty()) {
        errMsg = L"Downloaded data had no valid candles";
        return false;
    }
    return true;
}

// ─── searchTicker ────────────────────────────────────────────────────────────
std::vector<std::wstring> PythonBridge::searchTicker(const std::wstring& query) {
    std::vector<std::wstring> results;
    if (!m_ready || query.empty()) return results;

    std::string arg    = wstrToUtf8(query);
    std::string result = callPyFunc("yahoo_finance", "search_ticker", arg.c_str());

    if (result.empty() || result.rfind("ERROR", 0) == 0) return results;

    std::stringstream ss(result);
    std::string line;
    while (std::getline(ss, line)) {
        if (!line.empty())
            results.push_back(utf8ToWstr(line));
    }
    return results;
}

// ─── tickerInfo ──────────────────────────────────────────────────────────────
std::wstring PythonBridge::tickerInfo(const std::wstring& ticker) {
    if (!m_ready) return L"";
    std::string result = callPyFunc("yahoo_finance", "fetch_ticker_info",
                                    wstrToUtf8(ticker).c_str());
    if (result.rfind("ERROR", 0) == 0) return L"";
    return utf8ToWstr(result);
}

// ─── runUserScript ───────────────────────────────────────────────────────────
std::wstring PythonBridge::runUserScript(const std::wstring& scriptName) {
    std::wstring path = m_exeDir + L"\\..\\scripts\\" + scriptName;
    // Read file
    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return L"Cannot open script file.";
    DWORD sz = GetFileSize(hFile, nullptr);
    std::string buf(sz, '\0');
    DWORD read = 0;
    ReadFile(hFile, &buf[0], sz, &read, nullptr);
    CloseHandle(hFile);
    buf.resize(read);
    return runConsoleCode(utf8ToWstr(buf));
}
