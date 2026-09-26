#include "Engine/PostEffect/PostEffectManager.h"

#include "App/Scene/SceneManager.h"
#include "Engine/Camera/Camera.h"
#include "Engine/PostEffect/Bloom/BloomRenderer.h"
#include "Engine/PostEffect/Fog/FogManager.h"
#include "Engine/PostEffect/Fog/FogRenderer.h"
#include "Engine/SrvManager/SrvManager.h"
#include "Engine/Winapp/WinApp.h"
#include "Engine/Time/TimeManager.h"
#include <cassert>
#include <cstddef>
#include <cstdio>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#if defined(ENABLE_DEVELOPMENT_TOOLS)
#include "externals/json.hpp"
#endif

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

PostEffectManager::PostEffectManager() = default;

PostEffectManager::~PostEffectManager() = default;

#if defined(ENABLE_DEVELOPMENT_TOOLS)
std::string PostEffectManager::GetDevelopmentSettingsJson() const
{
    const auto& p = copyImageRenderer_->GetPostEffectParameter();
    const FogData& fog = fogManager_->GetFogData();
    const auto* bloom = bloomRenderer_->GetBloomParameter();
    nlohmann::json state = {
        {"animationEnabled", isAnimationEnabled_},
        {"pixelSize", p.pixelSize}, {"colorBrightness", p.colorBrightness},
        {"colorContrast", p.colorContrast}, {"colorSaturation", p.colorSaturation},
        {"focusDepth", p.focusDepth}, {"focusRange", p.focusRange},
        {"depthOfFieldRadius", p.depthOfFieldRadius},
        {"motionBlurDirectionX", p.motionBlurDirection.x},
        {"motionBlurDirectionY", p.motionBlurDirection.y},
        {"motionBlurStrength", p.motionBlurStrength},
        {"motionBlurSampleCount", p.motionBlurSampleCount},
        {"chromaticAberrationStrength", p.chromaticAberrationStrength},
        {"lensDistortionStrength", p.lensDistortionStrength},
        {"filmGrainStrength", p.filmGrainStrength}, {"lensDirtStrength", p.lensDirtStrength},
        {"cameraShakeStrength", cameraShakeOverride_.value_or(SceneManager::GetInstance()->GetCameraShakeStrength())},
        {"bokehRadius", p.bokehRadius}, {"bokehSides", p.bokehSides},
        {"fisheyeStrength", p.fisheyeStrength},
        {"lightThreshold", p.lightThreshold}, {"lightStrength", p.lightStrength},
        {"lightRadius", p.lightRadius},
        {"outlineNormalThreshold", p.outlineNormalThreshold},
        {"outlineNormalSoftness", p.outlineNormalSoftness},
        {"outlineNormalStrength", p.outlineNormalStrength},
        {"fogEnabled", fog.isEnabled != 0}, {"distanceFogEnabled", fog.distanceEnabled != 0},
        {"fogColorR", fog.color.x}, {"fogColorG", fog.color.y}, {"fogColorB", fog.color.z},
        {"fogStart", fog.distance.start}, {"fogEnd", fog.distance.end},
        {"fogCurve", fog.distance.curve}, {"fogDensity", fog.distance.density},
        {"bloomEnabled", bloom && bloom->isEnabled != 0},
        {"bloomThreshold", bloom ? bloom->threshold : 0.0f},
        {"bloomBlurRadius", bloom ? bloom->blurRadius : 0},
        {"bloomBlurSigma", bloom ? bloom->blurSigma : 0.0f},
        {"bloomIntensity", bloom ? bloom->intensity : 0.0f}
    };
    state["passes"] = nlohmann::json::array();
    for (const auto& pass : SceneManager::GetInstance()->GetPostEffects()) {
        state["passes"].push_back({{"type", static_cast<int>(pass.type)},
                                    {"name", GetPostEffectTypeName(pass.type)},
                                    {"enabled", pass.enabled}});
    }
    return state.dump();
}

