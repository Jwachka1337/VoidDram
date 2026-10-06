#pragma once

#include <windows.h>
#include <atomic>
#include <thread>
#include <cstdint>
#include "raii.hpp"
#include "ntapi_memory.hpp"

enum PurgeMode {
    LowPriorityStandby = 0,
    FullStandby = 1,
    StandbyAndWorkingSets = 2
};

enum PurgeTrigger {
    KernelNotification = 0,
    TimeoutFallback = 1,
    Manual = 2
};

struct CleanerStats {
    uint64_t totalPurges = 0;
    uint64_t kernelTriggers = 0;
    uint64_t timeoutTriggers = 0;
    uint64_t manualTriggers = 0;
    uint64_t lastFreedMb = 0;
    uint64_t totalFreedMb = 0;
    uint32_t memoryLoadPercent = 0;
    uint64_t availPhysMb = 0;
    uint64_t totalPhysMb = 0;
    NTSTATUS lastStatus = 0;
    PurgeMode currentMode = LowPriorityStandby;
    uint32_t fallbackTimeoutSec = 30;
};

class ReactiveMemoryCleaner {
public:
    NtMemoryManager m_ntManager;
    MyHandle m_stopEvent;
    MyHandle m_lowMemoryNotification;
    MyHandle m_manualTriggerEvent;

    std::thread m_workerThread;
    std::atomic<bool> m_isRunning{false};
    std::atomic<bool> m_stopRequested{false};

    std::atomic<PurgeMode> m_purgeMode{LowPriorityStandby};
    std::atomic<DWORD> m_fallbackTimeoutMs{30000};
    std::atomic<DWORD> m_fallbackLoadThresholdPercent{80};

    std::atomic<uint64_t> m_totalPurges{0};
    std::atomic<uint64_t> m_kernelTriggers{0};
    std::atomic<uint64_t> m_timeoutTriggers{0};
    std::atomic<uint64_t> m_manualTriggers{0};
    std::atomic<uint64_t> m_lastFreedMb{0};
    std::atomic<uint64_t> m_totalFreedMb{0};
    std::atomic<uint32_t> m_lastLoadPercent{0};
    std::atomic<uint64_t> m_availPhysMb{0};
    std::atomic<uint64_t> m_totalPhysMb{0};
    std::atomic<NTSTATUS> m_lastStatus{0};

    ReactiveMemoryCleaner();
    ~ReactiveMemoryCleaner();

    bool Start();
    void Stop();
    void TriggerManualPurge();

    void SetPurgeMode(PurgeMode mode) {
        m_purgeMode = mode;
    }

    PurgeMode GetPurgeMode() {
        return m_purgeMode;
    }

    CleanerStats GetStats();
    void WorkerThreadLoop();
    void ExecutePurgeInternal(PurgeTrigger trigger);
    void RefreshMemoryStatus();
};
