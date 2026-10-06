#pragma once

#include <windows.h>
#include <shellapi.h>
#include "raii.hpp"

#ifndef SE_PROF_SINGLE_PROCESS_NAME
#define SE_PROF_SINGLE_PROCESS_NAME TEXT("SeProfileSingleProcessPrivilege")
#endif

struct PrivilegeResult {
    bool quotaPrivilege = false;
    bool profilePrivilege = false;
    DWORD lastError = 0;

    bool HasProfilePrivilege() { return profilePrivilege; }
    bool AllEnabled() { return quotaPrivilege && profilePrivilege; }
};

inline bool IsProcessElevated() {
    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) return false;
    MyHandle token(hToken);

    TOKEN_ELEVATION elevation;
    DWORD size = sizeof(TOKEN_ELEVATION);
    if (!GetTokenInformation(token.Get(), TokenElevation, &elevation, sizeof(elevation), &size)) {
        return false;
    }
    return elevation.TokenIsElevated != 0;
}

inline bool SetSinglePrivilege(HANDLE hToken, const wchar_t* name) {
    LUID luid;
    if (!LookupPrivilegeValueW(NULL, name, &luid)) return false;

    TOKEN_PRIVILEGES tp;
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    SetLastError(0);
    if (!AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL)) {
        return false;
    }
    return GetLastError() == ERROR_SUCCESS;
}

inline PrivilegeResult EnableRequiredPrivileges() {
    PrivilegeResult res;
    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        res.lastError = GetLastError();
        return res;
    }
    MyHandle token(hToken);

    res.quotaPrivilege = SetSinglePrivilege(token.Get(), SE_INCREASE_QUOTA_NAME);
    res.profilePrivilege = SetSinglePrivilege(token.Get(), SE_PROF_SINGLE_PROCESS_NAME);
    return res;
}

inline bool RelaunchAsAdmin(HWND parent = NULL, const wchar_t* args = L"") {
    wchar_t path[MAX_PATH];
    if (GetModuleFileNameW(NULL, path, MAX_PATH) == 0) return false;

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas";
    sei.lpFile = path;
    sei.lpParameters = args;
    sei.hwnd = parent;
    sei.nShow = SW_NORMAL;
    return ShellExecuteExW(&sei) != FALSE;
}