void PostEffectManager::ApplyDevelopmentSetting(const std::string& key, const std::string& value)
{
    auto parse = [&]() -> float {
        char* end = nullptr;
        const float number = std::strtof(value.c_str(), &end);
        return end != value.c_str() && *end == '\0' && std::isfinite(number) ? number : NAN;
    };
    const bool enabled = value == "true";
    auto& p = copyImageRenderer_->GetPostEffectParameter();
    FogData* fog = fogManager_->GetEditableFogData();
    auto* bloom = bloomRenderer_->GetEditableBloomParameter();
    if (key == "animationEnabled") {
        isAnimationEnabled_ = enabled;
        p.animationEnabled = enabled ? 1 : 0;
        return;
    }
    if (key == "fogEnabled" && fog) { fog->isEnabled = enabled; return; }
    if (key == "distanceFogEnabled" && fog) { fog->distanceEnabled = enabled; return; }
    if (key == "bloomEnabled" && bloom) { bloom->isEnabled = enabled; return; }
    if (key.starts_with("pass.")) {
        const int type = std::atoi(key.c_str() + 5);
        for (const auto& pass : SceneManager::GetInstance()->GetPostEffects()) {
            if (static_cast<int>(pass.type) == type) {
                passOverrides_[type] = enabled;
                SceneManager::GetInstance()->SetPostEffectEnabled(pass.type, enabled);
                break;
            }
        }
        return;
    }
    const float number = parse();
    if (!std::isfinite(number)) return;
#define SET_FLOAT(name, field, low, high) if (key == name) { field = std::clamp(number, low, high); return; }
#define SET_INT(name, field, low, high) if (key == name) { field = static_cast<int>(std::clamp(number, float(low), float(high))); return; }
    SET_FLOAT("pixelSize", p.pixelSize, 1.0f, 64.0f)
    SET_FLOAT("colorBrightness", p.colorBrightness, -1.0f, 1.0f)
    SET_FLOAT("colorContrast", p.colorContrast, 0.0f, 3.0f)
    SET_FLOAT("colorSaturation", p.colorSaturation, 0.0f, 3.0f)
    SET_FLOAT("focusDepth", p.focusDepth, 0.0f, 1.0f)
    SET_FLOAT("focusRange", p.focusRange, 0.0001f, 0.2f)
    SET_FLOAT("depthOfFieldRadius", p.depthOfFieldRadius, 0.0f, 32.0f)
    SET_FLOAT("motionBlurDirectionX", p.motionBlurDirection.x, -1.0f, 1.0f)
    SET_FLOAT("motionBlurDirectionY", p.motionBlurDirection.y, -1.0f, 1.0f)
    SET_FLOAT("motionBlurStrength", p.motionBlurStrength, 0.0f, 0.1f)
    SET_INT("motionBlurSampleCount", p.motionBlurSampleCount, 1, 32)
    SET_FLOAT("chromaticAberrationStrength", p.chromaticAberrationStrength, 0.0f, 0.05f)
    SET_FLOAT("lensDistortionStrength", p.lensDistortionStrength, -1.0f, 1.0f)
    SET_FLOAT("filmGrainStrength", p.filmGrainStrength, 0.0f, 0.5f)
    SET_FLOAT("lensDirtStrength", p.lensDirtStrength, 0.0f, 3.0f)
    SET_FLOAT("bokehRadius", p.bokehRadius, 0.0f, 32.0f)
    SET_INT("bokehSides", p.bokehSides, 3, 12)
    SET_FLOAT("fisheyeStrength", p.fisheyeStrength, 0.01f, 3.0f)
    SET_FLOAT("lightThreshold", p.lightThreshold, 0.0f, 2.0f)
    SET_FLOAT("lightStrength", p.lightStrength, 0.0f, 5.0f)
    SET_FLOAT("lightRadius", p.lightRadius, 0.01f, 1.0f)
    SET_FLOAT("outlineNormalThreshold", p.outlineNormalThreshold, 0.0f, 1.0f)
    SET_FLOAT("outlineNormalSoftness", p.outlineNormalSoftness, 0.001f, 1.0f)
    SET_FLOAT("outlineNormalStrength", p.outlineNormalStrength, 0.0f, 1.0f)
    if (key == "cameraShakeStrength") {
        cameraShakeOverride_ = std::clamp(number, 0.0f, 0.05f);
        SceneManager::GetInstance()->SetCameraShakeStrength(*cameraShakeOverride_);
        return;
    }
    if (fog) {
        SET_FLOAT("fogColorR", fog->color.x, 0.0f, 1.0f)
        SET_FLOAT("fogColorG", fog->color.y, 0.0f, 1.0f)
        SET_FLOAT("fogColorB", fog->color.z, 0.0f, 1.0f)
        SET_FLOAT("fogStart", fog->distance.start, 0.0f, 3000.0f)
        SET_FLOAT("fogEnd", fog->distance.end, 0.01f, 3000.0f)
        SET_FLOAT("fogCurve", fog->distance.curve, 0.01f, 8.0f)
        SET_FLOAT("fogDensity", fog->distance.density, 0.0f, 1.0f)
    }
    if (bloom) {
        SET_FLOAT("bloomThreshold", bloom->threshold, 0.0f, 5.0f)
        SET_INT("bloomBlurRadius", bloom->blurRadius, 0, 32)
        SET_FLOAT("bloomBlurSigma", bloom->blurSigma, 0.01f, 20.0f)
        SET_FLOAT("bloomIntensity", bloom->intensity, 0.0f, 5.0f)
    }
#undef SET_FLOAT
#undef SET_INT
}
#endif

