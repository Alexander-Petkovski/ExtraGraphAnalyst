#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <commdlg.h>
#include <uxtheme.h>
#include <dwmapi.h>
// Dark mode constant – defined in SDK 10.0.18362+, define it ourselves if missing
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#include <shlwapi.h>
#include <objbase.h>
#include <gdiplus.h>
#include "MainWindow.h"
#include "ChartWindow.h"
#include "PythonBridge.h"
#include <algorithm>
#include <sstream>

#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "dwmapi.lib")

const wchar_t* MainWindow::CLASS_NAME = L"EGA_Main";

// ─── registerClass ────────────────────────────────────────────────────────────
bool MainWindow::registerClass(HINSTANCE hInst) {
    WNDCLASSEXW wc   = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(CLR_BG);
    wc.lpszClassName = CLASS_NAME;
    wc.hIcon         = LoadIconW(nullptr, IDI_APPLICATION);
    wc.hIconSm       = LoadIconW(nullptr, IDI_APPLICATION);
    return RegisterClassExW(&wc) != 0;
}

// ─── create ──────────────────────────────────────────────────────────────────
MainWindow* MainWindow::create(HINSTANCE hInst) {
    auto* win  = new MainWindow(hInst);
    HWND  hwnd = CreateWindowExW(
        0,
        CLASS_NAME,
        L"ExtraGraphAnalyst  –  Technical Analysis Suite",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 1400, 900,
        nullptr, nullptr, hInst,
        win          // lpCreateParams -> stored in WM_NCCREATE
    );
    if (!hwnd) { delete win; return nullptr; }
    return win;
}

void MainWindow::show(int nCmdShow) {
    ShowWindow(m_hwnd, nCmdShow);
    UpdateWindow(m_hwnd);
}

// ─── WndProc ─────────────────────────────────────────────────────────────────
LRESULT CALLBACK MainWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* cs   = reinterpret_cast<CREATESTRUCTW*>(lp);
        auto* self = reinterpret_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) self->m_hwnd = hwnd;
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    auto* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self) return DefWindowProcW(hwnd, msg, wp, lp);
    return self->handleMessage(msg, wp, lp);
}

LRESULT MainWindow::handleMessage(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        onCreate();
        return 0;

    case WM_SIZE:
        if (wp != SIZE_MINIMIZED)
            onSize(LOWORD(lp), HIWORD(lp));
        return 0;

    case WM_COMMAND:
        onCommand(LOWORD(wp), HIWORD(wp), (HWND)lp);
        return 0;

    case WM_ERASEBKGND: {
        RECT rc; GetClientRect(m_hwnd, &rc);
        FillRect((HDC)wp, &rc, m_bgBrush);
        return 1;
    }

    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORBTN:
    case WM_CTLCOLORLISTBOX:
        return onCtlColor((HDC)wp, (HWND)lp, msg);

    case WM_DESTROY:
        onDestroy();
        PostQuitMessage(0);
        return 0;

    // Enter in the ticker box triggers fetch
    case WM_KEYDOWN:
        if (wp == VK_RETURN && GetFocus() == m_edtTicker) {
            fetchFromYahoo();
            return 0;
        }
        break;

    default:
        return DefWindowProcW(m_hwnd, msg, wp, lp);
    }
    return DefWindowProcW(m_hwnd, msg, wp, lp);
}

// ─── Helper: make a BUTTON ───────────────────────────────────────────────────
static HWND makeBtn(HWND parent, HINSTANCE hInst, LPCWSTR text, int id,
                    int x, int y, int w, int h,
                    DWORD extra = 0) {
    return CreateWindowExW(0, L"BUTTON", text,
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | extra,
        x, y, w, h,
        parent, (HMENU)(UINT_PTR)id, hInst, nullptr);
}


static HWND makeLbl(HWND parent, HINSTANCE hInst, LPCWSTR text,
                    int x, int y, int w, int h) {
    return CreateWindowExW(0, L"STATIC", text,
        WS_CHILD | WS_VISIBLE | SS_LEFT | SS_CENTERIMAGE,
        x, y, w, h,
        parent, nullptr, hInst, nullptr);
}

