#include "DxrLocalShadowRenderer.h"
#include "Engine/Light/LightManager.h"
#include "Engine/Development/DevelopmentWebPanel.h"
#include "Engine/ImGuiManager/ImGuiManager.h"
#include <cmath>
#include <cstring>
#include <stdexcept>

DxrLocalShadowRenderer::~DxrLocalShadowRenderer() {
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    DevelopmentWebPanel::GetInstance().UnregisterOwner(this);
#endif
}
void DxrLocalShadowRenderer::Initialize() {
    pass_.Initialize(); SetSettings(settings_);
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    DevelopmentWebPanel::GetInstance().RegisterSource<DxrLocalShadowRenderer>(this, "rtLocalShadows", "局所ライトRTシャドウ", false,
        &DxrLocalShadowRenderer::GetDevelopmentState, &DxrLocalShadowRenderer::GetDevelopmentControls,
        &DxrLocalShadowRenderer::SetDevelopmentBool, &DxrLocalShadowRenderer::SetDevelopmentNumber, nullptr);
#endif
}
bool DxrLocalShadowRenderer::SetSettings(const DxrLocalShadowSettings& settings) {
    if (settings.maxLightCount < 1 || settings.maxLightCount > 8 || settings.sampleCount < 1 || settings.sampleCount > 16
        || settings.spatialPassCount > 3 || !std::isfinite(settings.emitterRadius) || settings.emitterRadius < 0 || settings.emitterRadius > 1000
        || !std::isfinite(settings.normalBias) || settings.normalBias < 0 || settings.normalBias > 10
        || !std::isfinite(settings.rayBias) || settings.rayBias <= 0 || settings.rayBias > 1
        || !std::isfinite(settings.historyWeight) || settings.historyWeight < 0 || settings.historyWeight > 0.95f) { return false; }
    if (settings.emitterRadius != settings_.emitterRadius || settings.maxLightCount != settings_.maxLightCount) { pass_.ResetHistory(); }
    auto passSettings = pass_.GetSettings(); passSettings.isEnabled = settings.isEnabled;
    passSettings.isDebugVisible = settings.isDebugVisible; passSettings.shouldUseTemporalHistory = settings.shouldUseTemporalHistory;
    passSettings.sampleCount = settings.sampleCount; passSettings.spatialPassCount = settings.spatialPassCount;
    passSettings.normalBias = settings.normalBias; passSettings.rayBias = settings.rayBias; passSettings.historyWeight = settings.historyWeight;
    passSettings.maxRoughness = 1; passSettings.maxDistance = 100000;
    if (!pass_.SetSettings(passSettings)) { return false; }
    settings_ = settings; return true;
}
void DxrLocalShadowRenderer::Prepare(DxrRenderer& scene) {
    selectedLightCount_ = 0; scene.SetLocalShadowParameters({}, 0);
    auto* lights = LightManager::GetInstance();
    if (!settings_.isEnabled || !scene.IsReady() || !scene.GetSettings().isEnabled || !lights->IsInitialized()) { return; }
    DxrLocalShadowParameters parameters;
    parameters.sampleCount = settings_.sampleCount; parameters.controls = {settings_.emitterRadius, settings_.normalBias, settings_.rayBias, 0};
    for (uint32_t index = 0; index < LightManager::kMaxPointLights && selectedLightCount_ < settings_.maxLightCount; ++index) {
        auto light = lights->GetPointLight(index);
        if (lights->IsPointLightShadowEnabled(index) && light.isActive != 0 && light.intensity > 0 && light.radius > 0
            && (light.color.x > 0 || light.color.y > 0 || light.color.z > 0)) {
            parameters.pointMask |= 1u << index; ++selectedLightCount_;
        }
    }
    for (uint32_t index = 0; index < LightManager::kMaxSpotLights && selectedLightCount_ < settings_.maxLightCount; ++index) {
        auto light = lights->GetSpotLight(index);
        if (lights->IsSpotLightShadowEnabled(index) && light.isActive != 0 && light.intensity > 0 && light.distance > 0
            && (light.color.x > 0 || light.color.y > 0 || light.color.z > 0)) {
            parameters.spotMask |= 1u << index; ++selectedLightCount_;
        }
    }
    if (selectedLightCount_ == 0) { status_ = "No eligible local shadow lights"; return; }
    try {
        if (!selectionBuffer_) { selectionBuffer_ = DirectXCommon::GetInstance()->CreateBufferResource(256); }
        void* data = nullptr;
        if (FAILED(selectionBuffer_->Map(0, nullptr, &data))) { throw std::runtime_error("RT local selection mapping failed"); }
        std::memcpy(data, &parameters, sizeof(parameters)); selectionBuffer_->Unmap(0, nullptr);
        scene.SetLocalShadowParameters(parameters, selectionBuffer_->GetGPUVirtualAddress());
    } catch (const std::exception& error) {
        selectedLightCount_ = 0; settings_.isEnabled = false; SetSettings(settings_); status_ = error.what(); Logger::Error(status_);
    }
}
D3D12_GPU_DESCRIPTOR_HANDLE DxrLocalShadowRenderer::Draw(const DxrLocalShadowInputs& inputs) {
    auto output = pass_.Draw(inputs);
    if (!pass_.GetSettings().isEnabled) { settings_.isEnabled = false; }
    status_ = pass_.GetStatus();
    if (!settings_.isEnabled) { status_ = "RT local shadows disabled"; }
    else if (!pass_.HasValidFrame()) { status_ = "RT local shadows waiting for valid scene, capture and selected lights"; }
    return output;
}
void DxrLocalShadowRenderer::DrawImGui() {
#ifdef USE_IMGUI
    if (ImGui::Begin("RT Local Shadows")) {
        ImGui::TextUnformatted(status_.c_str()); auto settings = settings_;
        bool hasChanged = ImGui::Checkbox("Enabled", &settings.isEnabled);
        hasChanged |= ImGui::Checkbox("Selected local lighting only", &settings.isDebugVisible);
        hasChanged |= ImGui::Checkbox("Temporal history", &settings.shouldUseTemporalHistory);
        int lightCount = static_cast<int>(settings.maxLightCount); int samples = static_cast<int>(settings.sampleCount);
        int spatialPasses = static_cast<int>(settings.spatialPassCount);
        hasChanged |= ImGui::SliderInt("Maximum lights", &lightCount, 1, 8); settings.maxLightCount = static_cast<uint32_t>(lightCount);
        hasChanged |= ImGui::SliderInt("Samples per light", &samples, 1, 16); settings.sampleCount = static_cast<uint32_t>(samples);
        hasChanged |= ImGui::SliderInt("Spatial passes", &spatialPasses, 0, 3); settings.spatialPassCount = static_cast<uint32_t>(spatialPasses);
        hasChanged |= ImGui::SliderFloat("Emitter radius", &settings.emitterRadius, 0, 10);
        hasChanged |= ImGui::SliderFloat("History weight", &settings.historyWeight, 0, 0.95f);
        if (hasChanged) { SetSettings(settings); }
        if (ImGui::Button("Reset history")) { ResetHistory(); }
        ImGui::Text("Selected lights %u", selectedLightCount_);
        ImGui::Text("Trace %.3f / filter %.3f / composite %.3f ms", GetTraceGpuTimeMs(), GetFilterGpuTimeMs(), GetCompositeGpuTimeMs());
        ImGui::Text("Targets %.2f MiB", GetAllocationBytes() / 1048576.0);
    }
    ImGui::End();
#endif
}
#if defined(ENABLE_DEVELOPMENT_TOOLS)
nlohmann::json DxrLocalShadowRenderer::GetDevelopmentState() const {
    return {{"status", status_}, {"isEnabled", settings_.isEnabled}, {"isDebugVisible", settings_.isDebugVisible},
        {"shouldUseTemporalHistory", settings_.shouldUseTemporalHistory}, {"maxLightCount", settings_.maxLightCount},
        {"sampleCount", settings_.sampleCount}, {"spatialPassCount", settings_.spatialPassCount}, {"emitterRadius", settings_.emitterRadius},
        {"historyWeight", settings_.historyWeight}, {"selectedLightCount", selectedLightCount_}, {"hasValidFrame", HasValidFrame()},
        {"traceGpuMs", GetTraceGpuTimeMs()}, {"filterGpuMs", GetFilterGpuTimeMs()}, {"compositeGpuMs", GetCompositeGpuTimeMs()}, {"allocationBytes", GetAllocationBytes()}};
}
nlohmann::json DxrLocalShadowRenderer::GetDevelopmentControls() const {
    return nlohmann::json::array({
        {{"key", "status"}, {"label", "状態"}, {"type", "metric"}},
        {{"key", "isEnabled"}, {"label", "局所ライトRTシャドウ"}, {"type", "bool"}},
        {{"key", "isDebugVisible"}, {"label", "対象ライト成分だけ表示"}, {"type", "bool"}},
        {{"key", "shouldUseTemporalHistory"}, {"label", "時間蓄積"}, {"type", "bool"}},
        {{"key", "maxLightCount"}, {"label", "最大ライト数"}, {"type", "number"}, {"min", 1}, {"max", 8}, {"step", 1}},
        {{"key", "sampleCount"}, {"label", "光源あたりサンプル数"}, {"type", "number"}, {"min", 1}, {"max", 16}, {"step", 1}},
        {{"key", "spatialPassCount"}, {"label", "空間フィルター回数"}, {"type", "number"}, {"min", 0}, {"max", 3}, {"step", 1}},
        {{"key", "emitterRadius"}, {"label", "発光面の半径"}, {"type", "number"}, {"min", 0}, {"max", 10}, {"step", 0.01}},
        {{"key", "historyWeight"}, {"label", "履歴の重み"}, {"type", "number"}, {"min", 0}, {"max", 0.95}, {"step", 0.05}},
        {{"key", "selectedLightCount"}, {"label", "対象ライト数"}, {"type", "metric"}},
        {{"key", "traceGpuMs"}, {"label", "探索 (ms)"}, {"type", "metric"}},
        {{"key", "filterGpuMs"}, {"label", "ノイズ除去 (ms)"}, {"type", "metric"}},
        {{"key", "compositeGpuMs"}, {"label", "合成 (ms)"}, {"type", "metric"}},
        {{"key", "allocationBytes"}, {"label", "実割当 (bytes)"}, {"type", "metric"}}
    });
}
bool DxrLocalShadowRenderer::SetDevelopmentBool(const std::string& key, bool isEnabled) {
    auto settings = settings_;
    if (key == "isEnabled") { settings.isEnabled = isEnabled; }
    else if (key == "isDebugVisible") { settings.isDebugVisible = isEnabled; }
    else if (key == "shouldUseTemporalHistory") { settings.shouldUseTemporalHistory = isEnabled; }
    else { return false; }
    return SetSettings(settings);
}
bool DxrLocalShadowRenderer::SetDevelopmentNumber(const std::string& key, double value) {
    if (!std::isfinite(value)) { return false; }
    auto settings = settings_;
    if (key == "maxLightCount" || key == "sampleCount" || key == "spatialPassCount") {
        if (value < 0 || value > 16 || std::floor(value) != value) { return false; }
        if (key == "maxLightCount") { settings.maxLightCount = static_cast<uint32_t>(value); }
        else if (key == "sampleCount") { settings.sampleCount = static_cast<uint32_t>(value); }
        else { settings.spatialPassCount = static_cast<uint32_t>(value); }
    }
    else if (key == "emitterRadius") { settings.emitterRadius = static_cast<float>(value); }
    else if (key == "historyWeight") { settings.historyWeight = static_cast<float>(value); }
    else { return false; }
    return SetSettings(settings);
}
#endif
