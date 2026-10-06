#include "../include/memory_cleaner.hpp"

ReactiveMemoryCleaner::ReactiveMemoryCleaner() {
    m_stopEvent.Reset(CreateEventW(NULL, TRUE, FALSE, NULL));
    m_lowMemoryNotification.Reset(CreateMemoryResourceNotification(LowMemoryResourceNotification));
    m_manualTriggerEvent.Reset(CreateEventW(NULL, FALSE, FALSE, NULL));

    RefreshMemoryStatus();
}

ReactiveMemoryCleaner::~ReactiveMemoryCleaner() {
    Stop();
}

bool ReactiveMemoryCleaner::Start() {
    if (m_isRunning) return true;

    if (!m_lowMemoryNotification.IsValid() || !m_stopEvent.IsValid() || !m_manualTriggerEvent.IsValid()) {
        return false;
    }

    m_stopRequested = false;
    ResetEvent(m_stopEvent.Get());

    m_isRunning = true;
    m_workerThread = std::thread(&ReactiveMemoryCleaner::WorkerThreadLoop, this);
    return true;
}

void ReactiveMemoryCleaner::Stop() {
    if (!m_isRunning) return;

    m_stopRequested = true;
    if (m_stopEvent.IsValid()) {
        SetEvent(m_stopEvent.Get());
    }

    if (m_workerThread.joinable()) {
        m_workerThread.join();
    }

    m_isRunning = false;
}

void ReactiveMemoryCleaner::TriggerManualPurge() {
    if (m_manualTriggerEvent.IsValid()) {
        SetEvent(m_manualTriggerEvent.Get());
    }
}

CleanerStats ReactiveMemoryCleaner::GetStats() {
    CleanerStats s;
    s.totalPurges = m_totalPurges;
    s.kernelTriggers = m_kernelTriggers;
    s.timeoutTriggers = m_timeoutTriggers;
    s.manualTriggers = m_manualTriggers;
    s.lastFreedMb = m_lastFreedMb;
    s.totalFreedMb = m_totalFreedMb;
    s.memoryLoadPercent = m_lastLoadPercent;
    s.availPhysMb = m_availPhysMb;
    s.totalPhysMb = m_totalPhysMb;
    s.lastStatus = m_lastStatus;
    s.currentMode = m_purgeMode;
    s.fallbackTimeoutSec = m_fallbackTimeoutMs / 1000;
    return s;
}

void ReactiveMemoryCleaner::RefreshMemoryStatus() {
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(MEMORYSTATUSEX);
    if (GlobalMemoryStatusEx(&mem)) {
        m_lastLoadPercent = mem.dwMemoryLoad;
        m_availPhysMb = mem.ullAvailPhys / (1024 * 1024);
        m_totalPhysMb = mem.ullTotalPhys / (1024 * 1024);
    }
}

void ReactiveMemoryCleaner::WorkerThreadLoop() {
    HANDLE waitHandles[3] = {
        m_stopEvent.Get(),
        m_lowMemoryNotification.Get(),
        m_manualTriggerEvent.Get()
    };

    while (!m_stopRequested) {
        DWORD timeout = m_fallbackTimeoutMs;
        DWORD waitRes = WaitForMultipleObjects(3, waitHandles, FALSE, timeout);

        if (waitRes == WAIT_OBJECT_0) {
            break;
        }
        else if (waitRes == WAIT_OBJECT_0 + 1) {
            ExecutePurgeInternal(KernelNotification);
        }
        else if (waitRes == WAIT_OBJECT_0 + 2) {
            ExecutePurgeInternal(Manual);
        }
        else if (waitRes == WAIT_TIMEOUT) {
            RefreshMemoryStatus();

            BOOL isKernelLow = FALSE;
            QueryMemoryResourceNotification(m_lowMemoryNotification.Get(), &isKernelLow);

            DWORD threshold = m_fallbackLoadThresholdPercent;
            DWORD currentLoad = m_lastLoadPercent;

            if (isKernelLow || currentLoad >= threshold) {
                ExecutePurgeInternal(TimeoutFallback);
            }
        }
    }
}

void ReactiveMemoryCleaner::ExecutePurgeInternal(PurgeTrigger trigger) {
    MEMORYSTATUSEX before;
    before.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&before);

    PurgeMode mode = m_purgeMode;
    NTSTATUS status = 0;

    if (mode == LowPriorityStandby) {
        status = m_ntManager.PurgeLowPriorityStandbyList();
    }
    else if (mode == FullStandby) {
        status = m_ntManager.PurgeStandbyList();
    }
    else if (mode == StandbyAndWorkingSets) {
        m_ntManager.EmptyWorkingSets();
        status = m_ntManager.PurgeStandbyList();
    }

    MEMORYSTATUSEX after;
    after.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&after);

    uint64_t freedBytes = (after.ullAvailPhys > before.ullAvailPhys)
        ? (after.ullAvailPhys - before.ullAvailPhys)
        : 0;
    uint64_t freedMb = freedBytes / (1024 * 1024);

    m_totalPurges++;
    if (trigger == KernelNotification) {
        m_kernelTriggers++;
    } else if (trigger == TimeoutFallback) {
        m_timeoutTriggers++;
    } else {
        m_manualTriggers++;
    }

    m_lastFreedMb = freedMb;
    m_totalFreedMb += freedMb;
    m_lastLoadPercent = after.dwMemoryLoad;
    m_availPhysMb = after.ullAvailPhys / (1024 * 1024);
    m_totalPhysMb = after.ullTotalPhys / (1024 * 1024);
    m_lastStatus = status;
}