// ─── onCreate ────────────────────────────────────────────────────────────────
void MainWindow::onCreate() {
    HINSTANCE hi = m_hInst;

    // GDI brushes
    m_bgBrush    = CreateSolidBrush(CLR_BG);
    m_panelBrush = CreateSolidBrush(CLR_PANEL);
    m_editBrush  = CreateSolidBrush(CLR_EDIT);

    // ── Enable dark mode title bar (Windows 10 1903+) ──────────────────
    BOOL dark = TRUE;
    DwmSetWindowAttribute(m_hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

    // ── Status bar ────────────────────────────────────────────────────
    m_hStatus = CreateWindowExW(0, STATUSCLASSNAME, nullptr,
        WS_CHILD | WS_VISIBLE | SBARS_SIZEGRIP,
        0, 0, 0, 0,
        m_hwnd, (HMENU)ID_STATUSBAR, hi, nullptr);
    int sbParts[] = { 600, 900, -1 };
    SendMessageW(m_hStatus, SB_SETPARTS, 3, (LPARAM)sbParts);
    SendMessageW(m_hStatus, SB_SETTEXTW, 0, (LPARAM)L" ExtraGraphAnalyst  |  Ready");
    SendMessageW(m_hStatus, SB_SETTEXTW, 1, (LPARAM)L" No file loaded");

    // ── Toolbar row 1 ─────────────────────────────────────────────────
    //   [Open File]  [Candle]  [Line]  | TF: [combo] | Ticker: [___] [Fetch] | Period: [combo]
    int x = 6, y = 6, bh = 26;
    m_btnOpen   = makeBtn(m_hwnd, hi, L"Open CSV",    ID_FILE_OPEN,    x,   y, 76, bh); x += 80;
    m_btnCandle = makeBtn(m_hwnd, hi, L"Candle",      ID_CHART_CANDLE, x,   y, 58, bh); x += 62;
    m_btnLine   = makeBtn(m_hwnd, hi, L"Line",        ID_CHART_LINE,   x,   y, 46, bh); x += 52;
    x += 8;
    m_lblTF     = makeLbl(m_hwnd, hi, L"TF:", x, y + 5, 24, 16); x += 26;
    m_cmbTF     = CreateWindowExW(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        x, y, 74, 200, m_hwnd, (HMENU)ID_TF_COMBO, hi, nullptr);
    const wchar_t* tfs[] = { L"15m", L"30m", L"1H", L"4H", L"Daily", L"Weekly", L"Monthly", L"Yearly" };
    for (auto t : tfs) SendMessageW(m_cmbTF, CB_ADDSTRING, 0, (LPARAM)t);
    SendMessageW(m_cmbTF, CB_SETCURSEL, 4, 0); // Default: Daily
    x += 78;

    // ── Yahoo Finance: ticker input ────────────────────────────────────
    x += 8;
    m_lblTicker = makeLbl(m_hwnd, hi, L"Ticker:", x, y + 5, 46, 16); x += 48;
    m_edtTicker = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_UPPERCASE | ES_AUTOHSCROLL,
        x, y + 2, 90, bh - 4, m_hwnd, (HMENU)ID_TICKER_EDIT, hi, nullptr);
    x += 94;
    m_btnFetch  = makeBtn(m_hwnd, hi, L"Fetch \x25BC", ID_TICKER_FETCH, x, y, 62, bh); x += 66;
    x += 6;
    m_lblPeriod = makeLbl(m_hwnd, hi, L"Period:", x, y + 5, 48, 16); x += 50;
    m_cmbPeriod = CreateWindowExW(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        x, y, 80, 200, m_hwnd, (HMENU)ID_PERIOD_COMBO, hi, nullptr);
    const wchar_t* periods[] = {
        L"1mo", L"3mo", L"6mo", L"1y", L"2y", L"5y", L"10y", L"max"
    };
    for (auto p : periods) SendMessageW(m_cmbPeriod, CB_ADDSTRING, 0, (LPARAM)p);
    SendMessageW(m_cmbPeriod, CB_SETCURSEL, 3, 0); // Default: 1y

    // ── Toolbar row 2: Category dropdown buttons + Scripts ────────────
    x = 6; y = 36;
    m_btnMovAvgs  = makeBtn(m_hwnd, hi, L"Moving Avgs \x25BC",  ID_BTN_MOVAVGS,     x, y, 120, 24); x += 124;
    m_btnOscills  = makeBtn(m_hwnd, hi, L"Oscillators \x25BC",  ID_BTN_OSCILLATORS, x, y, 118, 24); x += 122;
    m_btnBands    = makeBtn(m_hwnd, hi, L"Bands & Levels \x25BC", ID_BTN_BANDS,     x, y, 138, 24); x += 142;
    m_btnPredMenu = makeBtn(m_hwnd, hi, L"Predictors \x25BC",   ID_BTN_PREDICTORS,  x, y, 114, 24); x += 118;
    x += 14;
    m_lblScripts   = makeLbl(m_hwnd, hi, L"Script:", x, y + 5, 50, 16); x += 52;
    m_cmbScripts   = CreateWindowExW(0, L"COMBOBOX", nullptr,
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        x, y, 160, 200, m_hwnd, (HMENU)ID_SCRIPT_COMBO, hi, nullptr);
    x += 164;
    m_btnRunScript = makeBtn(m_hwnd, hi, L"Run",     ID_SCRIPT_RUN,     x, y, 44, 24); x += 46;
    m_btnRefresh   = makeBtn(m_hwnd, hi, L"Refresh", ID_SCRIPT_REFRESH, x, y, 60, 24);

    // volume is true by default (set in IndicatorSettings struct)
    updateDropdownLabels();

    // ── Python Console ────────────────────────────────────────────────
    // Created last (positioned in onSize)
    m_lblConsole = makeLbl(m_hwnd, hi, L"Python Console", 0, 0, 120, 16);

    m_edtConsoleIn  = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
        0, 0, 100, 50,
        m_hwnd, (HMENU)ID_CONSOLE_EDIT, hi, nullptr);

    m_edtConsoleOut = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L">>> ",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL |
        WS_VSCROLL | ES_READONLY,
        0, 0, 100, 50,
        m_hwnd, (HMENU)ID_CONSOLE_OUT, hi, nullptr);

    m_btnRun   = makeBtn(m_hwnd, hi, L"Run",   ID_CONSOLE_RUN,   0, 0, 50, 24);
    m_btnClear = makeBtn(m_hwnd, hi, L"Clear", ID_CONSOLE_CLEAR, 0, 0, 50, 24);

    // Apply dark theme hints to controls
    auto applyTheme = [](HWND hwnd) {
        SetWindowTheme(hwnd, L"DarkMode_Explorer", nullptr);
    };
    applyTheme(m_cmbTF);
    applyTheme(m_cmbPeriod);
    applyTheme(m_edtTicker);
    applyTheme(m_cmbScripts);
    applyTheme(m_edtConsoleIn);
    applyTheme(m_edtConsoleOut);

    // Populate scripts
    refreshScripts();
}

