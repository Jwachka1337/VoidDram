#pragma once

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>
#include <string>
#include "../resources/resource.h"
#include "memory_cleaner.hpp"
#include "timer_resolution.hpp"
#include "task_scheduler.hpp"

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "gdi32.lib")

#define WM_TRAYICON_MSG (WM_APP + 100)
#define TIMER_UI_REFRESH 1001

class TrayWindow {
public:
    HINSTANCE m_hInstance = NULL;
    HWND m_hWnd = NULL;
    NOTIFYICONDATAW m_nid{};
    bool m_trayAdded = false;

    HWND m_hProgressBar = NULL;
    HWND m_hLblRam = NULL;
    HWND m_hLblStats = NULL;
    HWND m_hRadioLow = NULL;
    HWND m_hRadioFull = NULL;
    HWND m_hRadioWorking = NULL;
    HWND m_hChkTimerRes = NULL;
    HWND m_hChkAutostart = NULL;
    HWND m_hBtnClean = NULL;
    HWND m_hBtnMinimize = NULL;
    HWND m_hBtnAbout = NULL;
    HFONT m_hFontUi = NULL;
    HFONT m_hFontBold = NULL;
    HBRUSH m_hBgBrush = NULL;

    ReactiveMemoryCleaner& m_cleaner;
    ScopedTimerResolution& m_timerRes;

    TrayWindow(ReactiveMemoryCleaner& cleaner, ScopedTimerResolution& timerRes);
    ~TrayWindow();

    bool Initialize(HINSTANCE hInstance, int nCmdShow);
    int RunMessageLoop();
    void ShowBalloonNotification(const std::wstring& title, const std::wstring& message);

    static LRESULT CALLBACK WndProcStatic(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK AboutWndProcStatic(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

    void CreateDashboardControls();
    void UpdateUiMetrics();
    void ShowContextMenu();
    void ToggleWindowVisibility();
    void SyncSettingsToUi();
    void ShowAboutDialog();
};