void PostEffectManager::Initialize(DirectXCommon* dxCommon)
{
    assert(dxCommon != nullptr);
    dxCommon_ = dxCommon;

    copyImageRenderer_ = std::make_unique<CopyImageRenderer>();
    copyImageRenderer_->Initialize(dxCommon_);

    bloomRenderer_ = std::make_unique<BloomRenderer>();
    bloomRenderer_->Initialize(dxCommon_);

    fogManager_ = std::make_unique<FogManager>();
    fogManager_->Initialize(dxCommon_);

    fogRenderer_ = std::make_unique<FogRenderer>();
    fogRenderer_->Initialize(dxCommon_);

    for (uint32_t index = 0; index < kPingPongRenderTargetCount; ++index) {
        pingPongRenderTargets_[index].Initialize(dxCommon_, kFirstPingPongRTVIndex + index);
    }
}

void PostEffectManager::Update(Camera* camera)
{
    if (FogData* fogData = fogManager_->GetEditableFogData()) {
        fogData->color = SceneManager::GetInstance()->GetSceneFogColor();
    }
    if (camera != nullptr) {
        auto& parameter = copyImageRenderer_->GetPostEffectParameter();
        parameter.outlineNearClip = camera->GetNearClip();
        parameter.outlineFarClip = camera->GetFarClip();
        fogManager_->SetCameraInfo(
            camera->GetNearClip(),
            camera->GetFarClip(),
            camera->GetFovY(),
            camera->GetAspectRatio());
    }

    fogManager_->Update();
    bloomRenderer_->Update();
#if defined(ENABLE_DEVELOPMENT_TOOLS)
    if (cameraShakeOverride_) {
        SceneManager::GetInstance()->SetCameraShakeStrength(*cameraShakeOverride_);
        if (*cameraShakeOverride_ > 0.0f) {
            SceneManager::GetInstance()->AddPostEffect(PostEffectType::CameraShake, PostEffectStage::BeforeParticle);
        } else {
            SceneManager::GetInstance()->RemovePostEffect(PostEffectType::CameraShake);
        }
    }
    for (const auto& [type, enabled] : passOverrides_) {
        for (const auto& pass : SceneManager::GetInstance()->GetPostEffects()) {
            if (static_cast<int>(pass.type) == type) {
                SceneManager::GetInstance()->SetPostEffectEnabled(pass.type, enabled);
                break;
            }
        }
    }
#endif
}