// ─── onSize ──────────────────────────────────────────────────────────────────
void MainWindow::onSize(int w, int h) {
    if (!m_hStatus) return;

    // Status bar
    SendMessageW(m_hStatus, WM_SIZE, 0, 0);
    RECT sbRc = {}; GetWindowRect(m_hStatus, &sbRc);
    int sbH = sbRc.bottom - sbRc.top;

    const int TOOLBAR_H = 66;         // 2 rows of controls
    const int CONSOLE_H = 200;        // python console area
    const int CHART_H   = std::max(100, h - sbH - TOOLBAR_H - CONSOLE_H);
    const int CONSOLE_Y = TOOLBAR_H + CHART_H;

    // Chart window
    if (m_chart)
        m_chart->moveResize(0, TOOLBAR_H, w, CHART_H);

    // Console area layout
    const int RUN_W   = 54;
    const int CLR_W   = 54;
    const int BTN_H   = 24;
    const int LBL_H   = 16;
    const int MARGIN  = 4;
    int inH   = 70;   // input box height
    int outH  = CONSOLE_H - LBL_H - MARGIN - inH - BTN_H - MARGIN * 3;
    if (outH < 40) outH = 40;

    MoveWindow(m_lblConsole,   MARGIN,              CONSOLE_Y + MARGIN,
               180,            LBL_H,               TRUE);
    MoveWindow(m_edtConsoleOut, MARGIN,             CONSOLE_Y + MARGIN + LBL_H + MARGIN,
               w - MARGIN * 2, outH,                TRUE);
    int inputY = CONSOLE_Y + MARGIN + LBL_H + MARGIN + outH + MARGIN;
    MoveWindow(m_edtConsoleIn,  MARGIN,             inputY,
               w - MARGIN * 3 - RUN_W - CLR_W,     inH,     TRUE);
    MoveWindow(m_btnRun,   w - MARGIN - RUN_W - CLR_W, inputY,   RUN_W, BTN_H, TRUE);
    MoveWindow(m_btnClear, w - MARGIN - CLR_W,         inputY,   CLR_W, BTN_H, TRUE);
}

// ─── onCommand ───────────────────────────────────────────────────────────────
void MainWindow::onCommand(int id, int code, HWND /*hCtrl*/) {
    switch (id) {
    case ID_FILE_OPEN:       openFile();             break;
    case ID_TICKER_FETCH:    fetchFromYahoo();       break;
    case ID_TICKER_EDIT:
        if (code == EN_CHANGE) { /* live – do nothing */ }
        break;
    case ID_CHART_CANDLE:
        if (m_chart) m_chart->setMode(ChartMode::Candlestick);
        break;
    case ID_CHART_LINE:
        if (m_chart) m_chart->setMode(ChartMode::Line);
        break;

    // Category dropdown buttons → open popup menus
    case ID_BTN_MOVAVGS:     showMovingAvgsMenu(m_btnMovAvgs);   break;
    case ID_BTN_OSCILLATORS: showOscillatorsMenu(m_btnOscills);  break;
    case ID_BTN_BANDS:       showBandsMenu(m_btnBands);          break;
    case ID_BTN_PREDICTORS:  showPredictorsMenu(m_btnPredMenu);  break;

    // Indicator menu item selected → toggle + refresh
    case ID_IND_SMA20:  case ID_IND_SMA50:  case ID_IND_SMA200:
    case ID_IND_EMA12:  case ID_IND_EMA26:  case ID_IND_WMA20:
    case ID_IND_BB:     case ID_IND_RSI:    case ID_IND_MACD:
    case ID_IND_STOCH:  case ID_IND_ATR:    case ID_IND_VWAP:
    case ID_IND_FIBO:   case ID_IND_SR:     case ID_IND_ICHIMOKU:
    case ID_IND_VOLUME:
        toggleIndicator(id);
        break;

    // Predictor menu item selected → toggle + refresh
    case ID_PRED_LINREG:     case ID_PRED_EMAPROJ:
    case ID_PRED_MOMENTUM:   case ID_PRED_HOLTWINTERS:
        togglePredictor(id);
        break;

    case ID_CONSOLE_RUN:     runConsole();    break;
    case ID_CONSOLE_CLEAR:   clearConsole();  break;
    case ID_SCRIPT_RUN:      runScript();     break;
    case ID_SCRIPT_REFRESH:  refreshScripts(); break;
    }
}

