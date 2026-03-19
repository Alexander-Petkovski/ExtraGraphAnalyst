#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include "DataLoader.h"
#include "ChartWindow.h"

// ─── Control IDs ─────────────────────────────────────────────────────────────
enum {
    ID_FILE_OPEN        = 100,
    ID_FILE_EXIT        = 101,

    ID_CHART_CANDLE     = 110,
    ID_CHART_LINE       = 111,

    ID_TF_COMBO         = 120,

    // Indicator menu IDs (used by both menus and settings)
    ID_IND_SMA20        = 200,
    ID_IND_SMA50        = 201,
    ID_IND_SMA200       = 202,
    ID_IND_EMA12        = 203,
    ID_IND_EMA26        = 204,
    ID_IND_WMA20        = 205,
    ID_IND_BB           = 206,
    ID_IND_RSI          = 207,
    ID_IND_MACD         = 208,
    ID_IND_STOCH        = 209,
    ID_IND_ATR          = 210,
    ID_IND_VWAP         = 211,
    ID_IND_FIBO         = 212,
    ID_IND_SR           = 213,
    ID_IND_ICHIMOKU     = 214,
    ID_IND_VOLUME       = 215,

    // Predictor menu IDs
    ID_PRED_LINREG      = 300,
    ID_PRED_EMAPROJ     = 301,
    ID_PRED_MOMENTUM    = 302,
    ID_PRED_HOLTWINTERS = 303,

    // Python console
    ID_CONSOLE_EDIT     = 400,
    ID_CONSOLE_RUN      = 401,
    ID_CONSOLE_CLEAR    = 402,
    ID_CONSOLE_OUT      = 403,

    // Scripts
    ID_SCRIPT_COMBO     = 500,
    ID_SCRIPT_RUN       = 501,
    ID_SCRIPT_REFRESH   = 502,

    // Status bar
    ID_STATUSBAR        = 600,

    // Yahoo Finance
    ID_TICKER_EDIT      = 700,
    ID_TICKER_FETCH     = 701,
    ID_PERIOD_COMBO     = 702,
    ID_TICKER_INFO      = 703,

    // Dropdown category buttons (open popup menus)
    ID_BTN_MOVAVGS      = 800,   // Moving Averages menu
    ID_BTN_OSCILLATORS  = 801,   // Oscillators menu
    ID_BTN_BANDS        = 802,   // Bands & Levels menu
    ID_BTN_PREDICTORS   = 803,   // Predictors menu
};

// ─── MainWindow ──────────────────────────────────────────────────────────────
class MainWindow {
public:
    static const wchar_t* CLASS_NAME;
    static bool        registerClass(HINSTANCE hInst);
    static MainWindow* create(HINSTANCE hInst);

    HWND hwnd() const { return m_hwnd; }
    void show(int nCmdShow);
    void createChart();   // call after show()

private:
    MainWindow(HINSTANCE hInst) : m_hInst(hInst) {}

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT handleMessage(UINT msg, WPARAM wp, LPARAM lp);

    void onCreate();
    void onSize(int w, int h);
    void onCommand(int id, int code, HWND hCtrl);
    void onDestroy();
    LRESULT onCtlColor(HDC hdc, HWND hCtrl, UINT msg);

    // ─── Actions ──────────────────────────────────────────────────────
    void openFile();
    void fetchFromYahoo();
    void refreshIndicators();
    void refreshPredictors();
    void runConsole();
    void clearConsole();
    void runScript();
    void refreshScripts();
    void setStatus(const std::wstring& msg, int part = 0);
    void appendConsoleOut(const std::wstring& text);

    // ─── Dropdown menu helpers ─────────────────────────────────────────
    void showMovingAvgsMenu(HWND btn);
    void showOscillatorsMenu(HWND btn);
    void showBandsMenu(HWND btn);
    void showPredictorsMenu(HWND btn);
    void updateDropdownLabels();          // refresh button text with active counts
    void toggleIndicator(int id);         // flip one indicator setting + refresh
    void togglePredictor(int id);         // flip one predictor setting + refresh

    // ─── Helpers ──────────────────────────────────────────────────────
    bool indicatorActive(int id) const;   // read from m_indSettings
    bool predictorActive(int id) const;   // read from m_predSettings
    static void appendMenuItem(HMENU m, int id, const wchar_t* lbl, bool checked);
    static void showMenuBelowBtn(HMENU m, HWND btn, HWND owner);

    // ─── Win32 handles ────────────────────────────────────────────────
    HINSTANCE    m_hInst        = nullptr;
    HWND         m_hwnd         = nullptr;
    HWND         m_hStatus      = nullptr;

    // Toolbar row 1: file / view / Yahoo
    HWND         m_btnOpen      = nullptr;
    HWND         m_btnCandle    = nullptr;
    HWND         m_btnLine      = nullptr;
    HWND         m_cmbTF        = nullptr;
    HWND         m_lblTF        = nullptr;
    HWND         m_edtTicker    = nullptr;
    HWND         m_btnFetch     = nullptr;
    HWND         m_cmbPeriod    = nullptr;
    HWND         m_lblTicker    = nullptr;
    HWND         m_lblPeriod    = nullptr;

    // Toolbar row 2: category dropdown buttons + scripts
    HWND         m_btnMovAvgs   = nullptr;
    HWND         m_btnOscills   = nullptr;
    HWND         m_btnBands     = nullptr;
    HWND         m_btnPredMenu  = nullptr;
    HWND         m_lblScripts   = nullptr;
    HWND         m_cmbScripts   = nullptr;
    HWND         m_btnRunScript = nullptr;
    HWND         m_btnRefresh   = nullptr;

    // Python console
    HWND         m_edtConsoleIn  = nullptr;
    HWND         m_edtConsoleOut = nullptr;
    HWND         m_btnRun        = nullptr;
    HWND         m_btnClear      = nullptr;
    HWND         m_lblConsole    = nullptr;

    // Chart
    ChartWindow* m_chart         = nullptr;

    // GDI brushes
    HBRUSH       m_bgBrush       = nullptr;
    HBRUSH       m_panelBrush    = nullptr;
    HBRUSH       m_editBrush     = nullptr;

    // State
    ChartData         m_chartData;
    IndicatorSettings m_indSettings;
    PredictorSettings m_predSettings;

    static const COLORREF CLR_BG     = RGB(13,  17,  23);
    static const COLORREF CLR_PANEL  = RGB(22,  27,  34);
    static const COLORREF CLR_EDIT   = RGB(30,  36,  44);
    static const COLORREF CLR_TEXT   = RGB(201, 209, 217);
    static const COLORREF CLR_ACCENT = RGB( 88, 166, 255);
};
