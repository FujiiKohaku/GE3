#pragma once
#include <Windows.h>
#include <cstdint>
#include <memory>

class WinApp {
public:
    static WinApp* GetInstance();

    static void FinalizeInstance();

public:
    class ConstructorKey {
    private:
        ConstructorKey() = default;
        friend class WinApp;
    };

    explicit WinApp(ConstructorKey) { }
    ~WinApp() = default;

    WinApp(const WinApp&) = delete;
    WinApp& operator=(const WinApp&) = delete;

public:
    void initialize();
    void Finalize();

    static const int32_t kClientWidth = 1280;
    static const int32_t kClientHeight = 720;

    static LRESULT CALLBACK WindowProc(
        HWND hwnd,
        UINT msg,
        WPARAM wparam,
        LPARAM lparam);

    HWND GetHwnd() const { return hwnd_; }
    HINSTANCE GetHinstance() const { return wc_.hInstance; }

    bool ProcessMessage();

private:
    static std::unique_ptr<WinApp> instance_;

private:
    HWND hwnd_ = nullptr;
    WNDCLASS wc_ {};
};