// ─── onCtlColor ──────────────────────────────────────────────────────────────
LRESULT MainWindow::onCtlColor(HDC hdc, HWND hCtrl, UINT msg) {
    SetTextColor(hdc, CLR_TEXT);
    SetBkColor(hdc, CLR_PANEL);

    if (msg == WM_CTLCOLOREDIT) {
        SetBkColor(hdc, CLR_EDIT);
        return (LRESULT)m_editBrush;
    }
    if (msg == WM_CTLCOLORLISTBOX) {
        SetBkColor(hdc, CLR_EDIT);
        return (LRESULT)m_editBrush;
    }
    return (LRESULT)m_panelBrush;
}

// ─── openFile ────────────────────────────────────────────────────────────────
void MainWindow::openFile() {
    wchar_t filePath[MAX_PATH] = {};
    OPENFILENAMEW ofn       = {};
    ofn.lStructSize          = sizeof(ofn);
    ofn.hwndOwner            = m_hwnd;
    ofn.lpstrFilter          = L"Data Files\0*.csv;*.xlsx;*.xls;*.txt\0CSV Files\0*.csv\0Excel Files\0*.xlsx;*.xls\0All Files\0*.*\0";
    ofn.lpstrFile            = filePath;
    ofn.nMaxFile             = MAX_PATH;
    ofn.lpstrTitle           = L"Open Financial Data";
    ofn.Flags                = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    if (!GetOpenFileNameW(&ofn)) return;

    setStatus(L" Loading file...");

    std::wstring errMsg;
    bool ok = PythonBridge::instance().loadFile(filePath, m_chartData.candles, errMsg);
    if (!ok) {
        std::wstring msg = L"Failed to load file: " + errMsg;
        MessageBoxW(m_hwnd, msg.c_str(), L"Load Error", MB_ICONERROR);
        setStatus(L" Load failed: " + errMsg);
        return;
    }

    m_chartData.loaded   = true;
    m_chartData.filePath = filePath;

    // Extract filename for status
    const wchar_t* fname = PathFindFileNameW(filePath);

    setStatus(std::wstring(L" Loaded: ") + fname +
              L"  (" + std::to_wstring(m_chartData.candles.size()) + L" candles)", 0);
    setStatus(std::wstring(L" ") + filePath, 1);

    if (m_chart) {
        m_chart->setData(&m_chartData);
        m_chart->setIndicatorSettings(m_indSettings);
    }

    // Auto-compute active indicators
    refreshIndicators();
    refreshPredictors();
}

