#pragma once

#include "PreviewGif.h"
#include <d3d12.h>
#include <wrl.h>
#include <filesystem>
#include <string>

class DirectXCommon;

class PreviewCapture {
public:
    void Initialize(DirectXCommon* dx);
    void RequestPng();
    void StartGif(int framesPerSecond, int frameCount);
    void StopGif();
    bool IsRecording() const { return recording_; }
    int FrameIndex() const { return frameIndex_; }
    int FrameCount() const { return frameCount_; }
    float FrameSeconds() const;
    const std::string& Status() const { return status_; }
    // Record commands before drawing ImGui, then consume after the GPU fence.
    void Prepare();
    void Complete();

private:
    std::filesystem::path NewPath(const char* extension) const;
    void EnsureReadback(ID3D12Resource* source);
    DirectXCommon* dx_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> readback_;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint_ {};
    UINT64 readbackSize_ = 0;
    bool pngRequested_ = false;
    bool prepared_ = false;
    bool recording_ = false;
    int frameIndex_ = 0;
    int frameCount_ = 0;
    int framesPerSecond_ = 15;
    PreviewGif gif_;
    std::filesystem::path gifPath_;
    std::string status_ = "Ready. Captures exclude the controls.";
};
