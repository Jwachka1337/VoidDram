#include "../include/tray_window.hpp"
#include <format>

TrayWindow::TrayWindow(ReactiveMemoryCleaner& cleaner, ScopedTimerResolution& timerRes)
    : m_cleaner(cleaner), m_timerRes(timerRes) {
    m_hBgBrush = GetSysColorBrush(COLOR_BTNFACE);
}

TrayWindow::~TrayWindow() {
    if (m_trayAdded) {
        Shell_NotifyIconW(NIM_DELETE, &m_nid);
        m_trayAdded = false;
    }

    if (m_hFontUi) DeleteObject(m_hFontUi);
    if (m_hFontBold) DeleteObject(m_hFontBold);
}

bool TrayWindow::Initialize(HINSTANCE hInstance, int nCmdShow) {
    m_hInstance = hInstance;

    INITCOMMONCONTROLSEX icex{};
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_PROGRESS_CLASS | ICC_STANDARD_CLASSES;
    InitCommonControlsEx(&icex);

    const wchar_t CLASS_NAME[] = L"VoidDRAM_MainWindowClass";

    HICON hAppIcon = (HICON)LoadImageW(m_hInstance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
    if (!hAppIcon) hAppIcon = LoadIconW(NULL, IDI_APPLICATION);

    HICON hAppIconSm = (HICON)LoadImageW(m_hInstance, MAKEINTRESOURCEW(IDI_APP_ICON), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    if (!hAppIconSm) hAppIconSm = hAppIcon;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WndProcStatic;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = m_hBgBrush;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon = hAppIcon;
    wc.hIconSm = hAppIconSm;

    if (!RegisterClassExW(&wc)) {
        return false;
    }

    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int posX = (screenW - 520) / 2;
    int posY = (screenH - 490) / 2;

    m_hWnd = CreateWindowExW(
        WS_EX_APPWINDOW,
        CLASS_NAME,
        L"VoidDRAM",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        posX, posY, 520, 490,
        NULL, NULL, hInstance, this
    );

    if (!m_hWnd) {
        return false;
    }

    HMENU hMenuBar = CreateMenu();
    HMENU hMenuAbout = CreatePopupMenu();
    AppendMenuW(hMenuAbout, MF_STRING, IDC_BTN_OPEN_REPO, L"Открыть репозиторий проекта");
    AppendMenuW(hMenuAbout, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenuAbout, MF_STRING, IDM_ABOUT, L"О программе...");
    AppendMenuW(hMenuBar, MF_POPUP, (UINT_PTR)hMenuAbout, L"О программе");
    SetMenu(m_hWnd, hMenuBar);

    CreateDashboardControls();

    m_nid.cbSize = sizeof(NOTIFYICONDATAW);
    m_nid.hWnd = m_hWnd;
    m_nid.uID = 1;
    m_nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    m_nid.uCallbackMessage = WM_TRAYICON_MSG;
    m_nid.hIcon = hAppIconSm;
    wcscpy_s(m_nid.szTip, L"VoidDRAM");

    if (Shell_NotifyIconW(NIM_ADD, &m_nid)) {
        m_trayAdded = true;
    }

    SetTimer(m_hWnd, TIMER_UI_REFRESH, 1000, NULL);

    SyncSettingsToUi();
    UpdateUiMetrics();

    if (nCmdShow != SW_HIDE) {
        ShowWindow(m_hWnd, nCmdShow);
        UpdateWindow(m_hWnd);
    }

    return true;
}

void TrayWindow::CreateDashboardControls() {
    m_hFontUi = CreateFontW(
        16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
    );

    m_hFontBold = CreateFontW(
        18, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, L"Segoe UI"
    );

    HWND hTitle = CreateWindowExW(
        0, L"STATIC",
        L"VoidDRAM - Монитор оперативной памяти",
        WS_CHILD | WS_VISIBLE,
        20, 15, 335, 24,
        m_hWnd, NULL, m_hInstance, NULL
    );
    SendMessageW(hTitle, WM_SETFONT, (WPARAM)m_hFontBold, TRUE);

    m_hBtnAbout = CreateWindowExW(
        0, L"BUTTON",
        L"О программе",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        365, 12, 115, 26,
        m_hWnd, (HMENU)IDC_BTN_ABOUT, m_hInstance, NULL
    );
    SendMessageW(m_hBtnAbout, WM_SETFONT, (WPARAM)m_hFontUi, TRUE);

    m_hLblRam = CreateWindowExW(
        0, L"STATIC",
        L"Загрузка RAM: расчет...",
        WS_CHILD | WS_VISIBLE,
        20, 48, 460, 20,
        m_hWnd, (HMENU)IDC_LBL_RAM_INFO, m_hInstance, NULL
    );
    SendMessageW(m_hLblRam, WM_SETFONT, (WPARAM)m_hFontUi, TRUE);

    m_hProgressBar = CreateWindowExW(
        0, PROGRESS_CLASSW, NULL,
        WS_CHILD | WS_VISIBLE | PBS_SMOOTH,
        20, 72, 460, 16,
        m_hWnd, (HMENU)IDC_PROGRESS_RAM, m_hInstance, NULL
    );
    SendMessageW(m_hProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, 100));

    m_hLblStats = CreateWindowExW(
        0, L"STATIC",
        L"События ядра: 0 | Таймаут fallback (30с): 0 | Ручных: 0\n"
        L"Освобождено: 0 МБ",
        WS_CHILD | WS_VISIBLE,
        20, 100, 460, 44,
        m_hWnd, (HMENU)IDC_LBL_STATS_INFO, m_hInstance, NULL
    );
    SendMessageW(m_hLblStats, WM_SETFONT, (WPARAM)m_hFontUi, TRUE);

    HWND hGrpMode = CreateWindowExW(
        0, L"BUTTON", L"Режим очистки",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        20, 152, 460, 110,
        m_hWnd, NULL, m_hInstance, NULL
    );
    SendMessageW(hGrpMode, WM_SETFONT, (WPARAM)m_hFontUi, TRUE);

    m_hRadioLow = CreateWindowExW(
        0, L"BUTTON",
        L"Низкий приоритет (Standby 0) - Рекомендуется для игр",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON | WS_GROUP,
        35, 175, 430, 22,
        m_hWnd, (HMENU)IDC_RADIO_MODE_LOW, m_hInstance, NULL
    );
    SendMessageW(m_hRadioLow, WM_SETFONT, (WPARAM)m_hFontUi, TRUE);

    m_hRadioFull = CreateWindowExW(
        0, L"BUTTON",
        L"Полный Standby (0–7)",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        35, 202, 430, 22,
        m_hWnd, (HMENU)IDC_RADIO_MODE_FULL, m_hInstance, NULL
    );
    SendMessageW(m_hRadioFull, WM_SETFONT, (WPARAM)m_hFontUi, TRUE);

    m_hRadioWorking = CreateWindowExW(
        0, L"BUTTON",
        L"Standby и Working Sets всех процессов",
        WS_CHILD | WS_VISIBLE | BS_AUTORADIOBUTTON,
        35, 229, 430, 22,
        m_hWnd, (HMENU)IDC_RADIO_MODE_WORKING, m_hInstance, NULL
    );
    SendMessageW(m_hRadioWorking, WM_SETFONT, (WPARAM)m_hFontUi, TRUE);

    HWND hGrpTweaks = CreateWindowExW(
        0, L"BUTTON", L"Параметры",
        WS_CHILD | WS_VISIBLE | BS_GROUPBOX,
        20, 270, 460, 85,
        m_hWnd, NULL, m_hInstance, NULL
    );
    SendMessageW(hGrpTweaks, WM_SETFONT, (WPARAM)m_hFontUi, TRUE);

    m_hChkTimerRes = CreateWindowExW(
        0, L"BUTTON",
        L"Разрешение таймера 1 мс",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 293, 430, 22,
        m_hWnd, (HMENU)IDC_CHK_TIMER_RES, m_hInstance, NULL
    );
    SendMessageW(m_hChkTimerRes, WM_SETFONT, (WPARAM)m_hFontUi, TRUE);

    m_hChkAutostart = CreateWindowExW(
        0, L"BUTTON",
        L"Автозапуск с Windows",
        WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        35, 320, 430, 22,
        m_hWnd, (HMENU)IDC_CHK_AUTOSTART, m_hInstance, NULL
    );
    SendMessageW(m_hChkAutostart, WM_SETFONT, (WPARAM)m_hFontUi, TRUE);

    m_hBtnClean = CreateWindowExW(
        0, L"BUTTON",
        L"Очистить память",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        20, 368, 225, 34,
        m_hWnd, (HMENU)IDC_BTN_CLEAN_NOW, m_hInstance, NULL
    );
    SendMessageW(m_hBtnClean, WM_SETFONT, (WPARAM)m_hFontBold, TRUE);

    m_hBtnMinimize = CreateWindowExW(
        0, L"BUTTON",
        L"Свернуть в трей",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        255, 368, 225, 34,
        m_hWnd, (HMENU)IDC_BTN_MINIMIZE, m_hInstance, NULL
    );
    SendMessageW(m_hBtnMinimize, WM_SETFONT, (WPARAM)m_hFontUi, TRUE);
}

void TrayWindow::SyncSettingsToUi() {
    PurgeMode mode = m_cleaner.GetPurgeMode();
    SendMessageW(m_hRadioLow, BM_SETCHECK, (mode == LowPriorityStandby) ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_hRadioFull, BM_SETCHECK, (mode == FullStandby) ? BST_CHECKED : BST_UNCHECKED, 0);
    SendMessageW(m_hRadioWorking, BM_SETCHECK, (mode == StandbyAndWorkingSets) ? BST_CHECKED : BST_UNCHECKED, 0);

    SendMessageW(m_hChkTimerRes, BM_SETCHECK, m_timerRes.IsActive() ? BST_CHECKED : BST_UNCHECKED, 0);

    bool isAutoStart = TaskSchedulerManager::IsTaskRegistered(L"VoidDRAM_ReactiveMemoryCleaner");
    SendMessageW(m_hChkAutostart, BM_SETCHECK, isAutoStart ? BST_CHECKED : BST_UNCHECKED, 0);
}

void TrayWindow::UpdateUiMetrics() {
    CleanerStats stats = m_cleaner.GetStats();

    SendMessageW(m_hProgressBar, PBM_SETPOS, stats.memoryLoadPercent, 0);

    uint64_t usedMb = (stats.totalPhysMb > stats.availPhysMb) ? (stats.totalPhysMb - stats.availPhysMb) : 0;
    double usedGb = (double)usedMb / 1024.0;
    double totalGb = (double)stats.totalPhysMb / 1024.0;

    std::wstring ramStr = std::format(
        L"RAM: {:.1f} / {:.1f} GB ({:.1f} GB свободно) - Загрузка: {}%",
        usedGb, totalGb, (double)stats.availPhysMb / 1024.0, stats.memoryLoadPercent
    );
    SetWindowTextW(m_hLblRam, ramStr.c_str());

    std::wstring statusDesc = NtMemoryManager::FormatStatus(stats.lastStatus);
    std::wstring statsStr = std::format(
        L"События ядра: {} | Fallback таймаут: {} | Ручных: {}\n"
        L"Суммарно освобождено: {} МБ | NTAPI: {}",
        stats.kernelTriggers, stats.timeoutTriggers, stats.manualTriggers,
        stats.totalFreedMb, statusDesc
    );
    SetWindowTextW(m_hLblStats, statsStr.c_str());

    std::wstring trayTip = std::format(
        L"VoidDRAM: {}% RAM ({:.1f} GB свободно)",
        stats.memoryLoadPercent, (double)stats.availPhysMb / 1024.0
    );
    if (m_trayAdded && trayTip.length() < sizeof(m_nid.szTip) / sizeof(m_nid.szTip[0])) {
        if (wcscmp(m_nid.szTip, trayTip.c_str()) != 0) {
            wcscpy_s(m_nid.szTip, trayTip.c_str());
            NOTIFYICONDATAW nid{};
            nid.cbSize = sizeof(NOTIFYICONDATAW);
            nid.hWnd = m_hWnd;
            nid.uID = 1;
            nid.uFlags = NIF_TIP;
            wcscpy_s(nid.szTip, trayTip.c_str());
            Shell_NotifyIconW(NIM_MODIFY, &nid);
        }
    }
}

void TrayWindow::ShowBalloonNotification(const std::wstring& title, const std::wstring& message) {
    if (!m_trayAdded) return;

    NOTIFYICONDATAW nid{};
    nid.cbSize = sizeof(NOTIFYICONDATAW);
    nid.hWnd = m_hWnd;
    nid.uID = 1;
    nid.uFlags = NIF_INFO;
    nid.dwInfoFlags = NIIF_INFO;
    wcscpy_s(nid.szInfoTitle, title.c_str());
    wcscpy_s(nid.szInfo, message.c_str());
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void TrayWindow::ToggleWindowVisibility() {
    if (IsWindowVisible(m_hWnd)) {
        ShowWindow(m_hWnd, SW_HIDE);
    } else {
        ShowWindow(m_hWnd, SW_SHOW);
        SetForegroundWindow(m_hWnd);
        UpdateUiMetrics();
    }
}

void TrayWindow::ShowContextMenu() {
    POINT pt;
    GetCursorPos(&pt);

    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    CleanerStats stats = m_cleaner.GetStats();

    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_CLEAN_NOW, L"Очистить память");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);

    HMENU hSubMode = CreatePopupMenu();
    AppendMenuW(hSubMode, MF_STRING | (stats.currentMode == LowPriorityStandby ? MF_CHECKED : MF_UNCHECKED),
        IDM_TRAY_MODE_LOW_PRIORITY, L"Низкий приоритет (Standby 0)");
    AppendMenuW(hSubMode, MF_STRING | (stats.currentMode == FullStandby ? MF_CHECKED : MF_UNCHECKED),
        IDM_TRAY_MODE_FULL_STANDBY, L"Полный Standby (0–7)");
    AppendMenuW(hSubMode, MF_STRING | (stats.currentMode == StandbyAndWorkingSets ? MF_CHECKED : MF_UNCHECKED),
        IDM_TRAY_MODE_WORKING_SETS, L"Standby и Working Sets");
    AppendMenuW(hMenu, MF_POPUP, (UINT_PTR)hSubMode, L"Режим очистки");

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING | (m_timerRes.IsActive() ? MF_CHECKED : MF_UNCHECKED),
        IDM_TRAY_TIMER_RES, L"Разрешение таймера 1 мс");

    bool isAutostart = TaskSchedulerManager::IsTaskRegistered(L"VoidDRAM_ReactiveMemoryCleaner");
    AppendMenuW(hMenu, MF_STRING | (isAutostart ? MF_CHECKED : MF_UNCHECKED),
        IDM_TRAY_AUTOSTART, L"Автозапуск с Windows");

    AppendMenuW(hMenu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_SHOW_DASHBOARD, L"Панель управления...");
    AppendMenuW(hMenu, MF_STRING, IDM_ABOUT, L"О программе...");
    AppendMenuW(hMenu, MF_STRING, IDC_BTN_OPEN_REPO, L"Открыть репозиторий проекта");
    AppendMenuW(hMenu, MF_STRING, IDM_TRAY_EXIT, L"Выход");

    SetMenuDefaultItem(hMenu, IDM_TRAY_SHOW_DASHBOARD, FALSE);

    SetForegroundWindow(m_hWnd);
    TrackPopupMenuEx(hMenu, TPM_RIGHTALIGN | TPM_BOTTOMALIGN, pt.x, pt.y, m_hWnd, NULL);
    PostMessageW(m_hWnd, WM_NULL, 0, 0);

    DestroyMenu(hMenu);
}