// ─── fetchFromYahoo ──────────────────────────────────────────────────────────
void MainWindow::fetchFromYahoo() {
    // Read ticker symbol from edit box
    int len = GetWindowTextLengthW(m_edtTicker);
    if (len <= 0) {
        MessageBoxW(m_hwnd, L"Please enter a ticker symbol (e.g. AAPL, BTC-USD, ^GSPC)",
                    L"No Ticker", MB_ICONINFORMATION);
        return;
    }
    std::wstring ticker(len + 1, L'\0');
    GetWindowTextW(m_edtTicker, &ticker[0], len + 1);
    ticker.resize(len);

    // Trim whitespace
    while (!ticker.empty() && iswspace(ticker.front())) ticker.erase(ticker.begin());
    while (!ticker.empty() && iswspace(ticker.back()))  ticker.pop_back();
    if (ticker.empty()) return;

    // Read period from combo
    wchar_t periodBuf[32] = {};
    int pSel = (int)SendMessageW(m_cmbPeriod, CB_GETCURSEL, 0, 0);
    SendMessageW(m_cmbPeriod, CB_GETLBTEXT, pSel, (LPARAM)periodBuf);
    std::wstring period = periodBuf[0] ? std::wstring(periodBuf) : L"1y";

    // Read timeframe from combo
    wchar_t tfBuf[32] = {};
    int tfSel = (int)SendMessageW(m_cmbTF, CB_GETCURSEL, 0, 0);
    SendMessageW(m_cmbTF, CB_GETLBTEXT, tfSel, (LPARAM)tfBuf);
    std::wstring tfLabel = tfBuf[0] ? std::wstring(tfBuf) : L"Daily";

    setStatus(L" Fetching " + ticker + L" from Yahoo Finance...");

    // Disable fetch button during download
    EnableWindow(m_btnFetch, FALSE);

    std::wstring errMsg;
    bool ok = PythonBridge::instance().fetchTicker(ticker, period, tfLabel,
                                                    m_chartData.candles, errMsg);
    EnableWindow(m_btnFetch, TRUE);

    if (!ok) {
        std::wstring msg = L"Failed to fetch " + ticker + L":\n\n" + errMsg;
        // Strip "ERROR:" prefix for cleaner message
        if (msg.find(L"ERROR:") != std::wstring::npos)
            msg.erase(msg.find(L"ERROR:"), 6);
        MessageBoxW(m_hwnd, msg.c_str(), L"Yahoo Finance Error", MB_ICONWARNING);
        setStatus(L" Fetch failed for " + ticker);
        return;
    }

    m_chartData.loaded   = true;
    m_chartData.filePath = ticker;  // use ticker as identifier
    m_chartData.timeframe = tfLabel;

    // Fetch and show basic ticker info in status bar
    std::wstring info = PythonBridge::instance().tickerInfo(ticker);
    std::wstring statusInfo = ticker;
    if (!info.empty() && info.rfind(L"ERROR", 0) != 0) {
        // info = "Name|Sector|Currency|MarketCap|Exchange"
        // Extract name (first field) and currency
        size_t p1 = info.find(L'|');
        size_t p3 = info.find(L'|', p1 + 1);
        size_t p4 = info.find(L'|', p3 + 1);
        size_t p5 = info.find(L'|', p4 + 1);
        if (p1 != std::wstring::npos) {
            std::wstring name     = info.substr(0, p1);
            std::wstring currency = (p3 != std::wstring::npos && p4 != std::wstring::npos)
                                    ? info.substr(p3 + 1, p4 - p3 - 1) : L"";
            std::wstring mktcap  = (p4 != std::wstring::npos && p5 != std::wstring::npos)
                                    ? info.substr(p4 + 1, p5 - p4 - 1) : L"";
            statusInfo = ticker + L"  " + name;
            if (!currency.empty()) statusInfo += L"  [" + currency + L"]";
            if (!mktcap.empty() && mktcap != L"N/A" && mktcap != L"0")
                statusInfo += L"  Cap: " + mktcap;
        }
    }

    setStatus(L" " + statusInfo +
              L"  |  " + std::to_wstring(m_chartData.candles.size()) +
              L" bars  |  " + period + L"  " + tfLabel, 0);
    setStatus(L" Source: Yahoo Finance  |  " + period + L"  " + tfLabel, 1);

    if (m_chart) {
        m_chart->setData(&m_chartData);
        m_chart->setIndicatorSettings(m_indSettings);
    }

    refreshIndicators();
    refreshPredictors();
}

// ─── refreshIndicators ───────────────────────────────────────────────────────
void MainWindow::refreshIndicators() {
    // Update sub-panel flags from current settings
    SubPanelFlags sf;
    sf.volume = m_indSettings.volume;
    sf.rsi    = m_indSettings.rsi;
    sf.macd   = m_indSettings.macd;
    sf.stoch  = m_indSettings.stoch;
    if (m_chart) m_chart->setSubPanels(sf);

    if (!m_chartData.loaded) return;

    std::wstring errMsg;
    bool ok = PythonBridge::instance().computeIndicators(
        m_chartData.candles, m_indSettings, m_chartData.indicators, errMsg);
    if (!ok && !errMsg.empty())
        setStatus(L" Indicator error: " + errMsg);
    else
        setStatus(L" Indicators updated");

    if (m_chart) {
        m_chart->setIndicatorSettings(m_indSettings);
        m_chart->repaint();
    }
}

// ─── refreshPredictors ───────────────────────────────────────────────────────
void MainWindow::refreshPredictors() {
    if (!m_chartData.loaded) return;

    std::wstring errMsg;
    bool ok = PythonBridge::instance().computePredictors(
        m_chartData.candles, m_predSettings, m_chartData.predictors, errMsg);
    if (!ok && !errMsg.empty())
        setStatus(L" Predictor error: " + errMsg);
    else
        setStatus(L" Predictors updated");

    if (m_chart) {
        m_chart->setPredictorResults(m_chartData.predictors);
        m_chart->repaint();
    }
}