void PostEffectManager::DrawImGui()
{
    fogManager_->DrawImGui();
    bloomRenderer_->DrawImGui();

#ifdef USE_IMGUI
    ImGui::Begin("Post Effects");

    CopyImageRenderer::PostEffectParameter& parameter = copyImageRenderer_->GetPostEffectParameter();
    const char* animationButtonLabel = "Animated Effects: OFF";
    if (isAnimationEnabled_) {
        animationButtonLabel = "Animated Effects: ON";
    }
    if (ImGui::Button(animationButtonLabel)) {
        isAnimationEnabled_ = !isAnimationEnabled_;
        if (isAnimationEnabled_) { parameter.animationEnabled = 1; } else { parameter.animationEnabled = 0; }
    }
    ImGui::SliderFloat("Pixel Size", &parameter.pixelSize, 1.0f, 64.0f, "%.0f");
    ImGui::SliderFloat("Brightness", &parameter.colorBrightness, -1.0f, 1.0f);
    ImGui::SliderFloat("Contrast", &parameter.colorContrast, 0.0f, 3.0f);
    ImGui::SliderFloat("Saturation", &parameter.colorSaturation, 0.0f, 3.0f);
    ImGui::SliderFloat("Focus Depth", &parameter.focusDepth, 0.0f, 1.0f, "%.4f");
    ImGui::SliderFloat("Focus Range", &parameter.focusRange, 0.0001f, 0.2f, "%.4f");
    ImGui::SliderFloat("DoF Radius", &parameter.depthOfFieldRadius, 0.0f, 32.0f);
    ImGui::SliderFloat2("Motion Direction", &parameter.motionBlurDirection.x, -1.0f, 1.0f);
    ImGui::SliderFloat("Motion Strength", &parameter.motionBlurStrength, 0.0f, 0.1f);
    ImGui::SliderInt("Motion Samples", &parameter.motionBlurSampleCount, 1, 32);
    ImGui::SliderFloat("Chromatic Aberration", &parameter.chromaticAberrationStrength, 0.0f, 0.05f);
    ImGui::SliderFloat("Lens Distortion", &parameter.lensDistortionStrength, -1.0f, 1.0f);
    ImGui::SliderFloat("Film Grain", &parameter.filmGrainStrength, 0.0f, 0.5f);
    ImGui::SliderFloat("Lens Dirt", &parameter.lensDirtStrength, 0.0f, 3.0f);
    float cameraShakeStrength = SceneManager::GetInstance()->GetCameraShakeStrength();
    if (ImGui::SliderFloat("Camera Shake", &cameraShakeStrength, 0.0f, 0.05f)) {
        SceneManager::GetInstance()->SetCameraShakeStrength(cameraShakeStrength);
    }
    ImGui::SliderFloat("Bokeh Radius", &parameter.bokehRadius, 0.0f, 32.0f);
    ImGui::SliderInt("Bokeh Sides", &parameter.bokehSides, 3, 12);
    ImGui::SliderFloat("Fisheye", &parameter.fisheyeStrength, 0.01f, 3.0f);
    ImGui::SliderFloat("Light Threshold", &parameter.lightThreshold, 0.0f, 2.0f);
    ImGui::SliderFloat("Light Strength", &parameter.lightStrength, 0.0f, 5.0f);
    ImGui::SliderFloat("Light Radius", &parameter.lightRadius, 0.01f, 1.0f);
    ImGui::SliderFloat("Outline Normal Threshold", &parameter.outlineNormalThreshold, 0.0f, 1.0f);
    ImGui::SliderFloat("Outline Normal Softness", &parameter.outlineNormalSoftness, 0.001f, 1.0f);
    ImGui::SliderFloat("Outline Normal Strength", &parameter.outlineNormalStrength, 0.0f, 1.0f);
    ImGui::Separator();

    const std::vector<PostEffectInfo>& postEffects = SceneManager::GetInstance()->GetPostEffects();
    if (postEffects.empty()) {
        ImGui::Text("1: Copy (implicit)");
    }

    for (std::size_t index = 0; index < postEffects.size(); ++index) {
        const PostEffectInfo& postEffect = postEffects[index];

        ImGui::Text("%u: %s", static_cast<unsigned int>(index + 1), GetPostEffectTypeName(postEffect.type));
        ImGui::SameLine();

        bool enabled = postEffect.enabled;
        char label[64] {};
        sprintf_s(label, sizeof(label), "Enabled##PostEffect%u", static_cast<unsigned int>(index));
        if (ImGui::Checkbox(label, &enabled)) {
            SceneManager::GetInstance()->SetPostEffectEnabled(postEffect.type, enabled);
        }
    }

    ImGui::End();
#endif
}

void PostEffectManager::PreDrawDepth()
{
    fogRenderer_->PreDrawDepth();
}

void PostEffectManager::PostDrawDepth()
{
    fogRenderer_->PostDrawDepth();
}

void PostEffectManager::PrepareDepthForParticleDraw()
{
    fogRenderer_->PrepareDepthForParticleDraw();
}

