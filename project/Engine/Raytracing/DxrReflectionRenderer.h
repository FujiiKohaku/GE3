#pragma once
#include "DxrRenderer.h"
#include <array>

enum class DxrLightingMode { Reflection, DiffuseIndirect, LocalShadow };

struct DxrReflectionSettings {
    bool isEnabled = false;
    bool isDebugVisible = false;
    bool shouldUseTemporalHistory = true;
    bool shouldTraceSunShadows = true;
    bool shouldUseTextureMipmaps = true;
    bool shouldUseLowDiscrepancySampling = true;
    bool shouldTraceMultipleReflections = false;
    uint32_t sampleCount = 4;
    uint32_t spatialPassCount = 2;
    uint32_t maxReflectionBounces = 2;
    float maxDistance = 1000.0f;
    float maxRoughness = 0.85f;
    float normalBias = 0.02f;
    float rayBias = 0.001f;
    float strength = 1.0f;
    float historyWeight = 0.9f;
    float maxRadiance = 10.0f;
    float indirectDistanceFadeRatio = 0.2f;
};
struct DxrReflectionInputs {
    const DxrRenderer* scene = nullptr;
    const Camera* camera = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE colorSrv = {};
    D3D12_GPU_DESCRIPTOR_HANDLE depthSrv = {};
    D3D12_GPU_DESCRIPTOR_HANDLE surfaceSrv = {};
    D3D12_GPU_DESCRIPTOR_HANDLE environmentSrv = {};
    D3D12_GPU_DESCRIPTOR_HANDLE materialSrv = {};
    D3D12_GPU_DESCRIPTOR_HANDLE motionVectorSrv = {};
    D3D12_GPU_DESCRIPTOR_HANDLE reprojectionSrv = {};
    D3D12_GPU_DESCRIPTOR_HANDLE previousReprojectionSrv = {};
    ID3D12Resource* depthTexture = nullptr;
    ID3D12Resource* surfaceTexture = nullptr;
    ID3D12Resource* environmentTexture = nullptr;
    ID3D12Resource* materialTexture = nullptr;
    ID3D12Resource* indirectTexture = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE indirectSrv = {};
    ID3D12Resource* localLightTexture = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE localLightSrv = {};
    uint64_t sceneRevision = 0;
};
class DxrReflectionRenderer {
public:
    explicit DxrReflectionRenderer(DxrLightingMode mode = DxrLightingMode::Reflection);
    ~DxrReflectionRenderer();
    void Initialize();
    bool SetSettings(const DxrReflectionSettings& settings);
    const DxrReflectionSettings& GetSettings() const { return settings_; }
    uint64_t GetSettingsRevision() const { return settingsRevision_; }
    D3D12_GPU_DESCRIPTOR_HANDLE Draw(const DxrReflectionInputs& inputs);
    void ResetHistory();
    void ReadCompleted();
    void DrawImGui();
    bool HasValidFrame() const { return hasValidFrame_; }
    bool HasUsedHistory() const { return hasUsedHistory_; }
    ID3D12Resource* GetRawTexture() const { return textures_[0].Get(); }
    ID3D12Resource* GetFilteredTexture() const;
    ID3D12Resource* GetColorTexture() const { return textures_[8].Get(); }
    ID3D12Resource* GetHistoryStatisticsTexture() const;
    uint64_t GetAllocationBytes() const { return allocationBytes_; }
    double GetTraceGpuTimeMs() const { return traceTimer_.GetDurationMs(); }
    double GetFilterGpuTimeMs() const { return filterTimer_.GetDurationMs(); }
    double GetCompositeGpuTimeMs() const { return compositeTimer_.GetDurationMs(); }
    const std::string& GetStatus() const { return status_; }
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    nlohmann::json GetDevelopmentState() const;
    nlohmann::json GetDevelopmentControls() const;
    bool SetDevelopmentBool(const std::string& key, bool isEnabled);
    bool SetDevelopmentNumber(const std::string& key, double value);
#endif
private:
    DxrLightingMode mode_ = DxrLightingMode::Reflection;
    uint64_t settingsRevision_ = 0;
    struct Parameters {
        Matrix4x4 inverseViewProjection;
        Matrix4x4 view;
        Matrix4x4 previousViewProjection;
        Matrix4x4 previousView;
        Vector4 cameraPosition;
        Vector4 controls;
        Vector4 temporal;
        Vector4 options;
        Vector4 composition;
        DxrLocalShadowParameters localShadows;
        Vector4 indirectSampling;
        Vector4 historyValidation;
    };
    void CreateResources();
    void CreatePipelines(const DxrRenderer& scene);
    void RenderFilter(uint32_t pipelineIndex, uint32_t signalIndex, uint32_t targetIndex,
        const DxrReflectionInputs& inputs, uint32_t filterStep);
    void ResetResources();
    DxrReflectionSettings settings_;
    bool isReady_ = false;
    bool hasValidFrame_ = false;
    bool hasHistory_ = false;
    bool hasUsedHistory_ = false;
    uint32_t frameIndex_ = 0;
    uint32_t historyWriteIndex_ = 2;
    uint32_t filteredIndex_ = 0;
    uint64_t allocationBytes_ = 0;
    uint64_t previousSceneRevision_ = 0;
    uint64_t previousGeometryRevision_ = 0;
    uint64_t previousSkyLightingHash_ = 0;
    uint64_t previousLightingHash_ = 0;
    uint64_t cameraHistoryId_ = 0;
    const Camera* historyCamera_ = nullptr;
    const DxrRenderer* historyScene_ = nullptr;
    Matrix4x4 previousViewProjection_ = {};
    Matrix4x4 previousView_ = {};
    Vector2 previousJitter_ = {};
    std::string status_ = "RT reflections disabled";
    Microsoft::WRL::ComPtr<ID3D12Device5> device_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rayRoot_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> materialRoot_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> filterRoot_;
    Microsoft::WRL::ComPtr<ID3D12StateObject> rayPipeline_;
    Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> rayProperties_;
    std::array<Microsoft::WRL::ComPtr<ID3D12PipelineState>, 3> filterPipelines_;
    static constexpr uint32_t kTargetCount = 14;
    std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kTargetCount> textures_;
    std::array<uint32_t, kTargetCount> srvIndices_ = {UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX, UINT_MAX};
    std::array<uint32_t, 3> uavIndices_ = {UINT_MAX, UINT_MAX, UINT_MAX};
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    Microsoft::WRL::ComPtr<ID3D12Resource> parameterBuffer_;
    Microsoft::WRL::ComPtr<ID3D12Resource> shaderTable_;
    GpuTimestampTimer traceTimer_;
    GpuTimestampTimer filterTimer_;
    GpuTimestampTimer compositeTimer_;
};