// ─── runConsole ──────────────────────────────────────────────────────────────
void MainWindow::runConsole() {
    int len = GetWindowTextLengthW(m_edtConsoleIn);
    if (len <= 0) return;

    std::wstring code(len + 1, L'\0');
    GetWindowTextW(m_edtConsoleIn, &code[0], len + 1);
    code.resize(len);

    appendConsoleOut(L">>> " + code + L"\n");

    std::wstring out = PythonBridge::instance().runConsoleCode(code);
    if (!out.empty()) appendConsoleOut(out + L"\n");

    // If data was possibly modified, refresh
    if (m_chartData.loaded) {
        refreshIndicators();
        refreshPredictors();
    }
}

void MainWindow::clearConsole() {
    SetWindowTextW(m_edtConsoleOut, L"");
}

void MainWindow::appendConsoleOut(const std::wstring& text) {
    int len = GetWindowTextLengthW(m_edtConsoleOut);
    SendMessageW(m_edtConsoleOut, EM_SETSEL, len, len);
    SendMessageW(m_edtConsoleOut, EM_REPLACESEL, FALSE, (LPARAM)text.c_str());
    SendMessageW(m_edtConsoleOut, EM_SCROLLCARET, 0, 0);
}

// ─── refreshScripts ──────────────────────────────────────────────────────────
void MainWindow::refreshScripts() {
    SendMessageW(m_cmbScripts, CB_RESETCONTENT, 0, 0);
    auto scripts = PythonBridge::instance().listUserScripts();
    for (auto& s : scripts)
        SendMessageW(m_cmbScripts, CB_ADDSTRING, 0, (LPARAM)s.c_str());
    if (!scripts.empty())
        SendMessageW(m_cmbScripts, CB_SETCURSEL, 0, 0);
    else
        SendMessageW(m_cmbScripts, CB_ADDSTRING, 0, (LPARAM)L"(no scripts found)");
}

// ─── runScript ───────────────────────────────────────────────────────────────
void MainWindow::runScript() {
    int sel = (int)SendMessageW(m_cmbScripts, CB_GETCURSEL, 0, 0);
    if (sel < 0) return;
    wchar_t buf[MAX_PATH] = {};
    SendMessageW(m_cmbScripts, CB_GETLBTEXT, sel, (LPARAM)buf);
    if (buf[0] == 0 || buf[0] == L'(') return;

    setStatus(L" Running script: " + std::wstring(buf));
    std::wstring out = PythonBridge::instance().runUserScript(buf);
    appendConsoleOut(L"=== Script: " + std::wstring(buf) + L" ===\n");
    appendConsoleOut(out.empty() ? L"(no output)\n" : out + L"\n");

    if (m_chartData.loaded) {
        refreshIndicators();
        refreshPredictors();
    }
}

// ─── Dropdown menu helpers ────────────────────────────────────────────────────
void MainWindow::appendMenuItem(HMENU m, int id, const wchar_t* lbl, bool checked) {
    MENUITEMINFOW mi  = {};
    mi.cbSize         = sizeof(mi);
    mi.fMask          = MIIM_ID | MIIM_STRING | MIIM_STATE;
    mi.wID            = (UINT)id;
    mi.dwTypeData     = const_cast<wchar_t*>(lbl);
    mi.fState         = checked ? MFS_CHECKED : MFS_UNCHECKED;
    InsertMenuItemW(m, GetMenuItemCount(m), TRUE, &mi);
}

void MainWindow::showMenuBelowBtn(HMENU m, HWND btn, HWND owner) {
    RECT rc; GetWindowRect(btn, &rc);
    TrackPopupMenu(m, TPM_LEFTALIGN | TPM_TOPALIGN,
                   rc.left, rc.bottom, 0, owner, nullptr);
    DestroyMenu(m);
}

void MainWindow::showMovingAvgsMenu(HWND btn) {
    HMENU m = CreatePopupMenu();
    appendMenuItem(m, ID_IND_SMA20,  L"SMA 20",   indicatorActive(ID_IND_SMA20));
    appendMenuItem(m, ID_IND_SMA50,  L"SMA 50",   indicatorActive(ID_IND_SMA50));
    appendMenuItem(m, ID_IND_SMA200, L"SMA 200",  indicatorActive(ID_IND_SMA200));
    appendMenuItem(m, ID_IND_EMA12,  L"EMA 12",   indicatorActive(ID_IND_EMA12));
    appendMenuItem(m, ID_IND_EMA26,  L"EMA 26",   indicatorActive(ID_IND_EMA26));
    appendMenuItem(m, ID_IND_WMA20,  L"WMA 20",   indicatorActive(ID_IND_WMA20));
    showMenuBelowBtn(m, btn, m_hwnd);
}

