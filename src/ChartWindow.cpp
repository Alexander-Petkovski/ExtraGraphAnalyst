#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <windowsx.h>
#include <objbase.h>
#include <gdiplus.h>
#include "ChartWindow.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>
#include <iomanip>

using namespace Gdiplus;

// ─── Class name ──────────────────────────────────────────────────────────────
const wchar_t* ChartWindow::CLASS_NAME = L"EGA_Chart";

// ─── registerClass ────────────────────────────────────────────────────────────
bool ChartWindow::registerClass(HINSTANCE hInst) {
    WNDCLASSEXW wc = {};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wc.lpfnWndProc   = WndProc;
    wc.cbWndExtra    = 0;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(nullptr, IDC_CROSS);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = CLASS_NAME;
    return RegisterClassExW(&wc) != 0;
}

// ─── create ──────────────────────────────────────────────────────────────────
ChartWindow* ChartWindow::create(HWND parent, int x, int y, int w, int h) {
    auto* self = new ChartWindow();
    HWND hwnd = CreateWindowExW(
        0, CLASS_NAME, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS,
        x, y, w, h,
        parent, nullptr,
        (HINSTANCE)GetWindowLongPtrW(parent, GWLP_HINSTANCE),
        self
    );
    if (!hwnd) { delete self; return nullptr; }
    return self;
}

// ─── WndProc ─────────────────────────────────────────────────────────────────
LRESULT CALLBACK ChartWindow::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* cs   = reinterpret_cast<CREATESTRUCTW*>(lp);
        auto* self = reinterpret_cast<ChartWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        if (self) self->m_hwnd = hwnd;
        return DefWindowProcW(hwnd, msg, wp, lp);
    }
    auto* self = reinterpret_cast<ChartWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (!self) return DefWindowProcW(hwnd, msg, wp, lp);
    return self->handleMessage(msg, wp, lp);
}

