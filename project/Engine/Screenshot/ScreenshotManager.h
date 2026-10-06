#pragma once

#include "Engine/2D/Text/Text.h"
#include <Windows.h>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <wrl.h>

class DirectXCommon;
class WinApp;
struct ID3D12Resource;

class ScreenshotManager {
public:
    static ScreenshotManager* GetInstance();
    static void Finalize();

    class ConstructorKey {
    private:
        ConstructorKey() = default;
        friend class ScreenshotManager;
    };

    explicit ScreenshotManager(ConstructorKey);
    ~ScreenshotManager() = default;

    void Initialize(DirectXCommon* dxCommon, WinApp* winApp);
    void Update();
    void RequestCapture(bool savePng);
    void DrawNotification();

    void PrepareCapture();
    void CompleteCapture();

private:
    static std::unique_ptr<ScreenshotManager> instance_;

    bool CreateReadbackResource(ID3D12Resource* backBuffer);
    bool CopyToClipboard(const std::uint8_t* sourcePixels);
    bool SavePng(const std::uint8_t* sourcePixels);
    std::filesystem::path CreatePngPath() const;
    void ShowSuccessNotification();
    void ShowFailureNotification();

    DirectXCommon* dxCommon_ = nullptr;
    WinApp* winApp_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> readbackResource_;

    bool initialized_ = false;
    bool captureRequested_ = false;
    bool capturePrepared_ = false;
    bool savePngRequested_ = false;

    std::uint32_t width_ = 0;
    std::uint32_t height_ = 0;
    std::uint32_t rowPitch_ = 0;
    std::uint64_t readbackSize_ = 0;

    Text notificationText_;
    float notificationTimer_ = 0.0f;
    static constexpr float kNotificationDuration = 2.0f;
};
