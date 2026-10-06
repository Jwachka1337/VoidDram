#pragma once

#include <windows.h>

class MyHandle {
public:
    HANDLE h = NULL;

    MyHandle() {}
    MyHandle(HANDLE handle) { h = handle; }

    ~MyHandle() {
        Close();
    }

    MyHandle(const MyHandle&) = delete;
    MyHandle& operator=(const MyHandle&) = delete;

    void Close() {
        if (h && h != INVALID_HANDLE_VALUE) {
            CloseHandle(h);
            h = NULL;
        }
    }

    void Reset(HANDLE handle = NULL) {
        if (h != handle) {
            Close();
            h = handle;
        }
    }

    HANDLE Get() const { return h; }
    bool IsValid() const { return h != NULL && h != INVALID_HANDLE_VALUE; }
    operator HANDLE() const { return h; }

    HANDLE* Put() {
        Close();
        return &h;
    }
};

template <typename T>
class MyComPtr {
public:
    T* ptr = nullptr;

    MyComPtr() {}
    MyComPtr(T* p) { ptr = p; }

    ~MyComPtr() {
        Reset();
    }

    MyComPtr(const MyComPtr&) = delete;
    MyComPtr& operator=(const MyComPtr&) = delete;

    void Reset() {
        if (ptr) {
            ptr->Release();
            ptr = nullptr;
        }
    }

    T* Get() const { return ptr; }
    T* operator->() const { return ptr; }
    T& operator*() const { return *ptr; }
    bool IsValid() const { return ptr != nullptr; }

    T** Put() {
        Reset();
        return &ptr;
    }

    void** VoidPut() {
        return (void**)Put();
    }
};

class ScopedCoInit {
public:
    HRESULT hr;

    ScopedCoInit() {
        hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
    }

    ~ScopedCoInit() {
        if (SUCCEEDED(hr)) {
            CoUninitialize();
        }
    }
};
