#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <commctrl.h>
#include <objbase.h>
#include <gdiplus.h>
#include "MainWindow.h"
#include "ChartWindow.h"
#include "PythonBridge.h"

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow) {
    // ── Common controls (status bar, etc.) ────────────────────────────
    INITCOMMONCONTROLSEX icc{};
    icc.dwSize = sizeof(icc);
    icc.dwICC  = ICC_WIN95_CLASSES | ICC_STANDARD_CLASSES | ICC_BAR_CLASSES;
    InitCommonControlsEx(&icc);

    // ── GDI+ startup ──────────────────────────────────────────────────
    Gdiplus::GdiplusStartupInput gdipInput;
    ULONG_PTR gdipToken;
    Gdiplus::GdiplusStartup(&gdipToken, &gdipInput, nullptr);

    // ── Python startup ────────────────────────────────────────────────
    PythonBridge::instance().init();

    // ── Register window classes ───────────────────────────────────────
    if (!MainWindow::registerClass(hInst)) {
        MessageBoxW(nullptr, L"Failed to register main window class.",
                    L"Startup Error", MB_ICONERROR);
        return 1;
    }
    if (!ChartWindow::registerClass(hInst)) {
        MessageBoxW(nullptr, L"Failed to register chart window class.",
                    L"Startup Error", MB_ICONERROR);
        return 1;
    }

    // ── Create & show main window ─────────────────────────────────────
    MainWindow* win = MainWindow::create(hInst);
    if (!win) {
        MessageBoxW(nullptr, L"Failed to create main window.",
                    L"Startup Error", MB_ICONERROR);
        return 1;
    }

    win->show(nCmdShow);
    win->createChart();

    // ── Message loop ──────────────────────────────────────────────────
    MSG msg{};
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }

    // ── Cleanup ───────────────────────────────────────────────────────
    PythonBridge::instance().shutdown();
    Gdiplus::GdiplusShutdown(gdipToken);

    return (int)msg.wParam;
}
