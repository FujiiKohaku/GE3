#include "PostEffectManager.h"
#include "Bloom/BloomRenderer.h"
#include "Fog/FogRenderer.h"
#include "Volumetric/VolumetricLightRenderer.h"
#include "App/Scene/Common/SceneManager.h"
#include <set>
#include <stdexcept>

std::vector<std::filesystem::path> PostEffectManager::GetShaderSourcePaths(const nlohmann::json* preset) const
{
    std::set<std::filesystem::path> paths {
        "resources/Shaders/PostEffect/Fullscreen.VS.hlsl",
        "resources/Shaders/PostEffect/Bloom/BloomExtract.PS.hlsl",
        "resources/Shaders/PostEffect/Bloom/BloomBlur.PS.hlsl",
        "resources/Shaders/PostEffect/Bloom/BloomComposite.PS.hlsl",
        "resources/Shaders/PostEffect/Fog/Fog.VS.hlsl",
        "resources/Shaders/PostEffect/Fog/DistanceFog.PS.hlsl"
    };
    for (const auto& entry : copyImageRenderer_->pipelineStates_) {
        paths.emplace(copyImageRenderer_->GetPixelShaderPath(static_cast<PostEffectType>(entry.first & 0xffffffffu)));
    }
    for (const auto& effect : SceneManager::GetInstance()->GetPostEffects()) {
        paths.emplace(copyImageRenderer_->GetPixelShaderPath(effect.type));
    }
    if (preset != nullptr && preset->is_object() && preset->contains("effects") && preset->at("effects").is_array()) {
        for (const auto& effect : preset->at("effects")) {
            if (!effect.is_object() || !effect.contains("type") || !effect.at("type").is_string()) { continue; }
            const std::string name = effect.at("type").get<std::string>();
            for (int index = 0; index <= static_cast<int>(PostEffectType::ScreenLighting); ++index) {
                const auto type = static_cast<PostEffectType>(index);
                if (name == GetPostEffectTypeName(type)) { paths.emplace(copyImageRenderer_->GetPixelShaderPath(type)); break; }
            }
        }
    }
    if (fogRenderer_->depthResolvePipeline_) { paths.emplace("resources/Shaders/SuperResolution/DepthResolve.PS.hlsl"); }
    if (volumetricLightRenderer_->IsReady()) {
        paths.emplace("resources/Shaders/PostEffect/Volumetric/Raymarch.PS.hlsl");
        paths.emplace("resources/Shaders/PostEffect/Volumetric/Temporal.PS.hlsl");
        paths.emplace("resources/Shaders/PostEffect/Volumetric/Composite.PS.hlsl");
    }
    if (autoExposureRenderer_->isReady_) {
        paths.emplace("resources/Shaders/PostEffect/Exposure/Clear.CS.hlsl");
        paths.emplace("resources/Shaders/PostEffect/Exposure/Histogram.CS.hlsl");
        paths.emplace("resources/Shaders/PostEffect/Exposure/Adapt.CS.hlsl");
    }
    return { paths.begin(), paths.end() };
}

void PostEffectManager::EnsurePresetPipelines(const std::vector<PostEffectType>& types)
{
    CopyImageRenderer candidate;
    candidate.dxCommon_ = dxCommon_;
    candidate.rootSignature_ = copyImageRenderer_->rootSignature_;
    candidate.pipelineStates_ = copyImageRenderer_->pipelineStates_;
    std::set<std::wstring> compiledSources;
    const DXGI_FORMAT kFormats[] = { DXGI_FORMAT_R16G16B16A16_FLOAT, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB };
    for (DXGI_FORMAT format : kFormats) {
        candidate.outputFormat_ = format;
        for (PostEffectType type : types) {
            const uint64_t key = (uint64_t(format) << 32) | uint64_t(type);
            if (!candidate.pipelineStates_.contains(key)) {
                const std::wstring sources[] = { L"resources/Shaders/PostEffect/Fullscreen.VS.hlsl", candidate.GetPixelShaderPath(type) };
                for (const auto& source : sources) {
                    if (compiledSources.insert(source).second) { dxCommon_->LoadCompiledShader(source, true); }
                }
            }
            candidate.GetOrCreateGraphicsPipeline(type);
        }
    }
    // Keep all old PSOs alive while in-flight frames may still reference them.
    copyImageRenderer_->pipelineStates_.swap(candidate.pipelineStates_);
}

