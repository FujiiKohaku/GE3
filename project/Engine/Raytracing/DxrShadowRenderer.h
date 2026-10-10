#pragma once
#include "Engine/Raytracing/DxrRenderer.h"
#include "Engine/Raytracing/ShadowDenoiser.h"

struct DxrShadowSettings {
    bool isEnabled = false;
    bool isDebugVisible = false;
    bool isDenoisingEnabled = true;
    bool shouldUseTemporalHistory = true;
    uint32_t maxHistoryFrames = 32;
    uint32_t spatialPassCount = 2;
    uint32_t sampleCount = 8;
    float sunAngularRadiusRadians = 0.00465f;
    float normalBias = 0.02f;
    float rayBias = 0.001f;
    float maxRayDistance = 10000.0f;
};
struct DxrShadowInputs {
    const DxrRenderer* scene = nullptr;
    const Camera* camera = nullptr;
    ID3D12Resource* depthTexture = nullptr;
    ID3D12Resource* normalTexture = nullptr;
    ID3D12Resource* directionalLightTexture = nullptr;
    D3D12_GPU_DESCRIPTOR_HANDLE colorSrv = {};
    D3D12_GPU_DESCRIPTOR_HANDLE depthSrv = {};
    D3D12_GPU_DESCRIPTOR_HANDLE normalSrv = {};
    D3D12_GPU_DESCRIPTOR_HANDLE directionalLightSrv = {};
    Vector3 lightDirection = {0, -1, 0};
    D3D12_GPU_DESCRIPTOR_HANDLE motionVectorSrv = {};
    D3D12_GPU_DESCRIPTOR_HANDLE reprojectionSrv = {};
    D3D12_GPU_DESCRIPTOR_HANDLE previousReprojectionSrv = {};
    uint64_t sceneRevision = 0;
};

class DxrShadowRenderer {
public:
    ~DxrShadowRenderer();
    void Initialize();
    bool SetSettings(const DxrShadowSettings& settings);
    const DxrShadowSettings& GetSettings() const { return settings_; }
    D3D12_GPU_DESCRIPTOR_HANDLE Draw(const DxrShadowInputs& inputs);
    void DrawImGui();
    void ReadCompleted() { traceTimer_.ReadCompleted(); compositeTimer_.ReadCompleted(); denoiser_.ReadCompleted(); }
    bool HasValidFrame() const { return hasValidFrame_; }
    ID3D12Resource* GetMaskTexture() const { return mask_.Get(); }
    ID3D12Resource* GetColorTexture() const { return color_.Get(); }
    ID3D12Resource* GetDenoisedMaskTexture() const { return denoiser_.GetOutputTexture(); }
    ID3D12Resource* GetDenoiseHistoryTexture() const { return denoiser_.GetHistoryTexture(); }
    double GetDenoiseGpuTimeMs() const { return denoiser_.GetGpuTimeMs(); }
    uint64_t GetDenoiseAllocationBytes() const { return denoiser_.GetAllocationBytes(); }
    bool HasUsedShadowHistory() const { return denoiser_.HasUsedHistory(); }
    void ResetHistory() { denoiser_.ResetHistory(); }
    double GetTraceGpuTimeMs() const { return traceTimer_.GetDurationMs(); }
    double GetCompositeGpuTimeMs() const { return compositeTimer_.GetDurationMs(); }
    uint64_t GetTextureAllocationBytes() const { return textureAllocationBytes_; }
    const std::string& GetStatus() const { return status_; }
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    nlohmann::json GetDevelopmentState() const;
    nlohmann::json GetDevelopmentControls() const;
    bool SetDevelopmentBool(const std::string& key, bool isEnabled);
    bool SetDevelopmentNumber(const std::string& key, double value);
#endif
private:
    void CreateResources();
    void CreatePipelines(const DxrRenderer& scene);
    Microsoft::WRL::ComPtr<ID3D12RootSignature> materialRoot_;
    void ResetResources();
    DxrShadowSettings settings_;
    bool isReady_ = false;
    bool hasValidFrame_ = false;
    std::string status_ = "RT shadows disabled";
    uint64_t textureAllocationBytes_ = 0;
    uint32_t maskUavIndex_ = UINT_MAX;
    uint32_t maskSrvIndex_ = UINT_MAX;
    uint32_t colorSrvIndex_ = UINT_MAX;
    Microsoft::WRL::ComPtr<ID3D12Device5> device_;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> commandList_;
    Microsoft::WRL::ComPtr<ID3D12Resource> mask_;
    Microsoft::WRL::ComPtr<ID3D12Resource> color_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap_;
    Microsoft::WRL::ComPtr<ID3D12Resource> parameters_;
    Microsoft::WRL::ComPtr<ID3D12Resource> shaderTable_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> traceRoot_;
    Microsoft::WRL::ComPtr<ID3D12RootSignature> compositeRoot_;
    Microsoft::WRL::ComPtr<ID3D12StateObject> tracePipeline_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> compositePipeline_;
    uint8_t hitShaderIdentifier_[D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES] = {};
    GpuTimestampTimer traceTimer_;
    GpuTimestampTimer compositeTimer_;
    ShadowDenoiser denoiser_;
    uint32_t sampleFrameIndex_ = 0;
};
