#pragma once

#include <windows.h>
#include <mmsystem.h>

#pragma comment(lib, "winmm.lib")

class ScopedTimerResolution {
public:
    UINT period = 1;
    bool active = false;

    ScopedTimerResolution(UINT ms = 1) {
        period = ms;
        Enable(period);
    }

    ~ScopedTimerResolution() {
        Disable();
    }

    ScopedTimerResolution(const ScopedTimerResolution&) = delete;
    ScopedTimerResolution& operator=(const ScopedTimerResolution&) = delete;

    bool Enable(UINT ms = 1) {
        if (active && period == ms) return true;
        Disable();
        period = ms;
        if (timeBeginPeriod(period) == TIMERR_NOERROR) {
            active = true;
            return true;
        }
        return false;
    }

    void Disable() {
        if (active) {
            timeEndPeriod(period);
            active = false;
        }
    }

    bool IsActive() { return active; }
};
