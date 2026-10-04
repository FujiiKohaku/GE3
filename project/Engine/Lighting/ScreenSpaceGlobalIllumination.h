#pragma once
#include "Engine/Camera/Camera.h"
#include "Engine/Debug/GpuTimestampTimer.h"
#include <array>
#if defined(ENABLE_DEVELOPMENT_TOOLS)
#include "externals/json.hpp"
#endif

enum class ScreenSpaceGlobalIlluminationDebugMode {
    None,
    IndirectLight,
    RawIndirectLight
};

struct ScreenSpaceGlobalIlluminationSettings {
    bool isEnabled = true;
    bool shouldUseTemporalHistory = true;
    bool shouldBlur = true;
    bool shouldUseHierarchicalDepth = true;
    float strength = 0.65f;
    float maxDistance = 12.0f;
    float thickness = 0.5f;
    float historyWeight = 0.85f;
    float maxRadiance = 4.0f;
    uint32_t rayCount = 4;
    uint32_t stepCount = 16;
    ScreenSpaceGlobalIlluminationDebugMode debugMode = ScreenSpaceGlobalIlluminationDebugMode::None;
};

struct ScreenSpaceGlobalIlluminationInputs {
    D3D12_GPU_DESCRIPTOR_HANDLE colorSrv {};
    D3D12_GPU_DESCRIPTOR_HANDLE depthSrv {};
    D3D12_GPU_DESCRIPTOR_HANDLE normalSrv {};
    D3D12_GPU_DESCRIPTOR_HANDLE materialSrv {};
    D3D12_GPU_DESCRIPTOR_HANDLE motionVectorSrv {};
    D3D12_GPU_DESCRIPTOR_HANDLE depthPyramidSrv {};
    const Camera* camera = nullptr;
    uint64_t sceneRevision = 0;
};

class ScreenSpaceGlobalIllumination {
public:
    ~ScreenSpaceGlobalIllumination();
    void Initialize();
    D3D12_GPU_DESCRIPTOR_HANDLE Draw(const ScreenSpaceGlobalIlluminationInputs& inputs);
    bool SetSettings(const ScreenSpaceGlobalIlluminationSettings& settings);
    const ScreenSpaceGlobalIlluminationSettings& GetSettings() const { return settings_; }
    bool IsEnabled() const { return settings_.isEnabled; }
    void SetEnabled(bool isEnabled);
    void ResetHistory();
    uint64_t GetSettingsRevision() const { return settingsRevision_; }
    bool HasHistory() const { return hasHistory_; }
    void ReadCompleted() { timer_.ReadCompleted(); }
    double GetGpuTimeMs() const { return timer_.GetDurationMs(); }
    void DrawImGui();
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    nlohmann::json GetDevelopmentState() const;
    nlohmann::json GetDevelopmentControls() const;
    bool SetDevelopmentBool(const std::string& key, bool isEnabled);
    bool SetDevelopmentNumber(const std::string& key, double value);
    bool ExecuteDevelopmentCommand(const std::string& key);
#endif

private:
    struct Parameters {
        Matrix4x4 projection;
        Matrix4x4 inverseProjection;
        Matrix4x4 view;
        Matrix4x4 currentToPreviousView;
        Matrix4x4 previousProjection;
        Vector4 tracing;
        Vector4 sampling;
        Vector4 temporal;
        Vector4 composition;
    };
    void CreateTarget(uint32_t index, uint32_t width, uint32_t height);
    void Render(uint32_t passIndex, uint32_t targetIndex, uint32_t sourceIndex,
        const ScreenSpaceGlobalIlluminationInputs& inputs);
    static constexpr uint32_t kTargetCount = 9;
    static constexpr uint32_t kPassCount = 5;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kTargetCount> textures_;
    std::array<D3D12_CPU_DESCRIPTOR_HANDLE, kTargetCount> rtvHandles_ {};
    std::array<D3D12_GPU_DESCRIPTOR_HANDLE, kTargetCount> srvHandles_ {};
    std::array<uint32_t, kTargetCount> srvIndices_ = {UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX,
        UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX};
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, kPassCount> pipelines_;
    Microsoft::WRL::ComPtr<ID3D12Resource> parameterResource_;
    Parameters* parameterData_ = nullptr;
    ScreenSpaceGlobalIlluminationSettings settings_;
    GpuTimestampTimer timer_;
    uint32_t historyWriteIndex_ = 2;
    uint32_t frameIndex_ = 0;
    bool hasHistory_ = false;
    bool isInitialized_ = false;
    const Camera* historyCamera_ = nullptr;
    uint64_t cameraHistoryId_ = 0;
    uint64_t sceneRevision_ = 0;
    uint64_t settingsRevision_ = 0;
    Matrix4x4 previousView_ {};
    Matrix4x4 previousProjection_ {};
    Vector2 previousJitterNdc_ {};
};