int TrayWindow::RunMessageLoop() {
    MSG msg;
    while (GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK TrayWindow::WndProcStatic(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    TrayWindow* pThis = NULL;
    if (uMsg == WM_NCCREATE) {
        CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
        pThis = (TrayWindow*)cs->lpCreateParams;
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (TrayWindow*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);
    }

    if (pThis) {
        return pThis->HandleMessage(hWnd, uMsg, wParam, lParam);
    }
    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

LRESULT TrayWindow::HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_TIMER:
            if (wParam == TIMER_UI_REFRESH) {
                if (IsWindowVisible(hWnd)) {
                    UpdateUiMetrics();
                }
            }
            return 0;

        case WM_TRAYICON_MSG:
            if (LOWORD(lParam) == WM_RBUTTONUP || LOWORD(lParam) == WM_CONTEXTMENU) {
                ShowContextMenu();
            } else if (LOWORD(lParam) == WM_LBUTTONDBLCLK || LOWORD(lParam) == WM_LBUTTONUP) {
                ToggleWindowVisibility();
            }
            return 0;

        case WM_COMMAND: {
            WORD id = LOWORD(wParam);
            switch (id) {
                case IDM_TRAY_CLEAN_NOW:
                case IDC_BTN_CLEAN_NOW:
                    m_cleaner.TriggerManualPurge();
                    UpdateUiMetrics();
                    break;

                case IDC_BTN_MINIMIZE:
                    ShowWindow(hWnd, SW_HIDE);
                    ShowBalloonNotification(L"VoidDRAM", L"Свернуто в системный трей.");
                    break;

                case IDM_TRAY_MODE_LOW_PRIORITY:
                case IDC_RADIO_MODE_LOW:
                    m_cleaner.SetPurgeMode(LowPriorityStandby);
                    SyncSettingsToUi();
                    break;

                case IDM_TRAY_MODE_FULL_STANDBY:
                case IDC_RADIO_MODE_FULL:
                    m_cleaner.SetPurgeMode(FullStandby);
                    SyncSettingsToUi();
                    break;

                case IDM_TRAY_MODE_WORKING_SETS:
                case IDC_RADIO_MODE_WORKING:
                    m_cleaner.SetPurgeMode(StandbyAndWorkingSets);
                    SyncSettingsToUi();
                    break;

                case IDM_TRAY_TIMER_RES:
                case IDC_CHK_TIMER_RES: {
                    if (m_timerRes.IsActive()) {
                        m_timerRes.Disable();
                    } else {
                        m_timerRes.Enable(1);
                    }
                    SyncSettingsToUi();
                    break;
                }

                case IDM_TRAY_AUTOSTART:
                case IDC_CHK_AUTOSTART: {
                    bool currentlyRegistered = TaskSchedulerManager::IsTaskRegistered(L"VoidDRAM_ReactiveMemoryCleaner");
                    if (currentlyRegistered) {
                        TaskSchedulerManager::DeleteTask(L"VoidDRAM_ReactiveMemoryCleaner");
                    } else {
                        wchar_t szExe[MAX_PATH];
                        if (GetModuleFileNameW(NULL, szExe, MAX_PATH) > 0) {
                            TaskSchedulerManager::RegisterElevatedLogonTask(L"VoidDRAM_ReactiveMemoryCleaner", szExe, L"--minimized");
                        }
                    }
                    SyncSettingsToUi();
                    break;
                }

                case IDM_TRAY_SHOW_DASHBOARD:
                    ShowWindow(hWnd, SW_SHOW);
                    SetForegroundWindow(hWnd);
                    UpdateUiMetrics();
                    break;

                case IDM_ABOUT:
                case IDC_BTN_ABOUT:
                    ShowAboutDialog();
                    break;

                case IDM_OPEN_REPO:
                case IDC_BTN_OPEN_REPO:
                    ShellExecuteW(NULL, L"open", L"https://github.com/Jwachka1337/VoidDram", NULL, NULL, SW_SHOWNORMAL);
                    break;

                case IDM_TRAY_EXIT:
                    DestroyWindow(hWnd);
                    break;
            }
            return 0;
        }

        case WM_CLOSE:
            ShowWindow(hWnd, SW_HIDE);
            ShowBalloonNotification(L"VoidDRAM", L"Приложение свёрнуто в трей.");
            return 0;

        case WM_SYSCOMMAND:
            if ((wParam & 0xFFF0) == SC_MINIMIZE) {
                ShowWindow(hWnd, SW_HIDE);
                return 0;
            }
            break;

        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, GetSysColor(COLOR_WINDOWTEXT));
            SetBkMode(hdcStatic, TRANSPARENT);
            return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
        }

        case WM_CTLCOLORDLG:
            return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);

        case WM_DESTROY:
            KillTimer(hWnd, TIMER_UI_REFRESH);
            PostQuitMessage(0);
            return 0;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK TrayWindow::AboutWndProcStatic(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    TrayWindow* pThis = (TrayWindow*)GetWindowLongPtrW(hWnd, GWLP_USERDATA);

    switch (uMsg) {
        case WM_CREATE: {
            CREATESTRUCTW* cs = (CREATESTRUCTW*)lParam;
            pThis = (TrayWindow*)cs->lpCreateParams;
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, (LONG_PTR)pThis);

            HWND hTitle = CreateWindowExW(
                0, L"STATIC",
                L"VoidDRAM",
                WS_CHILD | WS_VISIBLE,
                20, 15, 345, 22,
                hWnd, NULL, pThis->m_hInstance, NULL
            );
            SendMessageW(hTitle, WM_SETFONT, (WPARAM)pThis->m_hFontBold, TRUE);

            HWND hDesc = CreateWindowExW(
                0, L"STATIC",
                L"Реактивный Windows Standby List Cleaner\nВерсия 1.0",
                WS_CHILD | WS_VISIBLE,
                20, 40, 345, 36,
                hWnd, NULL, pThis->m_hInstance, NULL
            );
            SendMessageW(hDesc, WM_SETFONT, (WPARAM)pThis->m_hFontUi, TRUE);

            HWND hBtnRepo = CreateWindowExW(
                0, L"BUTTON",
                L"Открыть репозиторий проекта",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                20, 85, 345, 34,
                hWnd, (HMENU)IDC_BTN_OPEN_REPO, pThis->m_hInstance, NULL
            );
            SendMessageW(hBtnRepo, WM_SETFONT, (WPARAM)pThis->m_hFontBold, TRUE);

            HWND hBtnClose = CreateWindowExW(
                0, L"BUTTON",
                L"Закрыть",
                WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                145, 130, 100, 28,
                hWnd, (HMENU)IDCANCEL, pThis->m_hInstance, NULL
            );
            SendMessageW(hBtnClose, WM_SETFONT, (WPARAM)pThis->m_hFontUi, TRUE);
            return 0;
        }

        case WM_COMMAND: {
            WORD id = LOWORD(wParam);
            if (id == IDC_BTN_OPEN_REPO) {
                ShellExecuteW(NULL, L"open", L"https://github.com/Jwachka1337/VoidDram", NULL, NULL, SW_SHOWNORMAL);
            } else if (id == IDCANCEL || id == IDOK) {
                DestroyWindow(hWnd);
            }
            return 0;
        }

        case WM_CTLCOLORSTATIC: {
            HDC hdcStatic = (HDC)wParam;
            SetTextColor(hdcStatic, GetSysColor(COLOR_WINDOWTEXT));
            SetBkMode(hdcStatic, TRANSPARENT);
            return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);
        }

        case WM_CTLCOLORDLG:
            return (LRESULT)GetSysColorBrush(COLOR_BTNFACE);

        case WM_CLOSE:
            DestroyWindow(hWnd);
            return 0;

        case WM_DESTROY:
            if (pThis && pThis->m_hWnd) {
                PostMessageW(pThis->m_hWnd, WM_NULL, 0, 0);
            }
            return 0;
    }

    return DefWindowProcW(hWnd, uMsg, wParam, lParam);
}

