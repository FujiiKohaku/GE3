#pragma once
#include "DlssSuperResolution.h"
#include "Engine/Debug/GpuTimestampTimer.h"
#include <array>
class Camera;
struct TemporalResolutionSettings {
    bool isEnabled = false;
    bool shouldUseBicubic = true;
    uint32_t inputWidth = 1280;
    uint32_t inputHeight = 720;
    uint32_t outputWidth = 1280;
    uint32_t outputHeight = 720;
    float historyWeight = 0.9f;
    float varianceGamma = 1.25f;
};
struct TemporalResolutionFrameInputs {
    SuperResolutionFrameInputs scene;
    D3D12_GPU_DESCRIPTOR_HANDLE depthSrv {};
    D3D12_GPU_DESCRIPTOR_HANDLE motionSrv {};
    D3D12_GPU_DESCRIPTOR_HANDLE reprojectionSrv {};
    ID3D12Resource* reprojectionTexture = nullptr;
    Camera* camera = nullptr;
};
class TemporalSuperResolution {
public:
    ~TemporalSuperResolution();
    bool SetSettings(const TemporalResolutionSettings& settings);
    const TemporalResolutionSettings& GetSettings() const { return settings_; }
    void BeginFrame(const SuperResolutionHistoryInputs& inputs);
    Vector2 GetProjectionJitterNdc() const;
    D3D12_GPU_DESCRIPTOR_HANDLE Evaluate(const TemporalResolutionFrameInputs& inputs);
    void ResetHistory() { hasHistory_ = false; hasUsedHistory_ = false; }
    bool IsActive() const { return isActive_; }
    bool HasUsedHistory() const { return hasUsedHistory_; }
    ID3D12Resource* GetOutputTexture() const;
    uint64_t GetAllocationBytes() const { return allocationBytes_; }
    double GetGpuTimeMs() const { return timer_.GetDurationMs(); }
    void ReadCompleted() { timer_.ReadCompleted(); }
private:
    void Initialize();
    void CreateResources();
    struct Constants {
        Matrix4x4 inverseViewProjection;
        Matrix4x4 view;
        Matrix4x4 previousViewProjection;
        Matrix4x4 previousView;
        Vector4 cameraPosition;
        Vector4 inputSize;
        Vector4 outputSize;
        Vector4 controls;
        Vector4 jitter;
    };
    TemporalResolutionSettings settings_;
    SuperResolutionHistoryInputs historyInputs_;
    bool isReady_ = false;
    bool shouldRecreateResources_ = true;
    bool isActive_ = false;
    bool hasHistory_ = false;
    bool hasUsedHistory_ = false;
    uint32_t frameIndex_ = 0;
    uint32_t currentIndex_ = 0;
    uint64_t allocationBytes_ = 0;
    Vector2 jitterPixels_ {};
    Matrix4x4 previousViewProjection_ {};
    Matrix4x4 previousView_ {};
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 4> textures_;
    std::array<uint32_t, 8> descriptorIndices_ {UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX};
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipeline_;
    Microsoft::WRL::ComPtr<ID3D12Resource> constantsResource_;
    Constants* constants_ = nullptr;
    GpuTimestampTimer timer_;
};
