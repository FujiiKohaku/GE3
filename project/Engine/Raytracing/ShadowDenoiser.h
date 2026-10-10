#pragma once
#include "Engine/Debug/GpuTimestampTimer.h"
#include "Engine/Math/MathStruct.h"
#include <array>

class Camera;
struct ShadowDenoiserInputs {
    const Camera* camera = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE rawMaskSrv = {};
    D3D12_GPU_DESCRIPTOR_HANDLE depthSrv = {};
    D3D12_GPU_DESCRIPTOR_HANDLE normalSrv = {};
    D3D12_GPU_DESCRIPTOR_HANDLE directionalLightSrv = {};
    D3D12_GPU_DESCRIPTOR_HANDLE motionVectorSrv = {};
    D3D12_GPU_DESCRIPTOR_HANDLE reprojectionSrv = {};
    D3D12_GPU_DESCRIPTOR_HANDLE previousReprojectionSrv = {};
    uint64_t sceneRevision = 0;
    uint64_t casterRevision = 0;
    Vector3 lightDirection = {};
    bool shouldUseTemporalHistory = true;
    uint32_t maxHistoryFrames = 32;
    uint32_t spatialPassCount = 2;
};

class ShadowDenoiser {
public:
    ~ShadowDenoiser();
    void Initialize() { timer_.Initialize(); }
    void ResetHistory();
    D3D12_GPU_DESCRIPTOR_HANDLE Draw(const ShadowDenoiserInputs& inputs);
    void ReadCompleted() { timer_.ReadCompleted(); }
    double GetGpuTimeMs() const { return timer_.GetDurationMs(); }
    uint64_t GetAllocationBytes() const { return allocationBytes_; }
    bool HasUsedHistory() const { return hasUsedHistory_; }
    ID3D12Resource* GetOutputTexture() const;
    ID3D12Resource* GetHistoryTexture() const;
private:
    static constexpr uint32_t kTargetCount = 6;
    void CreateResources();
    void ReleaseResources();
    void DrawPass(const ShadowDenoiserInputs& inputs, uint32_t targetIndex,
        D3D12_GPU_DESCRIPTOR_HANDLE signalSrv, uint32_t step, bool isTemporal);
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kTargetCount> textures_;
    std::array<uint32_t, kTargetCount> srvIndices_ = {UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX};
    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, kTargetCount> rtvHandles_ = {};
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> root_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> temporalPipeline_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> spatialPipeline_;
    Microsoft::WRL::ComPtr<ID3D12Resource> parameters_;
    bool isReady_ = false;
    bool hasHistory_ = false;
    bool hasUsedHistory_ = false;
    uint32_t historyIndex_ = 0;
    uint32_t outputIndex_ = UINT_MAX;
    uint64_t allocationBytes_ = 0;
    const Camera* previousCamera_ = nullptr;
    uint64_t previousCameraHistoryId_ = 0;
    uint64_t previousSceneRevision_ = 0;
    uint64_t previousCasterRevision_ = 0;
    Matrix4x4 previousViewProjection_ = {};
    Matrix4x4 previousView_ = {};
    Vector2 previousJitter_ = {};
    Vector3 previousLightDirection_ = {};
    GpuTimestampTimer timer_;
};
