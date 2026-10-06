#pragma once

#include <windows.h>
#include <winternl.h>
#include <string>

#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#define STATUS_PRIVILEGE_NOT_HELD ((NTSTATUS)0xC0000061L)
#define STATUS_ACCESS_DENIED ((NTSTATUS)0xC0000022L)

enum MEMORY_COMMANDS {
    MemoryCaptureAccessedBits = 0,
    MemoryCaptureAndResetAccessedBits = 1,
    MemoryEmptyWorkingSets = 2,
    MemoryFlushModifiedList = 3,
    MemoryPurgeStandbyList = 4,
    MemoryPurgeLowPriorityStandbyList = 5
};

typedef NTSTATUS(NTAPI* pfnNtSetSystemInformation)(
    INT SystemInformationClass,
    PVOID SystemInformation,
    ULONG SystemInformationLength
);

class NtMemoryManager {
public:
    pfnNtSetSystemInformation NtSetSystemInfo = nullptr;

    NtMemoryManager() {
        HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
        if (hNtdll) {
            NtSetSystemInfo = (pfnNtSetSystemInformation)GetProcAddress(hNtdll, "NtSetSystemInformation");
        }
    }

    bool IsAvailable() {
        return NtSetSystemInfo != nullptr;
    }

    NTSTATUS ExecuteCommand(int cmd) {
        if (!NtSetSystemInfo) return -1;
        ULONG val = cmd;
        return NtSetSystemInfo(80, &val, sizeof(val));
    }

    NTSTATUS PurgeLowPriorityStandbyList() {
        return ExecuteCommand(MemoryPurgeLowPriorityStandbyList);
    }

    NTSTATUS PurgeStandbyList() {
        return ExecuteCommand(MemoryPurgeStandbyList);
    }

    NTSTATUS EmptyWorkingSets() {
        return ExecuteCommand(MemoryEmptyWorkingSets);
    }

    static std::wstring FormatStatus(NTSTATUS status) {
        if (status == STATUS_SUCCESS) return L"STATUS_SUCCESS";
        if (status == STATUS_PRIVILEGE_NOT_HELD) return L"STATUS_PRIVILEGE_NOT_HELD (Нужен админ)";
        if (status == STATUS_ACCESS_DENIED) return L"STATUS_ACCESS_DENIED";
        return L"Код: " + std::to_wstring(status);
    }
};
