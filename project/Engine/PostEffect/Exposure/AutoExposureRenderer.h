#pragma once
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/math/MathStruct.h"
#include "Engine/Debug/GpuTimestampTimer.h"
#include <array>
struct AutoExposureSettings {
    bool isEnabled = false;
    float minExposure = 0.015625f;
    float maxExposure = 64.0f;
    float middleGray = 0.18f;
    float lowPercentile = 0.05f;
    float highPercentile = 0.95f;
    float brightenSpeedPerSecond = 1.0f;
    float darkenSpeedPerSecond = 3.0f;
};
class AutoExposureRenderer {
public:
    ~AutoExposureRenderer();
    bool SetSettings(const AutoExposureSettings& settings);
    const AutoExposureSettings& GetSettings() const { return settings_; }
    void ResetHistory() { hasHistory_ = false; }
    void Generate(ID3D12Resource* sceneTexture, D3D12_GPU_DESCRIPTOR_HANDLE sceneSrv, float deltaSeconds);
    ID3D12Resource* GetExposureTexture() const;
    D3D12_GPU_DESCRIPTOR_HANDLE GetExposureSrv() const;
    uint64_t GetAllocationBytes() const { return allocationBytes_; }
    double GetGpuTimeMs() const { return timer_.GetDurationMs(); }
    void ReadCompleted() { timer_.ReadCompleted(); }
private:
    void Initialize();
    struct Constants {
        Vector4 luminanceRange;
        Vector4 exposureRange;
        Vector4 adaptation;
        Vector4 percentiles;
    };
    AutoExposureSettings settings_;
    bool isReady_ = false;
    bool hasHistory_ = false;
    uint32_t currentIndex_ = 0;
    uint64_t allocationBytes_ = 0;
    std::array<uint32_t, 4> descriptorIndices_ {UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX};
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, 3> pipelines_;
    Microsoft::WRL::ComPtr<ID3D12Resource> histogram_;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, 2> textures_;
    Microsoft::WRL::ComPtr<ID3D12Resource> constantsResource_;
    Constants* constants_ = nullptr;
    GpuTimestampTimer timer_;
};
