#include <windows.h>
#include <string>
#include "../include/raii.hpp"
#include "../include/privileges.hpp"
#include "../include/ntapi_memory.hpp"
#include "../include/timer_resolution.hpp"
#include "../include/memory_cleaner.hpp"
#include "../include/tray_window.hpp"

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;

    ScopedCoInit comInit;

    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"Local\\VoidDRAM_SingleInstance_Mutex_Unique");
    if (hMutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        HWND hExisting = FindWindowW(L"VoidDRAM_MainWindowClass", NULL);
        if (hExisting) {
            ShowWindow(hExisting, SW_SHOW);
            SetForegroundWindow(hExisting);
        }
        CloseHandle(hMutex);
        return 0;
    }
    MyHandle singleMutex(hMutex);

    if (!IsProcessElevated()) {
        int choice = MessageBoxW(
            NULL,
            L"VoidDRAM требует прав Администратора"
            L"(NtSetSystemInformation) и сброса Standby List.\n\n"
            L"Перезапустить приложение с повышенными привилегиями?",
            L"VoidDRAM",
            MB_YESNO | MB_ICONQUESTION
        );

        if (choice == IDYES) {
            RelaunchAsAdmin(NULL, lpCmdLine);
        }
        return 0;
    }

    PrivilegeResult privResult = EnableRequiredPrivileges();
    if (!privResult.HasProfilePrivilege()) {
        MessageBoxW(
            NULL,
            L"Не удалось включить привилегию SE_PROFILE_SINGLE_PROCESS_NAME.",
            L"VoidDRAM",
            MB_OK | MB_ICONWARNING
        );
    }

    NtMemoryManager ntMgr;
    if (!ntMgr.IsAvailable()) {
        MessageBoxW(
            NULL,
            L"Не удалось получить NtSetSystemInformation из ntdll.dll.",
            L"VoidDRAM",
            MB_OK | MB_ICONERROR
        );
        return 1;
    }

    ScopedTimerResolution timerResolution(1);

    ReactiveMemoryCleaner cleaner;
    if (!cleaner.Start()) {
        MessageBoxW(
            NULL,
            L"Ошибка инициализации CreateMemoryResourceNotification.",
            L"VoidDRAM",
            MB_OK | MB_ICONERROR
        );
        return 1;
    }

    bool startMinimized = false;
    if (lpCmdLine) {
        std::wstring cmdStr(lpCmdLine);
        if (cmdStr.find(L"--minimized") != std::wstring::npos ||
            cmdStr.find(L"/minimized") != std::wstring::npos) {
            startMinimized = true;
        }
    }

    TrayWindow trayApp(cleaner, timerResolution);
    if (!trayApp.Initialize(hInstance, startMinimized ? SW_HIDE : nCmdShow)) {
        cleaner.Stop();
        return 1;
    }

    if (startMinimized) {
        trayApp.ShowBalloonNotification(L"VoidDRAM", L"Работает в фоне.");
    }

    int exitCode = trayApp.RunMessageLoop();

    cleaner.Stop();
    timerResolution.Disable();

    return exitCode;
}
