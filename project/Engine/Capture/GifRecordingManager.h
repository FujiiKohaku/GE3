#pragma once
#include "Engine/2D/Text/Text.h"
#include <d3d12.h>
#include <wrl.h>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class DirectXCommon;
class WinApp;

// Main thread owns capture resources; the worker owns GIF conversion and file I/O.
class GifRecordingManager {
public:
    static GifRecordingManager* GetInstance();
    static void Finalize();
    ~GifRecordingManager();
    void Initialize(DirectXCommon* dx, WinApp* window);
    void ToggleRecording();
    void ChangeRecordingDuration(int direction);
    int GetRecordingSeconds() const { return recordingSeconds_; }
    void Update();
    void PrepareCapture();
    void CompleteCapture();
    void DrawNotification();
private:
    using Clock = std::chrono::steady_clock;
    struct Frame { std::vector<unsigned char> rgba; int timeCs = 0; };
    void StopRecording();
    void FailCapture(const std::string& message);
    void Encode(std::filesystem::path path);
    void Notify(const std::string& message);
    double Elapsed() const;
    static std::unique_ptr<GifRecordingManager> instance_;
    DirectXCommon* dx_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> readback_;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint_{};
    UINT64 readbackSize_ = 0;
    UINT sourceWidth_ = 0, sourceHeight_ = 0;
    DXGI_FORMAT sourceFormat_ = DXGI_FORMAT_UNKNOWN;
    Text notification_;
    Clock::time_point started_{}, notifyUntil_{};
    double nextCapture_ = 0;
    int captureTimeCs_ = 0;
    int recordingSeconds_ = 5;
    int activeSeconds_ = 5;
    bool recording_ = false, prepared_ = false, initialized_ = false;
    std::thread worker_;
    std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<Frame> queue_;
    bool stopping_ = false, finished_ = false;
    int endTimeCs_ = 0;
    std::string workerResult_, captureError_;
};