void PostEffectManager::SetBoostRadialBlurParameters(bool isBoosting)
{
    CopyImageRenderer::PostEffectParameter& postEffectParameter = copyImageRenderer_->GetPostEffectParameter();
    if (isAnimationEnabled_) {
        postEffectParameter.time += TimeManager::GetInstance()->GetDeltaTime();
    }
    if (postEffectParameter.time > 1000.0f) {
        postEffectParameter.time = 0.0f;
    }

    float boostKickStrength = SceneManager::GetInstance()->GetPostEffectKickStrength();
    postEffectParameter.boostKickStrength = boostKickStrength;

    if (isBoosting) {
        postEffectParameter.radialBlurSampleCount = 48;
        if (boostKickStrength > 0.0f) {
            postEffectParameter.radialBlurSampleCount = 64;
        }
        postEffectParameter.radialBlurWidth = 0.25f + 0.28f * boostKickStrength;
    } else {
        postEffectParameter.radialBlurSampleCount = 32;
        postEffectParameter.radialBlurWidth = 0.05f;
        postEffectParameter.boostKickStrength = 0.0f;
    }
}

void PostEffectManager::UpdatePostEffectParameters(
    SceneManager* sceneManager)
{
    if (sceneManager == nullptr) {
        return;
    }

    CopyImageRenderer::PostEffectParameter& postEffectParameter =
        copyImageRenderer_->GetPostEffectParameter();
    // ArchiveAtmosphere uses the existing reserved slots; the buffer layout stays intact.
    postEffectParameter.padding0 = 16.5f;
    postEffectParameter.padding1 = 2.3f;
    postEffectParameter.padding2 = sceneManager->GetArchiveApproach();
    postEffectParameter.radialBlurCenter =
        sceneManager->GetPostEffectCenter();
    postEffectParameter.cameraShakeStrength =
        sceneManager->GetCameraShakeStrength();
    postEffectParameter.vignetteStrength =
        sceneManager->GetVignetteStrength();
    postEffectParameter.sonicBoomProgress =
        sceneManager->GetSonicBoomProgress();
    postEffectParameter.sonicBoomCenter =
        sceneManager->GetSonicBoomCenter();
    postEffectParameter.blackHoleCenter =
        sceneManager->GetBlackHoleCenter();
    postEffectParameter.blackHoleRadius =
        sceneManager->GetBlackHoleRadius();
    postEffectParameter.blackHoleStrength =
        sceneManager->GetBlackHoleStrength();
    postEffectParameter.waterEffectIntensity =
        sceneManager->GetWaterEffectIntensity();
    postEffectParameter.paintProgress =
        sceneManager->GetPaintProgress();
    postEffectParameter.paintIntensity =
        sceneManager->GetPaintIntensity();
    postEffectParameter.paintSeed =
        sceneManager->GetPaintSeed();
    postEffectParameter.paintPatternType =
        sceneManager->GetPaintPatternType();
    postEffectParameter.paintColor =
        sceneManager->GetPaintColor();
}

