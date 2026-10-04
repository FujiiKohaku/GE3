#pragma once
#include "Engine/DirectXCommon/DirectXCommon.h"
#include "Engine/Camera/Camera.h"
#include "Engine/Debug/GpuTimestampTimer.h"
#include <array>
struct ScreenSpaceReflectionInputs {
    D3D12_GPU_DESCRIPTOR_HANDLE colorSrv = {};
    D3D12_GPU_DESCRIPTOR_HANDLE depthSrv = {};
    D3D12_GPU_DESCRIPTOR_HANDLE normalSrv = {};
    D3D12_GPU_DESCRIPTOR_HANDLE motionVectorSrv = {};
    uint64_t sceneRevision = 0;
    const Camera* camera = nullptr;
};
enum class ScreenSpaceReflectionDebugMode {
    None,
    Reflection,
    ViewDepth,
    RayDirection,
    HitCoordinates,
    HitStatus
};
struct ScreenSpaceReflectionSettings {
    bool isEnabled = true;
    bool shouldUseHierarchicalDepth = true;
    bool shouldUseTemporalHistory = true;
    float historyWeight = 0.85f;
    bool shouldBlurReflection = true;
    float maxBlurRadiusPx = 24.0f;
    ScreenSpaceReflectionDebugMode debugMode = ScreenSpaceReflectionDebugMode::None;
    float maxDistance = 300.0f;
    float thickness = 2.0f;
    float strength = 0.85f;
};
class ScreenSpaceReflection {
public:
    ~ScreenSpaceReflection();
    void Initialize();
    D3D12_GPU_DESCRIPTOR_HANDLE Draw(const ScreenSpaceReflectionInputs& inputs);
    void DrawImGui();
    void SetDebugVisible(bool isDebugVisible) {
        settings_.debugMode = ScreenSpaceReflectionDebugMode::None;
        if (isDebugVisible) { settings_.debugMode = ScreenSpaceReflectionDebugMode::Reflection; }
    }
    void SetDebugMode(ScreenSpaceReflectionDebugMode debugMode) { settings_.debugMode = debugMode; }
    void SetSettings(const ScreenSpaceReflectionSettings& settings) { settings_ = settings; }
    const ScreenSpaceReflectionSettings& GetSettings() const { return settings_; }
    bool IsEnabled() const { return settings_.isEnabled; }
    void SetEnabled(bool isEnabled) { settings_.isEnabled = isEnabled; }
    void ResetHistory() { hasHistory_ = false; }
    bool HasHistory() const { return hasHistory_; }
    void ReadCompleted() { timer_.ReadCompleted(); }
    double GetGpuTimeMs() const { return timer_.GetDurationMs(); }
private:
    struct Parameters {
        Matrix4x4 projection;
        Matrix4x4 inverseProjection;
        Matrix4x4 view;
        Vector4 settings;
        Vector4 traversal;
        Matrix4x4 currentToPreviousView;
        Matrix4x4 previousProjection;
        Vector4 temporal;
        Vector4 filter;
    };
    void CreateTarget(uint32_t index, uint32_t width, uint32_t height);
    void Render(uint32_t index, const ScreenSpaceReflectionInputs& inputs);
    void CreateDepthPyramid();
    void BuildDepthPyramid(const ScreenSpaceReflectionInputs& inputs);
    static constexpr uint32_t kTargetCount = 9;
    static constexpr uint32_t kDepthMipCount = 12;
    Microsoft::WRL::ComPtr<ID3D12Resource> depthPyramid_;
    std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, 2> depthPipelineStates_;
    std::array<uint32_t, kDepthMipCount> depthMipSrvIndices_ = {};
    uint32_t depthSrvIndex_ = UINT_MAX;
    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, kDepthMipCount> depthRtvHandles_ = {};
    ScreenSpaceReflectionSettings settings_;
    Parameters* parameterData_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12Resource> parameterResource_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, 5> pipelineStates_;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kTargetCount> textures_;
    std::array<uint32_t, kTargetCount> srvIndices_ = {UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX};
    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, kTargetCount> rtvHandles_ = {};
    std::array<D3D12_GPU_DESCRIPTOR_HANDLE, kTargetCount> srvHandles_ = {};
    bool hasHistory_ = false;
    uint32_t historyWriteIndex_ = 3;
    uint32_t reflectionIndex_ = 0;
    const Camera* historyCamera_ = nullptr;
    uint64_t cameraHistoryId_ = 0;
    uint64_t sceneRevision_ = 0;
    Matrix4x4 previousView_ = {};
    Matrix4x4 previousProjection_ = {};
    Vector2 previousJitterNdc_ = {};
    ScreenSpaceReflectionSettings previousSettings_;
    GpuTimestampTimer timer_;
};
