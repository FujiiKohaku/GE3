#pragma once
#include "DxrReflectionRenderer.h"

struct DxrLocalShadowSettings {
    bool isEnabled = false;
    bool isDebugVisible = false;
    bool shouldUseTemporalHistory = true;
    uint32_t maxLightCount = 4;
    uint32_t sampleCount = 4;
    uint32_t spatialPassCount = 2;
    float emitterRadius = 0.1f;
    float normalBias = 0.02f;
    float rayBias = 0.001f;
    float historyWeight = 0.9f;
};
using DxrLocalShadowInputs = DxrReflectionInputs;

class DxrLocalShadowRenderer {
public:
    ~DxrLocalShadowRenderer();
    void Initialize();
    bool SetSettings(const DxrLocalShadowSettings& settings);
    const DxrLocalShadowSettings& GetSettings() const { return settings_; }
    void Prepare(DxrRenderer& scene);
    D3D12_GPU_DESCRIPTOR_HANDLE Draw(const DxrLocalShadowInputs& inputs);
    void ResetHistory() { pass_.ResetHistory(); }
    void ReadCompleted() { pass_.ReadCompleted(); }
    void DrawImGui();
    bool HasValidFrame() const { return pass_.HasValidFrame(); }
    bool HasUsedHistory() const { return pass_.HasUsedHistory(); }
    ID3D12Resource* GetRawTexture() const { return pass_.GetRawTexture(); }
    ID3D12Resource* GetFilteredTexture() const { return pass_.GetFilteredTexture(); }
    ID3D12Resource* GetColorTexture() const { return pass_.GetColorTexture(); }
    ID3D12Resource* GetHistoryStatisticsTexture() const { return pass_.GetHistoryStatisticsTexture(); }
    uint64_t GetAllocationBytes() const { return pass_.GetAllocationBytes(); }
    uint64_t GetSettingsRevision() const { return pass_.GetSettingsRevision(); }
    uint32_t GetSelectedLightCount() const { return selectedLightCount_; }
    double GetTraceGpuTimeMs() const { return pass_.GetTraceGpuTimeMs(); }
    double GetFilterGpuTimeMs() const { return pass_.GetFilterGpuTimeMs(); }
    double GetCompositeGpuTimeMs() const { return pass_.GetCompositeGpuTimeMs(); }
    const std::string& GetStatus() const { return status_; }
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    nlohmann::json GetDevelopmentState() const;
    nlohmann::json GetDevelopmentControls() const;
    bool SetDevelopmentBool(const std::string& key, bool isEnabled);
    bool SetDevelopmentNumber(const std::string& key, double value);
#endif
private:
    DxrLocalShadowSettings settings_;
    uint32_t selectedLightCount_ = 0;
    std::string status_ = "RT local shadows disabled";
    Microsoft::WRL::ComPtr<ID3D12Resource> selectionBuffer_;
    DxrReflectionRenderer pass_{DxrLightingMode::LocalShadow};
};