void PostEffectManager::Apply(SceneManager* sceneManager, D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle)
{
    if (sceneManager == nullptr) {
        CopyImageRenderer::PostEffectParameter& postEffectParameter = copyImageRenderer_->GetPostEffectParameter();
        postEffectParameter.radialBlurCenter = { 0.5f, 0.5f };

        SetBackBufferRenderTarget();
        ApplyPostEffectToCurrentTarget(PostEffectType::Copy, sceneColorHandle);
        return;
    }

    UpdatePostEffectParameters(sceneManager);

    const std::vector<PostEffectInfo>& postEffects = sceneManager->GetPostEffects();
    int enabledCount = 0;
    for (const PostEffectInfo& postEffect : postEffects) {
        if (postEffect.enabled) {
            enabledCount++;
        }
    }

    if (enabledCount == 0) {
        SetBackBufferRenderTarget();
        ApplyPostEffectToCurrentTarget(PostEffectType::Copy, sceneColorHandle);
        return;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE inputHandle = sceneColorHandle;
    uint32_t pingPongIndex = 0;
    int appliedCount = 0;

    for (const PostEffectInfo& postEffect : postEffects) {
        if (!postEffect.enabled) {
            continue;
        }

        appliedCount++;
        if (appliedCount == enabledCount) {
            if (postEffect.type == PostEffectType::Bloom && bloomRenderer_->IsEnabled()) {
                bloomRenderer_->Generate(inputHandle);
                SetBackBufferRenderTarget();
                bloomRenderer_->Composite(inputHandle);
                continue;
            }

            SetBackBufferRenderTarget();
            ApplyPostEffectToCurrentTarget(postEffect.type, inputHandle);
        } else {
            RenderTarget& renderTarget = pingPongRenderTargets_[pingPongIndex];
            if (postEffect.type == PostEffectType::Bloom && bloomRenderer_->IsEnabled()) {
                bloomRenderer_->Generate(inputHandle);
                renderTarget.BeginRender();
                bloomRenderer_->Composite(inputHandle);
                renderTarget.EndRender();

                inputHandle = renderTarget.GetSrvHandleGPU();
                pingPongIndex = GetNextPingPongIndex(pingPongIndex);
                continue;
            }

            renderTarget.BeginRender();
            ApplyPostEffectToCurrentTarget(postEffect.type, inputHandle);
            renderTarget.EndRender();

            inputHandle = renderTarget.GetSrvHandleGPU();
            pingPongIndex = GetNextPingPongIndex(pingPongIndex);
        }
    }
}

void PostEffectManager::PrepareSceneForParticleDraw(
    SceneManager* sceneManager,
    D3D12_GPU_DESCRIPTOR_HANDLE sceneColorHandle)
{
    UpdatePostEffectParameters(sceneManager);

    particleCompositionTargetIndex_ = 0;

    if (sceneManager == nullptr) {
        RenderTarget& renderTarget =
            pingPongRenderTargets_[particleCompositionTargetIndex_];
        renderTarget.BeginRender();
        ApplyPostEffectToCurrentTarget(
            PostEffectType::Copy,
            sceneColorHandle);
        renderTarget.EndRender();
        return;
    }

    const std::vector<PostEffectInfo>& postEffects =
        sceneManager->GetPostEffects();

    D3D12_GPU_DESCRIPTOR_HANDLE inputHandle =
        sceneColorHandle;
    uint32_t targetIndex = 0;
    bool appliedSceneEffect = false;

    for (const PostEffectInfo& postEffect : postEffects) {
        if (!postEffect.enabled) {
            continue;
        }

        if (postEffect.stage !=
            PostEffectStage::BeforeParticle) {
            continue;
        }

        RenderTarget& renderTarget =
            pingPongRenderTargets_[targetIndex];

        if (postEffect.type == PostEffectType::Bloom &&
            bloomRenderer_->IsEnabled()) {
            bloomRenderer_->Generate(inputHandle);
            renderTarget.BeginRender();
            bloomRenderer_->Composite(inputHandle);
            renderTarget.EndRender();
        } else {
            renderTarget.BeginRender();
            ApplyPostEffectToCurrentTarget(
                postEffect.type,
                inputHandle);
            renderTarget.EndRender();
        }

        inputHandle = renderTarget.GetSrvHandleGPU();
        particleCompositionTargetIndex_ = targetIndex;
        targetIndex = GetNextPingPongIndex(targetIndex);
        appliedSceneEffect = true;
    }

    if (!appliedSceneEffect) {
        particleCompositionTargetIndex_ = 0;
        RenderTarget& renderTarget =
            pingPongRenderTargets_[particleCompositionTargetIndex_];
        renderTarget.BeginRender();
        ApplyPostEffectToCurrentTarget(
            PostEffectType::Copy,
            sceneColorHandle);
        renderTarget.EndRender();
    }
}

void PostEffectManager::BeginParticleDraw()
{
    pingPongRenderTargets_[particleCompositionTargetIndex_]
        .BeginRenderWithDepth(GetDepthDSVHandle());
}

void PostEffectManager::EndParticleDraw()
{
    pingPongRenderTargets_[particleCompositionTargetIndex_]
        .EndRender();
}

void PostEffectManager::ApplyAfterParticleDraw(
    SceneManager* sceneManager)
{
    D3D12_GPU_DESCRIPTOR_HANDLE inputHandle =
        pingPongRenderTargets_[particleCompositionTargetIndex_]
            .GetSrvHandleGPU();

    if (sceneManager == nullptr) {
        SetBackBufferRenderTarget();
        ApplyPostEffectToCurrentTarget(
            PostEffectType::Copy,
            inputHandle);
        return;
    }

    const std::vector<PostEffectInfo>& postEffects =
        sceneManager->GetPostEffects();

    int enabledCount = 0;
    for (const PostEffectInfo& postEffect : postEffects) {
        if (!postEffect.enabled) {
            continue;
        }

        if (postEffect.stage !=
            PostEffectStage::AfterParticle) {
            continue;
        }

        enabledCount += 1;
    }

    if (enabledCount == 0) {
        SetBackBufferRenderTarget();
        ApplyPostEffectToCurrentTarget(
            PostEffectType::Copy,
            inputHandle);
        return;
    }

    uint32_t targetIndex =
        GetNextPingPongIndex(
            particleCompositionTargetIndex_);
    int appliedCount = 0;

    for (const PostEffectInfo& postEffect : postEffects) {
        if (!postEffect.enabled) {
            continue;
        }

        if (postEffect.stage !=
            PostEffectStage::AfterParticle) {
            continue;
        }

        appliedCount += 1;
        if (appliedCount == enabledCount) {
            if (postEffect.type == PostEffectType::Bloom &&
                bloomRenderer_->IsEnabled()) {
                bloomRenderer_->Generate(inputHandle);
                SetBackBufferRenderTarget();
                bloomRenderer_->Composite(inputHandle);
            } else {
                SetBackBufferRenderTarget();
                ApplyPostEffectToCurrentTarget(
                    postEffect.type,
                    inputHandle);
            }
            continue;
        }

        RenderTarget& renderTarget =
            pingPongRenderTargets_[targetIndex];
        if (postEffect.type == PostEffectType::Bloom &&
            bloomRenderer_->IsEnabled()) {
            bloomRenderer_->Generate(inputHandle);
            renderTarget.BeginRender();
            bloomRenderer_->Composite(inputHandle);
            renderTarget.EndRender();
        } else {
            renderTarget.BeginRender();
            ApplyPostEffectToCurrentTarget(
                postEffect.type,
                inputHandle);
            renderTarget.EndRender();
        }

        inputHandle = renderTarget.GetSrvHandleGPU();
        targetIndex = GetNextPingPongIndex(targetIndex);
    }
}

D3D12_CPU_DESCRIPTOR_HANDLE PostEffectManager::GetDepthDSVHandle() const
{
    return fogRenderer_->GetDepthDSVHandle();
}

D3D12_GPU_VIRTUAL_ADDRESS PostEffectManager::GetFogConstantBufferView() const
{
    return fogManager_->GetConstantBufferView();
}

void PostEffectManager::ApplyPostEffectToCurrentTarget(PostEffectType type, D3D12_GPU_DESCRIPTOR_HANDLE inputHandle)
{
    if (type == PostEffectType::Fog) {
        D3D12_GPU_VIRTUAL_ADDRESS fogConstantBufferView = fogManager_->GetConstantBufferView();
        if (fogConstantBufferView == 0) {
            copyImageRenderer_->SetPostEffectType(PostEffectType::Copy);
            copyImageRenderer_->Draw(
                inputHandle, fogRenderer_->GetDepthSRVHandle(), normalTextureHandle_);
            return;
        }

        fogRenderer_->Apply(inputHandle, fogConstantBufferView);
        return;
    }

    if (type == PostEffectType::Bloom) {
        copyImageRenderer_->SetPostEffectType(PostEffectType::Copy);
        copyImageRenderer_->Draw(
            inputHandle, fogRenderer_->GetDepthSRVHandle(), normalTextureHandle_);
        return;
    }

    copyImageRenderer_->SetPostEffectType(type);
    copyImageRenderer_->Draw(
        inputHandle, fogRenderer_->GetDepthSRVHandle(), normalTextureHandle_);
}

void PostEffectManager::SetBackBufferRenderTarget()
{
    dxCommon_->SetBackBufferRenderTarget(dxCommon_->GetDSVHandle());
}

uint32_t PostEffectManager::GetNextPingPongIndex(uint32_t currentIndex) const
{
    uint32_t nextIndex = currentIndex + 1;
    if (nextIndex >= kPingPongRenderTargetCount) {
        nextIndex = 0;
    }

    return nextIndex;
}

void PostEffectManager::RenderTarget::Initialize(DirectXCommon* dxCommon, uint32_t rtvIndex)
{
    assert(dxCommon != nullptr);
    dxCommon_ = dxCommon;

    CreateResource();
    CreateViews(rtvIndex);

    viewport_.Width = static_cast<float>(WinApp::kClientWidth);
    viewport_.Height = static_cast<float>(WinApp::kClientHeight);
    viewport_.TopLeftX = 0.0f;
    viewport_.TopLeftY = 0.0f;
    viewport_.MinDepth = 0.0f;
    viewport_.MaxDepth = 1.0f;

    scissorRect_.left = 0;
    scissorRect_.top = 0;
    scissorRect_.right = WinApp::kClientWidth;
    scissorRect_.bottom = WinApp::kClientHeight;
}

void PostEffectManager::RenderTarget::BeginRender()
{
    Transition(D3D12_RESOURCE_STATE_RENDER_TARGET);

    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    commandList->RSSetViewports(1, &viewport_);
    commandList->RSSetScissorRects(1, &scissorRect_);
    commandList->OMSetRenderTargets(1, &rtvHandle_, false, nullptr);
    commandList->ClearRenderTargetView(rtvHandle_, clearColor_, 0, nullptr);
}

void PostEffectManager::RenderTarget::BeginRenderWithDepth(
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle)
{
    Transition(D3D12_RESOURCE_STATE_RENDER_TARGET);

    ID3D12GraphicsCommandList* commandList =
        dxCommon_->GetCommandList();
    commandList->RSSetViewports(1, &viewport_);
    commandList->RSSetScissorRects(1, &scissorRect_);
    commandList->OMSetRenderTargets(
        1,
        &rtvHandle_,
        false,
        &dsvHandle);
}

void PostEffectManager::RenderTarget::EndRender()
{
    Transition(D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
}

D3D12_GPU_DESCRIPTOR_HANDLE PostEffectManager::RenderTarget::GetSrvHandleGPU() const
{
    return srvHandleGPU_;
}

void PostEffectManager::RenderTarget::CreateResource()
{
    ID3D12Device* device = dxCommon_->GetDevice();

    D3D12_RESOURCE_DESC resourceDesc {};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    resourceDesc.Width = WinApp::kClientWidth;
    resourceDesc.Height = WinApp::kClientHeight;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = format_;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    D3D12_HEAP_PROPERTIES heapProperties {};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_CLEAR_VALUE clearValue {};
    clearValue.Format = format_;
    clearValue.Color[0] = clearColor_[0];
    clearValue.Color[1] = clearColor_[1];
    clearValue.Color[2] = clearColor_[2];
    clearValue.Color[3] = clearColor_[3];

    HRESULT result = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        &clearValue,
        IID_PPV_ARGS(&resource_));
    assert(SUCCEEDED(result));

    currentState_ = D3D12_RESOURCE_STATE_RENDER_TARGET;
}

void PostEffectManager::RenderTarget::CreateViews(uint32_t rtvIndex)
{
    ID3D12Device* device = dxCommon_->GetDevice();

    rtvHandle_ = dxCommon_->GetRTVHandle(rtvIndex);

    D3D12_RENDER_TARGET_VIEW_DESC rtvDesc {};
    rtvDesc.Format = format_;
    rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    device->CreateRenderTargetView(resource_.Get(), &rtvDesc, rtvHandle_);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc {};
    srvDesc.Format = format_;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    srvIndex_ = SrvManager::GetInstance()->Allocate();
    srvHandleCPU_ = SrvManager::GetInstance()->GetCPUDescriptorHandle(srvIndex_);
    srvHandleGPU_ = SrvManager::GetInstance()->GetGPUDescriptorHandle(srvIndex_);
    device->CreateShaderResourceView(resource_.Get(), &srvDesc, srvHandleCPU_);
}

void PostEffectManager::RenderTarget::Transition(D3D12_RESOURCE_STATES nextState)
{
    if (currentState_ == nextState) {
        return;
    }

    D3D12_RESOURCE_BARRIER barrier {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource = resource_.Get();
    barrier.Transition.StateBefore = currentState_;
    barrier.Transition.StateAfter = nextState;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    dxCommon_->GetCommandList()->ResourceBarrier(1, &barrier);
    currentState_ = nextState;
}