void MainWindow::showOscillatorsMenu(HWND btn) {
    HMENU m = CreatePopupMenu();
    appendMenuItem(m, ID_IND_RSI,    L"RSI (14)",     indicatorActive(ID_IND_RSI));
    appendMenuItem(m, ID_IND_MACD,   L"MACD",         indicatorActive(ID_IND_MACD));
    appendMenuItem(m, ID_IND_STOCH,  L"Stochastic",   indicatorActive(ID_IND_STOCH));
    appendMenuItem(m, ID_IND_ATR,    L"ATR (14)",     indicatorActive(ID_IND_ATR));
    appendMenuItem(m, ID_IND_VOLUME, L"Volume",       indicatorActive(ID_IND_VOLUME));
    showMenuBelowBtn(m, btn, m_hwnd);
}

void MainWindow::showBandsMenu(HWND btn) {
    HMENU m = CreatePopupMenu();
    appendMenuItem(m, ID_IND_BB,       L"Bollinger Bands",    indicatorActive(ID_IND_BB));
    appendMenuItem(m, ID_IND_VWAP,     L"VWAP",               indicatorActive(ID_IND_VWAP));
    appendMenuItem(m, ID_IND_FIBO,     L"Fibonacci",          indicatorActive(ID_IND_FIBO));
    appendMenuItem(m, ID_IND_SR,       L"Support/Resistance", indicatorActive(ID_IND_SR));
    appendMenuItem(m, ID_IND_ICHIMOKU, L"Ichimoku Cloud",     indicatorActive(ID_IND_ICHIMOKU));
    showMenuBelowBtn(m, btn, m_hwnd);
}

void MainWindow::showPredictorsMenu(HWND btn) {
    HMENU m = CreatePopupMenu();
    appendMenuItem(m, ID_PRED_LINREG,      L"Linear Regression", predictorActive(ID_PRED_LINREG));
    appendMenuItem(m, ID_PRED_EMAPROJ,     L"EMA Projection",    predictorActive(ID_PRED_EMAPROJ));
    appendMenuItem(m, ID_PRED_MOMENTUM,    L"Momentum",          predictorActive(ID_PRED_MOMENTUM));
    appendMenuItem(m, ID_PRED_HOLTWINTERS, L"Holt-Winters",      predictorActive(ID_PRED_HOLTWINTERS));
    showMenuBelowBtn(m, btn, m_hwnd);
}

void MainWindow::updateDropdownLabels() {
    int movAvgCount = (m_indSettings.sma20   ? 1 : 0)
                    + (m_indSettings.sma50   ? 1 : 0)
                    + (m_indSettings.sma200  ? 1 : 0)
                    + (m_indSettings.ema12   ? 1 : 0)
                    + (m_indSettings.ema26   ? 1 : 0)
                    + (m_indSettings.wma20   ? 1 : 0);

    int oscillCount = (m_indSettings.rsi     ? 1 : 0)
                    + (m_indSettings.macd    ? 1 : 0)
                    + (m_indSettings.stoch   ? 1 : 0)
                    + (m_indSettings.atr     ? 1 : 0)
                    + (m_indSettings.volume  ? 1 : 0);

    int bandsCount  = (m_indSettings.bb       ? 1 : 0)
                    + (m_indSettings.vwap     ? 1 : 0)
                    + (m_indSettings.fibo     ? 1 : 0)
                    + (m_indSettings.sr       ? 1 : 0)
                    + (m_indSettings.ichimoku ? 1 : 0);

    int predCount   = (m_predSettings.linReg      ? 1 : 0)
                    + (m_predSettings.emaProj     ? 1 : 0)
                    + (m_predSettings.momentum    ? 1 : 0)
                    + (m_predSettings.holtWinters ? 1 : 0);

    auto makeLabel = [](const wchar_t* name, int count) -> std::wstring {
        if (count > 0)
            return std::wstring(name) + L" (" + std::to_wstring(count) + L") \x25BC";
        return std::wstring(name) + L" \x25BC";
    };

    if (m_btnMovAvgs)  SetWindowTextW(m_btnMovAvgs,  makeLabel(L"Moving Avgs",   movAvgCount).c_str());
    if (m_btnOscills)  SetWindowTextW(m_btnOscills,  makeLabel(L"Oscillators",   oscillCount).c_str());
    if (m_btnBands)    SetWindowTextW(m_btnBands,    makeLabel(L"Bands & Levels", bandsCount).c_str());
    if (m_btnPredMenu) SetWindowTextW(m_btnPredMenu, makeLabel(L"Predictors",    predCount).c_str());
}