void TrayWindow::ShowAboutDialog() {
    const wchar_t ABOUT_CLASS[] = L"VoidDRAM_AboutWindowClass";

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = AboutWndProcStatic;
    wc.hInstance = m_hInstance;
    wc.lpszClassName = ABOUT_CLASS;
    wc.hbrBackground = m_hBgBrush;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    RegisterClassExW(&wc);

    int dlgW = 400;
    int dlgH = 210;
    RECT rcParent{};
    GetWindowRect(m_hWnd, &rcParent);
    int posX = rcParent.left + (rcParent.right - rcParent.left - dlgW) / 2;
    int posY = rcParent.top + (rcParent.bottom - rcParent.top - dlgH) / 2;

    HWND hAboutWnd = CreateWindowExW(
        WS_EX_DLGMODALFRAME,
        ABOUT_CLASS,
        L"О программе",
        WS_POPUP | WS_CAPTION | WS_SYSMENU,
        posX, posY, dlgW, dlgH,
        m_hWnd, NULL, m_hInstance, this
    );

    if (!hAboutWnd) return;

    EnableWindow(m_hWnd, FALSE);
    ShowWindow(hAboutWnd, SW_SHOW);
    UpdateWindow(hAboutWnd);

    MSG msg;
    while (IsWindow(hAboutWnd) && GetMessageW(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (!IsWindow(hAboutWnd)) break;
    }

    EnableWindow(m_hWnd, TRUE);
    SetForegroundWindow(m_hWnd);
}