bool PostEffectManager::ReloadShaders(std::string& error)
{
    error.clear();
    try {
        if (dxCommon_ == nullptr || !dxCommon_->IsCommandListPristine()) {
            throw std::runtime_error("Reload post shaders immediately after PostDraw, before recording the next frame");
        }
        dxCommon_->RefreshShaderIncludeWriteTime();
        // Force compilation even for includes whose timestamps are older than their cached DXIL.
        for (const auto& path : GetShaderSourcePaths()) { dxCommon_->LoadCompiledShader(path.wstring(), true); }

        CopyImageRenderer copy;
        copy.dxCommon_ = dxCommon_; copy.rootSignature_ = copyImageRenderer_->rootSignature_;
        for (const auto& entry : copyImageRenderer_->pipelineStates_) {
            copy.outputFormat_ = static_cast<DXGI_FORMAT>(entry.first >> 32);
            const auto type = static_cast<PostEffectType>(entry.first & 0xffffffffu);
            copy.pipelineStates_[entry.first] = copy.CreateGraphicsPipeline(copy.GetPixelShaderPath(type));
        }
        BloomRenderer bloom;
        bloom.dxCommon_ = dxCommon_; bloom.rootSignature_ = bloomRenderer_->rootSignature_; bloom.format_ = bloomRenderer_->format_;
        bloom.CreatePipelineStates();
        FogRenderer fog;
        fog.dxCommon_ = dxCommon_; fog.rootSignature_ = fogRenderer_->rootSignature_;
        fog.CreatePipelineState();
        if (fogRenderer_->depthResolvePipeline_) { fog.CreateDepthResolvePipeline(); }
        VolumetricLightRenderer volume;
        volume.dxCommon_ = dxCommon_;
        if (volumetricLightRenderer_->IsReady()) { volume.CreatePipelines(); }
        AutoExposureRenderer exposure;
        exposure.rootSignature_ = autoExposureRenderer_->rootSignature_;
        if (autoExposureRenderer_->isReady_) { exposure.CreatePipelines(); }

        // No GPU resources, descriptors or user parameters are replaced. Commit only after all stages succeed.
        dxCommon_->WaitForGPU();
        copyImageRenderer_->pipelineStates_.swap(copy.pipelineStates_);
        bloomRenderer_->pipelineStates_.swap(bloom.pipelineStates_);
        fogRenderer_->pipelineState_.Swap(fog.pipelineState_);
        fogRenderer_->depthResolvePipeline_.Swap(fog.depthResolvePipeline_);
        if (volumetricLightRenderer_->IsReady()) {
            volumetricLightRenderer_->rootSignature_.Swap(volume.rootSignature_);
            volumetricLightRenderer_->raymarchPipeline_.Swap(volume.raymarchPipeline_);
            volumetricLightRenderer_->temporalPipeline_.Swap(volume.temporalPipeline_);
            volumetricLightRenderer_->compositePipeline_.Swap(volume.compositePipeline_);
            volumetricLightRenderer_->ResetHistory();
        }
        if (autoExposureRenderer_->isReady_) {
            autoExposureRenderer_->pipelines_.swap(exposure.pipelines_);
            autoExposureRenderer_->ResetHistory();
        }
        ++shaderRevision_;
        return true;
    } catch (const std::exception& exception) {
        error = std::string("Post shader reload failed: ") + exception.what();
        return false;
    }
}
