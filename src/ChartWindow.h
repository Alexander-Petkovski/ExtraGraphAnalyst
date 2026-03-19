#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <objbase.h>
#include <gdiplus.h>
#include <string>
#include <vector>
#include "DataLoader.h"

// ─── Chart display mode ───────────────────────────────────────────────────────
enum class ChartMode { Candlestick, Line };

// ─── Which sub-panels to show below main chart ──────────────────────────────
struct SubPanelFlags {
    bool rsi    = false;
    bool macd   = false;
    bool stoch  = false;
    bool volume = true;
};

// ─── ChartWindow: GDI+ chart area ────────────────────────────────────────────
class ChartWindow {
public:
    static const wchar_t* CLASS_NAME;
    static bool registerClass(HINSTANCE hInst);

    // Create the chart as a child window of parent
    static ChartWindow* create(HWND parent, int x, int y, int w, int h);

    HWND hwnd() const { return m_hwnd; }

    // Push new chart data; triggers repaint
    void setData(const ChartData* data);
    void setMode(ChartMode mode);
    void setIndicatorSettings(const IndicatorSettings& s);
    void setPredictorResults(const std::vector<PredictorResult>& preds);
    void setSubPanels(const SubPanelFlags& f);

    void moveResize(int x, int y, int w, int h);
    void repaint();

private:
    ChartWindow() = default;

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    LRESULT handleMessage(UINT msg, WPARAM wp, LPARAM lp);

    void onCreate();
    void onPaint();
    void onSize(int w, int h);
    void onMouseMove(int x, int y);
    void onMouseWheel(int delta, int x, int y);
    void onLButtonDown(int x, int y);
    void onLButtonUp(int x, int y);
    void onMouseLeave();

    // ─── Rendering helpers ─────────────────────────────────────────────
    void renderAll(Gdiplus::Graphics& g, int w, int h);
    void renderMainChart(Gdiplus::Graphics& g,
                         Gdiplus::RectF     rc,
                         int firstIdx, int lastIdx,
                         double priceMin, double priceMax);
    void renderVolume(Gdiplus::Graphics& g,
                      Gdiplus::RectF     rc,
                      int firstIdx, int lastIdx);
    void renderRSI(Gdiplus::Graphics& g,
                   Gdiplus::RectF     rc,
                   int firstIdx, int lastIdx);
    void renderMACD(Gdiplus::Graphics& g,
                    Gdiplus::RectF     rc,
                    int firstIdx, int lastIdx);
    void renderCrosshair(Gdiplus::Graphics& g, int w, int h);
    void renderPriceAxis(Gdiplus::Graphics& g, Gdiplus::RectF rc,
                         double priceMin, double priceMax);
    void renderTimeAxis(Gdiplus::Graphics& g, Gdiplus::RectF rc,
                        int firstIdx, int lastIdx);

    // ─── Coordinate helpers ────────────────────────────────────────────
    float priceToY(double price, float top, float bottom,
                   double pMin, double pMax) const;
    int   xToIdx(float x, Gdiplus::RectF chartRc,
                 int firstIdx, int lastIdx) const;
    float idxToX(int idx, Gdiplus::RectF chartRc,
                 int firstIdx, int lastIdx) const;

    // ─── State ────────────────────────────────────────────────────────
    HWND             m_hwnd       = nullptr;
    const ChartData* m_data       = nullptr;
    ChartMode        m_mode       = ChartMode::Candlestick;
    IndicatorSettings m_indSettings;
    std::vector<PredictorResult> m_predictors;
    SubPanelFlags    m_subPanels;

    // View state
    int   m_firstVisible = 0;    // index of first visible candle
    int   m_visibleCount = 80;   // how many candles visible

    // Mouse state
    int   m_mouseX = -1, m_mouseY = -1;
    bool  m_dragging    = false;
    int   m_dragStartX  = 0;
    int   m_dragFirstVisible = 0;
    bool  m_mouseInside = false;

    // Cached size
    int   m_width = 0, m_height = 0;

    // GDI+ font
    Gdiplus::Font* m_fontSmall  = nullptr;
    Gdiplus::Font* m_fontTiny   = nullptr;
};