LRESULT ChartWindow::handleMessage(UINT msg, WPARAM wp, LPARAM lp) {
    switch (msg) {
    case WM_CREATE:
        onCreate();
        return 0;
    case WM_SIZE:
        onSize(LOWORD(lp), HIWORD(lp));
        return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(m_hwnd, &ps);
        onPaint();
        EndPaint(m_hwnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1; // handled in WM_PAINT
    case WM_MOUSEMOVE:
        onMouseMove(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_MOUSEWHEEL:
        onMouseWheel(GET_WHEEL_DELTA_WPARAM(wp), GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_LBUTTONDOWN:
        onLButtonDown(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_LBUTTONUP:
        onLButtonUp(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        return 0;
    case WM_MOUSELEAVE:
        onMouseLeave();
        return 0;
    default:
        return DefWindowProcW(m_hwnd, msg, wp, lp);
    }
}

// ─── Event handlers ──────────────────────────────────────────────────────────
void ChartWindow::onCreate() {
    m_fontSmall = new Font(L"Segoe UI", 8.0f);
    m_fontTiny  = new Font(L"Segoe UI", 7.0f);
}

void ChartWindow::onSize(int w, int h) {
    m_width  = w;
    m_height = h;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void ChartWindow::onMouseMove(int x, int y) {
    if (!m_mouseInside) {
        TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, m_hwnd, 0 };
        TrackMouseEvent(&tme);
        m_mouseInside = true;
    }
    if (m_dragging) {
        int delta = (x - m_dragStartX);
        // Figure out candle width
        if (m_data && !m_data->candles.empty() && m_visibleCount > 0) {
            float candleW = (float)m_width / m_visibleCount;
            int   shift   = (int)(-delta / candleW);
            int   newFirst = m_dragFirstVisible + shift;
            int   total    = (int)m_data->candles.size();
            newFirst = std::max(0, std::min(newFirst, total - m_visibleCount));
            m_firstVisible = newFirst;
        }
    }
    m_mouseX = x;
    m_mouseY = y;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void ChartWindow::onMouseWheel(int delta, int /*x*/, int /*y*/) {
    // Zoom in/out
    int step = m_visibleCount / 8;
    if (step < 2) step = 2;
    if (delta > 0) {
        m_visibleCount = std::max(10, m_visibleCount - step);
    } else {
        int total = m_data ? (int)m_data->candles.size() : 200;
        m_visibleCount = std::min(total, m_visibleCount + step);
    }
    // Keep last visible bar in place
    if (m_data) {
        int total = (int)m_data->candles.size();
        m_firstVisible = std::max(0, std::min(m_firstVisible, total - m_visibleCount));
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void ChartWindow::onLButtonDown(int x, int y) {
    SetCapture(m_hwnd);
    m_dragging        = true;
    m_dragStartX      = x;
    m_dragFirstVisible = m_firstVisible;
}

void ChartWindow::onLButtonUp(int /*x*/, int /*y*/) {
    if (m_dragging) { ReleaseCapture(); m_dragging = false; }
}

void ChartWindow::onMouseLeave() {
    m_mouseInside = false;
    m_mouseX = m_mouseY = -1;
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

// ─── Public setters ───────────────────────────────────────────────────────────
void ChartWindow::setData(const ChartData* data) {
    m_data = data;
    if (data && !data->candles.empty()) {
        int total      = (int)data->candles.size();
        m_visibleCount = std::min(total, 80);
        m_firstVisible = std::max(0, total - m_visibleCount);
    }
    InvalidateRect(m_hwnd, nullptr, FALSE);
}

void ChartWindow::setMode(ChartMode mode)                              { m_mode = mode; InvalidateRect(m_hwnd, nullptr, FALSE); }
void ChartWindow::setIndicatorSettings(const IndicatorSettings& s)    { m_indSettings = s; InvalidateRect(m_hwnd, nullptr, FALSE); }
void ChartWindow::setPredictorResults(const std::vector<PredictorResult>& p) { m_predictors = p; InvalidateRect(m_hwnd, nullptr, FALSE); }
void ChartWindow::setSubPanels(const SubPanelFlags& f)                { m_subPanels = f; InvalidateRect(m_hwnd, nullptr, FALSE); }
void ChartWindow::repaint()                                            { InvalidateRect(m_hwnd, nullptr, FALSE); }

void ChartWindow::moveResize(int x, int y, int w, int h) {
    MoveWindow(m_hwnd, x, y, w, h, TRUE);
}

// ─── Coordinate helpers ───────────────────────────────────────────────────────
float ChartWindow::priceToY(double price, float top, float bottom,
                             double pMin, double pMax) const {
    if (pMax <= pMin) return (top + bottom) * 0.5f;
    double range = pMax - pMin;
    return bottom - (float)((price - pMin) / range * (bottom - top));
}

float ChartWindow::idxToX(int idx, RectF chartRc, int firstIdx, int lastIdx) const {
    int count  = lastIdx - firstIdx;
    if (count <= 0) return chartRc.X;
    float step = chartRc.Width / count;
    return chartRc.X + (idx - firstIdx) * step + step * 0.5f;
}

int ChartWindow::xToIdx(float x, RectF chartRc, int firstIdx, int lastIdx) const {
    int count = lastIdx - firstIdx;
    if (count <= 0) return firstIdx;
    float step = chartRc.Width / count;
    int idx = firstIdx + (int)((x - chartRc.X) / step);
    return std::max(firstIdx, std::min(idx, lastIdx - 1));
}

// ─── onPaint: double-buffered ─────────────────────────────────────────────────
void ChartWindow::onPaint() {
    int w = m_width, h = m_height;
    if (w <= 0 || h <= 0) return;

    // Back-buffer bitmap
    Bitmap   bmp(w, h, PixelFormat32bppARGB);
    Graphics g(&bmp);
    g.SetSmoothingMode(SmoothingModeAntiAlias);
    g.SetTextRenderingHint(TextRenderingHintClearTypeGridFit);

    renderAll(g, w, h);

    // Blit to screen
    HDC hdc = GetDC(m_hwnd);
    Graphics screen(hdc);
    screen.DrawImage(&bmp, 0, 0);
    ReleaseDC(m_hwnd, hdc);
}

// ─── Colors ───────────────────────────────────────────────────────────────────
static const Color CLR_BG         (255, 13,  17,  23);
static const Color CLR_GRID       (255, 35,  40,  50);
static const Color CLR_TEXT       (255, 178, 190, 205);
static const Color CLR_TEXT_DIM   (255,  90, 100, 115);
static const Color CLR_BULL       (255,  38, 166, 154);  // teal
static const Color CLR_BEAR       (255, 239,  83,  80);  // red
static const Color CLR_VOLUME     (255,  40,  60,  90);
static const Color CLR_CROSSHAIR  (255, 120, 130, 150);
static const Color CLR_SMA20      (255, 255, 200,   0);
static const Color CLR_SMA50      (255, 255, 130,   0);
static const Color CLR_SMA200     (255, 255,  70,  70);
static const Color CLR_EMA12      (255, 100, 200, 255);
static const Color CLR_EMA26      (255,  60, 140, 255);
static const Color CLR_BB         (255, 100, 150, 255);
static const Color CLR_VWAP       (255, 255, 120, 200);
static const Color CLR_ICHI_T     (255,  64, 224, 208);
static const Color CLR_ICHI_K     (255, 255,  69,   0);
static const Color CLR_PRED_DEF   (255, 150, 150, 200);
static const Color CLR_PRED_BEST  (255,  50, 220, 100);
static const Color CLR_RSI_LINE   (255, 120, 200, 255);
static const Color CLR_MACD_LINE  (255, 100, 200, 255);
static const Color CLR_MACD_SIG   (255, 255, 160,  80);
static const Color CLR_STOCH_K    (255, 100, 200, 255);
static const Color CLR_STOCH_D    (255, 255, 130,  80);

// ─── renderAll ────────────────────────────────────────────────────────────────
void ChartWindow::renderAll(Graphics& g, int w, int h) {
    // Background
    SolidBrush bgBrush(CLR_BG);
    g.FillRectangle(&bgBrush, 0, 0, w, h);

    if (!m_data || !m_data->loaded || m_data->candles.empty()) {
        // Placeholder text
        Font        font(L"Segoe UI", 14.0f);
        SolidBrush  tb(CLR_TEXT_DIM);
        StringFormat sf;
        sf.SetAlignment(StringAlignmentCenter);
        sf.SetLineAlignment(StringAlignmentCenter);
        RectF rc(0.0f, 0.0f, (float)w, (float)h);
        g.DrawString(L"Open a CSV or Excel file to begin  (File > Open)", -1, &font, rc, &sf, &tb);
        return;
    }

    const auto& candles = m_data->candles;
    int total     = (int)candles.size();
    int firstIdx  = std::max(0, std::min(m_firstVisible, total - 1));
    int lastIdx   = std::min(total, firstIdx + m_visibleCount);

    // ─── Layout: how much space for sub-panels
    const int PRICE_AXIS_W = 70;
    const int TIME_AXIS_H  = 22;
    float subH = 0;
    if (m_subPanels.volume) subH += 60;
    if (m_subPanels.rsi  && !m_data->indicators.rsi14.empty())  subH += 80;
    if (m_subPanels.macd && !m_data->indicators.macdLine.empty()) subH += 80;

    float mainH = h - TIME_AXIS_H - subH;
    if (mainH < 80) mainH = 80;

    RectF mainRc(0.0f, 0.0f, (float)(w - PRICE_AXIS_W), mainH);
    RectF priceAxisRc((float)(w - PRICE_AXIS_W), 0.0f, (float)PRICE_AXIS_W, mainH);
    RectF timeAxisRc(0.0f, mainH + subH, (float)(w - PRICE_AXIS_W), (float)TIME_AXIS_H);

    // ─── Find price range
    double pMin =  1e18, pMax = -1e18;
    for (int i = firstIdx; i < lastIdx; ++i) {
        pMin = std::min(pMin, candles[i].low);
        pMax = std::max(pMax, candles[i].high);
    }
    // Extend for overlay indicators
    auto extendRange = [&](const std::vector<double>& v) {
        for (int i = firstIdx; i < lastIdx && i < (int)v.size(); ++i) {
            if (!std::isnan(v[i]) && std::isfinite(v[i])) {
                pMin = std::min(pMin, v[i]);
                pMax = std::max(pMax, v[i]);
            }
        }
    };
    if (m_indSettings.sma20)  extendRange(m_data->indicators.sma20);
    if (m_indSettings.sma50)  extendRange(m_data->indicators.sma50);
    if (m_indSettings.sma200) extendRange(m_data->indicators.sma200);
    if (m_indSettings.ema12)  extendRange(m_data->indicators.ema12);
    if (m_indSettings.ema26)  extendRange(m_data->indicators.ema26);
    if (m_indSettings.bb) {
        extendRange(m_data->indicators.bbUpper);
        extendRange(m_data->indicators.bbLower);
    }
    double padding = (pMax - pMin) * 0.05;
    pMin -= padding;  pMax += padding;
    if (pMax <= pMin) { pMin -= 1; pMax += 1; }

    // ─── Draw grid
    {
        Pen gridPen(CLR_GRID, 1.0f);
        int gridLines = 5;
        for (int i = 1; i < gridLines; ++i) {
            float y = mainRc.Y + mainRc.Height * i / gridLines;
            g.DrawLine(&gridPen, mainRc.X, y, mainRc.X + mainRc.Width, y);
        }
        // vertical grid every ~50px
        float step = 50.0f;
        for (float x = mainRc.X + step; x < mainRc.X + mainRc.Width; x += step) {
            g.DrawLine(&gridPen, x, mainRc.Y, x, mainRc.Y + mainRc.Height);
        }
    }

    // ─── Render price axis
    renderPriceAxis(g, priceAxisRc, pMin, pMax);

    // ─── Render time axis
    renderTimeAxis(g, timeAxisRc, firstIdx, lastIdx);

    // ─── Sub-panels
    float subY = mainH;
    if (m_subPanels.volume) {
        RectF volRc(0.0f, subY, (float)(w - PRICE_AXIS_W), 60.0f);
        renderVolume(g, volRc, firstIdx, lastIdx);
        subY += 60;
    }
    if (m_subPanels.rsi && !m_data->indicators.rsi14.empty()) {
        RectF rsiRc(0.0f, subY, (float)(w - PRICE_AXIS_W), 80.0f);
        renderRSI(g, rsiRc, firstIdx, lastIdx);
        subY += 80;
    }
    if (m_subPanels.macd && !m_data->indicators.macdLine.empty()) {
        RectF macdRc(0.0f, subY, (float)(w - PRICE_AXIS_W), 80.0f);
        renderMACD(g, macdRc, firstIdx, lastIdx);
        subY += 80;
    }

    // ─── Main chart (candlesticks / line + overlays)
    renderMainChart(g, mainRc, firstIdx, lastIdx, pMin, pMax);

    // ─── Crosshair
    if (m_mouseInside)
        renderCrosshair(g, w, h);
}

// ─── renderMainChart ─────────────────────────────────────────────────────────
void ChartWindow::renderMainChart(Graphics& g, RectF rc,
                                   int firstIdx, int lastIdx,
                                   double pMin, double pMax) {
    const auto& candles = m_data->candles;
    int count = lastIdx - firstIdx;
    if (count <= 0) return;

    float step     = rc.Width / count;
    float candleW  = std::max(1.0f, step * 0.7f);
    float halfBody = candleW * 0.5f;

    SolidBrush bullBrush(CLR_BULL);
    SolidBrush bearBrush(CLR_BEAR);
    Pen        bullPen(CLR_BULL, 1.0f);
    Pen        bearPen(CLR_BEAR, 1.0f);

    if (m_mode == ChartMode::Line) {
        // Line chart
        Pen linePen(CLR_EMA12, 1.5f);
        std::vector<PointF> pts;
        for (int i = firstIdx; i < lastIdx; ++i) {
            float x = rc.X + (i - firstIdx) * step + step * 0.5f;
            float y = priceToY(candles[i].close, rc.Y, rc.Y + rc.Height, pMin, pMax);
            pts.push_back({ x, y });
        }
        if (pts.size() >= 2)
            g.DrawLines(&linePen, pts.data(), (int)pts.size());
    } else {
        // Candlestick
        for (int i = firstIdx; i < lastIdx; ++i) {
            const auto& c = candles[i];
            float cx   = rc.X + (i - firstIdx) * step + step * 0.5f;
            float yO   = priceToY(c.open,  rc.Y, rc.Y + rc.Height, pMin, pMax);
            float yC   = priceToY(c.close, rc.Y, rc.Y + rc.Height, pMin, pMax);
            float yH   = priceToY(c.high,  rc.Y, rc.Y + rc.Height, pMin, pMax);
            float yL   = priceToY(c.low,   rc.Y, rc.Y + rc.Height, pMin, pMax);
            bool  bull = (c.close >= c.open);

            Pen&        wick  = bull ? bullPen : bearPen;
            SolidBrush& body  = bull ? bullBrush : bearBrush;

            // Wick
            g.DrawLine(&wick, cx, yH, cx, yL);

            // Body
            float bodyTop = std::min(yO, yC);
            float bodyH   = std::max(1.0f, std::abs(yO - yC));
            if (step > 4)
                g.FillRectangle(&body, cx - halfBody, bodyTop, candleW, bodyH);
            else
                g.DrawLine(&wick, cx, bodyTop, cx, bodyTop + bodyH);
        }
    }

    // ─── Overlay indicators
    auto drawLine = [&](const std::vector<double>& arr, const Color& col, float lw = 1.0f) {
        if (arr.empty()) return;
        Pen pen(col, lw);
        PointF prev;
        bool hasPrev = false;
        for (int i = firstIdx; i < lastIdx && i < (int)arr.size(); ++i) {
            if (std::isnan(arr[i]) || !std::isfinite(arr[i])) { hasPrev = false; continue; }
            float x = rc.X + (i - firstIdx) * step + step * 0.5f;
            float y = priceToY(arr[i], rc.Y, rc.Y + rc.Height, pMin, pMax);
            if (hasPrev) g.DrawLine(&pen, prev, PointF(x, y));
            prev = { x, y }; hasPrev = true;
        }
    };

    const auto& ind = m_data->indicators;
    if (m_indSettings.sma20)  drawLine(ind.sma20,    CLR_SMA20,  1.2f);
    if (m_indSettings.sma50)  drawLine(ind.sma50,    CLR_SMA50,  1.2f);
    if (m_indSettings.sma200) drawLine(ind.sma200,   CLR_SMA200, 1.5f);
    if (m_indSettings.ema12)  drawLine(ind.ema12,    CLR_EMA12,  1.0f);
    if (m_indSettings.ema26)  drawLine(ind.ema26,    CLR_EMA26,  1.0f);
    if (m_indSettings.vwap)   drawLine(ind.vwap,     CLR_VWAP,   1.2f);

    // Bollinger Bands
    if (m_indSettings.bb && !ind.bbMiddle.empty()) {
        drawLine(ind.bbUpper,  CLR_BB, 1.0f);
        drawLine(ind.bbMiddle, CLR_BB, 1.0f);
        drawLine(ind.bbLower,  CLR_BB, 1.0f);
        // Shaded fill
        SolidBrush fillBrush(Color(25, 100, 150, 255));
        for (int i = firstIdx + 1; i < lastIdx &&
             i < (int)ind.bbUpper.size() && i < (int)ind.bbLower.size(); ++i) {
            if (std::isnan(ind.bbUpper[i]) || std::isnan(ind.bbLower[i])) continue;
            float x1 = rc.X + (i - 1 - firstIdx) * step + step * 0.5f;
            float x2 = rc.X + (i     - firstIdx) * step + step * 0.5f;
            float yU1 = priceToY(ind.bbUpper[i-1], rc.Y, rc.Y + rc.Height, pMin, pMax);
            float yU2 = priceToY(ind.bbUpper[i],   rc.Y, rc.Y + rc.Height, pMin, pMax);
            float yL1 = priceToY(ind.bbLower[i-1], rc.Y, rc.Y + rc.Height, pMin, pMax);
            float yL2 = priceToY(ind.bbLower[i],   rc.Y, rc.Y + rc.Height, pMin, pMax);
            PointF pts[] = { {x1,yU1},{x2,yU2},{x2,yL2},{x1,yL1} };
            g.FillPolygon(&fillBrush, pts, 4);
        }
    }

    // Ichimoku
    if (m_indSettings.ichimoku) {
        drawLine(ind.ichimokuTenkan, CLR_ICHI_T, 1.0f);
        drawLine(ind.ichimokuKijun,  CLR_ICHI_K, 1.0f);
    }

    // Fibonacci levels (horizontal lines across chart)
    if (m_indSettings.fibo) {
        Pen fiboPen(Color(150, 200, 170, 80), 1.0f);
        fiboPen.SetDashStyle(DashStyleDash);
        SolidBrush fiboTxt(Color(200, 170, 80));
        Font fiboFont(L"Segoe UI", 7.0f);
        for (double lvl : ind.fiboLevels) {
            if (lvl < pMin || lvl > pMax) continue;
            float y = priceToY(lvl, rc.Y, rc.Y + rc.Height, pMin, pMax);
            g.DrawLine(&fiboPen, rc.X, y, rc.X + rc.Width, y);
            std::wstringstream ws; ws << std::fixed << std::setprecision(2) << lvl;
            g.DrawString(ws.str().c_str(), -1, m_fontTiny,
                         PointF(rc.X + 2, y - 10), &fiboTxt);
        }
    }

    // S/R levels
    if (m_indSettings.sr) {
        Pen srPen(Color(120, 255, 255, 100), 1.0f);
        srPen.SetDashStyle(DashStyleDot);
        for (double lvl : ind.srLevels) {
            if (lvl < pMin || lvl > pMax) continue;
            float y = priceToY(lvl, rc.Y, rc.Y + rc.Height, pMin, pMax);
            g.DrawLine(&srPen, rc.X, y, rc.X + rc.Width, y);
        }
    }

    // ─── Predictor forecast (draws from last candle forward)
    for (const auto& pred : m_predictors) {
        if (pred.forecast.empty()) continue;
        Color col = pred.recommended ? CLR_PRED_BEST : CLR_PRED_DEF;
        Pen pen(col, pred.recommended ? 2.0f : 1.0f);
        if (pred.recommended) pen.SetDashStyle(DashStyleSolid);
        else pen.SetDashStyle(DashStyleDash);

        float lastX = rc.X + (lastIdx - 1 - firstIdx) * step + step * 0.5f;
        float lastY = priceToY(candles[lastIdx - 1].close, rc.Y, rc.Y + rc.Height, pMin, pMax);
        PointF prevPt(lastX, lastY);

        for (int j = 0; j < (int)pred.forecast.size(); ++j) {
            float x = lastX + (j + 1) * step;
            if (x > rc.X + rc.Width) break;
            float y = priceToY(pred.forecast[j], rc.Y, rc.Y + rc.Height, pMin, pMax);
            g.DrawLine(&pen, prevPt, PointF(x, y));
            prevPt = { x, y };
        }
        // Label
        if (!pred.forecast.empty()) {
            SolidBrush lb(col);
            g.DrawString(pred.name.c_str(), -1, m_fontTiny, prevPt, &lb);
        }
    }
}

// ─── renderVolume ─────────────────────────────────────────────────────────────
void ChartWindow::renderVolume(Graphics& g, RectF rc, int firstIdx, int lastIdx) {
    const auto& candles = m_data->candles;
    int count = lastIdx - firstIdx;
    if (count <= 0) return;

    // Background
    SolidBrush bg(Color(255, 16, 20, 28));
    g.FillRectangle(&bg, rc);

    // Divider
    Pen divPen(CLR_GRID, 1.0f);
    g.DrawLine(&divPen, rc.X, rc.Y, rc.X + rc.Width, rc.Y);

    // Label
    SolidBrush lbl(CLR_TEXT_DIM);
    g.DrawString(L"VOL", -1, m_fontTiny, PointF(rc.X + 2, rc.Y + 2), &lbl);

    double maxVol = 1;
    for (int i = firstIdx; i < lastIdx; ++i)
        maxVol = std::max(maxVol, candles[i].volume);

    float step = rc.Width / count;
    float barW = std::max(1.0f, step * 0.7f);

    for (int i = firstIdx; i < lastIdx; ++i) {
        double vol = candles[i].volume;
        if (vol <= 0) continue;
        float barH = (float)(vol / maxVol) * (rc.Height - 4);
        float x    = rc.X + (i - firstIdx) * step;
        float y    = rc.Y + rc.Height - barH;
        bool  bull = (candles[i].close >= candles[i].open);
        Color col  = bull ? Color(100, 38, 166, 154) : Color(100, 239, 83, 80);
        SolidBrush br(col);
        g.FillRectangle(&br, x, y, barW, barH);
    }
}

// ─── renderRSI ────────────────────────────────────────────────────────────────
void ChartWindow::renderRSI(Graphics& g, RectF rc, int firstIdx, int lastIdx) {
    const auto& rsi = m_data->indicators.rsi14;
    if (rsi.empty()) return;

    SolidBrush bg(Color(255, 16, 20, 28));
    g.FillRectangle(&bg, rc);
    Pen divPen(CLR_GRID, 1.0f);
    g.DrawLine(&divPen, rc.X, rc.Y, rc.X + rc.Width, rc.Y);

    SolidBrush lbl(CLR_TEXT_DIM);
    g.DrawString(L"RSI(14)", -1, m_fontTiny, PointF(rc.X + 2, rc.Y + 2), &lbl);

    // 70/30 reference lines
    Pen refPen(Color(60, 255, 255, 255), 1.0f);
    refPen.SetDashStyle(DashStyleDot);
    float y70 = rc.Y + rc.Height * (1.0f - 70.0f / 100.0f);
    float y30 = rc.Y + rc.Height * (1.0f - 30.0f / 100.0f);
    g.DrawLine(&refPen, rc.X, y70, rc.X + rc.Width, y70);
    g.DrawLine(&refPen, rc.X, y30, rc.X + rc.Width, y30);

    int count = lastIdx - firstIdx;
    float step = rc.Width / count;

    Pen rsiPen(CLR_RSI_LINE, 1.2f);
    PointF prev; bool hasPrev = false;
    for (int i = firstIdx; i < lastIdx && i < (int)rsi.size(); ++i) {
        if (std::isnan(rsi[i])) { hasPrev = false; continue; }
        float x = rc.X + (i - firstIdx) * step + step * 0.5f;
        float y = rc.Y + rc.Height * (1.0f - (float)rsi[i] / 100.0f);
        if (hasPrev) g.DrawLine(&rsiPen, prev, PointF(x, y));
        prev = { x, y }; hasPrev = true;
    }
}

// ─── renderMACD ───────────────────────────────────────────────────────────────
void ChartWindow::renderMACD(Graphics& g, RectF rc, int firstIdx, int lastIdx) {
    const auto& macd = m_data->indicators.macdLine;
    const auto& sig  = m_data->indicators.signalLine;
    const auto& hist = m_data->indicators.macdHist;
    if (macd.empty()) return;

    SolidBrush bg(Color(255, 16, 20, 28));
    g.FillRectangle(&bg, rc);
    Pen divPen(CLR_GRID, 1.0f);
    g.DrawLine(&divPen, rc.X, rc.Y, rc.X + rc.Width, rc.Y);

    SolidBrush lbl(CLR_TEXT_DIM);
    g.DrawString(L"MACD(12,26,9)", -1, m_fontTiny, PointF(rc.X + 2, rc.Y + 2), &lbl);

    // Find range
    double vMin = 0, vMax = 0;
    for (int i = firstIdx; i < lastIdx && i < (int)hist.size(); ++i)
        if (!std::isnan(hist[i])) { vMin = std::min(vMin, hist[i]); vMax = std::max(vMax, hist[i]); }
    for (int i = firstIdx; i < lastIdx && i < (int)macd.size(); ++i)
        if (!std::isnan(macd[i])) { vMin = std::min(vMin, macd[i]); vMax = std::max(vMax, macd[i]); }
    double range = std::max(vMax - vMin, 0.001);
    float midY = rc.Y + rc.Height * (float)(vMax / range);

    Pen zeroPen(CLR_GRID, 1.0f);
    g.DrawLine(&zeroPen, rc.X, midY, rc.X + rc.Width, midY);

    int count = lastIdx - firstIdx;
    float step = rc.Width / count;
    float barW = std::max(1.0f, step * 0.7f);

    // Histogram
    for (int i = firstIdx; i < lastIdx && i < (int)hist.size(); ++i) {
        if (std::isnan(hist[i])) continue;
        float x    = rc.X + (i - firstIdx) * step;
        float y    = rc.Y + rc.Height * (float)((vMax - hist[i]) / range);
        float barH = std::abs(y - midY);
        Color col  = hist[i] >= 0 ? Color(120, 38, 166, 154) : Color(120, 239, 83, 80);
        SolidBrush br(col);
        float top  = std::min(y, midY);
        g.FillRectangle(&br, x, top, barW, barH);
    }

    // MACD & signal lines
    Pen macdPen(CLR_MACD_LINE, 1.2f);
    Pen sigPen(CLR_MACD_SIG, 1.0f);
    PointF prevM, prevS; bool hM = false, hS = false;
    for (int i = firstIdx; i < lastIdx; ++i) {
        float x = rc.X + (i - firstIdx) * step + step * 0.5f;
        if (i < (int)macd.size() && !std::isnan(macd[i])) {
            float y = rc.Y + rc.Height * (float)((vMax - macd[i]) / range);
            if (hM) g.DrawLine(&macdPen, prevM, PointF(x, y));
            prevM = { x, y }; hM = true;
        }
        if (i < (int)sig.size() && !std::isnan(sig[i])) {
            float y = rc.Y + rc.Height * (float)((vMax - sig[i]) / range);
            if (hS) g.DrawLine(&sigPen, prevS, PointF(x, y));
            prevS = { x, y }; hS = true;
        }
    }
}

// ─── renderPriceAxis ─────────────────────────────────────────────────────────
void ChartWindow::renderPriceAxis(Graphics& g, RectF rc, double pMin, double pMax) {
    SolidBrush bg(Color(255, 16, 20, 28));
    g.FillRectangle(&bg, rc);
    Pen axPen(CLR_GRID, 1.0f);
    g.DrawLine(&axPen, rc.X, rc.Y, rc.X, rc.Y + rc.Height);

    SolidBrush txt(CLR_TEXT);
    int lines = 6;
    for (int i = 0; i <= lines; ++i) {
        float t = (float)i / lines;
        double price = pMax - t * (pMax - pMin);
        float  y     = rc.Y + t * rc.Height;
        std::wstringstream ws;
        ws << std::fixed << std::setprecision(2) << price;
        g.DrawString(ws.str().c_str(), -1, m_fontTiny, PointF(rc.X + 3, y - 8), &txt);
    }
}

// ─── renderTimeAxis ──────────────────────────────────────────────────────────
void ChartWindow::renderTimeAxis(Graphics& g, RectF rc, int firstIdx, int lastIdx) {
    if (!m_data || m_data->candles.empty()) return;

    SolidBrush bg(Color(255, 16, 20, 28));
    g.FillRectangle(&bg, rc);
    Pen axPen(CLR_GRID, 1.0f);
    g.DrawLine(&axPen, rc.X, rc.Y, rc.X + rc.Width, rc.Y);

    SolidBrush txt(CLR_TEXT_DIM);
    int count = lastIdx - firstIdx;
    if (count <= 0) return;
    float step  = rc.Width / count;
    int   every = std::max(1, count / 8);

    for (int i = firstIdx; i < lastIdx; i += every) {
        float x = rc.X + (i - firstIdx) * step;
        const auto& lbl = m_data->candles[i].label;
        // Trim label to ~10 chars
        std::wstring s = lbl.size() > 10 ? lbl.substr(0, 10) : lbl;
        g.DrawString(s.c_str(), -1, m_fontTiny, PointF(x, rc.Y + 4), &txt);
    }
}

// ─── renderCrosshair ─────────────────────────────────────────────────────────
void ChartWindow::renderCrosshair(Graphics& g, int w, int h) {
    if (m_mouseX < 0 || m_mouseY < 0) return;
    Pen pen(CLR_CROSSHAIR, 1.0f);
    pen.SetDashStyle(DashStyleDash);
    g.DrawLine(&pen, (float)m_mouseX, 0.0f, (float)m_mouseX, (float)h);
    g.DrawLine(&pen, 0.0f, (float)m_mouseY, (float)w, (float)m_mouseY);

    if (!m_data || !m_data->loaded) return;

    // Small cross at intersection
    SolidBrush dot(CLR_CROSSHAIR);
    g.FillRectangle(&dot, (float)m_mouseX - 2, (float)m_mouseY - 2, 4.0f, 4.0f);

    // Date label at bottom of crosshair
    const int PRICE_AXIS_W = 70;
    const int TIME_AXIS_H  = 22;
    int count     = m_visibleCount;
    int firstIdx  = m_firstVisible;
    float chartW  = (float)(w - PRICE_AXIS_W);
    if (count > 0 && chartW > 0) {
        float step   = chartW / count;
        int   idx    = firstIdx + (int)((m_mouseX) / step);
        idx = std::max(firstIdx, std::min(idx, (int)m_data->candles.size() - 1));
        const std::wstring& lbl = m_data->candles[idx].label;

        // Draw label box at bottom
        RectF lblBg((float)m_mouseX - 40, (float)(h - TIME_AXIS_H - 2), 80, 16);
        SolidBrush bg(Color(220, 30, 35, 45));
        g.FillRectangle(&bg, lblBg);
        SolidBrush ft(CLR_TEXT);
        StringFormat sf;
        sf.SetAlignment(StringAlignmentCenter);
        g.DrawString(lbl.c_str(), -1, m_fontTiny, lblBg, &sf, &ft);
    }
}
