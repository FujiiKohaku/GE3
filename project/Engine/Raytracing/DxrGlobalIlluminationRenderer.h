#pragma once
#include "DxrReflectionRenderer.h"

// The ray scene, hit shading and denoising implementation are shared with reflections.
using DxrGlobalIlluminationSettings = DxrReflectionSettings;
using DxrGlobalIlluminationInputs = DxrReflectionInputs;

class DxrGlobalIlluminationRenderer {
public:
    void Initialize() { pass_.Initialize(); }
    bool SetSettings(const DxrGlobalIlluminationSettings& settings) { return pass_.SetSettings(settings); }
    const DxrGlobalIlluminationSettings& GetSettings() const { return pass_.GetSettings(); }
    uint64_t GetSettingsRevision() const { return pass_.GetSettingsRevision(); }
    D3D12_GPU_DESCRIPTOR_HANDLE Draw(const DxrGlobalIlluminationInputs& inputs) { return pass_.Draw(inputs); }
    void ResetHistory() { pass_.ResetHistory(); }
    void ReadCompleted() { pass_.ReadCompleted(); }
    void DrawImGui() { pass_.DrawImGui(); }
    bool HasValidFrame() const { return pass_.HasValidFrame(); }
    bool HasUsedHistory() const { return pass_.HasUsedHistory(); }
    ID3D12Resource* GetRawTexture() const { return pass_.GetRawTexture(); }
    ID3D12Resource* GetFilteredTexture() const { return pass_.GetFilteredTexture(); }
    ID3D12Resource* GetColorTexture() const { return pass_.GetColorTexture(); }
    ID3D12Resource* GetHistoryStatisticsTexture() const { return pass_.GetHistoryStatisticsTexture(); }
    uint64_t GetAllocationBytes() const { return pass_.GetAllocationBytes(); }
    double GetTraceGpuTimeMs() const { return pass_.GetTraceGpuTimeMs(); }
    double GetFilterGpuTimeMs() const { return pass_.GetFilterGpuTimeMs(); }
    double GetCompositeGpuTimeMs() const { return pass_.GetCompositeGpuTimeMs(); }
    const std::string& GetStatus() const { return pass_.GetStatus(); }
private:
    DxrReflectionRenderer pass_{DxrLightingMode::DiffuseIndirect};
};