bool MainWindow::indicatorActive(int id) const {
    switch (id) {
    case ID_IND_SMA20:    return m_indSettings.sma20;
    case ID_IND_SMA50:    return m_indSettings.sma50;
    case ID_IND_SMA200:   return m_indSettings.sma200;
    case ID_IND_EMA12:    return m_indSettings.ema12;
    case ID_IND_EMA26:    return m_indSettings.ema26;
    case ID_IND_WMA20:    return m_indSettings.wma20;
    case ID_IND_BB:       return m_indSettings.bb;
    case ID_IND_RSI:      return m_indSettings.rsi;
    case ID_IND_MACD:     return m_indSettings.macd;
    case ID_IND_STOCH:    return m_indSettings.stoch;
    case ID_IND_ATR:      return m_indSettings.atr;
    case ID_IND_VWAP:     return m_indSettings.vwap;
    case ID_IND_FIBO:     return m_indSettings.fibo;
    case ID_IND_SR:       return m_indSettings.sr;
    case ID_IND_ICHIMOKU: return m_indSettings.ichimoku;
    case ID_IND_VOLUME:   return m_indSettings.volume;
    default:              return false;
    }
}

bool MainWindow::predictorActive(int id) const {
    switch (id) {
    case ID_PRED_LINREG:      return m_predSettings.linReg;
    case ID_PRED_EMAPROJ:     return m_predSettings.emaProj;
    case ID_PRED_MOMENTUM:    return m_predSettings.momentum;
    case ID_PRED_HOLTWINTERS: return m_predSettings.holtWinters;
    default:                  return false;
    }
}

void MainWindow::toggleIndicator(int id) {
    switch (id) {
    case ID_IND_SMA20:    m_indSettings.sma20    = !m_indSettings.sma20;    break;
    case ID_IND_SMA50:    m_indSettings.sma50    = !m_indSettings.sma50;    break;
    case ID_IND_SMA200:   m_indSettings.sma200   = !m_indSettings.sma200;   break;
    case ID_IND_EMA12:    m_indSettings.ema12    = !m_indSettings.ema12;    break;
    case ID_IND_EMA26:    m_indSettings.ema26    = !m_indSettings.ema26;    break;
    case ID_IND_WMA20:    m_indSettings.wma20    = !m_indSettings.wma20;    break;
    case ID_IND_BB:       m_indSettings.bb       = !m_indSettings.bb;       break;
    case ID_IND_RSI:      m_indSettings.rsi      = !m_indSettings.rsi;      break;
    case ID_IND_MACD:     m_indSettings.macd     = !m_indSettings.macd;     break;
    case ID_IND_STOCH:    m_indSettings.stoch    = !m_indSettings.stoch;    break;
    case ID_IND_ATR:      m_indSettings.atr      = !m_indSettings.atr;      break;
    case ID_IND_VWAP:     m_indSettings.vwap     = !m_indSettings.vwap;     break;
    case ID_IND_FIBO:     m_indSettings.fibo     = !m_indSettings.fibo;     break;
    case ID_IND_SR:       m_indSettings.sr       = !m_indSettings.sr;       break;
    case ID_IND_ICHIMOKU: m_indSettings.ichimoku = !m_indSettings.ichimoku; break;
    case ID_IND_VOLUME:   m_indSettings.volume   = !m_indSettings.volume;   break;
    }
    updateDropdownLabels();
    refreshIndicators();
}

void MainWindow::togglePredictor(int id) {
    switch (id) {
    case ID_PRED_LINREG:      m_predSettings.linReg      = !m_predSettings.linReg;      break;
    case ID_PRED_EMAPROJ:     m_predSettings.emaProj     = !m_predSettings.emaProj;     break;
    case ID_PRED_MOMENTUM:    m_predSettings.momentum    = !m_predSettings.momentum;    break;
    case ID_PRED_HOLTWINTERS: m_predSettings.holtWinters = !m_predSettings.holtWinters; break;
    }
    updateDropdownLabels();
    refreshPredictors();
}

// ─── Helpers ─────────────────────────────────────────────────────────────────
void MainWindow::setStatus(const std::wstring& msg, int part) {
    if (m_hStatus)
        SendMessageW(m_hStatus, SB_SETTEXTW, part, (LPARAM)msg.c_str());
}

// ─── onDestroy ───────────────────────────────────────────────────────────────
void MainWindow::onDestroy() {
    if (m_bgBrush)    { DeleteObject(m_bgBrush);    m_bgBrush    = nullptr; }
    if (m_panelBrush) { DeleteObject(m_panelBrush); m_panelBrush = nullptr; }
    if (m_editBrush)  { DeleteObject(m_editBrush);  m_editBrush  = nullptr; }
}

// ─── Chart creation (called after window is shown) ───────────────────────────
// We expose this so main.cpp can create the chart after the window is visible
void MainWindow::createChart() {
    RECT rc; GetClientRect(m_hwnd, &rc);
    const int TOOLBAR_H = 66;
    RECT sbRc = {}; GetWindowRect(m_hStatus, &sbRc);
    int sbH = sbRc.bottom - sbRc.top;
    int chartH = std::max(100, (int)(rc.bottom - sbH - TOOLBAR_H - 200));
    m_chart = ChartWindow::create(m_hwnd, 0, TOOLBAR_H, rc.right, chartH);
}
