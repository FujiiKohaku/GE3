#pragma once
#include <Windows.h>
#include <cstdint>
#include <memory>
#include "Engine/Math/MathStruct.h"

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
    void Show();
    void Finalize();

    static const int32_t kClientWidth = 1280;
    static const int32_t kClientHeight = 720;
    int32_t GetClientWidth() const;
    int32_t GetClientHeight() const;
    static constexpr int32_t kFullscreenWidth = 1920;
    static constexpr int32_t kFullscreenHeight = 1080;
    int32_t GetRenderWidth() const;
    int32_t GetRenderHeight() const;
    bool IsBorderlessFullscreen() const { return isBorderlessFullscreen_; }
    bool SetBorderlessFullscreen(bool isEnabled);
    Vector2 ClientToUi(const Vector2& clientPosition) const;
    Vector2 UiToClient(const Vector2& uiPosition) const;

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
    bool isBorderlessFullscreen_ = false;
    WINDOWPLACEMENT windowPlacement_ { sizeof(WINDOWPLACEMENT) };
};
